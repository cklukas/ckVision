// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/cell_grid.hpp"

#include <string>
#include <vector>

#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Attr;
using ckv::Color;
using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::Style;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::CellAlignment;
using ckv::widgets::CellGrid;
using ckv::widgets::CellGridModel;
using ckv::widgets::GridCell;
using ckv::widgets::GridFrame;
using ckv::widgets::GridIndex;
using ckv::widgets::GridMove;
using ckv::widgets::GridPosition;
using ckv::widgets::GridRange;
using ckv::widgets::MaterializedCellGridModel;
using ckv::widgets::spreadsheet_column_label;

namespace {

struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    // Focus lives in an Application; everything else works on a bare theme
    // and registry.
    ckv::term::HeadlessTerminal terminal{ckv::Size{40, 12}};
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, &app}; }
};

ckv::KeyEvent key(Key k, Modifier modifiers = Modifier::None, std::string text = "") {
    return ckv::KeyEvent{KeyChord{k, modifiers, std::move(text)}};
}

ckv::MouseEvent mouse(ckv::MouseAction action, Point at, ckv::MouseButton button = ckv::MouseButton::Left,
                      Modifier modifiers = Modifier::None) {
    return ckv::MouseEvent{action, button, at, std::nullopt, modifiers};
}

// Three rows by three columns, "a".."i", five cells wide each: with a
// two-cell gutter the columns start at x = 2, 7 and 12.
MaterializedCellGridModel small_table() {
    MaterializedCellGridModel model;
    model.set_cells({{"a", "b", "c"}, {"d", "e", "f"}, {"g", "h", "i"}});
    model.set_uniform_column_width(5);
    return model;
}

MaterializedCellGridModel tall_table(std::size_t rows) {
    std::vector<std::vector<std::string>> cells;
    for (std::size_t row = 0; row < rows; ++row)
        cells.push_back({"r" + std::to_string(row + 1), "x", "y", "z"});
    MaterializedCellGridModel model;
    model.set_cells(std::move(cells));
    model.set_uniform_column_width(5);
    return model;
}

struct Painted {
    Surface surface;
    explicit Painted(ckv::Size size) : surface(size, ckv::Cell::from_grapheme(" ", Style{})) {}
    std::string row(int y) const {
        std::string text;
        for (int x = 0; x < surface.size().width; ++x) text += surface.at(Point{x, y}).grapheme();
        return text;
    }
    const Style& style(int x, int y) const { return surface.at(Point{x, y}).style(); }
};

Painted paint(CellGrid& grid, ckv::Size size) {
    Painted painted(size);
    Painter painter(painted.surface, Rect{0, 0, size.width, size.height});
    grid.draw(painter);
    return painted;
}

}  // namespace

// --- Column names --------------------------------------------------------

CK_TEST(spreadsheet_column_labels_follow_the_convention_every_reader_knows) {
    CK_CHECK(spreadsheet_column_label(0) == "A");
    CK_CHECK(spreadsheet_column_label(25) == "Z");
    CK_CHECK(spreadsheet_column_label(26) == "AA");
    CK_CHECK(spreadsheet_column_label(27) == "AB");
    CK_CHECK(spreadsheet_column_label(701) == "ZZ");
    CK_CHECK(spreadsheet_column_label(702) == "AAA");
}

// --- The materialized model ----------------------------------------------

CK_TEST(a_materialized_model_pads_every_row_to_the_widest) {
    MaterializedCellGridModel model;
    model.set_cells({{"a", "b", "c"}, {"d"}});
    CK_CHECK(model.row_count() == 2);
    CK_CHECK(model.column_count() == 3);
    CK_CHECK(model.cell(1, 2).text.empty());
    CK_CHECK(model.cell(1, 0).text == "d");
    CK_CHECK(model.row_label(1) == "2");
    CK_CHECK(model.column_label(2) == "C");
    CK_CHECK(model.row_header_width() == 2);
}

CK_TEST(arrows_move_the_materialized_cursor_and_clamp_at_the_edges) {
    MaterializedCellGridModel model = small_table();
    model.set_viewport(5, 20);
    model.navigate(GridMove::Up, false);
    CK_CHECK((model.cursor() == GridPosition{0, 0}));
    model.navigate(GridMove::Down, false);
    model.navigate(GridMove::Right, false);
    CK_CHECK((model.cursor() == GridPosition{1, 1}));
    model.navigate(GridMove::JumpRight, false);
    model.navigate(GridMove::Right, false);
    CK_CHECK((model.cursor() == GridPosition{1, 2}));
    model.navigate(GridMove::GridEnd, false);
    CK_CHECK((model.cursor() == GridPosition{2, 2}));
    model.navigate(GridMove::RowStart, false);
    CK_CHECK((model.cursor() == GridPosition{2, 0}));
    model.navigate(GridMove::GridStart, false);
    CK_CHECK((model.cursor() == GridPosition{0, 0}));
}

CK_TEST(shift_extends_a_selection_from_the_anchor_and_a_plain_move_drops_it) {
    MaterializedCellGridModel model = small_table();
    model.set_viewport(5, 20);
    CK_CHECK(!model.selection());
    model.navigate(GridMove::Right, true);
    model.navigate(GridMove::Down, true);
    CK_CHECK((model.anchor() == GridPosition{0, 0}));
    CK_CHECK((model.selection() == GridRange{{0, 0}, {1, 1}}));
    model.navigate(GridMove::Up, true);
    CK_CHECK((model.selection() == GridRange{{0, 0}, {0, 1}}));
    model.navigate(GridMove::Left, false);
    CK_CHECK(!model.selection());
    CK_CHECK((model.cursor() == GridPosition{0, 0}));
}

CK_TEST(a_page_is_the_body_height_and_the_body_scrolls_after_the_cursor) {
    MaterializedCellGridModel model = tall_table(30);
    model.set_viewport(5, 30);
    model.navigate(GridMove::PageDown, false);
    CK_CHECK((model.cursor() == GridPosition{5, 0}));
    CK_CHECK((model.origin() == GridPosition{1, 0}));
    const GridFrame frame = model.frame();
    CK_CHECK((frame.rows == std::vector<GridIndex>({1, 2, 3, 4, 5})));
    model.navigate(GridMove::PageUp, false);
    CK_CHECK((model.cursor() == GridPosition{0, 0}));
    CK_CHECK((model.origin() == GridPosition{0, 0}));
    model.navigate(GridMove::JumpDown, false);
    CK_CHECK((model.cursor() == GridPosition{29, 0}));
    CK_CHECK((model.origin() == GridPosition{25, 0}));
}

CK_TEST(frozen_bands_lead_the_frame_and_never_scroll) {
    MaterializedCellGridModel model = tall_table(30);
    model.set_frozen(1, 1);
    model.set_viewport(4, 20);
    GridFrame frame = model.frame();
    CK_CHECK(frame.frozen_rows == 1);
    CK_CHECK(frame.frozen_columns == 1);
    CK_CHECK((frame.rows == std::vector<GridIndex>({0, 1, 2, 3})));
    CK_CHECK((frame.columns == std::vector<GridIndex>({0, 1, 2, 3})));
    for (int step = 0; step < 6; ++step) model.navigate(GridMove::Down, false);
    frame = model.frame();
    CK_CHECK((model.cursor() == GridPosition{6, 0}));
    CK_CHECK((frame.rows == std::vector<GridIndex>({0, 4, 5, 6})));
    // The body never scrolls into the band.
    model.scroll_by(-10, -10);
    CK_CHECK((model.origin() == GridPosition{1, 1}));
}

CK_TEST(columns_fit_by_their_widths_and_the_body_follows_the_cursor_sideways) {
    MaterializedCellGridModel model = small_table();
    model.set_column_widths({4, 6, 10});
    model.set_viewport(5, 12);
    GridFrame frame = model.frame();
    CK_CHECK((frame.columns == std::vector<GridIndex>({0, 1})));
    model.navigate(GridMove::JumpRight, false);
    frame = model.frame();
    CK_CHECK((model.cursor() == GridPosition{0, 2}));
    CK_CHECK(model.origin().column == 2);
    CK_CHECK((frame.columns == std::vector<GridIndex>({2})));
    // A column wider than the viewport is still listed, alone.
    model.set_viewport(5, 3);
    CK_CHECK((model.frame().columns == std::vector<GridIndex>({2})));
}

CK_TEST(scroll_by_moves_the_body_and_leaves_the_cursor_where_it_is) {
    MaterializedCellGridModel model = tall_table(30);
    model.set_viewport(5, 30);
    model.scroll_by(2, 0);
    CK_CHECK((model.origin() == GridPosition{2, 0}));
    CK_CHECK((model.cursor() == GridPosition{0, 0}));
    model.scroll_by(-10, 0);
    CK_CHECK((model.origin() == GridPosition{0, 0}));
    model.scroll_by(100, 0);
    CK_CHECK((model.origin() == GridPosition{29, 0}));
}

CK_TEST(the_frame_lists_only_the_spans_that_reach_it) {
    MaterializedCellGridModel model = tall_table(30);
    model.set_viewport(5, 30);
    model.set_spans({GridRange{{0, 0}, {0, 1}}, GridRange{{20, 0}, {21, 3}}});
    CK_CHECK(model.frame().spans.size() == 1);
    model.navigate(GridMove::JumpDown, false);
    CK_CHECK(model.frame().spans.empty());
    model.scroll_by(-8, 0);
    CK_CHECK(model.frame().spans.size() == 1);
    CK_CHECK((model.frame().spans.front() == GridRange{{20, 0}, {21, 3}}));
}

// --- Painting -------------------------------------------------------------

CK_TEST(the_grid_paints_the_header_the_gutter_and_the_cells_in_their_roles) {
    Fixture f;
    MaterializedCellGridModel model = small_table();
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    f.app.set_focus(&grid);  // the cursor wears its own role while the grid holds the keyboard
    CK_CHECK(grid.gutter_width() == 2);
    const Painted painted = paint(grid, ckv::Size{20, 5});
    CK_CHECK(painted.row(0) == "    A    B    C     ");
    CK_CHECK(painted.row(1) == "1 a    b    c       ");
    CK_CHECK(painted.row(3) == "3 g    h    i       ");
    CK_CHECK(painted.row(4) == "                    ");
    CK_CHECK(painted.style(4, 0) == f.theme.resolve(f.roles.cell_grid_header));
    CK_CHECK(painted.style(0, 1) == f.theme.resolve(f.roles.cell_grid_header));
    CK_CHECK(painted.style(2, 1) == f.theme.resolve(f.roles.cell_grid_cursor));
    CK_CHECK(painted.style(7, 1) == f.theme.resolve(f.roles.cell_grid_normal));
    CK_CHECK(painted.style(2, 4) == f.theme.resolve(f.roles.cell_grid_normal));
}

CK_TEST(the_cursor_is_muted_while_the_keyboard_is_elsewhere) {
    // Two grids side by side must say which one the arrow keys move.
    Fixture f;
    MaterializedCellGridModel model = small_table();
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    CK_CHECK(paint(grid, ckv::Size{20, 5}).style(2, 1) == f.theme.resolve(f.roles.cell_grid_cursor_inactive));
    f.app.set_focus(&grid);
    CK_CHECK(paint(grid, ckv::Size{20, 5}).style(2, 1) == f.theme.resolve(f.roles.cell_grid_cursor));
}

CK_TEST(a_cell_aligned_to_the_end_ends_at_its_right_edge) {
    Fixture f;
    MaterializedCellGridModel model = small_table();
    model.set_cell({0, 1}, GridCell{"42", {}, CellAlignment::End});
    model.set_cell({0, 2}, GridCell{"mid", {}, CellAlignment::Center});
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    const Painted painted = paint(grid, ckv::Size{20, 5});
    CK_CHECK(painted.row(1) == "1 a       42 mid    ");
}

CK_TEST(the_header_can_be_hidden) {
    Fixture f;
    MaterializedCellGridModel model = small_table();
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 3});
    grid.set_model(model);
    grid.set_column_header_visible(false);
    CK_CHECK(grid.header_height() == 0);
    const Painted painted = paint(grid, ckv::Size{20, 3});
    CK_CHECK(painted.row(0) == "1 a    b    c       ");
    CK_CHECK((grid.cell_at(Point{2, 0}) == GridPosition{0, 0}));
}

CK_TEST(the_selection_wears_its_role_and_the_cursor_its_own) {
    Fixture f;
    MaterializedCellGridModel model = small_table();
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    f.app.set_focus(&grid);  // the cursor wears its own role while the grid holds the keyboard
    CK_CHECK(grid.on_key(key(Key::Right, Modifier::Shift)));
    CK_CHECK(grid.on_key(key(Key::Down, Modifier::Shift)));
    CK_CHECK((model.selection() == GridRange{{0, 0}, {1, 1}}));
    const Painted painted = paint(grid, ckv::Size{20, 5});
    CK_CHECK(painted.style(2, 1) == f.theme.resolve(f.roles.cell_grid_selection));
    CK_CHECK(painted.style(7, 1) == f.theme.resolve(f.roles.cell_grid_selection));
    CK_CHECK(painted.style(7, 2) == f.theme.resolve(f.roles.cell_grid_cursor));
    CK_CHECK(painted.style(12, 1) == f.theme.resolve(f.roles.cell_grid_normal));
}

CK_TEST(a_coloured_cell_swaps_its_colours_under_the_cursor_and_keeps_its_attributes) {
    Fixture f;
    MaterializedCellGridModel model = small_table();
    GridCell cell;
    cell.text = "a";
    cell.style.foreground = Color::rgb(255, 255, 0);
    cell.style.background = Color::rgb(0, 0, 128);
    cell.style.attributes = Attr::Italic;
    model.set_cell({0, 0}, cell);
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    f.app.set_focus(&grid);  // the cursor wears its own role while the grid holds the keyboard
    const Painted painted = paint(grid, ckv::Size{20, 5});
    const Style& at_cursor = painted.style(2, 1);
    CK_CHECK(at_cursor.fg == Color::rgb(0, 0, 128));
    CK_CHECK(at_cursor.bg == Color::rgb(255, 255, 0));
    CK_CHECK(ckv::has_attr(at_cursor.attrs, Attr::Italic));
    CK_CHECK(ckv::has_attr(at_cursor.attrs, Attr::Bold));
    CK_CHECK(ckv::has_attr(at_cursor.attrs, Attr::Underline));
    // The same cell away from the cursor keeps its own colours, unswapped.
    model.place_cursor({1, 1}, false);
    grid.model_changed();
    const Painted moved = paint(grid, ckv::Size{20, 5});
    CK_CHECK(moved.style(2, 1).fg == Color::rgb(255, 255, 0));
    CK_CHECK(moved.style(2, 1).bg == Color::rgb(0, 0, 128));
    CK_CHECK(!ckv::has_attr(moved.style(2, 1).attrs, Attr::Bold));
}

CK_TEST(an_unreadable_swap_falls_back_to_the_role_colours) {
    Fixture f;
    MaterializedCellGridModel model = small_table();
    GridCell cell;
    cell.text = "a";
    cell.style.foreground = Color::rgb(10, 10, 10);
    cell.style.background = Color::rgb(20, 20, 20);
    model.set_cell({0, 0}, cell);
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    f.app.set_focus(&grid);  // the cursor wears its own role while the grid holds the keyboard
    const Painted painted = paint(grid, ckv::Size{20, 5});
    const Style cursor = f.theme.resolve(f.roles.cell_grid_cursor);
    CK_CHECK(painted.style(2, 1).fg == cursor.fg);
    CK_CHECK(painted.style(2, 1).bg == cursor.bg);
    CK_CHECK(ckv::has_attr(painted.style(2, 1).attrs, Attr::Bold));
}

CK_TEST(a_span_anchor_paints_across_its_columns_and_the_covered_cells_keep_that_paint) {
    Fixture f;
    MaterializedCellGridModel model = small_table();
    model.set_cell({0, 0}, GridCell{"merged title", {}, CellAlignment::Start});
    model.set_cell({0, 1}, GridCell{"HIDDEN", {}, CellAlignment::Start});
    model.set_spans({GridRange{{0, 0}, {0, 1}}});
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    f.app.set_focus(&grid);  // the cursor wears its own role while the grid holds the keyboard
    const Painted painted = paint(grid, ckv::Size{20, 5});
    CK_CHECK(painted.row(1) == "1 merged titc       ");
    CK_CHECK((grid.cell_rect(GridPosition{0, 0}) == Rect{2, 1, 10, 1}));
    CK_CHECK((grid.cell_rect(GridPosition{0, 1}) == Rect{7, 1, 5, 1}));
    CK_CHECK((!grid.cell_rect(GridPosition{9, 9})));
    // The cursor on a covered cell shows where it really is.
    model.place_cursor({0, 1}, false);
    grid.model_changed();
    const Painted moved = paint(grid, ckv::Size{20, 5});
    CK_CHECK(moved.row(1) == "1 merge     c       ");
    CK_CHECK(moved.style(7, 1) == f.theme.resolve(f.roles.cell_grid_cursor));
}

CK_TEST(the_grid_asks_the_provider_only_for_the_cells_it_lists) {
    struct Counting final : CellGridModel {
        MaterializedCellGridModel inner = tall_table(1000);
        mutable std::size_t cell_queries = 0;
        void set_viewport(int rows, int width) override { inner.set_viewport(rows, width); }
        GridFrame frame() const override { return inner.frame(); }
        int column_width(GridIndex column) const override { return inner.column_width(column); }
        int row_header_width() const override { return inner.row_header_width(); }
        std::string row_label(GridIndex row) const override { return inner.row_label(row); }
        std::string column_label(GridIndex column) const override { return inner.column_label(column); }
        GridCell cell(GridIndex row, GridIndex column) const override {
            ++cell_queries;
            return inner.cell(row, column);
        }
        GridPosition cursor() const override { return inner.cursor(); }
        std::optional<GridRange> selection() const override { return inner.selection(); }
        void navigate(GridMove move, bool extend) override { inner.navigate(move, extend); }
        void place_cursor(GridPosition at, bool extend) override { inner.place_cursor(at, extend); }
        void scroll_by(int rows, int columns) override { inner.scroll_by(rows, columns); }
    };
    Fixture f;
    Counting model;
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    CK_CHECK(grid.gutter_width() == 5);
    (void)paint(grid, ckv::Size{20, 5});
    CK_CHECK(grid.frame().rows.size() == 4);
    CK_CHECK(grid.frame().columns.size() == 3);
    CK_CHECK(model.cell_queries == 12);
}

// --- Keyboard -------------------------------------------------------------

CK_TEST(keys_ask_the_provider_to_navigate) {
    Fixture f;
    MaterializedCellGridModel model = tall_table(30);
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 30, 6});
    grid.set_model(model);
    int changes = 0;
    grid.on_changed = [&changes] { ++changes; };
    CK_CHECK(grid.on_key(key(Key::Down)));
    CK_CHECK((model.cursor() == GridPosition{1, 0}));
    CK_CHECK(grid.on_key(key(Key::Right, Modifier::Ctrl)));
    CK_CHECK((model.cursor() == GridPosition{1, 3}));
    CK_CHECK(grid.on_key(key(Key::Home)));
    CK_CHECK((model.cursor() == GridPosition{1, 0}));
    CK_CHECK(grid.on_key(key(Key::End, Modifier::Ctrl)));
    CK_CHECK((model.cursor() == GridPosition{29, 3}));
    CK_CHECK(grid.on_key(key(Key::Home, Modifier::Ctrl)));
    CK_CHECK((model.cursor() == GridPosition{0, 0}));
    CK_CHECK(grid.on_key(key(Key::PageDown)));
    CK_CHECK((model.cursor() == GridPosition{5, 0}));
    CK_CHECK(grid.frame().rows.front() == 1);
    // Ctrl+PageDown belongs to the owner.
    CK_CHECK(!grid.on_key(key(Key::PageDown, Modifier::Ctrl)));
    CK_CHECK(!grid.on_key(key(Key::Down, Modifier::Alt)));
    CK_CHECK(changes == 6);
}

CK_TEST(activation_type_ahead_and_clear_reach_the_owner_only_when_installed) {
    Fixture f;
    MaterializedCellGridModel model = small_table();
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    CK_CHECK(!grid.on_key(key(Key::Enter)));
    CK_CHECK(!grid.on_key(key(Key::F2)));
    CK_CHECK(!grid.on_key(key(Key::Delete)));
    CK_CHECK(!grid.on_key(key(Key::Char, Modifier::None, "x")));

    int activated = 0;
    std::string typed;
    int cleared = 0;
    grid.on_activate = [&activated] { ++activated; };
    grid.on_type_ahead = [&typed](const std::string& text) { typed += text; };
    grid.on_clear_request = [&cleared] { ++cleared; };
    CK_CHECK(grid.on_key(key(Key::Enter)));
    CK_CHECK(grid.on_key(key(Key::F2)));
    CK_CHECK(activated == 2);
    CK_CHECK(grid.on_key(key(Key::Char, Modifier::None, "x")));
    CK_CHECK(grid.on_key(key(Key::Char, Modifier::None, "=")));
    CK_CHECK(typed == "x=");
    CK_CHECK(!grid.on_key(key(Key::Char, Modifier::Ctrl, "x")));
    CK_CHECK(grid.on_key(key(Key::Delete)));
    CK_CHECK(cleared == 1);
    CK_CHECK(!grid.on_key(key(Key::Enter, Modifier::Ctrl)));
}

CK_TEST(an_empty_grid_leaves_every_key_to_its_owner) {
    Fixture f;
    MaterializedCellGridModel model;
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    grid.on_activate = [] {};
    CK_CHECK(!grid.on_key(key(Key::Down)));
    CK_CHECK(!grid.on_key(key(Key::Enter)));
    CK_CHECK((!grid.on_mouse(mouse(ckv::MouseAction::Down, Point{3, 1}))));
    const Painted painted = paint(grid, ckv::Size{20, 5});
    CK_CHECK(painted.row(1) == "                    ");
}

// --- Mouse ----------------------------------------------------------------

CK_TEST(a_press_places_the_cursor_and_a_drag_extends_the_selection) {
    Fixture f;
    MaterializedCellGridModel model = small_table();
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    CK_CHECK((grid.cell_at(Point{8, 2}) == GridPosition{1, 1}));
    CK_CHECK(!grid.cell_at(Point{8, 0}));   // the header
    CK_CHECK(!grid.cell_at(Point{1, 2}));   // the gutter
    CK_CHECK(!grid.cell_at(Point{18, 2}));  // past the last column
    CK_CHECK(!grid.cell_at(Point{8, 4}));   // below the last row
    CK_CHECK((grid.on_mouse(mouse(ckv::MouseAction::Down, Point{8, 2}))));
    CK_CHECK((model.cursor() == GridPosition{1, 1}));
    CK_CHECK(!model.selection());
    CK_CHECK((grid.on_mouse(mouse(ckv::MouseAction::Move, Point{13, 3}))));
    CK_CHECK((model.cursor() == GridPosition{2, 2}));
    CK_CHECK((model.selection() == GridRange{{1, 1}, {2, 2}}));
    CK_CHECK((grid.on_mouse(mouse(ckv::MouseAction::Up, Point{13, 3}))));
    CK_CHECK((!grid.on_mouse(mouse(ckv::MouseAction::Move, Point{3, 1}))));
    CK_CHECK((model.cursor() == GridPosition{2, 2}));
    // A press with Shift extends the selection from where it started.
    CK_CHECK((grid.on_mouse(mouse(ckv::MouseAction::Down, Point{3, 1}, ckv::MouseButton::Left, Modifier::Shift))));
    CK_CHECK((model.selection() == GridRange{{0, 0}, {1, 1}}));
    CK_CHECK((!grid.on_mouse(mouse(ckv::MouseAction::Down, Point{3, 0}))));
    CK_CHECK((!grid.on_mouse(mouse(ckv::MouseAction::Down, Point{3, 1}, ckv::MouseButton::Right))));
}

CK_TEST(the_wheel_scrolls_the_body) {
    Fixture f;
    MaterializedCellGridModel model = tall_table(30);
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 30, 6});
    grid.set_model(model);
    CK_CHECK((grid.on_mouse(mouse(ckv::MouseAction::Wheel, Point{5, 3}, ckv::MouseButton::WheelDown))));
    CK_CHECK((model.origin() == GridPosition{3, 0}));
    CK_CHECK((model.cursor() == GridPosition{0, 0}));
    CK_CHECK(grid.frame().rows.front() == 3);
    CK_CHECK((grid.on_mouse(mouse(ckv::MouseAction::Wheel, Point{5, 3}, ckv::MouseButton::WheelUp))));
    CK_CHECK((model.origin() == GridPosition{0, 0}));
    CK_CHECK((grid.on_mouse(mouse(ckv::MouseAction::Wheel, Point{5, 3}, ckv::MouseButton::WheelRight))));
    CK_CHECK((model.origin() == GridPosition{0, 1}));
}

CK_TEST(the_second_press_of_a_double_click_activates_the_cell_and_starts_no_drag) {
    // The grid does not time presses: Application counts them on its clock
    // (MouseEvent::click_count) and the script below covers that end to end.
    Fixture f;
    MaterializedCellGridModel model = small_table();
    CellGrid grid;
    grid.set_context(f.ctx());
    grid.set_bounds(Rect{0, 0, 20, 5});
    grid.set_model(model);
    int activated = 0;
    grid.on_activate = [&activated] { ++activated; };
    CK_CHECK((grid.on_mouse(mouse(ckv::MouseAction::Down, Point{8, 2}))));
    CK_CHECK((grid.on_mouse(mouse(ckv::MouseAction::Up, Point{8, 2}))));
    CK_CHECK(activated == 0);
    ckv::MouseEvent second = mouse(ckv::MouseAction::Down, Point{8, 2});
    second.click_count = 2;
    CK_CHECK(grid.on_mouse(second));
    CK_CHECK(activated == 1);
    CK_CHECK((model.cursor() == GridPosition{1, 1}));
    // No drag follows the second press.
    CK_CHECK((!grid.on_mouse(mouse(ckv::MouseAction::Move, Point{13, 3}))));
    CK_CHECK((model.cursor() == GridPosition{1, 1}));
}

CK_TEST(a_scripted_cell_grid_is_walked_selected_and_activated_through_dispatched_input) {
    // Application-level script: the grid sits in a real Application, and
    // every key, press, drag and wheel turn reaches it through dispatch. The
    // clock that times the double click is the application's own.
    ckv::term::HeadlessTerminal term(ckv::Size{30, 8});
    ckv::ManualClock clock(1'000'000'000);
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    MaterializedCellGridModel model = small_table();
    auto* grid = app.root().add(std::make_unique<CellGrid>());
    grid->set_bounds(Rect{0, 0, 20, 5});
    grid->set_model(model);
    int activated = 0;
    std::vector<std::string> typed;
    grid->on_activate = [&] { ++activated; };
    grid->on_type_ahead = [&](const std::string& text) { typed.push_back(text); };
    app.set_focus(grid);
    app.step(0);

    CK_CHECK(app.dispatch(key(Key::Down)));
    CK_CHECK(app.dispatch(key(Key::Right, Modifier::Shift)));
    CK_CHECK((model.cursor() == GridPosition{1, 1}));
    CK_CHECK((model.selection() == GridRange{{1, 0}, {1, 1}}));
    CK_CHECK(app.dispatch(key(Key::Enter)));
    CK_CHECK(activated == 1);
    CK_CHECK(app.dispatch(key(Key::Char, Modifier::None, "q")));
    CK_CHECK((typed == std::vector<std::string>{"q"}));

    // A press places the cursor, a drag extends from it, and a second press
    // on the same cell within the double-click interval of the application's
    // clock activates it.
    CK_CHECK(app.dispatch(mouse(ckv::MouseAction::Down, Point{3, 1})));
    app.dispatch(mouse(ckv::MouseAction::Move, Point{13, 3}));
    app.dispatch(mouse(ckv::MouseAction::Up, Point{13, 3}));
    app.step(0);
    CK_CHECK((model.selection() == GridRange{{0, 0}, {2, 2}}));
    clock.advance(100'000'000);
    app.dispatch(mouse(ckv::MouseAction::Down, Point{13, 3}));
    app.dispatch(mouse(ckv::MouseAction::Up, Point{13, 3}));
    clock.advance(100'000'000);
    app.dispatch(mouse(ckv::MouseAction::Down, Point{13, 3}));
    app.dispatch(mouse(ckv::MouseAction::Up, Point{13, 3}));
    app.step(0);
    CK_CHECK(activated == 2);
    CK_CHECK((model.cursor() == GridPosition{2, 2}));
}
