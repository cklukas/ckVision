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
    RoleId button_shadow;  // the composited drop shadow's glyph color; bg matches the dialog surface
    RoleId input_normal;
    RoleId input_focused;
    RoleId input_invalid;  // a failed validator (ARCHITECTURE.md §5 dialog-accept veto)
    RoleId message_info_text;
    RoleId message_warning_text;
    RoleId message_error_text;
    RoleId message_confirm_text;

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

    // M6 data/scrolling widgets (plans/widgets.md M6a/M6b) — each of
    // these previously had no role family of its own and borrowed
    // input_*/dialog_*/button_* instead (review finding E2: "M6
    // widgets borrow foreign roles"), which meant restyling an input
    // field silently restyled every list/tree/table too. Fallback
    // values match exactly what each widget rendered before this
    // split, so no widget's appearance changes — only the indirection.
    RoleId list_normal;      // ListView / TreeView: an unselected row
    RoleId list_selected;    // ListView / TreeView: the cursor row
    RoleId table_header;     // Table: the column-header row
    RoleId memo_normal;
    RoleId memo_focused;
    RoleId option_normal;    // CheckGroup / RadioGroup
    RoleId option_focused;
    RoleId scrollbar_track;  // shared by every widget with a built-in Scrollbar
    RoleId scrollbar_thumb;
    RoleId image_fallback;   // ImageView's no-graphics-capability text fallback
    RoleId canvas_fallback;  // Canvas's no-graphics-capability text fallback
    RoleId text_view_text;
    RoleId status_line_normal;
    RoleId status_line_disabled;
    RoleId splitter_normal;   // widgets::Splitter's divider bar, unfocused
    RoleId splitter_focused;  // widgets::Splitter's divider bar while it holds keyboard focus
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
