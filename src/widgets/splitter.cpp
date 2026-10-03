// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/splitter.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {

Splitter::Splitter(Rect bounds, std::unique_ptr<View> first, std::unique_ptr<View> second,
                    Orientation orientation)
    : View(bounds), orientation_(orientation) {
    CKV_ASSERT(first != nullptr);
    CKV_ASSERT(second != nullptr);
    first_ = add_child(std::move(first));
    second_ = add_child(std::move(second));
    set_focus_policy(ui::FocusPolicy::TabStop);

    const int usable = std::max(0, main_extent() - divider_extent());
    split_position_ = clamp_split(usable / 2);
    anchored_extent_ = split_position_;
    relayout();
}

void Splitter::on_attached() {
    hovered_role_ = context().roles->find("ckv.splitter.hovered");
    if (normal_role_ == ui::kInvalidRole) {
        normal_role_ = context().roles->find("ckv.splitter.normal");
    }
    if (focused_role_ == ui::kInvalidRole) {
        focused_role_ = context().roles->find("ckv.splitter.focused");
    }
}

int Splitter::main_extent() const noexcept {
    return orientation_ == Orientation::Horizontal ? bounds().width : bounds().height;
}

int Splitter::clamp_split(int position) const {
    const int usable = std::max(0, main_extent() - divider_extent());
    const bool horizontal = orientation_ == Orientation::Horizontal;
    const int first_min =
        horizontal ? first_->horizontal_size_hint().min : first_->vertical_size_hint().min;
    const int second_min =
        horizontal ? second_->horizontal_size_hint().min : second_->vertical_size_hint().min;
    const int max_position = std::max(first_min, usable - second_min);
    return std::clamp(position, std::min(first_min, usable), std::min(max_position, usable));
}

void Splitter::set_split_position(int position) {
    cancel_drag();
    apply_split_position(position);
}

void Splitter::apply_split_position(int position) {
    const int clamped = clamp_split(position);
    const int usable = std::max(0, main_extent() - divider_extent());
    anchored_extent_ = anchor_ == SplitterPane::First ? clamped : usable - clamped;
    if (clamped == split_position_) return;
    relayout();
    invalidate();
}

void Splitter::set_resize_anchor(SplitterPane pane) {
    if (pane == anchor_) return;
    cancel_drag();
    const int usable = std::max(0, main_extent() - divider_extent());
    anchor_ = pane;
    anchored_extent_ = anchor_ == SplitterPane::First ? split_position_ : usable - split_position_;
}

void Splitter::set_anchored_extent(int extent) {
    cancel_drag();
    anchored_extent_ = std::max(0, extent);
    relayout();
    invalidate();
}

bool Splitter::collapsed() const noexcept { return !first_->visible() || !second_->visible(); }

int Splitter::requested_split() const noexcept {
    const int usable = std::max(0, main_extent() - divider_extent());
    return anchor_ == SplitterPane::First ? anchored_extent_ : usable - anchored_extent_;
}

void Splitter::move_split(int position) {
    const int before = split_position_;
    apply_split_position(position);
    if (split_position_ != before && on_split_moved) on_split_moved();
}

void Splitter::relayout() {
    const bool both = first_->visible() && second_->visible();
    set_focus_policy(both ? ui::FocusPolicy::TabStop : ui::FocusPolicy::None);
    if (!both) {
        cancel_drag();
        const Rect whole{0, 0, bounds().width, bounds().height};
        if (first_->visible()) first_->set_bounds(whole);
        if (second_->visible()) second_->set_bounds(whole);
        return;
    }
    split_position_ = clamp_split(requested_split());
    const int usable = std::max(0, main_extent() - divider_extent());
    const int second_extent = std::max(0, usable - split_position_);
    const int after = std::min(split_position_ + divider_extent(), std::max(0, main_extent()));

    if (orientation_ == Orientation::Horizontal) {
        first_->set_bounds(Rect{0, 0, split_position_, bounds().height});
        second_->set_bounds(Rect{after, 0, second_extent, bounds().height});
    } else {
        first_->set_bounds(Rect{0, 0, bounds().width, split_position_});
        second_->set_bounds(Rect{0, after, bounds().width, second_extent});
    }
}

void Splitter::set_presentation(SplitterPresentation presentation) {
    if (presentation_ == presentation) return;
    cancel_drag();
    presentation_ = presentation;
    relayout();
    size_hint_changed();
    invalidate();
}

Rect Splitter::divider_bounds() const noexcept {
    if (collapsed()) return {};
    const int width = std::max(0, bounds().width);
    const int height = std::max(0, bounds().height);
    if (orientation_ == Orientation::Horizontal) {
        const int x = std::clamp(split_position_, 0, width);
        return Rect{x, 0, std::min(divider_extent(), width - x), height};
    }
    const int y = std::clamp(split_position_, 0, height);
    return Rect{0, y, width, std::min(divider_extent(), height - y)};
}

std::optional<PointerShape> Splitter::pointer_shape_at(Point local) const {
    if (!divider_bounds().contains(local)) return std::nullopt;
    if (!enabled_in_tree()) return PointerShape::NotAllowed;
    return orientation_ == Orientation::Horizontal ? PointerShape::ResizeEastWest : PointerShape::ResizeNorthSouth;
}

void Splitter::on_hover_changed(bool hovered) {
    if (!hovered) hover_position_.reset();
    invalidate();
}

void Splitter::draw(scene::Painter& painter) {
    const Rect handle = divider_bounds();
    if (handle.empty()) return;
    const bool enabled = enabled_in_tree();
    Style style = context().theme->resolve(enabled && (has_focus() || dragging_) ? focused_role_ : normal_role_);
    if (enabled && !has_focus() && !dragging_ && hover_position_ && handle.contains(*hover_position_))
        style = accent_style(style, context().theme->resolve(hovered_role_));
    auto clipped = painter.clipped(handle);
    if (presentation_ == SplitterPresentation::Gutter)
        clipped.fill(handle, Cell::from_grapheme(" ", style));
    const bool horizontal = orientation_ == Orientation::Horizontal;
    const int along = split_position_ + (divider_extent() - 1) / 2;
    if (horizontal) clipped.vline(Point{along, 0}, bounds().height, scene::LineStyle::Single, style);
    else clipped.hline(Point{0, along}, bounds().width, scene::LineStyle::Single, style);
    if (presentation_ != SplitterPresentation::Line) {
        const int cross = horizontal ? bounds().height : bounds().width;
        const int length = std::min(3, cross);
        const int start = (cross - length) / 2;
        for (int i = 0; i < length; ++i)
            clipped.draw_text(horizontal ? Point{along, start + i} : Point{start + i, along}, horizontal ? "⋮" : "⋯", style);
    }
}

namespace {

// `sum` is which axis is the main one for `orientation` — summing
// (plus the divider) along it and maxing the other, the same "main
// axis sums, cross axis maxes" shape Row/Column's own aggregate hints
// use (layout.cpp's sum_hints/max_hints), specialized to exactly two
// children plus a fixed divider extent instead of an arbitrary vector.
SizeHint combine(SizeHint a, SizeHint b, int divider, bool sum) {
    if (sum) {
        const int max = (a.max == ui::kUnboundedExtent || b.max == ui::kUnboundedExtent)
                             ? ui::kUnboundedExtent
                             : a.max + divider + b.max;
        return SizeHint{a.min + divider + b.min, a.preferred + divider + b.preferred, max};
    }
    const int max = (a.max == ui::kUnboundedExtent || b.max == ui::kUnboundedExtent)
                        ? ui::kUnboundedExtent
                        : std::max(a.max, b.max);
    return SizeHint{std::max(a.min, b.min), std::max(a.preferred, b.preferred), max};
}

}  // namespace

SizeHint Splitter::horizontal_size_hint() const {
    if (!first_->visible()) return second_->visible() ? second_->horizontal_size_hint() : SizeHint{0, 0, 0};
    if (!second_->visible()) return first_->horizontal_size_hint();
    return combine(first_->horizontal_size_hint(), second_->horizontal_size_hint(), divider_extent(),
                    orientation_ == Orientation::Horizontal);
}

SizeHint Splitter::vertical_size_hint() const {
    if (!first_->visible()) return second_->visible() ? second_->vertical_size_hint() : SizeHint{0, 0, 0};
    if (!second_->visible()) return first_->vertical_size_hint();
    return combine(first_->vertical_size_hint(), second_->vertical_size_hint(), divider_extent(),
                    orientation_ == Orientation::Vertical);
}

bool Splitter::on_key(const KeyEvent& event) {
    if (collapsed() || !enabled_in_tree() || event.action == KeyAction::Release) return false;
    const Key key = event.chord.key;
    if (orientation_ == Orientation::Horizontal) {
        if (key == Key::Left) {
            move_split(split_position_ - 1);
            return true;
        }
        if (key == Key::Right) {
            move_split(split_position_ + 1);
            return true;
        }
    } else {
        if (key == Key::Up) {
            move_split(split_position_ - 1);
            return true;
        }
        if (key == Key::Down) {
            move_split(split_position_ + 1);
            return true;
        }
    }
    return false;
}

bool Splitter::on_mouse(const MouseEvent& event) {
    if (collapsed() || !enabled_in_tree()) { cancel_drag(); return false; }
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    const int along = orientation_ == Orientation::Horizontal ? local.x : local.y;
    if (event.action == MouseAction::Down) {
        if (event.button != MouseButton::Left || !divider_bounds().contains(local)) return false;
        drag_offset_ = along - split_position_;
        dragging_ = true;
        invalidate();
        return true;
    }
    if (event.action == MouseAction::Move) {
        hover_position_ = local;
        invalidate();
        if (!dragging_) return false;
        move_split(along - drag_offset_);
        return true;
    }
    if (event.action == MouseAction::Up && dragging_) {
        dragging_ = false;
        invalidate();
        return true;
    }
    return false;
}

void Splitter::on_focus(const FocusEvent& event) {
    if (!event.gained) cancel_drag();
    invalidate();
}

}  // namespace ckv::widgets
