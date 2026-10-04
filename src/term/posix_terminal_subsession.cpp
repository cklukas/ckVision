// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/posix_terminal_subsession.hpp"
#include "cvision/term/process_launch_internal.hpp"

#if !defined(_WIN32)

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

namespace ckv::term {

PosixTerminalSubsession::PosixTerminalSubsession(TerminalLaunchSpec spec, TerminalSubsessionOptions options)
    : spec_(std::move(spec)), options_(std::move(options)), emulator_(spec_.profile, options_) {}

std::unique_ptr<PosixTerminalSubsession> PosixTerminalSubsession::launch(TerminalLaunchSpec spec,
                                                                           TerminalSubsessionOptions options) {
    auto session = std::unique_ptr<PosixTerminalSubsession>(new PosixTerminalSubsession(std::move(spec), std::move(options)));
    // Refused before the fork, not diagnosed after it: an unnamed exit policy
    // decides whether `close()` is bounded, and picking one here would be the
    // library making that choice behind the caller's back — which is the whole
    // failure this enumeration exists to prevent. Reported as a Failed session
    // so it arrives through the same channel as any other launch failure.
    if (session->spec_.exit_policy == core::TerminalExitPolicy::Unspecified) {
        session->emulator_.mark_failed(
            "terminal launch spec did not name an exit policy: set "
            "`exit_policy` to TerminateAfterGrace (bounded close) or "
            "WaitForExit (waits for the child, never escalates)");
        return session;
    }
    if (session->spec_.windows_command) {
        session->emulator_.mark_failed("Windows command-processor launch is unavailable on POSIX");
        return session;
    }
    if (!session->spawn()) session->emulator_.mark_failed("unable to launch private child PTY session");
    return session;
}

PosixTerminalSubsession::~PosixTerminalSubsession() { close(); }

bool PosixTerminalSubsession::spawn() {
    detail::PosixLaunchData launch;
    detail::ProcessPreparationError error;
    if (!detail::prepare_posix_process_launch(spec_, launch, error)) return false;

    int exec_status[2] = {-1, -1};
    if (::pipe(exec_status) != 0) return false;
    for (const int descriptor : exec_status) {
        const int flags = ::fcntl(descriptor, F_GETFD, 0);
        if (flags < 0 || ::fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) < 0) {
            (void)::close(exec_status[0]);
            (void)::close(exec_status[1]);
            return false;
        }
    }

    const pid_t child = forkpty(&master_fd_, nullptr, nullptr, nullptr);
    if (child < 0) {
        (void)::close(exec_status[0]);
        (void)::close(exec_status[1]);
        return false;
    }
    if (child == 0) {
        (void)::close(exec_status[0]);
        auto report_exec_failure = [&exec_status]() noexcept {
            const unsigned char failure = 1;
            // Best-effort by design: the child _exit(127)s whether or not the
            // parent reads this byte. Consume the result into a variable —
            // (void)::write(...) does NOT satisfy GCC's warn_unused_result on
            // write(), which is why every build but this platform's was clean.
            const ssize_t reported = ::write(exec_status[1], &failure, sizeof(failure));
            (void)reported;
            _exit(127);
        };
        // Before anything the child could be interrupted during, and before the
        // program it is about to become inherits what this process happened to
        // be ignoring.
        detail::reset_process_child_signals();
        if (::chdir(launch.directory.c_str()) != 0) report_exec_failure();
        ::execve(launch.executable.c_str(), launch.argv.data(), launch.environment.data());
        report_exec_failure();
    }
    (void)::close(exec_status[1]);
    child_pid_ = static_cast<int>(child);
    wait_handles_[0] = WaitHandle{WaitHandleKind::PosixFileDescriptor, static_cast<std::uintptr_t>(master_fd_)};
    wait_handle_count_ = 1;
    unsigned char failure = 0;
    ssize_t exec_result = 0;
    do {
        exec_result = ::read(exec_status[0], &failure, sizeof(failure));
    } while (exec_result < 0 && errno == EINTR);
    (void)::close(exec_status[0]);
    if (exec_result > 0) {
        close();
        return false;
    }
    const int flags = ::fcntl(master_fd_, F_GETFL, 0);
    if (flags < 0 || ::fcntl(master_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        close();
        return false;
    }
    resize(spec_.profile.cells, spec_.profile.cell_pixels);
    return true;
}

void PosixTerminalSubsession::resize(Size cells, PixelSize cell_pixels) {
    emulator_.resize(cells, cell_pixels);
    if (master_fd_ < 0) return;
    const Size bounded = emulator_.profile().cells;
    winsize size{};
    size.ws_col = static_cast<unsigned short>(bounded.width);
    size.ws_row = static_cast<unsigned short>(bounded.height);
    const auto pixel_dimension = [](int pixels_per_cell, int cell_count) noexcept {
        const std::int64_t product = static_cast<std::int64_t>(std::max(0, pixels_per_cell)) *
                                     static_cast<std::int64_t>(std::max(0, cell_count));
        return static_cast<unsigned short>(std::clamp<std::int64_t>(
            product, 0, std::numeric_limits<unsigned short>::max()));
    };
    size.ws_xpixel = pixel_dimension(cell_pixels.width, bounded.width);
    size.ws_ypixel = pixel_dimension(cell_pixels.height, bounded.height);
    (void)::ioctl(master_fd_, TIOCSWINSZ, &size);
}

void PosixTerminalSubsession::send_input(std::string_view bytes) {
    emulator_.send_input(bytes);
    if (master_fd_ < 0) return;
    std::string pending = emulator_.take_pending_input();
    bool retain_pending = true;
    while (!pending.empty()) {
        const ssize_t written = ::write(master_fd_, pending.data(), pending.size());
        if (written > 0) pending.erase(0, static_cast<std::size_t>(written));
        else if (written < 0 && errno == EINTR) continue;
        else if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        else {
            retain_pending = false;
            observe_exit();
            break;
        }
    }
    if (retain_pending && !pending.empty()) emulator_.send_input(pending);
}

bool PosixTerminalSubsession::drain(std::size_t byte_budget) {
    const TerminalSubsessionState state_before = emulator_.state();
    if (master_fd_ < 0) {
        observe_exit();
        return emulator_.state() != state_before;
    }
    if (byte_budget == 0) return false;
    bool changed = false;
    bool reached_read_boundary = false;
    char buffer[4096];
    while (byte_budget > 0) {
        const std::size_t request = std::min(byte_budget, sizeof(buffer));
        const ssize_t count = ::read(master_fd_, buffer, request);
        if (count > 0) {
            emulator_.feed_output(std::string_view(buffer, static_cast<std::size_t>(count)));
            byte_budget -= static_cast<std::size_t>(count);
            changed = true;
        } else if (count < 0 && errno == EINTR) continue;
        else {
            reached_read_boundary = true;
            break;
        }
    }
    // Emulator query replies (for example CSI 6 n) are generated while
    // consuming child output. Flush them through this private PTY only; they
    // never become outer-terminal presenter bytes.
    send_input({});
    if (reached_read_boundary) observe_exit();
    return changed || emulator_.state() != state_before;
}

void PosixTerminalSubsession::observe_exit() {
    if (child_pid_ < 0) return;
    int status = 0;
    const pid_t observed = ::waitpid(static_cast<pid_t>(child_pid_), &status, WNOHANG);
    if (observed != static_cast<pid_t>(child_pid_)) return;
    emulator_.mark_exited(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    child_pid_ = -1;
    if (master_fd_ >= 0) {
        (void)::close(master_fd_);
        master_fd_ = -1;
    }
    wait_handle_count_ = 0;
}

void PosixTerminalSubsession::request_termination() noexcept {
    if (child_pid_ < 0) return;
    const pid_t child = static_cast<pid_t>(child_pid_);
    // The group, so a shell cannot leave its own foreground child behind — the
    // same rule `close()` follows, and for the same reason.
    if (::kill(-child, SIGHUP) != 0) (void)::kill(child, SIGHUP);
    if (::kill(-child, SIGTERM) != 0) (void)::kill(child, SIGTERM);
}

void PosixTerminalSubsession::request_kill() noexcept {
    if (child_pid_ < 0) return;
    const pid_t child = static_cast<pid_t>(child_pid_);
    // The group again, for the reason the asking half gives: a shell killed on
    // its own leaves its foreground child running with nobody's terminal to
    // write to.
    if (::kill(-child, SIGKILL) != 0) (void)::kill(child, SIGKILL);
}

void PosixTerminalSubsession::close() noexcept {
    if (closed_) return;
    closed_ = true;
    if (child_pid_ >= 0) {
        const pid_t child = static_cast<pid_t>(child_pid_);
        const auto signal_process_group = [child](int signal) noexcept {
            // forkpty makes the child a session/process-group leader. Signal
            // the group so shells cannot leave a foreground descendant
            // behind when the embedded view closes.
            if (::kill(-child, signal) != 0)
                (void)::kill(child, signal);
        };
        // The hangup is the signal, not the descriptor. Sending it first and
        // closing the PTY last is not a preference: a child blocked writing to
        // its terminal cannot act on ANY signal until that write completes,
        // and the write cannot complete while nothing empties the PTY. Close
        // the master first and the queue can never drain again, so the child
        // this protocol exists to end is exactly the child it wedges — on
        // macOS it goes to "exiting" and does not reap even under SIGKILL, and
        // close() blocks in waitpid forever. One flooding program would take
        // its whole host with it, which for a multiplexer's server means every
        // other terminal on the machine.
        signal_process_group(SIGHUP);
        signal_process_group(SIGTERM);
        int status = 0;
        // Throws away whatever the child has queued. The caller is closing
        // this terminal, so its output has no reader left; what the reads are
        // for is making room, so a child stuck in write() can finish, notice
        // the signal it has already been sent, and exit.
        const auto make_room = [this]() noexcept {
            if (master_fd_ < 0) return;
            char scrap[4096];
            for (int reads = 0; reads < 64; ++reads) {
                const ssize_t count = ::read(master_fd_, scrap, sizeof scrap);
                if (count > 0) continue;
                if (count < 0 && errno == EINTR) continue;
                break;  // EAGAIN on an empty master, or the read boundary
            }
        };
        // Returns once there is nothing left to wait for — the child was
        // reaped, or it was never ours to reap — and false if `attempts`
        // hundredths of a second went by with the child still running.
        const auto reap_within = [&](int attempts) noexcept {
            for (int attempt = 0; attempt < attempts; ++attempt) {
                const pid_t result = ::waitpid(child, &status, WNOHANG);
                if (result == child) return true;
                if (result < 0 && errno != EINTR) return true;
                make_room();
                (void)::poll(nullptr, 0, 10);
            }
            return false;
        };
        if (spec_.exit_policy == TerminalExitPolicy::WaitForExit) {
            // This policy deliberately waits for the child to honour the
            // requested graceful termination; it never escalates behind the
            // caller's back. It does keep the PTY moving while it waits,
            // because a wait that can deadlock on the child it is waiting for
            // is not a policy, it is a hang.
            while (!reap_within(100)) {
            }
        } else {
            // TerminateAfterGrace is intentionally bounded: close remains
            // safe for a child that traps or ignores SIGTERM.
            if (!reap_within(100)) {
                signal_process_group(SIGKILL);
                // Draining still matters after SIGKILL. A process wedged in a
                // tty write is not in a state a kill can complete, so the room
                // has to come first and the signal takes effect behind it.
                (void)reap_within(200);
            }
        }
        child_pid_ = -1;
    }
    if (master_fd_ >= 0) { (void)::close(master_fd_); master_fd_ = -1; }
    wait_handle_count_ = 0;
    emulator_.close();
}

}  // namespace ckv::term

#endif
