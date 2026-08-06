// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/standard_roles.hpp"

namespace ckv::ui {
namespace {

// The classic blue-desktop palette (public, well-documented CUA-era
// convention — cyan-on-blue dialogs, black-on-white default buttons —
// not derived from any framework's source, per AGENTS.md).
constexpr Color kBlue = Color::rgb(0, 0, 170);
constexpr Color kCyan = Color::rgb(0, 170, 170);
constexpr Color kWhite = Color::rgb(255, 255, 255);
constexpr Color kBlack = Color::rgb(0, 0, 0);
constexpr Color kGray = Color::rgb(170, 170, 170);
constexpr Color kYellow = Color::rgb(255, 255, 85);
constexpr Color kRed = Color::rgb(170, 0, 0);

}  // namespace

StandardRoles intern_standard_roles(RoleRegistry& registry) {
    StandardRoles r;
    r.dialog_frame = registry.intern("ckv.dialog.frame", Style{kWhite, kCyan, Attr{}});
    r.dialog_background = registry.intern("ckv.dialog.background", Style{kBlack, kCyan, Attr{}});
    r.label_text = registry.intern("ckv.label.text", Style{kBlack, kCyan, Attr{}});
    r.label_mnemonic = registry.intern("ckv.label.mnemonic", Style{kYellow, kCyan, Attr::Underline});
    r.static_text = registry.intern("ckv.static.text", Style{kBlack, kCyan, Attr{}});
    r.button_normal = registry.intern("ckv.button.normal", Style{kBlack, kGray, Attr{}});
    r.button_focused = registry.intern("ckv.button.focused", Style{kWhite, kGray, Attr::Reverse});
    r.button_default = registry.intern("ckv.button.default", Style{kYellow, kBlack, Attr::Bold});
    r.input_normal = registry.intern("ckv.input.normal", Style{kBlack, kWhite, Attr{}});
    r.input_focused = registry.intern("ckv.input.focused", Style{kBlack, kWhite, Attr::Underline});
    r.input_invalid = registry.intern("ckv.input.invalid", Style{kWhite, kRed, Attr::Bold});
    r.window_frame_active = registry.intern("ckv.window.frame.active", Style{kWhite, kGray, Attr::Bold});
    r.window_frame_inactive = registry.intern("ckv.window.frame.inactive", Style{kBlack, kGray, Attr{}});
    r.window_title_active = registry.intern("ckv.window.title.active", Style{kWhite, kBlue, Attr::Bold});
    r.window_title_inactive = registry.intern("ckv.window.title.inactive", Style{kBlack, kGray, Attr{}});
    r.menu_bar_normal = registry.intern("ckv.menu.bar.normal", Style{kBlack, kGray, Attr{}});
    r.menu_bar_active = registry.intern("ckv.menu.bar.active", Style{kWhite, kBlack, Attr::Reverse});
    r.menu_dropdown_normal = registry.intern("ckv.menu.dropdown.normal", Style{kBlack, kGray, Attr{}});
    r.menu_dropdown_highlighted =
        registry.intern("ckv.menu.dropdown.highlighted", Style{kWhite, kBlack, Attr::Reverse});
    r.menu_dropdown_disabled = registry.intern("ckv.menu.dropdown.disabled", Style{kBlack, kGray, Attr::Dim});
    return r;
}

Theme make_classic_theme(const RoleRegistry& registry, const StandardRoles& roles) {
    Theme theme(registry);
    theme.set(roles.dialog_frame, Style{kWhite, kCyan, Attr{}});
    theme.set(roles.dialog_background, Style{kBlack, kCyan, Attr{}});
    theme.set(roles.label_text, Style{kBlack, kCyan, Attr{}});
    theme.set(roles.label_mnemonic, Style{kYellow, kCyan, Attr::Underline});
    theme.set(roles.static_text, Style{kBlack, kCyan, Attr{}});
    theme.set(roles.button_normal, Style{kBlack, kGray, Attr{}});
    theme.set(roles.button_focused, Style{kWhite, kGray, Attr::Reverse});
    theme.set(roles.button_default, Style{kYellow, kBlack, Attr::Bold});
    theme.set(roles.input_normal, Style{kBlack, kWhite, Attr{}});
    theme.set(roles.input_focused, Style{kBlack, kWhite, Attr::Underline});
    theme.set(roles.input_invalid, Style{kWhite, kRed, Attr::Bold});
    theme.set(roles.window_frame_active, Style{kWhite, kGray, Attr::Bold});
    theme.set(roles.window_frame_inactive, Style{kBlack, kGray, Attr{}});
    theme.set(roles.window_title_active, Style{kWhite, kBlue, Attr::Bold});
    theme.set(roles.window_title_inactive, Style{kBlack, kGray, Attr{}});
    theme.set(roles.menu_bar_normal, Style{kBlack, kGray, Attr{}});
    theme.set(roles.menu_bar_active, Style{kWhite, kBlack, Attr::Reverse});
    theme.set(roles.menu_dropdown_normal, Style{kBlack, kGray, Attr{}});
    theme.set(roles.menu_dropdown_highlighted, Style{kWhite, kBlack, Attr::Reverse});
    theme.set(roles.menu_dropdown_disabled, Style{kBlack, kGray, Attr::Dim});
    return theme;
}

}  // namespace ckv::ui
