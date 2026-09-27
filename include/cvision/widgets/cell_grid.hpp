// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// CellGrid: a two-dimensional grid of cells under a column header and beside
// a row gutter — the surface of a spreadsheet, a query result, a matrix —
// with a cursor, a rectangular selection, frozen leading bands and merged
// spans (D-067).
//
// The grid paints; the provider decides. A `CellGridModel` answers what the
// viewport shows (the frame), what each cell says, and where the cursor and
// the selection are — and it is asked to MOVE them. That is the opposite of
// ListView and Table, which keep their own cursor, and it is deliberate: in a
// grid of cells the cursor and the selection are what the application's
// commands act on (fill this range, sort that one, format the cell here), the
// rules for moving them need the application's knowledge (where a block of
// data ends, which rows are hidden, how a frozen band scrolls), and a second
// copy kept in the widget would have to be reconciled with the first on every
// key. So there is one copy, the provider's, and the widget asks for it
// whenever it paints or hit-tests. TerminalView relates to its emulator the
// same way. `MaterializedCellGridModel` is the provider for a grid whose
// content is a table of values with no application behind it.
//
// Indices are the provider's own: ordered integers in its row and column
// space, listed by the frame in the order they show. A hidden row is simply
// not listed; a span is resolved over the listed indices, so a region whose
// anchor has scrolled away still covers the cells the reader can see.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/core/style.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// An index in the provider's own row or column space. Ordered, so that a
// span can be resolved over the frame's entries; not necessarily dense.
using GridIndex = std::size_t;

// One cell, addressed by the provider's row and column indices.
struct GridPosition {
    // The provider's row and column index of the cell.
    GridIndex row = 0;
    GridIndex column = 0;

    // Memberwise equality.
    friend bool operator==(const GridPosition&, const GridPosition&) = default;
};

// An inclusive rectangle of provider indices, corners ordered so that
// `first` is the top-left cell and `last` the bottom-right one.
struct GridRange {
    // The top-left and bottom-right cells, both inside the range.
    GridPosition first;
    GridPosition last;

    // Whether `at` lies inside the rectangle, edges included. Compares indices only, so it
    // assumes the corners are ordered and counts an index the frame does not list as
    // inside.
    bool contains(GridPosition at) const noexcept {
        return at.row >= first.row && at.row <= last.row && at.column >= first.column &&
               at.column <= last.column;
    }

    // Memberwise equality.
    friend bool operator==(const GridRange&, const GridRange&) = default;
};

// Where a cell's text sits in the cell: numbers end at the right edge,
// words start at the left, a title may be centred.
enum class CellAlignment { Start, End, Center };

// What a cell changes about the grid's own cell style. Each member left
// empty keeps the theme's answer, so a provider says only what it knows —
// a fill colour without an opinion about the text colour, say — and a theme
// switch restyles everything the provider did not pin.
struct GridCellStyle {
    // The text and fill colours, each replacing the theme's when set.
    std::optional<Color> foreground;
    std::optional<Color> background;
    // Replaces the base attributes outright when set: a provider that states
    // a cell's weight and slant states all of them.
    std::optional<Attr> attributes;
    // The underline shape, replacing the theme's when set.
    std::optional<UnderlineShape> underline;

    // Whether the cell states a colour of its own — what decides how the
    // cursor and the selection are drawn over it (see CellGrid).
    bool has_color() const noexcept { return foreground.has_value() || background.has_value(); }

    // Memberwise equality.
    friend bool operator==(const GridCellStyle&, const GridCellStyle&) = default;
};

// What one cell shows, as the provider answers it.
struct GridCell {
    // The cell's UTF-8 text, drawn on one row and clipped, without an ellipsis, to the
    // cell's width (a span anchor's whole painted width). A cell a span covers shows none.
    std::string text;
    // What the cell changes about the grid's normal cell style.
    GridCellStyle style;
    // Where the text sits within that width.
    CellAlignment alignment = CellAlignment::Start;

    // Memberwise equality.
    friend bool operator==(const GridCell&, const GridCell&) = default;
};

// What the viewport shows: the visible rows and columns in display order —
// each frozen band first, then the scrolling body — and the merged spans
// that reach into them.
struct GridFrame {
    // The listed row and column indices, top to bottom and left to right. The grid draws
    // one screen row per listed row and each listed column at the provider's
    // column_width(), stopping at its right and bottom edges.
    std::vector<GridIndex> rows;
    std::vector<GridIndex> columns;
    // How many leading entries of `rows`/`columns` are the frozen band. They
    // never scroll; the body after them does.
    std::size_t frozen_rows = 0;
    std::size_t frozen_columns = 0;
    // Merged regions intersecting the frame, in provider indices. The first
    // cell of a span is its anchor: it carries the text, painted across every
    // listed column the span covers on the anchor's row. The other cells of
    // the span hold no content of their own.
    std::vector<GridRange> spans;

    // Memberwise equality.
    friend bool operator==(const GridFrame&, const GridFrame&) = default;
};

// A movement the grid asks its provider for. The provider decides what each
// means in its own space: a page is the body's height; a jump is to the edge
// of whatever the provider calls a block; the starts and ends are the
// provider's own.
enum class GridMove {
    Up,
    Down,
    Left,
    Right,
    PageUp,
    PageDown,
    JumpUp,
    JumpDown,
    JumpLeft,
    JumpRight,
    RowStart,
    RowEnd,
    GridStart,
    GridEnd,
};

// The provider: caller-owned, synchronous, queried only for the frame it
// declared. It owns the cursor, the selection and the scroll origin, and is
// asked to move them (D-067); after any change of its own it tells the grid
// with CellGrid::model_changed().
class CellGridModel {
public:
    // Providers are destroyed through this interface.
    virtual ~CellGridModel() = default;

    // The room the grid has for cells: the rows below the column header and
    // the cells to the right of the gutter, which the frame — frozen band and
    // body together — may fill. Called whenever the grid's geometry changes,
    // before the frame is asked for.
    virtual void set_viewport(int rows, int width) = 0;
    // What the viewport shows now. The grid reads it right after each set_viewport call,
    // which it makes on every relayout: a resize, set_model, model_changed(), a change of
    // header visibility and each request it makes of the provider. It keeps the copy for
    // drawing and hit-testing until the next read.
    virtual GridFrame frame() const = 0;

    // The width in cells of a listed column; the grid treats anything below 1 as 1.
    virtual int column_width(GridIndex column) const = 0;
    // The gutter's width in cells, including the blank cell that separates
    // the label from the first column; zero for no gutter.
    virtual int row_header_width() const = 0;
    // The labels for a listed row and column, asked for while drawing. A row label is
    // right-aligned in the gutter before its separating blank; a column label is centred
    // over its column in the header row. Both are clipped to the room they have.
    virtual std::string row_label(GridIndex row) const = 0;
    virtual std::string column_label(GridIndex column) const = 0;
    // The content of a listed cell, asked for while drawing.
    virtual GridCell cell(GridIndex row, GridIndex column) const = 0;

    // The cell the cursor is on. The grid draws it only where the frame lists it.
    virtual GridPosition cursor() const = 0;
    // Absent while the cursor stands alone.
    virtual std::optional<GridRange> selection() const = 0;

    // `extend` is the reader holding Shift: the selection grows from where
    // it started to where the cursor lands, instead of collapsing to the
    // cursor.
    virtual void navigate(GridMove move, bool extend) = 0;
    // Asked when the reader presses on a listed cell (`extend` while Shift is held) or drags
    // onto one (`extend` true).
    virtual void place_cursor(GridPosition at, bool extend) = 0;
    // A wheel moved the body by so many rows and columns; the cursor stays
    // where it is.
    virtual void scroll_by(int rows, int columns) = 0;
};

// "A", "B", ..., "Z", "AA", "AB", ...: the column names a reader of any
// spreadsheet knows, and the default column label of the materialized model.
std::string spreadsheet_column_label(GridIndex column);

// A provider whose content is a table of values held here, with the generic
// interaction rules: the cursor clamps to the table, a page is the body's
// height, a jump reaches the table's edge, and the body scrolls so the
// cursor stays in view. For a grid with an application behind it the
// application implements CellGridModel itself; this one is for a grid that
// shows a result, and for tests.
class MaterializedCellGridModel final : public CellGridModel {
public:
    // An empty table: no cells, columns 10 cells wide, nothing frozen. None of the setters
    // below tells a CellGrid showing this model; call CellGrid::model_changed() after them.
    MaterializedCellGridModel() = default;

    // Every row is padded to the widest row, so the table is rectangular.
    // Replaces every cell, including styles and alignments set with set_cell, with
    // plain start-aligned text; clamps the cursor into the new table, drops the selection
    // and scrolls the cursor into view.
    void set_cells(std::vector<std::vector<std::string>> rows);
    // One cell's full content, style and alignment included. `at` must lie inside the
    // table (asserted); set_cell never grows it. cell_at's reference stays valid until the
    // next set_cells.
    void set_cell(GridPosition at, GridCell cell);
    const GridCell& cell_at(GridPosition at) const;
    // The table's size; column_count() is the length of the widest row given to set_cells.
    std::size_t row_count() const noexcept { return rows_.size(); }
    std::size_t column_count() const noexcept { return column_count_; }

    // Widths default to the uniform width; a per-column width wins where
    // one is given. Every width is in cells and raised to at least 1.
    void set_uniform_column_width(int width);
    void set_column_widths(std::vector<int> widths);
    // Labels default to 1-based row numbers and spreadsheet column names.
    // A label list shorter than the table leaves the remaining indices at their default.
    void set_row_labels(std::vector<std::string> labels);
    void set_column_labels(std::vector<std::string> labels);
    // How many leading rows and columns stay in place while the body scrolls. The frame
    // limits them to what can work: fewer rows than the viewport, so at least one body row
    // remains, and fewer columns than the table has.
    void set_frozen(std::size_t rows, std::size_t columns);
    // The merged regions, each with ordered corners, stored as given. The frame reports
    // those that intersect the rectangle from its first to its last listed cell.
    void set_spans(std::vector<GridRange> spans);

    // The first body row and column shown.
    GridPosition origin() const noexcept { return origin_; }
    // Where the selection started, while one is being extended.
    std::optional<GridPosition> anchor() const noexcept { return anchor_; }

    // Only keeps the origin valid for the new size: a reader who scrolled away from the
    // cursor is not brought back by a resize, only by the next cursor movement.
    void set_viewport(int rows, int width) override;
    GridFrame frame() const override;
    int column_width(GridIndex column) const override;
    // The digit count of the row count (at least 1), plus the separating blank.
    int row_header_width() const override;
    std::string row_label(GridIndex row) const override;
    std::string column_label(GridIndex column) const override;
    GridCell cell(GridIndex row, GridIndex column) const override;
    GridPosition cursor() const override { return cursor_; }
    std::optional<GridRange> selection() const override;
    void navigate(GridMove move, bool extend) override;
    void place_cursor(GridPosition at, bool extend) override;
    // Moves the origin, never past the frozen band before it or the table's last row and
    // column after it; the cursor and the selection stay.
    void scroll_by(int rows, int columns) override;

private:
    std::size_t frozen_row_band() const noexcept;
    std::size_t frozen_column_band() const noexcept;
    std::size_t body_rows() const noexcept;
    // How many body columns fit in the viewport starting at `from` — at
    // least one, so a column wider than the viewport is still reachable.
    std::size_t fitted_columns(GridIndex from) const noexcept;
    void begin_extend(bool extend);
    void scroll_to_cursor();
    void clamp_origin();
    GridPosition clamped(GridPosition at) const noexcept;

    std::vector<std::vector<GridCell>> rows_;
    std::size_t column_count_ = 0;
    int uniform_column_width_ = 10;
    std::vector<int> column_widths_;
    std::vector<std::string> row_labels_;
    std::vector<std::string> column_labels_;
    std::size_t frozen_rows_ = 0;
    std::size_t frozen_columns_ = 0;
    std::vector<GridRange> spans_;
    int viewport_rows_ = 1;
    int viewport_width_ = 1;
    GridPosition cursor_;
    GridPosition origin_;
    std::optional<GridPosition> anchor_;
};

// Resolves its own theme roles from context() once attached (D-028):
// "ckv.cellgrid.normal" for a cell, "ckv.cellgrid.header" for the column
// header and the gutter, "ckv.cellgrid.cursor" for the cursor cell while the
// grid holds the keyboard ("ckv.cellgrid.cursor.inactive" while it does not)
// and "ckv.cellgrid.selection" for the other selected cells.
//
// How the cursor and the selection are drawn over a cell:
//   * a cell with no colour of its own wears the role — its colours, and
//     its attributes on top of the cell's own;
//   * a cell with a colour of its own keeps both of its colours and swaps
//     them, so the document's colouring stays visible under the highlight.
//     When the swapped pair would not be readable — the two colours equal,
//     or their contrast under 3:1 by the grid's own luminance estimate —
//     the role's colours are used instead. The cursor is additionally bold
//     and underlined in the swapped case (only underlined while the grid does
//     not hold the keyboard), so it stays distinguishable inside a selection
//     drawn from the same two colours.
//
// Keyboard: the arrows move the cursor, with Shift extending the selection
// and Ctrl jumping; PageUp and PageDown page; Home and End reach the row's
// ends and, with Ctrl, the grid's. Enter and F2 activate the cursor cell
// (`on_activate`), a printable key starts type-ahead (`on_type_ahead`), and
// Delete asks to clear (`on_clear_request`); each of the three is consumed
// only where a handler is installed, so an application that binds them to
// commands of its own is not pre-empted.
// Mouse: a press places the cursor, with Shift extending; a drag extends; a
// double click activates; the wheel scrolls the body.
class CellGrid : public ui::View {
public:
    // A grid with no model, which draws blank; it is a tab stop.
    CellGrid();

    // Draws with these roles instead of the "ckv.cellgrid." ones; the inactive cursor role
    // is still resolved from the theme. An override set before attachment survives it; a
    // change repaints.
    void set_role_override(ui::RoleId normal_role, ui::RoleId header_role, ui::RoleId cursor_role,
                           ui::RoleId selection_role) noexcept {
        if (normal_role_ == normal_role && header_role_ == header_role && cursor_role_ == cursor_role &&
            selection_role_ == selection_role)
            return;
        normal_role_ = normal_role;
        header_role_ = header_role;
        cursor_role_ = cursor_role;
        selection_role_ = selection_role;
        invalidate();
    }

    // Borrows `model`; it must outlive this grid or be cleared first.
    // Setting a model lays the grid out against it at once (set_viewport, then frame) and
    // repaints. clear_model() detaches it, ends any drag and leaves the grid blank;
    // model() is the borrowed provider, or nullptr. Neither fires on_changed.
    void set_model(CellGridModel& model);
    void clear_model();
    CellGridModel* model() const noexcept { return model_; }
    // The provider's state moved on its own — an edit, a command, a scroll
    // the application performed. The grid re-reads the frame and repaints.
    void model_changed();

    // Whether the column header row is drawn. On by default.
    void set_column_header_visible(bool visible);
    bool column_header_visible() const noexcept { return column_header_; }

    // --- geometry, in this view's own cells -----------------------------
    // Rows the column header takes: 1 while it is visible, otherwise 0.
    int header_height() const noexcept { return column_header_ ? 1 : 0; }
    // The gutter as last laid out (the provider's answer, clamped to the
    // width the grid has).
    int gutter_width() const noexcept { return gutter_width_; }
    // The frame as last laid out; what draw() and the hit tests read.
    const GridFrame& frame() const noexcept { return frame_; }
    // The provider position under a local point, when it is a listed cell.
    std::optional<GridPosition> cell_at(Point local) const;
    // The rect a listed cell occupies. A span's anchor answers with its whole
    // painted extent; a cell the frame does not list answers nothing.
    std::optional<Rect> cell_rect(GridPosition at) const;

    // --- what the grid tells its owner ----------------------------------
    // Enter, F2 or a double click (MouseEvent::click_count) on the cursor cell.
    // Enter and F2 count only without Ctrl or Shift. The first press of a double click has
    // already placed the cursor on the cell the second one activates.
    std::function<void()> on_activate;
    // A printable key the grid has no use for: the reader started typing
    // into the cell. The text is the key's own.
    // Not fired while Ctrl, Alt or Super is held.
    std::function<void(const std::string&)> on_type_ahead;
    // Delete, without Ctrl or Shift.
    std::function<void()> on_clear_request;
    // The grid asked the provider to move the cursor, the selection or the
    // body, and repainted. An owner showing the cursor's address elsewhere
    // listens here.
    // It fires after every such request, whether or not the provider actually moved
    // anything, and never for model_changed(), set_model() or a resize.
    std::function<void()> on_changed;

    void on_resized() override;
    void on_attached() override;
    // The cursor's face follows focus, so gaining or losing it repaints.
    void on_focus(const FocusEvent& event) override;
    void draw(scene::Painter& painter) override;
    // Consumes the keys the class comment lists, and nothing without a model, while the
    // frame lists no cell, or with Alt or Super held. Ctrl+PageUp and Ctrl+PageDown pass
    // on to the owner, as do Enter, F2, Delete and printable keys without their handler.
    bool on_key(const KeyEvent& event) override;
    // With a model, consumes a left press on a listed cell, pointer moves and the release
    // during a drag started by a single press, and every wheel event: ui::kWheelRows rows
    // per vertical notch and one column per horizontal one. Presses elsewhere pass on.
    bool on_mouse(const MouseEvent& event) override;

private:
    // Tells the provider the room it has and reads the frame back. The
    // gutter can change width with the rows shown (row 999 to row 1000), and
    // a wider gutter is a narrower body, so the answer is read again until
    // it settles.
    void relayout();
    // The x at which the listed column `index` starts.
    int column_x(std::size_t index) const noexcept;
    const GridRange* span_covering(GridPosition at) const noexcept;
    bool column_listed(GridIndex column) const noexcept;
    Style cell_style(const GridCell& cell, bool is_cursor, bool is_selected) const;
    void after_request();
    bool request(GridMove move, bool extend);

    CellGridModel* model_ = nullptr;
    GridFrame frame_;
    std::vector<int> column_widths_;  // per listed column, as last laid out
    int gutter_width_ = 0;
    bool column_header_ = true;
    bool dragging_ = false;

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId header_role_ = ui::kInvalidRole;
    ui::RoleId cursor_role_ = ui::kInvalidRole;
    ui::RoleId selection_role_ = ui::kInvalidRole;
    ui::RoleId cursor_inactive_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
