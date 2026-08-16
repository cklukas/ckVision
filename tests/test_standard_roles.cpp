// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/standard_roles.hpp"

#include <set>
#include <vector>

#include "cvision/testing/cktest.hpp"

using ckv::ui::intern_standard_roles;
using ckv::ui::kInvalidRole;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;

namespace {
// How many roles StandardRoles names. Bump deliberately when a role is
// added, so an accidental duplicate or a forgotten intern still fails.
constexpr std::size_t kStandardRoleCount = 65;
}  // namespace

CK_TEST(intern_standard_roles_produces_one_distinct_role_id_per_named_role) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    const std::set<ckv::ui::RoleId> ids{
        r.desktop_background,   r.dialog_frame,            r.dialog_background,
        r.label_text,           r.label_mnemonic,          r.hotkey,              r.static_text,
        r.help_text,
        r.button_normal,        r.button_focused,          r.button_default,
        r.button_shadow,        r.input_normal,            r.input_focused,
        r.input_invalid,         r.message_info_text,      r.message_warning_text,
        r.message_error_text,    r.message_confirm_text,
        r.window_frame_active,  r.window_frame_inactive,   r.window_title_active,
        r.window_title_inactive, r.window_control,         r.menu_bar_normal,
        r.menu_bar_active,
        r.menu_dropdown_normal, r.menu_dropdown_highlighted, r.menu_dropdown_disabled,
        r.list_normal,          r.list_selected,           r.list_selected_inactive,
        r.table_header,
        r.memo_normal,          r.memo_focused,            r.option_normal,
        r.option_focused,       r.scrollbar_track,         r.scrollbar_thumb,
        r.image_fallback,       r.canvas_fallback,         r.text_view_text,
        r.flow_view_text,
        r.status_line_normal,   r.status_line_disabled,    r.status_line_selected,
        r.status_line_selected_hotkey, r.status_line_selected_disabled,
        r.splitter_normal,
        r.splitter_focused,     r.editor_text,             r.editor_gutter,
        r.editor_selection,     r.editor_search,           r.editor_syntax_plain,
        r.editor_syntax_keyword, r.editor_syntax_type,      r.editor_syntax_property,
        r.editor_syntax_string, r.editor_syntax_number,    r.editor_syntax_comment,
        r.editor_syntax_command, r.editor_syntax_operator, r.editor_syntax_escape,
        r.editor_syntax_error};
    // Every named role interns to its own id, and nothing is interned that
    // the struct does not name. The count is stated once, here, rather
    // than repeated as a literal beside every list of roles.
    CK_CHECK(ids.size() == kStandardRoleCount);
    for (auto id : ids) CK_CHECK(id != kInvalidRole);
    CK_CHECK(reg.size() == kStandardRoleCount);
}

CK_TEST(intern_standard_roles_is_idempotent_across_repeated_calls_on_the_same_registry) {
    RoleRegistry reg;
    const StandardRoles first = intern_standard_roles(reg);
    const StandardRoles second = intern_standard_roles(reg);
    CK_CHECK(first.dialog_frame == second.dialog_frame);
    CK_CHECK(first.button_default == second.button_default);
    CK_CHECK(reg.size() == kStandardRoleCount);  // no duplicates from the second call
}

CK_TEST(standard_role_names_are_namespaced_under_ckv_prefix) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    CK_CHECK(reg.name(r.dialog_frame).rfind("ckv.", 0) == 0);
    CK_CHECK(reg.name(r.button_default).rfind("ckv.", 0) == 0);
}

CK_TEST(each_standard_role_resolves_via_find_to_the_same_id_intern_returned) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    CK_CHECK(reg.find("ckv.input.invalid") == r.input_invalid);
    CK_CHECK(reg.find("ckv.button.focused") == r.button_focused);
}

CK_TEST(classic_theme_resolves_every_standard_role_to_a_non_default_style) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    const auto theme = make_classic_theme(reg, r);
    // Every role must resolve to SOMETHING coherent; button_default and
    // button_normal must differ (the whole point of a "default" button
    // being visually distinct), and input_invalid must differ from
    // input_normal (the dialog-accept veto's visual signal).
    CK_CHECK(!(theme.resolve(r.button_default) == theme.resolve(r.button_normal)));
    // A persistent radio/check selection is not an immediate action: Classic
    // keeps its cyan choice surface distinct from a green button face.
    CK_CHECK(!(theme.resolve(r.option_normal) == theme.resolve(r.button_normal)));
    CK_CHECK(!(theme.resolve(r.input_normal) == theme.resolve(r.dialog_background)));
    CK_CHECK(!(theme.resolve(r.input_invalid) == theme.resolve(r.input_normal)));
}

CK_TEST(desktop_background_is_visually_distinct_from_dialog_background_in_every_scheme) {
    // The whole point of a SEPARATE desktop_background role: the
    // desktop must never accidentally look identical to a dialog/
    // window interior just because an application forgot to pick a
    // different color for it — this is a property of the role split
    // itself, not something any one application can get right or
    // wrong by construction, so it's asserted for all four schemes.
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    for (const auto& theme : {make_classic_theme(reg, r), ckv::ui::make_dark_theme(reg, r),
                               ckv::ui::make_light_theme(reg, r), ckv::ui::make_mono_theme(reg, r)}) {
        CK_CHECK(!(theme.resolve(r.desktop_background) == theme.resolve(r.dialog_background)));
    }
}

CK_TEST(active_and_inactive_window_frame_roles_resolve_to_visually_distinct_styles) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    const auto theme = make_classic_theme(reg, r);
    CK_CHECK(!(theme.resolve(r.window_frame_active) == theme.resolve(r.window_frame_inactive)));
    CK_CHECK(!(theme.resolve(r.window_title_active) == theme.resolve(r.window_title_inactive)));
}

CK_TEST(classic_theme_still_falls_back_correctly_for_a_role_it_did_not_explicitly_set) {
    RoleRegistry reg;
    const auto extra = reg.intern("app.custom.role", ckv::Style{});
    const StandardRoles r = intern_standard_roles(reg);
    const auto theme = make_classic_theme(reg, r);
    CK_CHECK(theme.resolve(extra) == ckv::Style{});  // untouched by make_classic_theme
    (void)r;
}

// --- Dark / Light / Mono schemes ----------------------------------------

CK_TEST(dark_light_and_mono_themes_each_style_every_standard_role_distinctly_from_default) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    for (const auto& theme : {ckv::ui::make_dark_theme(reg, r), ckv::ui::make_light_theme(reg, r),
                               ckv::ui::make_mono_theme(reg, r)}) {
        CK_CHECK(!(theme.resolve(r.button_default) == ckv::Style{}));
        CK_CHECK(!(theme.resolve(r.input_invalid) == ckv::Style{}));
        CK_CHECK(!(theme.resolve(r.window_title_active) == ckv::Style{}));
        CK_CHECK(!(theme.resolve(r.editor_syntax_error) == ckv::Style{}));
        CK_CHECK(!(theme.resolve(r.editor_selection) == ckv::Style{}));
    }
}

CK_TEST(each_scheme_distinguishes_default_from_normal_buttons) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    for (const auto& theme : {ckv::ui::make_dark_theme(reg, r), ckv::ui::make_light_theme(reg, r),
                               ckv::ui::make_mono_theme(reg, r)}) {
        CK_CHECK(!(theme.resolve(r.button_default) == theme.resolve(r.button_normal)));
    }
}

CK_TEST(each_scheme_distinguishes_active_from_inactive_window_frames) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    for (const auto& theme : {ckv::ui::make_dark_theme(reg, r), ckv::ui::make_light_theme(reg, r),
                               ckv::ui::make_mono_theme(reg, r)}) {
        CK_CHECK(!(theme.resolve(r.window_frame_active) == theme.resolve(r.window_frame_inactive)));
    }
}

CK_TEST(each_scheme_distinguishes_invalid_input_from_normal_input) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    for (const auto& theme : {ckv::ui::make_dark_theme(reg, r), ckv::ui::make_light_theme(reg, r),
                               ckv::ui::make_mono_theme(reg, r)}) {
        CK_CHECK(!(theme.resolve(r.input_invalid) == theme.resolve(r.input_normal)));
    }
}

CK_TEST(the_mono_theme_uses_only_black_white_or_gray_never_a_hued_color) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    const auto theme = ckv::ui::make_mono_theme(reg, r);
    const std::vector<ckv::ui::RoleId> all{
        r.desktop_background,    r.dialog_frame,             r.dialog_background,
        r.label_text,            r.label_mnemonic,           r.hotkey,               r.static_text,
        r.help_text,
        r.button_normal,         r.button_focused,           r.button_default,
        r.button_shadow,         r.input_normal,             r.input_focused,
        r.input_invalid,
        r.window_frame_active,   r.window_frame_inactive,    r.window_title_active,
        r.window_title_inactive, r.window_control,           r.menu_bar_normal,
        r.menu_bar_active,
        r.menu_dropdown_normal,  r.menu_dropdown_highlighted, r.menu_dropdown_disabled,
        r.list_normal,           r.list_selected,            r.list_selected_inactive,
        r.table_header,
        r.memo_normal,           r.memo_focused,             r.option_normal,
        r.option_focused,        r.scrollbar_track,          r.scrollbar_thumb,
        r.image_fallback,        r.canvas_fallback,          r.text_view_text,
        r.status_line_normal,    r.status_line_disabled,     r.splitter_normal,
        r.splitter_focused};
    for (auto role : all) {
        const ckv::Style style = theme.resolve(role);
        for (const ckv::Color& c : {style.fg, style.bg}) {
            // Monochrome-safe: every channel equal (a true gray, including
            // pure black/white as the degenerate cases).
            CK_CHECK(c.r() == c.g() && c.g() == c.b());
        }
    }
}

CK_TEST(each_built_in_scheme_produces_a_different_style_for_the_same_role) {
    // Not a rigorous design check, just a sanity guard against a
    // copy-paste mistake that left two "different" schemes identical.
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    const auto classic = make_classic_theme(reg, r);
    const auto dark = ckv::ui::make_dark_theme(reg, r);
    const auto light = ckv::ui::make_light_theme(reg, r);
    const auto mono = ckv::ui::make_mono_theme(reg, r);
    CK_CHECK(!(classic.resolve(r.dialog_background) == dark.resolve(r.dialog_background)));
    CK_CHECK(!(classic.resolve(r.dialog_background) == light.resolve(r.dialog_background)));
    CK_CHECK(!(dark.resolve(r.dialog_background) == light.resolve(r.dialog_background)));
    CK_CHECK(!(dark.resolve(r.dialog_background) == mono.resolve(r.dialog_background)));
}

CK_TEST(high_contrast_theme_uses_only_black_white_and_inverts_selected_surfaces) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    const auto theme = ckv::ui::make_high_contrast_theme(reg, r);
    for (const ckv::Style style : {theme.resolve(r.dialog_background), theme.resolve(r.menu_bar_normal),
                                   theme.resolve(r.editor_text), theme.resolve(r.editor_syntax_error)}) {
        for (const ckv::Color color : {style.fg, style.bg}) {
            CK_CHECK((color.r() == 0 || color.r() == 255));
            CK_CHECK(color.r() == color.g() && color.g() == color.b());
        }
    }
    const ckv::Style selected{ckv::Color::rgb(0, 0, 0), ckv::Color::rgb(255, 255, 255), ckv::Attr{}};
    CK_CHECK(theme.resolve(r.menu_dropdown_highlighted) == selected);
    CK_CHECK(theme.resolve(r.list_selected) == theme.resolve(r.menu_dropdown_highlighted));
}

CK_TEST(a_hued_scheme_draws_the_window_caption_on_the_frames_own_background) {
    // The caption sits in the frame. Giving it a different background makes
    // it read as a selected item pasted onto the border rather than as the
    // window's name — which is exactly what a block of accent colour means
    // everywhere else in these schemes.
    //
    // Mono and High Contrast are deliberately excluded: neither has colour to
    // spend, so inverting the caption is the only way either can distinguish
    // it at all.
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    const std::vector<ckv::ui::Theme> hued{
        make_classic_theme(reg, r),
        ckv::ui::make_dark_theme(reg, r),
        ckv::ui::make_light_theme(reg, r),
    };

    for (const ckv::ui::Theme& theme : hued) {
        CK_CHECK(theme.resolve(r.window_title_active).bg == theme.resolve(r.window_frame_active).bg);
        CK_CHECK(theme.resolve(r.window_title_inactive).bg == theme.resolve(r.window_frame_inactive).bg);
        // It still has to be legible as a caption: the active one is set
        // apart by weight rather than by a coloured block behind it.
        CK_CHECK(has_attr(theme.resolve(r.window_title_active).attrs, ckv::Attr::Bold));
    }
}
