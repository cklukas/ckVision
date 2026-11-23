// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/dock.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"
#include "ui/layout_metrics.hpp"

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
    size_hint_changed();
    return observer;
}

std::unique_ptr<View> Dock::remove_child(View* child) {
    std::unique_ptr<View> owned = View::remove_child(child);
    if (owned == nullptr) return nullptr;
    specs_.erase(child);
    relayout();
    size_hint_changed();
    return owned;
}

void Dock::set_spacing(int spacing) {
    CKV_ASSERT(spacing >= 0);
    if (spacing == spacing_) return;
    spacing_ = spacing;
    relayout();
    size_hint_changed();
}

void Dock::set_padding(int horizontal, int vertical) {
    CKV_ASSERT(horizontal >= 0 && vertical >= 0);
    if (horizontal == padding_horizontal_ && vertical == padding_vertical_) return;
    padding_horizontal_ = horizontal;
    padding_vertical_ = vertical;
    relayout();
    size_hint_changed();
}

SizeHint Dock::horizontal_size_hint() const {
    if (preferred_size().width > 0) return View::horizontal_size_hint();
    int width = 2 * padding_horizontal_;
    int side_count = 0;
    int center_w = 0;
    int top_bottom_w = 0;
    for (View* child : detail::visible_children(children())) {
        auto it = specs_.find(child);
        if (it == specs_.end()) continue;
        const int pref = child->horizontal_size_hint().preferred;
        switch (it->second) {
            case DockEdge::Left:
            case DockEdge::Right:
                width += pref;
                ++side_count;
                break;
            case DockEdge::Center:
                center_w = std::max(center_w, pref);
                break;
            case DockEdge::Top:
            case DockEdge::Bottom:
                top_bottom_w = std::max(top_bottom_w, pref + 2 * padding_horizontal_);
                break;
        }
    }
    width += center_w + side_count * spacing_;
    const int best = std::max(width, top_bottom_w);
    return SizeHint{best, best, kUnboundedExtent};
}

SizeHint Dock::vertical_size_hint() const {
    if (preferred_size().height > 0) return View::vertical_size_hint();
    int height = 2 * padding_vertical_;
    int edge_count = 0;
    int center_h = 0;
    for (View* child : detail::visible_children(children())) {
        auto it = specs_.find(child);
        if (it == specs_.end()) continue;
        const int pref = child->vertical_size_hint().preferred;
        switch (it->second) {
            case DockEdge::Top:
            case DockEdge::Bottom:
                height += pref;
                ++edge_count;
                break;
            case DockEdge::Left:
            case DockEdge::Right:
            case DockEdge::Center:
                center_h = std::max(center_h, pref);
                break;
        }
    }
    height += center_h + edge_count * spacing_;
    return SizeHint{height, height, kUnboundedExtent};
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

    const int inner_w = std::max(0, bounds().width - 2 * padding_horizontal_);
    const int inner_h = std::max(0, bounds().height - 2 * padding_vertical_);
    DockEdgeExtents edges;
    if (top != nullptr) edges.top = detail::preferred_height_for_width(*top, inner_w);
    if (bottom != nullptr) edges.bottom = detail::preferred_height_for_width(*bottom, inner_w);
    if (left != nullptr) edges.left = left->horizontal_size_hint().preferred;
    if (right != nullptr) edges.right = right->horizontal_size_hint().preferred;

    const DockRects rects =
        compute_dock_layout(Rect{padding_horizontal_, padding_vertical_, inner_w, inner_h}, edges);
    Rect top_r = rects.top;
    Rect bottom_r = rects.bottom;
    Rect left_r = rects.left;
    Rect right_r = rects.right;
    if (center == nullptr && spacing_ > 0) {
        if (top != nullptr && bottom != nullptr && bottom_r.height > 0) {
            const int d = std::min(spacing_, bottom_r.height);
            bottom_r.y += d;
            bottom_r.height -= d;
        }
        if (left != nullptr && right != nullptr && right_r.width > 0) {
            const int d = std::min(spacing_, right_r.width);
            right_r.x += d;
            right_r.width -= d;
        }
    }
    if (top != nullptr) top->set_bounds(top_r);
    if (bottom != nullptr) bottom->set_bounds(bottom_r);
    if (left != nullptr) left->set_bounds(left_r);
    if (right != nullptr) right->set_bounds(right_r);
    if (center != nullptr) {
        Rect c = rects.center;
        if (spacing_ > 0) {
            if (top != nullptr && c.height > 0) {
                const int d = std::min(spacing_, c.height);
                c.y += d;
                c.height -= d;
            }
            if (bottom != nullptr && c.height > 0) {
                c.height -= std::min(spacing_, c.height);
            }
            if (left != nullptr && c.width > 0) {
                const int d = std::min(spacing_, c.width);
                c.x += d;
                c.width -= d;
            }
            if (right != nullptr && c.width > 0) {
                c.width -= std::min(spacing_, c.width);
            }
        }
        center->set_bounds(c);
    }
}

}  // namespace ckv::ui
