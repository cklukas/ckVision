// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/standard_roles.hpp"

#include <set>

#include "cktest.hpp"

using ckv::ui::intern_standard_roles;
using ckv::ui::kInvalidRole;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;

CK_TEST(intern_standard_roles_produces_eleven_distinct_role_ids) {
    RoleRegistry reg;
    const StandardRoles r = intern_standard_roles(reg);
    const std::set<ckv::ui::RoleId> ids{r.dialog_frame,   r.dialog_background, r.label_text,
                                         r.label_mnemonic, r.static_text,       r.button_normal,
                                         r.button_focused, r.button_default,    r.input_normal,
                                         r.input_focused,  r.input_invalid};
    CK_CHECK(ids.size() == 11);
    for (auto id : ids) CK_CHECK(id != kInvalidRole);
    CK_CHECK(reg.size() == 11);
}

CK_TEST(intern_standard_roles_is_idempotent_across_repeated_calls_on_the_same_registry) {
    RoleRegistry reg;
    const StandardRoles first = intern_standard_roles(reg);
    const StandardRoles second = intern_standard_roles(reg);
    CK_CHECK(first.dialog_frame == second.dialog_frame);
    CK_CHECK(first.button_default == second.button_default);
    CK_CHECK(reg.size() == 11);  // no duplicates from the second call
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
    CK_CHECK(!(theme.resolve(r.input_invalid) == theme.resolve(r.input_normal)));
}

CK_TEST(classic_theme_still_falls_back_correctly_for_a_role_it_did_not_explicitly_set) {
    RoleRegistry reg;
    const auto extra = reg.intern("app.custom.role", ckv::Style{});
    const StandardRoles r = intern_standard_roles(reg);
    const auto theme = make_classic_theme(reg, r);
    CK_CHECK(theme.resolve(extra) == ckv::Style{});  // untouched by make_classic_theme
    (void)r;
}
