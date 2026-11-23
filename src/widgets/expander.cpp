// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/expander.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/layout.hpp"

namespace ckv::widgets {

Expander::Expander(std::string title, bool expanded)
    : ui::View(Rect{}, ui::FocusPolicy::TabStop),
      title_(std::move(title)),
      expanded_(expanded) {}

void Expander::set_title(std::string title) {
    if (title_ == title) return;
    title_ = std::move(title);
    size_hint_changed();
    invalidate();
}

void Expander::set_expanded(bool expanded) {
    if (expanded_ == expanded) return;
    expanded_ = expanded;
    if (content_ != nullptr) content_->set_visible(expanded_);
    relayout();
    size_hint_changed();
    invalidate();
    const auto cb = on_change;
    if (cb) cb(expanded_);
}

ui::View* Expander::set_content(std::unique_ptr<ui::View> content) {
    if (content_ != nullptr) {
        remove_child(content_);
        content_ = nullptr;
    }
    if (content == nullptr) {
        size_hint_changed();
        invalidate();
        return nullptr;
    }
    content->set_visible(expanded_);
    content_ = add_child(std::move(content));
    relayout();
    size_hint_changed();
    invalidate();
    return content_;
}

void Expander::set_role_override(ui::RoleId role) {
    if (role_override_ == role) return;
    role_override_ = role;
    invalidate();
}

void Expander::on_attached() {
    normal_role_ = context().roles->find("ckv.label.text");
    focused_role_ = context().roles->find("ckv.option.focused");
    disabled_role_ = context().roles->find("ckv.label.disabled");
    if (accessible_info().role == ui::AccessibleRole::None) set_accessible_role(ui::AccessibleRole::Panel);
    if (accessible_info().name.empty()) set_accessible_name(title_);
}

void Expander::relayout() {
    if (content_ == nullptr) return;
    if (!expanded_) {
        content_->set_bounds(Rect{0, 1, std::max(0, bounds().width), 0});
        return;
    }
    content_->set_bounds(Rect{0, 1, std::max(0, bounds().width), std::max(0, bounds().height - 1)});
}

void Expander::on_resized() { relayout(); }

void Expander::on_child_size_hint_changed(ui::View&) {
    relayout();
    size_hint_changed();
}

void Expander::draw(scene::Painter& painter) {
    const int w = bounds().width;
    const int h = bounds().height;
    if (w <= 0 || h <= 0) return;
    const bool enabled = enabled_in_tree();
    const ui::RoleId normal = role_override_ != ui::kInvalidRole ? role_override_ : normal_role_;
    const Style face = !enabled ? context().theme->resolve(disabled_role_)
                                : (has_focus() ? context().theme->resolve(focused_role_)
                                               : context().theme->resolve(normal));
    painter.fill(Rect{0, 0, w, 1}, Cell::from_grapheme(" ", face));
    const std::string header = std::string(expanded_ ? "[v] " : "[>] ") + title_;
    painter.draw_text(Point{0, 0}, text::clip_to_width_view(header, w), face);
}

bool Expander::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || event.action == KeyAction::Release) return false;
    if (event.chord.key == Key::Enter ||
        (event.chord.key == Key::Char && event.chord.modifiers == Modifier::None &&
         event.chord.text == " ")) {
        toggle();
        return true;
    }
    if (event.chord.key == Key::Right && !expanded_) {
        set_expanded(true);
        return true;
    }
    if (event.chord.key == Key::Left && expanded_) {
        set_expanded(false);
        return true;
    }
    return false;
}

bool Expander::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || event.action != MouseAction::Down ||
        event.button != MouseButton::Left) {
        return false;
    }
    const Rect abs = absolute_bounds();
    if (event.cell.y != abs.y || event.cell.x < abs.x || event.cell.x >= abs.right()) return false;
    toggle();
    return true;
}

void Expander::on_focus(const FocusEvent&) { invalidate(); }

std::optional<PointerShape> Expander::pointer_shape_at(Point local) const {
    if (!enabled_in_tree() || local.y != 0 || local.x < 0 || local.x >= bounds().width)
        return std::nullopt;
    return PointerShape::Pointer;
}

ui::SizeHint Expander::horizontal_size_hint() const {
    const int header_w = 4 + text::text_width(title_);
    if (content_ == nullptr) {
        return ui::SizeHint{std::min(4, header_w), header_w, ui::kUnboundedExtent};
    }
    const ui::SizeHint child = content_->horizontal_size_hint();
    return ui::SizeHint{std::max(std::min(4, header_w), child.min),
                        std::max(header_w, child.preferred), ui::kUnboundedExtent};
}

ui::SizeHint Expander::vertical_size_hint() const {
    if (!expanded_ || content_ == nullptr) return ui::SizeHint{1, 1, 1};
    const ui::SizeHint child = content_->vertical_size_hint();
    const int max_h = child.max >= ui::kUnboundedExtent ? ui::kUnboundedExtent : 1 + child.max;
    return ui::SizeHint{1 + child.min, 1 + child.preferred, max_h};
}

int Expander::height_for_width(int width) const {
    if (!expanded_ || content_ == nullptr) return 1;
    const int ch = content_->height_for_width(std::max(0, width));
    return 1 + (ch >= 0 ? ch : content_->vertical_size_hint().preferred);
}

}  // namespace ckv::widgets
