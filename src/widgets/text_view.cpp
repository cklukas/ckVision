// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/text_view.hpp"

#include <algorithm>

namespace ckv::widgets {

TextView::TextView() {
    auto scrollbar = std::make_unique<Scrollbar>(Orientation::Vertical);
    scrollbar_ = static_cast<Scrollbar*>(add_child(std::move(scrollbar)));
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void TextView::on_attached() {
    if (text_role_ == ui::kInvalidRole) text_role_ = context().roles->find("ckv.textview.text");
}

void TextView::split_lines() {
    lines_.clear();
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = raw_text_.find('\n', start);
        if (nl == std::string::npos) {
            lines_.push_back(raw_text_.substr(start));
            break;
        }
        lines_.push_back(raw_text_.substr(start, nl - start));
        start = nl + 1;
    }
}

void TextView::set_text(std::string text) {
    raw_text_ = std::move(text);
    split_lines();
    on_resized();
    invalidate();
}

int TextView::top_line() const noexcept { return scrollbar_ != nullptr ? scrollbar_->position() : 0; }

void TextView::on_resized() {
    if (scrollbar_ == nullptr) return;
    scrollbar_->set_bounds(Rect{std::max(0, bounds().width - 1), 0, std::min(1, bounds().width), bounds().height});
    scrollbar_->set_range(static_cast<int>(lines_.size()), std::max(1, bounds().height));
}

bool TextView::on_key(const KeyEvent& event) {
    if (scrollbar_ == nullptr) return false;
    switch (event.chord.key) {
        case Key::Up:
            scrollbar_->set_position(scrollbar_->position() - 1);
            return true;
        case Key::Down:
            scrollbar_->set_position(scrollbar_->position() + 1);
            return true;
        case Key::PageUp:
            scrollbar_->set_position(scrollbar_->position() - std::max(1, bounds().height));
            return true;
        case Key::PageDown:
            scrollbar_->set_position(scrollbar_->position() + std::max(1, bounds().height));
            return true;
        case Key::Home:
            scrollbar_->set_position(0);
            return true;
        case Key::End:
            scrollbar_->set_position(scrollbar_->max_position());
            return true;
        default:
            return false;
    }
}

bool TextView::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Wheel || scrollbar_ == nullptr) return false;
    if (event.button == MouseButton::WheelUp) {
        scrollbar_->set_position(scrollbar_->position() - 1);
        return true;
    }
    if (event.button == MouseButton::WheelDown) {
        scrollbar_->set_position(scrollbar_->position() + 1);
        return true;
    }
    return false;
}

void TextView::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(text_role_);
    const int visible_width = std::max(0, bounds().width - 1);
    const int top = top_line();
    for (int row = 0; row < bounds().height; ++row) {
        const std::size_t index = static_cast<std::size_t>(top + row);
        painter.fill(Rect{0, row, visible_width, 1}, Cell::from_grapheme(" ", style));
        if (index < lines_.size()) painter.draw_text(Point{0, row}, lines_[index], style);
    }
}

}  // namespace ckv::widgets
