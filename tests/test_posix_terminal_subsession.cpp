// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#if !defined(_WIN32)

#include <poll.h>
#include <unistd.h>

#include <csignal>
#include <chrono>
#include <string>
#include <vector>

#include "cvision/term/posix_terminal_subsession.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/terminal_view.hpp"

#include "cvision/testing/cktest.hpp"

namespace {

std::string row_text(const ckv::term::TerminalSnapshot& snapshot, int row) {
    std::string text;
    for (int column = 0; column < snapshot.cells.width; ++column) {
        const ckv::Cell& cell = snapshot.cell_buffer[static_cast<std::size_t>(row * snapshot.cells.width + column)];
        if (!cell.is_continuation()) text += cell.grapheme();
    }
    return text;
}

std::string screen_text(const ckv::term::TerminalSnapshot& snapshot) {
    std::string text;
    for (int row = 0; row < snapshot.cells.height; ++row) text += row_text(snapshot, row);
    return text;
}

// Waits for `needle` for up to `budget_ms` of real time.
//
// The budget is wall-clock and not a number of attempts, because an attempt
// is not a unit of waiting: `poll` returns immediately while a child is
// producing, so the same loop that spans seconds in a debug build spans
// milliseconds in an optimised one, and a child that needs a quarter of a
// second to finish counting is declared silent. What these tests mean is
// "within a few seconds", so that is what they now say.
bool pump_until(ckv::term::PosixTerminalSubsession& session, std::string_view needle,
                int budget_ms = 5'000, int poll_ms = 10) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);
    for (;;) {
        if (session.file_descriptor() >= 0) {
            pollfd ready{session.file_descriptor(), POLLIN | POLLHUP, 0};
            (void)::poll(&ready, 1, poll_ms);
        }
        (void)session.drain(32 * 1024);
        if (screen_text(session.snapshot()).find(needle) != std::string::npos) return true;
        if (std::chrono::steady_clock::now() >= deadline) return false;
    }
}

// Drains until the child has exited and its output has been read to the end.
//
// This is what the bounded-drain tests below should wait for. A child's exit
// is a definite event: after EOF there is, by definition, nothing further to
// read, so "did the tail arrive" becomes a statement about the contract
// rather than about how far this machine got through a loop before giving
// up. The deadline is a guard against hanging a test run, not the thing
// being waited for — reaching it is a failure.
bool pump_until_exit(ckv::term::PosixTerminalSubsession& session, int guard_ms = 30'000) {
    const auto guard = std::chrono::steady_clock::now() + std::chrono::milliseconds(guard_ms);
    while (std::chrono::steady_clock::now() < guard) {
        if (session.file_descriptor() >= 0) {
            pollfd ready{session.file_descriptor(), POLLIN | POLLHUP, 0};
            (void)::poll(&ready, 1, 10);
        }
        (void)session.drain(32 * 1024);
        if (session.state() == ckv::term::TerminalSubsessionState::Exited) return true;
    }
    return false;
}

}  // namespace

CK_TEST(posix_terminal_subsession_runs_bash_interactively_with_readline_input) {
    if (::access("/bin/bash", X_OK) != 0) return;
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/bash", {"--noprofile", "--norc", "-i"});
    launch.profile.cells = ckv::Size{48, 4};
    launch.environment = {{"TERM", "xterm"}, {"PS1", "ckv-bash$ "}};
    // Bounded, because this child is an INTERACTIVE shell and the default
    // policy is not bounded. `WaitForExit` sends SIGHUP and SIGTERM and then
    // loops until the child is reaped, never escalating — which is exactly
    // what that policy promises. An interactive bash ignores SIGTERM, and
    // whether it dies on the SIGHUP is a race against its own startup: this
    // test hung in ~1 run in 3, in the DESTRUCTOR, long after its assertions
    // had passed, and was read by four sessions as one of the waits above
    // timing out. It is not — `pump_until` is bounded at 5 s and returns
    // false, so a needle that never arrives fails in five seconds and cannot
    // produce a 60 s timeout.
    //
    // The three cases below whose children resist signals already say this;
    // this one needed it for the same reason and did not say it.
    launch.exit_policy = ckv::term::TerminalExitPolicy::TerminateAfterGrace;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "ckv-bash$"));

    session->send_input("printf 'readline-ok\\n'");
    session->send_input("\n");
    CK_CHECK(pump_until(*session, "readline-ok"));
}

CK_TEST(posix_terminal_subsession_presents_a_real_alternate_screen_transition) {
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "printf '\\033[?1049h\\033[2J\\033[HALT-SCREEN\\033[?25h'; sleep 1"});
    launch.profile.cells = ckv::Size{48, 4};
    launch.environment = {{"TERM", "xterm-256color"}};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "ALT-SCREEN"));
    CK_CHECK(session->snapshot().alternate_buffer);
    CK_CHECK(session->snapshot().cursor.visible);
    session->close();
}

CK_TEST(a_child_starts_from_default_signal_dispositions_whatever_its_host_ignored) {
    // exec restores the signals a process HANDLED and leaves the ones it
    // IGNORED ignored. That asymmetry is the bug: an application that ignores
    // SIGPIPE process-wide — which anything writing to sockets or pipes of its
    // own does — otherwise hands the same SIG_IGN to every shell it opens, and
    // to everything that shell runs.
    //
    // The observation is an ordinary pipeline whose reader leaves after one
    // byte. With the disposition reset, the kernel ends `cat` and the pipeline
    // is silent, which is how pipelines have always worked. With it inherited,
    // `cat` survives the write, collects EPIPE, and complains onto the
    // reader's screen.
    struct sigaction ignored = {};
    ignored.sa_handler = SIG_IGN;
    sigemptyset(&ignored.sa_mask);
    struct sigaction previous = {};
    CK_CHECK(sigaction(SIGPIPE, &ignored, &previous) == 0);

    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "cat /dev/zero | dd bs=1 count=1 of=/dev/null 2>/dev/null; "
                          "printf PIPELINE-DONE"});
    launch.profile.cells = ckv::Size{80, 24};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    const bool finished = pump_until(*session, "PIPELINE-DONE");
    // Put the test process back before asserting, so a failure here cannot
    // leave the rest of the suite running under a disposition it did not ask
    // for.
    (void)sigaction(SIGPIPE, &previous, nullptr);

    // The pipeline really ran, so silence below is silence and not absence.
    CK_CHECK(finished);
    // What is looked for is `cat` still being ALIVE to complain — its own
    // message, which it can only reach after a write that returned instead of
    // ending it. A bare "Broken pipe" would be the wrong thing to look for: a
    // shell is entitled to report a job the kernel killed that way, and that
    // report means the fix worked.
    const std::string screen = screen_text(session->snapshot());
    CK_CHECK(screen.find("cat:") == std::string::npos);
}

CK_TEST(posix_terminal_subsession_bounds_a_noisy_child_without_losing_later_output) {
    // The first run is intentionally larger than one application drain
    // budget.  The final marker must still arrive after several fair drains;
    // a parser that blocks, drops its framing state, or starves the owner
    // thread cannot satisfy this contract.
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "head -c 50000 /dev/zero | tr '\\000' x; printf FLOOD-DONE"});
    launch.profile.cells = ckv::Size{80, 24};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until_exit(*session));
    CK_CHECK(screen_text(session->snapshot()).find("FLOOD-DONE") != std::string::npos);
    CK_CHECK(session->snapshot().diagnostics.size() <= 64U);
}

CK_TEST(posix_terminal_subsession_preserves_output_tail_when_child_exits_between_bounded_drains) {
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "head -c 100000 /dev/zero | tr '\\000' x; printf EXIT-TAIL"});
    launch.profile.cells = ckv::Size{80, 24};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    // The tail is written and the child is gone before the last drains run,
    // which is the case this exists for: what was already in the pipe must
    // survive the exit that closed it.
    CK_CHECK(pump_until_exit(*session));
    CK_CHECK(screen_text(session->snapshot()).find("EXIT-TAIL") != std::string::npos);
    CK_CHECK(session->file_descriptor() < 0);
}

CK_TEST(posix_terminal_subsession_reads_only_its_private_pty) {
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf child"});
    launch.profile.cells = ckv::Size{16, 2};
        launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    std::unique_ptr<ckv::term::PosixTerminalSubsession> session =
        ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(session->file_descriptor() >= 0);

    pollfd ready{session->file_descriptor(), POLLIN, 0};
    CK_CHECK(::poll(&ready, 1, 1'000) == 1);
    CK_CHECK((ready.revents & POLLIN) != 0);
    CK_CHECK(session->drain(16 * 1024));
    const ckv::term::TerminalSnapshot snapshot = session->snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "c");
    CK_CHECK(snapshot.cell_buffer[4].grapheme() == "d");
}

CK_TEST(application_external_wait_handles_include_attached_private_child_sessions) {
    ckv::term::HeadlessTerminal outer(ckv::Size{24, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app(outer, clock);
    ckv::term::TerminalLaunchSpec wait_spec =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf child; exit 0"});
    wait_spec.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    ckv::term::TerminalSubsession& session = app.launch_terminal_subsession(std::move(wait_spec));
    const std::span<const ckv::term::WaitHandle> handles = app.wait_handles();
    CK_CHECK(handles.size() == 1U);
    CK_CHECK(handles.front().kind == ckv::term::WaitHandleKind::PosixFileDescriptor);
    CK_CHECK(handles.front().value == static_cast<std::uintptr_t>(session.wait_handles().front().value));

    for (int attempt = 0; attempt < 100 && session.state() != ckv::term::TerminalSubsessionState::Exited; ++attempt) {
        const std::span<const ckv::term::WaitHandle> ready_handles = app.wait_handles();
        if (!ready_handles.empty()) {
            pollfd ready{static_cast<int>(ready_handles.front().value), POLLIN | POLLHUP, 0};
            (void)::poll(&ready, 1, 10);
        }
        (void)app.step(0);
    }
    CK_CHECK(session.state() == ckv::term::TerminalSubsessionState::Exited);
    CK_CHECK(app.wait_handles().empty());
}

CK_TEST(posix_terminal_subsession_output_repaints_a_terminal_view_without_chrome_input) {
    ckv::term::HeadlessTerminal outer(ckv::Size{24, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app(outer, clock);
    ckv::term::TerminalLaunchSpec paint_spec =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf ready"});
    paint_spec.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    ckv::term::TerminalSubsession& session = app.launch_terminal_subsession(std::move(paint_spec));
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    view->set_fills_root(false);
    view->set_bounds(ckv::Rect{1, 1, 20, 3});
    app.root().add_child(std::move(view));

    bool painted = false;
    for (int attempt = 0; attempt < 100 && !painted; ++attempt) {
        const std::span<const ckv::term::WaitHandle> handles = session.wait_handles();
        if (!handles.empty()) {
            pollfd ready{static_cast<int>(handles.front().value), POLLIN | POLLHUP, 0};
            (void)::poll(&ready, 1, 10);
        }
        (void)app.step(0);
        painted = app.current_frame().at(ckv::Point{1, 1}).grapheme() == "r";
    }
    CK_CHECK(painted);
}

CK_TEST(posix_terminal_subsession_paints_initial_prompt_and_followup_output_without_menu_redraw) {
    ckv::term::HeadlessTerminal outer(ckv::Size{32, 8});
    ckv::ManualClock clock;
    ckv::ui::Application app(outer, clock);
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-i"});
    launch.profile.cells = ckv::Size{24, 4};
    launch.environment = {{"TERM", "xterm"}, {"PS1", "ckv-redraw$ "}};
    // This is an interactive shell. Its destructor must have the same bounded
    // teardown policy as the bash interaction test above: an interactive shell
    // may ignore SIGTERM, and a successful paint assertion must not leave the
    // suite waiting forever for test-fixture cleanup.
    launch.exit_policy = ckv::core::TerminalExitPolicy::TerminateAfterGrace;
    ckv::term::TerminalSubsession& session = app.launch_terminal_subsession(std::move(launch));
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    view->set_fills_root(false);
    view->set_bounds(ckv::Rect{1, 1, 24, 4});
    app.root().add_child(std::move(view));

    const auto pump_frame_until = [&](std::string_view needle) {
        for (int attempt = 0; attempt < 160; ++attempt) {
            const std::span<const ckv::term::WaitHandle> handles = session.wait_handles();
            if (!handles.empty()) {
                pollfd ready{static_cast<int>(handles.front().value), POLLIN | POLLHUP, 0};
                (void)::poll(&ready, 1, 10);
            }
            (void)app.step(0);
            std::string frame_text;
            for (int row = 1; row < 5; ++row)
                for (int column = 1; column < 25; ++column)
                    frame_text += app.current_frame().at(ckv::Point{column, row}).grapheme();
            if (frame_text.find(needle) != std::string::npos) return true;
        }
        return false;
    };

    CK_CHECK(pump_frame_until("ckv-redraw$"));
    session.send_input("printf 'ls-redraw-ok\\n'\n");
    CK_CHECK(pump_frame_until("ls-redraw-ok"));
}

CK_TEST(posix_terminal_subsession_surfaces_spawn_failure_without_throwing) {
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/definitely/missing/ckvision-child");
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Failed);
    CK_CHECK(!session->snapshot().diagnostics.empty());
    CK_CHECK(session->file_descriptor() < 0);
}

CK_TEST(posix_terminal_subsessions_are_isolated) {
    ckv::term::TerminalLaunchSpec left = ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf left"});
    ckv::term::TerminalLaunchSpec right = ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf right"});
    left.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto first = ckv::term::PosixTerminalSubsession::launch(std::move(left));
    right.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto second = ckv::term::PosixTerminalSubsession::launch(std::move(right));
    pollfd first_ready{first->file_descriptor(), POLLIN, 0};
    pollfd second_ready{second->file_descriptor(), POLLIN, 0};
    CK_CHECK(::poll(&first_ready, 1, 1'000) == 1);
    CK_CHECK(::poll(&second_ready, 1, 1'000) == 1);
    CK_CHECK(first->drain(16 * 1024));
    CK_CHECK(second->drain(16 * 1024));
    CK_CHECK(first->snapshot().cell_buffer[0].grapheme() == "l");
    CK_CHECK(second->snapshot().cell_buffer[0].grapheme() == "r");
}

CK_TEST(posix_terminal_subsession_observes_child_exit) {
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "exit 7"});
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    pollfd ready{session->file_descriptor(), POLLIN | POLLHUP, 0};
    CK_CHECK(::poll(&ready, 1, 1'000) == 1);
    for (int attempt = 0; attempt < 100 && session->state() != ckv::term::TerminalSubsessionState::Exited; ++attempt) {
        const bool changed = session->drain(16 * 1024);
        if (session->state() == ckv::term::TerminalSubsessionState::Exited)
            CK_CHECK(changed);
        pollfd status_ready{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&status_ready, 1, 10);
    }
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Exited);
    CK_CHECK(session->file_descriptor() < 0);
    session->send_input("late-input");
    CK_CHECK(session->take_pending_input().empty());
}

CK_TEST(posix_terminal_subsession_propagates_content_resize_to_child) {
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "read ready; stty size"});
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    session->resize(ckv::Size{40, 12}, ckv::Size{9, 18});
    session->send_input("\n");
    pollfd ready{session->file_descriptor(), POLLIN, 0};
    CK_CHECK(::poll(&ready, 1, 1'000) == 1);
    for (int attempt = 0; attempt < 100 && screen_text(session->snapshot()).find("12 40") == std::string::npos; ++attempt) {
        (void)session->drain(16 * 1024);
        pollfd more{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&more, 1, 10);
    }
    CK_CHECK(screen_text(session->snapshot()).find("12 40") != std::string::npos);
}

CK_TEST(posix_terminal_subsession_uses_explicit_launch_environment_and_directory) {
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program(
            "/bin/sh", {"-c", "printf %s \"$CKV_CHILD_VALUE\"; test \"$(pwd -P)\" = \"$(cd /tmp && pwd -P)\" && printf directory"});
    launch.working_directory = "/tmp";
    launch.environment.push_back({"CKV_CHILD_VALUE", "declared"});
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    pollfd ready{session->file_descriptor(), POLLIN, 0};
    CK_CHECK(::poll(&ready, 1, 1'000) == 1);
    for (int attempt = 0; attempt < 100 && screen_text(session->snapshot()).find("declareddirectory") == std::string::npos; ++attempt) {
        (void)session->drain(16 * 1024);
        pollfd more{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&more, 1, 10);
    }
    CK_CHECK(screen_text(session->snapshot()).find("declareddirectory") != std::string::npos);
}

CK_TEST(a_launch_spec_can_name_the_child_something_other_than_its_executable) {
    // The leading dash is the whole point: it is the only way a shell is told
    // it is a login shell, so a terminal that cannot set argv[0] cannot open
    // the shell its user gets from login(1) or from any other terminal.
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf '[%s]' \"$0\""});
    launch.argv0 = "-sh";
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    pollfd ready{session->file_descriptor(), POLLIN, 0};
    CK_CHECK(::poll(&ready, 1, 1'000) == 1);
    for (int attempt = 0; attempt < 100 && screen_text(session->snapshot()).find("[-sh]") == std::string::npos;
         ++attempt) {
        (void)session->drain(16 * 1024);
        pollfd more{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&more, 1, 10);
    }
    CK_CHECK(screen_text(session->snapshot()).find("[-sh]") != std::string::npos);
}

CK_TEST(a_launch_spec_without_an_argv0_still_names_the_child_by_its_path) {
    // The field is an addition, not a new obligation: every existing caller
    // leaves it empty and must keep seeing exactly what it saw before.
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf '[%s]' \"$0\""});
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    pollfd ready{session->file_descriptor(), POLLIN, 0};
    CK_CHECK(::poll(&ready, 1, 1'000) == 1);
    for (int attempt = 0; attempt < 100 && screen_text(session->snapshot()).find("[/bin/sh]") == std::string::npos;
         ++attempt) {
        (void)session->drain(16 * 1024);
        pollfd more{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&more, 1, 10);
    }
    CK_CHECK(screen_text(session->snapshot()).find("[/bin/sh]") != std::string::npos);
}

CK_TEST(an_explicit_only_subsession_inherits_neither_environment_nor_directory) {
    // Isolation is still available in full; it is now asked for rather than
    // imposed. The default is the opposite, because a terminal whose shell
    // has no HOME is a terminal that cannot run the user's own programs.
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "test \"$(pwd -P)\" = / && test -z \"$HOME\" && printf isolated"});
    launch.environment_policy = ckv::term::TerminalEnvironmentPolicy::ExplicitOnly;
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    pollfd ready{session->file_descriptor(), POLLIN | POLLHUP, 0};
    CK_CHECK(::poll(&ready, 1, 1'000) == 1);
    for (int attempt = 0; attempt < 100 && screen_text(session->snapshot()).find("isolated") == std::string::npos;
         ++attempt) {
        (void)session->drain(16 * 1024);
        pollfd more{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&more, 1, 10);
    }
    CK_CHECK(screen_text(session->snapshot()).find("isolated") != std::string::npos);
}

CK_TEST(posix_terminal_subsession_close_releases_private_session_state) {
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "sleep 10"});
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(session->file_descriptor() >= 0);
    session->close();
    CK_CHECK(session->file_descriptor() < 0);
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Closed);
}

CK_TEST(posix_terminal_subsession_honours_bounded_termination_after_grace) {
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "trap '' TERM HUP; printf ready; while :; do sleep 1; done"});
    launch.exit_policy = ckv::term::TerminalExitPolicy::TerminateAfterGrace;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(session->file_descriptor() >= 0);
    for (int attempt = 0; attempt < 100 && screen_text(session->snapshot()).find("ready") == std::string::npos;
         ++attempt) {
        pollfd ready{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&ready, 1, 10);
        (void)session->drain(16 * 1024);
    }
    CK_CHECK(screen_text(session->snapshot()).find("ready") != std::string::npos);
    session->close();
    CK_CHECK(session->file_descriptor() < 0);
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Closed);
}

CK_TEST(a_child_still_writing_does_not_wedge_the_close_that_ends_it) {
    // The hangup is the signal, not the descriptor.
    //
    // A child blocked writing to its terminal cannot act on any signal until
    // that write completes, and the write cannot complete while nothing empties
    // the PTY. Closing the master first therefore wedges exactly the child this
    // protocol exists to end: on macOS it enters "exiting" and does not reap
    // even under SIGKILL, and close() blocks in waitpid with no way out. One
    // flooding program would take its whole host with it — for a multiplexer's
    // server, every other terminal on the machine.
    //
    // So the close protocol signals first, keeps emptying the PTY while it
    // waits, and closes the master last. This test states the outcome that
    // depends on all three: a child that is mid-flood and has nobody reading it
    // is still gone, promptly, when its terminal is closed.
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "yes cvision-flood-line"});
    launch.exit_policy = ckv::term::TerminalExitPolicy::TerminateAfterGrace;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "cvision-flood-line"));

    // Then stop reading, and give the child long enough to fill the PTY and
    // block in write() — which is what a server that has stopped draining one
    // terminal looks like from the child's side, and the state the wedge needs.
    for (int wait = 0; wait < 20; ++wait) (void)::poll(nullptr, 0, 10);

    const auto start = std::chrono::steady_clock::now();
    session->close();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    // Grace is one second and the kill escalation two more; anything near those
    // means the child had to be forced, and anything above them means it was
    // wedged. A child killed while writing takes milliseconds.
    CK_CHECK(elapsed < 3'000);
    CK_CHECK(session->file_descriptor() < 0);
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Closed);
}

CK_TEST(a_child_can_be_asked_to_end_without_waiting_for_it) {
    // The half of the close protocol a host needs when the WAITING is its own
    // decision: a multiplexer ending a session tells every child to go, keeps
    // drawing while they do, and escalates on its own schedule. Doing that with
    // `close()` means blocking the loop once per terminal; doing it without a
    // primitive means a host reaching for the pid.
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf ready; sleep 30"});
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "ready"));

    session->request_termination();
    // It returns at once — the point of it — and the child does go.
    CK_CHECK(pump_until_exit(*session, 5'000));
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Exited);
    // And the descriptor is still this host's to read until it closes: asking a
    // child to end is not the same as tearing its terminal down.
    session->close();
    CK_CHECK(session->file_descriptor() < 0);
}

CK_TEST(asking_twice_is_not_an_error_and_neither_is_asking_a_child_that_has_gone) {
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf ready; exit 0"});
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until_exit(*session, 5'000));
    session->request_termination();
    session->request_termination();
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Exited);
}

CK_TEST(a_child_that_declines_to_end_can_be_ended_without_waiting_for_it) {
    // The other end of the host's own escalation. `request_termination()` lets a
    // host ask on its own schedule; this is what it must be able to do when the
    // grace it timed itself runs out and the program is still there. `close()`
    // would do it too — by waiting, seconds per terminal, inside a loop that has
    // other terminals to serve — so a host that owns its clock needs the signal
    // on its own.
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "trap '' TERM HUP; printf ready; while :; do sleep 1; done"});
    launch.exit_policy = ckv::term::TerminalExitPolicy::TerminateAfterGrace;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "ready"));

    // Asked, and it declines — which is the state this exists for. Bounded, and
    // long enough that a child which was going to honour the signal would have.
    session->request_termination();
    for (int attempt = 0; attempt < 30; ++attempt) {
        pollfd ready{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&ready, 1, 10);
        (void)session->drain(16 * 1024);
    }
    CK_CHECK(session->state() != ckv::term::TerminalSubsessionState::Exited);

    const auto start = std::chrono::steady_clock::now();
    session->request_kill();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    // It returns at once — a signal, not a wait. `close()` on this same child
    // spends its grace and then two more seconds reaping.
    CK_CHECK(elapsed < 100);
    // And the child does go, observed by the same drain any other exit is.
    CK_CHECK(pump_until_exit(*session, 5'000));
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Exited);

    // Asking twice, and asking a child that has already gone, are both nothing.
    session->request_kill();
    session->request_kill();
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Exited);
    session->close();
    CK_CHECK(session->file_descriptor() < 0);
}

CK_TEST(posix_terminal_subsession_contains_abnormal_child_termination) {
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "kill -TERM $$"});
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    pollfd ready{session->file_descriptor(), POLLIN | POLLHUP, 0};
    CK_CHECK(::poll(&ready, 1, 1'000) == 1);
    for (int attempt = 0; attempt < 100 && session->state() != ckv::term::TerminalSubsessionState::Exited; ++attempt) {
        (void)session->drain(16 * 1024);
        pollfd more{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&more, 1, 10);
    }
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Exited);
}

CK_TEST(posix_terminal_subsession_confines_a_separately_launched_ckvision_application) {
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        CKV_NESTED_TERMINAL_CHILD_PATH);
    launch.profile.cells = ckv::Size{40, 12};
    launch.profile.cell_pixels = ckv::Size{9, 18};
    launch.environment = {{"TERM", "xterm"}};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    for (int attempt = 0; attempt < 50 && screen_text(session->snapshot()).find("NESTED-CKVISION") == std::string::npos;
         ++attempt) {
        pollfd ready{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&ready, 1, 20);
        (void)session->drain(16 * 1024);
    }
    CK_CHECK(screen_text(session->snapshot()).find("NESTED-CKVISION") != std::string::npos);
    CK_CHECK(session->snapshot().diagnostics.empty());
}

CK_TEST(posix_terminal_subsession_keeps_nested_ckvision_pixels_inside_terminal_view) {
    ckv::term::HeadlessTerminal outer(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(outer, clock);
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        CKV_NESTED_TERMINAL_CHILD_PATH);
    launch.profile.cells = ckv::Size{40, 12};
    launch.environment = {{"TERM", "xterm"}};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    ckv::term::TerminalSubsession& session = app.launch_terminal_subsession(std::move(launch));
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    view->set_fills_root(false);
    view->set_bounds(ckv::Rect{2, 2, 40, 12});
    app.root().add_child(std::move(view));
    (void)app.step(0);
    std::vector<ckv::Cell> outside_baseline(
        static_cast<std::size_t>(outer.size().width * outer.size().height));
    for (int row = 0; row < outer.size().height; ++row)
        for (int column = 0; column < outer.size().width; ++column)
            outside_baseline[static_cast<std::size_t>(row * outer.size().width + column)] =
                app.current_frame().at(ckv::Point{column, row});

    bool observed = false;
    for (int attempt = 0; attempt < 80 && !observed; ++attempt) {
        const std::span<const ckv::term::WaitHandle> handles = session.wait_handles();
        if (!handles.empty()) {
            pollfd ready{static_cast<int>(handles.front().value), POLLIN | POLLHUP, 0};
            (void)::poll(&ready, 1, 20);
        }
        (void)app.step(0);
        for (int row = 2; row < 14 && !observed; ++row)
            for (int column = 2; column < 42 && !observed; ++column)
                if (app.current_frame().at(ckv::Point{column, row}).grapheme() == "N") observed = true;
    }
    CK_CHECK(observed);

    bool leaked = false;
    for (int row = 0; row < outer.size().height; ++row) {
        for (int column = 0; column < outer.size().width; ++column) {
            const bool inside = column >= 2 && column < 42 && row >= 2 && row < 14;
            if (!inside && app.current_frame().at(ckv::Point{column, row}) !=
                               outside_baseline[static_cast<std::size_t>(row * outer.size().width + column)])
                leaked = true;
        }
    }
    CK_CHECK(!leaked);
}

CK_TEST(posix_terminal_subsession_decodes_the_example_sixel_child_output_privately) {
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "printf '\\033Pq#0;2;100;0;0!32~-!32~-!32~-!32~-!32~-!32~\\033\\\\'"});
    launch.profile.cells = ckv::Size{8, 4};
    launch.profile.cell_pixels = ckv::Size{9, 18};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    for (int attempt = 0; attempt < 100 && session->snapshot().rasters.empty(); ++attempt) {
        pollfd ready{session->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&ready, 1, 10);
        (void)session->drain(16 * 1024);
    }
    const ckv::term::TerminalSnapshot snapshot = session->snapshot();
    CK_CHECK(snapshot.rasters.size() == 1U);
    CK_CHECK(snapshot.rasters[0].image != nullptr);
}

#endif

// --- The environment a child is given -------------------------------------

CK_TEST(a_child_inherits_the_environment_its_owner_is_running_in) {
    // A shell without HOME reports its own builtins as broken -- "cd: HOME
    // not set" -- which reads as the program failing rather than as the host
    // having withheld something the program had every reason to expect.
    ::setenv("CKV_ENV_PROBE", "inherited", 1);
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "printf '[%s]\\n' \"$CKV_ENV_PROBE\"; sleep 5"});
    launch.profile.cells = ckv::Size{48, 4};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "[inherited]"));
    ::unsetenv("CKV_ENV_PROBE");
}

CK_TEST(a_spec_entry_replaces_the_inherited_one_rather_than_colliding_with_it) {
    ::setenv("CKV_ENV_PROBE", "inherited", 1);
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "printf '[%s]\\n' \"$CKV_ENV_PROBE\"; sleep 5"});
    launch.profile.cells = ckv::Size{48, 4};
    launch.environment = {{"CKV_ENV_PROBE", "overridden"}};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "[overridden]"));
    ::unsetenv("CKV_ENV_PROBE");
}

CK_TEST(an_explicit_only_child_sees_nothing_it_was_not_handed) {
    // The sandboxed case still exists; it is simply no longer the default.
    ::setenv("CKV_ENV_PROBE", "inherited", 1);
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "printf '[%s]\\n' \"$CKV_ENV_PROBE\"; sleep 5"});
    launch.profile.cells = ckv::Size{48, 4};
    launch.environment_policy = ckv::term::TerminalEnvironmentPolicy::ExplicitOnly;
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "[]"));
    ::unsetenv("CKV_ENV_PROBE");
}

CK_TEST(process_id_names_the_running_child_and_nobody_once_it_has_gone) {
    // The child prints its own $$, so the accessor is checked against the
    // child's account of itself rather than against another read of the same
    // member. `read` keeps it alive until the test decides otherwise.
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "echo CKV-PID:$$:DONE; read ignored"});
    launch.profile.cells = ckv::Size{48, 6};
    launch.environment = {{"TERM", "xterm"}};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, ":DONE"));

    const int reported = session->process_id();
    CK_CHECK(reported > 0);
    const std::string screen = screen_text(session->snapshot());
    const auto at = screen.find("CKV-PID:");
    CK_CHECK(at != std::string::npos);
    int printed = 0;
    for (std::size_t i = at + 8; i < screen.size() && screen[i] >= '0' && screen[i] <= '9'; ++i)
        printed = printed * 10 + (screen[i] - '0');
    CK_CHECK(printed == reported);

    session->request_kill();
    CK_CHECK(pump_until_exit(*session));
    CK_CHECK(session->process_id() == -1);
}

CK_TEST(the_seam_answers_the_pid_question_and_defaults_to_nobody) {
    // U5-b: a host holding only `core::TerminalSubsession&` may ask. The
    // POSIX session answers with its child; anything not holding a process —
    // here, the base default itself — answers -1 rather than obliging every
    // fake and mirror to say "no pid" in its own words.
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "echo SEAM-UP; read ignored"});
    launch.profile.cells = ckv::Size{48, 6};
    launch.environment = {{"TERM", "xterm"}};
    launch.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "SEAM-UP"));
    ckv::core::TerminalSubsession& seam = *session;
    CK_CHECK(seam.process_id() == session->process_id());
    CK_CHECK(seam.process_id() > 0);
    session->request_kill();
    CK_CHECK(pump_until_exit(*session));
    CK_CHECK(seam.process_id() == -1);
}

CK_TEST(a_launch_that_does_not_name_an_exit_policy_is_refused_before_it_forks) {
    // The policy decides whether `close()` — and so the destructor — is
    // bounded. Choosing one on the caller's behalf is the library deciding, in
    // silence, whether the caller's program can hang on exit: this suite hung
    // in roughly one run in three for exactly that reason, having never said
    // which policy it wanted. So an unnamed policy is refused.
    ckv::term::TerminalLaunchSpec unnamed = ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf hi"});
    CK_CHECK(unnamed.exit_policy == ckv::core::TerminalExitPolicy::Unspecified);
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(unnamed));
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Failed);
    CK_CHECK(!session->snapshot().diagnostics.empty());
    // Refused BEFORE the fork: no child, no descriptor, nothing to reap. A
    // refusal that had already spawned would leave the very orphan the policy
    // is about.
    CK_CHECK(session->file_descriptor() < 0);
    // And the diagnostic names the way out, not only the fault.
    const std::string first = session->snapshot().diagnostics.front().message;
    CK_CHECK(first.find("exit policy") != std::string::npos);
    CK_CHECK(first.find("TerminateAfterGrace") != std::string::npos);
}

CK_TEST(naming_a_policy_is_all_that_an_unnamed_launch_was_missing) {
    // The positive partner. Without it the case above would pass just as
    // happily if `launch` had been broken for every spec rather than only for
    // unnamed ones — an "is refused" assertion with nothing saying what is
    // still accepted proves only that something failed.
    ckv::term::TerminalLaunchSpec named = ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf hi"});
    named.exit_policy = ckv::core::TerminalExitPolicy::WaitForExit;
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(named));
    CK_CHECK(session->state() != ckv::term::TerminalSubsessionState::Failed);
    CK_CHECK(session->file_descriptor() >= 0);
}
