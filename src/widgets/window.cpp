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

std::unique_ptr<ui::View> Window::set_frame_overlay(std::unique_ptr<ui::View> overlay) {
    std::unique_ptr<ui::View> previous = frame_overlay_ != nullptr ? remove_child(frame_overlay_) : nullptr;
    frame_overlay_ = nullptr;
    if (overlay != nullptr) {
        frame_overlay_ = add_child(std::move(overlay));
        frame_overlay_->set_bounds(frame_overlay_rect());
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
        set_bounds(clamp_size(available));  // zooming must still honor min/max, like every other resize path
        zoomed_ = true;
    }
}

void Window::reposition_within(Rect available) noexcept {
    const Rect b = bounds();
    // Shrink to fit `available` — but never below min_size_: a window
    // smaller than its own declared minimum isn't usefully reachable
    // either, so min_size_ wins over "fits available" when the two
    // conflict (a desktop shrunk smaller than a window's own minimum).
    const int width = std::min(b.width, std::max(available.width, min_size_.width));
    const int height = std::min(b.height, std::max(available.height, min_size_.height));
    const Rect sized = clamp_size(Rect{b.x, b.y, width, height});

    // Reposition so no edge falls outside `available` when the window
    // does fit; when it doesn't (min_size_ > available), pin to
    // available's own origin so the title bar/close control stay at a
    // predictable, reachable spot rather than wherever a prior larger
    // desktop happened to leave it.
    const int x = std::clamp(sized.x, available.x, available.x + std::max(0, available.width - sized.width));
    const int y = std::clamp(sized.y, available.y, available.y + std::max(0, available.height - sized.height));
    set_bounds(Rect{x, y, sized.width, sized.height});
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

Rect Window::frame_overlay_rect() const noexcept {
    if (frame_overlay_ == nullptr) return Rect{0, 0, 0, 0};
    const Rect b = bounds();
    if (b.height <= 2 || b.width <= 4) return Rect{0, 0, 0, 0};  // frame too small to host it at all
    // Right-aligned on the bottom border, 2 cells in from each corner
    // (leaving the resize grip's own corner cell untouched and mirroring
    // the close/zoom controls' own inset on the top border) — never
    // wider than fits between those margins.
    const int max_width = std::max(1, b.width - 4);
    const int width = std::min(std::max(1, frame_overlay_->horizontal_size_hint().preferred), max_width);
    return Rect{b.width - 2 - width, b.height - 1, width, 1};
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
    if (frame_overlay_ != nullptr) frame_overlay_->set_bounds(frame_overlay_rect());
}

void Window::relayout_frame_overlay() noexcept {
    if (frame_overlay_ != nullptr) frame_overlay_->set_bounds(frame_overlay_rect());
}

bool Window::point_in_close_control(Point local) const noexcept {
    // Must match draw()'s own "b.width > 6" gate — a control that
    // isn't drawn must not be clickable, and at widths where only the
    // close control is drawn, its region must not extend into where
    // the (undrawn) zoom control's columns would otherwise be.
    if (bounds().width <= 6) return false;
    return local.y == 0 && in_range(local.x, 1, 4);  // "[x]" at columns 1-3
}

bool Window::point_in_zoom_control(Point local) const noexcept {
    if (bounds().width <= 8) return false;  // must match draw()'s own "b.width > 8" gate
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

    // The interior fills with the SAME style as the frame — matching
    // the convention's own "the window's palette is uniform across
    // frame and content unless a child widget paints over it," not
    // just the border. Without this, any content cell a child widget
    // doesn't happen to cover shows raw, unstyled Surface fill — a
    // window with sparse content (a couple of labels and buttons, not
    // a full-bleed panel) previously showed a visibly blank patch
    // instead of reading as one solid window.
    painter.fill(content_rect(), Cell::from_grapheme(" ", frame_style));

    // Active windows get a DOUBLE-line frame, inactive ones single —
    // the classic windowed-desktop convention for "this is the one
    // that has focus," and (unlike relying on color alone) still
    // legible on a monochrome terminal.
    painter.draw_box(b, active_ ? scene::LineStyle::Double : scene::LineStyle::Single, frame_style);
    if (b.width > 6) {
        // U+25A0 BLACK SQUARE for close, matching the convention's
        // close-control glyph (not "x" — a plain filled square reads
        // as a "control" the way "x" doesn't, and needs no ASCII
        // fallback since box-drawing-adjacent glyphs are already
        // required for the frame itself).
        painter.draw_text(Point{1, 0}, "[■]", frame_style);
        // Up-arrow "maximize"; changes to an up/down arrow once
        // zoomed, matching the convention's own "toggle" affordance —
        // previously this drew empty brackets with no glyph at all.
        // b.width - 4, not - 3: a 3-glyph "[X]" ending at b.width - 3
        // would land its last character ON the frame's own top-right
        // corner column (b.width - 1) — this must end one column short
        // of the corner, matching the close control's own symmetric
        // margin on the left.
        painter.draw_text(Point{b.width - 4, 0}, zoomed_ ? "[↕]" : "[↑]", frame_style);
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

void schedule_self_detach(Window& window, ui::Application& app) {
    app.post([&window]() {
        // detach_child(), not remove_child(): if `parent` is a Desktop
        // and `window` was added via add_window(), plain remove_child
        // would silently leave a dangling pointer in Desktop's own
        // windows_ bookkeeping — detach_child() is the polymorphic
        // entry point that gets this right regardless of what kind of
        // View actually parents this window.
        if (ui::View* parent = window.parent()) parent->detach_child(&window);
    });
}

}  // namespace ckv::widgets
