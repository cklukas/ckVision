// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/minimized_window_stub.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

namespace {
// Enter or Space, the two keys that press a control — the same pair Button
// answers to, so a stub reached by Tab behaves like every other focusable
// thing on the desktop.
bool activation_chord(const KeyChord& chord) noexcept {
    return chord.key == Key::Enter || (chord.key == Key::Char && chord.text == " ");
}
}  // namespace

MinimizedWindowStub::MinimizedWindowStub(Window& window) : window_(&window), title_(window.title()) {
    // Reachable by keyboard, because the mouse is not the only way a reader
    // arrives here and a window that can only be got back by pointer is only
    // half got back.
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{natural_width(), 1});
}

void MinimizedWindowStub::on_attached() {
    // The window's own inactive roles, not a set of this widget's own: a
    // parked window is a window that is not active, and a theme that
    // retints inactive frames retints these with them. There is no state in
    // which a stub is the active window — activating one restores it, and
    // the stub is gone before the activation lands.
    if (frame_role_ == ui::kInvalidRole) frame_role_ = context().roles->find("ckv.window.frame.inactive");
    if (title_role_ == ui::kInvalidRole) title_role_ = context().roles->find("ckv.window.title.inactive");
    if (control_role_ == ui::kInvalidRole) control_role_ = context().roles->find("ckv.window.control");
}

int MinimizedWindowStub::natural_width() const {
    return kChromeWidth + std::max(1, text::text_width(title_));
}

void MinimizedWindowStub::refresh() {
    if (window_ == nullptr) return;
    if (title_ == window_->title()) return;
    title_ = window_->title();
    set_preferred_size(Size{natural_width(), 1});
    invalidate();
    size_hint_changed();
}

ui::SizeHint MinimizedWindowStub::horizontal_size_hint() const {
    return ui::SizeHint{kChromeWidth + 1, natural_width(), natural_width()};
}

// Exactly one row: a rolled-up window is its top border and nothing else.
ui::SizeHint MinimizedWindowStub::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

int MinimizedWindowStub::restore_control_x() const noexcept { return bounds().width - 5; }

bool MinimizedWindowStub::point_in_close_control(Point local) const noexcept {
    if (local.y != 0 || bounds().width < kChromeWidth) return false;
    return local.x >= 2 && local.x <= 4;
}

bool MinimizedWindowStub::point_in_restore_control(Point local) const noexcept {
    if (local.y != 0 || bounds().width < kChromeWidth) return false;
    const int x = restore_control_x();
    return local.x >= x && local.x <= x + 2;
}

void MinimizedWindowStub::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    const Style frame = theme.resolve(frame_role_);
    const Style caption = theme.resolve(title_role_);
    // A control contributes its foreground and attributes only, over the
    // frame's own background — Window::draw's rule, for Window::draw's
    // reason: a mark that brought its own background would punch a hole in
    // the border it sits in.
    const Style control_style = theme.resolve(control_role_);
    const Style control{control_style.fg, frame.bg, frame.attrs | control_style.attrs};

    const int width = bounds().width;
    if (width <= 0) return;
    painter.fill(Rect{0, 0, width, 1}, Cell::from_grapheme("─", frame));
    painter.draw_text(Point{0, 0}, "┌", frame);
    painter.draw_text(Point{width - 1, 0}, "┐", frame);
    if (width < kChromeWidth) return;

    painter.draw_text(Point{2, 0}, "[", frame);
    painter.draw_text(Point{3, 0}, "■", control);
    painter.draw_text(Point{4, 0}, "]", frame);

    // U+2191 UPWARDS ARROW: the frame's own maximize glyph, which on a
    // window that is nowhere on screen says the only thing it can say —
    // bring this back up. `↕` is not used here: that one means "restore
    // from maximized", a question about SIZE, and a parked window's size is
    // whatever it was and is not what this control changes.
    const int restore_x = restore_control_x();
    painter.draw_text(Point{restore_x, 0}, "[", frame);
    painter.draw_text(Point{restore_x + 1, 0}, "↑", control);
    painter.draw_text(Point{restore_x + 2, 0}, "]", frame);

    // Centred between the two controls, with a padding space each side —
    // the frame centres its caption, and a stub that left-aligned would read
    // as a different kind of thing parked under windows that do not.
    const int available = std::max(0, width - kChromeWidth);
    const std::string shown = text::elide_to_width(title_, available);
    if (shown.empty()) return;
    const int shown_width = text::text_width(shown);
    const int start = std::clamp((width - shown_width) / 2, 6, std::max(6, restore_x - 1 - shown_width));
    painter.draw_text(Point{start - 1, 0}, " ", frame);
    painter.draw_text(Point{start, 0}, shown, caption);
    painter.draw_text(Point{start + shown_width, 0}, " ", frame);
}

// Both handlers END this stub: restoring and closing each take the window
// out of the parked set, and the Desktop deletes the stub as part of that.
// So the callable is COPIED before it runs and nothing on `this` is touched
// afterwards — the copy is what keeps the lambda alive while it destroys the
// std::function it was stored in. (The event router already re-resolves its
// target through a liveness handle, so a view dying inside its own handler
// is a case the dispatch itself is built for.)
bool MinimizedWindowStub::on_mouse(const MouseEvent& event) {
    if (!enabled() || event.button != MouseButton::Left || event.action != MouseAction::Down) return false;
    const Rect absolute = absolute_bounds();
    const Point local{event.cell.x - absolute.x, event.cell.y - absolute.y};
    if (point_in_close_control(local)) {
        const std::function<void()> close = on_close;
        if (close) close();
        return true;
    }
    // Everything else on the stub is the same request as its restore
    // control. A reader who clicks the caption of a window they want back is
    // asking for it as plainly as one who aims at the arrow, and there is no
    // second thing a one-row frame could have meant.
    const std::function<void()> restore = on_restore;
    if (restore) restore();
    return true;
}

bool MinimizedWindowStub::on_key(const KeyEvent& event) {
    if (!enabled() || !activation_chord(event.chord)) return false;
    const std::function<void()> restore = on_restore;
    if (restore) restore();
    return true;
}

}  // namespace ckv::widgets
