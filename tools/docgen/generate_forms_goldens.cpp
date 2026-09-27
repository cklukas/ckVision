// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Fixture generator for the Forms focus script (VISION 3 and 6): the frame
// as the example opens, with the Name field focused and its status hint, and
// the frame after Tab has walked to the Presentation mode group and an arrow
// has moved within it, with the group's own hint. tests/test_forms_smoke.cpp
// replays the same keys; generated_golden_bytes regenerates these on every
// host.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "forms_app.hpp"

namespace {

void write_dump(const std::filesystem::path& directory, const std::string& name, const ckv::ui::Application& app) {
    const std::filesystem::path path = directory / (name + ".dump");
    std::ofstream output(path, std::ios::binary);
    output << ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    std::fprintf(stderr, "wrote %s\n", path.string().c_str());
}

void press(ckv::term::HeadlessTerminal& terminal, ckv::ui::Application& app, ckv::Key key) {
    terminal.inject_event(ckv::KeyEvent{ckv::KeyChord{key, ckv::Modifier::None, ""}});
    app.step(0);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);

    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::forms::FormsApp forms(app);
    app.step(0);
    write_dump(directory, "forms_focus_name", app);

    press(terminal, app, ckv::Key::Tab);   // the Options group
    press(terminal, app, ckv::Key::Tab);   // the Presentation mode group
    press(terminal, app, ckv::Key::Down);  // Modeless, and the focus stays
    write_dump(directory, "forms_focus_group", app);
    return 0;
}
