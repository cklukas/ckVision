// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Captures every Workbench tab.  Tab changes use TabControl's public API so
// each documented page is still the app's production object graph.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "frame_svg.hpp"
#include "workbench_app.hpp"

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
    ckv::workbench::WorkbenchApp workbench(app);
    app.step(0);
    write_svg(out_dir, "workbench-text", term.display());
    workbench.tabs()->set_active_index(1);
    app.step(0);
    write_svg(out_dir, "workbench-data", term.display());
    workbench.tabs()->set_active_index(2);
    app.step(0);
    write_svg(out_dir, "workbench-help", term.display());
    return 0;
}
