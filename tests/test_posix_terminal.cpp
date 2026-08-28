// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Integration tests for PosixTerminal, run against a forked PTY child
// — never against this test process's own controlling terminal, so a
// bug here can never disrupt the actual session running the tests.
#if defined(CKVISION_HAS_POSIX_TERMINAL)

#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/record_replay_terminal.hpp"
#include "cvision/ui/application.hpp"

#include <fcntl.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif
#include <signal.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#include "cvision/testing/cktest.hpp"

using namespace ckv;
using namespace ckv::term;

namespace ckv::term {
// Internal POSIX deadline conversion, kept at namespace scope in the owning
// translation unit so this deterministic contract test need not infer a
// poll(2) timeout from scheduler timing.
int poll_timeout_milliseconds(std::int64_t now_nanos,
                              std::int64_t deadline_nanos) noexcept;
}  // namespace ckv::term

namespace {

struct PtyChild {
    int master_fd = -1;
    int stderr_fd = -1;
    pid_t pid = -1;
    // When non-negative, the parent acknowledges after draining output before
    // the child may close its slave PTY. This makes output-lifecycle tests
    // deterministic on systems that discard undrained slave output at close.
    int output_ack_fd = -1;
};

volatile sig_atomic_t g_host_winch_hits = 0;
volatile sig_atomic_t g_host_sigint_hits = 0;

extern "C" void host_winch_handler(int) { g_host_winch_hits = 1; }
extern "C" void host_sigint_handler(int) { g_host_sigint_hits = 1; }

// Forks a child with `slave_fd` as its controlling stdin/stdout, then
// runs `child_fn` in the child and _exit()s. The parent gets the
// master fd to interact with. Never touches the real terminal.
PtyChild spawn_pty_child(void (*child_fn)(int slave_fd), bool capture_stderr = false) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);

    int stderr_pipe[2] = {-1, -1};
    if (capture_stderr) {
        CK_CHECK(::pipe(stderr_pipe) == 0);
        CK_CHECK(::fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK) == 0);
    }

    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        ::close(master_fd);
        if (capture_stderr) {
            ::close(stderr_pipe[0]);
            if (::dup2(stderr_pipe[1], STDERR_FILENO) < 0) ::_exit(1);
            ::close(stderr_pipe[1]);
        }
        ::setsid();
        child_fn(slave_fd);
        ::_exit(0);
    }
    ::close(slave_fd);
    if (capture_stderr)
        ::close(stderr_pipe[1]);
    else
        CK_CHECK(stderr_pipe[0] == -1 && stderr_pipe[1] == -1);
    return PtyChild{master_fd, capture_stderr ? stderr_pipe[0] : -1, pid, -1};
}

// Like spawn_pty_child(), but keeps the slave side open after child_fn has
// produced its observable output. The parent writes one acknowledgement byte
// only after it has drained and checked that output, then the child may exit.
PtyChild spawn_pty_child_until_output_acknowledged(void (*child_fn)(int slave_fd, int acknowledge_fd)) {
    int master_fd = -1;
    int slave_fd = -1;
    int acknowledge_pipe[2] = {-1, -1};
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    CK_CHECK(::pipe(acknowledge_pipe) == 0);

    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        ::close(master_fd);
        ::close(acknowledge_pipe[1]);
        ::setsid();
        child_fn(slave_fd, acknowledge_pipe[0]);
        ::_exit(0);
    }
    ::close(slave_fd);
    ::close(acknowledge_pipe[0]);
    return PtyChild{master_fd, -1, pid, acknowledge_pipe[1]};
}

void acknowledge_output(PtyChild& child) {
    CK_CHECK(child.output_ack_fd >= 0);
    const char acknowledgement = 'A';
    CK_CHECK(::write(child.output_ack_fd, &acknowledgement, 1) == 1);
    ::close(child.output_ack_fd);
    child.output_ack_fd = -1;
}

std::string read_available(int fd, int max_wait_ms) {
    std::string out;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(max_wait_ms);
    char buf[4096];
    while (std::chrono::steady_clock::now() < deadline) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0) {
            out.append(buf, static_cast<std::size_t>(n));
            continue;  // drain what's immediately available before waiting again
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return out;
}

std::string read_until_contains(int fd, std::string_view needle, int max_wait_ms) {
    std::string out;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(max_wait_ms);
    char buf[4096];
    while (std::chrono::steady_clock::now() < deadline) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0) {
            out.append(buf, static_cast<std::size_t>(n));
            if (out.find(needle) != std::string::npos) return out;
            continue;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return out;
}

std::string read_output_until_status(int output_fd, int status_fd, char expected_status,
                                     int max_wait_ms) {
    std::string out;
    bool received_status = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(max_wait_ms);
    char output[4096];
    while (std::chrono::steady_clock::now() < deadline) {
        bool drained_any = false;
        for (ssize_t n = ::read(output_fd, output, sizeof(output)); n > 0;
             n = ::read(output_fd, output, sizeof(output))) {
            out.append(output, static_cast<std::size_t>(n));
            drained_any = true;
        }

        char status = 0;
        const ssize_t n = ::read(status_fd, &status, 1);
        if (n == 1) {
            CK_CHECK(status == expected_status);
            received_status = true;
            break;
        }
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) break;
        // Idle only when there was nothing to move. A child blocked writing a
        // large payload can proceed no faster than this side drains, so
        // sleeping after a productive pass throttles the very transfer being
        // waited for: 256 KiB through a small PTY buffer is hundreds of passes,
        // and at 5ms each that overran the deadline on a loaded CI runner while
        // finishing in milliseconds here.
        if (!drained_any) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    for (ssize_t n = ::read(output_fd, output, sizeof(output)); n > 0;
         n = ::read(output_fd, output, sizeof(output)))
        out.append(output, static_cast<std::size_t>(n));
    CK_CHECK(received_status);
    return out;
}

// ResumeProbe paints at the upper-left corner and Application always finishes
// a presented frame by explicitly hiding the cursor. Those protocol markers
// let this PTY test isolate the frame from session-entry and probe traffic,
// then compare the exact bytes that recreate visible application state.
std::string last_presented_frame(std::string_view output) {
    constexpr std::string_view kFrameStart = "\x1B[1;1H";
    constexpr std::string_view kFrameEnd = "\x1B[?25l";
    const std::size_t start = output.rfind(kFrameStart);
    if (start == std::string_view::npos) return {};
    const std::size_t end = output.find(kFrameEnd, start);
    if (end == std::string_view::npos) return {};
    return std::string(output.substr(start, end + kFrameEnd.size() - start));
}

// Bounded on purpose. An unbounded waitpid() turns any stuck child into a hang
// of the whole shard, which CTest can only report as a timeout naming the shard
// rather than the test -- exactly how a PTY deadlock reached CI as an opaque
// 120s stall. Waiting to a deadline and killing the straggler turns that into an
// ordinary failure at the CK_CHECK(WIFEXITED(...)) every caller already has.
int wait_child(pid_t pid, int max_wait_ms = 20'000) {
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(max_wait_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        const pid_t reaped = ::waitpid(pid, &status, WNOHANG);
        if (reaped == pid || reaped < 0) return status;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ::kill(pid, SIGKILL);
    ::waitpid(pid, &status, 0);
    return status;
}

// The companion for a child that is mid-write to a PTY. Such a child can only
// reach its exit once this side has taken the bytes, so waiting without draining
// deadlocks outright: the parent blocks in waitpid() while the child blocks in
// write(), and neither moves again. Drains while it waits.
int wait_child_draining(pid_t pid, int output_fd, std::string* sink = nullptr,
                        int max_wait_ms = 20'000) {
    int status = 0;
    char buf[4096];
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(max_wait_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        bool drained_any = false;
        for (ssize_t n = ::read(output_fd, buf, sizeof(buf)); n > 0;
             n = ::read(output_fd, buf, sizeof(buf))) {
            if (sink != nullptr) sink->append(buf, static_cast<std::size_t>(n));
            drained_any = true;
        }
        const pid_t reaped = ::waitpid(pid, &status, WNOHANG);
        if (reaped == pid || reaped < 0) return status;
        if (!drained_any) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ::kill(pid, SIGKILL);
    ::waitpid(pid, &status, 0);
    return status;
}

class KeyProbe final : public ui::View {
public:
    bool on_key(const KeyEvent&) override {
        ++key_events;
        return true;
    }

    int key_events = 0;
};

class ThrowingKeyProbe final : public ui::View {
public:
    ThrowingKeyProbe() { set_focus_policy(ui::FocusPolicy::TabStop); }

    bool on_key(const KeyEvent&) override { throw std::runtime_error("intentional callback failure"); }
};

class ResumeProbe final : public ui::View {
public:
    void draw(scene::Painter& painter) override {
        ++draw_count;
        painter.draw_text(Point{0, 0}, "RESUME-FRAME", Style{});
    }

    bool on_key(const KeyEvent& event) override {
        if (event.chord.text != "q") return false;
        quit = true;
        return true;
    }

    bool quit = false;
    int draw_count = 0;
};

void drain_pty_master(int fd) {
    char buffer[4096];
    while (::read(fd, buffer, sizeof(buffer)) > 0) {
    }
}

std::size_t count_occurrences(std::string_view haystack, std::string_view needle) {
    std::size_t count = 0;
    for (std::size_t at = 0; (at = haystack.find(needle, at)) != std::string_view::npos;
         at += needle.size())
        ++count;
    return count;
}

}  // namespace

CK_TEST(posix_poll_rounds_positive_sub_millisecond_deadlines_up_instead_of_spinning) {
    CK_CHECK(poll_timeout_milliseconds(1'000, 1'001) == 1);
    CK_CHECK(poll_timeout_milliseconds(1'000, 1'000'999) == 1);
    CK_CHECK(poll_timeout_milliseconds(1'000, 1'001'000) == 1);
    CK_CHECK(poll_timeout_milliseconds(1'000, 1'001'001) == 2);
    CK_CHECK(poll_timeout_milliseconds(1'000, 999) == 0);
    CK_CHECK(poll_timeout_milliseconds(
                 1'000, std::numeric_limits<std::int64_t>::max()) == -1);
}

CK_TEST(construction_enters_alt_screen_and_raw_mode_sequences) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    const std::string output = read_available(child.master_fd, 300);
    CK_CHECK(output.find("\x1B[?1049h") != std::string::npos);  // alt screen
    CK_CHECK(output.find("\x1B[?2004h") != std::string::npos);  // bracketed paste on
    CK_CHECK(output.find("\x1B[?1016h") != std::string::npos);  // SGR pixel mouse on
    CK_CHECK(output.find("\x1B[?2026h") != std::string::npos);  // synchronized-output probe on
    CK_CHECK(output.find("\x1B[?2026l") != std::string::npos);  // synchronized-output probe off
    CK_CHECK(output.find("\x1B[?2031h") != std::string::npos);  // color-scheme notifications on
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(posix_terminal_combined_wait_returns_when_a_borrowed_private_pty_becomes_readable) {
    int terminal_master = -1;
    int terminal_slave = -1;
    int child_master = -1;
    int child_slave = -1;
    CK_CHECK(::openpty(&terminal_master, &terminal_slave, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::openpty(&child_master, &child_slave, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(terminal_master, F_SETFL, O_NONBLOCK) == 0);

    const pid_t process = ::fork();
    CK_CHECK(process >= 0);
    if (process == 0) {
        ::close(terminal_master);
        ::close(child_slave);
        PosixClock clock;
        PosixTerminal terminal(clock, terminal_slave, terminal_slave, baseline_capabilities(),
                               /*enable_capability_probes=*/false);
        const WaitHandle child_handle{WaitHandleKind::PosixFileDescriptor,
                                      static_cast<std::uintptr_t>(child_master)};
        const std::array<WaitHandle, 1> child_handles{child_handle};
        (void)terminal.poll(clock.now_nanos() + 2'000'000'000LL, child_handles);
        terminal.write("COMBINED-PTY-WAKE");
        ::close(child_master);
        ::_exit(0);
    }

    ::close(terminal_slave);
    ::close(child_master);
    ::usleep(50'000);  // allow the child to enter its combined wait
    const char byte = 'w';
    CK_CHECK(::write(child_slave, &byte, 1) == 1);
    const std::string output = read_until_contains(terminal_master, "COMBINED-PTY-WAKE", 1'000);
    CK_CHECK(output.find("COMBINED-PTY-WAKE") != std::string::npos);
    const int status = wait_child(process);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child_slave);
    ::close(terminal_master);
}

CK_TEST(final_posix_terminal_destruction_restores_the_hosts_signal_dispositions) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        struct sigaction host_action{};
        host_action.sa_handler = &host_winch_handler;
        sigemptyset(&host_action.sa_mask);
        CK_CHECK(::sigaction(SIGWINCH, &host_action, nullptr) == 0);

        {
            PosixClock clock;
            PosixTerminal term(clock, slave_fd, slave_fd, baseline_capabilities(),
                               /*enable_capability_probes=*/false);
        }

        g_host_winch_hits = 0;
        ::raise(SIGWINCH);
        if (g_host_winch_hits != 1) ::_exit(1);
    });
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(sigint_remains_host_owned_with_two_live_posix_applications) {
    // Run in a child: a regression that accidentally installs SIG_DFL for
    // SIGINT must fail this contract without interrupting the test runner.
    PtyChild child = spawn_pty_child([](int first_slave_fd) {
        struct sigaction host_action{};
        host_action.sa_handler = &host_sigint_handler;
        sigemptyset(&host_action.sa_mask);
        if (::sigaction(SIGINT, &host_action, nullptr) != 0) ::_exit(1);

        int second_master_fd = -1;
        int second_slave_fd = -1;
        if (::openpty(&second_master_fd, &second_slave_fd, nullptr, nullptr, nullptr) != 0) ::_exit(1);
        {
            ManualClock clock;
            PosixTerminal first_terminal(clock, first_slave_fd, first_slave_fd,
                                         baseline_capabilities(), /*enable_capability_probes=*/false);
            PosixTerminal second_terminal(clock, second_slave_fd, second_slave_fd,
                                          baseline_capabilities(), /*enable_capability_probes=*/false);
            ui::Application first_application(first_terminal, clock);
            ui::Application second_application(second_terminal, clock);

            g_host_sigint_hits = 0;
            ::raise(SIGINT);
            if (g_host_sigint_hits != 1 || first_application.quit_requested() ||
                second_application.quit_requested())
                ::_exit(1);
        }
        ::close(second_master_fd);
        ::close(second_slave_fd);

        // D-024 does not own SIGINT at any session count. The host
        // disposition must still receive it after both sessions have ended.
        g_host_sigint_hits = 0;
        ::raise(SIGINT);
        if (g_host_sigint_hits != 1) ::_exit(1);
    });
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

template <TerminalProfile Profile>
void check_conservative_profile_does_not_enable_unsupported_input_modes() {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        const Capabilities profile = capabilities_for_profile(Profile);
        PosixClock clock;
        {
            PosixTerminal term(clock, slave_fd, slave_fd, Profile);
            if (term.capabilities() != profile) ::_exit(1);
            term.write("CONSERVATIVE-PROFILE");
        }
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    const std::string output = read_available(child.master_fd, 500);
    CK_CHECK(output.find("CONSERVATIVE-PROFILE") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1049h") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1002h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1006h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1016h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2004h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1004h") == std::string::npos);
    CK_CHECK(output.find("\x1B[>3u") == std::string::npos);
    CK_CHECK(output.find("\x1B[<u") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1002l") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1016l") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2004l") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1004l") == std::string::npos);
    CK_CHECK(output.find("\x1B]11;?\x1B\\") == std::string::npos);
    CK_CHECK(output.find("\x1B]10;?\x1B\\") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2026$p") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2026h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2026l") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1016$p") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2031h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2031l") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2031$p") == std::string::npos);
    CK_CHECK(output.find("\x1B[16t") == std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(each_conservative_terminal_profile_does_not_enable_unsupported_input_modes) {
    // Tmux, Screen, and the Linux console have different color guarantees,
    // but every one promises this same absence of optional input modes and
    // runtime probing. Exercise each concrete constructor path on a PTY so a
    // future profile edit cannot inherit a modern mode by accident.
    check_conservative_profile_does_not_enable_unsupported_input_modes<TerminalProfile::TmuxConservative>();
    check_conservative_profile_does_not_enable_unsupported_input_modes<TerminalProfile::ScreenConservative>();
    check_conservative_profile_does_not_enable_unsupported_input_modes<TerminalProfile::LinuxConsole>();
}

template <TerminalProfile Profile>
void check_conservative_profile_consumes_extensions_but_preserves_keyboard_input() {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd, Profile);
        const std::int64_t deadline = clock.now_nanos() + 2'000'000'000LL;
        bool ordinary_key = false;
        bool unexpected_extension_event = false;
        while (!ordinary_key && clock.now_nanos() < deadline) {
            for (const TerminalEvent& event : term.poll(deadline)) {
                if (const auto* key = std::get_if<KeyEvent>(&event)) {
                    ordinary_key = ordinary_key ||
                                   key->chord == KeyChord{Key::Char, Modifier::None, "q"};
                } else {
                    unexpected_extension_event = true;
                }
            }
        }
        if (!ordinary_key || unexpected_extension_event) ::_exit(1);
        term.write("CONSERVATIVE-INPUT-OK");
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    ::usleep(50'000);  // let the child enter its profile-specific terminal poll
    constexpr std::string_view kUnsupportedExtensionsThenKey =
        "\x1B[I\x1B[200~discard this\x1B[201~\x1B[<0;10;20Mq";
    CK_CHECK(::write(child.master_fd, kUnsupportedExtensionsThenKey.data(), kUnsupportedExtensionsThenKey.size()) ==
             static_cast<ssize_t>(kUnsupportedExtensionsThenKey.size()));
    const std::string output = read_until_contains(child.master_fd, "CONSERVATIVE-INPUT-OK", 1'500);
    CK_CHECK(output.find("CONSERVATIVE-INPUT-OK") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(conservative_terminal_profiles_consume_unrequested_extensions_on_a_real_pty) {
    check_conservative_profile_consumes_extensions_but_preserves_keyboard_input<
        TerminalProfile::TmuxConservative>();
    check_conservative_profile_consumes_extensions_but_preserves_keyboard_input<
        TerminalProfile::ScreenConservative>();
    check_conservative_profile_consumes_extensions_but_preserves_keyboard_input<
        TerminalProfile::LinuxConsole>();
}

CK_TEST(modern_vt_profile_enters_and_resets_only_its_documented_baseline_modes) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        const Capabilities profile = capabilities_for_profile(TerminalProfile::ModernVt);
        PosixClock clock;
        {
            // Named profiles default raw capability refinement off. This must
            // enter exactly ModernVt's declared baseline, not inherit the
            // constructor's unprofiled probe lifecycle.
            PosixTerminal term(clock, slave_fd, slave_fd, TerminalProfile::ModernVt);
            if (term.capabilities() != profile) ::_exit(1);
            term.write("MODERN-VT-PROFILE");
        }
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    const std::string output = read_available(child.master_fd, 500);
    CK_CHECK(output.find("MODERN-VT-PROFILE") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1049h") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1003h") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1006h") != std::string::npos);
    CK_CHECK(output.find("\x1B[?2004h") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1004h") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1003l") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1006l") != std::string::npos);
    CK_CHECK(output.find("\x1B[?2004l") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1004l") != std::string::npos);

    // The session hands the mouse pointer back on the way out, whether or
    // not it ever asked for a shape: a shape is host state, and this
    // profile enables the pointer-shape capability.
    CK_CHECK(output.find("\x1B]22;\x1B\\") != std::string::npos);

    // 1000, 1002 and 1016 are not part of this session. The first two are
    // distinct mouse protocols — 1002 reports motion only while a button is
    // held, which is not what this session asked for — and the third is
    // pixel positioning; resetting any of them would mutate a host state
    // this profile never established.
    CK_CHECK(output.find("\x1B[?1000h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1000l") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1002h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1002l") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1016h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1016l") == std::string::npos);
    CK_CHECK(output.find("\x1B]11;?\x1B\\") == std::string::npos);
    CK_CHECK(output.find("\x1B]10;?\x1B\\") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2026$p") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1016$p") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2031h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2031l") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2031$p") == std::string::npos);
    CK_CHECK(output.find("\x1B[16t") == std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(modern_vt_profile_decodes_its_documented_focus_paste_and_cell_mouse_input) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd, TerminalProfile::ModernVt);
        const std::int64_t deadline = clock.now_nanos() + 2'000'000'000LL;
        bool focus_gained = false;
        bool pasted_text = false;
        bool cell_mouse = false;
        while (!(focus_gained && pasted_text && cell_mouse) && clock.now_nanos() < deadline) {
            for (const TerminalEvent& event : term.poll(deadline)) {
                if (const auto* focus = std::get_if<FocusEvent>(&event)) {
                    focus_gained = focus_gained || focus->gained;
                } else if (const auto* text = std::get_if<TextEvent>(&event)) {
                    pasted_text = pasted_text || (text->from_paste && text->text == "profile paste");
                } else if (const auto* mouse = std::get_if<MouseEvent>(&event)) {
                    cell_mouse = cell_mouse ||
                                 (mouse->action == MouseAction::Down && mouse->button == MouseButton::Left &&
                                  mouse->cell == Point{9, 19} && !mouse->pixel.has_value());
                }
            }
        }
        if (focus_gained && pasted_text && cell_mouse) term.write("MODERN-VT-INPUT");
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    ::usleep(50'000);  // let the child enter its profile-specific terminal poll
    constexpr std::string_view kModernVtFocusAndPaste = "\x1B[I\x1B[200~profile paste\x1B[201~";
    CK_CHECK(::write(child.master_fd, kModernVtFocusAndPaste.data(), kModernVtFocusAndPaste.size()) ==
             static_cast<ssize_t>(kModernVtFocusAndPaste.size()));
    // The candidate marker is deliberately held for the documented quiet
    // period. A later independent mouse report therefore arrives after that
    // safe boundary, rather than being a potentially hostile paste tail.
    ::usleep(static_cast<useconds_t>(kPasteTerminationQuietNanos / 1'000 + 10'000));
    constexpr std::string_view kModernVtMouse = "\x1B[<0;10;20M";
    CK_CHECK(::write(child.master_fd, kModernVtMouse.data(), kModernVtMouse.size()) ==
             static_cast<ssize_t>(kModernVtMouse.size()));
    const std::string output = read_until_contains(child.master_fd, "MODERN-VT-INPUT", 1'500);
    CK_CHECK(output.find("MODERN-VT-INPUT") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(record_replay_preserves_a_real_posix_input_batch_and_output_operation) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal inner(clock, slave_fd, slave_fd, TerminalProfile::ModernVt);
        RecordingTerminal recording(inner);

        const std::int64_t deadline = clock.now_nanos() + 2'000'000'000LL;
        const std::vector<TerminalEvent> recorded_events = recording.poll(deadline);
        const bool received_key = std::ranges::any_of(recorded_events, [](const TerminalEvent& event) {
            const auto* key = std::get_if<KeyEvent>(&event);
            return key != nullptr && key->chord == KeyChord{Key::Char, Modifier::None, "r"};
        });
        if (!received_key) ::_exit(1);

        recording.write("PTY-RECORDED-OUTPUT");

        // The recording was sourced from a real raw PTY batch, but replay is
        // still deliberately terminal-less. Its sole source of input and
        // baseline state is the captured operation stream.
        ReplayTerminal replay(recording.recording(), recording.initial_capabilities(), recording.initial_size());
        if (replay.poll(0) != recorded_events) ::_exit(1);
        replay.write("PTY-RECORDED-OUTPUT");
        if (!replay.matches_recording()) ::_exit(1);

        inner.write("PTY-REPLAY-MATCH");
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    ::usleep(50'000);  // let the child enter the recording wrapper's real PTY poll
    constexpr std::string_view kInput = "r";
    CK_CHECK(::write(child.master_fd, kInput.data(), kInput.size()) == static_cast<ssize_t>(kInput.size()));
    const std::string output = read_until_contains(child.master_fd, "PTY-REPLAY-MATCH", 1'500);
    CK_CHECK(output.find("PTY-RECORDED-OUTPUT") != std::string::npos);
    CK_CHECK(output.find("PTY-REPLAY-MATCH") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(posix_terminal_pushes_and_pops_an_explicit_kitty_keyboard_session) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    {
        Capabilities kitty = baseline_capabilities();
        kitty.keyboard_protocol = KeyboardProtocol::Kitty;
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd, kitty, /*enable_capability_probes=*/false);
        const std::string entered = read_available(master_fd, 50);
        CK_CHECK(entered.find("\x1B[>3u") != std::string::npos);
    }
    const std::string restored = read_available(master_fd, 50);
    CK_CHECK(restored.find("\x1B[<u") != std::string::npos);
    CK_CHECK(restored.find("\x1B[<u") < restored.find("\x1B[?1049l"));
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_negotiates_full_kitty_enhancements_after_the_protocol_is_proven) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        bool flags_verified = false;
        bool release_tagged = false;
        const std::int64_t give_up = clock.now_nanos() + 3'000'000'000LL;
        while (clock.now_nanos() < give_up && !(flags_verified && release_tagged)) {
            const auto events = term.poll(clock.now_nanos() + 100'000'000LL);
            for (const TerminalEvent& event : events) {
                if (const auto* key = std::get_if<KeyEvent>(&event)) {
                    if (key->chord.key == Key::Enter && key->action == KeyAction::Release &&
                        key->reports_release)
                        release_tagged = true;
                }
            }
            flags_verified = flags_verified ||
                             (term.capabilities().kitty_keyboard_flags == kKittyRequestedFlags &&
                              keyboard_reports_all_releases(term.capabilities()));
        }
        if (flags_verified) term.write("KITTY-FLAGS-VERIFIED");
        if (release_tagged) term.write("ENTER-RELEASE-TAGGED");
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    ::usleep(50'000);  // let construction issue its non-blocking queries
    // The protocol's proof: it exists, nothing is in force yet.
    constexpr std::string_view kProofReply = "\x1B[?0u";
    CK_CHECK(::write(child.master_fd, kProofReply.data(), kProofReply.size()) ==
             static_cast<ssize_t>(kProofReply.size()));
    // The probe window closes ~250 ms after construction; the session then
    // pushes the full requested set and immediately asks what is in force.
    const std::string negotiated = read_available(child.master_fd, 1500);
    const std::size_t push_at = negotiated.find("\x1B[>27u");
    CK_CHECK(push_at != std::string::npos);
    CK_CHECK(negotiated.find("\x1B[?u", push_at) != std::string::npos);
    // Answer the readback, then hold Enter down and let it up.
    constexpr std::string_view kReadbackAndKeys = "\x1B[?27u\x1B[13u\x1B[13;1:3u";
    CK_CHECK(::write(child.master_fd, kReadbackAndKeys.data(), kReadbackAndKeys.size()) ==
             static_cast<ssize_t>(kReadbackAndKeys.size()));
    const std::string output = read_available(child.master_fd, 2000);
    CK_CHECK(output.find("KITTY-FLAGS-VERIFIED") != std::string::npos);
    CK_CHECK(output.find("ENTER-RELEASE-TAGGED") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(posix_terminal_demotes_a_kitty_host_that_honours_escape_codes_without_text) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        bool demoted = false;
        const std::int64_t give_up = clock.now_nanos() + 3'000'000'000LL;
        while (clock.now_nanos() < give_up && !demoted) {
            (void)term.poll(clock.now_nanos() + 100'000'000LL);
            demoted = term.capabilities().keyboard_protocol == KeyboardProtocol::Kitty &&
                      term.capabilities().kitty_keyboard_flags == kKittyBaselineFlags;
        }
        if (demoted) term.write("KITTY-DEMOTED-TO-BASELINE");
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    ::usleep(50'000);
    constexpr std::string_view kProofReply = "\x1B[?0u";
    CK_CHECK(::write(child.master_fd, kProofReply.data(), kProofReply.size()) ==
             static_cast<ssize_t>(kProofReply.size()));
    const std::string negotiated = read_available(child.master_fd, 1500);
    const std::size_t push_at = negotiated.find("\x1B[>27u");
    CK_CHECK(push_at != std::string::npos);
    // All-keys-as-escape-codes without associated text: the session must
    // not stay on a set that would force layout guessing (D-055). It steps
    // its own entry back to the baseline and asks again.
    constexpr std::string_view kHalfHonoured = "\x1B[?11u";
    CK_CHECK(::write(child.master_fd, kHalfHonoured.data(), kHalfHonoured.size()) ==
             static_cast<ssize_t>(kHalfHonoured.size()));
    const std::string demotion = read_available(child.master_fd, 1000);
    const std::size_t pop_at = demotion.find("\x1B[<u");
    CK_CHECK(pop_at != std::string::npos);
    const std::size_t repush_at = demotion.find("\x1B[>3u", pop_at);
    CK_CHECK(repush_at != std::string::npos);
    CK_CHECK(demotion.find("\x1B[?u", repush_at) != std::string::npos);
    constexpr std::string_view kBaselineReadback = "\x1B[?3u";
    CK_CHECK(::write(child.master_fd, kBaselineReadback.data(), kBaselineReadback.size()) ==
             static_cast<ssize_t>(kBaselineReadback.size()));
    const std::string output = read_available(child.master_fd, 2000);
    CK_CHECK(output.find("KITTY-DEMOTED-TO-BASELINE") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(posix_terminal_enters_restores_and_decodes_an_explicit_x10_mouse_session) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    {
        Capabilities x10 = baseline_capabilities();
        x10.mouse_protocol = MouseProtocol::X10;
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd, x10, /*enable_capability_probes=*/false);

        const std::string entered = read_available(master_fd, 50);
        CK_CHECK(entered.find("\x1B[?9h") != std::string::npos);
        CK_CHECK(entered.find("\x1B[?1003h") == std::string::npos);

        constexpr std::string_view kLeftPress = "\x1B[M \x2A\x25";
        CK_CHECK(::write(master_fd, kLeftPress.data(), kLeftPress.size()) ==
                 static_cast<ssize_t>(kLeftPress.size()));
        const auto events = term.poll(clock.now_nanos());
        CK_CHECK(events.size() == 1);
        const auto mouse = std::get<MouseEvent>(events.front());
        CK_CHECK(mouse.action == MouseAction::Down);
        CK_CHECK(mouse.button == MouseButton::Left);
        CK_CHECK(mouse.cell == (Point{9, 4}));
    }
    const std::string restored = read_available(master_fd, 50);
    CK_CHECK(restored.find("\x1B[?9l") != std::string::npos);
    CK_CHECK(restored.find("\x1B[?9l") < restored.find("\x1B[?1049l"));
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_delivers_dual_space_sgr_pixel_mouse_events_over_a_pty) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    {
        // This is an explicit, authoritative profile: the terminal has already
        // established mode 1016 and its cell-pixel metric. Keep probes off so
        // the test isolates a real SGR-pixel input report from probe traffic.
        Capabilities pixel_mouse = baseline_capabilities();
        pixel_mouse.mouse_protocol = MouseProtocol::SGR;
        pixel_mouse.pixel_mouse = true;
        pixel_mouse.cell_pixels = Size{8, 16};
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd, pixel_mouse,
                           /*enable_capability_probes=*/false);

        const std::string entered = read_available(master_fd, 50);
        CK_CHECK(entered.find("\x1B[?1003h") != std::string::npos);
        CK_CHECK(entered.find("\x1B[?1006h") != std::string::npos);
        CK_CHECK(entered.find("\x1B[?1016h") != std::string::npos);

        // SGR-pixel coordinates are one-based. The report must retain the
        // original pixel position and derive the independent cell position
        // from the authoritative 8x16 cell metric.
        constexpr std::string_view kPixelPress = "\x1B[<0;41;33M";
        CK_CHECK(::write(master_fd, kPixelPress.data(), kPixelPress.size()) ==
                 static_cast<ssize_t>(kPixelPress.size()));
        const auto events = term.poll(clock.now_nanos());
        CK_CHECK(events.size() == 1);
        const auto mouse = std::get<MouseEvent>(events.front());
        CK_CHECK(mouse.action == MouseAction::Down);
        CK_CHECK(mouse.button == MouseButton::Left);
        CK_CHECK((mouse.pixel == PixelPoint{40, 32}));
        CK_CHECK(mouse.cell == (Point{5, 2}));
    }
    const std::string restored = read_available(master_fd, 50);
    CK_CHECK(restored.find("\x1B[?1003l") != std::string::npos);
    CK_CHECK(restored.find("\x1B[?1006l") != std::string::npos);
    CK_CHECK(restored.find("\x1B[?1016l") != std::string::npos);
    CK_CHECK(restored.find("\x1B[?1016l") < restored.find("\x1B[?1049l"));
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_refines_the_baseline_only_from_a_timely_capability_probe_response) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        if (term.capabilities() != baseline_capabilities()) ::_exit(1);
        term.write("BASELINE-CAPS");
        const auto events = term.poll(clock.now_nanos() + 2'000'000'000LL);
        bool saw_dark_scheme = false;
        bool saw_sixel = false;
        bool saw_sixel_limits = false;
        bool saw_pixel_mouse = false;
        bool saw_synchronized_output = false;
        bool saw_color_scheme_notifications = false;
        for (const TerminalEvent& event : events) {
            const auto* changed = std::get_if<CapabilityChangedEvent>(&event);
            if (changed == nullptr) continue;
            saw_dark_scheme = saw_dark_scheme || changed->capabilities.color_scheme == ColorScheme::Dark;
            saw_sixel = saw_sixel || changed->capabilities.sixel_graphics;
            saw_sixel_limits = saw_sixel_limits ||
                               (changed->capabilities.sixel_color_registers == 16 &&
                                changed->capabilities.sixel_max_geometry == Size{640, 480});
            saw_pixel_mouse = saw_pixel_mouse ||
                              (changed->capabilities.pixel_mouse &&
                               changed->capabilities.cell_pixels == Size{8, 16});
            saw_synchronized_output = saw_synchronized_output || changed->capabilities.synchronized_output;
            saw_color_scheme_notifications = saw_color_scheme_notifications ||
                                             changed->capabilities.color_scheme_notifications;
        }
        if (saw_dark_scheme && saw_sixel && saw_sixel_limits && saw_pixel_mouse && saw_synchronized_output &&
            saw_color_scheme_notifications)
            term.write("TIMELY-PROBES-APPLIED");
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    ::usleep(50'000);  // let construction issue its non-blocking queries
    constexpr std::string_view kProbeReplies =
        "\x1B]11;rgb:0000/0000/0000\x07\x1B[?62;1;4;6c\x1B[?1;0;16S\x1B[?2;0;640;480S"
        "\x1B[?2026;1$y\x1B[?1016;1$y\x1B[?2031;1$y\x1B[6;16;8t";
    CK_CHECK(::write(child.master_fd, kProbeReplies.data(), kProbeReplies.size()) ==
             static_cast<ssize_t>(kProbeReplies.size()));
    const std::string output = read_available(child.master_fd, 2000);
    CK_CHECK(output.find("BASELINE-CAPS") != std::string::npos);
    CK_CHECK(output.find("\x1B]11;?\x1B\\") != std::string::npos);
    CK_CHECK(output.find("\x1B]10;?\x1B\\") != std::string::npos);
    CK_CHECK(output.find("\x1B[?2026$p") != std::string::npos);
    CK_CHECK(output.find("\x1B[?2026h\x1B[?2026$p\x1B[?2026l") != std::string::npos);
    CK_CHECK(output.find("\x1B[?2031$p") != std::string::npos);
    CK_CHECK(output.find("\x1B[c") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1;4;0S") != std::string::npos);
    CK_CHECK(output.find("\x1B[?2;4;0S") != std::string::npos);
    CK_CHECK(output.find("\x1B[?1016$p") != std::string::npos);
    CK_CHECK(output.find("\x1B[16t") != std::string::npos);
    CK_CHECK(output.find("TIMELY-PROBES-APPLIED") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(posix_terminal_an_event_bearing_poll_still_reads_buffered_probe_replies) {
    // A capability override set between construction and the first poll
    // queues a change event for that poll. Returning the event WITHOUT
    // reading is how a session used to lose its graphics: the caller's
    // next poll can be a whole frame away, a first frame can be expensive,
    // and the probe replies — buffered well inside the window — were then
    // read only after it had closed and rejected as stale. The batch must
    // harvest what is already readable alongside the event it delivers.
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);
        CapabilityOverrides overrides;
        overrides.sixel_color_registers = 64;
        term.set_capability_overrides(overrides);
        // The terminal answered DA1 promptly; the reply is buffered before
        // the caller ever polls, 40 ms into a 250 ms window.
        constexpr std::string_view kDa1 = "\x1B[?62;1;4;6c";
        CK_CHECK(::write(master_fd, kDa1.data(), kDa1.size()) ==
                 static_cast<ssize_t>(kDa1.size()));
        clock.advance(40'000'000);
        const auto batch = term.poll(clock.now_nanos());
        bool saw_override = false;
        bool saw_sixel = false;
        for (const TerminalEvent& event : batch) {
            const auto* changed = std::get_if<CapabilityChangedEvent>(&event);
            if (changed == nullptr) continue;
            saw_override = saw_override || changed->capabilities.sixel_color_registers == 64;
            saw_sixel = saw_sixel || changed->capabilities.sixel_graphics;
        }
        CK_CHECK(saw_override);
        CK_CHECK(saw_sixel);
        CK_CHECK(term.capabilities().sixel_graphics);
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_a_resize_batch_still_reads_buffered_probe_replies) {
    // The resize branch had the same shape: a batch carrying the resize
    // (and the re-probe it begins) returned before reading, so replies
    // already in the buffer aged past the very window the re-probe opened.
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);
        constexpr std::string_view kDa1 = "\x1B[?62;1;4;6c";
        CK_CHECK(::write(master_fd, kDa1.data(), kDa1.size()) ==
                 static_cast<ssize_t>(kDa1.size()));
        struct winsize ws{};
        ws.ws_row = 30;
        ws.ws_col = 100;
        CK_CHECK(::ioctl(master_fd, TIOCSWINSZ, &ws) == 0);
        clock.advance(40'000'000);
        const auto batch = term.poll(clock.now_nanos());
        bool saw_resize = false;
        for (const TerminalEvent& event : batch)
            saw_resize = saw_resize || std::holds_alternative<ResizeEvent>(event);
        CK_CHECK(saw_resize);
        CK_CHECK(term.capabilities().sixel_graphics);
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_capability_probe_expiry_uses_the_injected_clock) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);
        // No wall-clock delay belongs in this contract: advancing the clock
        // beyond the bounded window makes this otherwise-valid reply late.
        clock.advance(250'000'001);
        // Without prior positive DECRQM evidence, a post-deadline mode-2031
        // DSR is still merely stale probe traffic and must not establish a
        // live notification capability.
        constexpr std::string_view kDarkReply = "\x1B[?997;1n";
        CK_CHECK(::write(master_fd, kDarkReply.data(), kDarkReply.size()) ==
                 static_cast<ssize_t>(kDarkReply.size()));
        const auto events = term.poll(clock.now_nanos());
        CK_CHECK(events.empty());
        CK_CHECK(term.capabilities() == baseline_capabilities());
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_rejects_late_probe_state_before_decoding_later_bytes_in_the_same_read) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);
        clock.advance(250'000'001);
        // In one OS read, a late mode/metric pair must not turn the trailing
        // SGR report into a pixel event. The ordinary SGR cell coordinates
        // remain the only trustworthy interpretation.
        constexpr std::string_view late_replies_then_mouse =
            "\x1B[?1016;1$y\x1B[6;16;8t\x1B[<0;41;33M";
        CK_CHECK(::write(master_fd, late_replies_then_mouse.data(), late_replies_then_mouse.size()) ==
                 static_cast<ssize_t>(late_replies_then_mouse.size()));
        const auto events = term.poll(clock.now_nanos());
        CK_CHECK(events.size() == 1);
        const auto mouse = std::get<MouseEvent>(events.front());
        CK_CHECK(mouse.cell == (Point{40, 32}));
        CK_CHECK(!mouse.pixel.has_value());
        CK_CHECK(term.capabilities() == baseline_capabilities());
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_accepts_verified_live_color_scheme_notifications_after_probe_expiry) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);

        constexpr std::string_view kModeEnabled = "\x1B[?2031;1$y";
        CK_CHECK(::write(master_fd, kModeEnabled.data(), kModeEnabled.size()) ==
                 static_cast<ssize_t>(kModeEnabled.size()));
        const auto enabled_events = term.poll(clock.now_nanos());
        CK_CHECK(enabled_events.size() == 1);
        CK_CHECK(std::get<CapabilityChangedEvent>(enabled_events[0]).capabilities.color_scheme_notifications);

        // Expire ordinary probe evidence. A later 997 DSR remains valid only
        // because the earlier DECRQM report established this live stream.
        clock.advance(250'000'001);
        constexpr std::string_view kLightNotification = "\x1B[?997;2n";
        CK_CHECK(::write(master_fd, kLightNotification.data(), kLightNotification.size()) ==
                 static_cast<ssize_t>(kLightNotification.size()));
        const auto notification_events = term.poll(clock.now_nanos());
        CK_CHECK(notification_events.size() == 1);
        const auto& caps = std::get<CapabilityChangedEvent>(notification_events[0]).capabilities;
        CK_CHECK(caps.color_scheme_notifications);
        CK_CHECK(caps.color_scheme == ColorScheme::Light);
        CK_CHECK(term.capabilities() == caps);
    }
    const std::string output = read_available(master_fd, 50);
    CK_CHECK(output.find("\x1B[?2031h") != std::string::npos);
    CK_CHECK(output.find("\x1B[?2031$p") != std::string::npos);
    CK_CHECK(output.find("\x1B[?2031l") != std::string::npos);
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_discards_late_capability_probe_responses_and_reprobes_after_resize) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        if (term.capabilities() != baseline_capabilities()) ::_exit(1);
        term.write("BASELINE-WINDOW-OPEN");
        const auto late = term.poll(clock.now_nanos() + 2'000'000'000LL);
        if (!late.empty() || term.capabilities() != baseline_capabilities()) ::_exit(1);
        term.write("LATE-PROBE-IGNORED");

        bool resized = false;
        const auto resize_deadline = clock.now_nanos() + 2'000'000'000LL;
        while (!resized && clock.now_nanos() < resize_deadline) {
            for (const TerminalEvent& event : term.poll(resize_deadline))
                if (std::holds_alternative<ResizeEvent>(event)) resized = true;
        }
        if (!resized) ::_exit(1);
        term.write("RESIZE-REPROBED");

        const auto metrics = term.poll(clock.now_nanos() + 2'000'000'000LL);
        for (const TerminalEvent& event : metrics) {
            const auto* changed = std::get_if<CapabilityChangedEvent>(&event);
            if (changed != nullptr && changed->capabilities.cell_pixels == Size{8, 16} &&
                !changed->capabilities.pixel_mouse)
                term.write("LATE-PIXEL-MODE-NOT-REUSED");
        }
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    const std::string timed_out = read_available(child.master_fd, 600);
    CK_CHECK(timed_out.find("BASELINE-WINDOW-OPEN") != std::string::npos);
    constexpr std::string_view kLateReplies = "\x1B]11;rgb:ffff/ffff/ffff\x07\x1B[?1016;1$y";
    CK_CHECK(::write(child.master_fd, kLateReplies.data(), kLateReplies.size()) ==
             static_cast<ssize_t>(kLateReplies.size()));
    const std::string late = read_until_contains(child.master_fd, "LATE-PROBE-IGNORED", 1000);

    struct winsize ws{};
    ws.ws_col = 100;
    ws.ws_row = 40;
    CK_CHECK(::ioctl(child.master_fd, TIOCSWINSZ, &ws) == 0);
    CK_CHECK(::kill(child.pid, SIGWINCH) == 0);
    const std::string resized = read_until_contains(child.master_fd, "RESIZE-REPROBED", 1000);
    CK_CHECK(resized.find("RESIZE-REPROBED") != std::string::npos);
    constexpr std::string_view kMetricsReply = "\x1B[6;16;8t";
    CK_CHECK(::write(child.master_fd, kMetricsReply.data(), kMetricsReply.size()) ==
             static_cast<ssize_t>(kMetricsReply.size()));
    const std::string metrics = read_until_contains(child.master_fd, "LATE-PIXEL-MODE-NOT-REUSED", 1000);
    const std::string all_output = timed_out + late + resized + metrics;
    // The child writes this marker after processing the late reply and before
    // blocking for the resize. PTY delivery is asynchronous, so the parent
    // can observe it in either adjacent drain window.
    CK_CHECK(all_output.find("LATE-PROBE-IGNORED") != std::string::npos);
    CK_CHECK(all_output.find("LATE-PIXEL-MODE-NOT-REUSED") != std::string::npos);
    CK_CHECK(count_occurrences(all_output, "\x1B]11;?\x1B\\") == 2);
    CK_CHECK(count_occurrences(all_output, "\x1B]10;?\x1B\\") == 2);
    CK_CHECK(count_occurrences(all_output, "\x1B[?2026$p") == 2);
    CK_CHECK(count_occurrences(all_output, "\x1B[?2031$p") == 2);
    CK_CHECK(count_occurrences(all_output, "\x1B[c") == 2);
    CK_CHECK(count_occurrences(all_output, "\x1B[?1016h") == 2);
    CK_CHECK(count_occurrences(all_output, "\x1B[?1016$p") == 2);
    CK_CHECK(count_occurrences(all_output, "\x1B[16t") == 2);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(posix_terminal_invalidates_runtime_pixel_metrics_before_a_resize_reprobe) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);

        constexpr std::string_view kInitialMetricAndMode = "\x1B[?1016;1$y\x1B[6;18;9t";
        CK_CHECK(::write(master_fd, kInitialMetricAndMode.data(), kInitialMetricAndMode.size()) ==
                 static_cast<ssize_t>(kInitialMetricAndMode.size()));
        const auto initial_events = term.poll(clock.now_nanos());
        CK_CHECK(initial_events.size() == 1);
        CK_CHECK(std::get<CapabilityChangedEvent>(initial_events.front()).capabilities.cell_pixels == (Size{9, 18}));
        CK_CHECK(term.capabilities().pixel_mouse);

        struct winsize resized {};
        resized.ws_col = 100;
        resized.ws_row = 40;
        CK_CHECK(::ioctl(master_fd, TIOCSWINSZ, &resized) == 0);
        const auto resize_events = term.poll(clock.now_nanos());
        CK_CHECK(resize_events.size() == 2);
        CK_CHECK(std::get<ResizeEvent>(resize_events[0]).cells == (Size{100, 40}));
        const auto& invalidated = std::get<CapabilityChangedEvent>(resize_events[1]).capabilities;
        CK_CHECK(invalidated.cell_pixels == (Size{}));
        CK_CHECK(!invalidated.pixel_mouse);
        CK_CHECK(term.capabilities() == invalidated);

        // The new geometry alone is not enough: a fresh probe window must
        // also receive fresh mode evidence before pixel coordinates return.
        constexpr std::string_view kNewMetric = "\x1B[6;16;8t";
        CK_CHECK(::write(master_fd, kNewMetric.data(), kNewMetric.size()) == static_cast<ssize_t>(kNewMetric.size()));
        const auto metric_events = term.poll(clock.now_nanos());
        CK_CHECK(metric_events.size() == 1);
        CK_CHECK(std::get<CapabilityChangedEvent>(metric_events.front()).capabilities.cell_pixels == (Size{8, 16}));
        CK_CHECK(!term.capabilities().pixel_mouse);

        constexpr std::string_view kNewMode = "\x1B[?1016;1$y";
        CK_CHECK(::write(master_fd, kNewMode.data(), kNewMode.size()) == static_cast<ssize_t>(kNewMode.size()));
        const auto mode_events = term.poll(clock.now_nanos());
        CK_CHECK(mode_events.size() == 1);
        CK_CHECK(term.capabilities().cell_pixels == (Size{8, 16}));
        CK_CHECK(term.capabilities().pixel_mouse);
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_withdraws_runtime_sixel_geometry_until_a_resize_reprobe_confirms_it) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);
        constexpr std::string_view kInitialGeometry = "\x1B[?2;0;640;480S";
        CK_CHECK(::write(master_fd, kInitialGeometry.data(), kInitialGeometry.size()) ==
                 static_cast<ssize_t>(kInitialGeometry.size()));
        const auto initial_events = term.poll(clock.now_nanos());
        CK_CHECK(initial_events.size() == 1);
        CK_CHECK(term.capabilities().sixel_graphics);
        CK_CHECK(term.capabilities().sixel_max_geometry == (Size{640, 480}));

        struct winsize resized {};
        resized.ws_col = 100;
        resized.ws_row = 40;
        CK_CHECK(::ioctl(master_fd, TIOCSWINSZ, &resized) == 0);
        const auto resize_events = term.poll(clock.now_nanos());
        CK_CHECK(resize_events.size() == 2);
        CK_CHECK(std::holds_alternative<ResizeEvent>(resize_events[0]));
        const auto& withdrawn = std::get<CapabilityChangedEvent>(resize_events[1]).capabilities;
        CK_CHECK(!withdrawn.sixel_graphics);
        CK_CHECK(withdrawn.sixel_max_geometry == (Size{}));

        // DA1 still advertises that this terminal has Sixel support, but it
        // does not describe the new window-limited maximum. It cannot restore
        // raster output after a resize by itself.
        constexpr std::string_view kDa1Sixel = "\x1B[?62;1;4;6c";
        CK_CHECK(::write(master_fd, kDa1Sixel.data(), kDa1Sixel.size()) ==
                 static_cast<ssize_t>(kDa1Sixel.size()));
        CK_CHECK(term.poll(clock.now_nanos()).empty());
        CK_CHECK(!term.capabilities().sixel_graphics);

        constexpr std::string_view kReprobedGeometry = "\x1B[?2;0;320;240S";
        CK_CHECK(::write(master_fd, kReprobedGeometry.data(), kReprobedGeometry.size()) ==
                 static_cast<ssize_t>(kReprobedGeometry.size()));
        const auto geometry_events = term.poll(clock.now_nanos());
        CK_CHECK(geometry_events.size() == 1);
        CK_CHECK(term.capabilities().sixel_graphics);
        CK_CHECK(term.capabilities().sixel_max_geometry == (Size{320, 240}));
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_keeps_sixel_across_a_resize_when_the_host_reports_no_geometry) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);
        // DA1 with parameter 4 and nothing else: this terminal draws Sixel
        // and does not implement XTSMGRAPHICS, which is the common case
        // among terminals that draw Sixel at all.
        constexpr std::string_view kDa1Sixel = "\x1B[?62;1;4;6c";
        CK_CHECK(::write(master_fd, kDa1Sixel.data(), kDa1Sixel.size()) ==
                 static_cast<ssize_t>(kDa1Sixel.size()));
        CK_CHECK(term.poll(clock.now_nanos()).size() == 1);
        CK_CHECK(term.capabilities().sixel_graphics);
        CK_CHECK(term.capabilities().sixel_max_geometry == (Size{}));

        struct winsize resized {};
        resized.ws_col = 100;
        resized.ws_row = 40;
        CK_CHECK(::ioctl(master_fd, TIOCSWINSZ, &resized) == 0);
        const auto resize_events = term.poll(clock.now_nanos());
        CK_CHECK(!resize_events.empty());
        CK_CHECK(std::holds_alternative<ResizeEvent>(resize_events[0]));
        // Nothing about this terminal's graphics depended on its window
        // size, and no reply is outstanding that could turn them back on.
        // Withdrawing them here left every picture a cell fallback for the
        // rest of the session.
        CK_CHECK(term.capabilities().sixel_graphics);
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_restores_withheld_sixel_when_the_resize_reprobe_goes_unanswered) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);
        constexpr std::string_view kInitialGeometry = "\x1B[?2;0;640;480S";
        CK_CHECK(::write(master_fd, kInitialGeometry.data(), kInitialGeometry.size()) ==
                 static_cast<ssize_t>(kInitialGeometry.size()));
        CK_CHECK(term.poll(clock.now_nanos()).size() == 1);
        CK_CHECK(term.capabilities().sixel_graphics);

        struct winsize resized {};
        resized.ws_col = 100;
        resized.ws_row = 40;
        CK_CHECK(::ioctl(master_fd, TIOCSWINSZ, &resized) == 0);
        (void)term.poll(clock.now_nanos());
        // Withheld while the fresh bound is in flight — this terminal has
        // answered that query before, so one is worth waiting for.
        CK_CHECK(!term.capabilities().sixel_graphics);

        // It never comes. A terminal that answered once and not again lost
        // a reply; it did not lose its graphics, and the probe window
        // closing is what ends the wait.
        clock.advance(400'000'000);
        (void)term.poll(clock.now_nanos());
        CK_CHECK(term.capabilities().sixel_graphics);
        CK_CHECK(term.capabilities().sixel_max_geometry == (Size{}));
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_reprobe_rejects_a_metric_reply_that_started_before_resize) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);

        constexpr std::string_view kMetricPrefix = "\x1B[6;18;9";
        CK_CHECK(::write(master_fd, kMetricPrefix.data(), kMetricPrefix.size()) ==
                 static_cast<ssize_t>(kMetricPrefix.size()));
        CK_CHECK(term.poll(clock.now_nanos()).empty());

        struct winsize resized {};
        resized.ws_col = 100;
        resized.ws_row = 40;
        CK_CHECK(::ioctl(master_fd, TIOCSWINSZ, &resized) == 0);
        const auto resize_events = term.poll(clock.now_nanos());
        CK_CHECK(resize_events.size() == 1);
        CK_CHECK(std::get<ResizeEvent>(resize_events.front()).cells == (Size{100, 40}));

        constexpr std::string_view kOldMetricFinal = "t";
        CK_CHECK(::write(master_fd, kOldMetricFinal.data(), kOldMetricFinal.size()) ==
                 static_cast<ssize_t>(kOldMetricFinal.size()));
        CK_CHECK(term.poll(clock.now_nanos()).empty());
        CK_CHECK(term.capabilities().cell_pixels == (Size{}));

        constexpr std::string_view kNewMetric = "\x1B[6;16;8t";
        CK_CHECK(::write(master_fd, kNewMetric.data(), kNewMetric.size()) == static_cast<ssize_t>(kNewMetric.size()));
        const auto metric_events = term.poll(clock.now_nanos());
        CK_CHECK(metric_events.size() == 1);
        CK_CHECK(term.capabilities().cell_pixels == (Size{8, 16}));
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_keeps_authoritative_pixel_metrics_across_resize_without_probes) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        Capabilities forced = baseline_capabilities();
        forced.cell_pixels = Size{9, 18};
        forced.pixel_mouse = true;
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd, forced, /*enable_capability_probes=*/false);

        struct winsize resized {};
        resized.ws_col = 100;
        resized.ws_row = 40;
        CK_CHECK(::ioctl(master_fd, TIOCSWINSZ, &resized) == 0);
        const auto events = term.poll(clock.now_nanos());
        CK_CHECK(events.size() == 1);
        CK_CHECK(std::get<ResizeEvent>(events.front()).cells == (Size{100, 40}));
        CK_CHECK(term.capabilities() == forced);
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_applies_runtime_capability_overrides_and_reports_the_effective_change) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        Capabilities observed = baseline_capabilities();
        observed.sixel_graphics = true;
        observed.sixel_color_registers = 256;
        observed.cell_pixels = Size{8, 16};
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd, observed, /*enable_capability_probes=*/false);

        CapabilityOverrides overrides;
        overrides.sixel_graphics = false;
        overrides.sixel_color_registers = 64;
        overrides.cell_pixels = Size{9, 18};
        term.set_capability_overrides(overrides);

        const auto forced_events = term.poll(clock.now_nanos());
        CK_CHECK(forced_events.size() == 1);
        const auto& forced = std::get<CapabilityChangedEvent>(forced_events.front()).capabilities;
        CK_CHECK(!forced.sixel_graphics);
        CK_CHECK(forced.sixel_color_registers == 64);
        CK_CHECK(forced.cell_pixels == (Size{9, 18}));

        term.set_capability_overrides({});
        const auto restored_events = term.poll(clock.now_nanos());
        CK_CHECK(restored_events.size() == 1);
        CK_CHECK(std::get<CapabilityChangedEvent>(restored_events.front()).capabilities == observed);
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_can_keep_an_explicit_capability_profile_authoritative) {
    // This contract is about the backend's explicit-profile policy, not
    // process teardown. A direct isolated PTY removes fork/ack scheduling
    // from that observation; dedicated child tests cover output and signal
    // restoration elsewhere in this file.
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    Capabilities forced = baseline_capabilities();
    forced.synchronized_output = true;
    PosixClock clock;
    PosixTerminal term(clock, slave_fd, slave_fd, forced, /*enable_capability_probes=*/false);
    CK_CHECK(term.capabilities() == forced);
    term.write("FORCED-PROFILE");
    const std::string output = read_until_contains(master_fd, "FORCED-PROFILE", 1'500);
    CK_CHECK(output.find("FORCED-PROFILE") != std::string::npos);
    CK_CHECK(output.find("\x1B]11;?\x1B\\") == std::string::npos);
    CK_CHECK(output.find("\x1B]10;?\x1B\\") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2026$p") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2026h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2026l") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2031h") == std::string::npos);
    CK_CHECK(output.find("\x1B[?2031$p") == std::string::npos);
    CK_CHECK(output.find("\x1B[c") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1016$p") == std::string::npos);
    CK_CHECK(output.find("\x1B[?1016h") == std::string::npos);
    CK_CHECK(output.find("\x1B[16t") == std::string::npos);
    term.restore();
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_with_probes_disabled_rejects_raw_refinement_before_same_batch_input) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        // This is an explicit host policy: ordinary SGR input is trusted, but
        // probe replies are not. The raw mode/metric bytes must not make the
        // following report pixel-addressed when probing is disabled.
        Capabilities forced = baseline_capabilities();
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd, forced, /*enable_capability_probes=*/false);
        constexpr std::string_view replies_then_mouse =
            "\x1B[?1016;1$y\x1B[6;16;8t\x1B[<0;41;33M";
        CK_CHECK(::write(master_fd, replies_then_mouse.data(), replies_then_mouse.size()) ==
                 static_cast<ssize_t>(replies_then_mouse.size()));

        const auto events = term.poll(clock.now_nanos());
        CK_CHECK(events.size() == 1);
        const auto mouse = std::get<MouseEvent>(events.front());
        CK_CHECK(mouse.cell == (Point{40, 32}));
        CK_CHECK(!mouse.pixel.has_value());
        CK_CHECK(term.capabilities() == forced);
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_never_misinterprets_sgr_pixel_coordinates_while_their_metric_is_unproved) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);
        constexpr std::string_view kMouseReport = "\x1B[<0;41;33M";
        CK_CHECK(::write(master_fd, kMouseReport.data(), kMouseReport.size()) ==
                 static_cast<ssize_t>(kMouseReport.size()));

        // The session has temporarily enabled mode 1016 to establish whether
        // it is usable. Before both replies prove the coordinate space, its
        // numbers must not be exposed as a fictitious cell click.
        CK_CHECK(term.poll(clock.now_nanos()).empty());

        constexpr std::string_view kProofThenMouse =
            "\x1B[6;16;8t\x1B[?1016;1$y\x1B[<0;41;33M";
        CK_CHECK(::write(master_fd, kProofThenMouse.data(), kProofThenMouse.size()) ==
                 static_cast<ssize_t>(kProofThenMouse.size()));
        const auto events = term.poll(clock.now_nanos());
        CK_CHECK(events.size() == 3);
        CK_CHECK(std::holds_alternative<CapabilityChangedEvent>(events[0]));
        CK_CHECK(std::holds_alternative<CapabilityChangedEvent>(events[1]));
        const auto& mouse = std::get<MouseEvent>(events[2]);
        CK_CHECK(mouse.pixel.has_value());
        if (mouse.pixel) CK_CHECK(*mouse.pixel == (PixelPoint{40, 32}));
        CK_CHECK(mouse.cell == (Point{5, 2}));
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_resets_an_unproved_pixel_mode_before_restoring_ordinary_sgr_input) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd);
        read_available(master_fd, 50);  // session entry and initial probe

        clock.advance(250'000'001);
        CK_CHECK(term.poll(clock.now_nanos()).empty());
        const std::string expired_probe_output = read_available(master_fd, 50);
        CK_CHECK(expired_probe_output.find("\x1B[?1016l") != std::string::npos);

        // Once the backend has restored ordinary SGR coordinates, later
        // reports again have the documented cell-space interpretation.
        constexpr std::string_view kMouseReport = "\x1B[<0;41;33M";
        CK_CHECK(::write(master_fd, kMouseReport.data(), kMouseReport.size()) ==
                 static_cast<ssize_t>(kMouseReport.size()));
        const auto events = term.poll(clock.now_nanos());
        CK_CHECK(events.size() == 1);
        const auto& mouse = std::get<MouseEvent>(events.front());
        CK_CHECK(mouse.cell == (Point{40, 32}));
        CK_CHECK(!mouse.pixel.has_value());
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(posix_terminal_delivers_a_backpressured_pty_write_without_truncation) {
    constexpr std::size_t kPayloadBytes = 256U * 1024U;
    int master_fd = -1;
    int slave_fd = -1;
    int status_pipe[2] = {-1, -1};
    int acknowledge_pipe[2] = {-1, -1};
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    CK_CHECK(::pipe(status_pipe) == 0);
    CK_CHECK(::pipe(acknowledge_pipe) == 0);

    const pid_t child = ::fork();
    CK_CHECK(child >= 0);
    if (child == 0) {
        ::close(master_fd);
        ::close(status_pipe[0]);
        ::close(acknowledge_pipe[1]);
        PosixClock clock;
        {
            PosixTerminal terminal(clock, slave_fd, slave_fd);
            const char ready = 'R';
            if (::write(status_pipe[1], &ready, 1) != 1) ::_exit(1);
            terminal.write(std::string(kPayloadBytes, 'Z'));
            const char delivered = 'D';
            if (::write(status_pipe[1], &delivered, 1) != 1) ::_exit(1);
            char acknowledged = 0;
            if (::read(acknowledge_pipe[0], &acknowledged, 1) != 1 || acknowledged != 'A') ::_exit(1);
        }
        ::close(acknowledge_pipe[0]);
        ::close(status_pipe[1]);
        ::_exit(0);
    }

    ::close(slave_fd);
    ::close(status_pipe[1]);
    ::close(acknowledge_pipe[0]);
    CK_CHECK(::fcntl(status_pipe[0], F_SETFL, O_NONBLOCK) == 0);
    std::string output = read_output_until_status(master_fd, status_pipe[0], 'R', 1'000);
    output += read_output_until_status(master_fd, status_pipe[0], 'D', 15'000);
    // Keep the slave open until the parent has drained the payload. A PTY may
    // discard output that remains queued when its slave is closed, which is a
    // transport-lifecycle concern rather than evidence that write_all() lost
    // bytes before the terminal accepted them.
    output += read_available(master_fd, 250);
    const std::size_t payload_count = count_occurrences(output, "Z");
    const char acknowledged = 'A';
    CK_CHECK(::write(acknowledge_pipe[1], &acknowledged, 1) == 1);
    // Drain while waiting: the child is only past its 256 KiB write once this
    // side has consumed it, so a plain wait_child() here is a deadlock.
    const int child_status = wait_child_draining(child, master_fd, &output);

    CK_CHECK(WIFEXITED(child_status));
    CK_CHECK(WEXITSTATUS(child_status) == 0);
    CK_CHECK(payload_count == kPayloadBytes);
    ::close(status_pipe[0]);
    ::close(acknowledge_pipe[1]);
    ::close(master_fd);
}

CK_TEST(destruction_restores_the_screen_and_modes) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        { PosixTerminal term(clock, slave_fd, slave_fd); }  // constructed and destructed
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    const std::string output = read_available(child.master_fd, 300);
    CK_CHECK(output.find("\x1B[?1049l") != std::string::npos);  // leave alt screen
    CK_CHECK(output.find("\x1B[?2004l") != std::string::npos);  // bracketed paste off
    CK_CHECK(output.find("\x1B[?2031l") != std::string::npos);  // color-scheme notifications off
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(exception_unwinding_restores_terminal_state_before_the_catch_path) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    int status_pipe[2] = {-1, -1};
    int acknowledge_pipe[2] = {-1, -1};
    CK_CHECK(::pipe(status_pipe) == 0);
    CK_CHECK(::pipe(acknowledge_pipe) == 0);

    const pid_t child = ::fork();
    CK_CHECK(child >= 0);
    if (child == 0) {
        ::close(master_fd);
        ::close(status_pipe[0]);
        ::close(acknowledge_pipe[1]);
        ::setsid();
        PosixClock clock;
        try {
            PosixTerminal term(clock, slave_fd, slave_fd);
            throw 7;
        } catch (...) {
            constexpr std::string_view kCaught = "EXCEPTION-RESTORED";
            if (::write(slave_fd, kCaught.data(), kCaught.size()) != static_cast<ssize_t>(kCaught.size()))
                ::_exit(1);
            const char ready = 'R';
            if (::write(status_pipe[1], &ready, 1) != 1) ::_exit(1);
            // Keep the slave open until the parent has consumed the full
            // ordered sequence. A PTY is permitted to discard queued output
            // when its slave closes, so a post-exit read cannot prove whether
            // the terminal restore actually happened before the catch path.
            char acknowledged = 0;
            if (::read(acknowledge_pipe[0], &acknowledged, 1) != 1 || acknowledged != 'A') ::_exit(1);
        }
        ::close(acknowledge_pipe[0]);
        ::close(status_pipe[1]);
        ::_exit(0);
    }

    ::close(slave_fd);
    ::close(status_pipe[1]);
    ::close(acknowledge_pipe[0]);
    CK_CHECK(::fcntl(status_pipe[0], F_SETFL, O_NONBLOCK) == 0);
    std::string output = read_output_until_status(master_fd, status_pipe[0], 'R', 1'000);
    const char acknowledged = 'A';
    CK_CHECK(::write(acknowledge_pipe[1], &acknowledged, 1) == 1);
    const int status = wait_child(child);
    output += read_available(master_fd, 50);

    const std::size_t restored_at = output.find("\x1B[?1049l");
    const std::size_t synchronized_restore_at = output.find("\x1B[?2026l");
    const std::size_t scheme_restore_at = output.find("\x1B[?2031l");
    const std::size_t caught_at = output.find("EXCEPTION-RESTORED");
    CK_CHECK(restored_at != std::string::npos);
    CK_CHECK(synchronized_restore_at != std::string::npos);
    CK_CHECK(scheme_restore_at != std::string::npos);
    CK_CHECK(caught_at != std::string::npos);
    CK_CHECK(restored_at < caught_at);
    CK_CHECK(synchronized_restore_at < caught_at);
    CK_CHECK(scheme_restore_at < caught_at);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(status_pipe[0]);
    ::close(acknowledge_pipe[1]);
    ::close(master_fd);
}

CK_TEST(application_diagnostics_flush_only_after_the_terminal_session_is_restored) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        {
            ui::Application app(term, clock);
            app.diagnostics().log(LogLevel::Warning, "probe timed out");
        }
        ::usleep(100'000);  // keep the slave open while the parent observes the ordered flush
    }, true);
    const std::string output = read_until_contains(child.master_fd, "\x1B[?1049l", 1000);
    const std::string diagnostic = read_until_contains(child.stderr_fd, "warning: probe timed out", 1000);
    const int status = wait_child(child.pid);
    const std::size_t restored_at = output.find("\x1B[?1049l");
    const std::size_t synchronized_restore_at = output.find("\x1B[?2026l");
    const std::size_t scheme_restore_at = output.find("\x1B[?2031l");
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    CK_CHECK(restored_at != std::string::npos);
    CK_CHECK(synchronized_restore_at != std::string::npos);
    CK_CHECK(scheme_restore_at != std::string::npos);
    CK_CHECK(count_occurrences(output, "\x1B[?1049l") == 1);
    CK_CHECK(diagnostic.find("warning: probe timed out") != std::string::npos);
    ::close(child.master_fd);
    ::close(child.stderr_fd);
}

CK_TEST(assertion_diagnostic_follows_the_terminal_restore_ledger) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        CKV_ASSERT(false);
    }, true);
    const std::string output = read_until_contains(child.master_fd, "\x1B[?1049l", 1000);
    const std::string diagnostic =
        read_until_contains(child.stderr_fd, "ckVision contract violation: false (", 1000);
    const int status = wait_child(child.pid);
    const std::size_t restored_at = output.find("\x1B[?1049l");
    const std::size_t synchronized_restore_at = output.find("\x1B[?2026l");
    const std::size_t scheme_restore_at = output.find("\x1B[?2031l");
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
    CK_CHECK(restored_at != std::string::npos);
    CK_CHECK(synchronized_restore_at != std::string::npos);
    CK_CHECK(scheme_restore_at != std::string::npos);
    CK_CHECK(diagnostic.find("tests/test_posix_terminal.cpp:") != std::string::npos);
    ::close(child.master_fd);
    ::close(child.stderr_fd);
}

CK_TEST(callback_failure_restores_every_terminal_state_before_emitting_its_diagnostic) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        ui::Application app(term, clock);
        auto* view = static_cast<ThrowingKeyProbe*>(app.root().add_child(std::make_unique<ThrowingKeyProbe>()));
        app.set_focus(view);
        // The initial frame is known work and is intentionally presented
        // without waiting. Enter the long idle poll only after that work has
        // been delivered, so the parent can deterministically inject the key
        // into an actually blocked application.
        app.step(clock.now_nanos());
        app.step(clock.now_nanos() + 2'000'000'000LL);
    }, true);
    ::usleep(100'000);  // let the child enter its terminal poll before input arrives
    drain_pty_master(child.master_fd);  // exclude the entry/probe traffic from the crash ledger assertion
    CK_CHECK(::write(child.master_fd, "X", 1) == 1);
    const std::string output = read_until_contains(child.master_fd, "\x1B[?1049l", 1000);
    const std::string diagnostic =
        read_until_contains(child.stderr_fd, "ckVision contract violation: application callback threw", 1000);
    const int status = wait_child(child.pid);
    const std::size_t restored_at = output.find("\x1B[?1049l");
    const std::size_t synchronized_restore_at = output.find("\x1B[?2026l");
    const std::size_t scheme_restore_at = output.find("\x1B[?2031l");
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
    CK_CHECK(restored_at != std::string::npos);
    CK_CHECK(synchronized_restore_at != std::string::npos);
    CK_CHECK(scheme_restore_at != std::string::npos);
    CK_CHECK(diagnostic.find("ckVision contract violation: application callback threw") != std::string::npos);
    ::close(child.master_fd);
    ::close(child.stderr_fd);
}

CK_TEST(write_sends_bytes_through_to_the_pty) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        term.write("HELLO-MARKER");
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    const std::string output = read_available(child.master_fd, 300);
    CK_CHECK(output.find("HELLO-MARKER") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(poll_decodes_bytes_written_from_the_other_side_of_the_pty) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        const std::int64_t deadline = clock.now_nanos() + 2'000'000'000LL;
        std::vector<TerminalEvent> events;
        while (events.empty()) {
            auto batch = term.poll(deadline);
            for (auto& e : batch) events.push_back(std::move(e));
            if (clock.now_nanos() > deadline) break;
        }
        if (!events.empty() && std::holds_alternative<KeyEvent>(events[0]) &&
            std::get<KeyEvent>(events[0]).chord.text == "Q") {
            term.write("GOT-Q");
        }
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    ::usleep(100'000);  // let the child finish constructing and enter poll()
    if (::write(child.master_fd, "Q", 1) != 1) CK_CHECK(false);
    const std::string output = read_available(child.master_fd, 2000);
    CK_CHECK(output.find("GOT-Q") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(fatal_signal_still_restores_the_screen_before_the_process_dies) {
    int master_fd = -1;
    int slave_fd = -1;
    struct termios original{};
    struct termios restored{};
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    CK_CHECK(::tcgetattr(slave_fd, &original) == 0);
    const pid_t child = ::fork();
    CK_CHECK(child >= 0);
    if (child == 0) {
        ::close(master_fd);
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        ::usleep(50'000);
        ::abort();  // SIGABRT: the panic-safe restore path must still fire
    }
    const int status = wait_child(child);
    const std::string output = read_available(master_fd, 500);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
    // The restore sequence must have been written before the process
    // died, even though it never reached its own destructor normally.
    CK_CHECK(output.find("\x1B[?1049l") != std::string::npos);
    CK_CHECK(output.find("\x1B[?2026l") != std::string::npos);
    CK_CHECK(output.find("\x1B[?2031l") != std::string::npos);
    CK_CHECK(::tcgetattr(slave_fd, &restored) == 0);
    CK_CHECK((restored.c_lflag & (ICANON | ECHO)) == (original.c_lflag & (ICANON | ECHO)));
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(fatal_signal_restores_every_live_terminal_session_before_the_process_dies) {
    int first_master_fd = -1;
    int first_slave_fd = -1;
    int second_master_fd = -1;
    int second_slave_fd = -1;
    struct termios first_original {};
    struct termios second_original {};
    struct termios first_restored {};
    struct termios second_restored {};
    CK_CHECK(::openpty(&first_master_fd, &first_slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::openpty(&second_master_fd, &second_slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(first_master_fd, F_SETFL, O_NONBLOCK) == 0);
    CK_CHECK(::fcntl(second_master_fd, F_SETFL, O_NONBLOCK) == 0);
    CK_CHECK(::tcgetattr(first_slave_fd, &first_original) == 0);
    CK_CHECK(::tcgetattr(second_slave_fd, &second_original) == 0);

    const pid_t child = ::fork();
    CK_CHECK(child >= 0);
    if (child == 0) {
        ::close(first_master_fd);
        ::close(second_master_fd);
        PosixClock first_clock;
        PosixClock second_clock;
        PosixTerminal first(first_clock, first_slave_fd, first_slave_fd);
        PosixTerminal second(second_clock, second_slave_fd, second_slave_fd);
        ::raise(SIGABRT);  // D-024 must restore both registry entries.
        ::_exit(1);        // unreachable: the fatal handler re-raises SIGABRT.
    }

    const int status = wait_child(child);
    const std::string first_output = read_available(first_master_fd, 500);
    const std::string second_output = read_available(second_master_fd, 500);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
    CK_CHECK(first_output.find("\x1B[?1049l") != std::string::npos);
    CK_CHECK(second_output.find("\x1B[?1049l") != std::string::npos);
    CK_CHECK(first_output.find("\x1B[?2026l") != std::string::npos);
    CK_CHECK(second_output.find("\x1B[?2026l") != std::string::npos);
    CK_CHECK(first_output.find("\x1B[?2031l") != std::string::npos);
    CK_CHECK(second_output.find("\x1B[?2031l") != std::string::npos);
    CK_CHECK(::tcgetattr(first_slave_fd, &first_restored) == 0);
    CK_CHECK(::tcgetattr(second_slave_fd, &second_restored) == 0);
    CK_CHECK((first_restored.c_lflag & (ICANON | ECHO)) == (first_original.c_lflag & (ICANON | ECHO)));
    CK_CHECK((second_restored.c_lflag & (ICANON | ECHO)) == (second_original.c_lflag & (ICANON | ECHO)));
    ::close(first_master_fd);
    ::close(first_slave_fd);
    ::close(second_master_fd);
    ::close(second_slave_fd);
}

CK_TEST(sigcont_reenters_every_live_terminal_session) {
    const pid_t child = ::fork();
    CK_CHECK(child >= 0);
    if (child == 0) {
        int first_master_fd = -1;
        int first_slave_fd = -1;
        int second_master_fd = -1;
        int second_slave_fd = -1;
        if (::openpty(&first_master_fd, &first_slave_fd, nullptr, nullptr, nullptr) != 0 ||
            ::openpty(&second_master_fd, &second_slave_fd, nullptr, nullptr, nullptr) != 0)
            ::_exit(1);
        if (::fcntl(first_master_fd, F_SETFL, O_NONBLOCK) != 0 ||
            ::fcntl(second_master_fd, F_SETFL, O_NONBLOCK) != 0)
            ::_exit(1);
        struct termios first_original {};
        struct termios second_original {};
        if (::tcgetattr(first_slave_fd, &first_original) != 0 ||
            ::tcgetattr(second_slave_fd, &second_original) != 0)
            ::_exit(1);

        bool passed = false;
        {
            // The one-session job-control hierarchy below proves that the
            // process really stops and resumes. This contract isolates the
            // D-024 registry's independent promise: one continuation signal
            // re-enters every already-registered terminal session.
            ManualClock first_clock(1'000);
            ManualClock second_clock(1'000);
            PosixTerminal first(first_clock, first_slave_fd, first_slave_fd, baseline_capabilities(),
                                /*enable_capability_probes=*/false);
            PosixTerminal second(second_clock, second_slave_fd, second_slave_fd, baseline_capabilities(),
                                 /*enable_capability_probes=*/false);
            drain_pty_master(first_master_fd);
            drain_pty_master(second_master_fd);

            // Simulate the state immediately after SIGTSTP's restoration.
            // SIGCONT must restore raw input and re-enter both sessions from
            // their independently recorded registry ledgers.
            if (::tcsetattr(first_slave_fd, TCSANOW, &first_original) != 0 ||
                ::tcsetattr(second_slave_fd, TCSANOW, &second_original) != 0)
                ::_exit(1);
            if (::raise(SIGCONT) != 0) ::_exit(1);

            const auto first_events = first.poll(first_clock.now_nanos());
            const auto second_events = second.poll(second_clock.now_nanos());
            struct termios first_current {};
            struct termios second_current {};
            const std::string first_output = read_available(first_master_fd, 50);
            const std::string second_output = read_available(second_master_fd, 50);
            passed = first_events.size() == 1 && second_events.size() == 1 &&
                     std::holds_alternative<CapabilityChangedEvent>(first_events.front()) &&
                     std::holds_alternative<CapabilityChangedEvent>(second_events.front()) &&
                     ::tcgetattr(first_slave_fd, &first_current) == 0 &&
                     ::tcgetattr(second_slave_fd, &second_current) == 0 &&
                     (first_current.c_lflag & (ICANON | ECHO)) == 0 &&
                     (second_current.c_lflag & (ICANON | ECHO)) == 0 &&
                     first_output.find("\x1B[?1049h") != std::string::npos &&
                     second_output.find("\x1B[?1049h") != std::string::npos &&
                     first_output.find("\x1B[?1003h\x1B[?1006h") != std::string::npos &&
                     second_output.find("\x1B[?1003h\x1B[?1006h") != std::string::npos;
        }
        ::close(first_master_fd);
        ::close(first_slave_fd);
        ::close(second_master_fd);
        ::close(second_slave_fd);
        ::_exit(passed ? 0 : 1);
    }
    const int status = wait_child(child);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
}

CK_TEST(sigwinch_delivers_a_coalesced_resize_event) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        const std::int64_t deadline = clock.now_nanos() + 2'000'000'000LL;
        bool got_resize = false;
        while (!got_resize && clock.now_nanos() < deadline) {
            auto batch = term.poll(deadline);
            for (auto& e : batch)
                if (std::holds_alternative<ResizeEvent>(e)) got_resize = true;
        }
        if (got_resize) term.write("GOT-RESIZE");
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    ::usleep(100'000);
    struct winsize ws{};
    ws.ws_col = 100;
    ws.ws_row = 40;
    ::ioctl(child.master_fd, TIOCSWINSZ, &ws);
    ::kill(child.pid, SIGWINCH);
    const std::string output = read_available(child.master_fd, 2000);
    CK_CHECK(output.find("GOT-RESIZE") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(posix_terminal_exports_input_and_wake_handles_for_an_external_wait_loop) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    {
        ManualClock clock(1'000);
        PosixTerminal term(clock, slave_fd, slave_fd, baseline_capabilities(),
                           /*enable_capability_probes=*/false);
        ui::Application app(term, clock);
        const auto handles = app.wait_handles();
        CK_CHECK(handles.size() == 2);
        CK_CHECK((handles[0] == WaitHandle{WaitHandleKind::PosixFileDescriptor,
                                           static_cast<std::uintptr_t>(slave_fd)}));
        CK_CHECK(handles[1].kind == WaitHandleKind::PosixFileDescriptor);
        CK_CHECK(handles[1].value != handles[0].value);

        std::array<struct pollfd, 2> host_wait = {{
            {static_cast<int>(handles[0].value), POLLIN, 0},
            {static_cast<int>(handles[1].value), POLLIN, 0},
        }};
        constexpr std::string_view kInput = "Q";
        CK_CHECK(::write(master_fd, kInput.data(), kInput.size()) == static_cast<ssize_t>(kInput.size()));
        CK_CHECK(::poll(host_wait.data(), host_wait.size(), 1'000) > 0);
        CK_CHECK((host_wait[0].revents & POLLIN) != 0);
        CK_CHECK(app.step(clock.now_nanos()));

        host_wait[0].revents = 0;
        host_wait[1].revents = 0;
        app.wake();
        CK_CHECK(::poll(host_wait.data(), host_wait.size(), 1'000) > 0);
        CK_CHECK((host_wait[1].revents & POLLIN) != 0);
        app.step(clock.now_nanos());

        term.restore();
        CK_CHECK(term.wait_handles().empty());
    }
    ::close(master_fd);
    ::close(slave_fd);
}

CK_TEST(wake_interrupts_an_in_flight_posix_poll_without_creating_input) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal term(clock, slave_fd, slave_fd);
        std::thread waker([&term] {
            ::usleep(50'000);
            term.wake();
        });
        const auto started = std::chrono::steady_clock::now();
        const auto events = term.poll(clock.now_nanos() + 2'000'000'000LL);
        const auto elapsed = std::chrono::steady_clock::now() - started;
        waker.join();
        if (events.empty() && elapsed < std::chrono::milliseconds(750)) term.write("GOT-WAKE");
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    const std::string output = read_available(child.master_fd, 1500);
    CK_CHECK(output.find("GOT-WAKE") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

CK_TEST(request_quit_interrupts_an_in_flight_posix_application_run) {
    PtyChild child = spawn_pty_child_until_output_acknowledged([](int slave_fd, int acknowledge_fd) {
        PosixClock clock;
        PosixTerminal terminal(clock, slave_fd, slave_fd);
        ui::Application app(terminal, clock);
        std::thread requester([&app] {
            ::usleep(50'000);
            app.request_quit();
        });
        const auto started = std::chrono::steady_clock::now();
        app.run();
        const auto elapsed = std::chrono::steady_clock::now() - started;
        requester.join();
        if (app.quit_requested() && elapsed < std::chrono::milliseconds(750))
            terminal.write("GOT-QUIT-WAKE");
        char acknowledgement = 0;
        if (::read(acknowledge_fd, &acknowledgement, 1) != 1 || acknowledgement != 'A') ::_exit(1);
        ::close(acknowledge_fd);
    });
    const std::string output = read_until_contains(child.master_fd, "GOT-QUIT-WAKE", 1'500);
    CK_CHECK(output.find("GOT-QUIT-WAKE") != std::string::npos);
    acknowledge_output(child);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(child.master_fd);
}

// A session that asked the terminal a question has to stay for the answer.
// Restoring a terminal hands its input queue on rather than emptying it, so
// a reply that arrives after the session stopped reading is delivered to
// whoever reads next — in an ordinary session the shell, which shows the
// tail of `CSI 0 n` as a typed `n`.
CK_TEST(a_session_reads_the_last_frames_answer_before_it_hands_the_terminal_back) {
    int master_fd = -1;
    int slave_fd = -1;
    int status_pipe[2] = {-1, -1};
    int release_pipe[2] = {-1, -1};
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    CK_CHECK(::pipe(status_pipe) == 0);
    CK_CHECK(::pipe(release_pipe) == 0);
    // The session restores whatever mode it found, and this test measures the
    // queue that mode leaves behind: a canonical-mode tty reports only
    // completed lines as available, which would make the measurement say
    // "empty" about a queue holding an unterminated reply.
    struct termios raw {};
    CK_CHECK(::tcgetattr(slave_fd, &raw) == 0);
    ::cfmakeraw(&raw);
    CK_CHECK(::tcsetattr(slave_fd, TCSANOW, &raw) == 0);

    const pid_t child = ::fork();
    CK_CHECK(child >= 0);
    if (child == 0) {
        ::close(master_fd);
        ::close(status_pipe[0]);
        ::close(release_pipe[1]);
        ::setsid();

        PosixClock clock;
        PosixTerminal terminal(clock, slave_fd, slave_fd, TerminalProfile::ModernVt);
        {
            ui::Application app(terminal, clock);
            app.set_frame_completion_tracking(true);
            app.step(clock.now_nanos());
            const auto answered_by = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (app.frames_awaiting_terminal() > 0 &&
                   std::chrono::steady_clock::now() < answered_by)
                app.step(clock.now_nanos() + 100'000'000);
            // The premise: this host answers. A written-off frame instead of a
            // reply means the parent never played its part, and the rest of
            // this test would be measuring nothing.
            if (app.last_terminal_round_trip_nanos() < 0) ::_exit(1);

            const char answered = 'A';
            if (::write(status_pipe[1], &answered, 1) != 1) ::_exit(2);
            char release = 0;
            if (::read(release_pipe[0], &release, 1) != 1 || release != 'G') ::_exit(3);

            // The last frame of the session, and the question that rides out
            // with it — exactly where a quit leaves things.
            app.invalidate_all();
            app.step(clock.now_nanos());
            if (app.frames_awaiting_terminal() == 0) ::_exit(4);
            const char asked = 'Q';
            if (::write(status_pipe[1], &asked, 1) != 1) ::_exit(5);
        }
        // The session is over and the terminal is back in the mode it was
        // found in. Whatever the answer met on arrival, the next reader of
        // this terminal must find nothing left for it.
        int pending = -1;
        if (::ioctl(slave_fd, FIONREAD, &pending) != 0) ::_exit(6);
        const char verdict = pending == 0 ? 'D' : 'L';
        if (::write(status_pipe[1], &verdict, 1) != 1) ::_exit(9);
        ::_exit(pending == 0 ? 0 : 7);
    }
    ::close(slave_fd);
    ::close(status_pipe[1]);
    ::close(release_pipe[0]);
    CK_CHECK(::fcntl(status_pipe[0], F_SETFL, O_NONBLOCK) == 0);

    // Play a terminal that answers, until the child has measured one round
    // trip against it.
    std::string seen;
    std::size_t scanned = 0;
    const auto answer_new_questions = [&] {
        for (std::size_t at = seen.find("\x1B[5n", scanned); at != std::string::npos;
             at = seen.find("\x1B[5n", scanned)) {
            scanned = at + 4;
            CK_CHECK(::write(master_fd, "\x1B[0n", 4) == 4);
        }
    };
    char status = 0;
    const auto answering_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < answering_deadline) {
        char buf[4096];
        for (ssize_t n = ::read(master_fd, buf, sizeof(buf)); n > 0;
             n = ::read(master_fd, buf, sizeof(buf)))
            seen.append(buf, static_cast<std::size_t>(n));
        answer_new_questions();
        if (::read(status_pipe[0], &status, 1) == 1) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CK_CHECK(status == 'A');

    // From here this terminal answers only when told to, so the child's last
    // question is outstanding for as long as this test wants it to be.
    const char release = 'G';
    CK_CHECK(::write(release_pipe[1], &release, 1) == 1);
    status = 0;
    const auto asked_by = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < asked_by) {
        if (::read(status_pipe[0], &status, 1) == 1) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CK_CHECK(status == 'Q');

    // The child is now inside the handover. It must still be there: a session
    // that walked away from its own question would already have reported.
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    char premature = 0;
    CK_CHECK(::read(status_pipe[0], &premature, 1) != 1);

    CK_CHECK(::write(master_fd, "\x1B[0n", 4) == 4);
    char verdict = 0;
    const auto done_by = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < done_by) {
        if (::read(status_pipe[0], &verdict, 1) == 1) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CK_CHECK(verdict == 'D');  // 'L': the reply was left for the next reader

    const int child_status = wait_child(child);
    CK_CHECK(WIFEXITED(child_status));
    CK_CHECK(WEXITSTATUS(child_status) == 0);
    ::close(release_pipe[1]);
    ::close(status_pipe[0]);
    ::close(master_fd);
}

CK_TEST(resume_applies_a_resize_that_occurred_while_the_session_was_stopped_before_presenting) {
    int master_fd = -1;
    int slave_fd = -1;
    int status_pipe[2] = {-1, -1};
    int release_pipe[2] = {-1, -1};
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    CK_CHECK(::pipe(status_pipe) == 0);
    CK_CHECK(::pipe(release_pipe) == 0);
    struct winsize initial_size {};
    initial_size.ws_col = 80;
    initial_size.ws_row = 24;
    CK_CHECK(::ioctl(master_fd, TIOCSWINSZ, &initial_size) == 0);
    const pid_t child = ::fork();
    CK_CHECK(child >= 0);
    if (child == 0) {
        ::close(master_fd);
        ::close(status_pipe[0]);
        ::close(release_pipe[1]);

        // Keep SIGCONT pending while the parent changes the PTY's geometry.
        // The child then unblocks it immediately before Application::step(),
        // deterministically exercising the normal resumed-session path
        // without perturbing this test runner's own signal state.
        sigset_t continue_signal {};
        sigemptyset(&continue_signal);
        sigaddset(&continue_signal, SIGCONT);
        if (::sigprocmask(SIG_BLOCK, &continue_signal, nullptr) != 0) ::_exit(1);

        ManualClock clock;
        PosixTerminal terminal(clock, slave_fd, slave_fd);
        ui::Application app(terminal, clock);
        app.step(clock.now_nanos());
        const char ready = 'R';
        if (::write(status_pipe[1], &ready, 1) != 1) ::_exit(1);
        char release = 0;
        if (::read(release_pipe[0], &release, 1) != 1 || release != 'G') ::_exit(1);
        if (::sigprocmask(SIG_UNBLOCK, &continue_signal, nullptr) != 0) ::_exit(1);
        app.step(clock.now_nanos());
        const bool correct_size = app.root().bounds() == Rect{0, 0, 100, 40} &&
                                  app.current_frame().size() == Size{100, 40};
        const char done = correct_size ? 'P' : 'F';
        if (::write(status_pipe[1], &done, 1) != 1) ::_exit(9);
        ::_exit(correct_size ? 0 : 1);
    }
    ::close(slave_fd);
    ::close(status_pipe[1]);
    ::close(release_pipe[0]);
    CK_CHECK(::fcntl(status_pipe[0], F_SETFL, O_NONBLOCK) == 0);
    (void)read_output_until_status(master_fd, status_pipe[0], 'R', 1000);
    struct winsize resumed_size {};
    resumed_size.ws_col = 100;
    resumed_size.ws_row = 40;
    CK_CHECK(::ioctl(master_fd, TIOCSWINSZ, &resumed_size) == 0);
    CK_CHECK(::kill(child, SIGCONT) == 0);
    const char release = 'G';
    CK_CHECK(::write(release_pipe[1], &release, 1) == 1);
    (void)read_output_until_status(master_fd, status_pipe[0], 'P', 1000);
    const int child_status = wait_child(child);
    CK_CHECK(WIFEXITED(child_status));
    CK_CHECK(WEXITSTATUS(child_status) == 0);
    ::close(release_pipe[1]);
    ::close(status_pipe[0]);
    ::close(master_fd);
}

CK_TEST(resume_invalidates_runtime_pixel_metrics_until_fresh_probe_evidence) {
    int master_fd = -1;
    int slave_fd = -1;
    int status_pipe[2] = {-1, -1};
    int release_pipe[2] = {-1, -1};
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    CK_CHECK(::pipe(status_pipe) == 0);
    CK_CHECK(::pipe(release_pipe) == 0);
    const pid_t child = ::fork();
    CK_CHECK(child >= 0);
    if (child == 0) {
        ::close(master_fd);
        ::close(status_pipe[0]);
        ::close(release_pipe[1]);
        sigset_t continue_signal {};
        sigemptyset(&continue_signal);
        sigaddset(&continue_signal, SIGCONT);
        if (::sigprocmask(SIG_BLOCK, &continue_signal, nullptr) != 0) ::_exit(1);

        ManualClock clock;
        PosixTerminal terminal(clock, slave_fd, slave_fd);
        ui::Application app(terminal, clock);
        app.step(clock.now_nanos());
        const char ready = 'R';
        if (::write(status_pipe[1], &ready, 1) != 1) ::_exit(1);

        char release = 0;
        if (::read(release_pipe[0], &release, 1) != 1 || release != 'M') ::_exit(1);
        app.step(clock.now_nanos());
        if (terminal.capabilities().cell_pixels != Size{9, 18} || !terminal.capabilities().pixel_mouse)
            ::_exit(1);
        const char metrics_ready = 'M';
        if (::write(status_pipe[1], &metrics_ready, 1) != 1) ::_exit(1);

        if (::read(release_pipe[0], &release, 1) != 1 || release != 'G') ::_exit(1);
        if (::sigprocmask(SIG_UNBLOCK, &continue_signal, nullptr) != 0) ::_exit(1);
        app.step(clock.now_nanos());
        const bool invalidated = terminal.capabilities().cell_pixels == Size{} && !terminal.capabilities().pixel_mouse;
        const char done = invalidated ? 'P' : 'F';
        if (::write(status_pipe[1], &done, 1) != 1) ::_exit(9);
        ::_exit(invalidated ? 0 : 1);
    }

    ::close(slave_fd);
    ::close(status_pipe[1]);
    ::close(release_pipe[0]);
    CK_CHECK(::fcntl(status_pipe[0], F_SETFL, O_NONBLOCK) == 0);
    (void)read_output_until_status(master_fd, status_pipe[0], 'R', 1'000);
    constexpr std::string_view kMetricAndMode = "\x1B[?1016;1$y\x1B[6;18;9t";
    CK_CHECK(::write(master_fd, kMetricAndMode.data(), kMetricAndMode.size()) ==
             static_cast<ssize_t>(kMetricAndMode.size()));
    const char process_metrics = 'M';
    CK_CHECK(::write(release_pipe[1], &process_metrics, 1) == 1);
    (void)read_output_until_status(master_fd, status_pipe[0], 'M', 1'000);
    CK_CHECK(::kill(child, SIGCONT) == 0);
    const char resume = 'G';
    CK_CHECK(::write(release_pipe[1], &resume, 1) == 1);
    (void)read_output_until_status(master_fd, status_pipe[0], 'P', 1'000);
    const int child_status = wait_child(child);
    CK_CHECK(WIFEXITED(child_status));
    CK_CHECK(WEXITSTATUS(child_status) == 0);
    ::close(release_pipe[1]);
    ::close(status_pipe[0]);
    ::close(master_fd);
}

CK_TEST(suspend_restores_and_resume_reenters_a_non_orphaned_terminal_session) {
    int master_fd = -1;
    int slave_fd = -1;
    int status_pipe[2] = {-1, -1};
    int release_pipe[2] = {-1, -1};
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);
    struct winsize window_size {};
    window_size.ws_col = 80;
    window_size.ws_row = 24;
    CK_CHECK(::ioctl(slave_fd, TIOCSWINSZ, &window_size) == 0);
    CK_CHECK(::fcntl(master_fd, F_SETFL, O_NONBLOCK) == 0);
    CK_CHECK(::pipe(status_pipe) == 0);
    CK_CHECK(::pipe(release_pipe) == 0);
    const pid_t supervisor = ::fork();
    CK_CHECK(supervisor >= 0);
    if (supervisor == 0) {
        ::close(master_fd);
        ::close(status_pipe[0]);
        if (::setsid() < 0 || ::ioctl(slave_fd, TIOCSCTTY, 0) != 0) ::_exit(1);
        const pid_t worker = ::fork();
        if (worker < 0) ::_exit(1);
        if (worker == 0) {
            ::close(release_pipe[1]);
            char release = 0;
            if (::read(release_pipe[0], &release, 1) != 1 || release != 'G') ::_exit(1);
            int result = 1;
            {
                PosixClock clock;
                PosixTerminal terminal(clock, slave_fd, slave_fd);
                ui::Application app(terminal, clock);
                auto* probe = app.root().add(std::make_unique<ResumeProbe>());
                probe->set_bounds(Rect{0, 0, 20, 1});
                probe->set_focus_policy(ui::FocusPolicy::TabStop);
                app.set_focus(probe);
                app.step(clock.now_nanos());
                const char ready = 'R';
                if (::write(status_pipe[1], &ready, 1) != 1) ::_exit(1);
                const auto deadline = clock.now_nanos() + 5'000'000'000LL;
                bool reported_repaint = false;
                while (!probe->quit && clock.now_nanos() < deadline) {
                    app.step(deadline);
                    if (!reported_repaint && probe->draw_count >= 2) {
                        const char repainted = 'P';
                        if (::write(status_pipe[1], &repainted, 1) != 1) ::_exit(1);
                        reported_repaint = true;
                    }
                }
                struct termios current {};
                const bool raw_mode = ::tcgetattr(slave_fd, &current) == 0 &&
                                      (current.c_lflag & (ICANON | ECHO)) == 0;
                result = probe->quit && raw_mode ? 0 : 1;
            }
            ::_exit(result);
        }
        ::close(release_pipe[0]);
        if (::setpgid(worker, worker) != 0) ::_exit(1);
        if (::tcsetpgrp(slave_fd, worker) != 0) ::_exit(1);
        if (::write(status_pipe[1], &worker, sizeof(worker)) != sizeof(worker)) ::_exit(1);
        const char go = 'G';
        if (::write(release_pipe[1], &go, 1) != 1) ::_exit(1);
        int worker_status = 0;
        if (::waitpid(worker, &worker_status, WUNTRACED) != worker || !WIFSTOPPED(worker_status)) ::_exit(1);
        const char stopped = 'S';
        if (::write(status_pipe[1], &stopped, 1) != 1) ::_exit(1);
        if (::waitpid(worker, &worker_status, 0) != worker || !WIFEXITED(worker_status)) ::_exit(1);
        ::_exit(WEXITSTATUS(worker_status));
    }
    ::close(slave_fd);
    ::close(status_pipe[1]);
    ::close(release_pipe[0]);
    ::close(release_pipe[1]);
    pid_t worker = -1;
    CK_CHECK(::read(status_pipe[0], &worker, sizeof(worker)) == sizeof(worker));
    CK_CHECK(::fcntl(status_pipe[0], F_SETFL, O_NONBLOCK) == 0);
    const std::string initial = read_output_until_status(master_fd, status_pipe[0], 'R', 1000);
    CK_CHECK(initial.find("\x1B[?1049h") != std::string::npos);
    CK_CHECK(initial.find("\x1B[?2026h\x1B[?2026$p\x1B[?2026l") != std::string::npos);
    CK_CHECK(initial.find("\x1B[?2031h") != std::string::npos);
    CK_CHECK(initial.find("RESUME-FRAME") != std::string::npos);
    const std::string initial_frame = last_presented_frame(initial);
    CK_CHECK(!initial_frame.empty());
    CK_CHECK(::kill(worker, SIGTSTP) == 0);
    const std::string suspended = read_output_until_status(master_fd, status_pipe[0], 'S', 1000);
    CK_CHECK(suspended.find("\x1B[?1049l") != std::string::npos);
    CK_CHECK(suspended.find("\x1B[?2026l") != std::string::npos);
    CK_CHECK(suspended.find("\x1B[?2031l") != std::string::npos);
    CK_CHECK(::kill(worker, SIGCONT) == 0);
    const std::string resumed = read_output_until_status(master_fd, status_pipe[0], 'P', 1000);
    CK_CHECK(resumed.find("\x1B[?1049h") != std::string::npos);
    CK_CHECK(resumed.find("\x1B[?2026h\x1B[?2026$p\x1B[?2026l") != std::string::npos);
    CK_CHECK(resumed.find("\x1B[?2031h") != std::string::npos);
    CK_CHECK(resumed.find("RESUME-FRAME") != std::string::npos);
    const std::string resumed_frame = last_presented_frame(resumed);
    CK_CHECK(!resumed_frame.empty());
    CK_CHECK(resumed_frame == initial_frame);
    CK_CHECK(::write(master_fd, "q", 1) == 1);
    (void)read_available(master_fd, 200);
    const int status = wait_child(supervisor);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
    ::close(status_pipe[0]);
    ::close(master_fd);
}

CK_TEST(resize_observation_is_scoped_to_the_terminal_whose_geometry_changed) {
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        int first_master = -1;
        int first_slave = -1;
        int second_master = -1;
        int second_slave = -1;
        if (::openpty(&first_master, &first_slave, nullptr, nullptr, nullptr) != 0 ||
            ::openpty(&second_master, &second_slave, nullptr, nullptr, nullptr) != 0)
            ::_exit(1);
        bool first_resized = false;
        bool second_resized = false;
        {
            PosixClock first_clock;
            PosixClock second_clock;
            PosixTerminal first(first_clock, first_slave, first_slave);
            PosixTerminal second(second_clock, second_slave, second_slave);
            struct winsize ws{};
            ws.ws_col = 100;
            ws.ws_row = 40;
            if (::ioctl(first_slave, TIOCSWINSZ, &ws) != 0) ::_exit(1);
            for (const TerminalEvent& event : first.poll(first_clock.now_nanos()))
                first_resized = first_resized || std::holds_alternative<ResizeEvent>(event);
            for (const TerminalEvent& event : second.poll(second_clock.now_nanos()))
                second_resized = second_resized || std::holds_alternative<ResizeEvent>(event);
        }
        ::close(first_master);
        ::close(first_slave);
        ::close(second_master);
        ::close(second_slave);
        ::_exit(first_resized && !second_resized ? 0 : 1);
    }
    const int status = wait_child(pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
}

CK_TEST(two_applications_on_separate_ptys_keep_wake_and_quit_isolated) {
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        int first_master = -1;
        int first_slave = -1;
        int second_master = -1;
        int second_slave = -1;
        if (::openpty(&first_master, &first_slave, nullptr, nullptr, nullptr) != 0 ||
            ::openpty(&second_master, &second_slave, nullptr, nullptr, nullptr) != 0)
            ::_exit(1);
        if (::fcntl(first_master, F_SETFL, O_NONBLOCK) != 0 ||
            ::fcntl(second_master, F_SETFL, O_NONBLOCK) != 0)
            ::_exit(1);
        // The production backend deliberately requires a blocking output
        // terminal so session restoration cannot silently lose bytes. Keep
        // these artificial PTYs drained just as a real terminal host would,
        // including while an Application presents its first or resized frame.
        std::atomic<bool> drain_outputs = true;
        std::thread output_drainer([&] {
            while (drain_outputs.load(std::memory_order_acquire)) {
                drain_pty_master(first_master);
                drain_pty_master(second_master);
                ::usleep(1'000);
            }
            drain_pty_master(first_master);
            drain_pty_master(second_master);
        });
        {
            PosixClock first_clock;
            PosixClock second_clock;
            PosixTerminal first_terminal(first_clock, first_slave, first_slave);
            PosixTerminal second_terminal(second_clock, second_slave, second_slave);
            ui::Application first(first_terminal, first_clock);
            ui::Application second(second_terminal, second_clock);
            const Rect second_initial_bounds = second.root().bounds();
            auto* first_probe = first.root().add(std::make_unique<KeyProbe>());
            auto* second_probe = second.root().add(std::make_unique<KeyProbe>());
            first_probe->set_focus_policy(ui::FocusPolicy::TabStop);
            second_probe->set_focus_policy(ui::FocusPolicy::TabStop);
            first.set_focus(first_probe);
            second.set_focus(second_probe);

            // Known dirty work returns immediately by contract. Establish a
            // genuinely dormant state before testing that each terminal's
            // wake channel interrupts only its own blocked step.
            first.step(first_clock.now_nanos());
            second.step(second_clock.now_nanos());

            std::atomic<bool> second_step_returned = false;
            std::thread first_step([&] { first.step(first_clock.now_nanos() + 2'000'000'000LL); });
            std::thread second_step([&] {
                second.step(second_clock.now_nanos() + 2'000'000'000LL);
                second_step_returned.store(true, std::memory_order_release);
            });
            ::usleep(50'000);
            first.wake();
            first_step.join();
            ::usleep(50'000);
            if (second_step_returned.load(std::memory_order_acquire)) ::_exit(1);
            second.wake();
            second_step.join();

            if (first.quit_requested() || second.quit_requested()) ::_exit(1);
            if (::write(first_master, "x", 1) != 1) ::_exit(1);
            if (!first.step(first_clock.now_nanos() + 2'000'000'000LL)) ::_exit(1);
            if (first_probe->key_events != 1 || second_probe->key_events != 0) ::_exit(1);
            struct winsize ws{};
            ws.ws_col = 100;
            ws.ws_row = 40;
            if (::ioctl(first_slave, TIOCSWINSZ, &ws) != 0) ::_exit(1);
            if (!first.step(first_clock.now_nanos())) ::_exit(1);
            if (first.root().bounds().width != 100 || first.root().bounds().height != 40) ::_exit(1);
            second.step(second_clock.now_nanos());
            if (second.root().bounds() != second_initial_bounds) ::_exit(1);
            first.request_quit();
            if (!first.quit_requested() || second.quit_requested()) ::_exit(1);
        }
        drain_outputs.store(false, std::memory_order_release);
        output_drainer.join();
        ::close(first_master);
        ::close(second_master);
        ::close(first_slave);
        ::close(second_slave);
        ::_exit(0);
    }
    const int status = wait_child(pid);
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
}

CK_TEST(exhausting_the_session_registry_aborts_without_mutating_the_failed_terminal) {
    // Regression: registration used to happen AFTER raw mode/alt-screen
    // were already entered, so a failed registration (every registry
    // slot in use) left that failed construction's tty permanently
    // mutated with no destructor ever running to undo it. Verifies the
    // fix: the 9th (over the 8-slot registry) PTY receives ZERO bytes
    // from the failed construction, and the child dies by its own
    // CKV_ASSERT-triggered abort() rather than completing.
    //
    // All 9 PTYs are created here in the PARENT (before forking) so
    // their master sides survive the child's abort and can be
    // inspected afterward — a PTY opened inside the child would take
    // its master fd down with the aborting process.
    std::vector<int> master_fds;
    std::vector<int> slave_fds;
    for (int i = 0; i < 9; ++i) {
        int master = -1;
        int slave = -1;
        CK_CHECK(::openpty(&master, &slave, nullptr, nullptr, nullptr) == 0);
        master_fds.push_back(master);
        slave_fds.push_back(slave);
    }

    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        for (int m : master_fds) ::close(m);  // the child only needs the slave sides
        ::setsid();
        PosixClock clock;
        std::vector<std::unique_ptr<PosixTerminal>> held;  // keep the first 8 slots occupied
        for (int i = 0; i < 8; ++i)
            held.push_back(std::make_unique<PosixTerminal>(clock, slave_fds[static_cast<std::size_t>(i)],
                                                             slave_fds[static_cast<std::size_t>(i)]));
        // The 9th construction must abort here (CKV_ASSERT fires
        // before any write/tcsetattr on slave_fds[8]) — the process
        // dies and the line after this never runs.
        auto ninth = std::make_unique<PosixTerminal>(clock, slave_fds[8], slave_fds[8]);
        ::_exit(1);  // unreachable if the fix holds: construction should have aborted above
    }
    for (int s : slave_fds) ::close(s);  // the parent only needs the master sides

    int status = 0;
    ::waitpid(pid, &status, 0);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);

    const std::string ninth_output = read_available(master_fds[8], 200);
    CK_CHECK(ninth_output.empty());  // zero mutation: no alt-screen/mode bytes ever reached it

    for (int m : master_fds) ::close(m);
}

namespace ckv::term {
// Defined at namespace scope in posix_terminal.cpp for exactly this reach:
// the D-024 handler paths write restore sequences and diagnostics through
// it, where a short count or an EINTR silently truncating output was the
// failure CI's -Werror=unused-result pointed at.
void write_all_signal_safe(int fd, const char* bytes, std::size_t count) noexcept;
}  // namespace ckv::term

namespace {
extern "C" void interrupting_alarm(int) {}
}  // namespace

CK_TEST(the_signal_safe_write_loop_delivers_every_byte_through_interruptions) {
    int pipe_fds[2];
    CK_CHECK(::pipe(pipe_fds) == 0);

    // A reader slow enough that the writer must block on a full pipe, in a
    // child so the write side genuinely stalls rather than racing a thread.
    constexpr std::size_t kTotal = 8u * 1024u * 1024u;
    const pid_t reader = ::fork();
    CK_CHECK(reader >= 0);
    if (reader == 0) {
        ::close(pipe_fds[1]);
        char chunk[64 * 1024];
        std::size_t received = 0;
        for (;;) {
            const ssize_t got = ::read(pipe_fds[0], chunk, sizeof chunk);
            if (got <= 0) break;
            received += static_cast<std::size_t>(got);
            const struct timespec briefly{0, 2'000'000};  // keep the pipe full
            ::nanosleep(&briefly, nullptr);
        }
        ::_exit(received == kTotal ? 0 : 1);
    }
    ::close(pipe_fds[0]);

    // SIGALRM without SA_RESTART, firing faster than the reader drains:
    // every blocked write is interrupted, repeatedly — EINTR before any byte
    // moved, a short count after some did. Those are the two conditions the
    // loop exists for, and the two a bare ::write silently mishandles.
    struct sigaction interrupter{};
    interrupter.sa_handler = &interrupting_alarm;
    sigemptyset(&interrupter.sa_mask);
    interrupter.sa_flags = 0;  // deliberately not SA_RESTART
    struct sigaction saved_action{};
    CK_CHECK(::sigaction(SIGALRM, &interrupter, &saved_action) == 0);
    struct itimerval tick{};
    tick.it_interval.tv_usec = 1000;
    tick.it_value.tv_usec = 1000;
    CK_CHECK(::setitimer(ITIMER_REAL, &tick, nullptr) == 0);

    const std::vector<char> payload(kTotal, '\x5A');
    ckv::term::write_all_signal_safe(pipe_fds[1], payload.data(), payload.size());

    struct itimerval off{};
    (void)::setitimer(ITIMER_REAL, &off, nullptr);
    (void)::sigaction(SIGALRM, &saved_action, nullptr);
    ::close(pipe_fds[1]);  // EOF: the child can now total what arrived

    int status = 0;
    CK_CHECK(::waitpid(reader, &status, 0) == reader);
    // The child's verdict IS the assertion: every byte arrived despite the
    // interruptions. Degrading the helper to a single ::write delivers one
    // pipe buffer's worth and fails here.
    CK_CHECK(WIFEXITED(status));
    CK_CHECK(WEXITSTATUS(status) == 0);
}

#endif  // CKVISION_HAS_POSIX_TERMINAL
