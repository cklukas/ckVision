// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Captures the shipped editor example through its ordinary headless
// application path. The SVG is therefore evidence of the same object graph
// built by examples/editor/main.cpp, rather than an illustrative mockup.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "cvision/term/headless_terminal.hpp"
#include "editor_app.hpp"
#include "frame_svg.hpp"

namespace {
void write_svg(const std::filesystem::path& directory, const std::string& name,
               const ckv::term::VirtualDisplay& display) {
    std::ofstream out(directory / (name + ".svg"), std::ios::binary);
    out << ckv::docgen::render_virtual_display_svg(display);
    std::fprintf(stderr, "wrote %s (%dx%d cells)\n", (directory / (name + ".svg")).string().c_str(),
                 display.size().width, display.size().height);
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);

    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::editor_example::EditorApp editor(app);
    app.step(0);
    write_svg(out_dir, "editor-initial", terminal.display());

    (void)editor.open_sample("settings.json");
    app.step(0);
    write_svg(out_dir, "editor-json", terminal.display());

    (void)editor.open_sample("config.yaml");
    app.step(0);

    for (int index = 0; index < 4; ++index)
        app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::Shift, ""}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "f"}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F3, ckv::Modifier::None, ""}});
    app.step(0);
    write_svg(out_dir, "editor-search", terminal.display());

    ckv::term::HeadlessTerminal close_terminal(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock close_clock;
    ckv::ui::Application close_app(close_terminal, close_clock);
    ckv::editor_example::EditorApp close_editor(close_app);
    close_app.step(0);
    close_app.dispatch(ckv::TextEvent{"#", false});
    (void)close_editor.window()->close();
    close_app.step(0);
    write_svg(out_dir, "editor-close-confirm", close_terminal.display());

    // Search > Replace... (Ctrl+R) with "name" selected, a replacement typed,
    // and the first press of Replace having found the match it will change.
    ckv::term::HeadlessTerminal replace_terminal(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock replace_clock;
    ckv::ui::Application replace_app(replace_terminal, replace_clock);
    ckv::editor_example::EditorApp replace_editor(replace_app);
    replace_app.step(0);
    const auto press = [&replace_terminal, &replace_app](ckv::Key key, ckv::Modifier modifiers, std::string text) {
        replace_terminal.inject_event(ckv::KeyEvent{ckv::KeyChord{key, modifiers, std::move(text)}});
        replace_app.step(0);
    };
    for (int index = 0; index < 4; ++index) press(ckv::Key::Right, ckv::Modifier::Shift, "");
    press(ckv::Key::Char, ckv::Modifier::Ctrl, "r");
    press(ckv::Key::Tab, ckv::Modifier::None, "");
    replace_terminal.inject_bytes("title", 0);
    replace_app.step(0);
    press(ckv::Key::Enter, ckv::Modifier::None, "");
    write_svg(out_dir, "editor-replace", replace_terminal.display());
    return 0;
}
