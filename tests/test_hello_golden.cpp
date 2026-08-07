// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Golden regression test for HelloApp — a faithful ckVision port of
// Turbo Vision's own canonical minimal example (see hello_app.hpp's
// file comment). Pins the EXACT rendered output (via the golden dump
// format, D-014) of a well-known, independently-specified small app
// shape, so any unintended visual regression in Window/MenuBar/
// StatusLine/Button/StaticText/Desktop rendering shows up here even
// if no other test happens to assert the specific cells involved.
//
// To intentionally update these fixtures after a deliberate rendering
// change: rebuild and run
//   build/tools/docgen/generate_hello_golden tests/golden
// then review the diff like any other source change before
// committing — regeneration is a manual, reviewed act, never
// automatic.
#include "cktest.hpp"
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
    const std::string expected = read_file("golden/hello_greeting.dump");
    CK_CHECK(!expected.empty());
    CK_CHECK(actual == expected);
}

// --- Behavioral parity with the original (not just pixels) ----------------

CK_TEST(alt_g_opens_the_greeting_dialog_matching_the_originals_alt_g_mnemonic) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);
    app.step(0);
    CK_CHECK(hello.desktop().windows().empty());

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    CK_CHECK(hello.desktop().windows().size() == 1);
    CK_CHECK(hello.desktop().windows().front()->title() == "Hello, World!");
}

CK_TEST(every_greeting_button_dismisses_the_dialog_matching_the_originals_shared_cmcancel) {
    // The original wires all four buttons to the SAME command
    // (cmCancel) — every button means "dismiss," none is individually
    // distinguishable by the handler. The port preserves that
    // observable behavior (all four close the dialog) even though it
    // gets there structurally differently — see hello_app.cpp's
    // greeting_box() doc comment for why.
    for (int button_index = 0; button_index < 4; ++button_index) {
        ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
        ManualClock clock;
        Application app(term, clock);
        ckv::hello::HelloApp hello(app);
        app.step(0);
        app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
        CK_CHECK(hello.desktop().windows().size() == 1);

        for (int i = 0; i < button_index; ++i) app.focus_next();
        app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
        app.step(0);  // drains the deferred schedule_self_detach() post
        CK_CHECK(hello.desktop().windows().empty());
    }
}

CK_TEST(alt_x_quits_matching_the_originals_alt_x_status_line_shortcut) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);
    app.step(0);
    CK_CHECK(!app.quit_requested());
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}});
    CK_CHECK(app.quit_requested());
}
