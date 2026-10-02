// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/table.hpp"

#include <algorithm>

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Table;
using ckv::widgets::TableCell;
using ckv::widgets::TableCellRef;
using ckv::widgets::TableColumn;
using ckv::widgets::TableEditResult;
using ckv::widgets::TableModel;
using ckv::widgets::TableRowId;
using ckv::widgets::format_cell_value;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

Table make_table(Fixture&) { return Table(); }

ckv::KeyEvent key(ckv::Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }

ckv::MouseEvent click(ckv::Point p) {
    return ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, p, std::nullopt, Modifier::None};
}

struct Provider final : TableModel {
    std::vector<TableRowId> order{10, 20, 30};
    std::vector<std::int64_t> values{1, 2, 3};
    mutable std::size_t cell_queries = 0;
    std::optional<TableCellRef> committed;
    std::optional<ckv::widgets::CellValue> committed_value;

    std::size_t row_count() const override { return order.size(); }
    TableRowId row_id_at(std::size_t index) const override { return order[index]; }
    std::optional<std::size_t> index_of(TableRowId id) const override {
        const auto found = std::find(order.begin(), order.end(), id);
        return found == order.end() ? std::nullopt : std::optional<std::size_t>(found - order.begin());
    }
    TableCell cell(TableCellRef reference) const override {
        ++cell_queries;
        const auto index = index_of(reference.row);
        return TableCell{values[*index], {}, std::nullopt, true};
    }
    TableEditResult commit(TableCellRef reference, const ckv::widgets::CellValue& value) override {
        committed = reference;
        committed_value = value;
        const auto number = std::get_if<std::int64_t>(&value);
        if (number == nullptr) return TableEditResult::reject("Expected integer");
        values[*index_of(reference.row)] = *number;
        return TableEditResult::accept();
    }
};
}  // namespace

// --- Basics --------------------------------------------------------------

CK_TEST(an_empty_table_has_no_cursor) {
    Fixture f;
    auto table = make_table(f);
    CK_CHECK(table.cursor_row() == -1);
}

CK_TEST(setting_rows_places_the_cursor_on_the_first_row) {
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Name", 10, 3}, TableColumn{"Age", 5, 3}});
    table.set_rows({{"Bob", "30"}, {"Amy", "25"}});
    CK_CHECK(table.cursor_row() == 0);
    CK_CHECK(table.row(0)[0] == "Bob");
}

CK_TEST(a_row_with_the_wrong_cell_count_aborts) {
    CK_EXPECT_ABORT({
        Fixture f;
        auto table = make_table(f);
        table.set_columns({TableColumn{"A", 5, 3}, TableColumn{"B", 5, 3}});
        table.set_rows({{"only one cell"}});
    });
}

// --- Sorting -----------------------------------------------------------

CK_TEST(sort_by_reorders_display_rows_without_touching_underlying_storage) {
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Name", 10, 3}});
    table.set_rows({{"Charlie"}, {"Alice"}, {"Bob"}});
    table.sort_by(0, true);
    CK_CHECK(table.row(0)[0] == "Alice");
    CK_CHECK(table.row(1)[0] == "Bob");
    CK_CHECK(table.row(2)[0] == "Charlie");
}

CK_TEST(sort_descending_reverses_the_order) {
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Name", 10, 3}});
    table.set_rows({{"Charlie"}, {"Alice"}, {"Bob"}});
    table.sort_by(0, false);
    CK_CHECK(table.row(0)[0] == "Charlie");
    CK_CHECK(table.row(2)[0] == "Alice");
}

CK_TEST(sort_column_of_negative_one_restores_insertion_order) {
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Name", 10, 3}});
    table.set_rows({{"Charlie"}, {"Alice"}, {"Bob"}});
    table.sort_by(0, true);
    table.sort_by(-1, true);
    CK_CHECK(table.row(0)[0] == "Charlie");
    CK_CHECK(table.row(1)[0] == "Alice");
    CK_CHECK(table.row(2)[0] == "Bob");
}

CK_TEST(sort_out_of_range_column_aborts) {
    CK_EXPECT_ABORT({
        Fixture f;
        auto table = make_table(f);
        table.set_columns({TableColumn{"A", 5, 3}});
        table.set_rows({{"x"}});
        table.sort_by(5, true);
    });
}

CK_TEST(clicking_a_header_sorts_ascending_and_clicking_again_reverses_it) {
    Fixture f;
    auto table = make_table(f);
    table.set_bounds(Rect{0, 0, 30, 10});
    table.set_columns({TableColumn{"Name", 10, 3}});
    table.set_rows({{"Charlie"}, {"Alice"}, {"Bob"}});
    table.on_mouse(click(ckv::Point{2, 0}));
    CK_CHECK(table.sort_column() == 0);
    CK_CHECK(table.sort_ascending());
    CK_CHECK(table.row(0)[0] == "Alice");
    table.on_mouse(click(ckv::Point{2, 0}));
    CK_CHECK(!table.sort_ascending());
    CK_CHECK(table.row(0)[0] == "Charlie");
}

CK_TEST(sorting_preserves_selection_identity_not_just_the_display_index) {
    // cursor_row() is a DISPLAY index, but sort_by() must re-target it
    // to wherever the SAME underlying row ends up — selection identity
    // survives a sort, it does not silently follow display position 0.
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Name", 10, 3}});
    table.set_rows({{"Charlie"}, {"Alice"}, {"Bob"}});
    CK_CHECK(table.row(table.cursor_row())[0] == "Charlie");  // cursor 0, insertion order
    table.sort_by(0, true);
    // "Charlie" moved to display index 2 (last, alphabetically) — the
    // cursor must have followed it there, not stayed at display index 0.
    CK_CHECK(table.row(table.cursor_row())[0] == "Charlie");
    CK_CHECK(table.cursor_row() == 2);
}

// --- Cell style hook -----------------------------------------------------

CK_TEST(cell_style_hook_receives_the_underlying_row_index_not_the_display_index) {
    Fixture f;
    auto table = make_table(f);
    table.set_context(f.ctx());
    table.set_columns({TableColumn{"Name", 10, 3}});
    table.set_rows({{"Charlie"}, {"Alice"}, {"Bob"}});
    table.sort_by(0, true);  // display order: Alice(1), Bob(2), Charlie(0)

    std::vector<std::size_t> seen_underlying_indices;
    table.set_cell_style_hook([&](std::size_t row, std::size_t, ckv::Style base) {
        seen_underlying_indices.push_back(row);
        return base;
    });
    table.set_bounds(Rect{0, 0, 30, 5});
    Surface s(ckv::Size{30, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 30, 5});
    table.draw(painter);

    CK_CHECK(seen_underlying_indices.size() == 3);
    CK_CHECK(seen_underlying_indices[0] == 1);  // Alice is underlying index 1, drawn first (sorted)
}

CK_TEST(data_cells_stop_short_of_the_scrollbar_column_without_splitting_a_wide_glyph) {
    // WP-30. The data rows keep their last column for the scrollbar, which
    // paints over it after them; a cell reaching into that column is cut at a
    // cluster boundary before it, never inside a wide glyph.
    Fixture f;
    auto table = make_table(f);
    table.set_context(f.ctx());
    table.on_attached();
    table.set_columns({TableColumn{"AB", 4, 3}});
    table.set_rows({{"AB\xE4\xB8\xAD"}});  // AB + U+4E2D: the ideograph needs columns 2 and 3
    table.set_bounds(Rect{0, 0, 4, 2});
    Surface s(ckv::Size{4, 2}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 4, 2});
    table.draw(painter);
    CK_CHECK(s.at(ckv::Point{1, 1}).grapheme() == "B");
    CK_CHECK(s.at(ckv::Point{2, 1}).grapheme() == " ");
    CK_CHECK(!s.at(ckv::Point{3, 1}).is_continuation());
}

// --- Keyboard navigation -----------------------------------------------

CK_TEST(down_arrow_moves_the_cursor_row) {
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"A", 5, 3}});
    table.set_rows({{"1"}, {"2"}, {"3"}});
    table.on_key(key(Key::Down));
    CK_CHECK(table.cursor_row() == 1);
}

CK_TEST(navigation_clamps_at_the_boundaries) {
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"A", 5, 3}});
    table.set_rows({{"1"}, {"2"}});
    table.on_key(key(Key::Up));  // already at 0
    CK_CHECK(table.cursor_row() == 0);
    table.on_key(key(Key::End));
    CK_CHECK(table.cursor_row() == 1);
    table.on_key(key(Key::Down));  // one past the end
    CK_CHECK(table.cursor_row() == 1);
}

CK_TEST(empty_table_key_navigation_is_unhandled) {
    Fixture f;
    auto table = make_table(f);
    CK_CHECK(!table.on_key(key(Key::Down)));
}

// --- Column resize ------------------------------------------------------

CK_TEST(dragging_a_column_boundary_resizes_that_column) {
    Fixture f;
    auto table = make_table(f);
    table.set_bounds(Rect{0, 0, 30, 5});
    table.set_columns({TableColumn{"A", 10, 3}, TableColumn{"B", 10, 3}});
    table.set_rows({{"x", "y"}});
    // Boundary for column 0 is at local x = 10 (its width).
    table.on_mouse(click(ckv::Point{10, 0}));
    table.on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{15, 0}, std::nullopt,
                                    Modifier::None});
    CK_CHECK(table.columns()[0].width == 15);
    table.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{15, 0}, std::nullopt,
                                    Modifier::None});
}

CK_TEST(resizing_below_the_columns_minimum_width_clamps) {
    Fixture f;
    auto table = make_table(f);
    table.set_bounds(Rect{0, 0, 30, 5});
    table.set_columns({TableColumn{"A", 10, 3}});
    table.set_rows({{"x"}});
    table.on_mouse(click(ckv::Point{10, 0}));
    table.on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{-100, 0}, std::nullopt,
                                    Modifier::None});
    CK_CHECK(table.columns()[0].width == 3);
}

CK_TEST(move_without_a_prior_resize_down_is_unhandled) {
    Fixture f;
    auto table = make_table(f);
    table.set_bounds(Rect{0, 0, 30, 5});
    table.set_columns({TableColumn{"A", 10, 3}});
    table.set_rows({{"x"}});
    CK_CHECK(!table.on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{5, 0},
                                              std::nullopt, Modifier::None}));
}

// --- Rendering does not crash --------------------------------------

CK_TEST(draw_does_not_crash_for_an_empty_table) {
    Fixture f;
    auto table = make_table(f);
    table.set_context(f.ctx());
    table.set_columns({TableColumn{"A", 5, 3}});
    table.set_bounds(Rect{0, 0, 20, 5});
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 5});
    table.draw(painter);
    CK_CHECK(true);
}

CK_TEST(provider_backed_table_queries_only_visible_cells) {
    Fixture f;
    Provider model;
    model.order.resize(1'000'000);
    model.values.resize(1'000'000);
    for (std::size_t index = 0; index < model.order.size(); ++index) model.order[index] = static_cast<TableRowId>(index + 1);
    auto table = make_table(f);
    table.set_columns({TableColumn{"Value", 8, 3, ckv::widgets::TableCellType::Integer, true}});
    table.set_model(model);
    table.set_context(f.ctx());
    table.set_bounds(Rect{0, 0, 20, 5});
    Surface surface(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 20, 5});
    table.draw(painter);
    CK_CHECK(model.cell_queries <= 5);  // four visible rows plus no hidden materialization
}

CK_TEST(provider_backed_table_preserves_the_selected_cell_through_reorder) {
    Fixture f;
    Provider model;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Value", 8, 3, ckv::widgets::TableCellType::Integer, true}});
    table.set_model(model);
    table.on_key(key(Key::Down));
    CK_CHECK((table.selected_cell() == TableCellRef{20, 0}));
    model.order = {30, 10, 20};
    table.model_changed();
    CK_CHECK((table.selected_cell() == TableCellRef{20, 0}));
    CK_CHECK(table.cursor_row() == 2);
}

CK_TEST(provider_backed_table_clears_a_removed_selection) {
    Fixture f;
    Provider model;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Value", 8, 3, ckv::widgets::TableCellType::Integer, true}});
    table.set_model(model);
    table.on_key(key(Key::Down));
    model.order = {10, 30};
    table.model_changed();
    CK_CHECK(!table.selected_cell());
    CK_CHECK(table.cursor_row() == -1);
}

CK_TEST(provider_backed_table_commits_a_typed_edit_to_its_model) {
    Fixture f;
    Provider model;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Count", 8, 3, ckv::widgets::TableCellType::Integer, true}});
    table.set_model(model);
    CK_CHECK(table.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "42"}}));
    CK_CHECK(table.editing());
    CK_CHECK(table.on_key(key(Key::Enter)));
    CK_CHECK(!table.editing());
    CK_CHECK((model.committed == TableCellRef{10, 0}));
    CK_CHECK(std::get<std::int64_t>(*model.committed_value) == 42);
}

CK_TEST(a_letter_with_a_command_modifier_is_a_chord_not_the_start_of_an_edit) {
    Fixture f;
    Provider model;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Count", 8, 3, ckv::widgets::TableCellType::Integer, true}});
    table.set_model(model);
    // Alt+X is an application's Exit and Ctrl+P its palette: the table
    // leaves both to whoever bound them, and starts no edit.
    CK_CHECK(!table.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}}));
    CK_CHECK(!table.editing());
    CK_CHECK(!table.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, "p"}}));
    CK_CHECK(!table.editing());
    // Shift is part of a typed letter, so a capital starts an edit.
    CK_CHECK(table.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Shift, "X"}}));
    CK_CHECK(table.editing());
    CK_CHECK(table.edit_text() == "X");
}

CK_TEST(real_cells_format_and_commit_without_locale_or_partial_parses) {
    CK_CHECK(format_cell_value(ckv::widgets::CellValue{1.25}) == "1.25");

    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Value", 12, 3, ckv::widgets::TableCellType::Real, true}});
    table.set_rows({{"0"}});

    CK_CHECK(table.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "1.25"}}));
    CK_CHECK(table.on_key(key(Key::Enter)));
    CK_CHECK(!table.editing());
    CK_CHECK(table.row(0)[0] == "1.25");

    CK_CHECK(table.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, " 2.5"}}));
    CK_CHECK(table.on_key(key(Key::Enter)));  // refused, and still the table's key
    CK_CHECK(table.editing());
    CK_CHECK(table.edit_diagnostic() == "Enter a real number");
}

// --- Focus and editing faces ---------------------------------------------

namespace {
// A table whose context names an Application, so a test can give it focus.
struct Hosted {
    Fixture f;
    ckv::term::HeadlessTerminal term{ckv::Size{30, 6}};
    ckv::ManualClock clock;
    ckv::ui::Application app{term, clock};
    Provider model;
    Table table;

    Hosted() {
        table.set_columns({TableColumn{"Count", 8, 3, ckv::widgets::TableCellType::Integer, true}});
        table.set_model(model);
        table.set_context(ckv::ui::Context{&f.theme, &f.registry, &app});
        table.set_bounds(Rect{2, 1, 20, 5});
    }
    Surface draw() {
        Surface surface(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
        Painter painter(surface, Rect{0, 0, 20, 5});
        table.draw(painter);
        return surface;
    }
};
}  // namespace

CK_TEST(the_cursor_cell_wears_the_full_highlight_only_while_the_table_holds_the_keyboard) {
    Hosted h;
    CK_CHECK(h.draw().at(ckv::Point{0, 1}).style() == h.f.theme.resolve(h.f.roles.list_selected_inactive));
    h.app.set_focus(&h.table);
    CK_CHECK(h.draw().at(ckv::Point{0, 1}).style() == h.f.theme.resolve(h.f.roles.list_selected));
    CK_CHECK(h.draw().at(ckv::Point{0, 2}).style() == h.f.theme.resolve(h.f.roles.list_normal));
}

CK_TEST(a_cell_being_edited_is_drawn_as_a_field_with_the_caret_after_its_text) {
    Hosted h;
    h.app.set_focus(&h.table);
    CK_CHECK(!h.table.cursor_state().has_value());  // not editing: no caret
    CK_CHECK(h.table.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "42"}}));
    CK_CHECK(h.table.editing());

    const Surface surface = h.draw();
    const ckv::Style field = h.f.theme.resolve(h.f.roles.input_focused);
    CK_CHECK(surface.at(ckv::Point{0, 1}).grapheme() == "4");
    CK_CHECK(surface.at(ckv::Point{0, 1}).style() == field);
    CK_CHECK(surface.at(ckv::Point{7, 1}).style() == field);  // the column's full width

    const auto caret = h.table.cursor_state();
    CK_CHECK(caret.has_value());
    if (!caret) return;
    CK_CHECK(caret->visible);
    const Rect absolute = h.table.absolute_bounds();
    CK_CHECK(caret->position == (ckv::Point{absolute.x + 2, absolute.y + 1}));
}

// --- WP-38 review findings A16-A21 -----------------------------------------

namespace {
// A provider whose cells carry colours of their own.
struct StyledProvider final : TableModel {
    std::vector<TableRowId> order{1, 2};
    std::optional<ckv::Style> style;
    std::size_t row_count() const override { return order.size(); }
    TableRowId row_id_at(std::size_t index) const override { return order[index]; }
    std::optional<std::size_t> index_of(TableRowId id) const override {
        const auto found = std::find(order.begin(), order.end(), id);
        return found == order.end() ? std::nullopt : std::optional<std::size_t>(found - order.begin());
    }
    TableCell cell(TableCellRef) const override { return TableCell{std::string{"x"}, {}, style, false}; }
};

ckv::MouseEvent press(ckv::MouseButton button, ckv::Point p) {
    return ckv::MouseEvent{ckv::MouseAction::Down, button, p, std::nullopt, Modifier::None};
}
}  // namespace

CK_TEST(a_styled_cursor_cell_still_shows_the_cursor) {
    // A16: a cell's own colours are its content; the cursor is drawn over
    // them (D-067) -- swapped, and marked -- rather than lost under them.
    Hosted h;
    StyledProvider model;
    ckv::Style own;
    own.fg = ckv::Color::rgb(200, 30, 30);
    own.bg = ckv::Color::rgb(250, 250, 250);
    model.style = own;
    h.table.set_model(model);
    h.app.set_focus(&h.table);

    Surface surface = h.draw();
    const ckv::Style cursor = surface.at(ckv::Point{0, 1}).style();
    CK_CHECK(surface.at(ckv::Point{0, 2}).style() == own);  // no cursor: the cell as it styles itself
    CK_CHECK(cursor.fg == own.bg);
    CK_CHECK(cursor.bg == own.fg);
    CK_CHECK(has_attr(cursor.attrs, ckv::Attr::Bold));
    CK_CHECK(has_attr(cursor.attrs, ckv::Attr::Underline));

    // Colours that would not read swapped give way to the highlight's.
    own.fg = own.bg;
    model.style = own;
    const ckv::Style selected = h.f.theme.resolve(h.f.roles.list_selected);
    surface = h.draw();
    CK_CHECK(surface.at(ckv::Point{0, 1}).style().fg == selected.fg);
    CK_CHECK(surface.at(ckv::Point{0, 1}).style().bg == selected.bg);
    h.table.clear_model();
}

CK_TEST(a_style_hook_colours_the_cell_and_the_cursor_is_drawn_over_it) {
    // A16, the materialized path: the hook says what a cell looks like, and
    // the cursor composes over that exactly as over a provider cell's style.
    Fixture f;
    ckv::term::HeadlessTerminal term{ckv::Size{30, 6}};
    ckv::ManualClock clock;
    ckv::ui::Application app{term, clock};
    Table table;
    table.set_columns({TableColumn{"Name", 8, 3}});
    table.set_rows({{"a"}, {"b"}});
    table.set_context(ckv::ui::Context{&f.theme, &f.registry, &app});
    table.set_bounds(Rect{0, 0, 20, 4});
    ckv::Style own;
    own.fg = ckv::Color::rgb(0, 0, 160);
    own.bg = ckv::Color::rgb(240, 240, 200);
    const ckv::Style normal = f.theme.resolve(f.roles.list_normal);
    std::vector<ckv::Style> bases;
    table.set_cell_style_hook([&](std::size_t, std::size_t, ckv::Style base) {
        bases.push_back(base);
        return own;
    });
    Surface surface(ckv::Size{20, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 20, 4});
    table.draw(painter);
    CK_CHECK(bases.size() == 2);
    CK_CHECK(std::all_of(bases.begin(), bases.end(), [&](const ckv::Style& base) { return base == normal; }));
    CK_CHECK(surface.at(ckv::Point{0, 2}).style() == own);
    CK_CHECK(surface.at(ckv::Point{0, 1}).style().fg == own.bg);
    CK_CHECK(surface.at(ckv::Point{0, 1}).style().bg == own.fg);
    // Without the keyboard the cursor is still marked, more quietly.
    CK_CHECK(has_attr(surface.at(ckv::Point{0, 1}).style().attrs, ckv::Attr::Underline));
    CK_CHECK(!has_attr(surface.at(ckv::Point{0, 1}).style().attrs, ckv::Attr::Bold));
}

CK_TEST(enter_on_an_edit_that_does_not_commit_is_still_the_tables) {
    // A17: the edit stays open with its diagnostic; Enter must not fall
    // through to a dialog's default button while the reader fixes it.
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Count", 8, 3, ckv::widgets::TableCellType::Integer, true}});
    table.set_rows({{"1"}});
    CK_CHECK(table.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "x"}}));
    CK_CHECK(table.on_key(key(Key::Enter)));
    CK_CHECK(table.editing());
    CK_CHECK(table.edit_diagnostic() == "Enter an integer");
}

CK_TEST(a_sort_column_the_new_columns_no_longer_have_is_dropped) {
    // A18: a sort column past the end of the new list would index rows set
    // later for those columns.
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"A", 5, 3}, TableColumn{"B", 5, 3}, TableColumn{"C", 5, 3}});
    table.sort_by(2, true);
    table.set_columns({TableColumn{"A", 5, 3}});
    CK_CHECK(table.sort_column() == -1);
    table.set_rows({{"b"}, {"a"}});
    CK_CHECK(table.row(0)[0] == "b");  // natural order

    // A sort column still inside the new list is kept.
    table.sort_by(0, false);
    table.set_rows({});
    table.set_columns({TableColumn{"A", 5, 3}, TableColumn{"B", 5, 3}});
    CK_CHECK(table.sort_column() == 0);
    CK_CHECK(!table.sort_ascending());
}

CK_TEST(only_the_primary_button_sorts_or_moves_the_table_cursor) {
    // A19: a right or middle press is not a click on the table; it is left
    // for whoever offers a context menu.
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"A", 5, 3}});
    table.set_rows({{"b"}, {"a"}});
    table.set_bounds(Rect{0, 0, 20, 5});
    for (const ckv::MouseButton button : {ckv::MouseButton::Right, ckv::MouseButton::Middle}) {
        CK_CHECK(!table.on_mouse(press(button, ckv::Point{0, 0})));
        CK_CHECK(table.sort_column() == -1);
        CK_CHECK(!table.on_mouse(press(button, ckv::Point{0, 2})));
        CK_CHECK(table.cursor_row() == 0);
        CK_CHECK(!table.on_mouse(press(button, ckv::Point{5, 0})));  // a column boundary
        CK_CHECK(!table.on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, button, ckv::Point{9, 0},
                                                 std::nullopt, Modifier::None}));
        CK_CHECK(table.columns()[0].width == 5);
    }
    CK_CHECK(table.on_mouse(press(ckv::MouseButton::Left, ckv::Point{0, 2})));
    CK_CHECK(table.cursor_row() == 1);
}

CK_TEST(cancelling_an_edit_repaints_the_cell) {
    // A20: the field face and the caret go away at once, not at whatever
    // repaint happens to come next.
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Name", 8, 3, ckv::widgets::TableCellType::Text, true}});
    table.set_rows({{"a"}});
    CK_CHECK(table.begin_edit());
    int repaints = 0;
    table.set_dirty_rect_sink([&](Rect) { ++repaints; });
    table.cancel_edit();
    CK_CHECK(!table.editing());
    CK_CHECK(repaints >= 1);
    // With no edit open there is nothing to take back.
    repaints = 0;
    table.cancel_edit();
    CK_CHECK(repaints == 0);
}

CK_TEST(a_model_sort_is_reported_whether_or_not_a_cell_is_selected) {
    // A21: the owner learns of every request it has to carry out; the
    // selected cell comes with it when there is one.
    Fixture f;
    Provider model;
    auto table = make_table(f);
    table.set_columns({TableColumn{"Value", 8, 3}});
    table.set_model(model);
    std::vector<std::optional<TableCellRef>> reported;
    table.on_sort_requested = [&](std::optional<TableCellRef> selected, std::optional<std::size_t>, bool) {
        reported.push_back(selected);
    };
    table.sort_by(0, true);
    CK_CHECK(reported.size() == 1);
    CK_CHECK((reported.back() == std::optional<TableCellRef>{TableCellRef{10, 0}}));

    model.order = {20, 30};
    table.model_changed();  // the selected row is gone
    CK_CHECK(!table.selected_cell());
    table.sort_by(0, false);
    CK_CHECK(reported.size() == 2);
    CK_CHECK(!reported.back().has_value());
}

// --- WP-38 review finding A49 ------------------------------------------------

CK_TEST(new_columns_that_do_not_fit_the_materialized_rows_abort) {
    // A49: set_rows and set_columns hold one rule -- every materialized row
    // has exactly one cell per column -- whichever of the two comes second.
    CK_EXPECT_ABORT({
        Fixture f;
        auto table = make_table(f);
        table.set_columns({TableColumn{"A", 5, 3}, TableColumn{"B", 5, 3}});
        table.set_rows({{"1", "2"}});
        table.set_columns({TableColumn{"A", 5, 3}, TableColumn{"B", 5, 3}, TableColumn{"C", 5, 3}});
    });
}

CK_TEST(columns_may_change_freely_while_no_rows_are_materialized) {
    // A49: with no rows, or with a model supplying them, there is nothing for
    // new columns to disagree with.
    Fixture f;
    auto table = make_table(f);
    table.set_columns({TableColumn{"A", 5, 3}});
    table.set_rows({});
    table.set_columns({TableColumn{"A", 5, 3}, TableColumn{"B", 5, 3}});
    table.set_rows({{"1", "2"}});
    table.set_columns({TableColumn{"X", 5, 3}, TableColumn{"Y", 5, 3}});  // same count: fine
    CK_CHECK(table.columns()[0].title == "X");
    Provider model;
    table.set_model(model);
    table.set_columns({TableColumn{"Value", 8, 3}});
    CK_CHECK(table.columns().size() == 1);
    table.clear_model();
}

CK_TEST(a_scripted_table_sorts_on_a_header_click_and_edits_a_typed_cell_from_the_keyboard) {
    // Application-level script: the header click and every key reach the
    // table through Application::dispatch, and step() shows the rows as the
    // sort and the committed edit left them.
    ckv::term::HeadlessTerminal term(ckv::Size{30, 8});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* table = app.root().add(std::make_unique<Table>());
    table->set_bounds(Rect{0, 0, 30, 6});
    table->set_columns({TableColumn{"Name", 10, 3},
                        TableColumn{"Qty", 6, 3, ckv::widgets::TableCellType::Integer, true}});
    table->set_rows({{"Pear", "3"}, {"Apple", "7"}, {"Fig", "1"}});
    app.set_focus(table);
    app.step(0);
    const auto row = [&](int y) {
        std::string out;
        for (int x = 0; x < 30; ++x) out += app.composed_surface().at(ckv::Point{x, y}).grapheme();
        return out;
    };
    CK_CHECK(row(1).starts_with("Pear"));

    CK_CHECK(app.dispatch(click(ckv::Point{2, 0})));
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{2, 0}, std::nullopt,
                                 Modifier::None});
    app.step(0);
    CK_CHECK(table->sort_column() == 0);
    CK_CHECK(row(1).starts_with("Apple"));
    CK_CHECK(row(3).starts_with("Pear"));

    // Typing on a cell starts an edit that replaces it; Enter commits it
    // through the column's Integer type.
    CK_CHECK(app.dispatch(key(Key::Home)));
    CK_CHECK(app.dispatch(key(Key::Right)));
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "1"}}));
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "2"}}));
    CK_CHECK(table->editing());
    CK_CHECK(app.dispatch(key(Key::Enter)));
    app.step(0);
    CK_CHECK(!table->editing());
    CK_CHECK(table->row(0)[1] == "12");
    CK_CHECK(row(1).find("12") != std::string::npos);

    // A value the type refuses keeps the edit open; Escape abandons it.
    CK_CHECK(app.dispatch(key(Key::Enter)));
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "x"}}));
    CK_CHECK(app.dispatch(key(Key::Enter)));
    CK_CHECK(table->editing());
    CK_CHECK(app.dispatch(key(Key::Escape)));
    CK_CHECK(!table->editing());
    CK_CHECK(table->row(0)[1] == "12");
}

CK_TEST(a_scripted_table_scrolls_its_body_by_the_wheel_and_keeps_the_cursor_cell) {
    // Application-level script: a wheel notch over the body scrolls it by
    // ui::kWheelRows rows; the header stays, and so does the cursor cell.
    ckv::term::HeadlessTerminal term(ckv::Size{30, 8});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* table = app.root().add(std::make_unique<Table>());
    table->set_bounds(Rect{0, 0, 30, 5});
    table->set_columns({TableColumn{"Name", 10, 3}});
    std::vector<std::vector<std::string>> rows;
    for (int index = 0; index < 12; ++index) rows.push_back({"row " + std::to_string(index)});
    table->set_rows(std::move(rows));
    app.step(0);
    const auto row = [&](int y) {
        std::string out;
        for (int x = 0; x < 29; ++x) out += app.composed_surface().at(ckv::Point{x, y}).grapheme();
        return out;
    };
    const auto wheel = [&](ckv::MouseButton direction) {
        const bool handled = app.dispatch(
            ckv::MouseEvent{ckv::MouseAction::Wheel, direction, ckv::Point{3, 2}, std::nullopt, Modifier::None});
        app.step(0);
        return handled;
    };
    CK_CHECK(row(1).starts_with("row 0"));
    const int cursor_row = table->cursor_row();

    CK_CHECK(wheel(ckv::MouseButton::WheelDown));
    CK_CHECK(row(0).starts_with("Name"));
    CK_CHECK(row(1).starts_with("row 3"));
    CK_CHECK(table->cursor_row() == cursor_row);
    // The last page stops the wheel.
    for (int notch = 0; notch < 5; ++notch) wheel(ckv::MouseButton::WheelDown);
    CK_CHECK(row(1).starts_with("row 8"));
    CK_CHECK(row(4).starts_with("row 11"));
    CK_CHECK(wheel(ckv::MouseButton::WheelUp));
    CK_CHECK(row(1).starts_with("row 5"));
}


CK_TEST(table_banding_and_rules_preserve_cell_geometry_and_editor_identity) {
    Fixture f;
    Table table;
    table.set_context(f.ctx());
    table.set_columns({TableColumn{"Name", 5, 2}, TableColumn{"Value", 5, 2}});
    table.set_rows({{"One", "A"}, {"Two", "B"}, {"Three", "C"}});
    table.set_bounds(Rect{0, 0, 16, 5});
    table.set_selected_cell(TableCellRef{1, 0});
    const auto selected = table.selected_cell();
    table.set_banded_rows(true);
    table.set_column_dividers(true);
    CK_CHECK(table.selected_cell() == selected);
    Surface surface(ckv::Size{16, 5});
    Painter painter(surface, Rect{0, 0, 16, 5});
    table.draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 2}).style() == f.theme.resolve(f.roles.table_banded));
    CK_CHECK(surface.at(ckv::Point{6, 2}).grapheme() == "B");
    CK_CHECK(surface.at(ckv::Point{5, 2}).grapheme() == "│");
    CK_CHECK(surface.at(ckv::Point{5, 0}).grapheme() == "│");
    const ckv::Style explicit_style{ckv::Color::rgb(180, 20, 20), ckv::Color::rgb(250, 250, 250), ckv::Attr::Bold};
    table.set_cell_style_hook([&](std::size_t row, std::size_t column, ckv::Style base) {
        return row == 1 && column == 1 ? explicit_style : base;
    });
    table.draw(painter);
    CK_CHECK(surface.at(ckv::Point{6, 2}).style() == explicit_style);
    CK_CHECK(surface.at(ckv::Point{5, 2}).style().bg == f.theme.resolve(f.roles.table_banded).bg);
    table.set_column_dividers(false);
    table.draw(painter);
    CK_CHECK(surface.at(ckv::Point{5, 2}).grapheme() == " ");
    CK_CHECK(table.on_mouse(click(ckv::Point{6, 2})));
    CK_CHECK(table.selected_cell() == (std::optional<TableCellRef>{TableCellRef{2, 1}}));
}
