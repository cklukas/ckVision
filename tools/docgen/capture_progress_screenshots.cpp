// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "frame_svg.hpp"
#include "progress_lab_app.hpp"
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path directory(argv[1]);
    std::filesystem::create_directories(directory);
    ckv::ManualClock clock;
    ckv::term::HeadlessTerminal terminal({100, 32});
    ckv::ui::Application app(terminal, clock);
    ckv::progress_lab::ProgressLabApp lab(app);
    const auto capture = [&](const char* name) {
        app.step(0);
        std::ofstream output(directory / (std::string(name) + ".svg"), std::ios::binary);
        output << ckv::docgen::render_virtual_display_svg(terminal.display());
        if (!output) throw std::runtime_error("Cannot write progress figure");
    };
    capture("progress-lab");
    auto* state = lab.selector("State"); app.set_focus(state);
    capture("progress-lab-states");
    return 0;
}
