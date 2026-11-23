// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/check_box.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "widgets/mnemonic_internal.hpp"

namespace ckv::widgets {

CheckBox::CheckBox(std::string label, bool checked)
    : ui::View(Rect{}, ui::FocusPolicy::TabStop),
      label_(std::move(label)),
      parsed_(parse_mnemonic(label_)),
      checked_(checked) {}

void CheckBox::set_label(std::string label) {
    if (label_ == label) return;
    label_ = std::move(label);
    parsed_ = parse_mnemonic(label_);
    size_hint_changed();
    invalidate();
}

void CheckBox::set_checked(bool checked) {
    if (checked_ == checked) return;
    checked_ = checked;
    invalidate();
    const auto cb = on_change;
    if (cb) cb(checked_);
}

void CheckBox::toggle() { set_checked(!checked_); }

void CheckBox::on_attached() {
    normal_role_ = context().roles->find("ckv.option.normal");
    focused_role_ = context().roles->find("ckv.option.focused");
    disabled_role_ = context().roles->find("ckv.option.disabled");
    mnemonic_role_ = context().roles->find("ckv.label.mnemonic");
    if (accessible_info().role == ui::AccessibleRole::None) set_accessible_role(ui::AccessibleRole::CheckBox);
    if (accessible_info().name.empty()) set_accessible_name(parsed_.display);
}

void CheckBox::draw(scene::Painter& painter) {
    const int w = bounds().width;
    const int h = bounds().height;
    if (w <= 0 || h <= 0) return;
    const bool enabled = enabled_in_tree();
    const ui::RoleId active_normal =
        role_override_ != ui::kInvalidRole ? role_override_ : normal_role_;
    Style face = !enabled ? context().theme->resolve(disabled_role_)
                          : (has_focus() ? context().theme->resolve(focused_role_)
                                         : context().theme->resolve(active_normal));
    if (enabled && hovered() && !has_focus()) face.attrs |= Attr::Underline;
    painter.fill(Rect{0, 0, w, h}, Cell::from_grapheme(" ", face));
    const std::string prefix = checked_ ? "[X] " : "[ ] ";
    painter.draw_text(Point{0, 0}, prefix, face);
    if (w > 4 && !parsed_.display.empty()) {
        auto clipped = painter.clipped(Rect{4, 0, w - 4, 1});
        clipped.draw_text(Point{4, 0}, parsed_.display, face);
        if (enabled && !parsed_.mnemonic.empty() &&
            parsed_.mnemonic_byte_offset < parsed_.display.size()) {
            const Style accent = accent_style(face, context().theme->resolve(mnemonic_role_));
            const int offset = text::text_width(
                std::string_view(parsed_.display).substr(0, parsed_.mnemonic_byte_offset));
            clipped.draw_text(Point{4 + offset, 0}, parsed_.mnemonic, accent);
        }
    }
}

bool CheckBox::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || event.action == KeyAction::Release) return false;
    if (event.chord.key == Key::Enter ||
        (event.chord.key == Key::Char && event.chord.modifiers == Modifier::None &&
         event.chord.text == " ")) {
        toggle();
        return true;
    }
    return false;
}

bool CheckBox::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || event.action != MouseAction::Down ||
        event.button != MouseButton::Left) {
        return false;
    }
    const Rect abs = absolute_bounds();
    if (!abs.contains(event.cell)) return false;
    toggle();
    return true;
}

void CheckBox::on_focus(const FocusEvent&) { invalidate(); }

void CheckBox::on_hover_changed(bool) { invalidate(); }

void CheckBox::set_role_override(ui::RoleId role) {
    if (role_override_ == role) return;
    role_override_ = role;
    invalidate();
}

std::optional<PointerShape> CheckBox::pointer_shape_at(Point local) const {
    if (!enabled_in_tree() || !Rect{0, 0, bounds().width, bounds().height}.contains(local))
        return std::nullopt;
    return PointerShape::Pointer;
}

ui::SizeHint CheckBox::horizontal_size_hint() const {
    const int width = 4 + text::text_width(parsed_.display);
    return ui::SizeHint{std::min(4, width), width, width};
}

ui::SizeHint CheckBox::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

}  // namespace ckv::widgets
