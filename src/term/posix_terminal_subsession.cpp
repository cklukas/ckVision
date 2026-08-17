// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/posix_terminal_subsession.hpp"

#if !defined(_WIN32)

#include <algorithm>
#if defined(__APPLE__)
#include <crt_externs.h>
#endif
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

namespace {
// The environment this process was given. Reaching for it belongs here, in
// the platform adapter, and nowhere above it.
char** parent_environment() noexcept {
#if defined(__APPLE__)
    return *::_NSGetEnviron();
#else
    extern char** environ;
    return environ;
#endif
}
}  // namespace

namespace ckv::term {

PosixTerminalSubsession::PosixTerminalSubsession(TerminalLaunchSpec spec, TerminalSubsessionOptions options)
    : spec_(std::move(spec)), options_(std::move(options)), emulator_(spec_.profile, options_) {}

std::unique_ptr<PosixTerminalSubsession> PosixTerminalSubsession::launch(TerminalLaunchSpec spec,
                                                                           TerminalSubsessionOptions options) {
    auto session = std::unique_ptr<PosixTerminalSubsession>(new PosixTerminalSubsession(std::move(spec), std::move(options)));
    if (!session->spawn()) session->emulator_.mark_failed("unable to launch private child PTY session");
    return session;
}

PosixTerminalSubsession::~PosixTerminalSubsession() { close(); }

bool PosixTerminalSubsession::spawn() {
    if (spec_.executable.empty() || spec_.working_directory.empty()) return false;

    std::vector<char*> argv;
    argv.reserve(spec_.arguments.size() + 2);
    // execve takes the program to run and the name it is run under as two
    // separate things, and a spec that left them fused could not ask for a
    // login shell.
    argv.push_back(spec_.argv0.empty() ? spec_.executable.data() : spec_.argv0.data());
    for (std::string& argument : spec_.arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);

    std::vector<std::string> child_environment;
    if (spec_.environment_policy == TerminalEnvironmentPolicy::InheritAndOverride) {
        for (char** entry = parent_environment(); entry != nullptr && *entry != nullptr; ++entry)
            child_environment.emplace_back(*entry);
    }
    const auto entry_named = [&child_environment](const std::string& name) {
        return std::find_if(child_environment.begin(), child_environment.end(),
                            [&name](const std::string& entry) {
                                return entry.compare(0, name.size(), name) == 0 &&
                                       entry.size() > name.size() && entry[name.size()] == '=';
                            });
    };
    std::vector<std::string> named_by_spec;
    for (const auto& [name, value] : spec_.environment) {
        if (name.empty() || name.find('=') != std::string::npos) return false;
        // Naming the same variable twice in one spec is a contradiction the
        // caller has to resolve; overriding an inherited one is the point.
        if (std::find(named_by_spec.begin(), named_by_spec.end(), name) != named_by_spec.end()) return false;
        named_by_spec.push_back(name);
        const auto existing = entry_named(name);
        if (existing != child_environment.end()) *existing = name + "=" + value;
        else child_environment.push_back(name + "=" + value);
    }
    std::vector<char*> environment;
    environment.reserve(child_environment.size() + 1);
    for (std::string& entry : child_environment) environment.push_back(entry.data());
    environment.push_back(nullptr);

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
            (void)::write(exec_status[1], &failure, sizeof(failure));
            _exit(127);
        };
        if (::chdir(spec_.working_directory.c_str()) != 0) report_exec_failure();
        ::execve(spec_.executable.c_str(), argv.data(), environment.data());
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

void PosixTerminalSubsession::resize(Size cells, Size cell_pixels) {
    emulator_.resize(cells, cell_pixels);
    if (master_fd_ < 0) return;
    const Size bounded = emulator_.profile().cells;
    winsize size{};
    size.ws_col = static_cast<unsigned short>(bounded.width);
    size.ws_row = static_cast<unsigned short>(bounded.height);
    const auto pixel_dimension = [](int cell_pixels, int cells) noexcept {
        const std::int64_t product = static_cast<std::int64_t>(std::max(0, cell_pixels)) *
                                     static_cast<std::int64_t>(std::max(0, cells));
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
