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

    if (menu_bar_ != nullptr && status_line_ != nullptr) {
        StatusLine* const status = status_line_;
        ui::Application* const app_ptr = &app_;
        menu_bar_->on_highlight_changed = [status, app_ptr](const MenuHighlight& highlight) {
            if (highlight.none) {
                status->set_transient_hint("");
                return;
            }
            if (!highlight.enabled && !highlight.disabled_reason.empty()) {
                status->set_transient_hint("Unavailable: " + highlight.disabled_reason);
                return;
            }
            if (!highlight.help_context.empty()) {
                status->set_transient_hint(highlight.help_context);
                return;
            }
            if (highlight.command != ui::kInvalidCommand) {
                if (const ui::CommandInfo* info = app_ptr->commands().find(highlight.command)) {
                    const std::string title = parse_mnemonic(info->title).display;
                    const std::string chord = app_ptr->commands().chord_text(highlight.command);
                    status->set_transient_hint(chord.empty() ? title : title + " (" + chord + ")");
                    return;
                }
            }
            status->set_transient_hint("");
        };
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
