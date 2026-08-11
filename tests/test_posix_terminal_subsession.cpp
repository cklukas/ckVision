// Copyright (c) 2026 C. Klukas. All rights reserved.
#if !defined(_WIN32)

#include <poll.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "cvision/term/posix_terminal_subsession.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/terminal_view.hpp"

#include "cktest.hpp"

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

bool pump_until(ckv::term::PosixTerminalSubsession& session, std::string_view needle,
                int attempts = 200, int timeout_ms = 10) {
    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (session.file_descriptor() >= 0) {
            pollfd ready{session.file_descriptor(), POLLIN | POLLHUP, 0};
            (void)::poll(&ready, 1, timeout_ms);
        }
        (void)session.drain(32 * 1024);
        if (screen_text(session.snapshot()).find(needle) != std::string::npos) return true;
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
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "ALT-SCREEN"));
    CK_CHECK(session->snapshot().alternate_buffer);
    CK_CHECK(session->snapshot().cursor.visible);
    session->close();
}

CK_TEST(posix_terminal_subsession_bounds_a_noisy_child_without_losing_later_output) {
    // The first run is intentionally larger than one application drain
    // budget.  The final marker must still arrive after several fair drains;
    // a parser that blocks, drops its framing state, or starves the owner
    // thread cannot satisfy this contract.
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "i=0; while [ $i -lt 50000 ]; do printf x; i=$((i + 1)); done; printf FLOOD-DONE"});
    launch.profile.cells = ckv::Size{80, 24};
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "FLOOD-DONE", 1'200, 5));
    CK_CHECK(session->snapshot().diagnostics.size() <= 64U);
}

CK_TEST(posix_terminal_subsession_preserves_output_tail_when_child_exits_between_bounded_drains) {
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "i=0; while [ $i -lt 100000 ]; do printf x; i=$((i + 1)); done; printf EXIT-TAIL"});
    launch.profile.cells = ckv::Size{80, 24};
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(pump_until(*session, "EXIT-TAIL", 2'000, 2));
    for (int attempt = 0; attempt < 100 && session->state() != ckv::term::TerminalSubsessionState::Exited;
         ++attempt) {
        (void)session->drain(16 * 1024);
        if (session->file_descriptor() >= 0) {
            pollfd ready{session->file_descriptor(), POLLIN | POLLHUP, 0};
            (void)::poll(&ready, 1, 2);
        }
    }
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Exited);
    CK_CHECK(session->file_descriptor() < 0);
}

CK_TEST(posix_terminal_subsession_reads_only_its_private_pty) {
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf child"});
    launch.profile.cells = ckv::Size{16, 2};
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
    ckv::term::TerminalSubsession& session = app.launch_terminal_subsession(
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf child; exit 0"}));
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
    ckv::term::TerminalSubsession& session = app.launch_terminal_subsession(
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf ready"}));
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
    auto session = ckv::term::PosixTerminalSubsession::launch(std::move(launch));
    CK_CHECK(session->state() == ckv::term::TerminalSubsessionState::Failed);
    CK_CHECK(!session->snapshot().diagnostics.empty());
    CK_CHECK(session->file_descriptor() < 0);
}

CK_TEST(posix_terminal_subsessions_are_isolated) {
    ckv::term::TerminalLaunchSpec left = ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf left"});
    ckv::term::TerminalLaunchSpec right = ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf right"});
    auto first = ckv::term::PosixTerminalSubsession::launch(std::move(left));
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

CK_TEST(posix_terminal_subsession_does_not_inherit_ambient_environment_or_directory) {
    ckv::term::TerminalLaunchSpec launch = ckv::term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "test \"$(pwd -P)\" = / && test -z \"$HOME\" && printf isolated"});
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

CK_TEST(posix_terminal_subsession_contains_abnormal_child_termination) {
    ckv::term::TerminalLaunchSpec launch =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "kill -TERM $$"});
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
