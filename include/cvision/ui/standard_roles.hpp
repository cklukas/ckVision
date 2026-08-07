// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The theme roles ckVision's own M4-M6 widgets resolve through — the
// "ckv." namespace prefix (DECISIONS.md D-007) so third-party widgets
// can intern their own roles without colliding.
#pragma once

#include "cvision/ui/theme.hpp"

namespace ckv::ui {

struct StandardRoles {
    // The Desktop's own fill — deliberately a SEPARATE role from
    // dialog_background: sharing one role between the desktop and
    // every dialog/window interior means they can never look visually
    // distinct, which is exactly backwards for a windowed desktop
    // where the desktop is supposed to read as "background," not
    // "just another panel."
    RoleId desktop_background;
    RoleId dialog_frame;
    RoleId dialog_background;
    RoleId label_text;
    RoleId label_mnemonic;
    RoleId static_text;
    RoleId button_normal;
    RoleId button_focused;
    RoleId button_default;
    RoleId input_normal;
    RoleId input_focused;
    RoleId input_invalid;  // a failed validator (ARCHITECTURE.md §5 dialog-accept veto)

    // Window chrome (ARCHITECTURE.md §5 "Windows, popups, modality").
    RoleId window_frame_active;
    RoleId window_frame_inactive;
    RoleId window_title_active;
    RoleId window_title_inactive;

    // Menu system (ARCHITECTURE.md §5 "Menus").
    RoleId menu_bar_normal;
    RoleId menu_bar_active;         // the highlighted top-level menu title while the bar is active
    RoleId menu_dropdown_normal;
    RoleId menu_dropdown_highlighted;
    RoleId menu_dropdown_disabled;
};

// Interns every standard role into `registry` with the built-in
// Classic scheme's own values as fallback (so even an application that
// never explicitly loads a Theme still renders sensibly).
StandardRoles intern_standard_roles(RoleRegistry& registry);

// The faithful '90s blue-desktop look, built from `roles`' own
// fallbacks — provided as an explicit Theme (not just relying on
// fallbacks) so it round-trips through Theme::resolve identically to
// any other named scheme.
Theme make_classic_theme(const RoleRegistry& registry, const StandardRoles& roles);

// First versions of the remaining three built-in schemes
// (ARCHITECTURE.md §5 "Themes and schemes"; ROADMAP.md M6c — "the exit
// exercises every scheme"). Dark and Light are ordinary truecolor
// palettes; Mono is deliberately restricted to black/white/gray with
// Attr::Bold/Reverse/Underline carrying every visual distinction that
// Classic/Dark/Light express through hue — the actual "16-color-safe"
// property, verified by degrading correctly under a reduced-color
// terminal capability at the term layer, not by this module picking
// values from a 16-color palette itself (core carries truecolor
// requests only, per Color's own contract; quantization is a term-
// layer presentation concern).
Theme make_dark_theme(const RoleRegistry& registry, const StandardRoles& roles);
Theme make_light_theme(const RoleRegistry& registry, const StandardRoles& roles);
Theme make_mono_theme(const RoleRegistry& registry, const StandardRoles& roles);

}  // namespace ckv::ui
