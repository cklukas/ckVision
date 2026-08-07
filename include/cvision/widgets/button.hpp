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
// `on_press` on Enter/Space while focused, or a full mouse Down-then-Up
// cycle inside its own bounds (a drag-out-then-release cancels the
// press, matching every classic button's own behavior).
//
// Rendering follows the classic windowed-desktop button verbatim
// (verified against the reference implementation's own drawState):
// a solid face with the label centered, a drop shadow composited from
// half-block glyphs — "▄" at the face's right edge on its first row,
// "█" below it, and a "▀" run along the bottom row — and a depressed
// state that shifts the face one cell right while the shadow
// disappears, which is what makes a click visibly "push" the button
// into the surface. `shadow_role`'s background must match the surface
// the button sits on (a dialog's background, typically): the shadow
// glyphs' foreground paints the dark halves, and their background
// fills the rest of those cells.
class Button : public View {
public:
    Button(std::string text, const ui::Theme& theme, ui::RoleId normal_role, ui::RoleId focused_role,
           ui::RoleId default_role, ui::RoleId shadow_role);

    void set_text(std::string text);
    const std::string& text() const noexcept { return raw_text_; }

    void set_default(bool is_default) noexcept { is_default_ = is_default; }
    bool is_default() const noexcept { return is_default_; }

    std::function<void()> on_press;

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;

private:
    void fire_press();

    std::string raw_text_;
    std::string display_text_;
    const ui::Theme& theme_;
    ui::RoleId normal_role_;
    ui::RoleId focused_role_;
    ui::RoleId default_role_;
    ui::RoleId shadow_role_;
    bool is_default_ = false;
    bool has_focus_ = false;
    bool pressed_ = false;
};

}  // namespace ckv::widgets
