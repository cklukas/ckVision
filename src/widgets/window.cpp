// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/window.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"
#include "cvision/scene/box_drawing.hpp"

namespace ckv::widgets {

namespace {
bool in_range(int v, int begin, int end_exclusive) noexcept { return v >= begin && v < end_exclusive; }
}  // namespace

Window::Window(std::string title, const ui::Theme& theme, ui::RoleId frame_active_role,
               ui::RoleId frame_inactive_role, ui::RoleId title_active_role, ui::RoleId title_inactive_role)
    : title_(std::move(title)),
      theme_(theme),
      frame_active_role_(frame_active_role),
      frame_inactive_role_(frame_inactive_role),
      title_active_role_(title_active_role),
      title_inactive_role_(title_inactive_role) {}

void Window::set_title(std::string title) {
    title_ = std::move(title);
    invalidate();
}

void Window::set_active(bool active) {
    if (active == active_) return;
    active_ = active;
    invalidate();
}

std::unique_ptr<ui::View> Window::set_content(std::unique_ptr<ui::View> content) {
    std::unique_ptr<ui::View> previous = content_ != nullptr ? remove_child(content_) : nullptr;
    content_ = nullptr;
    if (content != nullptr) {
        content_ = add_child(std::move(content));
        content_->set_bounds(content_rect());
    }
    return previous;
}

void Window::set_min_size(Size min) noexcept {
    min_size_ = min;
    set_bounds(clamp_size(bounds()));
}

void Window::set_max_size(Size max) noexcept {
    max_size_ = max;
    set_bounds(clamp_size(bounds()));
}

bool Window::close() {
    if (close_request && !close_request()) return false;  // vetoed
    if (on_closed) on_closed();
    return true;
}

void Window::toggle_zoom(Rect available) {
    if (zoomed_) {
        set_bounds(restored_bounds_);
        zoomed_ = false;
    } else {
        restored_bounds_ = bounds();
        set_bounds(available);
        zoomed_ = true;
    }
}

void Window::enter_move_mode() {
    keyboard_mode_ = KeyboardMode::Move;
    keyboard_mode_start_bounds_ = bounds();
}

void Window::enter_resize_mode() {
    keyboard_mode_ = KeyboardMode::Resize;
    keyboard_mode_start_bounds_ = bounds();
}

Rect Window::content_rect() const noexcept {
    const Rect b = bounds();
    if (b.width <= 2 || b.height <= 2) return Rect{1, 1, 0, 0};
    return Rect{1, 1, b.width - 2, b.height - 2};
}

Rect Window::clamp_size(Rect b) const noexcept {
    int width = std::max(b.width, min_size_.width);
    int height = std::max(b.height, min_size_.height);
    if (max_size_.width > 0) width = std::min(width, max_size_.width);
    if (max_size_.height > 0) height = std::min(height, max_size_.height);
    return Rect{b.x, b.y, width, height};
}

void Window::on_resized() {
    if (content_ != nullptr) content_->set_bounds(content_rect());
}

bool Window::point_in_close_control(Point local) const noexcept {
    return local.y == 0 && in_range(local.x, 1, 4);  // "[x]" at columns 1-3
}

bool Window::point_in_zoom_control(Point local) const noexcept {
    const int w = bounds().width;
    return local.y == 0 && in_range(local.x, w - 3, w - 1);  // "[]" at the last two columns before the corner
}

bool Window::point_in_resize_grip(Point local) const noexcept {
    return resizable_ && local.x == bounds().width - 1 && local.y == bounds().height - 1;
}

void Window::draw(scene::Painter& painter) {
    const Rect b = Rect{0, 0, bounds().width, bounds().height};
    const ui::RoleId frame_role = active_ ? frame_active_role_ : frame_inactive_role_;
    const ui::RoleId title_role = active_ ? title_active_role_ : title_inactive_role_;
    const Style frame_style = theme_.resolve(frame_role);
    const Style title_style = theme_.resolve(title_role);

    painter.draw_box(b, scene::LineStyle::Single, frame_style);
    if (b.width > 6) {
        painter.draw_text(Point{1, 0}, "[x]", frame_style);
        painter.draw_text(Point{b.width - 3, 0}, "[]", frame_style);
    }
    if (b.width > 8) {
        const int available = b.width - 8;
        std::string shown = title_;
        if (static_cast<int>(shown.size()) > available) shown.resize(static_cast<std::size_t>(available));
        painter.draw_text(Point{4, 0}, shown, title_style);
    }
}

bool Window::on_key(const KeyEvent& event) {
    if (keyboard_mode_ == KeyboardMode::None) {
        if (event.chord.key == Key::Enter && accept_request) {
            accept_request();
            return true;
        }
        if (event.chord.key == Key::Escape && cancel_request) {
            cancel_request();
            return true;
        }
        return false;
    }

    const bool is_resize = keyboard_mode_ == KeyboardMode::Resize;
    switch (event.chord.key) {
        case Key::Left:
            set_bounds(clamp_size(is_resize ? Rect{bounds().x, bounds().y, bounds().width - 1, bounds().height}
                                             : Rect{bounds().x - 1, bounds().y, bounds().width, bounds().height}));
            return true;
        case Key::Right:
            set_bounds(clamp_size(is_resize ? Rect{bounds().x, bounds().y, bounds().width + 1, bounds().height}
                                             : Rect{bounds().x + 1, bounds().y, bounds().width, bounds().height}));
            return true;
        case Key::Up:
            set_bounds(clamp_size(is_resize ? Rect{bounds().x, bounds().y, bounds().width, bounds().height - 1}
                                             : Rect{bounds().x, bounds().y - 1, bounds().width, bounds().height}));
            return true;
        case Key::Down:
            set_bounds(clamp_size(is_resize ? Rect{bounds().x, bounds().y, bounds().width, bounds().height + 1}
                                             : Rect{bounds().x, bounds().y + 1, bounds().width, bounds().height}));
            return true;
        case Key::Enter:
            keyboard_mode_ = KeyboardMode::None;
            return true;
        case Key::Escape:
            set_bounds(keyboard_mode_start_bounds_);
            keyboard_mode_ = KeyboardMode::None;
            return true;
        default:
            return false;
    }
}

bool Window::on_mouse(const MouseEvent& event) {
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};

    if (event.action == MouseAction::Down) {
        if (point_in_close_control(local)) {
            close();
            return true;
        }
        if (point_in_zoom_control(local)) {
            toggle_zoom(bounds());  // standalone default: no Desktop to supply a real "available" rect
            return true;
        }
        if (point_in_resize_grip(local)) {
            drag_kind_ = DragKind::Resize;
            drag_start_mouse_ = event.cell;
            drag_start_bounds_ = bounds();
            return true;
        }
        if (movable_ && local.y == 0) {
            drag_kind_ = DragKind::Move;
            drag_start_mouse_ = event.cell;
            drag_start_bounds_ = bounds();
            return true;
        }
        return false;  // not frame chrome — let a child (content) handle it
    }

    if (drag_kind_ == DragKind::None) return false;

    if (event.action == MouseAction::Move) {
        const int dx = event.cell.x - drag_start_mouse_.x;
        const int dy = event.cell.y - drag_start_mouse_.y;
        if (drag_kind_ == DragKind::Move) {
            set_bounds(Rect{drag_start_bounds_.x + dx, drag_start_bounds_.y + dy, drag_start_bounds_.width,
                             drag_start_bounds_.height});
        } else {
            set_bounds(clamp_size(Rect{drag_start_bounds_.x, drag_start_bounds_.y, drag_start_bounds_.width + dx,
                                        drag_start_bounds_.height + dy}));
        }
        return true;
    }
    if (event.action == MouseAction::Up) {
        drag_kind_ = DragKind::None;
        return true;
    }
    return false;
}

}  // namespace ckv::widgets
