// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Captures the real LayoutsApp at representative sizes, including the
// application's normal resize, hard-floor degradation, and recovery paths.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/term/headless_terminal.hpp"
#include "frame_svg.hpp"
#include "layouts_app.hpp"

namespace {
void write_svg(const std::filesystem::path& dir, const std::string& name,
               const ckv::term::VirtualDisplay& display) {
    std::ofstream out(dir / (name + ".svg"));
    out << ckv::docgen::render_virtual_display_svg(display);
    std::fprintf(stderr, "wrote %s (%dx%d cells)\n", (dir / (name + ".svg")).string().c_str(),
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

    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    ckv::layouts::LayoutsApp layouts(app);

    app.step(0);
    write_svg(out_dir, "layouts-initial", term.display());
    term.resize(ckv::Size{100, 30});
    app.step(0);
    write_svg(out_dir, "layouts-wide", term.display());
    term.resize(ckv::Size{30, 10});
    app.step(0);
    write_svg(out_dir, "layouts-narrow", term.display());
    term.resize(ckv::Size{19, 5});
    app.step(0);
    write_svg(out_dir, "layouts-too-small", term.display());
    term.resize(ckv::Size{80, 24});
    app.step(0);
    write_svg(out_dir, "layouts-recovered", term.display());
    return 0;
}
