// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/scroll_viewport.hpp"

#include <algorithm>

namespace ckv::widgets {

namespace {

// Whether a bar of `policy` is on screen for a content extent of `extent`
// within `available` cells. The same question Scrollbar::should_show()
// answers about itself, asked before the range has been handed over — the
// bar cannot be consulted here, because how much room the content gets is
// exactly what this decides.
bool bar_is_shown(ScrollbarPolicy policy, int extent, int available) noexcept {
    switch (policy) {
        case ScrollbarPolicy::Always:
            return true;
        case ScrollbarPolicy::Hidden:
            return false;
        case ScrollbarPolicy::Auto:
            break;
    }
    return extent > available;
}

}  // namespace

ScrollViewport::ScrollViewport() {
    v_scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    v_scrollbar_->set_policy(ScrollbarPolicy::Auto);
    v_scrollbar_->on_position_changed = [this](int p) {
        scroll_y_ = p;
        reposition_content();
    };

    h_scrollbar_ = make<Scrollbar>(Orientation::Horizontal);
    h_scrollbar_->set_policy(ScrollbarPolicy::Auto);
    h_scrollbar_->on_position_changed = [this](int p) {
        scroll_x_ = p;
        reposition_content();
    };
}

std::unique_ptr<ui::View> ScrollViewport::set_content(std::unique_ptr<ui::View> content) {
    std::unique_ptr<ui::View> previous = content_ != nullptr ? remove_child(content_) : nullptr;
    content_ = content != nullptr ? add_child(std::move(content)) : nullptr;
    // Scrollbars are persistent siblings created by the constructor. Keep
    // them above document content for both painting and hit-testing.
    if (content_ != nullptr) lower_to_back(content_);
    scroll_x_ = 0;
    scroll_y_ = 0;
    relayout();
    return previous;
}

void ScrollViewport::scroll_to_bottom() {
    // set_scroll clamps, so asking for more than there is lands exactly
    // at the end rather than needing the extent computed here.
    // Measured the way relayout() measures it, so the bottom of wrapped
    // content is the bottom a reader can actually see.
    const int reach =
        content_ != nullptr
            ? std::max(content_->vertical_size_hint().preferred,
                       content_->height_for_width(std::max(0, content_area_width_)))
            : 0;
    set_scroll(scroll_x_, std::max(0, reach));
}

void ScrollViewport::set_scrollbars_always_visible(bool visible) noexcept {
    if (scrollbars_always_visible_ == visible) return;
    scrollbars_always_visible_ = visible;
    if (v_scrollbar_ != nullptr)
        v_scrollbar_->set_policy(visible ? ScrollbarPolicy::Always : ScrollbarPolicy::Auto);
    if (h_scrollbar_ != nullptr)
        h_scrollbar_->set_policy(visible ? ScrollbarPolicy::Always : ScrollbarPolicy::Auto);
    relayout();
    invalidate();
}

void ScrollViewport::set_vertical_scrollbar_policy(ScrollbarPolicy policy) {
    if (v_scrollbar_ == nullptr || v_scrollbar_->policy() == policy) return;
    v_scrollbar_->set_policy(policy);
    relayout();
    invalidate();
}

void ScrollViewport::set_horizontal_scrollbar_policy(ScrollbarPolicy policy) {
    if (h_scrollbar_ == nullptr || h_scrollbar_->policy() == policy) return;
    h_scrollbar_->set_policy(policy);
    relayout();
    invalidate();
}

ScrollbarPolicy ScrollViewport::vertical_scrollbar_policy() const noexcept {
    return v_scrollbar_ != nullptr ? v_scrollbar_->policy() : ScrollbarPolicy::Hidden;
}

ScrollbarPolicy ScrollViewport::horizontal_scrollbar_policy() const noexcept {
    return h_scrollbar_ != nullptr ? h_scrollbar_->policy() : ScrollbarPolicy::Hidden;
}

bool ScrollViewport::can_scroll_vertically() const noexcept {
    return v_scrollbar_ != nullptr && v_scrollbar_->max_position() > 0;
}

bool ScrollViewport::can_scroll_horizontally() const noexcept {
    return h_scrollbar_ != nullptr && h_scrollbar_->max_position() > 0;
}

void ScrollViewport::set_scroll(int x, int y) {
    if (h_scrollbar_ != nullptr) h_scrollbar_->set_position(x);
    if (v_scrollbar_ != nullptr) v_scrollbar_->set_position(y);
}

bool ScrollViewport::ensure_visible(const ui::View& descendant) {
    if (content_ == nullptr || &descendant == content_) return false;
    // Where `descendant` sits in CONTENT-local coordinates: the sum of the
    // parent-local offsets between it and the content view. The same walk
    // answers whether it is inside this viewport at all — a caller that got
    // here from "whatever currently has focus" is entitled to ask about a
    // view living somewhere else entirely.
    int x = 0;
    int y = 0;
    const ui::View* view = &descendant;
    for (; view != nullptr && view != content_; view = view->parent()) {
        x += view->bounds().x;
        y += view->bounds().y;
    }
    if (view != content_) return false;

    const int width = std::max(1, content_area_width_);
    const int height = std::max(1, content_area_height_);
    const Rect box = descendant.bounds();
    int target_x = scroll_x_;
    int target_y = scroll_y_;
    // Least movement, and the near edge wins: a control taller than the
    // visible band shows its own top rather than its bottom, since its label
    // and its cursor are at the top.
    if (x + box.width > target_x + width) target_x = x + box.width - width;
    if (x < target_x) target_x = x;
    if (y + box.height > target_y + height) target_y = y + box.height - height;
    if (y < target_y) target_y = y;
    if (target_x == scroll_x_ && target_y == scroll_y_) return false;
    const int previous_x = scroll_x_;
    const int previous_y = scroll_y_;
    set_scroll(target_x, target_y);
    return scroll_x_ != previous_x || scroll_y_ != previous_y;
}

void ScrollViewport::on_resized() { relayout(); }

void ScrollViewport::reposition_content() {
    if (content_ == nullptr) return;
    content_->set_bounds(Rect{-scroll_x_, -scroll_y_, content_->bounds().width, content_->bounds().height});
}

void ScrollViewport::on_child_size_hint_changed(View&) {
    relayout();
    invalidate();
}

void ScrollViewport::relayout() {
    const int width = bounds().width;
    const int height = bounds().height;
    int preferred_w = width;
    if (content_ != nullptr) preferred_w = std::max(0, content_->horizontal_size_hint().preferred);

    // How tall the content is AT A GIVEN WIDTH. Wrapped content — static
    // text, a memo — is as tall as its width makes it, and only it can
    // say how tall (the architecture §5's one sanctioned second pass).
    // Measuring by the width-independent hint alone silently loses the
    // tail of any prose laid out narrower than the width that hint was
    // measured at: too short an extent means no bar and no way to reach
    // the rest, so the text is simply not there. The max keeps every
    // width-independent view reporting exactly what it always did, since
    // height_for_width defaults to a stored preferred height rather than
    // to that view's own vertical hint.
    const auto measured_height = [this, height](int for_width) {
        if (content_ == nullptr) return height;
        return std::max({0, content_->vertical_size_hint().preferred,
                         content_->height_for_width(std::max(0, for_width))});
    };

    const ScrollbarPolicy v_policy = vertical_scrollbar_policy();
    const ScrollbarPolicy h_policy = horizontal_scrollbar_policy();
    int preferred_h = measured_height(width);
    bool need_v = bar_is_shown(v_policy, preferred_h, height);
    // A vertical bar costs a column, and a narrower column count makes
    // wrapped content taller — so the decision is re-measured at the
    // width the content will actually get. This only ever grows the
    // content, so it cannot oscillate back.
    if (need_v) preferred_h = measured_height(width - 1);
    bool need_h = bar_is_shown(h_policy, preferred_w, std::max(0, width - (need_v ? 1 : 0)));
    need_v = bar_is_shown(v_policy, preferred_h, std::max(0, height - (need_h ? 1 : 0)));

    const int content_area_w = std::max(0, width - (need_v ? 1 : 0));
    const int content_area_h = std::max(0, height - (need_h ? 1 : 0));
    content_area_width_ = content_area_w;
    content_area_height_ = content_area_h;
    // A Hidden bar is not merely undrawn: its axis does not scroll, so the
    // content is held to the visible extent rather than its own larger
    // preferred one (see the header). Auto and Always are unchanged.
    const int content_w =
        h_policy == ScrollbarPolicy::Hidden ? content_area_w : std::max(content_area_w, preferred_w);
    const int content_h =
        v_policy == ScrollbarPolicy::Hidden ? content_area_h : std::max(content_area_h, preferred_h);

    if (v_scrollbar_ != nullptr)
        v_scrollbar_->set_bounds(Rect{content_area_w, 0, need_v ? std::min(1, width) : 0, content_area_h});
    if (h_scrollbar_ != nullptr)
        h_scrollbar_->set_bounds(Rect{0, content_area_h, content_area_w, need_h ? std::min(1, height) : 0});
    if (v_scrollbar_ != nullptr) v_scrollbar_->set_range(content_h, std::max(1, content_area_h));
    if (h_scrollbar_ != nullptr) h_scrollbar_->set_range(content_w, std::max(1, content_area_w));

    if (content_ != nullptr) content_->set_bounds(Rect{-scroll_x_, -scroll_y_, content_w, content_h});
}

bool ScrollViewport::on_key(const KeyEvent& event) {
    // A viewport with nothing to scroll consumes nothing. Otherwise every
    // surface that happens to be wrapped in one would swallow its
    // container's arrow keys in order to move by zero cells — which is
    // exactly what a dialog whose content fits must not do.
    switch (event.chord.key) {
        case Key::Up:
        case Key::Down:
        case Key::PageUp:
        case Key::PageDown:
        case Key::Home:
        case Key::End:
            return can_scroll_vertically() && v_scrollbar_->on_key(event);
        case Key::Left:
        case Key::Right:
            return can_scroll_horizontally() && h_scrollbar_->on_key(event);
        default:
            return false;
    }
}

bool ScrollViewport::on_mouse(const MouseEvent& event) {
    // Same rule as on_key: with nothing to scroll the wheel is left
    // unhandled, so Application's ancestor walk can carry it to whatever
    // outer surface does have somewhere to go.
    if (event.action != MouseAction::Wheel || !can_scroll_vertically()) return false;
    if (event.button == MouseButton::WheelUp) {
        v_scrollbar_->set_position(scroll_y_ - 1);
        return true;
    }
    if (event.button == MouseButton::WheelDown) {
        v_scrollbar_->set_position(scroll_y_ + 1);
        return true;
    }
    return false;
}

}  // namespace ckv::widgets
