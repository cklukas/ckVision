// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <functional>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/toast.hpp"

namespace ckv::widgets {

// An inline dismissible status/alert banner with an optional action button.
class CVISION_TYPE_API InfoBar final : public ui::View {
public:
    // Construct an InfoBar with `message`, `severity`, and optional `dismissible` close button.
    CVISION_MEMBER_API explicit InfoBar(std::string message = {}, NoticeSeverity severity = NoticeSeverity::Info,
                     bool dismissible = true);

    // Replace the banner message text.
    CVISION_MEMBER_API void set_message(std::string message);
    // Return the current message text.
    const std::string& message() const noexcept { return message_; }
    // Change the severity level.
    CVISION_MEMBER_API void set_severity(NoticeSeverity severity);
    // Return the current severity level.
    NoticeSeverity severity() const noexcept { return severity_; }
    // Configure an optional inline action button label (empty hides it).
    CVISION_MEMBER_API void set_action_label(std::string label);
    // Return the current action button label.
    const std::string& action_label() const noexcept { return action_label_; }
    // Enable or disable the close button.
    CVISION_MEMBER_API void set_dismissible(bool dismissible);
    // Return true if the close button is shown.
    bool dismissible() const noexcept { return dismissible_; }
    // Override the background/text theme role.
    CVISION_MEMBER_API void set_role_override(ui::RoleId role);

    // Invoked when the action button is clicked or triggered via Enter.
    std::function<void()> on_action;
    // Invoked when the user clicks the dismiss button.
    std::function<void()> on_dismiss;

    // Resolve theme roles on attach.
    CVISION_MEMBER_API void on_attached() override;
    // Repaint on focus change.
    CVISION_MEMBER_API void on_focus(const FocusEvent& event) override;
    // Repaint on hover change.
    CVISION_MEMBER_API void on_hover_changed(bool hovered) override;
    // Paint the severity indicator, message, optional action, and close affordance.
    CVISION_MEMBER_API void draw(scene::Painter& painter) override;
    // Handle Enter for action or Escape for dismiss when focused.
    CVISION_MEMBER_API bool on_key(const KeyEvent& event) override;
    // Handle clicks on the action button or dismiss button.
    CVISION_MEMBER_API bool on_mouse(const MouseEvent& event) override;
    // Return PointerShape::Pointer over interactive controls.
    CVISION_MEMBER_API std::optional<PointerShape> pointer_shape_at(Point local) const override;
    // Return preferred horizontal extent.
    CVISION_MEMBER_API ui::SizeHint horizontal_size_hint() const override;
    // Return fixed 1-row vertical extent.
    CVISION_MEMBER_API ui::SizeHint vertical_size_hint() const override;

private:
    Rect action_bounds() const noexcept;
    Rect close_bounds() const noexcept;

    std::string message_;
    std::string action_label_;
    NoticeSeverity severity_ = NoticeSeverity::Info;
    bool dismissible_ = true;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId accent_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId role_override_ = ui::kInvalidRole;
    bool action_pressed_ = false;
    bool close_pressed_ = false;
    std::optional<Point> hover_local_;
};

}  // namespace ckv::widgets
