// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
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

CK_TEST(the_cursor_row_shows_the_full_highlight_only_while_the_list_holds_focus) {
    // Two lists on screen have to be distinguishable: a reader must be able
    // to see which one their arrow keys will move. An unfocused list still
    // marks its place, but in the muted form.
    Fixture f;
    auto list = make_list(f);
    list.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    list.on_attached();
    list.set_items({"one", "two"});
    list.set_bounds(Rect{0, 0, 8, 2});
    ckv::scene::Surface surface(ckv::Size{8, 2}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 8, 2});

    // Unfocused to begin with — nothing has given it the keyboard.
    list.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 0}).style() == f.theme.resolve(f.roles.list_selected_inactive));
    CK_CHECK(surface.at(ckv::Point{0, 1}).style() == f.theme.resolve(f.roles.list_normal));

    list.on_focus(ckv::FocusEvent{true});
    list.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 0}).style() == f.theme.resolve(f.roles.list_selected));
    CK_CHECK(surface.at(ckv::Point{0, 1}).style() == f.theme.resolve(f.roles.list_normal));

    list.on_focus(ckv::FocusEvent{false});
    list.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 0}).style() == f.theme.resolve(f.roles.list_selected_inactive));
}

CK_TEST(a_selection_is_a_highlight_bar_rather_than_an_underline) {
    // The selected row must differ from a normal one by its background, the
    // way every other "this is the one" surface in the schemes does. An
    // underline alone reads as a text field or a hyperlink.
    Fixture f;
    auto list = make_list(f);
    list.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    list.on_attached();
    list.on_focus(ckv::FocusEvent{true});
    list.set_items({"one", "two"});
    list.set_bounds(Rect{0, 0, 8, 2});
    ckv::scene::Surface surface(ckv::Size{8, 2}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 8, 2});
    list.draw(painter);

    const ckv::Style selected = surface.at(ckv::Point{0, 0}).style();
    const ckv::Style normal = surface.at(ckv::Point{0, 1}).style();
    CK_CHECK(selected.bg != normal.bg);
    CK_CHECK(!has_attr(selected.attrs, ckv::Attr::Underline));
    // The bar runs the row's full width, including past the end of the text.
    CK_CHECK(surface.at(ckv::Point{6, 0}).style().bg == selected.bg);
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

CK_TEST(setting_the_cursor_moves_both_the_cursor_and_the_selection) {
    // set_selected alone marks a row chosen but leaves the cursor behind, so
    // the list paints two highlighted rows and the next arrow key moves from
    // the wrong place. Restoring a list to a known row has to move both.
    Fixture f;
    auto list = make_list(f);
    list.set_items({"one", "two", "three"});
    CK_CHECK(list.cursor() == 0);

    list.set_cursor(2);
    CK_CHECK(list.cursor() == 2);
    const std::vector<std::size_t> selected = list.selected_indices();
    CK_CHECK(selected.size() == 1U);
    CK_CHECK(selected[0] == 2U);

    // Arrow keys now continue from the cursor's new home.
    list.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Up, ckv::Modifier::None, ""}});
    CK_CHECK(list.cursor() == 1);

    // Out of range changes nothing.
    list.set_cursor(99);
    CK_CHECK(list.cursor() == 1);
}

CK_TEST(a_row_is_painted_across_the_whole_list_width) {
    // Regression: draw() filled bounds().width - 1, keeping the last column
    // clear "for the scrollbar". The scrollbar is a child and paints itself
    // over that column, so the reservation bought nothing — and under an Auto
    // policy with nothing to scroll it is not drawn at all, leaving whatever
    // is behind the list showing through a one-column notch down its right
    // edge. Stacked under a search box in a dialog, that notch is exactly
    // where the two widgets stop lining up.
    Fixture f;
    HeadlessTerminal term(ckv::Size{40, 10});
    ManualClock clock;
    Application app(term, clock);
    app.theme() = make_classic_theme(app.roles(), intern_standard_roles(app.roles()));
    auto* const list = app.root().add(std::make_unique<ListView>(/*multi_select=*/false));
    list->set_scrollbar_policy(ckv::widgets::ScrollbarPolicy::Auto);
    list->set_items({"one", "two"});
    list->set_bounds(Rect{0, 0, 12, 4});
    app.step(0);

    // Two items in four rows: nothing to scroll, so no bar, so every one of
    // the twelve columns is the list's own.
    const ckv::FrameView frame = app.current_frame();
    const ckv::Style row_style = frame.at(ckv::Point{0, 0}).style();
    for (int x = 0; x < 12; ++x) CK_CHECK(frame.at(ckv::Point{x, 0}).style().bg == row_style.bg);
}

CK_TEST(a_scrolling_list_still_shows_its_bar_in_the_last_column) {
    // The other half of the above: painting the full width must not bury the
    // scrollbar when there IS something to scroll. The bar paints after the
    // row fill, so it still wins its column.
    Fixture f;
    HeadlessTerminal term(ckv::Size{40, 10});
    ManualClock clock;
    Application app(term, clock);
    app.theme() = make_classic_theme(app.roles(), intern_standard_roles(app.roles()));
    auto* const list = app.root().add(std::make_unique<ListView>(/*multi_select=*/false));
    list->set_scrollbar_policy(ckv::widgets::ScrollbarPolicy::Auto);
    list->set_items({"one", "two", "three", "four", "five", "six"});
    list->set_bounds(Rect{0, 0, 12, 3});
    app.step(0);

    const ckv::FrameView frame = app.current_frame();
    CK_CHECK(frame.at(ckv::Point{11, 0}).style().bg != frame.at(ckv::Point{0, 0}).style().bg);
}

// --- Browsing versus choosing -----------------------------------------

CK_TEST(the_cursor_reports_every_move_including_the_one_a_caller_makes) {
    Fixture f;
    ListView list;
    list.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    list.set_bounds(Rect{0, 0, 20, 5});
    list.set_items({"one", "two", "three"});
    std::vector<std::size_t> moved;
    list.on_cursor_changed = [&](std::size_t at) { moved.push_back(at); };

    list.on_key(key(ckv::Key::Down));
    list.set_cursor(2);
    CK_CHECK(moved.size() == 2);
    CK_CHECK(moved[0] == 1);
    CK_CHECK(moved[1] == 2);
}

CK_TEST(a_cursor_that_did_not_move_reports_nothing) {
    // A listener may do real work — render a page, run a query — so it is
    // told about moves, not about keystrokes.
    Fixture f;
    ListView list;
    list.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    list.set_bounds(Rect{0, 0, 20, 5});
    list.set_items({"one", "two"});
    int moves = 0;
    list.on_cursor_changed = [&](std::size_t) { ++moves; };
    list.on_key(key(ckv::Key::Up));  // at the top
    CK_CHECK(moves == 0);
    list.set_cursor(0);  // already there
    CK_CHECK(moves == 0);
}

CK_TEST(browsing_and_choosing_are_reported_separately) {
    Fixture f;
    ListView list;
    list.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    list.set_bounds(Rect{0, 0, 20, 5});
    list.set_items({"one", "two", "three"});
    int moves = 0;
    int activations = 0;
    list.on_cursor_changed = [&](std::size_t) { ++moves; };
    list.on_activate = [&](std::size_t) { ++activations; };

    list.on_key(key(ckv::Key::Down));
    CK_CHECK(moves == 1);
    CK_CHECK(activations == 0);  // moving is not choosing

    list.on_key(key(ckv::Key::Enter));
    CK_CHECK(activations == 1);
    CK_CHECK(moves == 1);  // and choosing is not moving
}

CK_TEST(a_list_says_how_much_room_its_contents_want) {
    // A container asks a view how big it would like to be, and a view that
    // answers "nothing" gets nothing. That is not academic: the stock
    // window-list dialog shipped five rows tall with no entry visible, because
    // the list inside it reported no hints at all.
    ckv::widgets::ListView list;
    list.set_items({"one", "two", "three"});
    CK_CHECK(list.vertical_size_hint().preferred == 3);
    // Never zero, even empty: a box with no height tells a reader nothing about
    // whether it is empty or merely squeezed.
    ckv::widgets::ListView empty;
    CK_CHECK(empty.vertical_size_hint().preferred >= 1);
    CK_CHECK(empty.vertical_size_hint().min >= 1);

    // And bounded: ten thousand items is not a request for a window ten
    // thousand rows tall.
    std::vector<std::string> many;
    for (int index = 0; index < 10'000; ++index) many.push_back("item " + std::to_string(index));
    ckv::widgets::ListView long_list;
    long_list.set_items(many);
    CK_CHECK(long_list.vertical_size_hint().preferred ==
             static_cast<int>(ckv::widgets::ListView::kPreferredVisibleRows));

    // The width follows the longest item worth measuring, so a list of long
    // names opens wide enough to read them.
    ckv::widgets::ListView wide;
    wide.set_items({"a", "a much longer entry than the others"});
    CK_CHECK(wide.horizontal_size_hint().preferred >= 35);
    CK_CHECK(wide.horizontal_size_hint().preferred > list.horizontal_size_hint().preferred);
}
