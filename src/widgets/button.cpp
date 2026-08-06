// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/button.hpp"

#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

namespace {
bool contains(const Rect& r, Point p) noexcept {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}
}  // namespace

Button::Button(std::string text, const ui::Theme& theme, ui::RoleId normal_role, ui::RoleId focused_role,
               ui::RoleId default_role)
    : theme_(theme), normal_role_(normal_role), focused_role_(focused_role), default_role_(default_role) {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_text(std::move(text));
}

void Button::set_text(std::string text) {
    raw_text_ = std::move(text);
    const MnemonicText parsed = parse_mnemonic(raw_text_);
    display_text_ = parsed.display;
    // A button's preferred width includes the classic "[ Label ]"
    // bracketing so a Row/Column laying it out gets its true footprint.
    set_preferred_size(Size{text::text_width(display_text_) + 4, 1});
    invalidate();
}

void Button::draw(scene::Painter& painter) {
    const ui::RoleId role = has_focus_ ? focused_role_ : (is_default_ ? default_role_ : normal_role_);
    const Style style = theme_.resolve(role);
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", style));
    const std::string label = pressed_ ? display_text_ : ("[ " + display_text_ + " ]");
    painter.draw_text(Point{0, 0}, label, style);
}

SizeHint Button::horizontal_size_hint() const {
    const int width = text::text_width(display_text_) + 4;
    return SizeHint{width, width, width};
}

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
