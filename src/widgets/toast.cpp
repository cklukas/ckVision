// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/toast.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {
namespace {

std::string_view severity_badge(NoticeSeverity severity) noexcept {
    switch (severity) {
        case NoticeSeverity::Info: return "[i] ";
        case NoticeSeverity::Success: return "[+] ";
        case NoticeSeverity::Warning: return "[!] ";
        case NoticeSeverity::Error: return "[x] ";
    }
    return "[i] ";
}

std::string_view severity_role_name(NoticeSeverity severity) noexcept {
    switch (severity) {
        case NoticeSeverity::Info: return "ckv.notice.info";
        case NoticeSeverity::Success: return "ckv.notice.success";
        case NoticeSeverity::Warning: return "ckv.notice.warning";
        case NoticeSeverity::Error: return "ckv.notice.error";
    }
    return "ckv.notice.info";
}

}  // namespace

Toast::Toast(std::string message, NoticeSeverity severity, std::int64_t duration_nanos)
    : message_(std::move(message)), severity_(severity), duration_nanos_(duration_nanos) {}

void Toast::set_message(std::string message) {
    if (message_ == message) return;
    message_ = std::move(message);
    size_hint_changed();
    invalidate();
}

void Toast::set_severity(NoticeSeverity severity) {
    if (severity_ == severity) return;
    severity_ = severity;
    if (context().roles != nullptr) {
        accent_role_ = context().roles->find(severity_role_name(severity_));
        if (accent_role_ == ui::kInvalidRole) accent_role_ = normal_role_;
    if (accessible_info().role == ui::AccessibleRole::None) set_accessible_role(ui::AccessibleRole::Alert);
    if (accessible_info().name.empty()) set_accessible_name(message_);
    }
    invalidate();
}

void Toast::set_role_override(ui::RoleId role) {
    if (role_override_ == role) return;
    role_override_ = role;
    invalidate();
}

void Toast::dismiss() {
    if (timer_id_ != 0 && context().app != nullptr) {
        context().app->cancel_timer(timer_id_);
        timer_id_ = 0;
    }
    const auto cb = on_dismiss;
    if (cb) cb();
}

void Toast::on_attached() {
    normal_role_ = context().roles->find("ckv.tooltip");
    accent_role_ = context().roles->find(severity_role_name(severity_));
    if (accent_role_ == ui::kInvalidRole) accent_role_ = normal_role_;
    if (accessible_info().role == ui::AccessibleRole::None) set_accessible_role(ui::AccessibleRole::Alert);
    if (accessible_info().name.empty()) set_accessible_name(message_);
    if (duration_nanos_ > 0 && context().app != nullptr) {
        const std::weak_ptr<void> weak = lifetime_token();
        timer_id_ = context().app->start_timer(duration_nanos_, false, [this, weak] {
            if (weak.expired()) return;
            timer_id_ = 0;
            dismiss();
        });
    }
}

void Toast::on_detaching() {
    if (timer_id_ != 0 && context().app != nullptr) {
        context().app->cancel_timer(timer_id_);
        timer_id_ = 0;
    }
}

void Toast::draw(scene::Painter& painter) {
    const int w = bounds().width;
    const int h = bounds().height;
    if (w <= 0 || h <= 0) return;
    const ui::RoleId role = role_override_ != ui::kInvalidRole
                                ? role_override_
                                : (accent_role_ != ui::kInvalidRole ? accent_role_ : normal_role_);
    const Style face = context().theme->resolve(role);
    painter.fill(Rect{0, 0, w, h}, Cell::from_grapheme(" ", face));
    const std::string text = " " + std::string(severity_badge(severity_)) + message_ + " ";
    painter.draw_text(Point{0, 0}, text::clip_to_width_view(text, w), face);
}

bool Toast::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    if (!absolute_bounds().contains(event.cell)) return false;
    dismiss();
    return true;
}

ui::SizeHint Toast::horizontal_size_hint() const {
    const int width = 2 + static_cast<int>(severity_badge(severity_).size()) +
                      text::text_width(message_);
    return ui::SizeHint{std::min(8, width), width, width};
}

ui::SizeHint Toast::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

Toast* show_toast(ui::Application& app, Desktop& desktop, std::string message,
                  NoticeSeverity severity, std::int64_t duration_nanos) {
    (void)app;
    int existing_toasts = 0;
    for (const auto& child : desktop.children()) {
        if (dynamic_cast<const Toast*>(child.get()) != nullptr) ++existing_toasts;
    }
    auto toast = std::make_unique<Toast>(std::move(message), severity, duration_nanos);
    const int pref_w = std::max(8, toast->horizontal_size_hint().preferred);
    const Rect desk = desktop.bounds();
    const int w = std::min(pref_w, std::max(1, desk.width - 2));
    const int x = std::max(0, (desk.width - w) / 2);
    const int base_y = std::max(0, desk.height - 2);
    const int y = std::max(0, base_y - existing_toasts * 2);
    toast->set_bounds(Rect{x, y, w, 1});
    Toast* raw = desktop.add_popup(std::move(toast));
    if (raw == nullptr) return nullptr;
    const std::weak_ptr<void> desk_weak = desktop.lifetime_token();
    const std::weak_ptr<void> toast_weak = raw->lifetime_token();
    raw->on_dismiss = [&desktop, desk_weak, raw, toast_weak] {
        if (desk_weak.expired() || toast_weak.expired()) return;
        desktop.remove_popup(raw);
    };
    return raw;
}

}  // namespace ckv::widgets
