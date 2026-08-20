// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/anchor_pane.hpp"

#include "cvision/core/assert.hpp"

namespace ckv::ui {

Rect apply_anchors(Rect current, Anchors anchors, int delta_width, int delta_height) noexcept {
    Rect result = current;
    if (anchors.left && anchors.right) {
        result.width += delta_width;
    } else if (anchors.right) {
        result.x += delta_width;
    }
    // left-only or neither anchored: x and width stay exactly as given
    // — a plain Rect placement with no anchors behaves precisely like
    // one anchored top-left, which is why Anchors{} needs no special
    // case above to match it.
    if (anchors.top && anchors.bottom) {
        result.height += delta_height;
    } else if (anchors.bottom) {
        result.y += delta_height;
    }
    return result;
}

View* AnchorPane::add_item(std::unique_ptr<View> child, Anchors anchors) {
    CKV_ASSERT(child != nullptr);
    View* observer = add_child(std::move(child));
    specs_[observer] = anchors;
    return observer;
}

std::unique_ptr<View> AnchorPane::remove_item(View* child) {
    std::unique_ptr<View> owned = remove_child(child);
    if (owned) specs_.erase(child);
    return owned;
}

void AnchorPane::on_resized() {
    const int delta_width = bounds().width - last_size_.width;
    const int delta_height = bounds().height - last_size_.height;
    for (const auto& child : children()) {
        auto it = specs_.find(child.get());
        const Anchors anchors = (it == specs_.end()) ? Anchors{} : it->second;
        child->set_bounds(apply_anchors(child->bounds(), anchors, delta_width, delta_height));
    }
    last_size_ = Size{bounds().width, bounds().height};
}

}  // namespace ckv::ui
