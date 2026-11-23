// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/info_bar.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/layout.hpp"

namespace ckv::widgets {
namespace {

std::string_view badge_for(NoticeSeverity severity) noexcept {
    switch (severity) {
        case NoticeSeverity::Info: return "[i] ";
        case NoticeSeverity::Success: return "[+] ";
        case NoticeSeverity::Warning: return "[!] ";
        case NoticeSeverity::Error: return "[x] ";
    }
    return "[i] ";
}

std::string_view role_for(NoticeSeverity severity) noexcept {
    switch (severity) {
        case NoticeSeverity::Info: return "ckv.notice.info";
        case NoticeSeverity::Success: return "ckv.notice.success";
        case NoticeSeverity::Warning: return "ckv.notice.warning";
        case NoticeSeverity::Error: return "ckv.notice.error";
    }
    return "ckv.notice.info";
}

}  // namespace

InfoBar::InfoBar(std::string message, NoticeSeverity severity, bool dismissible)
    : ui::View(Rect{}, ui::FocusPolicy::TabStop),
      message_(std::move(message)),
      severity_(severity),
      dismissible_(dismissible) {}

void InfoBar::set_message(std::string message) {
    if (message_ == message) return;
    message_ = std::move(message);
    size_hint_changed();
    invalidate();
}

void InfoBar::set_severity(NoticeSeverity severity) {
    if (severity_ == severity) return;
    severity_ = severity;
    if (context().roles != nullptr) {
        accent_role_ = context().roles->find(role_for(severity_));
        if (accent_role_ == ui::kInvalidRole) accent_role_ = normal_role_;
    }
    invalidate();
}

void InfoBar::set_action_label(std::string label) {
    if (action_label_ == label) return;
    action_label_ = std::move(label);
    size_hint_changed();
    invalidate();
}

void InfoBar::set_dismissible(bool dismissible) {
    if (dismissible_ == dismissible) return;
    dismissible_ = dismissible;
    size_hint_changed();
    invalidate();
}

void InfoBar::set_role_override(ui::RoleId role) {
    if (role_override_ == role) return;
    role_override_ = role;
    invalidate();
}

void InfoBar::on_attached() {
    normal_role_ = context().roles->find("ckv.label.text");
    accent_role_ = context().roles->find(role_for(severity_));
    if (accent_role_ == ui::kInvalidRole) accent_role_ = normal_role_;
    disabled_role_ = context().roles->find("ckv.label.disabled");
    focused_role_ = context().roles->find("ckv.option.focused");
    if (accessible_info().role == ui::AccessibleRole::None) set_accessible_role(ui::AccessibleRole::StatusBar);
    if (accessible_info().name.empty()) set_accessible_name(message_);
}

void InfoBar::on_focus(const FocusEvent&) { invalidate(); }
void InfoBar::on_hover_changed(bool hovered) {
    if (!hovered) hover_local_.reset();
    invalidate();
}

Rect InfoBar::close_bounds() const noexcept {
    if (!dismissible_ || bounds().width < 4 || bounds().height <= 0) return {};
    return Rect{bounds().width - 3, 0, 3, 1};
}

Rect InfoBar::action_bounds() const noexcept {
    if (action_label_.empty() || bounds().height <= 0) return {};
    const int label_w = text::text_width(action_label_) + 2;
    const int close_w = dismissible_ ? 4 : 1;
    const int x = bounds().width - close_w - label_w;
    if (x < 4) return {};
    return Rect{x, 0, label_w, 1};
}

void InfoBar::draw(scene::Painter& painter) {
    const int w = bounds().width;
    const int h = bounds().height;
    if (w <= 0 || h <= 0) return;
    const bool enabled = enabled_in_tree();
    const ui::RoleId role = role_override_ != ui::kInvalidRole
                                ? role_override_
                                : (!enabled ? disabled_role_
                                            : (has_focus() ? focused_role_
                                                           : (accent_role_ != ui::kInvalidRole ? accent_role_ : normal_role_)));
    const Style face = context().theme->resolve(role);
    painter.fill(Rect{0, 0, w, h}, Cell::from_grapheme(" ", face));

    const Rect close_r = close_bounds();
    const Rect action_r = action_bounds();
    int msg_limit = w;
    if (!action_r.empty()) msg_limit = std::min(msg_limit, action_r.x - 1);
    else if (!close_r.empty()) msg_limit = std::min(msg_limit, close_r.x - 1);

    const std::string body = " " + std::string(badge_for(severity_)) + message_;
    if (msg_limit > 0)
        painter.draw_text(Point{0, 0}, text::clip_to_width_view(body, msg_limit), face);

    if (!action_r.empty()) {
        Style act_style = face;
        if (enabled) act_style.attrs |= Attr::Bold | Attr::Underline;
        if (enabled && ((hover_local_ && action_r.contains(*hover_local_)) || action_pressed_))
            act_style.attrs |= Attr::Reverse;
        painter.draw_text(Point{action_r.x, 0}, "[" + action_label_ + "]", act_style);
    }
    if (!close_r.empty()) {
        Style close_style = face;
        if (enabled && ((hover_local_ && close_r.contains(*hover_local_)) || close_pressed_))
            close_style.attrs |= Attr::Reverse;
        painter.draw_text(Point{close_r.x, 0}, "[x]", close_style);
    }
}

bool InfoBar::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || event.action == KeyAction::Release) return false;
    if (event.chord.key == Key::Enter && !action_label_.empty()) {
        const auto cb = on_action;
        if (cb) cb();
        return true;
    }
    if (event.chord.key == Key::Escape && dismissible_) {
        const auto cb = on_dismiss;
        if (cb) cb();
        return true;
    }
    return false;
}

bool InfoBar::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree()) {
        action_pressed_ = false;
        close_pressed_ = false;
        hover_local_.reset();
        return false;
    }
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    if (event.action == MouseAction::Move) {
        if (hover_local_ != local) {
            hover_local_ = local;
            invalidate();
        }
        return false;
    }
    if (event.button != MouseButton::Left && event.action != MouseAction::Up) return false;
    if (event.action == MouseAction::Down && event.button == MouseButton::Left) {
        if (!close_bounds().empty() && close_bounds().contains(local)) {
            close_pressed_ = true;
            action_pressed_ = false;
            invalidate();
            return true;
        }
        if (!action_bounds().empty() && action_bounds().contains(local)) {
            action_pressed_ = true;
            close_pressed_ = false;
            invalidate();
            return true;
        }
        return false;
    }
    if (event.action == MouseAction::Up && (close_pressed_ || action_pressed_)) {
        const bool was_close = close_pressed_;
        const bool was_action = action_pressed_;
        close_pressed_ = false;
        action_pressed_ = false;
        invalidate();
        if (was_close && !close_bounds().empty() && close_bounds().contains(local)) {
            const auto cb = on_dismiss;
            if (cb) cb();
            return true;
        }
        if (was_action && !action_bounds().empty() && action_bounds().contains(local)) {
            const auto cb = on_action;
            if (cb) cb();
            return true;
        }
        return true;
    }
    return false;
}

std::optional<PointerShape> InfoBar::pointer_shape_at(Point local) const {
    if (!enabled_in_tree()) return std::nullopt;
    if ((!close_bounds().empty() && close_bounds().contains(local)) ||
        (!action_bounds().empty() && action_bounds().contains(local))) {
        return PointerShape::Pointer;
    }
    return std::nullopt;
}

ui::SizeHint InfoBar::horizontal_size_hint() const {
    int width = 2 + static_cast<int>(badge_for(severity_).size()) + text::text_width(message_);
    if (!action_label_.empty()) width += 3 + text::text_width(action_label_);
    if (dismissible_) width += 4;
    return ui::SizeHint{std::min(12, width), width, ui::kUnboundedExtent};
}

ui::SizeHint InfoBar::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

}  // namespace ckv::widgets
