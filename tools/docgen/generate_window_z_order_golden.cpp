// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for the overlapping-window frame-topology golden.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);

    ckv::ui::RoleRegistry registry;
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    const ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::widgets::Desktop desktop(ckv::Rect{0, 0, 26, 12});
    desktop.set_context(ckv::ui::Context{&theme, &registry, nullptr});

    ckv::widgets::Window* const background =
        desktop.add_window(std::make_unique<ckv::widgets::Window>("Back"));
    background->set_bounds(ckv::Rect{2, 2, 14, 4});
    ckv::widgets::Window* const foreground =
        desktop.add_window(std::make_unique<ckv::widgets::Window>("Front"));
    foreground->set_bounds(ckv::Rect{12, 3, 12, 7});

    ckv::scene::Surface surface(ckv::Size{26, 12});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 26, 12});
    desktop.draw(painter);
    desktop.paint_children(painter);

    std::ofstream output(directory / "window_z_order_junction.dump", std::ios::binary);
    output << ckv::golden::serialize(ckv::scene::capture(surface));
    return output ? 0 : 1;
}
