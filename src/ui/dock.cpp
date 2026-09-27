// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/dock.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"
#include "cvision/ui/layout_metrics.hpp"

namespace ckv::ui {

DockRects compute_dock_layout(Rect available, DockEdgeExtents edges) noexcept {
    // AnchorPane can retain a negative extent while a stretched child is
    // squeezed past zero, so a later grow restores its original geometry.
    // Dock lays out the visible area of that child: an empty axis has zero
    // available cells, never a negative upper bound for std::clamp.
    Rect remaining{available.x, available.y, std::max(0, available.width),
                   std::max(0, available.height)};
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
    // An attachment callback that detached the child, or destroyed this
    // container, leaves nothing to place and no record to keep.
    if (observer == nullptr) return nullptr;
    specs_[observer] = edge;
    relayout();
    return observer;
}

std::unique_ptr<View> Dock::remove_child(View* child) {
    std::unique_ptr<View> owned = View::remove_child(child);
    if (owned == nullptr) return nullptr;
    specs_.erase(child);
    relayout();
    return owned;
}

void Dock::relayout() {
    View* top = nullptr;
    View* bottom = nullptr;
    View* left = nullptr;
    View* right = nullptr;
    View* center = nullptr;
    // A hidden edge is laid out as an absent one: a strip kept for a view
    // that is not drawn is a blank band the reader cannot account for.
    for (View* child : detail::visible_children(children())) {
        auto it = specs_.find(child);
        if (it == specs_.end()) continue;
        switch (it->second) {
            case DockEdge::Top: top = child; break;
            case DockEdge::Bottom: bottom = child; break;
            case DockEdge::Left: left = child; break;
            case DockEdge::Right: right = child; break;
            case DockEdge::Center: center = child; break;
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
