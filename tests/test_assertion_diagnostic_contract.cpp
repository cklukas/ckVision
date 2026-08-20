// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A contract violation says what it was, exactly once, in every world.
//
// D-024 gives a live session's fatal handler the job of printing it, and for a
// good reason: the message must not land in the middle of an alternate screen,
// so it waits until every session has been restored. But that made the message
// CONDITIONAL on a session existing — and a headless process has none. Every
// headless test in the fleet therefore aborted with the diagnostic sitting in
// the ledger unread, and the reader got a bare SIGABRT and a guess. One such
// abort cost a session most of a debugging pass; the assert had been naming its
// own broken contract the whole time, to nobody.
//
// Both halves are asserted here, and both are one message:
//   * no live session -> the fallback speaks, because nothing else will;
//   * a live session   -> the handler speaks and the fallback stays quiet,
//                         because two copies of a diagnostic is its own defect
//                         and the restore-first ordering is the point of D-024.
// Counting occurrences is what makes the pair meaningful: "contains it" would
// pass on a double print, and "prints nothing" would pass on silence.
#if defined(CKVISION_HAS_POSIX_TERMINAL)

#include "cvision/core/assert.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <string_view>

namespace {

constexpr const char* kExpr = "a_contract_this_test_broke_on_purpose";

// Trips the diagnostic in a child whose stderr is a pipe, and answers with
// everything that arrived there. `with_live_session` decides which world:
// constructing a PosixTerminal over a pty is what retains the fatal handler.
bool diagnostic_of_a_violation(bool with_live_session, std::string& stderr_text, int& signal_out) {
    int pipe_fds[2] = {-1, -1};
    if (::pipe(pipe_fds) != 0) return false;

    int master_fd = -1;
    int slave_fd = -1;
    if (with_live_session && ::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) != 0)
        return false;

    const pid_t child = ::fork();
    if (child < 0) return false;
    if (child == 0) {
        ::close(pipe_fds[0]);
        if (::dup2(pipe_fds[1], STDERR_FILENO) < 0) ::_exit(1);
        ::close(pipe_fds[1]);
        if (with_live_session) {
            ::close(master_fd);
            // Leaks deliberately: this process is about to abort, and letting
            // the destructor run would restore the session and take the very
            // handler under test down with it.
            static ckv::term::PosixClock clock;
            static ckv::term::PosixTerminal terminal(clock, slave_fd, slave_fd,
                                                     ckv::term::baseline_capabilities(),
                                                     /*enable_capability_probes=*/false);
            (void)terminal.size();
        }
        ckv::detail::assertion_failed(kExpr, "test_assertion_diagnostic_contract.cpp", 4242);
        ::_exit(1);  // unreachable: assertion_failed is [[noreturn]]
    }

    ::close(pipe_fds[1]);
    if (with_live_session) ::close(slave_fd);
    // Read to EOF BEFORE waiting: a child that fills the pipe while the parent
    // waits for it to die is a deadlock, and this one writes a restore
    // sequence to its pty as well.
    char buffer[512];
    ssize_t got = 0;
    while ((got = ::read(pipe_fds[0], buffer, sizeof(buffer))) > 0)
        stderr_text.append(buffer, static_cast<std::size_t>(got));
    ::close(pipe_fds[0]);
    if (with_live_session) ::close(master_fd);

    int status = 0;
    if (::waitpid(child, &status, 0) != child) return false;
    signal_out = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
    return true;
}

std::size_t times_it_says_so(std::string_view text) {
    constexpr std::string_view kPrefix = "ckVision contract violation: ";
    std::size_t count = 0;
    for (std::size_t at = text.find(kPrefix); at != std::string_view::npos;
         at = text.find(kPrefix, at + kPrefix.size()))
        ++count;
    return count;
}

bool says_it_once_and_names_it(std::string_view text) {
    if (times_it_says_so(text) != 1) return false;
    // The expression, the file and the line: a prefix alone would pass on a
    // diagnostic that had lost the only three facts it exists to carry.
    return text.find(kExpr) != std::string_view::npos &&
           text.find("test_assertion_diagnostic_contract.cpp") != std::string_view::npos &&
           text.find(":4242)") != std::string_view::npos;
}

}  // namespace

int main() {
    std::string headless;
    int headless_signal = 0;
    if (!diagnostic_of_a_violation(/*with_live_session=*/false, headless, headless_signal)) return 1;
    if (headless_signal != SIGABRT) {
        std::fprintf(stderr, "headless: expected SIGABRT, got signal %d\n", headless_signal);
        return 1;
    }
    if (!says_it_once_and_names_it(headless)) {
        std::fprintf(stderr, "headless: no single named diagnostic in [%s]\n", headless.c_str());
        return 1;
    }

    std::string with_session;
    int session_signal = 0;
    if (!diagnostic_of_a_violation(/*with_live_session=*/true, with_session, session_signal))
        return 1;
    if (session_signal != SIGABRT) {
        std::fprintf(stderr, "session: expected SIGABRT, got signal %d\n", session_signal);
        return 1;
    }
    if (!says_it_once_and_names_it(with_session)) {
        std::fprintf(stderr, "session: no single named diagnostic in [%s]\n", with_session.c_str());
        return 1;
    }
    return 0;
}

#else

int main() { return 0; }

#endif  // defined(CKVISION_HAS_POSIX_TERMINAL)
