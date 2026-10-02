// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/option_group.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/core/clock.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/window.hpp"

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
    // Focus is the Application's to give (D-065), so the fixture carries one
    // for the tests that need a group to hold it.
    ckv::term::HeadlessTerminal terminal{ckv::Size{40, 10}};
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, &app}; }
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
    group.set_context(f.ctx());
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

    f.app.set_focus(&group);
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
    group.set_context(f.ctx());
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

    f.app.set_focus(&group);
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

// --- captions with mnemonics, choices in columns (D-068) ------------------

CK_TEST(a_caption_marks_its_mnemonic_and_draws_without_the_marker) {
    Fixture f;
    RadioGroup group({"First", "Second"});
    group.set_group_label("&Mode:");
    CK_CHECK(group.group_label() == "&Mode:");
    CK_CHECK(group.group_mnemonic() == "M");
    group.set_context(f.ctx());
    group.set_bounds(Rect{0, 0, 16, 3});
    Surface s(ckv::Size{16, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 16, 3});
    group.draw(painter);
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "M");
    CK_CHECK(s.at(Point{1, 0}).grapheme() == "o");
    CK_CHECK(s.at(Point{5, 0}).grapheme() == " ");
    CK_CHECK(s.at(Point{0, 0}).style().fg == f.theme.resolve(f.roles.label_mnemonic).fg);
    CK_CHECK(s.at(Point{1, 0}).style() == f.theme.resolve(f.roles.label_text));
    // A caption without a marker has no mnemonic to route.
    CheckGroup plain({"Only"});
    plain.set_group_label("Options");
    CK_CHECK(plain.group_mnemonic().empty());
}

CK_TEST(alt_and_the_caption_letter_focus_the_group_from_anywhere_in_its_window) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);

    auto* window = app.root().add(std::make_unique<ckv::widgets::Window>("Form"));
    window->set_bounds(Rect{0, 0, 40, 10});
    auto& pane = window->content_pane();
    auto* flags = pane.add(std::make_unique<CheckGroup>(std::vector<std::string>{"&Optimize"}));
    flags->set_group_label("&Compilation");
    auto* target = pane.add(std::make_unique<RadioGroup>(std::vector<std::string>{"&Static", "S&hared"}));
    target->set_group_label("&Library");
    app.set_focus(flags);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "l"}}));
    CK_CHECK(app.focused() == target);
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "C"}}));
    CK_CHECK(app.focused() == flags);
    // The letter alone, without Alt, is not a route: it is the focused
    // group's own key, and "l" marks no choice of the check group.
    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "l"}}));
    CK_CHECK(app.focused() == flags);
}

CK_TEST(choices_flow_into_columns_row_major_each_as_wide_as_the_widest) {
    Fixture f;
    RadioGroup group({"Keep", "On", "Off", "Reset"});
    group.set_columns(4);
    CK_CHECK(group.columns() == 4);
    // One row packs its choices: each column is as wide as the one choice
    // in it, and three gaps of two lie between them.
    CK_CHECK((group.horizontal_size_hint() == ckv::ui::SizeHint{36, 36, 36}));
    CK_CHECK((group.vertical_size_hint() == ckv::ui::SizeHint{1, 1, 1}));
    group.set_selected(1);
    group.set_context(f.ctx());
    group.set_bounds(Rect{0, 0, 36, 1});
    Surface s(ckv::Size{36, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 36, 1});
    group.draw(painter);
    std::string row;
    for (int x = 0; x < 36; ++x) row += s.at(Point{x, 0}).grapheme();
    CK_CHECK(row == "( ) Keep  (•) On  ( ) Off  ( ) Reset");

    // Two rows align as a table, each column as wide as its widest choice.
    RadioGroup table({"Keep", "Reset/Normal", "Double", "Single acct"});
    table.set_columns(2);
    CK_CHECK((table.horizontal_size_hint() == ckv::ui::SizeHint{28, 28, 28}));
    table.set_context(f.ctx());
    table.set_bounds(Rect{0, 0, 28, 2});
    Surface u(ckv::Size{28, 2}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter table_painter(u, Rect{0, 0, 28, 2});
    table.draw(table_painter);
    std::string first, second;
    for (int x = 0; x < 28; ++x) first += u.at(Point{x, 0}).grapheme();
    for (int x = 0; x < 28; ++x) second += u.at(Point{x, 1}).grapheme();
    CK_CHECK(first == "( ) Keep    ( ) Reset/Normal");
    CK_CHECK(second == "( ) Double  ( ) Single acct ");

    // Five choices in two columns take three rows, the last one half full;
    // a caption adds its row and indents the choices under it.
    RadioGroup five({"A", "B", "C", "D", "E"});
    five.set_columns(2);
    five.set_group_label("Letters");
    CK_CHECK((five.vertical_size_hint() == ckv::ui::SizeHint{4, 4, 4}));
    CK_CHECK((five.horizontal_size_hint() == ckv::ui::SizeHint{13, 13, 13}));
    five.set_context(f.ctx());
    five.set_bounds(Rect{0, 0, 13, 4});
    Surface t(ckv::Size{13, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter five_painter(t, Rect{0, 0, 13, 4});
    five.draw(five_painter);
    CK_CHECK(t.at(Point{5, 1}).grapheme() == "A");
    CK_CHECK(t.at(Point{12, 1}).grapheme() == "B");
    CK_CHECK(t.at(Point{5, 2}).grapheme() == "C");
    CK_CHECK(t.at(Point{12, 2}).grapheme() == "D");
    CK_CHECK(t.at(Point{5, 3}).grapheme() == "E");
    CK_CHECK(t.at(Point{12, 3}).grapheme() == " ");

    // More columns than choices is one row; a count below one is one column.
    RadioGroup two({"Yes", "No"});
    two.set_columns(9);
    CK_CHECK((two.vertical_size_hint() == ckv::ui::SizeHint{1, 1, 1}));
    two.set_columns(0);
    CK_CHECK(two.columns() == 1);
    CK_CHECK((two.vertical_size_hint() == ckv::ui::SizeHint{2, 2, 2}));
}

CK_TEST(left_and_right_step_through_a_row_and_up_and_down_are_not_its_own) {
    Fixture f;
    RadioGroup group({"Keep", "On", "Off", "Reset"});
    group.set_columns(4);
    group.set_selected(0);
    CK_CHECK(group.on_key(key(Key::Right)));
    CK_CHECK(group.selected() == 1);
    CK_CHECK(group.on_key(key(Key::Left)));
    CK_CHECK(group.selected() == 0);
    CK_CHECK(group.on_key(key(Key::Left)));  // wraps to the end of the row
    CK_CHECK(group.selected() == 3);
    // Every column holds one choice, so Up and Down go on to the window.
    CK_CHECK(!group.on_key(key(Key::Up)));
    CK_CHECK(!group.on_key(key(Key::Down)));
    CK_CHECK(group.selected() == 3);
}

CK_TEST(up_and_down_move_within_a_column_and_wrap_there) {
    Fixture f;
    // Two columns: A C E down the first, B D down the second.
    RadioGroup group({"A", "B", "C", "D", "E"});
    group.set_columns(2);
    group.set_selected(1);
    CK_CHECK(group.on_key(key(Key::Down)));
    CK_CHECK(group.selected() == 3);
    CK_CHECK(group.on_key(key(Key::Down)));  // the second column ends at D
    CK_CHECK(group.selected() == 1);
    CK_CHECK(group.on_key(key(Key::Up)));
    CK_CHECK(group.selected() == 3);
    group.set_selected(4);
    CK_CHECK(group.on_key(key(Key::Down)));  // E is last in the first column
    CK_CHECK(group.selected() == 0);
    CK_CHECK(group.on_key(key(Key::Right)));  // and Right still walks the sequence
    CK_CHECK(group.selected() == 1);

    // A stacked group keeps its old contract: Up from the top wraps.
    CheckGroup stacked({"One", "Two", "Three"});
    CK_CHECK(stacked.on_key(key(Key::Up)));
    CK_CHECK(stacked.on_key(key(Key::Char, " ")));
    CK_CHECK(stacked.checked(2));
}

CK_TEST(a_lone_choice_leaves_every_arrow_to_the_window) {
    Fixture f;
    CheckGroup box({"Ask for a filename every time"});
    CK_CHECK(!box.on_key(key(Key::Up)));
    CK_CHECK(!box.on_key(key(Key::Down)));
    CK_CHECK(!box.on_key(key(Key::Left)));
    CK_CHECK(!box.on_key(key(Key::Right)));
    CK_CHECK(box.on_key(key(Key::Char, " ")));
    CK_CHECK(box.checked(0));
}

CK_TEST(a_press_lands_on_the_choice_whose_column_it_is_in) {
    Fixture f;
    CheckGroup group({"Keep", "On", "Off", "Reset"});
    group.set_columns(4);
    group.set_context(f.ctx());
    group.set_bounds(Rect{0, 0, 42, 1});
    const auto press = [](int x, int y) {
        return ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{x, y}, std::nullopt,
                               Modifier::None};
    };
    CK_CHECK(group.on_mouse(press(11, 0)));  // the second choice's marker, at 10
    CK_CHECK(group.checked(1));
    CK_CHECK(group.on_mouse(press(9, 0)));  // the gap after the first choice is still its own
    CK_CHECK(group.checked(0));
    CK_CHECK(group.on_mouse(press(41, 0)));  // the last column runs to the edge
    CK_CHECK(group.checked(3));
    CK_CHECK(!group.on_mouse(press(3, 1)));  // no second row

    // The half-full last row of a two-column group has nothing in its
    // second column.
    RadioGroup five({"A", "B", "C", "D", "E"});
    five.set_columns(2);
    five.set_context(f.ctx());
    five.set_bounds(Rect{0, 0, 12, 3});
    CK_CHECK(five.on_mouse(press(8, 1)));
    CK_CHECK(five.selected() == 3);
    CK_CHECK(!five.on_mouse(press(8, 2)));
    CK_CHECK(five.selected() == 3);
}

CK_TEST(option_presentations_share_row_geometry_and_leave_gaps_inert) {
    using ckv::widgets::OptionPresentation;
    CheckGroup checks({"A", "B", "C"});
    RadioGroup radios({"A", "B", "C"});
    checks.set_columns(2);
    radios.set_columns(2);
    checks.set_bounds(Rect{0, 0, 30, 8});
    radios.set_bounds(Rect{0, 0, 30, 8});
    for (auto style : {OptionPresentation::BoxedRows, OptionPresentation::Buttons}) {
        checks.set_presentation(style);
        radios.set_presentation(style);
        const int row_height = style == OptionPresentation::BoxedRows ? 3 : 1;
        const int width = style == OptionPresentation::BoxedRows ? 7 : 6;
        CK_CHECK(checks.vertical_size_hint().preferred == 2 * row_height);
        CK_CHECK(radios.vertical_size_hint().preferred == 2 * row_height);
        CK_CHECK(checks.pointer_shape_at(Point{0, 0}) == ckv::PointerShape::Pointer);
        CK_CHECK(!checks.pointer_shape_at(Point{width, 0}));
        CK_CHECK(!radios.pointer_shape_at(Point{width + 1, 0}));
        checks.set_checked(2, false);
        CK_CHECK(checks.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{1, row_height}, std::nullopt}));
        CK_CHECK(checks.checked(2));
        radios.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{1, row_height}, std::nullopt});
        CK_CHECK(radios.selected() == 2);
        checks.set_enabled(false);
        CK_CHECK(!checks.on_key(key(Key::Char, " ")));
        CK_CHECK(checks.pointer_shape_at(Point{0, 0}) == ckv::PointerShape::NotAllowed);
        checks.set_enabled(true);
        auto release = key(Key::Char, " ");
        release.action = ckv::KeyAction::Release;
        CK_CHECK(!checks.on_key(release));
        CK_CHECK(checks.checked(2));
    }
}

CK_TEST(option_focus_and_mnemonics_remain_legible_on_monochrome_surfaces) {
    Fixture f;
    f.theme = ckv::ui::make_mono_theme(f.registry, f.roles);
    CheckGroup group({"&First", "&Second"});
    group.set_context(f.ctx());
    group.set_group_label("&Options");
    group.set_checked(0, true);
    group.set_bounds(Rect{0, 0, 20, 7});
    f.app.set_focus(&group);
    for (auto style : {ckv::widgets::OptionPresentation::Classic, ckv::widgets::OptionPresentation::BoxedRows, ckv::widgets::OptionPresentation::Buttons}) {
        group.set_presentation(style);
        Surface surface(ckv::Size{20, 7});
        Painter painter(surface, Rect{0, 0, 20, 7});
        group.draw(painter);
        for (int y = 0; y < 7; ++y) for (int x = 0; x < 20; ++x) {
            const auto& cell = surface.at(Point{x, y});
            const auto glyph = cell.grapheme();
            if (glyph.size() == 1 && ((glyph[0] >= 'A' && glyph[0] <= 'Z') || (glyph[0] >= 'a' && glyph[0] <= 'z')))
                CK_CHECK(cell.style().fg != cell.style().bg);
        }
    }
    group.set_presentation(ckv::widgets::OptionPresentation::Buttons);
    group.on_key(key(Key::Right));
    Surface surface(ckv::Size{20, 7});
    Painter painter(surface, Rect{0, 0, 20, 7});
    group.draw(painter);
    CK_CHECK(group.checked(0));
    CK_CHECK(!group.checked(1));
    CK_CHECK(ckv::has_attr(surface.at(Point{6, 1}).style().attrs, ckv::Attr::Bold));
    CK_CHECK(ckv::has_attr(surface.at(Point{6, 2}).style().attrs, ckv::Attr::Underline));
}
