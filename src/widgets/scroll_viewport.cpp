// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/scroll_viewport.hpp"

#include <algorithm>

#include "widgets/frame_scrollbar_internal.hpp"

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

// The frame the content is clipped to. It adds nothing but its bounds, and
// tells the viewport when the content's size hints change so the viewport
// can re-measure.
class ContentFrame final : public ui::View {
public:
    void on_child_size_hint_changed(View&) override { size_hint_changed(); }
};

}  // namespace

ScrollViewport::ScrollViewport(ui::FocusPolicy focus_policy) : View(Rect{}, focus_policy) {
    frame_ = make<ContentFrame>();
    v_scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    v_scrollbar_liveness_ = v_scrollbar_->lifetime_token();
    v_scrollbar_->bind_subject(this);
    v_scrollbar_->set_policy(ScrollbarPolicy::Auto);
    v_scrollbar_->on_change = [this](int p) { set_scroll(scroll_x_, p); };

    h_scrollbar_ = make<Scrollbar>(Orientation::Horizontal);
    h_scrollbar_liveness_ = h_scrollbar_->lifetime_token();
    h_scrollbar_->bind_subject(this);
    h_scrollbar_->set_policy(ScrollbarPolicy::Auto);
    h_scrollbar_->on_change = [this](int p) { set_scroll(p, scroll_y_); };
}

ScrollViewport::~ScrollViewport() {
    detail::sever_scrollbar(*this, vertical_bar());
    detail::sever_scrollbar(*this, horizontal_bar());
}

Scrollbar* ScrollViewport::vertical_bar() const noexcept {
    return v_scrollbar_ != nullptr && !v_scrollbar_liveness_.expired() ? v_scrollbar_ : nullptr;
}

Scrollbar* ScrollViewport::horizontal_bar() const noexcept {
    return h_scrollbar_ != nullptr && !h_scrollbar_liveness_.expired() ? h_scrollbar_ : nullptr;
}

bool ScrollViewport::vertical_bar_embedded() const noexcept {
    Scrollbar* const bar = vertical_bar();
    return bar != nullptr && bar->parent() == this;
}

bool ScrollViewport::horizontal_bar_embedded() const noexcept {
    Scrollbar* const bar = horizontal_bar();
    return bar != nullptr && bar->parent() == this;
}

std::unique_ptr<Scrollbar> ScrollViewport::take_vertical_scrollbar() {
    if (!vertical_bar_embedded()) return nullptr;
    v_policy_ = v_scrollbar_->policy();
    std::unique_ptr<ui::View> detached = remove_child(v_scrollbar_);
    std::unique_ptr<Scrollbar> owned(static_cast<Scrollbar*>(detached.release()));
    relayout();
    invalidate();
    return owned;
}

std::unique_ptr<Scrollbar> ScrollViewport::take_horizontal_scrollbar() {
    if (!horizontal_bar_embedded()) return nullptr;
    h_policy_ = h_scrollbar_->policy();
    std::unique_ptr<ui::View> detached = remove_child(h_scrollbar_);
    std::unique_ptr<Scrollbar> owned(static_cast<Scrollbar*>(detached.release()));
    relayout();
    invalidate();
    return owned;
}

void ScrollViewport::on_detaching() {
    const bool v_detached = detail::detach_orphaned_scrollbar(*this, v_scrollbar_, v_scrollbar_liveness_);
    const bool h_detached = detail::detach_orphaned_scrollbar(*this, h_scrollbar_, h_scrollbar_liveness_);
    if (v_detached || h_detached) relayout();
}

void ScrollViewport::on_effective_visibility_changed() {
    sync_scrollbars();
}

std::unique_ptr<ui::View> ScrollViewport::set_content(std::unique_ptr<ui::View> content) {
    std::unique_ptr<ui::View> previous = content_ != nullptr ? frame_->remove_child(content_) : nullptr;
    content_ = content != nullptr ? frame_->add_child(std::move(content)) : nullptr;
    // Back to the origin through the bars, which hold the offset: zeroing
    // only the copy here left them where the old content was scrolled to,
    // and the next arrow key stepped on from there.
    set_scroll(0, 0);
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
    const ScrollbarPolicy policy = visible ? ScrollbarPolicy::Always : ScrollbarPolicy::Auto;
    v_policy_ = policy;
    h_policy_ = policy;
    if (Scrollbar* v = vertical_bar()) v->set_policy(policy);
    if (Scrollbar* h = horizontal_bar()) h->set_policy(policy);
    v_policy_ = policy;
    h_policy_ = policy;
    relayout();
    invalidate();
}

void ScrollViewport::set_vertical_scrollbar_policy(ScrollbarPolicy policy) {
    if (vertical_scrollbar_policy() == policy) return;
    v_policy_ = policy;
    if (Scrollbar* v = vertical_bar()) v->set_policy(policy);
    relayout();
    invalidate();
}

void ScrollViewport::set_horizontal_scrollbar_policy(ScrollbarPolicy policy) {
    if (horizontal_scrollbar_policy() == policy) return;
    h_policy_ = policy;
    if (Scrollbar* h = horizontal_bar()) h->set_policy(policy);
    relayout();
    invalidate();
}

ScrollbarPolicy ScrollViewport::vertical_scrollbar_policy() const noexcept {
    Scrollbar* const v = vertical_bar();
    return v != nullptr ? v->policy() : v_policy_;
}

ScrollbarPolicy ScrollViewport::horizontal_scrollbar_policy() const noexcept {
    Scrollbar* const h = horizontal_bar();
    return h != nullptr ? h->policy() : h_policy_;
}

bool ScrollViewport::can_scroll_vertically() const noexcept { return max_scroll_y_ > 0; }

bool ScrollViewport::can_scroll_horizontally() const noexcept { return max_scroll_x_ > 0; }

void ScrollViewport::set_scroll(int x, int y) {
    const int nx = std::clamp(x, 0, max_scroll_x_);
    const int ny = std::clamp(y, 0, max_scroll_y_);
    const int dy = ny - scroll_y_;
    const bool moved = nx != scroll_x_ || ny != scroll_y_;
    scroll_x_ = nx;
    scroll_y_ = ny;
    if (Scrollbar* h = horizontal_bar()) h->set_position(scroll_x_);
    if (Scrollbar* v = vertical_bar()) v->set_position(scroll_y_);
    if (moved) {
        reposition_content();
        if (dy != 0 && frame_ != nullptr) notify_scroll(frame_->bounds(), dy);
    }
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
    size_hint_changed();
    invalidate();
}

void ScrollViewport::on_descendant_focused(View& target) { ensure_visible(target); }

ui::SizeHint ScrollViewport::horizontal_size_hint() const {
    if (preferred_size().width > 0) return View::horizontal_size_hint();
    if (content_ == nullptr) return View::horizontal_size_hint();
    const int pref = std::max(0, content_->horizontal_size_hint().preferred);
    return ui::SizeHint{0, pref, ui::kUnboundedExtent};
}

ui::SizeHint ScrollViewport::vertical_size_hint() const {
    if (preferred_size().height > 0) return View::vertical_size_hint();
    if (content_ == nullptr) return View::vertical_size_hint();
    const int pref = std::max(0, content_->vertical_size_hint().preferred);
    return ui::SizeHint{0, pref, ui::kUnboundedExtent};
}

void ScrollViewport::sync_scrollbars() {
    if (Scrollbar* v = vertical_bar()) {
        v->set_visible(v->should_show() && detail::subject_visible_for_bar(*this, *v));
    }
    if (Scrollbar* h = horizontal_bar()) {
        h->set_visible(h->should_show() && detail::subject_visible_for_bar(*this, *h));
    }
}

void ScrollViewport::relayout() {
    const int width = bounds().width;
    const int height = bounds().height;
    int preferred_w = width;
    if (content_ != nullptr) preferred_w = std::max(0, content_->horizontal_size_hint().preferred);

    const auto measured_height = [this, height](int for_width) {
        if (content_ == nullptr) return height;
        return std::max({0, content_->vertical_size_hint().preferred,
                         content_->height_for_width(std::max(0, for_width))});
    };

    const ScrollbarPolicy v_policy = vertical_scrollbar_policy();
    const ScrollbarPolicy h_policy = horizontal_scrollbar_policy();
    v_policy_ = v_policy;
    h_policy_ = h_policy;
    const int v_cost = vertical_bar_embedded() ? 1 : 0;
    const int h_cost = horizontal_bar_embedded() ? 1 : 0;
    int preferred_h = measured_height(width);
    bool need_v = bar_is_shown(v_policy, preferred_h, height);
    // An embedded vertical bar costs a column, and a narrower column count
    // makes wrapped content taller — so the decision is re-measured at the
    // width the content will actually get. A bar hosted on an enclosing
    // window's frame costs zero columns here.
    if (need_v && v_cost > 0) preferred_h = measured_height(width - v_cost);
    bool need_h = bar_is_shown(h_policy, preferred_w, std::max(0, width - (need_v ? v_cost : 0)));
    need_v = bar_is_shown(v_policy, preferred_h, std::max(0, height - (need_h ? h_cost : 0)));

    const int content_area_w = std::max(0, width - (need_v ? v_cost : 0));
    const int content_area_h = std::max(0, height - (need_h ? h_cost : 0));
    content_area_width_ = content_area_w;
    content_area_height_ = content_area_h;
    // A Hidden bar is not merely undrawn: its axis does not scroll, so the
    // content is held to the visible extent rather than its own larger
    // preferred one (see the header). Auto and Always are unchanged.
    const int content_w =
        h_policy == ScrollbarPolicy::Hidden ? content_area_w : std::max(content_area_w, preferred_w);
    const int content_h =
        v_policy == ScrollbarPolicy::Hidden ? content_area_h : std::max(content_area_h, preferred_h);

    max_scroll_x_ = std::max(0, content_w - std::max(1, content_area_w));
    max_scroll_y_ = std::max(0, content_h - std::max(1, content_area_h));
    scroll_x_ = std::clamp(scroll_x_, 0, max_scroll_x_);
    scroll_y_ = std::clamp(scroll_y_, 0, max_scroll_y_);

    frame_->set_bounds(Rect{0, 0, content_area_w, content_area_h});
    Scrollbar* const v = vertical_bar();
    Scrollbar* const h = horizontal_bar();
    if (v != nullptr && v->parent() == this)
        v->set_bounds(Rect{content_area_w, 0, need_v ? std::min(1, width) : 0, content_area_h});
    if (h != nullptr && h->parent() == this)
        h->set_bounds(Rect{0, content_area_h, content_area_w, need_h ? std::min(1, height) : 0});
    if (v != nullptr) {
        v->set_range(content_h, std::max(1, content_area_h));
        v->set_position(scroll_y_);
    }
    if (h != nullptr) {
        h->set_range(content_w, std::max(1, content_area_w));
        h->set_position(scroll_x_);
    }
    sync_scrollbars();

    if (content_ != nullptr) content_->set_bounds(Rect{-scroll_x_, -scroll_y_, content_w, content_h});
}

bool ScrollViewport::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || event.action == KeyAction::Release) return false;
    // A viewport with nothing to scroll consumes nothing. Otherwise every
    // surface that happens to be wrapped in one would swallow its
    // container's arrow keys in order to move by zero cells — which is
    // exactly what a dialog whose content fits must not do.
    switch (event.chord.key) {
        case Key::Up:
            if (!can_scroll_vertically()) return false;
            set_scroll(scroll_x_, scroll_y_ - 1);
            return true;
        case Key::Down:
            if (!can_scroll_vertically()) return false;
            set_scroll(scroll_x_, scroll_y_ + 1);
            return true;
        case Key::PageUp:
            if (!can_scroll_vertically()) return false;
            set_scroll(scroll_x_, scroll_y_ - std::max(1, content_area_height_));
            return true;
        case Key::PageDown:
            if (!can_scroll_vertically()) return false;
            set_scroll(scroll_x_, scroll_y_ + std::max(1, content_area_height_));
            return true;
        case Key::Home:
            if (!can_scroll_vertically()) return false;
            set_scroll(scroll_x_, 0);
            return true;
        case Key::End:
            if (!can_scroll_vertically()) return false;
            set_scroll(scroll_x_, max_scroll_y_);
            return true;
        case Key::Left:
            if (!can_scroll_horizontally()) return false;
            set_scroll(scroll_x_ - 1, scroll_y_);
            return true;
        case Key::Right:
            if (!can_scroll_horizontally()) return false;
            set_scroll(scroll_x_ + 1, scroll_y_);
            return true;
        default:
            return false;
    }
}

bool ScrollViewport::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree()) return false;
    // Same rule as on_key: with nothing to scroll the wheel is left
    // unhandled, so Application's ancestor walk can carry it to whatever
    // outer surface does have somewhere to go.
    const int rows = ui::wheel_scroll_rows(event);
    if (rows != 0 && can_scroll_vertically()) {
        set_scroll(scroll_x_, scroll_y_ + rows);
        return true;
    }
    const int cols = ui::wheel_scroll_cols(event);
    if (cols != 0 && can_scroll_horizontally()) {
        set_scroll(scroll_x_ + cols, scroll_y_);
        return true;
    }
    return false;
}

}  // namespace ckv::widgets
