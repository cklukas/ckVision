// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/scroll_viewport.hpp"

#include <algorithm>

namespace ckv::widgets {

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
    const int reach = content_ != nullptr ? content_->vertical_size_hint().preferred : 0;
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

void ScrollViewport::set_scroll(int x, int y) {
    if (h_scrollbar_ != nullptr) h_scrollbar_->set_position(x);
    if (v_scrollbar_ != nullptr) v_scrollbar_->set_position(y);
}

void ScrollViewport::on_resized() { relayout(); }

void ScrollViewport::reposition_content() {
    if (content_ == nullptr) return;
    content_->set_bounds(Rect{-scroll_x_, -scroll_y_, content_->bounds().width, content_->bounds().height});
}

void ScrollViewport::relayout() {
    const int width = bounds().width;
    const int height = bounds().height;
    int preferred_w = width;
    int preferred_h = height;
    if (content_ != nullptr) {
        preferred_w = std::max(0, content_->horizontal_size_hint().preferred);
        preferred_h = std::max(0, content_->vertical_size_hint().preferred);
    }

    bool need_v = scrollbars_always_visible_ || preferred_h > height;
    bool need_h = scrollbars_always_visible_ || preferred_w > std::max(0, width - (need_v ? 1 : 0));
    need_v = scrollbars_always_visible_ || preferred_h > std::max(0, height - (need_h ? 1 : 0));

    const int content_area_w = std::max(0, width - (need_v ? 1 : 0));
    const int content_area_h = std::max(0, height - (need_h ? 1 : 0));
    const int content_w = std::max(content_area_w, preferred_w);
    const int content_h = std::max(content_area_h, preferred_h);

    if (v_scrollbar_ != nullptr)
        v_scrollbar_->set_bounds(Rect{content_area_w, 0, need_v ? std::min(1, width) : 0, content_area_h});
    if (h_scrollbar_ != nullptr)
        h_scrollbar_->set_bounds(Rect{0, content_area_h, content_area_w, need_h ? std::min(1, height) : 0});
    if (v_scrollbar_ != nullptr) v_scrollbar_->set_range(content_h, std::max(1, content_area_h));
    if (h_scrollbar_ != nullptr) h_scrollbar_->set_range(content_w, std::max(1, content_area_w));

    if (content_ != nullptr) content_->set_bounds(Rect{-scroll_x_, -scroll_y_, content_w, content_h});
}

bool ScrollViewport::on_key(const KeyEvent& event) {
    switch (event.chord.key) {
        case Key::Up:
        case Key::Down:
        case Key::PageUp:
        case Key::PageDown:
        case Key::Home:
        case Key::End:
            return v_scrollbar_ != nullptr && v_scrollbar_->on_key(event);
        case Key::Left:
        case Key::Right:
            return h_scrollbar_ != nullptr && h_scrollbar_->on_key(event);
        default:
            return false;
    }
}

bool ScrollViewport::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Wheel || v_scrollbar_ == nullptr) return false;
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
