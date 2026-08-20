// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"

namespace ckv::widgets {

struct ApplicationShellOptions {
    ui::Theme theme;
    std::vector<MenuBarItem> menus;
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
};

// Declarative common-case chrome composition (WP-35). The shell attaches the
// ordinary root Desktop/menu/status arrangement but deliberately remains a
// helper, not a framework owner: Application still owns process state,
// root() owns the views, and callers still decide whether/how to run the loop.
class ApplicationShell {
public:
    ApplicationShell(ui::Application& app, ApplicationShellOptions options);

    Desktop& desktop() noexcept { return *desktop_; }
    MenuBar* menu_bar() noexcept { return menu_bar_; }
    StatusLine* status_line() noexcept { return status_line_; }

private:
    ui::Application& app_;
    Desktop* desktop_ = nullptr;
    MenuBar* menu_bar_ = nullptr;
    StatusLine* status_line_ = nullptr;
};

}  // namespace ckv::widgets
