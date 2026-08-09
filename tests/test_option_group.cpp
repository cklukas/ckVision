// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/option_group.hpp"

#include "cktest.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::CheckGroup;
using ckv::widgets::CheckState;
using ckv::widgets::RadioGroup;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

ckv::KeyEvent key(ckv::Key k, std::string text = "") {
    return ckv::KeyEvent{KeyChord{k, Modifier::None, std::move(text)}};
}

ckv::KeyEvent key(ckv::Key k, Modifier modifier, std::string text) {
    return ckv::KeyEvent{KeyChord{k, modifier, std::move(text)}};
}
}  // namespace

// --- CheckGroup ------------------------------------------------------

CK_TEST(check_group_starts_with_everything_unchecked) {
    Fixture f;
    CheckGroup group({"&A", "&B"});
    CK_CHECK(!group.checked(0));
    CK_CHECK(!group.checked(1));
}

CK_TEST(space_toggles_the_current_item) {
    Fixture f;
    CheckGroup group({"&A", "&B"});
    group.on_key(key(Key::Char, " "));
    CK_CHECK(group.checked(0));
    CK_CHECK(!group.checked(1));
}

CK_TEST(down_arrow_moves_the_cursor_and_toggling_affects_the_new_position) {
    Fixture f;
    CheckGroup group({"&A", "&B"});
    group.on_key(key(Key::Down));
    group.on_key(key(Key::Char, " "));
    CK_CHECK(!group.checked(0));
    CK_CHECK(group.checked(1));
}

CK_TEST(down_arrow_wraps_from_the_last_item_to_the_first) {
    Fixture f;
    CheckGroup group({"&A", "&B"});
    group.on_key(key(Key::Down));
    group.on_key(key(Key::Down));  // wraps back to 0
    group.on_key(key(Key::Char, " "));
    CK_CHECK(group.checked(0));
}

CK_TEST(multiple_items_can_be_checked_independently) {
    Fixture f;
    CheckGroup group({"&A", "&B", "&C"});
    group.set_checked(0, true);
    group.set_checked(2, true);
    CK_CHECK(group.checked(0));
    CK_CHECK(!group.checked(1));
    CK_CHECK(group.checked(2));
}

CK_TEST(mnemonic_key_toggles_the_matching_item_and_moves_the_cursor_to_it) {
    Fixture f;
    CheckGroup group({"&A", "&B"});
    CK_CHECK(group.on_key(key(Key::Char, "b")));  // case-insensitive
    CK_CHECK(group.checked(1));
    group.on_key(key(Key::Char, " "));  // cursor should now be on item 1
    CK_CHECK(!group.checked(1));        // toggled back off
}

CK_TEST(alt_mnemonic_key_toggles_the_matching_check_item_but_ctrl_does_not) {
    Fixture f;
    CheckGroup group({"&A", "&B"});
    CK_CHECK(group.on_key(key(Key::Char, Modifier::Alt, "b")));
    CK_CHECK(group.checked(1));
    CK_CHECK(!group.on_key(key(Key::Char, Modifier::Ctrl, "a")));
    CK_CHECK(!group.checked(0));
}

CK_TEST(tristate_check_group_cycles_through_checked_mixed_and_unchecked) {
    Fixture f;
    CheckGroup group({"&A"});
    group.set_tristate(true);
    group.on_key(key(Key::Char, " "));
    CK_CHECK(group.check_state(0) == CheckState::Checked);
    group.on_key(key(Key::Char, " "));
    CK_CHECK(group.check_state(0) == CheckState::Mixed);
    CK_CHECK(!group.checked(0));
    group.on_key(key(Key::Char, " "));
    CK_CHECK(group.check_state(0) == CheckState::Unchecked);
}

CK_TEST(on_state_changed_reports_mixed_without_losing_the_existing_bool_callback) {
    Fixture f;
    CheckGroup group({"&A"});
    CheckState last_state = CheckState::Unchecked;
    bool last_bool = true;
    group.on_state_changed = [&](std::size_t index, CheckState state) {
        CK_CHECK(index == 0);
        last_state = state;
    };
    group.on_changed = [&](std::size_t index, bool value) {
        CK_CHECK(index == 0);
        last_bool = value;
    };
    group.set_check_state(0, CheckState::Mixed);
    CK_CHECK(last_state == CheckState::Mixed);
    CK_CHECK(last_bool == false);
}

CK_TEST(setting_checked_to_its_current_value_does_not_fire_on_changed) {
    Fixture f;
    CheckGroup group({"&A"});
    int calls = 0;
    group.on_changed = [&](std::size_t, bool) { ++calls; };
    group.set_checked(0, false);  // already false
    CK_CHECK(calls == 0);
}

CK_TEST(on_changed_reports_the_index_and_new_state) {
    Fixture f;
    CheckGroup group({"&A", "&B"});
    std::size_t last_index = 999;
    bool last_state = false;
    group.on_changed = [&](std::size_t i, bool s) {
        last_index = i;
        last_state = s;
    };
    group.set_checked(1, true);
    CK_CHECK(last_index == 1);
    CK_CHECK(last_state == true);
}

CK_TEST(checked_out_of_range_index_aborts) {
    CK_EXPECT_ABORT({
        Fixture f;
        CheckGroup group({"&A"});
        group.checked(5);
    });
}

CK_TEST(clicking_a_row_toggles_that_item) {
    Fixture f;
    CheckGroup group({"&A", "&B", "&C"});
    group.set_bounds(Rect{0, 0, 20, 3});
    group.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 1}, std::nullopt,
                                    Modifier::None});
    CK_CHECK(!group.checked(0));
    CK_CHECK(group.checked(1));
    CK_CHECK(!group.checked(2));
}

CK_TEST(clicking_outside_the_items_is_unhandled) {
    Fixture f;
    CheckGroup group({"&A"});
    group.set_bounds(Rect{0, 0, 20, 1});
    CK_CHECK(!group.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 50},
                                              std::nullopt, Modifier::None}));
}

CK_TEST(check_group_is_a_tab_stop) {
    Fixture f;
    CheckGroup group({"&A"});
    CK_CHECK(group.focusable());
}

CK_TEST(empty_check_group_does_not_crash_on_key_or_mouse) {
    Fixture f;
    CheckGroup group({});
    group.set_bounds(Rect{0, 0, 20, 1});
    group.on_key(key(Key::Char, " "));  // Space is still "handled" (a guarded no-op toggle); must not crash
    CK_CHECK(!group.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 0},
                                              std::nullopt, Modifier::None}));
}

// --- RadioGroup ----------------------------------------------------------

CK_TEST(radio_group_starts_with_nothing_selected) {
    Fixture f;
    RadioGroup group({"&A", "&B"});
    CK_CHECK(group.selected() == -1);
}

CK_TEST(selecting_one_item_deselects_the_previous_one) {
    Fixture f;
    RadioGroup group({"&A", "&B", "&C"});
    group.set_selected(0);
    CK_CHECK(group.selected() == 0);
    group.set_selected(2);
    CK_CHECK(group.selected() == 2);  // exclusive: 0 is implicitly no longer selected
}

CK_TEST(arrow_navigation_also_changes_selection) {
    Fixture f;
    RadioGroup group({"&A", "&B"});
    group.set_selected(0);
    group.on_key(key(Key::Down));
    CK_CHECK(group.selected() == 1);
}

CK_TEST(arrow_navigation_wraps) {
    Fixture f;
    RadioGroup group({"&A", "&B"});
    group.on_key(key(Key::Up));  // wraps to the last item from cursor 0
    CK_CHECK(group.selected() == 1);
}

CK_TEST(set_selected_to_negative_one_clears_the_selection) {
    Fixture f;
    RadioGroup group({"&A", "&B"});
    group.set_selected(1);
    group.set_selected(-1);
    CK_CHECK(group.selected() == -1);
}

CK_TEST(set_selected_out_of_range_is_a_harmless_no_op) {
    Fixture f;
    RadioGroup group({"&A", "&B"});
    group.set_selected(0);
    group.set_selected(99);  // out of range
    CK_CHECK(group.selected() == 0);  // unchanged
    group.set_selected(-2);  // also invalid (only -1 is the sanctioned "none")
    CK_CHECK(group.selected() == 0);
}

CK_TEST(setting_the_same_selection_again_does_not_fire_on_changed) {
    Fixture f;
    RadioGroup group({"&A", "&B"});
    group.set_selected(0);
    int calls = 0;
    group.on_changed = [&](int) { ++calls; };
    group.set_selected(0);
    CK_CHECK(calls == 0);
}

CK_TEST(mnemonic_key_selects_the_matching_item) {
    Fixture f;
    RadioGroup group({"&A", "&B"});
    CK_CHECK(group.on_key(key(Key::Char, "b")));
    CK_CHECK(group.selected() == 1);
}

CK_TEST(alt_mnemonic_key_selects_the_matching_radio_item_but_ctrl_does_not) {
    Fixture f;
    RadioGroup group({"&A", "&B"});
    CK_CHECK(group.on_key(key(Key::Char, Modifier::Alt, "b")));
    CK_CHECK(group.selected() == 1);
    CK_CHECK(!group.on_key(key(Key::Char, Modifier::Ctrl, "a")));
    CK_CHECK(group.selected() == 1);
}

CK_TEST(clicking_a_row_selects_that_item_exclusively) {
    Fixture f;
    RadioGroup group({"&A", "&B", "&C"});
    group.set_bounds(Rect{0, 0, 20, 3});
    group.set_selected(0);
    group.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 2}, std::nullopt,
                                    Modifier::None});
    CK_CHECK(group.selected() == 2);
}

CK_TEST(radio_group_is_a_tab_stop) {
    Fixture f;
    RadioGroup group({"&A"});
    CK_CHECK(group.focusable());
}

CK_TEST(empty_radio_group_does_not_crash_on_key_or_mouse) {
    Fixture f;
    RadioGroup group({});
    group.set_bounds(Rect{0, 0, 20, 1});
    group.on_key(key(Key::Char, " "));  // must not crash
    CK_CHECK(!group.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 0},
                                              std::nullopt, Modifier::None}));
}
