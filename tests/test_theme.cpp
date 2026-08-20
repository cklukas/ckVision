// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/theme.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::Attr;
using ckv::Color;
using ckv::Style;
using ckv::ui::kInvalidRole;
using ckv::ui::RoleRegistry;
using ckv::ui::Theme;

namespace {
Style style_a() { return Style{Color::rgb(1, 2, 3), Color::rgb(4, 5, 6), Attr{}}; }
Style style_b() { return Style{Color::rgb(9, 8, 7), Color::rgb(6, 5, 4), Attr::Bold}; }
}  // namespace

// --- RoleRegistry --------------------------------------------------------

CK_TEST(intern_assigns_sequential_ids_starting_at_zero) {
    RoleRegistry reg;
    CK_CHECK(reg.intern("a", style_a()) == 0);
    CK_CHECK(reg.intern("b", style_a()) == 1);
    CK_CHECK(reg.size() == 2);
}

CK_TEST(re_interning_the_same_name_with_the_same_fallback_is_idempotent) {
    RoleRegistry reg;
    const auto first = reg.intern("ckv.button.normal", style_a());
    const auto second = reg.intern("ckv.button.normal", style_a());
    CK_CHECK(first == second);
    CK_CHECK(reg.size() == 1);  // no duplicate entry created
}

CK_TEST(find_returns_invalid_role_for_a_name_never_interned) {
    RoleRegistry reg;
    reg.intern("a", style_a());
    CK_CHECK(reg.find("nonexistent") == kInvalidRole);
}

CK_TEST(find_returns_the_id_of_a_previously_interned_name) {
    RoleRegistry reg;
    const auto id = reg.intern("ckv.label.text", style_a());
    CK_CHECK(reg.find("ckv.label.text") == id);
}

CK_TEST(fallback_returns_the_style_supplied_at_intern_time) {
    RoleRegistry reg;
    const auto id = reg.intern("a", style_b());
    CK_CHECK(reg.fallback(id) == style_b());
}

CK_TEST(name_returns_the_interned_role_name) {
    RoleRegistry reg;
    const auto id = reg.intern("ckv.dialog.frame", style_a());
    CK_CHECK(reg.name(id) == "ckv.dialog.frame");
}

// --- Theme -----------------------------------------------------------------

CK_TEST(resolve_before_any_set_call_returns_the_registrys_fallback) {
    RoleRegistry reg;
    const auto id = reg.intern("a", style_a());
    Theme theme(reg);
    CK_CHECK(theme.resolve(id) == style_a());
}

CK_TEST(resolve_after_set_returns_the_override_not_the_fallback) {
    RoleRegistry reg;
    const auto id = reg.intern("a", style_a());
    Theme theme(reg);
    theme.set(id, style_b());
    CK_CHECK(theme.resolve(id) == style_b());
}

CK_TEST(resolve_for_a_role_registered_after_theme_construction_still_falls_back) {
    RoleRegistry reg;
    Theme theme(reg);  // constructed before any role exists
    const auto id = reg.intern("late", style_b());
    CK_CHECK(theme.resolve(id) == style_b());
}

CK_TEST(set_can_be_called_multiple_times_and_the_last_call_wins) {
    RoleRegistry reg;
    const auto id = reg.intern("a", style_a());
    Theme theme(reg);
    theme.set(id, style_b());
    theme.set(id, style_a());
    CK_CHECK(theme.resolve(id) == style_a());
}

CK_TEST(setting_a_high_numbered_role_does_not_disturb_lower_roles_defaults) {
    RoleRegistry reg;
    const auto low = reg.intern("low", style_a());
    const auto high = reg.intern("high", style_b());
    Theme theme(reg);
    theme.set(high, style_a());
    CK_CHECK(theme.resolve(low) == style_a());   // untouched fallback
    CK_CHECK(theme.resolve(high) == style_a());  // explicit override
}

CK_TEST(two_themes_over_the_same_registry_are_independent) {
    RoleRegistry reg;
    const auto id = reg.intern("a", style_a());
    Theme theme1(reg);
    Theme theme2(reg);
    theme1.set(id, style_b());
    CK_CHECK(theme1.resolve(id) == style_b());
    CK_CHECK(theme2.resolve(id) == style_a());  // theme2 never touched
}
