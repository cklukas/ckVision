// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Golden regression test for HelloApp's ckVision-owned visual contract.
// It pins its D-014 frames so regressions in Window/MenuBar/StatusLine/
// Button/StaticText/Desktop rendering are visible even when no other test
// happens to assert the relevant cells.
//
// greeting_box() is deliberately non-blocking: a command handler adds
// the modal and returns, so a normal Application::step paints it and
// later events dismiss it. This keeps event routing single-layered.
//
// To intentionally update these fixtures after a deliberate rendering
// change: rebuild and run
//   build/tools/docgen/generate_hello_golden tests/golden
// then review the diff like any other source change before
// committing — regeneration is a manual, reviewed act, never
// automatic.
#include "cvision/testing/cktest.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "hello_app.hpp"

#include <fstream>
#include <sstream>

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;

namespace {
std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}
}  // namespace

CK_TEST(hello_initial_frame_matches_the_pinned_golden_dump) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);
    app.step(0);

    const std::string actual = ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    const std::string expected = read_file("golden/hello_initial.dump");
    CK_CHECK(!expected.empty());
    CK_CHECK(actual == expected);
}

CK_TEST(hello_greeting_dialog_frame_matches_the_pinned_golden_dump) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);
    app.step(0);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    app.step(0);
    const std::string actual = ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    app.step(0);

    const std::string expected = read_file("golden/hello_greeting.dump");
    CK_CHECK(!expected.empty());
    CK_CHECK(actual == expected);
}

// --- Application behavior, in addition to pixels --------------------------

CK_TEST(alt_g_opens_the_greeting_dialog) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);
    app.step(0);
    CK_CHECK(hello.desktop().windows().empty());

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    CK_CHECK(hello.desktop().windows().size() == 1);
    CK_CHECK(hello.desktop().windows().front()->title() == "Hello, World!");
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    app.step(0);
}

CK_TEST(hello_about_dialog_carries_the_project_copyright) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);

    CK_CHECK(app.execute_command(app.commands().standard().help));
    app.step(0);
    const std::string frame =
        ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    CK_CHECK(frame.find("Copyright (c) 2026 C. Klukas. All rights reserved.") != std::string::npos);
}

CK_TEST(while_the_greeting_dialog_is_open_background_accelerators_are_scoped_out) {
    // The greeting box is a real modal (M9/WP-15, D-021) — Alt+X
    // (kQuit) is a BACKGROUND accelerator while it's open, and Esc
    // dismisses the box (cancel_request).
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);
    app.step(0);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    CK_CHECK(hello.desktop().windows().size() == 1);
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}});
    CK_CHECK(!app.quit_requested());  // scoped out while the dialog is modal
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    app.step(0);
    CK_CHECK(hello.desktop().windows().empty());

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}});
    CK_CHECK(app.quit_requested());  // restored once the modal closes
}

CK_TEST(the_greeting_message_box_dismisses_through_its_standard_ok_action) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);
    app.step(0);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    CK_CHECK(hello.desktop().windows().size() == 1);
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    app.step(0);
    CK_CHECK(hello.desktop().windows().empty());
}

CK_TEST(alt_x_quits_via_the_status_line_shortcut) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);
    app.step(0);
    CK_CHECK(!app.quit_requested());
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}});
    CK_CHECK(app.quit_requested());
}
