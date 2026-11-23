// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <functional>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// A focusable, clickable hyperlink control with optional OSC-8 target emission.
class CVISION_TYPE_API Link final : public ui::View {
public:
    // Construct a Link with display `label` and optional `url` target.
    CVISION_MEMBER_API explicit Link(std::string label = {}, std::string url = {});

    // Replace the displayed link label.
    CVISION_MEMBER_API void set_label(std::string label);
    // Return the displayed link label.
    const std::string& label() const noexcept { return label_; }
    // Replace the OSC-8 URL target (empty omits OSC-8 framing).
    CVISION_MEMBER_API void set_url(std::string url);
    // Return the URL target.
    const std::string& url() const noexcept { return url_; }
    // Override the normal-state theme role.
    CVISION_MEMBER_API void set_role_override(ui::RoleId role);

    // Invoked when the user activates the link via Enter, Space, or click.
    std::function<void()> on_activate;

    // Resolve theme roles on attach.
    CVISION_MEMBER_API void on_attached() override;
    // Paint the underlined link text with optional OSC-8 hyperlink target.
    CVISION_MEMBER_API void draw(scene::Painter& painter) override;
    // Activate on Enter or Space when focused.
    CVISION_MEMBER_API bool on_key(const KeyEvent& event) override;
    // Activate on left mouse click inside bounds.
    CVISION_MEMBER_API bool on_mouse(const MouseEvent& event) override;
    // Repaint on focus gain or loss.
    CVISION_MEMBER_API void on_focus(const FocusEvent& event) override;
    // Repaint on hover enter or leave.
    CVISION_MEMBER_API void on_hover_changed(bool hovered) override;
    // Return PointerShape::Pointer when enabled.
    CVISION_MEMBER_API std::optional<PointerShape> pointer_shape_at(Point local) const override;
    // Return preferred width for the link label.
    CVISION_MEMBER_API ui::SizeHint horizontal_size_hint() const override;
    // Return fixed single-row height.
    CVISION_MEMBER_API ui::SizeHint vertical_size_hint() const override;

private:
    std::string label_;
    std::string url_;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId role_override_ = ui::kInvalidRole;
    bool pressed_ = false;
};

}  // namespace ckv::widgets
