// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The theme roles ckVision's own M4-M6 widgets resolve through — the
// "ckv." namespace prefix (the decision log D-007) so third-party widgets
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
    // Shared command accent: command chords in status lines and '&'-marked
    // mnemonics in menus retain their surface background while applying this
    // role's foreground and attributes.
    RoleId hotkey;
    RoleId static_text;
    // Read-only reference/document content hosted on a window surface.
    // Kept separate from static_text so help can remain legible on blue
    // document windows without changing ordinary dialog labels.
    RoleId help_text;
    RoleId button_normal;
    RoleId button_focused;
    // A button under the pointer, but not focused and not pressed. Its own
    // role rather than a lightened normal: hover is the weakest of the
    // three states a button can be in at once, and a scheme has to be able
    // to say how weak — a monochrome one may want it to be nothing at all,
    // and setting it equal to normal is how it says so.
    RoleId button_hovered;
    RoleId button_default;
    RoleId button_shadow;  // the composited drop shadow's glyph color; bg matches the dialog surface
    RoleId button_pressed;  // a flat button held down, which has no shadow to lose
    RoleId input_normal;
    RoleId input_focused;
    RoleId input_invalid;  // a failed validator (the architecture §5 dialog-accept veto)
    RoleId message_info_text;
    RoleId message_warning_text;
    RoleId message_error_text;
    RoleId message_confirm_text;

    // Window chrome (the architecture §5 "Windows, popups, modality").
    RoleId window_frame_active;
    RoleId window_frame_inactive;
    RoleId window_title_active;
    RoleId window_title_inactive;
    // The maximize/restore control. Its foreground is composited onto the
    // owning frame's background, so the accent works for both document
    // windows and dialog chrome.
    RoleId window_control;
    // A frame control with the pointer held down on it. Its own role rather
    // than an inversion the widget works out: whether a pressed control
    // darkens, brightens or swaps is the theme's business, and a monochrome
    // theme has to answer it differently from a colour one.
    RoleId window_control_pressed;
    // A calendar's today and its marked span. Their own roles rather than
    // borrowed ones: today is a fact about the world, the marked span is a
    // fact about the data, and the reader has to tell them apart from the
    // day they themselves selected.
    RoleId calendar_today;
    RoleId calendar_marked;

    // Menu system (the architecture §5 "Menus").
    RoleId menu_bar_normal;
    RoleId menu_bar_active;         // the highlighted top-level menu title while the bar is active
    RoleId menu_dropdown_normal;
    RoleId menu_dropdown_highlighted;
    RoleId menu_dropdown_disabled;

    // M6 data/scrolling widgets (the widget catalog M6a/M6b) — each of
    // these previously had no role family of its own and borrowed
    // input_*/dialog_*/button_* instead (review finding E2: "M6
    // widgets borrow foreign roles"), which meant restyling an input
    // field silently restyled every list/tree/table too. Fallback
    // values match exactly what each widget rendered before this
    // split, so no widget's appearance changes — only the indirection.
    RoleId list_normal;      // ListView / TreeView: an unselected row
    // ListView / TreeView: the cursor row while the list holds the keyboard.
    // A highlight bar — a background the row's whole width — because that is
    // what "this row is the one" means everywhere else in these schemes. An
    // underline instead reads as a text field or a hyperlink, and says
    // nothing about which pane the keyboard is in.
    RoleId list_selected;
    // The same row while the keyboard is somewhere else. A reader with two
    // lists on screen has to be able to tell which one their arrow keys will
    // move, and a selection that looks identical either way cannot say.
    // Muted rather than absent: the list must still remember its place.
    RoleId list_selected_inactive;
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
    RoleId flow_view_text;   // FlowView's wrapped text and raster fallback surface
    RoleId status_line_normal;
    RoleId status_line_disabled;
    // A status item under the pointer. Distinct roles rather than an
    // inversion of the normal ones: a status item is a button, and which
    // colour a theme wants a pressed button to wear is the theme's call,
    // not a transformation the widget can guess.
    RoleId status_line_selected;
    RoleId status_line_selected_hotkey;
    RoleId status_line_selected_disabled;
    RoleId splitter_normal;   // widgets::Splitter's divider bar, unfocused
    RoleId splitter_focused;  // widgets::Splitter's divider bar while it holds keyboard focus

    // WP-41 editor semantics. These roles are intentionally standard rather
    // than widget-local fallbacks so every built-in scheme can keep source,
    // selection, search, and syntax categories legible.
    RoleId editor_text;
    RoleId editor_gutter;
    RoleId editor_selection;
    RoleId editor_search;
    RoleId editor_syntax_plain;
    RoleId editor_syntax_keyword;
    RoleId editor_syntax_type;
    RoleId editor_syntax_property;
    RoleId editor_syntax_string;
    RoleId editor_syntax_number;
    RoleId editor_syntax_comment;
    RoleId editor_syntax_command;
    RoleId editor_syntax_operator;
    RoleId editor_syntax_escape;
    RoleId editor_syntax_error;
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
// (the architecture §5 "Themes and schemes"; the roadmap M6c — "the exit
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

// Accessibility-grade high contrast: bright white text on black, with
// black-on-white focused and selected surfaces. Unlike Mono, it does not
// preserve a muted gray hierarchy that could reduce legibility.
Theme make_high_contrast_theme(const RoleRegistry& registry, const StandardRoles& roles);

}  // namespace ckv::ui
