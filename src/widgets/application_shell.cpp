// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/application_shell.hpp"

#include <memory>

#include "cvision/core/assert.hpp"

namespace ckv::widgets {

ApplicationShell::ApplicationShell(ui::Application& app, ApplicationShellOptions options)
    : app_(app) {
    app_.theme() = std::move(options.theme);
    desktop_ = app_.root().add(std::make_unique<Desktop>(app_.root().bounds()));
    CKV_ASSERT(desktop_ != nullptr);

    if (!options.menus.empty())
        menu_bar_ = desktop_->dock_top(std::make_unique<MenuBar>(std::move(options.menus)));

    if (!options.status_items.empty() || options.always_dock_status_line) {
        auto status = std::make_unique<StatusLine>();
        if (!options.status_items.empty()) status->set_items(std::move(options.status_items));
        status_line_ = desktop_->dock_bottom(std::move(status));
    }

    // Docked after the menu bar and the status line, so it sits inward of
    // whichever of them shares its edge.
    if (!options.tool_bar.empty()) {
        auto bar = std::make_unique<ToolBar>();
        bar->set_items(std::move(options.tool_bar));
        tool_bar_ = desktop_->dock(std::move(bar), options.tool_bar_edge);
    }
}

void ApplicationShell::detach_desktop() {
    if (desktop_ != nullptr && desktop_->parent() == &app_.root())
        (void)app_.root().detach_child(desktop_);
    desktop_ = nullptr;
    menu_bar_ = nullptr;
    tool_bar_ = nullptr;
    status_line_ = nullptr;
}

}  // namespace ckv::widgets
