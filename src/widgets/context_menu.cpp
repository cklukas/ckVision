// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/context_menu.hpp"

#include "cvision/ui/application.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {

ContextMenu::ContextMenu(std::vector<MenuItem> items) : items_(std::move(items)) {}

void ContextMenu::set_items(std::vector<MenuItem> items) { items_ = std::move(items); }

void ContextMenu::show(ui::Application& app, Desktop& desktop, Point anchor) {
    dismiss();
    if (items_.empty()) return;
    popup_ = show_context_menu(items_, anchor, app, desktop);
    if (popup_ == nullptr) return;
    popup_token_ = popup_->lifetime_token();
    const std::weak_ptr<void> self_token = lifetime_token();
    auto base_dismiss = std::move(popup_->on_dismiss);
    popup_->on_dismiss = [this, self_token, base = std::move(base_dismiss)](MenuDismissReason reason) mutable {
        if (base) base(reason);
        if (!self_token.expired()) {
            popup_ = nullptr;
            popup_token_.reset();
            const auto cb = on_dismiss;
            if (cb) cb();
        }
    };
}

void ContextMenu::dismiss() {
    if (popup_ == nullptr) return;
    if (!popup_token_.expired()) {
        if (popup_->on_dismiss) {
            auto handler = std::move(popup_->on_dismiss);
            handler(MenuDismissReason::Cancelled);
            return;
        }
        if (auto* parent = dynamic_cast<Desktop*>(popup_->parent())) {
            parent->remove_popup(popup_);
        }
    }
    popup_ = nullptr;
    popup_token_.reset();
    const auto cb = on_dismiss;
    if (cb) cb();
}

DropdownMenu* show_context_menu(ui::Application& app, Desktop& desktop, Point anchor,
                                std::vector<MenuItem> items, std::function<void()> on_dismiss) {
    DropdownMenu* attached = show_context_menu(std::move(items), anchor, app, desktop);
    if (attached == nullptr) return nullptr;
    if (on_dismiss) {
        auto base_dismiss = std::move(attached->on_dismiss);
        attached->on_dismiss = [base = std::move(base_dismiss), cb = std::move(on_dismiss)](MenuDismissReason reason) mutable {
            if (base) base(reason);
            if (cb) cb();
        };
    }
    return attached;
}

}  // namespace ckv::widgets
