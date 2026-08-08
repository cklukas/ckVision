// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Golden regression test for HelloApp's ckVision-owned visual contract.
// It pins its D-014 frames so regressions in Window/MenuBar/StatusLine/
// Button/StaticText/Desktop rendering are visible even when no other test
// happens to assert the relevant cells.
//
// greeting_box() blocks its caller (Desktop::exec_window, M9/WP-15/
// WP-17) — a test that wants to inspect or interact with the dialog
// WHILE it's open can't just dispatch(Alt+G) and continue afterward
// (by the time that call returns, the dialog has already closed).
// Instead it queues a closure via Application::post() BEFORE
// dispatching Alt+G: that closure runs during exec_window's own
// step()-pump, with the dialog fully attached (and, for the golden
// test, already painted — see its own comment for why that needs a
// second level of indirection).
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

    // The dialog attaches and paints during exec_window's FIRST nested
    // step() call — a closure queued now would run during that SAME
    // step()'s posted-work drain, BEFORE its own paint (step() paints
    // last). Queuing a second closure from inside the first defers the
    // actual capture to the NEXT step() call, by which point the prior
    // step's paint has already happened.
    std::string actual;
    app.post([&app, &actual] {
        app.post([&app, &actual] {
            const auto frame = ckv::scene::capture(app.composed_surface(), app.current_cursor());
            actual = ckv::golden::serialize(frame);
            app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
        });
    });
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});

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

    // The window attaches synchronously, inside dispatch(Alt+G) itself,
    // before exec_window's pump even starts — a posted closure sees it
    // on the pump's first step() without needing the golden test's
    // extra indirection.
    bool ran = false;
    app.post([&] {
        CK_CHECK(hello.desktop().windows().size() == 1);
        CK_CHECK(hello.desktop().windows().front()->title() == "Hello, World!");
        ran = true;
        app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    });
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    CK_CHECK(ran);
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

    bool ran = false;
    app.post([&] {
        CK_CHECK(hello.desktop().windows().size() == 1);
        app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}});
        CK_CHECK(!app.quit_requested());  // scoped out while the dialog is modal
        ran = true;
        app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    });
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    CK_CHECK(ran);
    CK_CHECK(hello.desktop().windows().empty());  // closed and self-detached during the pump

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}});
    CK_CHECK(app.quit_requested());  // restored once the modal closes
}

CK_TEST(every_greeting_button_dismisses_the_dialog) {
    // The example contract makes every greeting button a dismiss action.
    for (int button_index = 0; button_index < 4; ++button_index) {
        ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
        ManualClock clock;
        Application app(term, clock);
        ckv::hello::HelloApp hello(app);
        app.step(0);

        bool ran = false;
        app.post([&] {
            CK_CHECK(hello.desktop().windows().size() == 1);
            for (int i = 0; i < button_index; ++i) app.focus_next();
            app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
            ran = true;
        });
        app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
        CK_CHECK(ran);
        CK_CHECK(hello.desktop().windows().empty());
    }
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
