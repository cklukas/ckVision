// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/splitter.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"

namespace ckv::widgets {

Splitter::Splitter(Rect bounds, std::unique_ptr<View> first, std::unique_ptr<View> second,
                    Orientation orientation)
    : View(bounds), orientation_(orientation) {
    CKV_ASSERT(first != nullptr);
    CKV_ASSERT(second != nullptr);
    first_ = add_child(std::move(first));
    second_ = add_child(std::move(second));
    set_focus_policy(ui::FocusPolicy::TabStop);

    const int usable = std::max(0, main_extent() - kDividerExtent);
    split_position_ = clamp_split(usable / 2);
    anchored_extent_ = split_position_;
    relayout();
}

void Splitter::on_attached() {
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
    const int usable = std::max(0, main_extent() - kDividerExtent);
    const bool horizontal = orientation_ == Orientation::Horizontal;
    const int first_min =
        horizontal ? first_->horizontal_size_hint().min : first_->vertical_size_hint().min;
    const int second_min =
        horizontal ? second_->horizontal_size_hint().min : second_->vertical_size_hint().min;
    const int max_position = std::max(first_min, usable - second_min);
    return std::clamp(position, first_min, max_position);
}

void Splitter::set_split_position(int position) {
    const int clamped = clamp_split(position);
    const int usable = std::max(0, main_extent() - kDividerExtent);
    anchored_extent_ = anchor_ == SplitterPane::First ? clamped : usable - clamped;
    if (clamped == split_position_) return;
    relayout();
    invalidate();
}

void Splitter::set_resize_anchor(SplitterPane pane) {
    if (pane == anchor_) return;
    const int usable = std::max(0, main_extent() - kDividerExtent);
    anchor_ = pane;
    anchored_extent_ = anchor_ == SplitterPane::First ? split_position_ : usable - split_position_;
}

void Splitter::set_anchored_extent(int extent) {
    anchored_extent_ = std::max(0, extent);
    relayout();
    invalidate();
}

bool Splitter::collapsed() const noexcept { return !first_->visible() || !second_->visible(); }

int Splitter::requested_split() const noexcept {
    const int usable = std::max(0, main_extent() - kDividerExtent);
    return anchor_ == SplitterPane::First ? anchored_extent_ : usable - anchored_extent_;
}

void Splitter::move_split(int position) {
    const int before = split_position_;
    set_split_position(position);
    if (split_position_ != before && on_split_moved) on_split_moved();
}

void Splitter::relayout() {
    const bool both = first_->visible() && second_->visible();
    set_focus_policy(both ? ui::FocusPolicy::TabStop : ui::FocusPolicy::None);
    if (!both) {
        const Rect whole{0, 0, bounds().width, bounds().height};
        if (first_->visible()) first_->set_bounds(whole);
        if (second_->visible()) second_->set_bounds(whole);
        return;
    }
    split_position_ = clamp_split(requested_split());
    const int usable = std::max(0, main_extent() - kDividerExtent);
    const int second_extent = std::max(0, usable - split_position_);
    const int after = split_position_ + kDividerExtent;

    if (orientation_ == Orientation::Horizontal) {
        first_->set_bounds(Rect{0, 0, split_position_, bounds().height});
        second_->set_bounds(Rect{after, 0, second_extent, bounds().height});
    } else {
        first_->set_bounds(Rect{0, 0, bounds().width, split_position_});
        second_->set_bounds(Rect{0, after, bounds().width, second_extent});
    }
}

void Splitter::draw(scene::Painter& painter) {
    if (collapsed()) return;
    const ui::RoleId role = has_focus() ? focused_role_ : normal_role_;
    const Style style = context().theme->resolve(role);
    if (orientation_ == Orientation::Horizontal) {
        painter.vline(Point{split_position_, 0}, bounds().height, scene::LineStyle::Single, style);
    } else {
        painter.hline(Point{0, split_position_}, bounds().width, scene::LineStyle::Single, style);
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
    return combine(first_->horizontal_size_hint(), second_->horizontal_size_hint(), kDividerExtent,
                    orientation_ == Orientation::Horizontal);
}

SizeHint Splitter::vertical_size_hint() const {
    return combine(first_->vertical_size_hint(), second_->vertical_size_hint(), kDividerExtent,
                    orientation_ == Orientation::Vertical);
}

bool Splitter::on_key(const KeyEvent& event) {
    if (collapsed()) return false;
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
    const Rect abs = absolute_bounds();
    // The divider's position is measured along the main axis, in this
    // view's own coordinates — the same number split_position_ holds, so
    // the pointer lands on the divider exactly when the two agree.
    const int along = orientation_ == Orientation::Horizontal ? event.cell.x - abs.x
                                                              : event.cell.y - abs.y;

    if (event.action == MouseAction::Down) {
        if (event.button != MouseButton::Left || collapsed()) return false;
        // Only the divider itself. A press anywhere else belongs to
        // whichever pane is there, and claiming it would swallow clicks
        // meant for the list or the document beside it.
        if (along != split_position_) return false;
        dragging_ = true;
        return true;
    }

    if (!dragging_) return false;

    if (event.action == MouseAction::Move) {
        // The divider goes where the pointer is, not where it started plus
        // a delta: the grab point IS the divider, one cell wide, so there
        // is no offset to carry and none to accumulate error in.
        move_split(along);
        return true;
    }

    if (event.action == MouseAction::Up) {
        dragging_ = false;
        return true;
    }
    return false;
}

void Splitter::on_focus(const FocusEvent&) { invalidate(); }

}  // namespace ckv::widgets
