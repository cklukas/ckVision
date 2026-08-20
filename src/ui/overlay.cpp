// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/overlay.hpp"

#include "cvision/core/assert.hpp"

namespace ckv::ui {

View* Overlay::add_item(std::unique_ptr<View> child, OverlayMode mode) {
    CKV_ASSERT(child != nullptr);
    View* observer = add_child(std::move(child));
    specs_[observer] = mode;
    relayout();
    return observer;
}

std::unique_ptr<View> Overlay::remove_item(View* child) {
    std::unique_ptr<View> owned = remove_child(child);
    if (owned) specs_.erase(child);
    return owned;
}

void Overlay::relayout() {
    for (const auto& child : children()) {
        auto it = specs_.find(child.get());
        const OverlayMode mode = (it == specs_.end()) ? OverlayMode::Fill : it->second;
        if (mode == OverlayMode::Fill) {
            child->set_bounds(Rect{0, 0, bounds().width, bounds().height});
        }
    }
}

}  // namespace ckv::ui
