// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/button.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

namespace {
bool contains(const Rect& r, Point p) noexcept {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
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
}

void Button::set_text(std::string text) {
    raw_text_ = std::move(text);
    const MnemonicText parsed = parse_mnemonic(raw_text_);
    display_text_ = parsed.display;
    // Width: one shadow-spacer column on the left, one shadow column on
    // the right, plus one cell of face margin on each side of the
    // label. Height: face row + shadow row — the classic button is two
    // rows tall by construction; the drop shadow IS part of the widget.
    set_preferred_size(Size{text::text_width(display_text_) + 4, 2});
    invalidate();
    size_hint_changed();
}

// The classic button drawing algorithm, cell for cell (verified
// against the reference implementation's drawState): face rows
// y = 0..h-2, shadow row y = h-1.
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
void Button::draw(scene::Painter& painter) {
    const int w = bounds().width;
    const int h = bounds().height;
    if (w < 3 || h < 1) return;
    const int s = w - 1;  // the shadow column (right edge)

    const ui::RoleId face_role = has_focus_ ? focused_role_ : (is_default_ ? default_role_ : normal_role_);
    const ui::Theme& theme = *context().theme;
    const Style face = theme.resolve(face_role);
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
    std::string shown = display_text_;
    const int available = std::max(0, (pressed_ ? w : s) - label_x);
    if (label_width > available) shown.resize(static_cast<std::size_t>(available));
    painter.draw_text(Point{label_x, face_rows / 2}, shown, face);

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
    const int width = text::text_width(display_text_) + 4;
    return SizeHint{width, width, width};
}

SizeHint Button::vertical_size_hint() const { return SizeHint{1, 2, 2}; }

bool Button::on_key(const KeyEvent& event) {
    if (event.chord.key == Key::Enter || (event.chord.key == Key::Char && event.chord.text == " ")) {
        fire_press();
        return true;
    }
    return false;
}

bool Button::on_mouse(const MouseEvent& event) {
    if (event.action == MouseAction::Down) {
        pressed_ = true;
        invalidate();
        return true;
    }
    if (event.action == MouseAction::Up) {
        const bool released_inside = contains(absolute_bounds(), event.cell);
        pressed_ = false;
        invalidate();
        if (released_inside) fire_press();
        return true;
    }
    return event.action == MouseAction::Move;  // still claim Move while a Down/Up pair is in flight
}

void Button::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    invalidate();
}

void Button::fire_press() {
    if (on_press) on_press();
}

}  // namespace ckv::widgets
