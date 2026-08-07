// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/list_view.hpp"

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
using ckv::widgets::ListView;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

ListView make_list(Fixture&, bool multi = false) { return ListView(multi); }

ckv::KeyEvent key(ckv::Key k, std::string text = "") {
    return ckv::KeyEvent{KeyChord{k, Modifier::None, std::move(text)}};
}

ckv::MouseEvent click(ckv::Point p) {
    return ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, p, std::nullopt, Modifier::None};
}
}  // namespace

// --- Basics --------------------------------------------------------------

CK_TEST(empty_list_has_no_cursor) {
    Fixture f;
    auto list = make_list(f);
    CK_CHECK(list.cursor() == -1);
}

CK_TEST(setting_items_places_the_cursor_on_the_first_item) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"one", "two", "three"});
    CK_CHECK(list.cursor() == 0);
}

CK_TEST(re_setting_items_resets_selection_and_cursor) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"a", "b"});
    list.set_selected(1, true);
    list.set_items({"x", "y", "z"});
    CK_CHECK(list.cursor() == 0);
    CK_CHECK(!list.is_selected(0));
    CK_CHECK(!list.is_selected(1));
}

// --- Single select --------------------------------------------------

CK_TEST(single_select_moving_the_cursor_also_moves_the_selection) {
    Fixture f;
    auto list = make_list(f, false);
    list.set_items({"a", "b", "c"});
    list.on_key(key(Key::Down));
    CK_CHECK(list.cursor() == 1);
    CK_CHECK(list.is_selected(1));
    CK_CHECK(!list.is_selected(0));
}

CK_TEST(single_select_selecting_a_different_item_deselects_the_previous_one) {
    Fixture f;
    auto list = make_list(f, false);
    list.set_items({"a", "b", "c"});
    list.set_selected(0, true);
    list.set_selected(2, true);
    CK_CHECK(!list.is_selected(0));
    CK_CHECK(list.is_selected(2));
}

// --- Multi select ---------------------------------------------------

CK_TEST(multi_select_moving_the_cursor_does_not_change_selection) {
    Fixture f;
    auto list = make_list(f, true);
    list.set_items({"a", "b", "c"});
    list.set_selected(0, true);
    list.on_key(key(Key::Down));
    CK_CHECK(list.cursor() == 1);
    CK_CHECK(list.is_selected(0));  // untouched by cursor movement
    CK_CHECK(!list.is_selected(1));
}

CK_TEST(multi_select_space_toggles_independently) {
    Fixture f;
    auto list = make_list(f, true);
    list.set_items({"a", "b", "c"});
    list.on_key(key(Key::Char, " "));  // toggles item 0
    list.on_key(key(Key::Down));
    list.on_key(key(Key::Char, " "));  // toggles item 1
    CK_CHECK(list.is_selected(0));
    CK_CHECK(list.is_selected(1));
    CK_CHECK(list.selected_indices().size() == 2);
}

// --- Navigation -------------------------------------------------------

CK_TEST(page_down_moves_by_the_visible_height) {
    Fixture f;
    auto list = make_list(f);
    std::vector<std::string> items;
    for (int i = 0; i < 20; ++i) items.push_back(std::to_string(i));
    list.set_items(items);
    list.set_bounds(Rect{0, 0, 10, 5});
    list.on_key(key(Key::PageDown));
    CK_CHECK(list.cursor() == 5);
}

CK_TEST(home_and_end_jump_to_the_first_and_last_items) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"a", "b", "c", "d"});
    list.on_key(key(Key::End));
    CK_CHECK(list.cursor() == 3);
    list.on_key(key(Key::Home));
    CK_CHECK(list.cursor() == 0);
}

CK_TEST(navigation_clamps_at_the_boundaries_rather_than_wrapping) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"a", "b"});
    list.on_key(key(Key::Up));  // already at 0
    CK_CHECK(list.cursor() == 0);
    list.on_key(key(Key::Down));
    list.on_key(key(Key::Down));  // one past the end
    CK_CHECK(list.cursor() == 1);
}

// --- Keyboard search --------------------------------------------------

CK_TEST(typing_a_letter_jumps_to_the_next_item_starting_with_it) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"apple", "banana", "cherry"});
    CK_CHECK(list.on_key(key(Key::Char, "c")));
    CK_CHECK(list.cursor() == 2);
}

CK_TEST(keyboard_search_wraps_and_is_case_insensitive) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"Apple", "banana", "Cherry"});
    list.on_key(key(Key::End));  // cursor at 2 (Cherry)
    CK_CHECK(list.on_key(key(Key::Char, "a")));  // wraps around to "Apple"
    CK_CHECK(list.cursor() == 0);
}

CK_TEST(keyboard_search_with_no_match_is_unhandled) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"apple", "banana"});
    CK_CHECK(!list.on_key(key(Key::Char, "z")));
}

// --- Activation --------------------------------------------------------

CK_TEST(enter_activates_the_cursor_item) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"a", "b"});
    std::size_t activated = 999;
    list.on_activate = [&](std::size_t i) { activated = i; };
    list.on_key(key(Key::Enter));
    CK_CHECK(activated == 0);
}

CK_TEST(clicking_the_already_current_row_activates_it) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"a", "b", "c"});
    list.set_bounds(Rect{0, 0, 10, 5});
    bool activated = false;
    list.on_activate = [&](std::size_t) { activated = true; };
    list.on_mouse(click(ckv::Point{0, 0}));  // moves cursor to row 0 (already there)
    CK_CHECK(activated);
}

CK_TEST(clicking_a_different_row_moves_the_cursor_without_activating) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"a", "b", "c"});
    list.set_bounds(Rect{0, 0, 10, 5});
    bool activated = false;
    list.on_activate = [&](std::size_t) { activated = true; };
    list.on_mouse(click(ckv::Point{0, 2}));
    CK_CHECK(list.cursor() == 2);
    CK_CHECK(!activated);
}

CK_TEST(clicking_below_the_last_item_is_unhandled) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"a", "b"});
    list.set_bounds(Rect{0, 0, 10, 10});
    CK_CHECK(!list.on_mouse(click(ckv::Point{0, 5})));
}

// --- Scrolling ----------------------------------------------------------

CK_TEST(moving_the_cursor_past_the_visible_area_scrolls_to_keep_it_visible) {
    Fixture f;
    auto list = make_list(f);
    std::vector<std::string> items;
    for (int i = 0; i < 20; ++i) items.push_back(std::to_string(i));
    list.set_items(items);
    list.set_bounds(Rect{0, 0, 10, 5});
    for (int i = 0; i < 10; ++i) list.on_key(key(Key::Down));
    CK_CHECK(list.cursor() == 10);
    // Clicking at the visible row 0 must now hit item (scroll_top + 0),
    // not item 0 — i.e. the list actually scrolled.
    bool activated_index_zero = false;
    list.on_activate = [&](std::size_t i) { activated_index_zero = (i == 0); };
    list.on_mouse(click(ckv::Point{0, 0}));
    CK_CHECK(!activated_index_zero);
}

// --- Degenerate cases ----------------------------------------------------

CK_TEST(empty_list_does_not_crash_on_key_or_mouse) {
    Fixture f;
    auto list = make_list(f);
    list.set_bounds(Rect{0, 0, 10, 5});
    list.on_key(key(Key::Down));
    list.on_key(key(Key::Enter));
    CK_CHECK(!list.on_mouse(click(ckv::Point{0, 0})));
}

CK_TEST(is_selected_out_of_range_returns_false_rather_than_crashing) {
    Fixture f;
    auto list = make_list(f);
    list.set_items({"a"});
    CK_CHECK(!list.is_selected(50));
}
