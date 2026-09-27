// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for the M10 container suites, driven through the
// real Layouts example (examples/layouts). Input enters only through the
// HeadlessTerminal and Application::step: terminal resizes to 80x24, a wide
// 100x30 and a narrow 64x24, then back to 80x24 where F5 zooms the window
// and Right moves the Splitter. At each of the three sizes the region of
// every container family (the window's AnchorPane, Row, Column, Grid, Dock,
// Overlay) is written as its own dump, so a change in one family's layout
// shows up in that family's file alone. The keyboard state is pinned as one
// whole frame. tests/test_layouts_smoke.cpp runs the same script and compares
// each capture with these dumps.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/dock.hpp"
#include "cvision/ui/grid.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/overlay.hpp"
#include "cvision/widgets/window.hpp"
#include "layouts_app.hpp"

namespace {

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    output << text;
}

// The composed frame's cells inside `area` (absolute), as a dump of its own.
std::string capture_region(const ckv::ui::Application& app, ckv::Rect area) {
    const ckv::scene::Surface& frame = app.composed_surface();
    ckv::scene::Surface region(ckv::Size{area.width, area.height});
    for (int y = 0; y < area.height; ++y)
        for (int x = 0; x < area.width; ++x)
            region.set_cell(ckv::Point{x, y}, frame.at(ckv::Point{area.x + x, area.y + y}),
                            frame.link_target(ckv::Point{area.x + x, area.y + y}));
    return ckv::golden::serialize(ckv::scene::capture(region));
}

void write_families(const std::filesystem::path& directory, const char* stage, const ckv::ui::Application& app,
                    const ckv::layouts::LayoutsApp& layouts) {
    const struct {
        const char* family;
        ckv::Rect area;
    } families[] = {
        {"anchor", layouts.window()->absolute_bounds()},
        {"row", layouts.row()->absolute_bounds()},
        {"column", layouts.column()->absolute_bounds()},
        {"grid", layouts.grid()->absolute_bounds()},
        {"dock", layouts.dock()->absolute_bounds()},
        {"overlay", layouts.overlay()->absolute_bounds()},
    };
    for (const auto& entry : families)
        write_text(directory / ("layouts_" + std::string(entry.family) + "_" + stage + ".dump"),
                   capture_region(app, entry.area));
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

    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::layouts::LayoutsApp layouts(app);

    app.step(0);
    write_families(directory, "initial", app, layouts);
    terminal.resize(ckv::Size{100, 30});
    app.step(0);
    write_families(directory, "wide", app, layouts);
    terminal.resize(ckv::Size{64, 24});
    app.step(0);
    write_families(directory, "narrow", app, layouts);

    terminal.resize(ckv::Size{80, 24});
    app.step(0);
    press(terminal, app, ckv::Key::F5);
    press(terminal, app, ckv::Key::Right);
    press(terminal, app, ckv::Key::Right);
    write_text(directory / "layouts_keyboard.dump",
               ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor())));
    return 0;
}
