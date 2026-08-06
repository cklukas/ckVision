// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The theme roles ckVision's own M4-M6 widgets resolve through — the
// "ckv." namespace prefix (DECISIONS.md D-007) so third-party widgets
// can intern their own roles without colliding.
#pragma once

#include "cvision/ui/theme.hpp"

namespace ckv::ui {

struct StandardRoles {
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
};

// Interns every standard role into `registry` with the built-in
// Classic scheme's own values as fallback (so even an application that
// never explicitly loads a Theme still renders sensibly).
StandardRoles intern_standard_roles(RoleRegistry& registry);

// The faithful '90s blue-desktop look, built from `roles`' own
// fallbacks — provided as an explicit Theme (not just relying on
// fallbacks) so it round-trips through Theme::resolve identically to
// any other named scheme (Dark/Light/Mono arrive in M8).
Theme make_classic_theme(const RoleRegistry& registry, const StandardRoles& roles);

}  // namespace ckv::ui
