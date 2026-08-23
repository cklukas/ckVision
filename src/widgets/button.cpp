// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/button.hpp"

#include "cvision/ui/application.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {

// How long a keyboard-activated button stays visibly depressed.
constexpr std::int64_t kKeyPressFeedbackNanos = 90'000'000;  // 90 ms

namespace {
bool contains(const Rect& r, Point p) noexcept {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}

// Enter or Space, the two keys that press a button. Modifiers are not
// consulted: they can change while the key is held, and the release must
// still find the press it belongs to.
bool activation_chord(const KeyChord& chord) noexcept {
    return chord.key == Key::Enter || (chord.key == Key::Char && chord.text == " ");
}
}  // namespace

Button::Button(std::string text) {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_text(std::move(text));
}

void Button::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.button.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.button.focused");
    if (default_role_ == ui::kInvalidRole) default_role_ = context().roles->find("ckv.button.default");
    if (shadow_role_ == ui::kInvalidRole) shadow_role_ = context().roles->find("ckv.button.shadow");
    if (pressed_role_ == ui::kInvalidRole) pressed_role_ = context().roles->find("ckv.button.pressed");
    if (hovered_role_ == ui::kInvalidRole) hovered_role_ = context().roles->find("ckv.button.hovered");
    if (mnemonic_role_ == ui::kInvalidRole) mnemonic_role_ = context().roles->find("ckv.label.mnemonic");
}

void Button::set_text(std::string text) {
    raw_text_ = std::move(text);
    const MnemonicText parsed = parse_mnemonic(raw_text_);
    display_text_ = parsed.display;
    // Width: one shadow-spacer column on the left, one shadow column on
    // the right, plus one cell of face margin on each side of the
    // label. Height: face row + shadow row — the classic button is two
    // rows tall by construction; the drop shadow IS part of the widget.
    set_preferred_size(Size{std::max(minimum_width_, text::text_width(display_text_) + 4), 2});
    invalidate();
    size_hint_changed();
}

void Button::set_minimum_width(int width) {
    minimum_width_ = std::max(3, width);
    set_preferred_size(Size{std::max(minimum_width_, text::text_width(display_text_) + 4), 2});
    invalidate();
    size_hint_changed();
}

bool Button::activate_mnemonic(std::string_view mnemonic) {
    const MnemonicText parsed = parse_mnemonic(raw_text_);
    if (parsed.mnemonic.empty() || parsed.mnemonic.size() != mnemonic.size()) return false;
    for (std::size_t i = 0; i < mnemonic.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(parsed.mnemonic[i])) !=
            std::tolower(static_cast<unsigned char>(mnemonic[i])))
            return false;
    }
    fire_press();
    return true;
}

// ckVision's classic button drawing contract: face rows y = 0..h-2,
// shadow row y = h-1.
//
// Not pressed:            Pressed (face shifts right, shadow gone):
//   .FFFFFFFF▄              ..FFFFFFFF
//   .FFFFFFFF█              ..FFFFFFFF
//   ..▀▀▀▀▀▀▀▀              ..........
//
// '.' cells are drawn in the shadow style with a space glyph — their
// background matches the surrounding surface, so they read as empty
// margin, and the pressed state's one-cell right shift is what makes
// the button visibly sink into the surface.
void Button::set_flat(bool flat) {
    if (flat_ == flat) return;
    flat_ = flat;
    size_hint_changed();
    invalidate();
}

void Button::draw(scene::Painter& painter) {
    const int w = bounds().width;
    const int h = bounds().height;
    if (w < 1 || h < 1) return;
    if (flat_) {
        const ui::Theme& theme = *context().theme;
        const ui::RoleId role = face_role();
        const Style face = theme.resolve(role);
        painter.fill(Rect{0, 0, w, h}, Cell::from_grapheme(" ", face));
        const auto parsed = parse_mnemonic(raw_text_);
        const int label_x = std::max(0, (w - text::text_width(parsed.display)) / 2);
        draw_mnemonic(painter, Point{label_x, h / 2}, parsed, w - label_x, face,
                      accent_style(face, theme.resolve(mnemonic_role_)));
        return;
    }
    if (w < 3) return;
    const int s = w - 1;  // the shadow column (right edge)

    const ui::Theme& theme = *context().theme;
    // A shadowed button shows a press in its geometry rather than its
    // colour, so its face role skips the pressed state the flat one uses.
    const Style face =
        theme.resolve(has_focus_    ? focused_role_
                      : hovered()   ? hovered_role_
                      : is_default_ ? default_role_
                                    : normal_role_);
    const Style shadow = theme.resolve(shadow_role_);

    const int face_rows = std::max(1, h - 1);
    const int label_indent = pressed_ ? 2 : 1;

    for (int y = 0; y < face_rows; ++y) {
        painter.fill(Rect{0, y, w, 1}, Cell::from_grapheme(" ", face));
        painter.draw_text(Point{0, y}, " ", shadow);
        if (pressed_) {
            painter.draw_text(Point{1, y}, " ", shadow);
        } else {
            painter.draw_text(Point{s, y}, y == 0 ? "▄" : "█", shadow);
        }
    }

    // Label centered on the middle face row, nudged right when pressed.
    const int label_width = text::text_width(display_text_);
    const int centered = (w - label_width) / 2;
    const int label_x = std::max(label_indent, centered + (pressed_ ? 1 : 0));
    const int available = std::max(0, (pressed_ ? w : s) - label_x);
    draw_mnemonic(painter, Point{label_x, face_rows / 2}, parse_mnemonic(raw_text_), available, face,
                  accent_style(face, theme.resolve(mnemonic_role_)));

    // The bottom shadow row: two spacer cells, then the "▀" run under
    // the face. A pressed button has no shadow at all — the whole row
    // becomes surface-colored spacers.
    if (h >= 2) {
        painter.fill(Rect{0, h - 1, w, 1}, Cell::from_grapheme(" ", shadow));
        if (!pressed_) {
            for (int x = 2; x <= s; ++x) painter.draw_text(Point{x, h - 1}, "▀", shadow);
        }
    }
}

SizeHint Button::horizontal_size_hint() const {
    // Flat, the label is the whole button: a stepper beside a field has no
    // room for the classic footprint, and padding it would push the field out
    // of the row it shares.
    if (flat_) {
        const int width = std::max(1, text::text_width(display_text_));
        return SizeHint{width, width, ui::kUnboundedExtent};
    }
    const int width = std::max(minimum_width_, text::text_width(display_text_) + 4);
    return SizeHint{width, width, width};
}

SizeHint Button::vertical_size_hint() const {
    if (flat_) return SizeHint{1, 1, 1};
    return SizeHint{1, 2, 2};
}

bool Button::on_key(const KeyEvent& event) {
    if (!activation_chord(event.chord)) {
        // Escape takes back a keyboard press in flight without firing —
        // and without also closing the dialog, which is not what a reader
        // half-way into a press they regret is asking for. The Escape is
        // consumed here exactly when it cancelled something.
        if (key_armed_ && event.chord.key == Key::Escape && event.action == KeyAction::Press) {
            key_armed_ = false;
            armed_ = false;
            pressed_ = false;
            invalidate();
            return true;
        }
        return false;
    }

    // Releases have their own route (on_key_release); one handed to this
    // handler directly is not a press and must not act like one.
    if (event.action == KeyAction::Release) return false;

    // Whether THIS key will report a release. It is per-event because one
    // session can mix encodings: only when the negotiated kitty
    // enhancements cover every key (D-055) does an Enter or Space press
    // promise the release that would commit it — under lesser sessions
    // those keys arrive as legacy bytes that never report one, and a
    // button that waited would hold itself down forever and never act.
    const bool reports_release = event.reports_release;

    // A held key under a release-reporting protocol repeats while it stays
    // down: that is one press being held, not many, so it must not re-fire.
    // A legacy terminal has no such state — its auto-repeat is literally a
    // fresh keystroke each time, and activating again is the correct
    // response to it.
    if (event.action == KeyAction::Repeat && reports_release) return true;

    pressed_ = true;
    armed_ = true;
    invalidate();
    if (reports_release) {
        // Stay depressed until the key comes back up; on_key_release is
        // what commits the press or finds it already taken back.
        key_armed_ = true;
        return true;
    }
    // No release will arrive: hold the depressed state briefly so the
    // keystroke is visibly acknowledged, then act. Without this a keyboard
    // press produces no feedback at all and an accepted keystroke looks
    // exactly like an ignored one.
    if (context().app != nullptr) {
        const std::weak_ptr<void> liveness = lifetime_token();
        context().app->start_timer(kKeyPressFeedbackNanos, /*repeating=*/false, [this, liveness] {
            if (liveness.expired()) return;
            pressed_ = false;
            invalidate();
        });
    }
    armed_ = false;
    fire_press();
    return true;
}

bool Button::on_key_release(const KeyEvent& event) {
    if (!activation_chord(event.chord) || !key_armed_) return false;
    const bool fire = armed_ && has_focus_;
    key_armed_ = false;
    armed_ = false;
    pressed_ = false;
    invalidate();
    // Releasing is what commits the press — and only if it was never taken
    // back in between (Tab away, Escape, or the terminal losing focus).
    if (fire) fire_press();
    return true;
}

bool Button::on_mouse(const MouseEvent& event) {
    const bool inside = contains(absolute_bounds(), event.cell);
    if (event.action == MouseAction::Down) {
        pressed_ = true;
        armed_ = true;
        invalidate();
        return true;
    }
    if (event.action == MouseAction::Move) {
        // Dragging off the button lifts it and disarms the press; dragging
        // back re-arms it. This is how a pointer press is taken back after
        // it has begun — the button has to show that state, or the reader
        // cannot tell whether releasing here will act.
        if (armed_ || pressed_) {
            const bool now_pressed = inside && armed_;
            if (now_pressed != pressed_) {
                pressed_ = now_pressed;
                invalidate();
            }
        }
        return true;  // still claim Move while a Down/Up pair is in flight
    }
    if (event.action == MouseAction::Up) {
        const bool fire = armed_ && inside;
        pressed_ = false;
        armed_ = false;
        invalidate();
        if (fire) fire_press();
        return true;
    }
    return false;
}

void Button::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    // Focus moving away (Tab, or anything else) takes back a keyboard press
    // in flight: the key that eventually comes up is no longer this
    // button's, so it must neither look pressed nor act.
    if (!event.gained && key_armed_) {
        key_armed_ = false;
        armed_ = false;
        pressed_ = false;
    }
    invalidate();
}

void Button::fire_press() {
    if (on_press) on_press();
}

}  // namespace ckv::widgets
