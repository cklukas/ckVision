// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Deliberately explicit golden regeneration for the shipped editor example.
// Run this tool manually after an intentional visual-contract change, review
// the resulting dump, and commit it with the behavior change.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "editor_app.hpp"

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
    for (int index = 0; index < 4; ++index)
        app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::Shift, ""}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "f"}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F3, ckv::Modifier::None, ""}});
    app.step(0);
    complete = write("editor_search", app) && complete;
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_dark_theme(app.roles(), roles);
    app.root().invalidate();
    app.step(0);
    complete = write("editor_search_dark", app) && complete;
    app.theme() = ckv::ui::make_light_theme(app.roles(), roles);
    app.root().invalidate();
    app.step(0);
    complete = write("editor_search_light", app) && complete;
    app.theme() = ckv::ui::make_mono_theme(app.roles(), roles);
    app.root().invalidate();
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
    return complete ? 0 : 1;
}
