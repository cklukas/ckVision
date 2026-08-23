// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/minimized_window_stub.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>

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
    if (control_pressed_role_ == ui::kInvalidRole)
        control_pressed_role_ = context().roles->find("ckv.window.control.pressed");
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
    // The same pressed face the window frame's controls wear while a press is
    // held over them — and only while it is over them, so a pointer that has
    // moved away shows the reader the press is no longer going to count.
    const Style pressed = theme.resolve(control_pressed_role_);

    const int width = bounds().width;
    if (width <= 0) return;
    painter.fill(Rect{0, 0, width, 1}, Cell::from_grapheme("─", frame));
    painter.draw_text(Point{0, 0}, "┌", frame);
    painter.draw_text(Point{width - 1, 0}, "┐", frame);
    if (width < kChromeWidth) return;

    // Brackets and glyph alike, as Window draws its frame controls: the whole
    // three-cell control is the thing that is pressed.
    const bool close_armed = held_ == Held::Close && held_inside_;
    painter.draw_text(Point{2, 0}, "[", close_armed ? pressed : frame);
    painter.draw_text(Point{3, 0}, "■", close_armed ? pressed : control);
    painter.draw_text(Point{4, 0}, "]", close_armed ? pressed : frame);

    // U+2191 UPWARDS ARROW: the frame's own maximize glyph, which on a
    // window that is nowhere on screen says the only thing it can say —
    // bring this back up. `↕` is not used here: that one means "restore
    // from maximized", a question about SIZE, and a parked window's size is
    // whatever it was and is not what this control changes.
    const int restore_x = restore_control_x();
    const bool restore_armed = (held_ == Held::Restore && held_inside_) || key_armed_ || key_flash_;
    painter.draw_text(Point{restore_x, 0}, "[", restore_armed ? pressed : frame);
    painter.draw_text(Point{restore_x + 1, 0}, "↑", restore_armed ? pressed : control);
    painter.draw_text(Point{restore_x + 2, 0}, "]", restore_armed ? pressed : frame);

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
//
// And neither runs on the press. The press ARMS a control and the release
// DECIDES it, the way Window::on_mouse treats the frame's own controls: a
// reader may go down on the close mark, think better of it, and slide off
// before letting go — and a row that had already closed the window on the
// way down had taken that choice away. Everything on the row that is not the
// close control is the restore control: a reader who clicks the caption of a
// window they want back is asking for it as plainly as one who aims at the
// arrow, and there is no second thing a one-row frame could have meant.
bool MinimizedWindowStub::on_mouse(const MouseEvent& event) {
    if (!enabled()) return false;
    const Rect absolute = absolute_bounds();
    const Point local{event.cell.x - absolute.x, event.cell.y - absolute.y};
    const auto over_control = [&](Held control) {
        const bool on_row = local.y == 0 && local.x >= 0 && local.x < bounds().width;
        switch (control) {
            case Held::Close: return point_in_close_control(local);
            case Held::Restore: return on_row && !point_in_close_control(local);
            case Held::None: break;
        }
        return false;
    };

    if (held_ != Held::None) {
        // Motion with no button held: the release happened where this row
        // could not see it, and the pointer coming back is the proof. A press
        // nobody is holding is not armed.
        if (event.action == MouseAction::Move && event.button == MouseButton::None) {
            held_ = Held::None;
            held_inside_ = true;
            invalidate();
            return false;
        }
        const bool over = over_control(held_);
        if (event.action != MouseAction::Up) {
            // Held: nothing is decided, but say whether it still would be.
            if (over != held_inside_) {
                held_inside_ = over;
                invalidate();
            }
            return true;
        }
        const Held held = held_;
        held_ = Held::None;
        held_inside_ = true;
        invalidate();
        // Released off what it went down on: the reader moved away, and
        // moving away is how a press is taken back.
        if (!over) return true;
        if (held == Held::Close) {
            const std::function<void()> close = on_close;
            if (close) close();
            return true;
        }
        const std::function<void()> restore = on_restore;
        if (restore) restore();
        return true;
    }

    if (event.button != MouseButton::Left || event.action != MouseAction::Down) return false;
    held_ = point_in_close_control(local) ? Held::Close : Held::Restore;
    held_inside_ = true;
    invalidate();
    return true;
}

namespace {
// How long a keyboard press that cannot report its release stays visibly
// down before it acts — Button's figure, for the same reason.
constexpr std::int64_t kKeyPressFeedbackNanos = 90'000'000;
}  // namespace

bool MinimizedWindowStub::on_key(const KeyEvent& event) {
    if (!enabled()) return false;
    if (!activation_chord(event.chord)) {
        // Escape takes back a keyboard press in flight without restoring —
        // consumed here exactly when it cancelled something.
        if (key_armed_ && event.chord.key == Key::Escape && event.action == KeyAction::Press) {
            key_armed_ = false;
            invalidate();
            return true;
        }
        return false;
    }
    // Releases have their own route (on_key_release); one handed here is not
    // a press and must not act like one.
    if (event.action == KeyAction::Release) return false;
    // A held key under a release-reporting protocol repeats while it stays
    // down: one press being held, not many. A legacy terminal's auto-repeat
    // is a fresh keystroke each time, and acting again is right for it.
    if (event.action == KeyAction::Repeat && event.reports_release) return true;

    if (event.reports_release) {
        // Stay armed until the key comes back up; on_key_release commits the
        // press, or finds it already taken back.
        key_armed_ = true;
        invalidate();
        return true;
    }
    // No release will arrive: acknowledge the keystroke visibly, then act.
    // The flash outlives this stub only as a timer on the Application, and
    // the liveness token keeps it from touching a row that restoring took
    // off the desktop — the timer simply finds nobody home.
    key_flash_ = true;
    invalidate();
    if (context().app != nullptr) {
        const std::weak_ptr<void> liveness = lifetime_token();
        context().app->start_timer(kKeyPressFeedbackNanos, /*repeating=*/false, [this, liveness] {
            if (liveness.expired()) return;
            key_flash_ = false;
            invalidate();
        });
    }
    // Ends this stub — see on_mouse for why the callable is copied and
    // nothing on `this` is touched afterwards.
    const std::function<void()> restore = on_restore;
    if (restore) restore();
    return true;
}

bool MinimizedWindowStub::on_key_release(const KeyEvent& event) {
    if (!activation_chord(event.chord) || !key_armed_) return false;
    key_armed_ = false;
    invalidate();
    // Releasing is what commits the press — and it was never taken back in
    // between (Escape, or focus moving away), or key_armed_ would be false.
    const std::function<void()> restore = on_restore;
    if (restore) restore();
    return true;
}

void MinimizedWindowStub::on_focus(const FocusEvent& event) {
    // Focus moving away takes back a keyboard press in flight: the key that
    // eventually comes up is no longer this row's, so it must neither look
    // pressed nor act.
    if (!event.gained && key_armed_) key_armed_ = false;
    invalidate();
}

}  // namespace ckv::widgets
