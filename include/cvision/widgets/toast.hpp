// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <cstdint>
#include <functional>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::ui {
class Application;
}

namespace ckv::widgets {

class Desktop;

// Visual severity level shared by Toast and InfoBar.
enum class NoticeSeverity {
    Info,
    Success,
    Warning,
    Error,
};

// A transient auto-expiring overlay notification banner.
class CVISION_API Toast final : public ui::View {
public:
    // Construct a Toast with `message`, `severity`, and auto-dismiss lifetime in nanoseconds.
    explicit Toast(std::string message = {}, NoticeSeverity severity = NoticeSeverity::Info,
                   std::int64_t duration_nanos = 3'000'000'000LL);

    // Replace the displayed message text.
    void set_message(std::string message);
    // Return the current message text.
    const std::string& message() const noexcept { return message_; }
    // Change the severity level.
    void set_severity(NoticeSeverity severity);
    // Return the current severity level.
    NoticeSeverity severity() const noexcept { return severity_; }
    // Dismiss the toast immediately and invoke on_dismiss if set.
    void dismiss();
    // Override the background/text theme role.
    void set_role_override(ui::RoleId role);

    // Invoked when the toast expires or is dismissed.
    std::function<void()> on_dismiss;

    // Resolve theme roles and schedule auto-dismiss timer when attached.
    void on_attached() override;
    // Cancel pending timer on detach.
    void on_detaching() override;
    // Toasts float over desktop surfaces and cast a drop shadow.
    bool casts_shadow() const noexcept override { return true; }
    // Paint the severity badge and message.
    void draw(scene::Painter& painter) override;
    // Dismiss on left click inside the toast.
    bool on_mouse(const MouseEvent& event) override;
    // Return preferred width for the badge and message.
    ui::SizeHint horizontal_size_hint() const override;
    // Return single-row fixed height.
    ui::SizeHint vertical_size_hint() const override;

private:
    std::string message_;
    NoticeSeverity severity_ = NoticeSeverity::Info;
    std::int64_t duration_nanos_ = 3'000'000'000LL;
    std::uint64_t timer_id_ = 0;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId accent_role_ = ui::kInvalidRole;
    ui::RoleId role_override_ = ui::kInvalidRole;
};

// Show a transient Toast centered near the bottom of `desktop` and return a non-owning pointer.
CVISION_API Toast* show_toast(ui::Application& app, Desktop& desktop, std::string message,
                  NoticeSeverity severity = NoticeSeverity::Info,
                  std::int64_t duration_nanos = 3'000'000'000LL);

}  // namespace ckv::widgets
