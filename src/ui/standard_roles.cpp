// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/standard_roles.hpp"

#include <array>

namespace ckv::ui {
namespace {

// The classic blue-desktop palette (public, well-documented CUA-era
// convention — cyan-on-blue dialogs, black-on-white default buttons —
// not derived from any framework's source, per the engineering standard). Window
// frame/desktop/menu hues follow the well-documented, public CGA/EGA
// 16-color convention every DOS-era TUI framework of this style used
// (blue windows on a light-gray desktop, black-on-light-gray menus
// with a green selection bar) — a convention, not any one framework's
// copyrightable expression.
constexpr Color kBlue = Color::rgb(0, 0, 170);
constexpr Color kCyan = Color::rgb(0, 170, 170);
constexpr Color kGreen = Color::rgb(0, 170, 0);
constexpr Color kWhite = Color::rgb(255, 255, 255);
constexpr Color kBlack = Color::rgb(0, 0, 0);
constexpr Color kGray = Color::rgb(170, 170, 170);
constexpr Color kLightGray = Color::rgb(200, 200, 200);
constexpr Color kLightCyan = Color::rgb(85, 255, 255);
constexpr Color kYellow = Color::rgb(255, 255, 85);
constexpr Color kRed = Color::rgb(170, 0, 0);

void set_editor_roles(Theme& theme, const StandardRoles& roles, Color foreground, Color background, Color muted,
                      Color accent, Color keyword, Color string, Color error) {
    theme.set(roles.editor_text, Style{foreground, background, Attr{}});
    theme.set(roles.editor_gutter, Style{muted, background, Attr{}});
    theme.set(roles.editor_selection, Style{background, accent, Attr{}});
    theme.set(roles.editor_search, Style{background, keyword, Attr{}});
    theme.set(roles.editor_syntax_plain, Style{foreground, background, Attr{}});
    theme.set(roles.editor_syntax_keyword, Style{keyword, background, Attr{}});
    theme.set(roles.editor_syntax_type, Style{accent, background, Attr{}});
    theme.set(roles.editor_syntax_property, Style{keyword, background, Attr{}});
    theme.set(roles.editor_syntax_string, Style{string, background, Attr{}});
    theme.set(roles.editor_syntax_number, Style{Color::rgb(220, 130, 210), background, Attr{}});
    theme.set(roles.editor_syntax_comment, Style{muted, background, Attr{}});
    theme.set(roles.editor_syntax_command, Style{accent, background, Attr{}});
    theme.set(roles.editor_syntax_operator, Style{foreground, background, Attr{}});
    theme.set(roles.editor_syntax_escape, Style{keyword, background, Attr{}});
    theme.set(roles.editor_syntax_error, Style{error, background, Attr::Bold});
}

}  // namespace

StandardRoles intern_standard_roles(RoleRegistry& registry) {
    StandardRoles r;
    r.desktop_background = registry.intern("ckv.desktop.background", Style{kBlue, kLightGray, Attr{}});
    // Dialogs are the convention's GRAY family (a dialog is visually a
    // different kind of surface than a blue document window), with the
    // iconic green buttons and a black composited drop shadow whose
    // background matches the gray dialog surface the buttons sit on.
    r.dialog_frame = registry.intern("ckv.dialog.frame", Style{kWhite, kLightGray, Attr{}});
    r.dialog_background = registry.intern("ckv.dialog.background", Style{kBlack, kLightGray, Attr{}});
    r.label_text = registry.intern("ckv.label.text", Style{kBlack, kLightGray, Attr{}});
    r.label_mnemonic = registry.intern("ckv.label.mnemonic", Style{kYellow, kLightGray, Attr{}});
    r.hotkey = registry.intern("ckv.hotkey", Style{kRed, kLightGray, Attr{}});
    r.static_text = registry.intern("ckv.static.text", Style{kBlack, kLightGray, Attr{}});
    r.help_text = registry.intern("ckv.help.text", Style{kYellow, kBlue, Attr{}});
    r.button_normal = registry.intern("ckv.button.normal", Style{kBlack, kGreen, Attr{}});
    r.button_focused = registry.intern("ckv.button.focused", Style{kWhite, kGreen, Attr{}});
    r.button_hovered = registry.intern("ckv.button.hovered", Style{kWhite, kGreen, Attr{}});
    r.button_default = registry.intern("ckv.button.default", Style{kLightCyan, kGreen, Attr{}});
    r.button_shadow = registry.intern("ckv.button.shadow", Style{kBlack, kLightGray, Attr{}});
    // A flat button has no shadow to lose when it goes down, so the press is
    // in the colours, and they have to differ from the face it wears at rest
    // or the press says nothing at all.
    r.button_pressed = registry.intern("ckv.button.pressed", Style{kBlack, kLightGray, Attr{}});
    r.input_normal = registry.intern("ckv.input.normal", Style{kWhite, kBlue, Attr{}});
    r.input_focused = registry.intern("ckv.input.focused", Style{kWhite, kBlue, Attr{}});
    r.input_invalid = registry.intern("ckv.input.invalid", Style{kWhite, kRed, Attr::Bold});
    r.message_info_text = registry.intern("ckv.message.info.text", Style{kBlack, kLightGray, Attr{}});
    r.message_warning_text = registry.intern("ckv.message.warning.text", Style{kBlack, kLightGray, Attr::Bold});
    r.message_error_text = registry.intern("ckv.message.error.text", Style{kWhite, kRed, Attr::Bold});
    r.message_confirm_text = registry.intern("ckv.message.confirm.text", Style{kBlack, kLightGray, Attr{}});
    r.window_frame_active = registry.intern("ckv.window.frame.active", Style{kWhite, kBlue, Attr::Bold});
    r.window_frame_inactive = registry.intern("ckv.window.frame.inactive", Style{kLightGray, kBlue, Attr{}});
    r.window_title_active = registry.intern("ckv.window.title.active", Style{kWhite, kBlue, Attr::Bold});
    r.window_title_inactive = registry.intern("ckv.window.title.inactive", Style{kLightGray, kBlue, Attr{}});
    r.window_control = registry.intern("ckv.window.control", Style{kGreen, kBlue, Attr::Bold});
    r.window_control_pressed =
        registry.intern("ckv.window.control.pressed", Style{kBlue, kGreen, Attr::Bold});
    r.calendar_today = registry.intern("ckv.calendar.today", Style{kYellow, kBlue, Attr::Bold});
    r.calendar_marked = registry.intern("ckv.calendar.marked", Style{kBlack, kCyan, Attr{}});
    r.menu_bar_normal = registry.intern("ckv.menu.bar.normal", Style{kBlack, kLightGray, Attr{}});
    r.menu_bar_active = registry.intern("ckv.menu.bar.active", Style{kBlack, kGreen, Attr{}});
    r.menu_dropdown_normal = registry.intern("ckv.menu.dropdown.normal", Style{kBlack, kLightGray, Attr{}});
    r.menu_dropdown_highlighted =
        registry.intern("ckv.menu.dropdown.highlighted", Style{kBlack, kGreen, Attr{}});
    r.menu_dropdown_disabled = registry.intern("ckv.menu.dropdown.disabled", Style{kGray, kLightGray, Attr::Dim});
    // M6 widgets (fallbacks match exactly what each widget rendered
    // while still borrowing input_*/dialog_*/button_* — see the
    // struct's own comment).
    r.list_normal = registry.intern("ckv.list.normal", Style{kBlack, kWhite, Attr{}});
    r.list_selected = registry.intern("ckv.list.selected", Style{kBlack, kGreen, Attr{}});
    r.list_selected_inactive =
        registry.intern("ckv.list.selected.inactive", Style{kBlack, kLightGray, Attr{}});
    r.table_header = registry.intern("ckv.table.header", Style{kWhite, kLightGray, Attr{}});
    r.memo_normal = registry.intern("ckv.memo.normal", Style{kBlack, kWhite, Attr{}});
    r.memo_focused = registry.intern("ckv.memo.focused", Style{kBlack, kWhite, Attr::Underline});
    r.memo_invalid = registry.intern("ckv.memo.invalid", Style{kWhite, kRed, Attr::Bold});
    // Choice groups are a selection surface, not an action button. Keeping
    // their cyan fill distinct from the green button face preserves the
    // classic visual hierarchy in dense preference dialogs.
    r.option_normal = registry.intern("ckv.option.normal", Style{kBlack, kCyan, Attr{}});
    r.option_focused = registry.intern("ckv.option.focused", Style{kWhite, kCyan, Attr{}});
    // The classic scrollbar is a cyan-on-blue dotted page area with a
    // continuous cyan scroll box, matching the established CP437 desktop
    // convention. The blue field stays visually quiet; the glyphs
    // themselves are selected by Scrollbar.
    r.scrollbar_track = registry.intern("ckv.scrollbar.track", Style{kLightCyan, kBlue, Attr{}});
    r.scrollbar_thumb = registry.intern("ckv.scrollbar.thumb", Style{kLightCyan, kBlue, Attr{}});
    r.image_fallback = registry.intern("ckv.image.fallback", Style{kBlack, kLightGray, Attr{}});
    r.canvas_fallback = registry.intern("ckv.canvas.fallback", Style{kBlack, kLightGray, Attr{}});
    r.text_view_text = registry.intern("ckv.textview.text", Style{kBlack, kLightGray, Attr{}});
    r.flow_view_text = registry.intern("ckv.flow.text", Style{kBlack, kLightGray, Attr{}});
    r.status_line_normal = registry.intern("ckv.statusline.normal", Style{kBlack, kLightGray, Attr{}});
    r.status_line_disabled = registry.intern("ckv.statusline.disabled", Style{kGray, kLightGray, Attr::Dim});
    r.status_line_selected =
        registry.intern("ckv.statusline.selected", Style{kBlack, kGreen, Attr{}});
    r.status_line_selected_hotkey =
        registry.intern("ckv.statusline.selected.hotkey", Style{kRed, kGreen, Attr{}});
    r.status_line_selected_disabled =
        registry.intern("ckv.statusline.selected.disabled", Style{kGray, kGreen, Attr::Dim});
    r.splitter_normal = registry.intern("ckv.splitter.normal", Style{kWhite, kLightGray, Attr{}});
    r.splitter_focused = registry.intern("ckv.splitter.focused", Style{kWhite, kGreen, Attr{}});
    r.editor_text = registry.intern("ckv.editor.text", Style{kBlack, kWhite, Attr{}});
    r.editor_gutter = registry.intern("ckv.editor.gutter", Style{kGray, kWhite, Attr{}});
    r.editor_selection = registry.intern("ckv.editor.selection", Style{kWhite, kBlue, Attr{}});
    r.editor_search = registry.intern("ckv.editor.search", Style{kBlack, kYellow, Attr{}});
    r.editor_syntax_plain = registry.intern("ckv.editor.syntax.plain", Style{kBlack, kWhite, Attr{}});
    r.editor_syntax_keyword = registry.intern("ckv.editor.syntax.keyword", Style{kBlue, kWhite, Attr{}});
    r.editor_syntax_type = registry.intern("ckv.editor.syntax.type", Style{kLightCyan, kWhite, Attr{}});
    r.editor_syntax_property = registry.intern("ckv.editor.syntax.property", Style{kRed, kWhite, Attr{}});
    r.editor_syntax_string = registry.intern("ckv.editor.syntax.string", Style{kGreen, kWhite, Attr{}});
    r.editor_syntax_number = registry.intern("ckv.editor.syntax.number", Style{kRed, kWhite, Attr{}});
    r.editor_syntax_comment = registry.intern("ckv.editor.syntax.comment", Style{kGray, kWhite, Attr{}});
    r.editor_syntax_command = registry.intern("ckv.editor.syntax.command", Style{kBlue, kWhite, Attr{}});
    r.editor_syntax_operator = registry.intern("ckv.editor.syntax.operator", Style{kBlack, kWhite, Attr{}});
    r.editor_syntax_escape = registry.intern("ckv.editor.syntax.escape", Style{kRed, kWhite, Attr{}});
    r.editor_syntax_error = registry.intern("ckv.editor.syntax.error", Style{kWhite, kRed, Attr::Bold});
    return r;
}

Theme make_classic_theme(const RoleRegistry& registry, const StandardRoles& roles) {
    Theme theme(registry);
    theme.set(roles.desktop_background, Style{kBlue, kLightGray, Attr{}});
    theme.set(roles.dialog_frame, Style{kWhite, kLightGray, Attr{}});
    theme.set(roles.dialog_background, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.label_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.label_mnemonic, Style{kYellow, kLightGray, Attr{}});
    theme.set(roles.hotkey, Style{kRed, kLightGray, Attr{}});
    theme.set(roles.static_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.help_text, Style{kYellow, kBlue, Attr{}});
    theme.set(roles.button_normal, Style{kBlack, kGreen, Attr{}});
    theme.set(roles.button_focused, Style{kWhite, kGreen, Attr{}});
    theme.set(roles.button_hovered, Style{kWhite, kGreen, Attr{}});
    theme.set(roles.button_default, Style{kLightCyan, kGreen, Attr{}});
    theme.set(roles.button_shadow, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.button_pressed, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.input_normal, Style{kWhite, kBlue, Attr{}});
    theme.set(roles.input_focused, Style{kWhite, kBlue, Attr{}});
    theme.set(roles.input_invalid, Style{kWhite, kRed, Attr::Bold});
    theme.set(roles.message_info_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.message_warning_text, Style{kBlack, kLightGray, Attr::Bold});
    theme.set(roles.message_error_text, Style{kWhite, kRed, Attr::Bold});
    theme.set(roles.message_confirm_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.window_frame_active, Style{kWhite, kBlue, Attr::Bold});
    theme.set(roles.window_frame_inactive, Style{kLightGray, kBlue, Attr{}});
    theme.set(roles.window_title_active, Style{kWhite, kBlue, Attr::Bold});
    theme.set(roles.window_title_inactive, Style{kLightGray, kBlue, Attr{}});
    theme.set(roles.window_control, Style{kGreen, kBlue, Attr::Bold});
    theme.set(roles.window_control_pressed, Style{kBlue, kGreen, Attr::Bold});
    theme.set(roles.calendar_today, Style{kYellow, kBlue, Attr::Bold});
    theme.set(roles.calendar_marked, Style{kBlack, kCyan, Attr{}});
    theme.set(roles.menu_bar_normal, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.menu_bar_active, Style{kBlack, kGreen, Attr{}});
    theme.set(roles.menu_dropdown_normal, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.menu_dropdown_highlighted, Style{kBlack, kGreen, Attr{}});
    theme.set(roles.menu_dropdown_disabled, Style{kGray, kLightGray, Attr::Dim});
    theme.set(roles.list_normal, Style{kBlack, kWhite, Attr{}});
    theme.set(roles.list_selected, Style{kBlack, kGreen, Attr{}});
    theme.set(roles.list_selected_inactive, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.table_header, Style{kWhite, kLightGray, Attr{}});
    theme.set(roles.memo_normal, Style{kBlack, kWhite, Attr{}});
    theme.set(roles.memo_focused, Style{kBlack, kWhite, Attr::Underline});
    theme.set(roles.memo_invalid, Style{kWhite, kRed, Attr::Bold});
    theme.set(roles.option_normal, Style{kBlack, kCyan, Attr{}});
    theme.set(roles.option_focused, Style{kWhite, kCyan, Attr{}});
    theme.set(roles.scrollbar_track, Style{kLightCyan, kBlue, Attr{}});
    theme.set(roles.scrollbar_thumb, Style{kLightCyan, kBlue, Attr{}});
    theme.set(roles.image_fallback, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.canvas_fallback, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.text_view_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.flow_view_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.status_line_normal, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.status_line_disabled, Style{kGray, kLightGray, Attr::Dim});
    theme.set(roles.status_line_selected, Style{kBlack, kGreen, Attr{}});
    theme.set(roles.status_line_selected_hotkey, Style{kRed, kGreen, Attr{}});
    theme.set(roles.status_line_selected_disabled, Style{kGray, kGreen, Attr::Dim});
    theme.set(roles.splitter_normal, Style{kWhite, kLightGray, Attr{}});
    theme.set(roles.splitter_focused, Style{kWhite, kGreen, Attr{}});
    set_editor_roles(theme, roles, kBlack, kWhite, kGray, kBlue, kRed, kGreen, kRed);
    return theme;
}

namespace {

// Dark: near-black backgrounds, light text, a muted blue accent.
constexpr Color kDarkBg = Color::rgb(30, 30, 30);
constexpr Color kDarkPanel = Color::rgb(45, 45, 48);
constexpr Color kDarkFg = Color::rgb(220, 220, 220);
constexpr Color kDarkAccent = Color::rgb(0, 90, 158);
constexpr Color kDarkFocus = Color::rgb(70, 70, 76);
constexpr Color kDarkShadow = Color::rgb(18, 18, 20);
constexpr Color kDarkYellow = Color::rgb(220, 200, 80);
constexpr Color kDarkRed = Color::rgb(200, 70, 70);

// Light: light backgrounds, near-black text, a pale blue accent.
constexpr Color kLightBg = Color::rgb(245, 245, 245);
constexpr Color kLightPanel = Color::rgb(225, 225, 225);
constexpr Color kLightFg = Color::rgb(20, 20, 20);
constexpr Color kLightAccent = Color::rgb(190, 215, 245);
constexpr Color kLightFocus = Color::rgb(205, 205, 205);
constexpr Color kLightShadow = Color::rgb(150, 150, 152);
constexpr Color kLightYellow = Color::rgb(255, 235, 140);
constexpr Color kLightRed = Color::rgb(220, 90, 90);

// Mono: black/white/gray only — every distinction Classic/Dark/Light
// express through hue is carried by Attr here instead, so the scheme
// stays legible after quantization to a genuinely monochrome terminal.
constexpr Color kMonoBg = Color::rgb(0, 0, 0);
constexpr Color kMonoFg = Color::rgb(255, 255, 255);
constexpr Color kMonoGray = Color::rgb(160, 160, 160);

}  // namespace

Theme make_dark_theme(const RoleRegistry& registry, const StandardRoles& roles) {
    Theme theme(registry);
    theme.set(roles.desktop_background, Style{kDarkFocus, kDarkBg, Attr{}});
    theme.set(roles.dialog_frame, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.dialog_background, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.label_text, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.label_mnemonic, Style{kDarkYellow, kDarkPanel, Attr{}});
    theme.set(roles.hotkey, Style{kDarkYellow, kDarkPanel, Attr::Bold});
    theme.set(roles.static_text, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.help_text, Style{kDarkYellow, kDarkBg, Attr{}});
    theme.set(roles.button_normal, Style{kDarkFg, kDarkFocus, Attr{}});
    theme.set(roles.button_focused, Style{kMonoFg, kDarkAccent, Attr::Reverse});
    theme.set(roles.button_hovered, Style{kMonoFg, kDarkAccent, Attr{}});
    // Every button keeps the same raised face; the default one is marked by
    // an accent foreground and weight, not by becoming a different -- and in
    // a dark scheme, near-black -- surface. A control the reader is meant to
    // press should never be the colour of the background behind the dialog.
    theme.set(roles.button_default, Style{kDarkYellow, kDarkFocus, Attr::Bold});
    // Darker than both the dialog it falls on and the button casting it: a
    // shadow the colour of the control reads as part of the control, and the
    // depth it exists to describe disappears.
    theme.set(roles.button_shadow, Style{kDarkShadow, kDarkPanel, Attr{}});
    theme.set(roles.button_pressed, Style{kDarkFg, kDarkAccent, Attr::Bold});
    theme.set(roles.input_normal, Style{kDarkFg, kDarkBg, Attr{}});
    theme.set(roles.input_focused, Style{kDarkFg, kDarkBg, Attr{}});
    theme.set(roles.input_invalid, Style{kMonoFg, kDarkRed, Attr::Bold});
    theme.set(roles.message_info_text, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.message_warning_text, Style{kDarkYellow, kDarkPanel, Attr::Bold});
    theme.set(roles.message_error_text, Style{kMonoFg, kDarkRed, Attr::Bold});
    theme.set(roles.message_confirm_text, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.window_frame_active, Style{kMonoFg, kDarkFocus, Attr::Bold});
    theme.set(roles.window_frame_inactive, Style{kDarkFg, kDarkPanel, Attr{}});
    // The caption sits IN the frame, so it carries the frame's own background
    // and separates itself by weight and a brighter foreground. An accent
    // background here instead reads as a selected item pasted onto the border
    // rather than as the window's name, because that is what a differently
    // coloured block surrounded by frame means everywhere else in the theme.
    theme.set(roles.window_title_active, Style{kMonoFg, kDarkFocus, Attr::Bold});
    theme.set(roles.window_title_inactive, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.window_control, Style{kDarkYellow, kDarkPanel, Attr::Bold});
    theme.set(roles.window_control_pressed, Style{kDarkPanel, kDarkYellow, Attr::Bold});
    theme.set(roles.calendar_today, Style{kDarkYellow, kDarkPanel, Attr::Bold});
    theme.set(roles.calendar_marked, Style{kDarkFg, kDarkFocus, Attr{}});
    theme.set(roles.menu_bar_normal, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.menu_bar_active, Style{kMonoFg, kDarkAccent, Attr::Reverse});
    theme.set(roles.menu_dropdown_normal, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.menu_dropdown_highlighted, Style{kMonoFg, kDarkAccent, Attr::Reverse});
    theme.set(roles.menu_dropdown_disabled, Style{kDarkFg, kDarkPanel, Attr::Dim});
    theme.set(roles.list_normal, Style{kDarkFg, kDarkBg, Attr{}});
    theme.set(roles.list_selected, Style{kMonoFg, kDarkAccent, Attr{}});
    theme.set(roles.list_selected_inactive, Style{kDarkFg, kDarkFocus, Attr{}});
    theme.set(roles.table_header, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.memo_normal, Style{kDarkFg, kDarkBg, Attr{}});
    theme.set(roles.memo_focused, Style{kDarkFg, kDarkBg, Attr::Underline});
    theme.set(roles.memo_invalid, Style{kMonoFg, kDarkRed, Attr::Bold});
    theme.set(roles.option_normal, Style{kDarkFg, kDarkFocus, Attr{}});
    theme.set(roles.option_focused, Style{kMonoFg, kDarkAccent, Attr::Reverse});
    // The trough is a colour, not a texture: a blank cell in a shade set
    // back from the panel, so the thumb's half-covered cells meet it flush.
    theme.set(roles.scrollbar_track, Style{kDarkAccent, kDarkBg, Attr{}});
    theme.set(roles.scrollbar_thumb, Style{kDarkAccent, kDarkBg, Attr{}});
    theme.set(roles.image_fallback, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.canvas_fallback, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.text_view_text, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.flow_view_text, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.status_line_normal, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.status_line_disabled, Style{kDarkFg, kDarkPanel, Attr::Dim});
    theme.set(roles.status_line_selected, Style{kDarkBg, kDarkAccent, Attr{}});
    theme.set(roles.status_line_selected_hotkey, Style{kDarkBg, kDarkAccent, Attr::Bold});
    theme.set(roles.status_line_selected_disabled, Style{kDarkBg, kDarkAccent, Attr::Dim});
    theme.set(roles.splitter_normal, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.splitter_focused, Style{kMonoFg, kDarkAccent, Attr::Reverse});
    set_editor_roles(theme, roles, kDarkFg, kDarkBg, Color::rgb(120, 150, 120), kDarkAccent, kDarkYellow,
                     Color::rgb(100, 200, 120), kDarkRed);
    return theme;
}

Theme make_light_theme(const RoleRegistry& registry, const StandardRoles& roles) {
    Theme theme(registry);
    theme.set(roles.desktop_background, Style{kLightFocus, kLightBg, Attr{}});
    theme.set(roles.dialog_frame, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.dialog_background, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.label_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.label_mnemonic, Style{kLightFg, kLightPanel, Attr::Bold});
    theme.set(roles.hotkey, Style{kLightRed, kLightPanel, Attr::Bold});
    theme.set(roles.static_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.help_text, Style{kLightFg, kLightBg, Attr{}});
    theme.set(roles.button_normal, Style{kLightFg, kLightFocus, Attr{}});
    theme.set(roles.button_focused, Style{kLightFg, kLightAccent, Attr::Reverse});
    theme.set(roles.button_hovered, Style{kLightFg, kLightAccent, Attr{}});
    // Same rule as the dark scheme, and no borrowing of a monochrome constant
    // for a hued one: the face stays the button face.
    theme.set(roles.button_default, Style{kLightFg, kLightFocus, Attr::Bold});
    // A light scheme's shadow is a soft grey, not near-black: the same rule,
    // read against a pale surface.
    theme.set(roles.button_shadow, Style{kLightShadow, kLightPanel, Attr{}});
    theme.set(roles.button_pressed, Style{kLightFg, kLightAccent, Attr::Bold});
    theme.set(roles.input_normal, Style{kLightFg, kLightBg, Attr{}});
    theme.set(roles.input_focused, Style{kLightFg, kLightBg, Attr{}});
    theme.set(roles.input_invalid, Style{kMonoFg, kLightRed, Attr::Bold});
    theme.set(roles.message_info_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.message_warning_text, Style{kLightFg, kLightYellow, Attr::Bold});
    theme.set(roles.message_error_text, Style{kMonoFg, kLightRed, Attr::Bold});
    theme.set(roles.message_confirm_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.window_frame_active, Style{kLightFg, kLightFocus, Attr::Bold});
    theme.set(roles.window_frame_inactive, Style{kLightFg, kLightPanel, Attr{}});
    // Same rule as the dark scheme: the caption takes the frame's background
    // and stands out by weight, not by a block of accent colour.
    theme.set(roles.window_title_active, Style{kLightFg, kLightFocus, Attr::Bold});
    theme.set(roles.window_title_inactive, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.window_control, Style{kLightRed, kLightPanel, Attr::Bold});
    theme.set(roles.window_control_pressed, Style{kLightPanel, kLightRed, Attr::Bold});
    theme.set(roles.calendar_today, Style{kLightRed, kLightPanel, Attr::Bold});
    theme.set(roles.calendar_marked, Style{kLightFg, kLightAccent, Attr{}});
    theme.set(roles.menu_bar_normal, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.menu_bar_active, Style{kLightFg, kLightAccent, Attr::Reverse});
    theme.set(roles.menu_dropdown_normal, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.menu_dropdown_highlighted, Style{kLightFg, kLightAccent, Attr::Reverse});
    theme.set(roles.menu_dropdown_disabled, Style{kLightFg, kLightPanel, Attr::Dim});
    theme.set(roles.list_normal, Style{kLightFg, kLightBg, Attr{}});
    theme.set(roles.list_selected, Style{kLightFg, kLightAccent, Attr{}});
    theme.set(roles.list_selected_inactive, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.table_header, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.memo_normal, Style{kLightFg, kLightBg, Attr{}});
    theme.set(roles.memo_focused, Style{kLightFg, kLightBg, Attr::Underline});
    theme.set(roles.memo_invalid, Style{kMonoFg, kLightRed, Attr::Bold});
    theme.set(roles.option_normal, Style{kLightFg, kLightFocus, Attr{}});
    theme.set(roles.option_focused, Style{kLightFg, kLightAccent, Attr::Reverse});
    theme.set(roles.scrollbar_track, Style{kLightFg, kLightFocus, Attr{}});
    theme.set(roles.scrollbar_thumb, Style{kLightFg, kLightFocus, Attr{}});
    theme.set(roles.image_fallback, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.canvas_fallback, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.text_view_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.flow_view_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.status_line_normal, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.status_line_disabled, Style{kLightFg, kLightPanel, Attr::Dim});
    // Near-black on the pale accent, not the panel colour: kLightPanel is
    // rgb(225,225,225) and kLightAccent rgb(190,215,245), so a panel-coloured
    // glyph on the accent is near-white on pale blue — legible on a designer's
    // screenshot and not on a real one. Reported from a running ckmux, where
    // the selected entry of the window switcher bar was the first surface wide
    // enough to make it obvious. Every other light-scheme role already puts
    // kLightFg on its background; the selected trio were the exception.
    theme.set(roles.status_line_selected, Style{kLightFg, kLightAccent, Attr{}});
    theme.set(roles.status_line_selected_hotkey, Style{kLightRed, kLightAccent, Attr::Bold});
    theme.set(roles.status_line_selected_disabled, Style{kLightShadow, kLightAccent, Attr::Dim});
    theme.set(roles.splitter_normal, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.splitter_focused, Style{kLightFg, kLightAccent, Attr::Reverse});
    set_editor_roles(theme, roles, kLightFg, kLightBg, kLightFocus, kLightAccent, kLightRed,
                     Color::rgb(40, 130, 70), kLightRed);
    return theme;
}

Theme make_mono_theme(const RoleRegistry& registry, const StandardRoles& roles) {
    Theme theme(registry);
    theme.set(roles.desktop_background, Style{kMonoGray, kMonoBg, Attr{}});
    theme.set(roles.dialog_frame, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.dialog_background, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.label_text, Style{kMonoFg, kMonoBg, Attr{}});
    // Monochrome has no second colour to mark a mnemonic with, so here the
    // underline is the only available marker and stays.
    theme.set(roles.label_mnemonic, Style{kMonoFg, kMonoBg, Attr::Underline | Attr::Bold});
    theme.set(roles.hotkey, Style{kMonoFg, kMonoBg, Attr::Bold});
    theme.set(roles.static_text, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.help_text, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.button_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.button_focused, Style{kMonoBg, kMonoFg, Attr::Reverse});
    theme.set(roles.button_hovered, Style{kMonoFg, kMonoBg, Attr::Underline});
    theme.set(roles.button_default, Style{kMonoFg, kMonoBg, Attr::Bold});
    theme.set(roles.button_shadow, Style{kMonoGray, kMonoBg, Attr{}});
    theme.set(roles.button_pressed, Style{kMonoBg, kMonoFg, Attr::Bold});
    theme.set(roles.input_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.input_focused, Style{kMonoFg, kMonoBg, Attr::Underline});
    theme.set(roles.input_invalid, Style{kMonoBg, kMonoFg, Attr::Reverse | Attr::Bold});
    theme.set(roles.message_info_text, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.message_warning_text, Style{kMonoFg, kMonoBg, Attr::Bold});
    theme.set(roles.message_error_text, Style{kMonoBg, kMonoFg, Attr::Reverse | Attr::Bold});
    theme.set(roles.message_confirm_text, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.window_frame_active, Style{kMonoFg, kMonoBg, Attr::Bold});
    theme.set(roles.window_frame_inactive, Style{kMonoGray, kMonoBg, Attr{}});
    theme.set(roles.window_title_active, Style{kMonoBg, kMonoFg, Attr::Reverse | Attr::Bold});
    theme.set(roles.window_title_inactive, Style{kMonoGray, kMonoBg, Attr{}});
    theme.set(roles.window_control, Style{kMonoFg, kMonoBg, Attr::Bold});
    theme.set(roles.window_control_pressed, Style{kMonoBg, kMonoFg, Attr::Bold});
    theme.set(roles.calendar_today, Style{kMonoFg, kMonoBg, Attr::Bold});
    theme.set(roles.calendar_marked, Style{kMonoFg, kMonoBg, Attr::Underline});
    theme.set(roles.menu_bar_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.menu_bar_active, Style{kMonoBg, kMonoFg, Attr::Reverse});
    theme.set(roles.menu_dropdown_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.menu_dropdown_highlighted, Style{kMonoBg, kMonoFg, Attr::Reverse});
    theme.set(roles.menu_dropdown_disabled, Style{kMonoGray, kMonoBg, Attr::Dim});
    theme.set(roles.list_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.list_selected, Style{kMonoBg, kMonoFg, Attr::Reverse});
    theme.set(roles.list_selected_inactive, Style{kMonoGray, kMonoBg, Attr::Bold});
    theme.set(roles.table_header, Style{kMonoFg, kMonoBg, Attr::Bold});
    theme.set(roles.memo_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.memo_focused, Style{kMonoFg, kMonoBg, Attr::Underline});
    theme.set(roles.memo_invalid, Style{kMonoBg, kMonoFg, Attr::Reverse | Attr::Bold});
    theme.set(roles.option_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.option_focused, Style{kMonoBg, kMonoFg, Attr::Reverse});
    theme.set(roles.scrollbar_track, Style{kMonoFg, kMonoGray, Attr{}});
    theme.set(roles.scrollbar_thumb, Style{kMonoFg, kMonoGray, Attr::Bold});
    theme.set(roles.image_fallback, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.canvas_fallback, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.text_view_text, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.flow_view_text, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.status_line_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.status_line_disabled, Style{kMonoGray, kMonoBg, Attr::Dim});
    theme.set(roles.status_line_selected, Style{kMonoBg, kMonoFg, Attr{}});
    theme.set(roles.status_line_selected_hotkey, Style{kMonoBg, kMonoFg, Attr::Bold});
    theme.set(roles.status_line_selected_disabled, Style{kMonoBg, kMonoFg, Attr::Dim});
    theme.set(roles.splitter_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.splitter_focused, Style{kMonoBg, kMonoFg, Attr::Reverse});
    set_editor_roles(theme, roles, kMonoFg, kMonoBg, kMonoGray, kMonoFg, kMonoFg, kMonoFg, kMonoFg);
    return theme;
}

Theme make_high_contrast_theme(const RoleRegistry& registry, const StandardRoles& roles) {
    constexpr Color black = Color::rgb(0, 0, 0);
    constexpr Color white = Color::rgb(255, 255, 255);
    const Style normal{white, black, Attr{}};
    const Style selected{black, white, Attr{}};
    Theme theme(registry);
    // Size deduced: a hand-written count only ever says how many roles there
    // were when someone last counted.
    const std::array all_roles{
        roles.desktop_background,    roles.dialog_frame,             roles.dialog_background,
        roles.label_text,            roles.label_mnemonic,           roles.hotkey,
        roles.static_text,           roles.help_text,                roles.button_normal,
        roles.button_focused,
        roles.button_hovered,
        roles.button_default,        roles.button_shadow,            roles.button_pressed,
        roles.input_normal,
        roles.input_focused,         roles.input_invalid,            roles.message_info_text,
        roles.message_warning_text,  roles.message_error_text,        roles.message_confirm_text,
        roles.window_frame_active,   roles.window_frame_inactive,    roles.window_title_active,
        roles.window_title_inactive, roles.window_control,           roles.window_control_pressed,
        roles.calendar_today,        roles.calendar_marked,
        roles.menu_bar_normal,
        roles.menu_bar_active,       roles.menu_dropdown_normal,     roles.menu_dropdown_highlighted,
        roles.menu_dropdown_disabled, roles.list_normal,             roles.list_selected,
        roles.list_selected_inactive,
        roles.table_header,          roles.memo_normal,              roles.memo_focused,
        roles.memo_invalid,
        roles.option_normal,         roles.option_focused,           roles.scrollbar_track,
        roles.scrollbar_thumb,       roles.image_fallback,           roles.canvas_fallback,
        roles.text_view_text,        roles.flow_view_text,           roles.status_line_normal,
        roles.status_line_disabled,  roles.status_line_selected,     roles.status_line_selected_hotkey,
        roles.status_line_selected_disabled,
        roles.splitter_normal,       roles.splitter_focused,
        roles.editor_text,           roles.editor_gutter,            roles.editor_selection,
        roles.editor_search,         roles.editor_syntax_plain,      roles.editor_syntax_keyword,
        roles.editor_syntax_type,    roles.editor_syntax_property,   roles.editor_syntax_string,
        roles.editor_syntax_number,  roles.editor_syntax_comment,    roles.editor_syntax_command,
        roles.editor_syntax_operator, roles.editor_syntax_escape,    roles.editor_syntax_error,
    };
    for (const RoleId role : all_roles) theme.set(role, normal);

    // High contrast, like monochrome, marks the mnemonic by underline.
    theme.set(roles.label_mnemonic, Style{white, black, Attr::Underline | Attr::Bold});
    theme.set(roles.hotkey, Style{white, black, Attr::Bold});
    theme.set(roles.button_focused, selected);
    theme.set(roles.button_hovered, selected);
    theme.set(roles.button_default, Style{white, black, Attr::Bold});
    theme.set(roles.button_pressed, Style{black, white, Attr::Bold});
    theme.set(roles.input_focused, Style{white, black, Attr::Underline});
    theme.set(roles.input_invalid, Style{black, white, Attr::Bold});
    theme.set(roles.message_warning_text, Style{white, black, Attr::Bold});
    theme.set(roles.message_error_text, Style{black, white, Attr::Bold});
    theme.set(roles.window_frame_active, Style{white, black, Attr::Bold});
    theme.set(roles.window_frame_inactive, Style{white, black, Attr::Dim});
    theme.set(roles.window_title_active, selected);
    theme.set(roles.window_title_inactive, Style{white, black, Attr::Dim});
    theme.set(roles.window_control, Style{white, black, Attr::Bold});
    theme.set(roles.window_control_pressed, selected);
    theme.set(roles.calendar_today, Style{white, black, Attr::Bold});
    theme.set(roles.calendar_marked, Style{white, black, Attr::Underline});
    theme.set(roles.menu_bar_active, selected);
    theme.set(roles.menu_dropdown_highlighted, selected);
    theme.set(roles.menu_dropdown_disabled, Style{white, black, Attr::Dim});
    theme.set(roles.list_selected, selected);
    // Bold rather than a second background: high contrast has only two
    // colours, so the unfocused selection has to be marked by weight or it
    // would be indistinguishable from the focused one.
    theme.set(roles.list_selected_inactive, Style{white, black, Attr::Bold});
    theme.set(roles.table_header, Style{black, white, Attr::Bold});
    theme.set(roles.memo_focused, Style{white, black, Attr::Underline});
    theme.set(roles.memo_invalid, Style{black, white, Attr::Bold});
    theme.set(roles.option_focused, selected);
    // Two colours only: the trough takes the inverted surface so it is
    // visible at all, and the thumb draws onto it in the same pair.
    theme.set(roles.scrollbar_track, selected);
    theme.set(roles.scrollbar_thumb, selected);
    theme.set(roles.status_line_disabled, Style{white, black, Attr::Dim});
    theme.set(roles.status_line_selected, selected);
    theme.set(roles.status_line_selected_hotkey, Style{black, white, Attr::Bold});
    theme.set(roles.status_line_selected_disabled, Style{black, white, Attr::Dim});
    theme.set(roles.splitter_focused, selected);
    theme.set(roles.editor_gutter, Style{white, black, Attr::Dim});
    theme.set(roles.editor_selection, selected);
    theme.set(roles.editor_search, Style{black, white, Attr::Bold});
    theme.set(roles.editor_syntax_keyword, Style{white, black, Attr::Bold});
    theme.set(roles.editor_syntax_type, Style{white, black, Attr::Underline});
    theme.set(roles.editor_syntax_property, Style{white, black, Attr::Bold});
    theme.set(roles.editor_syntax_string, Style{white, black, Attr::Underline});
    theme.set(roles.editor_syntax_number, Style{white, black, Attr::Bold});
    theme.set(roles.editor_syntax_comment, Style{white, black, Attr::Dim});
    theme.set(roles.editor_syntax_command, Style{white, black, Attr::Bold});
    theme.set(roles.editor_syntax_escape, Style{white, black, Attr::Underline});
    theme.set(roles.editor_syntax_error, Style{black, white, Attr::Bold});
    return theme;
}

}  // namespace ckv::ui
