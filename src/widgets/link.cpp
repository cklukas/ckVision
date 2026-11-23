// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/link.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/layout.hpp"

namespace ckv::widgets {

Link::Link(std::string label, std::string url)
    : ui::View(Rect{}, ui::FocusPolicy::TabStop),
      label_(std::move(label)),
      url_(std::move(url)) {}

void Link::set_label(std::string label) {
    if (label_ == label) return;
    label_ = std::move(label);
    size_hint_changed();
    invalidate();
}

void Link::set_url(std::string url) {
    if (url_ == url) return;
    url_ = std::move(url);
    invalidate();
}

void Link::set_role_override(ui::RoleId role) {
    if (role_override_ == role) return;
    role_override_ = role;
    invalidate();
}

void Link::on_attached() {
    normal_role_ = context().roles->find("ckv.hotkey");
    focused_role_ = context().roles->find("ckv.option.focused");
    disabled_role_ = context().roles->find("ckv.label.disabled");
    if (accessible_info().role == ui::AccessibleRole::None) set_accessible_role(ui::AccessibleRole::Link);
    if (accessible_info().name.empty()) set_accessible_name(label_);
}

void Link::draw(scene::Painter& painter) {
    const int w = bounds().width;
    const int h = bounds().height;
    if (w <= 0 || h <= 0) return;
    const bool enabled = enabled_in_tree();
    const ui::RoleId normal = role_override_ != ui::kInvalidRole ? role_override_ : normal_role_;
    Style face = !enabled ? context().theme->resolve(disabled_role_)
                          : (has_focus() ? context().theme->resolve(focused_role_)
                                         : context().theme->resolve(normal));
    if (enabled) face.attrs |= Attr::Underline;
    if (enabled && hovered()) face.attrs |= Attr::Bold;
    painter.fill(Rect{0, 0, w, h}, Cell::from_grapheme(" ", face));
    painter.draw_text(Point{0, 0}, text::clip_to_width_view(label_, w), face, url_);
}

bool Link::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || event.action == KeyAction::Release) return false;
    if (event.chord.key == Key::Enter ||
        (event.chord.key == Key::Char && event.chord.modifiers == Modifier::None &&
         event.chord.text == " ")) {
        const auto cb = on_activate;
        if (cb) cb();
        return true;
    }
    return false;
}

bool Link::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree()) {
        pressed_ = false;
        return false;
    }
    if (event.action == MouseAction::Down && event.button == MouseButton::Left) {
        if (!absolute_bounds().contains(event.cell)) return false;
        pressed_ = true;
        return true;
    }
    if (event.action == MouseAction::Up && pressed_) {
        pressed_ = false;
        if (absolute_bounds().contains(event.cell)) {
            const auto cb = on_activate;
            if (cb) cb();
        }
        return true;
    }
    return false;
}

void Link::on_focus(const FocusEvent&) { invalidate(); }
void Link::on_hover_changed(bool) { invalidate(); }

std::optional<PointerShape> Link::pointer_shape_at(Point local) const {
    if (!enabled_in_tree() || !Rect{0, 0, bounds().width, bounds().height}.contains(local))
        return std::nullopt;
    return PointerShape::Pointer;
}

ui::SizeHint Link::horizontal_size_hint() const {
    const int w = text::text_width(label_);
    return ui::SizeHint{std::min(4, w), w, w};
}

ui::SizeHint Link::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

}  // namespace ckv::widgets
