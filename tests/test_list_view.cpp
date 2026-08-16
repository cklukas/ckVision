// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/list_view.hpp"

#include <algorithm>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::term::HeadlessTerminal;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::ListView;
using ckv::widgets::ListItem;
using ckv::widgets::ListItemId;
using ckv::widgets::ListModel;

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

struct Provider final : ListModel {
    std::vector<ListItemId> ids;
    mutable std::size_t item_queries = 0;

    std::size_t item_count() const override { return ids.size(); }
    ListItem item_at(std::size_t index) const override {
        ++item_queries;
        return ListItem{ids[index], "item " + std::to_string(ids[index]), std::nullopt};
    }
    std::optional<std::size_t> index_of(ListItemId id) const override {
        const auto found = std::find(ids.begin(), ids.end(), id);
        return found == ids.end() ? std::nullopt : std::optional<std::size_t>(found - ids.begin());
    }
};
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

CK_TEST(the_initial_cursor_row_uses_the_selected_paint_role) {
    Fixture f;
    auto list = make_list(f);
    list.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    list.set_items({"one", "two"});
    list.set_bounds(Rect{0, 0, 8, 2});
    ckv::scene::Surface surface(ckv::Size{8, 2}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 8, 2});

    list.draw(painter);

    CK_CHECK(surface.at(ckv::Point{0, 0}).style() == f.theme.resolve(f.roles.list_selected));
    CK_CHECK(surface.at(ckv::Point{0, 1}).style() == f.theme.resolve(f.roles.list_normal));
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

CK_TEST(on_selection_changed_reports_the_index_that_was_selected) {
    // M10/WP-22: the index argument, mirroring TreeView's own node
    // argument on the same hook.
    Fixture f;
    auto list = make_list(f, false);
    list.set_items({"a", "b", "c"});
    std::size_t reported = 999;
    list.on_selection_changed = [&](std::size_t i) { reported = i; };
    list.set_selected(2, true);
    CK_CHECK(reported == 2);
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

CK_TEST(multi_select_on_selection_changed_reports_the_toggled_index) {
    Fixture f;
    auto list = make_list(f, true);
    list.set_items({"a", "b", "c"});
    std::size_t reported = 999;
    list.on_selection_changed = [&](std::size_t i) { reported = i; };
    list.set_selected(1, true);
    CK_CHECK(reported == 1);
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

CK_TEST(a_single_click_on_the_current_row_does_not_activate_without_a_timed_second_click) {
    Fixture f;
    HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto list = make_list(f);
    list.set_context(ckv::ui::Context{&f.theme, &f.registry, &app});
    list.set_items({"a", "b", "c"});
    list.set_bounds(Rect{0, 0, 10, 5});
    bool activated = false;
    list.on_activate = [&](std::size_t) { activated = true; };
    list.on_mouse(click(ckv::Point{0, 0}));
    CK_CHECK(!activated);
}

CK_TEST(a_second_click_on_the_same_row_within_the_clock_threshold_activates_it) {
    Fixture f;
    HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto list = make_list(f);
    list.set_context(ckv::ui::Context{&f.theme, &f.registry, &app});
    list.set_items({"a", "b", "c"});
    list.set_bounds(Rect{0, 0, 10, 5});
    std::size_t activated = 999;
    list.on_activate = [&](std::size_t index) { activated = index; };

    list.on_mouse(click(ckv::Point{0, 1}));
    clock.advance(100'000'000);
    list.on_mouse(click(ckv::Point{0, 1}));

    CK_CHECK(activated == 1);
}

CK_TEST(a_second_click_after_the_clock_threshold_only_moves_the_cursor) {
    Fixture f;
    HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto list = make_list(f);
    list.set_context(ckv::ui::Context{&f.theme, &f.registry, &app});
    list.set_items({"a", "b", "c"});
    list.set_bounds(Rect{0, 0, 10, 5});
    bool activated = false;
    list.on_activate = [&](std::size_t) { activated = true; };

    list.on_mouse(click(ckv::Point{0, 1}));
    clock.advance(600'000'000);
    list.on_mouse(click(ckv::Point{0, 1}));

    CK_CHECK(!activated);
    CK_CHECK(list.cursor() == 1);
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

CK_TEST(provider_backed_list_queries_only_the_visible_slice) {
    Fixture f;
    Provider model;
    model.ids.resize(1'000'000);
    for (std::size_t index = 0; index < model.ids.size(); ++index) model.ids[index] = static_cast<ListItemId>(index + 1);
    auto list = make_list(f);
    list.set_model(model);
    list.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    list.set_bounds(Rect{0, 0, 20, 4});
    ckv::scene::Surface surface(ckv::Size{20, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 20, 4});
    list.draw(painter);
    CK_CHECK(model.item_queries <= 5);  // initial identity plus four visible rows
}

CK_TEST(provider_backed_list_preserves_cursor_and_selection_identity_across_reorder) {
    Fixture f;
    Provider model;
    model.ids = {10, 20, 30};
    auto list = make_list(f);
    list.set_model(model);
    list.on_key(key(Key::Down));  // cursor and single selection now identify item 20
    CK_CHECK(list.cursor_id() == 20);
    CK_CHECK(list.is_selected_id(20));
    model.ids = {30, 10, 20};
    list.model_changed();
    CK_CHECK(list.cursor_id() == 20);
    CK_CHECK(list.cursor() == 2);
    CK_CHECK(list.is_selected_id(20));
}
