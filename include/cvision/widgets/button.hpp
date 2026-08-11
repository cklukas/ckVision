// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <functional>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

// Baseline per plans/widgets.md: default/normal styling, mnemonic
// (visual only — see widgets/mnemonic.hpp), pressed feedback. Fires
// `on_press` on Enter/Space press and repeat while focused, never on
// key release, or a full mouse Down-then-Up cycle inside its own bounds
// (a drag-out-then-release cancels the press, matching every classic
// button's own behavior).
//
// Rendering follows ckVision's documented classic-desktop button contract:
// a solid face with the label centered, a drop shadow composited from
// half-block glyphs — "▄" at the face's right edge on its first row,
// "█" below it, and a "▀" run along the bottom row — and a depressed
// state that shifts the face one cell right while the shadow
// disappears, which is what makes a click visibly "push" the button
// into the surface. `shadow_role`'s background must match the surface
// the button sits on (a dialog's background, typically): the shadow
// glyphs' foreground paints the dark halves, and their background
// fills the rest of those cells.
//
// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.button.normal/focused/default/shadow" and
// "ckv.label.mnemonic". Already
// defaults to FocusPolicy::TabStop — a caller never needs to set that
// itself. set_role_override lets a caller redirect all four roles at
// once (e.g. a distinct "danger" button family); takes effect
// immediately whether called before or after attachment.
class Button : public View {
public:
    explicit Button(std::string text);

    void set_text(std::string text);
    const std::string& text() const noexcept { return raw_text_; }

    void set_default(bool is_default) noexcept { is_default_ = is_default; }
    bool is_default() const noexcept { return is_default_; }

    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role, ui::RoleId default_role,
                            ui::RoleId shadow_role) noexcept {
        normal_role_ = normal_role;
        focused_role_ = focused_role;
        default_role_ = default_role;
        shadow_role_ = shadow_role;
    }
    void set_mnemonic_role_override(ui::RoleId role) noexcept { mnemonic_role_ = role; }

    std::function<void()> on_press;

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    void fire_press();

    std::string raw_text_;
    std::string display_text_;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId default_role_ = ui::kInvalidRole;
    ui::RoleId shadow_role_ = ui::kInvalidRole;
    ui::RoleId mnemonic_role_ = ui::kInvalidRole;
    bool is_default_ = false;
    bool has_focus_ = false;
    bool pressed_ = false;
};

}  // namespace ckv::widgets
