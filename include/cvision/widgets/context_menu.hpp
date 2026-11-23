// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <functional>
#include <vector>

#include "cvision/widgets/menu.hpp"

namespace ckv::ui {
class Application;
}

namespace ckv::widgets {

class Desktop;

// A standalone popup context menu anchored at a screen position.
class CVISION_API ContextMenu final : public ui::View {
public:
    // Construct a ContextMenu with `items` and an optional dismiss callback.
    explicit ContextMenu(std::vector<MenuItem> items = {});

    // Replace the menu items.
    void set_items(std::vector<MenuItem> items);
    // Return the current menu items.
    const std::vector<MenuItem>& items() const noexcept { return items_; }
    // Open the context menu anchored at `anchor` inside `desktop`.
    void show(ui::Application& app, Desktop& desktop, Point anchor);
    // Dismiss the open menu if currently shown.
    void dismiss();
    // Return true if the popup menu is currently open.
    bool is_open() const noexcept { return popup_ != nullptr && !popup_token_.expired(); }

    // Invoked when the popup menu is dismissed without choosing or after choosing.
    std::function<void()> on_dismiss;

private:
    std::vector<MenuItem> items_;
    DropdownMenu* popup_ = nullptr;
    std::weak_ptr<void> popup_token_;
};

// Convenience helper to show a one-shot ContextMenu at `anchor` on `desktop`.
CVISION_API DropdownMenu* show_context_menu(ui::Application& app, Desktop& desktop, Point anchor,
                                            std::vector<MenuItem> items,
                                            std::function<void()> on_dismiss = {});

}  // namespace ckv::widgets
