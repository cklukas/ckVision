// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Captures both GraphicsApp tabs under Sixel and NoGraphics capabilities.  The
// VirtualDisplay receives Presenter bytes; no capture code reads Image pixels.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "frame_svg.hpp"
#include "graphics_app.hpp"

namespace {
void write_svg(const std::filesystem::path& dir, const std::string& name,
               const ckv::term::VirtualDisplay& display) {
    std::ofstream out(dir / (name + ".svg"));
    out << ckv::docgen::render_virtual_display_svg(display);
    std::fprintf(stderr, "wrote %s (%dx%d cells, raster=%s)\n", (dir / (name + ".svg")).string().c_str(),
                 display.size().width, display.size().height,
                 display.has_raster_pixels() ? "yes" : "no");
}

void capture_profile(const std::filesystem::path& dir, const std::string& prefix,
                     ckv::term::Capabilities capabilities) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, capabilities);
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    ckv::graphics::GraphicsApp graphics(app);
    app.step(0);
    write_svg(dir, prefix + "-image", term.display());
    graphics.tabs()->set_active_index(1);
    app.step(0);
    write_svg(dir, prefix + "-canvas", term.display());
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);
    capture_profile(out_dir, "graphics-sixel", ckv::term::headless_sixel_profile());
    capture_profile(out_dir, "graphics-no-graphics", ckv::term::headless_no_graphics_profile());
    return 0;
}
