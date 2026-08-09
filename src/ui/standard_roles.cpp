// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/standard_roles.hpp"

namespace ckv::ui {
namespace {

// The classic blue-desktop palette (public, well-documented CUA-era
// convention — cyan-on-blue dialogs, black-on-white default buttons —
// not derived from any framework's source, per AGENTS.md). Window
// frame/desktop/menu hues follow the well-documented, public CGA/EGA
// 16-color convention every DOS-era TUI framework of this style used
// (blue windows on a light-gray desktop, black-on-light-gray menus
// with a green selection bar) — a convention, not any one framework's
// copyrightable expression.
constexpr Color kBlue = Color::rgb(0, 0, 170);
constexpr Color kGreen = Color::rgb(0, 170, 0);
constexpr Color kWhite = Color::rgb(255, 255, 255);
constexpr Color kBlack = Color::rgb(0, 0, 0);
constexpr Color kGray = Color::rgb(170, 170, 170);
constexpr Color kLightGray = Color::rgb(200, 200, 200);
constexpr Color kLightCyan = Color::rgb(85, 255, 255);
constexpr Color kYellow = Color::rgb(255, 255, 85);
constexpr Color kRed = Color::rgb(170, 0, 0);

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
    r.label_mnemonic = registry.intern("ckv.label.mnemonic", Style{kYellow, kLightGray, Attr::Underline});
    r.static_text = registry.intern("ckv.static.text", Style{kBlack, kLightGray, Attr{}});
    r.button_normal = registry.intern("ckv.button.normal", Style{kBlack, kGreen, Attr{}});
    r.button_focused = registry.intern("ckv.button.focused", Style{kWhite, kGreen, Attr{}});
    r.button_default = registry.intern("ckv.button.default", Style{kLightCyan, kGreen, Attr{}});
    r.button_shadow = registry.intern("ckv.button.shadow", Style{kBlack, kLightGray, Attr{}});
    r.input_normal = registry.intern("ckv.input.normal", Style{kBlack, kWhite, Attr{}});
    r.input_focused = registry.intern("ckv.input.focused", Style{kBlack, kWhite, Attr::Underline});
    r.input_invalid = registry.intern("ckv.input.invalid", Style{kWhite, kRed, Attr::Bold});
    r.message_info_text = registry.intern("ckv.message.info.text", Style{kBlack, kLightGray, Attr{}});
    r.message_warning_text = registry.intern("ckv.message.warning.text", Style{kBlack, kLightGray, Attr::Bold});
    r.message_error_text = registry.intern("ckv.message.error.text", Style{kWhite, kRed, Attr::Bold});
    r.message_confirm_text = registry.intern("ckv.message.confirm.text", Style{kBlack, kLightGray, Attr{}});
    r.window_frame_active = registry.intern("ckv.window.frame.active", Style{kWhite, kBlue, Attr::Bold});
    r.window_frame_inactive = registry.intern("ckv.window.frame.inactive", Style{kLightGray, kBlue, Attr{}});
    r.window_title_active = registry.intern("ckv.window.title.active", Style{kWhite, kBlue, Attr::Bold});
    r.window_title_inactive = registry.intern("ckv.window.title.inactive", Style{kLightGray, kBlue, Attr{}});
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
    r.list_selected = registry.intern("ckv.list.selected", Style{kBlack, kWhite, Attr::Underline});
    r.table_header = registry.intern("ckv.table.header", Style{kWhite, kLightGray, Attr{}});
    r.memo_normal = registry.intern("ckv.memo.normal", Style{kBlack, kWhite, Attr{}});
    r.memo_focused = registry.intern("ckv.memo.focused", Style{kBlack, kWhite, Attr::Underline});
    r.option_normal = registry.intern("ckv.option.normal", Style{kBlack, kGreen, Attr{}});
    r.option_focused = registry.intern("ckv.option.focused", Style{kWhite, kGreen, Attr{}});
    r.scrollbar_track = registry.intern("ckv.scrollbar.track", Style{kWhite, kLightGray, Attr{}});
    r.scrollbar_thumb = registry.intern("ckv.scrollbar.thumb", Style{kBlack, kLightGray, Attr{}});
    r.image_fallback = registry.intern("ckv.image.fallback", Style{kBlack, kLightGray, Attr{}});
    r.canvas_fallback = registry.intern("ckv.canvas.fallback", Style{kBlack, kLightGray, Attr{}});
    r.text_view_text = registry.intern("ckv.textview.text", Style{kBlack, kLightGray, Attr{}});
    r.status_line_normal = registry.intern("ckv.statusline.normal", Style{kBlack, kLightGray, Attr{}});
    r.status_line_disabled = registry.intern("ckv.statusline.disabled", Style{kGray, kLightGray, Attr::Dim});
    r.splitter_normal = registry.intern("ckv.splitter.normal", Style{kWhite, kLightGray, Attr{}});
    r.splitter_focused = registry.intern("ckv.splitter.focused", Style{kWhite, kGreen, Attr{}});
    return r;
}

Theme make_classic_theme(const RoleRegistry& registry, const StandardRoles& roles) {
    Theme theme(registry);
    theme.set(roles.desktop_background, Style{kBlue, kLightGray, Attr{}});
    theme.set(roles.dialog_frame, Style{kWhite, kLightGray, Attr{}});
    theme.set(roles.dialog_background, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.label_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.label_mnemonic, Style{kYellow, kLightGray, Attr::Underline});
    theme.set(roles.static_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.button_normal, Style{kBlack, kGreen, Attr{}});
    theme.set(roles.button_focused, Style{kWhite, kGreen, Attr{}});
    theme.set(roles.button_default, Style{kLightCyan, kGreen, Attr{}});
    theme.set(roles.button_shadow, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.input_normal, Style{kBlack, kWhite, Attr{}});
    theme.set(roles.input_focused, Style{kBlack, kWhite, Attr::Underline});
    theme.set(roles.input_invalid, Style{kWhite, kRed, Attr::Bold});
    theme.set(roles.message_info_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.message_warning_text, Style{kBlack, kLightGray, Attr::Bold});
    theme.set(roles.message_error_text, Style{kWhite, kRed, Attr::Bold});
    theme.set(roles.message_confirm_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.window_frame_active, Style{kWhite, kBlue, Attr::Bold});
    theme.set(roles.window_frame_inactive, Style{kLightGray, kBlue, Attr{}});
    theme.set(roles.window_title_active, Style{kWhite, kBlue, Attr::Bold});
    theme.set(roles.window_title_inactive, Style{kLightGray, kBlue, Attr{}});
    theme.set(roles.menu_bar_normal, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.menu_bar_active, Style{kBlack, kGreen, Attr{}});
    theme.set(roles.menu_dropdown_normal, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.menu_dropdown_highlighted, Style{kBlack, kGreen, Attr{}});
    theme.set(roles.menu_dropdown_disabled, Style{kGray, kLightGray, Attr::Dim});
    theme.set(roles.list_normal, Style{kBlack, kWhite, Attr{}});
    theme.set(roles.list_selected, Style{kBlack, kWhite, Attr::Underline});
    theme.set(roles.table_header, Style{kWhite, kLightGray, Attr{}});
    theme.set(roles.memo_normal, Style{kBlack, kWhite, Attr{}});
    theme.set(roles.memo_focused, Style{kBlack, kWhite, Attr::Underline});
    theme.set(roles.option_normal, Style{kBlack, kGreen, Attr{}});
    theme.set(roles.option_focused, Style{kWhite, kGreen, Attr{}});
    theme.set(roles.scrollbar_track, Style{kWhite, kLightGray, Attr{}});
    theme.set(roles.scrollbar_thumb, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.image_fallback, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.canvas_fallback, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.text_view_text, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.status_line_normal, Style{kBlack, kLightGray, Attr{}});
    theme.set(roles.status_line_disabled, Style{kGray, kLightGray, Attr::Dim});
    theme.set(roles.splitter_normal, Style{kWhite, kLightGray, Attr{}});
    theme.set(roles.splitter_focused, Style{kWhite, kGreen, Attr{}});
    return theme;
}

namespace {

// Dark: near-black backgrounds, light text, a muted blue accent.
constexpr Color kDarkBg = Color::rgb(30, 30, 30);
constexpr Color kDarkPanel = Color::rgb(45, 45, 48);
constexpr Color kDarkFg = Color::rgb(220, 220, 220);
constexpr Color kDarkAccent = Color::rgb(0, 90, 158);
constexpr Color kDarkFocus = Color::rgb(70, 70, 76);
constexpr Color kDarkYellow = Color::rgb(220, 200, 80);
constexpr Color kDarkRed = Color::rgb(200, 70, 70);

// Light: light backgrounds, near-black text, a pale blue accent.
constexpr Color kLightBg = Color::rgb(245, 245, 245);
constexpr Color kLightPanel = Color::rgb(225, 225, 225);
constexpr Color kLightFg = Color::rgb(20, 20, 20);
constexpr Color kLightAccent = Color::rgb(190, 215, 245);
constexpr Color kLightFocus = Color::rgb(205, 205, 205);
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
    theme.set(roles.label_mnemonic, Style{kDarkYellow, kDarkPanel, Attr::Underline});
    theme.set(roles.static_text, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.button_normal, Style{kDarkFg, kDarkFocus, Attr{}});
    theme.set(roles.button_focused, Style{kMonoFg, kDarkAccent, Attr::Reverse});
    theme.set(roles.button_default, Style{kDarkYellow, kDarkBg, Attr::Bold});
    theme.set(roles.button_shadow, Style{kDarkBg, kDarkPanel, Attr{}});
    theme.set(roles.input_normal, Style{kDarkFg, kDarkBg, Attr{}});
    theme.set(roles.input_focused, Style{kDarkFg, kDarkBg, Attr::Underline});
    theme.set(roles.input_invalid, Style{kMonoFg, kDarkRed, Attr::Bold});
    theme.set(roles.message_info_text, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.message_warning_text, Style{kDarkYellow, kDarkPanel, Attr::Bold});
    theme.set(roles.message_error_text, Style{kMonoFg, kDarkRed, Attr::Bold});
    theme.set(roles.message_confirm_text, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.window_frame_active, Style{kMonoFg, kDarkFocus, Attr::Bold});
    theme.set(roles.window_frame_inactive, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.window_title_active, Style{kMonoFg, kDarkAccent, Attr::Bold});
    theme.set(roles.window_title_inactive, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.menu_bar_normal, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.menu_bar_active, Style{kMonoFg, kDarkAccent, Attr::Reverse});
    theme.set(roles.menu_dropdown_normal, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.menu_dropdown_highlighted, Style{kMonoFg, kDarkAccent, Attr::Reverse});
    theme.set(roles.menu_dropdown_disabled, Style{kDarkFg, kDarkPanel, Attr::Dim});
    theme.set(roles.list_normal, Style{kDarkFg, kDarkBg, Attr{}});
    theme.set(roles.list_selected, Style{kDarkFg, kDarkBg, Attr::Underline});
    theme.set(roles.table_header, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.memo_normal, Style{kDarkFg, kDarkBg, Attr{}});
    theme.set(roles.memo_focused, Style{kDarkFg, kDarkBg, Attr::Underline});
    theme.set(roles.option_normal, Style{kDarkFg, kDarkFocus, Attr{}});
    theme.set(roles.option_focused, Style{kMonoFg, kDarkAccent, Attr::Reverse});
    theme.set(roles.scrollbar_track, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.scrollbar_thumb, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.image_fallback, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.canvas_fallback, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.text_view_text, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.status_line_normal, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.status_line_disabled, Style{kDarkFg, kDarkPanel, Attr::Dim});
    theme.set(roles.splitter_normal, Style{kDarkFg, kDarkPanel, Attr{}});
    theme.set(roles.splitter_focused, Style{kMonoFg, kDarkAccent, Attr::Reverse});
    return theme;
}

Theme make_light_theme(const RoleRegistry& registry, const StandardRoles& roles) {
    Theme theme(registry);
    theme.set(roles.desktop_background, Style{kLightFocus, kLightBg, Attr{}});
    theme.set(roles.dialog_frame, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.dialog_background, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.label_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.label_mnemonic, Style{kLightFg, kLightPanel, Attr::Underline | Attr::Bold});
    theme.set(roles.static_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.button_normal, Style{kLightFg, kLightFocus, Attr{}});
    theme.set(roles.button_focused, Style{kLightFg, kLightAccent, Attr::Reverse});
    theme.set(roles.button_default, Style{kMonoBg, kLightYellow, Attr::Bold});
    theme.set(roles.button_shadow, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.input_normal, Style{kLightFg, kLightBg, Attr{}});
    theme.set(roles.input_focused, Style{kLightFg, kLightBg, Attr::Underline});
    theme.set(roles.input_invalid, Style{kMonoFg, kLightRed, Attr::Bold});
    theme.set(roles.message_info_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.message_warning_text, Style{kLightFg, kLightYellow, Attr::Bold});
    theme.set(roles.message_error_text, Style{kMonoFg, kLightRed, Attr::Bold});
    theme.set(roles.message_confirm_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.window_frame_active, Style{kLightFg, kLightFocus, Attr::Bold});
    theme.set(roles.window_frame_inactive, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.window_title_active, Style{kLightFg, kLightAccent, Attr::Bold});
    theme.set(roles.window_title_inactive, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.menu_bar_normal, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.menu_bar_active, Style{kLightFg, kLightAccent, Attr::Reverse});
    theme.set(roles.menu_dropdown_normal, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.menu_dropdown_highlighted, Style{kLightFg, kLightAccent, Attr::Reverse});
    theme.set(roles.menu_dropdown_disabled, Style{kLightFg, kLightPanel, Attr::Dim});
    theme.set(roles.list_normal, Style{kLightFg, kLightBg, Attr{}});
    theme.set(roles.list_selected, Style{kLightFg, kLightBg, Attr::Underline});
    theme.set(roles.table_header, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.memo_normal, Style{kLightFg, kLightBg, Attr{}});
    theme.set(roles.memo_focused, Style{kLightFg, kLightBg, Attr::Underline});
    theme.set(roles.option_normal, Style{kLightFg, kLightFocus, Attr{}});
    theme.set(roles.option_focused, Style{kLightFg, kLightAccent, Attr::Reverse});
    theme.set(roles.scrollbar_track, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.scrollbar_thumb, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.image_fallback, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.canvas_fallback, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.text_view_text, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.status_line_normal, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.status_line_disabled, Style{kLightFg, kLightPanel, Attr::Dim});
    theme.set(roles.splitter_normal, Style{kLightFg, kLightPanel, Attr{}});
    theme.set(roles.splitter_focused, Style{kLightFg, kLightAccent, Attr::Reverse});
    return theme;
}

Theme make_mono_theme(const RoleRegistry& registry, const StandardRoles& roles) {
    Theme theme(registry);
    theme.set(roles.desktop_background, Style{kMonoGray, kMonoBg, Attr{}});
    theme.set(roles.dialog_frame, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.dialog_background, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.label_text, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.label_mnemonic, Style{kMonoFg, kMonoBg, Attr::Underline | Attr::Bold});
    theme.set(roles.static_text, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.button_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.button_focused, Style{kMonoBg, kMonoFg, Attr::Reverse});
    theme.set(roles.button_default, Style{kMonoFg, kMonoBg, Attr::Bold});
    theme.set(roles.button_shadow, Style{kMonoGray, kMonoBg, Attr{}});
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
    theme.set(roles.menu_bar_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.menu_bar_active, Style{kMonoBg, kMonoFg, Attr::Reverse});
    theme.set(roles.menu_dropdown_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.menu_dropdown_highlighted, Style{kMonoBg, kMonoFg, Attr::Reverse});
    theme.set(roles.menu_dropdown_disabled, Style{kMonoGray, kMonoBg, Attr::Dim});
    theme.set(roles.list_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.list_selected, Style{kMonoFg, kMonoBg, Attr::Underline});
    theme.set(roles.table_header, Style{kMonoFg, kMonoBg, Attr::Bold});
    theme.set(roles.memo_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.memo_focused, Style{kMonoFg, kMonoBg, Attr::Underline});
    theme.set(roles.option_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.option_focused, Style{kMonoBg, kMonoFg, Attr::Reverse});
    theme.set(roles.scrollbar_track, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.scrollbar_thumb, Style{kMonoGray, kMonoBg, Attr{}});
    theme.set(roles.image_fallback, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.canvas_fallback, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.text_view_text, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.status_line_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.status_line_disabled, Style{kMonoGray, kMonoBg, Attr::Dim});
    theme.set(roles.splitter_normal, Style{kMonoFg, kMonoBg, Attr{}});
    theme.set(roles.splitter_focused, Style{kMonoBg, kMonoFg, Attr::Reverse});
    return theme;
}

}  // namespace ckv::ui
