// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/table.hpp"

#include <algorithm>

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
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
