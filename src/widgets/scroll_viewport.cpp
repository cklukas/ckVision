// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/scroll_viewport.hpp"

#include <algorithm>

namespace ckv::widgets {

ScrollViewport::ScrollViewport() {
    auto v = std::make_unique<Scrollbar>(Orientation::Vertical);
    v_scrollbar_ = static_cast<Scrollbar*>(add_child(std::move(v)));
    v_scrollbar_->on_position_changed = [this](int p) {
        scroll_y_ = p;
        reposition_content();
    };

    auto h = std::make_unique<Scrollbar>(Orientation::Horizontal);
    h_scrollbar_ = static_cast<Scrollbar*>(add_child(std::move(h)));
    h_scrollbar_->on_position_changed = [this](int p) {
        scroll_x_ = p;
        reposition_content();
    };
}

std::unique_ptr<ui::View> ScrollViewport::set_content(std::unique_ptr<ui::View> content) {
    std::unique_ptr<ui::View> previous = content_ != nullptr ? remove_child(content_) : nullptr;
    content_ = content != nullptr ? add_child(std::move(content)) : nullptr;
    scroll_x_ = 0;
    scroll_y_ = 0;
    relayout();
    return previous;
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
    const int content_area_w = std::max(0, width - 1);
    const int content_area_h = std::max(0, height - 1);

    if (v_scrollbar_ != nullptr)
        v_scrollbar_->set_bounds(Rect{content_area_w, 0, std::min(1, width), content_area_h});
    if (h_scrollbar_ != nullptr)
        h_scrollbar_->set_bounds(Rect{0, content_area_h, content_area_w, std::min(1, height)});

    int content_w = content_area_w;
    int content_h = content_area_h;
    if (content_ != nullptr) {
        content_w = std::max(content_area_w, content_->horizontal_size_hint().preferred);
        content_h = std::max(content_area_h, content_->vertical_size_hint().preferred);
    }
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
