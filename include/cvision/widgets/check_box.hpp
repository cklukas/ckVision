// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <functional>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

// A standalone single-choice check box ("[ ] Label" / "[X] Label").
class CVISION_TYPE_API CheckBox final : public ui::View {
public:
    // Construct a CheckBox with an optional mnemonic label and initial checked state.
    CVISION_MEMBER_API explicit CheckBox(std::string label = {}, bool checked = false);

    // Replace the label (re-parsing any '&' mnemonic).
    CVISION_MEMBER_API void set_label(std::string label);
    // Return the raw label text.
    const std::string& label() const noexcept { return label_; }
    // Return the parsed mnemonic character, or an empty string if none.
    const std::string& mnemonic() const noexcept { return parsed_.mnemonic; }

    // Set the checked state without invoking on_change.
    CVISION_MEMBER_API void set_checked(bool checked);
    // Return true if the check box is currently checked.
    bool is_checked() const noexcept { return checked_; }
    // Toggle the checked state and invoke on_change if set.
    CVISION_MEMBER_API void toggle();

    // Invoked when the user or toggle() flips the checked state.
    std::function<void(bool)> on_change;

    // Resolve theme roles once attached to a view tree.
    CVISION_MEMBER_API void on_attached() override;
    // Paint the "[ ] Label" / "[X] Label" indicator.
    CVISION_MEMBER_API void draw(scene::Painter& painter) override;
    // Handle Space / Enter to toggle when focused.
    CVISION_MEMBER_API bool on_key(const KeyEvent& event) override;
    // Toggle on left mouse press inside bounds.
    CVISION_MEMBER_API bool on_mouse(const MouseEvent& event) override;
    // Repaint on focus gain or loss.
    CVISION_MEMBER_API void on_focus(const FocusEvent& event) override;
    // Repaint on pointer hover enter or leave.
    CVISION_MEMBER_API void on_hover_changed(bool hovered) override;
    // Override the normal-state theme role.
    CVISION_MEMBER_API void set_role_override(ui::RoleId role);
    // Return PointerShape::Pointer over interactive bounds.
    CVISION_MEMBER_API std::optional<PointerShape> pointer_shape_at(Point local) const override;
    // Return preferred width for "[ ] " plus stripped label width.
    CVISION_MEMBER_API ui::SizeHint horizontal_size_hint() const override;
    // Return fixed height of 1 row.
    CVISION_MEMBER_API ui::SizeHint vertical_size_hint() const override;

private:
    std::string label_;
    MnemonicText parsed_;
    bool checked_ = false;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId mnemonic_role_ = ui::kInvalidRole;
    ui::RoleId role_override_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
