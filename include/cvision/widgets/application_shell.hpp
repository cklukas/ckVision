// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/command_presentation.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"

namespace ckv::widgets {

// What an ApplicationShell sets up; consumed (moved from) by its constructor.
struct ApplicationShellOptions {
    // Replaces the Application's theme wholesale.
    ui::Theme theme;
    // The menu bar's menus. Empty means no menu bar is docked.
    std::vector<MenuBarItem> menus;
    // The status line's items. Empty means no status line is docked, unless
    // always_dock_status_line asks for one anyway.
    std::vector<StatusLineItem> status_items;
    // Dock a StatusLine even when `status_items` is empty. The default rule —
    // items imply a bar — suits an application whose status line is a fixed
    // list written at construction. It cannot serve one whose items are
    // composed from live state (a context-sensitive hint bar that changes
    // with focus, say): such an application has nothing to hand over here,
    // yet needs the bar to exist so it can fill it on the first frame.
    // Passing a placeholder item purely to make the bar appear would leave
    // the shell holding contents the application then has to overwrite.
    bool always_dock_status_line = false;
    // The tool bar's buttons. Empty means no tool bar is docked.
    std::vector<CommandPresentation> tool_bar;
    // The edge the tool bar docks to: below the menu bar at the top (the
    // default), or above the status line at the bottom.
    DockEdge tool_bar_edge = DockEdge::Top;
};

// Declarative common-case chrome composition (WP-35). The shell attaches the
// ordinary root Desktop/menu/status arrangement but deliberately remains a
// helper, not a framework owner: Application still owns process state,
// root() owns the views, and callers still decide whether/how to run the loop.
// A controller that must remove this chrome before its Application ends may
// explicitly call detach_desktop().
class ApplicationShell {
public:
    // Installs `options.theme` on `app`, adds a Desktop covering the root's
    // current bounds to app.root(), and docks a MenuBar at its top, a
    // StatusLine at its bottom and a ToolBar inward of whichever of them is
    // on `options.tool_bar_edge`, when the options call for them. `app` is
    // borrowed and must outlive the shell. Destroying the shell leaves the
    // Desktop in place; only detach_desktop() removes it.
    ApplicationShell(ui::Application& app, ApplicationShellOptions options);

    // Detaches and destroys the Desktop this helper added to Application's
    // root. Idempotent. Call this when a controller ends before its borrowed
    // Application; afterwards the observer accessors are no longer usable.
    void detach_desktop();

    // The chrome the shell created, owned by the view tree. menu_bar(),
    // tool_bar() and status_line() are nullptr when none was docked or after
    // detach_desktop(); desktop() must not be called after detach_desktop().
    Desktop& desktop() noexcept { return *desktop_; }
    MenuBar* menu_bar() noexcept { return menu_bar_; }
    ToolBar* tool_bar() noexcept { return tool_bar_; }
    StatusLine* status_line() noexcept { return status_line_; }

private:
    ui::Application& app_;
    Desktop* desktop_ = nullptr;
    MenuBar* menu_bar_ = nullptr;
    ToolBar* tool_bar_ = nullptr;
    StatusLine* status_line_ = nullptr;
};

}  // namespace ckv::widgets
