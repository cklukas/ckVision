// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Integration tests for PosixTerminal, run against a forked PTY child
// — never against this test process's own controlling terminal, so a
// bug here can never disrupt the actual session running the tests.
#if defined(CKVISION_HAS_POSIX_TERMINAL)

#include "cvision/term/posix_terminal.hpp"

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "cktest.hpp"

using namespace ckv;
using namespace ckv::term;

namespace {

struct PtyChild {
    int master_fd = -1;
    pid_t pid = -1;
};

// Forks a child with `slave_fd` as its controlling stdin/stdout, then
// runs `child_fn` in the child and _exit()s. The parent gets the
// master fd to interact with. Never touches the real terminal.
PtyChild spawn_pty_child(void (*child_fn)(int slave_fd)) {
    int master_fd = -1;
    int slave_fd = -1;
    CK_CHECK(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);

    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        ::close(master_fd);
        ::setsid();
        child_fn(slave_fd);
        ::_exit(0);
    }
    ::close(slave_fd);
    return PtyChild{master_fd, pid};
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

int wait_child(pid_t pid) {
    int status = 0;
    ::waitpid(pid, &status, 0);
    return status;
}

}  // namespace

CK_TEST(construction_enters_alt_screen_and_raw_mode_sequences) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        PosixTerminal term(slave_fd, slave_fd);
        ::usleep(50'000);  // give the parent time to read before we tear down
    });
    const std::string output = read_available(child.master_fd, 300);
    CK_CHECK(output.find("\x1B[?1049h") != std::string::npos);  // alt screen
    CK_CHECK(output.find("\x1B[?2004h") != std::string::npos);  // bracketed paste on
    wait_child(child.pid);
    ::close(child.master_fd);
}

CK_TEST(destruction_restores_the_screen_and_modes) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        { PosixTerminal term(slave_fd, slave_fd); }  // constructed and destructed
    });
    const std::string output = read_available(child.master_fd, 300);
    CK_CHECK(output.find("\x1B[?1049l") != std::string::npos);  // leave alt screen
    CK_CHECK(output.find("\x1B[?2004l") != std::string::npos);  // bracketed paste off
    wait_child(child.pid);
    ::close(child.master_fd);
}

CK_TEST(write_sends_bytes_through_to_the_pty) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        PosixTerminal term(slave_fd, slave_fd);
        term.write("HELLO-MARKER");
        ::usleep(50'000);
    });
    const std::string output = read_available(child.master_fd, 300);
    CK_CHECK(output.find("HELLO-MARKER") != std::string::npos);
    wait_child(child.pid);
    ::close(child.master_fd);
}

CK_TEST(poll_decodes_bytes_written_from_the_other_side_of_the_pty) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        PosixTerminal term(slave_fd, slave_fd);
        const std::int64_t deadline = PosixTerminal::monotonic_now_nanos() + 2'000'000'000LL;
        std::vector<TerminalEvent> events;
        while (events.empty()) {
            auto batch = term.poll(deadline);
            for (auto& e : batch) events.push_back(std::move(e));
            if (PosixTerminal::monotonic_now_nanos() > deadline) break;
        }
        if (!events.empty() && std::holds_alternative<KeyEvent>(events[0]) &&
            std::get<KeyEvent>(events[0]).chord.text == "Q") {
            term.write("GOT-Q");
        }
    });
    ::usleep(100'000);  // let the child finish constructing and enter poll()
    ::write(child.master_fd, "Q", 1);
    const std::string output = read_available(child.master_fd, 2000);
    CK_CHECK(output.find("GOT-Q") != std::string::npos);
    wait_child(child.pid);
    ::close(child.master_fd);
}

CK_TEST(fatal_signal_still_restores_the_screen_before_the_process_dies) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        PosixTerminal term(slave_fd, slave_fd);
        ::usleep(50'000);
        ::abort();  // SIGABRT: the panic-safe restore path must still fire
    });
    const std::string output = read_available(child.master_fd, 500);
    const int status = wait_child(child.pid);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
    // The restore sequence must have been written before the process
    // died, even though it never reached its own destructor normally.
    CK_CHECK(output.find("\x1B[?1049l") != std::string::npos);
    ::close(child.master_fd);
}

CK_TEST(sigwinch_delivers_a_coalesced_resize_event) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        PosixTerminal term(slave_fd, slave_fd);
        const std::int64_t deadline = PosixTerminal::monotonic_now_nanos() + 2'000'000'000LL;
        bool got_resize = false;
        while (!got_resize && PosixTerminal::monotonic_now_nanos() < deadline) {
            auto batch = term.poll(deadline);
            for (auto& e : batch)
                if (std::holds_alternative<ResizeEvent>(e)) got_resize = true;
        }
        if (got_resize) term.write("GOT-RESIZE");
    });
    ::usleep(100'000);
    struct winsize ws{};
    ws.ws_col = 100;
    ws.ws_row = 40;
    ::ioctl(child.master_fd, TIOCSWINSZ, &ws);
    ::kill(child.pid, SIGWINCH);
    const std::string output = read_available(child.master_fd, 2000);
    CK_CHECK(output.find("GOT-RESIZE") != std::string::npos);
    wait_child(child.pid);
    ::close(child.master_fd);
}

CK_TEST(wake_interrupts_an_in_flight_posix_poll_without_creating_input) {
    PtyChild child = spawn_pty_child([](int slave_fd) {
        PosixTerminal term(slave_fd, slave_fd);
        std::thread waker([&term] {
            ::usleep(50'000);
            term.wake();
        });
        const auto started = std::chrono::steady_clock::now();
        const auto events = term.poll(PosixTerminal::monotonic_now_nanos() + 2'000'000'000LL);
        const auto elapsed = std::chrono::steady_clock::now() - started;
        waker.join();
        if (events.empty() && elapsed < std::chrono::milliseconds(750)) term.write("GOT-WAKE");
    });
    const std::string output = read_available(child.master_fd, 1500);
    CK_CHECK(output.find("GOT-WAKE") != std::string::npos);
    wait_child(child.pid);
    ::close(child.master_fd);
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
            PosixTerminal first(first_slave, first_slave);
            PosixTerminal second(second_slave, second_slave);
            struct winsize ws{};
            ws.ws_col = 100;
            ws.ws_row = 40;
            if (::ioctl(first_slave, TIOCSWINSZ, &ws) != 0) ::_exit(1);
            for (const TerminalEvent& event : first.poll(PosixTerminal::monotonic_now_nanos()))
                first_resized = first_resized || std::holds_alternative<ResizeEvent>(event);
            for (const TerminalEvent& event : second.poll(PosixTerminal::monotonic_now_nanos()))
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
        std::vector<std::unique_ptr<PosixTerminal>> held;  // keep the first 8 slots occupied
        for (int i = 0; i < 8; ++i)
            held.push_back(std::make_unique<PosixTerminal>(slave_fds[static_cast<std::size_t>(i)],
                                                             slave_fds[static_cast<std::size_t>(i)]));
        // The 9th construction must abort here (CKV_ASSERT fires
        // before any write/tcsetattr on slave_fds[8]) — the process
        // dies and the line after this never runs.
        auto ninth = std::make_unique<PosixTerminal>(slave_fds[8], slave_fds[8]);
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

#endif  // CKVISION_HAS_POSIX_TERMINAL
