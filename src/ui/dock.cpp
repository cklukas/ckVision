// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/dock.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"
#include "cvision/ui/layout_metrics.hpp"

namespace ckv::ui {

DockRects compute_dock_layout(Rect available, DockEdgeExtents edges) noexcept {
    Rect remaining = available;
    DockRects rects{};

    if (edges.top) {
        const int height = std::clamp(*edges.top, 0, remaining.height);
        rects.top = Rect{remaining.x, remaining.y, remaining.width, height};
        remaining.y += height;
        remaining.height -= height;
    }
    if (edges.bottom) {
        const int height = std::clamp(*edges.bottom, 0, remaining.height);
        rects.bottom =
            Rect{remaining.x, remaining.y + remaining.height - height, remaining.width, height};
        remaining.height -= height;
    }
    if (edges.left) {
        const int width = std::clamp(*edges.left, 0, remaining.width);
        rects.left = Rect{remaining.x, remaining.y, width, remaining.height};
        remaining.x += width;
        remaining.width -= width;
    }
    if (edges.right) {
        const int width = std::clamp(*edges.right, 0, remaining.width);
        rects.right =
            Rect{remaining.x + remaining.width - width, remaining.y, width, remaining.height};
        remaining.width -= width;
    }
    rects.center = remaining;
    return rects;
}

View* Dock::add_item(std::unique_ptr<View> child, DockEdge edge) {
    CKV_ASSERT(child != nullptr);
    for (const auto& [existing_child, existing_edge] : specs_) {
        (void)existing_child;
        CKV_ASSERT(existing_edge != edge);
    }

    View* observer = add_child(std::move(child));
    specs_[observer] = edge;
    relayout();
    return observer;
}

std::unique_ptr<View> Dock::remove_item(View* child) {
    std::unique_ptr<View> owned = remove_child(child);
    if (owned) {
        specs_.erase(child);
        relayout();
    }
    return owned;
}

void Dock::relayout() {
    View* top = nullptr;
    View* bottom = nullptr;
    View* left = nullptr;
    View* right = nullptr;
    View* center = nullptr;
    for (const auto& child : children()) {
        auto it = specs_.find(child.get());
        if (it == specs_.end()) continue;
        switch (it->second) {
            case DockEdge::Top: top = child.get(); break;
            case DockEdge::Bottom: bottom = child.get(); break;
            case DockEdge::Left: left = child.get(); break;
            case DockEdge::Right: right = child.get(); break;
            case DockEdge::Center: center = child.get(); break;
        }
    }

    DockEdgeExtents edges;
    if (top != nullptr) edges.top = detail::preferred_height_for_width(*top, bounds().width);
    if (bottom != nullptr) edges.bottom = detail::preferred_height_for_width(*bottom, bounds().width);
    if (left != nullptr) edges.left = left->horizontal_size_hint().preferred;
    if (right != nullptr) edges.right = right->horizontal_size_hint().preferred;

    const DockRects rects = compute_dock_layout(Rect{0, 0, bounds().width, bounds().height}, edges);
    if (top != nullptr) top->set_bounds(rects.top);
    if (bottom != nullptr) bottom->set_bounds(rects.bottom);
    if (left != nullptr) left->set_bounds(rects.left);
    if (right != nullptr) right->set_bounds(rects.right);
    if (center != nullptr) center->set_bounds(rects.center);
}

}  // namespace ckv::ui
