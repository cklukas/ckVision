// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for WP-25's overlapping-window activation script.
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

    ckv::term::HeadlessTerminal terminal(ckv::Size{60, 20});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto desktop = std::make_unique<ckv::widgets::Desktop>(app.root().bounds());
    ckv::widgets::Desktop* const desktop_ptr = desktop.get();
    app.root().add_child(std::move(desktop));

    auto back = std::make_unique<ckv::widgets::Window>("Back");
    back->set_bounds(ckv::Rect{3, 3, 36, 12});
    back->set_content(std::make_unique<ckv::widgets::InputLine>());
    desktop_ptr->add_window(std::move(back));
    auto front = std::make_unique<ckv::widgets::Window>("Front");
    front->set_bounds(ckv::Rect{15, 5, 36, 12});
    front->set_content(std::make_unique<ckv::widgets::InputLine>());
    desktop_ptr->add_window(std::move(front));

    app.step(0);
    write_dump(directory, "window_activation_before.dump", app);
    terminal.inject_event(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 6},
                                          std::nullopt, ckv::Modifier::None});
    app.step(0);
    write_dump(directory, "window_activation_after.dump", app);
    return 0;
}
