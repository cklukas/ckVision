// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for WP-24's HeadlessTerminal resize script.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

namespace {

void write_dump(const std::filesystem::path& directory, const char* name, const ckv::ui::Application& app) {
    std::ofstream output(directory / name);
    output << ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
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
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto desktop = std::make_unique<ckv::widgets::Desktop>(app.root().bounds());
    ckv::widgets::Desktop* const desktop_ptr = desktop.get();
    app.root().add_child(std::move(desktop));
    desktop_ptr->dock_top(std::make_unique<ckv::ui::View>());
    desktop_ptr->dock_bottom(std::make_unique<ckv::ui::View>());

    auto ordinary = std::make_unique<ckv::widgets::Window>("Ordinary");
    ordinary->set_bounds(ckv::Rect{60, 15, 18, 7});
    desktop_ptr->add_window(std::move(ordinary));
    auto zoomed = std::make_unique<ckv::widgets::Window>("Zoomed");
    zoomed->set_bounds(ckv::Rect{15, 4, 28, 12});
    ckv::widgets::Window* const zoomed_ptr = desktop_ptr->add_window(std::move(zoomed));
    zoomed_ptr->toggle_zoom(desktop_ptr->content_area());
    auto filling = std::make_unique<ckv::widgets::Window>("Keep filling");
    filling->set_bounds(ckv::Rect{2, 2, 16, 6});
    ckv::widgets::Window* const filling_ptr = desktop_ptr->add_window(std::move(filling));
    filling_ptr->set_grow_policy(ckv::widgets::DesktopGrowPolicy::KeepFilling);

    terminal.resize(ckv::Size{120, 40});
    app.step(0);
    write_dump(directory, "root_resize_grow.dump", app);
    terminal.resize(ckv::Size{40, 10});
    app.step(0);
    write_dump(directory, "root_resize_shrink.dump", app);
    return 0;
}
