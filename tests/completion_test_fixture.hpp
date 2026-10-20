// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/input_line.hpp"

namespace ckv::testing {

// Shared deterministic scene for completion behavior and its golden generator.
struct CompletionFixture {
    term::HeadlessTerminal terminal{Size{40, 10}};
    ManualClock clock;
    ui::Application app{terminal, clock};
    ui::StandardRoles roles = ui::intern_standard_roles(app.roles());
    widgets::InputLine* field = nullptr;
    CompletionFixture() {
        app.theme() = ui::make_classic_theme(app.roles(), roles);
        auto owned = std::make_unique<widgets::InputLine>();
        field = owned.get();
        field->set_bounds(Rect{1, 1, 20, 1});
        app.root().add_child(std::move(owned));
        app.set_focus(field);
    }
};

}  // namespace ckv::testing
