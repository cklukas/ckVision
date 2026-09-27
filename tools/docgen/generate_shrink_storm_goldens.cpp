// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for WP-21/WP-36's shrink storm: one KeepFilling
// window with a focused InputLine, driven through HeadlessTerminal::resize and
// Application::step from full chrome (80x24) through the degraded size (30x8)
// and below the hard floor (8x4) back to 80x24. tests/test_wp36.cpp runs the
// same script and compares each composed frame with these dumps.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/window.hpp"

namespace {

void write_dump(const std::filesystem::path& directory, const char* name, const ckv::ui::Application& app) {
    std::ofstream output(directory / name, std::ios::binary);
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

    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto desktop = std::make_unique<ckv::widgets::Desktop>(app.root().bounds());
    ckv::widgets::Desktop* const desktop_ptr = desktop.get();
    app.root().add_child(std::move(desktop));

    auto window = std::make_unique<ckv::widgets::Window>("Shrink");
    window->set_bounds(desktop_ptr->content_area());
    window->set_grow_policy(ckv::widgets::DesktopGrowPolicy::KeepFilling);
    auto input = std::make_unique<ckv::widgets::InputLine>();
    ckv::widgets::InputLine* const input_ptr = input.get();
    window->set_content(std::move(input));
    desktop_ptr->add_window(std::move(window));
    app.set_focus(input_ptr);

    struct Step {
        ckv::Size size;
        const char* name;
    };
    for (const Step step : {Step{ckv::Size{80, 24}, "shrink_storm_full.dump"},
                            Step{ckv::Size{30, 8}, "shrink_storm_degraded.dump"},
                            Step{ckv::Size{8, 4}, "shrink_storm_too_small.dump"},
                            Step{ckv::Size{80, 24}, "shrink_storm_recovered.dump"}}) {
        terminal.resize(step.size);
        app.step(0);
        write_dump(directory, step.name, app);
    }
    return 0;
}
