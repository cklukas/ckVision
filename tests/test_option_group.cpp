// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/option_group.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
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

CK_TEST(option_groups_can_use_an_exact_measured_column_width) {
    CheckGroup checks({"A much longer check choice"});
    checks.set_column_width(12);
    CK_CHECK((checks.horizontal_size_hint() == ckv::ui::SizeHint{12, 12, 12}));
    RadioGroup radios({"A much longer radio choice"});
    radios.set_column_width(14);
    CK_CHECK((radios.horizontal_size_hint() == ckv::ui::SizeHint{14, 14, 14}));
    radios.set_column_width(0);
    CK_CHECK(radios.horizontal_size_hint().preferred > 14);
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

CK_TEST(check_group_mnemonics_use_the_dialog_mnemonic_accent) {
    Fixture f;
    CheckGroup group({"&Auto save"});
    group.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    group.set_bounds(Rect{0, 0, 16, 1});
    Surface s(ckv::Size{16, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 16, 1});
    group.draw(painter);

    CK_CHECK(s.at(Point{4, 0}).grapheme() == "A");
    CK_CHECK(s.at(Point{4, 0}).style().fg == f.theme.resolve(f.roles.label_mnemonic).fg);
    CK_CHECK(s.at(Point{4, 0}).style().bg == f.theme.resolve(f.roles.option_normal).bg);
    CK_CHECK(s.at(Point{5, 0}).style() == f.theme.resolve(f.roles.option_normal));
}

CK_TEST(check_group_uses_square_brackets_and_an_uppercase_x_for_checked_items) {
    Fixture f;
    CheckGroup group({"Unchecked", "Checked"});
    group.set_checked(1, true);
    group.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    group.set_bounds(Rect{0, 0, 16, 2});
    Surface s(ckv::Size{16, 2}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 16, 2});
    group.draw(painter);

    CK_CHECK(s.at(Point{0, 0}).grapheme() == "[");
    CK_CHECK(s.at(Point{1, 0}).grapheme() == " ");
    CK_CHECK(s.at(Point{2, 0}).grapheme() == "]");
    CK_CHECK(s.at(Point{0, 1}).grapheme() == "[");
    CK_CHECK(s.at(Point{1, 1}).grapheme() == "X");
    CK_CHECK(s.at(Point{2, 1}).grapheme() == "]");
}

CK_TEST(captioned_check_group_indents_choices_beneath_its_caption) {
    Fixture f;
    CheckGroup group({"Choice"});
    group.set_group_label("Options");
    group.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    group.set_bounds(Rect{0, 0, 16, 2});
    Surface s(ckv::Size{16, 2}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 16, 2});
    group.draw(painter);

    CK_CHECK(s.at(Point{0, 0}).grapheme() == "O");
    CK_CHECK(s.at(Point{1, 1}).grapheme() == "[");
    CK_CHECK(s.at(Point{5, 1}).grapheme() == "C");
}

CK_TEST(a_check_group_caption_owns_a_row_and_turns_white_with_group_focus) {
    Fixture f;
    CheckGroup group({"First", "Second"});
    group.set_group_label("Choices");
    group.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    group.set_bounds(Rect{0, 0, 16, 3});
    CK_CHECK((group.vertical_size_hint() == ckv::ui::SizeHint{3, 3, 3}));

    Surface normal(ckv::Size{16, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter normal_painter(normal, Rect{0, 0, 16, 3});
    group.draw(normal_painter);
    CK_CHECK(normal.at(Point{0, 0}).grapheme() == "C");
    CK_CHECK(normal.at(Point{0, 0}).style() == f.theme.resolve(f.roles.label_text));
    CK_CHECK(normal.at(Point{1, 1}).grapheme() == "[");
    CK_CHECK(!group.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{2, 0},
                                              std::nullopt, Modifier::None}));
    CK_CHECK(group.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{2, 1},
                                             std::nullopt, Modifier::None}));
    CK_CHECK(group.checked(0));

    group.on_focus(ckv::FocusEvent{true});
    Surface focused(ckv::Size{16, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter focused_painter(focused, Rect{0, 0, 16, 3});
    group.draw(focused_painter);
    CK_CHECK(focused.at(Point{0, 0}).style().fg == f.theme.resolve(f.roles.option_focused).fg);
    CK_CHECK(focused.at(Point{0, 0}).style().bg == f.theme.resolve(f.roles.label_text).bg);
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

CK_TEST(radio_group_uses_round_brackets_and_a_round_dot_for_the_selection) {
    Fixture f;
    RadioGroup group({"Unselected", "Selected"});
    group.set_selected(1);
    group.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    group.set_bounds(Rect{0, 0, 16, 2});
    Surface s(ckv::Size{16, 2}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 16, 2});
    group.draw(painter);

    CK_CHECK(s.at(Point{0, 0}).grapheme() == "(");
    CK_CHECK(s.at(Point{1, 0}).grapheme() == " ");
    CK_CHECK(s.at(Point{2, 0}).grapheme() == ")");
    CK_CHECK(s.at(Point{0, 1}).grapheme() == "(");
    CK_CHECK(s.at(Point{1, 1}).grapheme() == "•");
    CK_CHECK(s.at(Point{2, 1}).grapheme() == ")");
}

CK_TEST(a_radio_group_caption_owns_a_row_and_turns_white_with_group_focus) {
    Fixture f;
    RadioGroup group({"First", "Second"});
    group.set_group_label("Mode");
    group.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    group.set_bounds(Rect{0, 0, 16, 3});
    CK_CHECK((group.vertical_size_hint() == ckv::ui::SizeHint{3, 3, 3}));

    Surface normal(ckv::Size{16, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter normal_painter(normal, Rect{0, 0, 16, 3});
    group.draw(normal_painter);
    CK_CHECK(normal.at(Point{0, 0}).grapheme() == "M");
    CK_CHECK(normal.at(Point{0, 0}).style() == f.theme.resolve(f.roles.label_text));
    CK_CHECK(normal.at(Point{1, 1}).grapheme() == "(");
    CK_CHECK(!group.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{2, 0},
                                              std::nullopt, Modifier::None}));
    CK_CHECK(group.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{2, 2},
                                             std::nullopt, Modifier::None}));
    CK_CHECK(group.selected() == 1);

    group.on_focus(ckv::FocusEvent{true});
    Surface focused(ckv::Size{16, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter focused_painter(focused, Rect{0, 0, 16, 3});
    group.draw(focused_painter);
    CK_CHECK(focused.at(Point{0, 0}).style().fg == f.theme.resolve(f.roles.option_focused).fg);
    CK_CHECK(focused.at(Point{0, 0}).style().bg == f.theme.resolve(f.roles.label_text).bg);
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

CK_TEST(the_cursor_follows_a_programmatic_selection) {
    // A dialog that opens with row N selected has told the reader where they
    // are, and their first arrow must move from THERE. It used to move from
    // row 0 wherever the selection was, so Up in a two-row group re-selected
    // the very row the reader was arrowing away from.
    Fixture f;
    RadioGroup group({"&A", "&B", "&C"});
    group.set_selected(2);
    group.on_key(key(Key::Up));
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

CK_TEST(space_ticks_a_box_and_enter_is_left_for_the_form_around_it) {
    // Regression: the group used to toggle on Enter and report it handled,
    // so a dialog's default button could not be reached from the keyboard
    // while any box had focus — which, in a settings dialog, is from the
    // moment it opens. Space is what ticks a box; Enter belongs to the form.
    Fixture f;
    CheckGroup group({"&One", "&Two"});
    group.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});

    CK_CHECK(group.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::None, " "}}));
    CK_CHECK(group.checked(0));

    CK_CHECK(!group.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}}));
    CK_CHECK(group.checked(0));  // unchanged: Enter was not ours to act on
}

CK_TEST(a_radio_group_likewise_leaves_enter_to_the_form) {
    Fixture f;
    RadioGroup group({"&One", "&Two"});
    group.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});

    CK_CHECK(group.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Down, ckv::Modifier::None, ""}}));
    CK_CHECK(group.selected() == 1);
    // Arrows already select as they move, so Enter had nothing left to do.
    CK_CHECK(!group.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}}));
    CK_CHECK(group.selected() == 1);
}
