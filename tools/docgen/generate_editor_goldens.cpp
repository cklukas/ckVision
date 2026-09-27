// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Deliberately explicit golden regeneration for the shipped editor example.
// Run this tool manually after an intentional visual-contract change, review
// the resulting dump, and commit it with the behavior change. It also plays
// the example's keyboard-only and mouse-only menu scripts
// (editor_menu_script.hpp) and writes the frames they pin
// (editor_menu_*.dump), the same scripts tests/test_editor_menu_scripts.cpp
// plays.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "editor_app.hpp"
#include "editor_menu_script.hpp"
#include "event_script.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path output_directory = argv[1];
    std::filesystem::create_directories(output_directory);

    const auto write = [&output_directory](std::string_view name, ckv::ui::Application& app) {
        const std::filesystem::path output = output_directory / (std::string(name) + ".dump");
        std::ofstream out(output, std::ios::binary);
        out << ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
        std::fprintf(stderr, "wrote %s\n", output.string().c_str());
        return static_cast<bool>(out);
    };
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::editor_example::EditorApp editor(app);
    app.step(0);
    bool complete = write("editor_initial", app);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.set_theme(ckv::ui::make_dark_theme(app.roles(), roles));
    app.step(0);
    complete = write("editor_initial_dark", app) && complete;
    app.set_theme(ckv::ui::make_light_theme(app.roles(), roles));
    app.step(0);
    complete = write("editor_initial_light", app) && complete;
    app.set_theme(ckv::ui::make_mono_theme(app.roles(), roles));
    app.step(0);
    complete = write("editor_initial_mono", app) && complete;
    app.set_theme(ckv::ui::make_classic_theme(app.roles(), roles));
    app.step(0);
    for (int index = 0; index < 4; ++index)
        app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::Shift, ""}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "f"}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F3, ckv::Modifier::None, ""}});
    app.step(0);
    complete = write("editor_search", app) && complete;
    app.set_theme(ckv::ui::make_dark_theme(app.roles(), roles));
    app.step(0);
    complete = write("editor_search_dark", app) && complete;
    app.set_theme(ckv::ui::make_light_theme(app.roles(), roles));
    app.step(0);
    complete = write("editor_search_light", app) && complete;
    app.set_theme(ckv::ui::make_mono_theme(app.roles(), roles));
    app.step(0);
    complete = write("editor_search_mono", app) && complete;

    ckv::term::HeadlessTerminal close_terminal(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock close_clock;
    ckv::ui::Application close_app(close_terminal, close_clock);
    ckv::editor_example::EditorApp close_editor(close_app);
    close_app.step(0);
    close_app.dispatch(ckv::TextEvent{"#", false});
    (void)close_editor.window()->close();
    close_app.step(0);
    complete = write("editor_close_confirm", close_app) && complete;
    const ckv::ui::StandardRoles close_roles = ckv::ui::intern_standard_roles(close_app.roles());
    close_app.set_theme(ckv::ui::make_dark_theme(close_app.roles(), close_roles));
    close_app.step(0);
    complete = write("editor_close_confirm_dark", close_app) && complete;
    close_app.set_theme(ckv::ui::make_light_theme(close_app.roles(), close_roles));
    close_app.step(0);
    complete = write("editor_close_confirm_light", close_app) && complete;
    close_app.set_theme(ckv::ui::make_mono_theme(close_app.roles(), close_roles));
    close_app.step(0);
    complete = write("editor_close_confirm_mono", close_app) && complete;

    // The Replace dialog after its first press of Replace has found the
    // selected word, reached through the terminal the way the smoke test's
    // script reaches it.
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
    complete = write("editor_replace", replace_app) && complete;
    const ckv::ui::StandardRoles replace_roles = ckv::ui::intern_standard_roles(replace_app.roles());
    replace_app.set_theme(ckv::ui::make_dark_theme(replace_app.roles(), replace_roles));
    replace_app.step(0);
    complete = write("editor_replace_dark", replace_app) && complete;
    replace_app.set_theme(ckv::ui::make_light_theme(replace_app.roles(), replace_roles));
    replace_app.step(0);
    complete = write("editor_replace_light", replace_app) && complete;
    replace_app.set_theme(ckv::ui::make_mono_theme(replace_app.roles(), replace_roles));
    replace_app.step(0);
    complete = write("editor_replace_mono", replace_app) && complete;

    ckv::docgen::EditorMenuStage keyboard(ckv::docgen::editor_menu_keyboard_script());
    ckv::docgen::EditorMenuStage mouse(ckv::docgen::editor_menu_mouse_script());
    complete = ckv::docgen::write_script_goldens(keyboard.player, keyboard.app, output_directory) > 0 && complete;
    complete = ckv::docgen::write_script_goldens(mouse.player, mouse.app, output_directory) > 0 && complete;
    return complete ? 0 : 1;
}
