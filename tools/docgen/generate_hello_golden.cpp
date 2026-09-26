// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// One-shot generator for tests/golden/hello_*.dump — NOT run by the
// test suite or CI; run by hand (and only by hand, deliberately, same
// as any golden-fixture regeneration) whenever HelloApp's rendered
// output is meant to change, then the new fixture is reviewed and
// committed like any other source change.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "hello_app.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;

namespace {
void write_dump(const std::filesystem::path& dir, const std::string& name, Application& app) {
    const ckv::golden::Document doc = ckv::scene::capture(app.composed_surface(), app.current_cursor());
    const std::string text = ckv::golden::serialize(doc);
    std::ofstream out(dir / (name + ".dump"), std::ios::binary);
    out << text;
    std::fprintf(stderr, "wrote %s\n", (dir / (name + ".dump")).string().c_str());
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);

    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);

    app.step(0);
    write_dump(out_dir, "hello_initial", app);

    // Greeting is a typed non-blocking modal: one ordinary step paints it,
    // then a later event dismisses it through the standard cancellation path.
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    app.step(0);
    write_dump(out_dir, "hello_greeting", app);
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    app.step(0);

    // The one quit command as the File menu presents it: F10 takes the menu
    // bar to File and Enter drops it open; its last item is "Exit" with the
    // command's chord.
    term.inject_event(ckv::KeyEvent{KeyChord{Key::F10, Modifier::None, ""}});
    term.inject_event(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    app.step(0);
    write_dump(out_dir, "hello_file_menu", app);
    // Esc closes one level: the dropdown, then the bar walk.
    term.inject_event(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    term.inject_event(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    app.step(0);

    // A runtime rebind of that command, with nothing else changing: the next
    // frame's status line and the next opening of the File menu both say
    // the new chord.
    const ckv::ui::CommandId quit = *app.commands().id_for("hello.quit");
    app.commands().unbind_key(KeyChord{Key::Char, Modifier::Alt, "x"});
    app.commands().bind_key(KeyChord{Key::Char, Modifier::Ctrl, "q"}, quit);
    app.step(0);
    write_dump(out_dir, "hello_rebound", app);
    term.inject_event(ckv::KeyEvent{KeyChord{Key::F10, Modifier::None, ""}});
    term.inject_event(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    app.step(0);
    write_dump(out_dir, "hello_rebound_menu", app);
    term.inject_event(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    term.inject_event(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    app.step(0);

    return 0;
}
