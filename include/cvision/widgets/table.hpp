// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Table: a provider-backed, typed, editable data view (D-043). A TableModel is
// caller-owned and queried only for visible cells. Row identity, never display
// position, is the durable cursor/selection state. `set_rows` remains a compact
// materialized convenience for static value tables.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

// A row's durable identity, independent of where sorting or filtering puts it. The Table's
// cursor follows the id, not the display position. A model chooses its own ids; the
// materialized set_rows table uses the row's index in the vector given to it, plus one.
using TableRowId = std::uint64_t;
// The id no row has: what a Table holds while nothing is selected. A model must never
// return it for a real row.
inline constexpr TableRowId kInvalidTableRowId = 0;

// The portable value of one cell: empty (std::monostate), a boolean, a 64-bit integer, a
// double or UTF-8 text. format_cell_value gives each its canonical text.
using CellValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

// How a column parses the text of an edit into the CellValue it commits.
enum class TableCellType {
    // The text as typed.
    Text,
    // "true", "false", "1" or "0", letters in any ASCII case.
    Boolean,
    // A base-10 std::int64_t filling the whole text: an optional '-', digits, nothing else.
    Integer,
    // A double in the C locale's notation, filling the whole text.
    Real,
};

// One cell as a TableModel answers it.
struct TableCell {
    // The cell's value. It is what the Table shows when `display` is empty.
    CellValue value;
    // An application may supply a display representation independent of the
    // portable value (for example an ISO date or formatted currency).
    // When non-empty it is what the cell shows and the text an edit starts from.
    std::string display;
    // When set, the style the cell is drawn in, replacing the table's normal style. The
    // cursor is drawn over it as Table describes, never replaced by it.
    std::optional<Style> style;
    // Whether this cell may be edited; its column must allow editing too.
    bool editable = true;
};

// One cell, addressed by its row's identity and its column's index in Table::columns().
struct TableCellRef {
    // The row's id and the column's index.
    TableRowId row = kInvalidTableRowId;
    std::size_t column = 0;

    // Memberwise equality.
    friend bool operator==(const TableCellRef&, const TableCellRef&) = default;
};

// A provider's answer to an edit.
struct TableEditResult {
    // Whether the value was stored. When it was not, `diagnostic` says why; the Table keeps
    // the edit open and exposes the text through edit_diagnostic() without drawing it.
    bool accepted = false;
    std::string diagnostic;

    // An accepted edit, and a refusal carrying the message a reader should see.
    static TableEditResult accept() { return {true, {}}; }
    static TableEditResult reject(std::string message) { return {false, std::move(message)}; }
};

// One column of a Table: its header, its geometry and how it edits.
struct TableColumn {
    // Keep the compact title/width/minimum construction order used by the
    // materialized convenience API; metadata follows it.
    // The header text, clipped to the column's width; the sort column's gets " ^" or " v"
    // appended.
    std::string title;
    // The column's width in cells, not counting the one blank cell that follows every
    // column. Dragging the header's boundary cell resizes it, never below min_width;
    // set_columns applies neither bound.
    int width = 10;
    int min_width = 3;
    // How an edit's text is parsed before it is committed.
    TableCellType type = TableCellType::Text;
    // Whether cells in this column may be edited at all. A provider cell must also be
    // editable; a materialized cell is editable exactly when its column is.
    bool editable = false;
};

// A synchronous visible-slice provider. A model may page, cache, or query its
// own storage, but it must provide a reverse identity lookup so a Table can
// preserve state through sorting/filtering/refresh without enumerating rows.
class TableModel {
public:
    // Providers are destroyed through this interface.
    virtual ~TableModel() = default;

    // The number of rows in display order, after the model's own sorting and filtering.
    virtual std::size_t row_count() const = 0;
    // The id of the row shown at `display_index`, which the Table keeps below row_count().
    // Never kInvalidTableRowId for a real row.
    virtual TableRowId row_id_at(std::size_t display_index) const = 0;
    // The reverse lookup: where `row` is now displayed, or nullopt when the model no longer
    // shows it. The Table asks it to keep its cursor on the same row across changes.
    virtual std::optional<std::size_t> index_of(TableRowId row) const = 0;
    // The content of one cell; the column indexes Table::columns(). Asked for the cells
    // being drawn and for the cell an edit begins on.
    virtual TableCell cell(TableCellRef reference) const = 0;

    // Header activation delegates ordering to the model. A model may update
    // synchronously or later; its owner calls Table::model_changed() after the
    // new order is observable.
    virtual void request_sort(std::optional<std::size_t> column, bool ascending) {
        (void)column;
        (void)ascending;
    }

    // The provider remains the final authority for validation and persistence.
    // The default makes a read-only provider explicit rather than silently
    // mutating a presentation cache.
    virtual TableEditResult commit(TableCellRef reference, const CellValue& value) {
        (void)reference;
        (void)value;
        return TableEditResult::reject("This cell is read-only");
    }
};

// Converts the portable CellValue vocabulary without locale or environment
// state. Display formatting beyond these canonical forms belongs to TableCell.
// Empty for std::monostate, "true" or "false", decimal digits for an integer, a
// round-trip decimal form for a double, and text unchanged.
std::string format_cell_value(const CellValue& value);

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.table.header"/"ckv.list.normal"/
// "ckv.list.selected". Its embedded Scrollbar resolves independently.
// Disabled (D-076), header and cells take "ckv.list.disabled"'s foreground
// on their own surfaces, the cursor cell on "ckv.list.selected.inactive"'s
// background, and no edit in progress is shown.
//
// Layout: the header takes the first row and the data rows the rest; the rightmost column
// of the data rows holds a vertical scrollbar. Columns start at the left edge, each
// followed by one blank cell. Header and cell text is clipped a whole grapheme cluster at a
// time to its column's width, and a cell's text also stops short of the scrollbar. The
// cursor is a single cell, highlighted with "ckv.list.selected" while the table has the
// focus and "ckv.list.selected.inactive" otherwise; a cell being edited is drawn in
// "ckv.input.focused" with a caret after its text. A cell with a style of its own (a
// provider cell's `style`, or what the style hook returns) keeps it under the cursor as
// CellGrid's cells do (D-067): a cell with a colour swaps its two colours, or takes the
// highlight's where the swapped pair would not read, and the cursor cell is then bold and
// underlined while the table has the focus (underlined only otherwise); a cell that sets no
// colour wears the highlight's colours with its own attributes.
//
// Keyboard: the arrows, PageUp, PageDown, Home and End (first and last row) move the
// cursor; Enter or F2 begins an edit, and a printable key without Alt, Ctrl or Super begins
// one that replaces the cell's text. While editing, Enter commits, Escape cancels,
// Backspace deletes the last grapheme and Delete clears the text. Mouse, primary button
// only: a press on a cell moves the cursor there, a press on a column title sorts by it
// (again to reverse), and a drag from the blank cell after a title resizes that column.
class Table : public ui::View {
public:
    // An empty table with no columns and no rows; it is a tab stop.
    Table();
    // Rules occupy existing inter-column gaps, preserving all cell geometry.
    void set_column_dividers(bool divided) noexcept;
    bool column_dividers() const noexcept { return column_dividers_; }

    // Optional alternating neutral surfaces; explicit styles and selection win.
    void set_banded_rows(bool banded) noexcept {
        if (banded_rows_ == banded) return;
        banded_rows_ = banded;
        invalidate();
    }
    bool banded_rows() const noexcept { return banded_rows_; }


    // Draws with these roles instead of "ckv.table.header", "ckv.list.normal" and
    // "ckv.list.selected"; the inactive, disabled and editing roles are still resolved from
    // the theme. An override set before attachment survives it; a change repaints.
    void set_role_override(ui::RoleId header_role, ui::RoleId normal_role, ui::RoleId selected_role) noexcept {
        if (header_role_ == header_role && normal_role_ == normal_role && selected_role_ == selected_role) return;
        header_role_ = header_role;
        normal_role_ = normal_role;
        selected_role_ = selected_role;
        invalidate();
    }

    // The columns, left to right; a TableCellRef's column indexes this list. While set_rows
    // content is shown, the new list must have exactly as many columns as each row has
    // cells, as set_rows requires; a mismatch is a caller contract violation (asserted). To
    // change the column count, clear the rows first (set_rows({})) or use a model. Setting
    // them cancels any edit and pulls the cursor column back inside the new list, but keeps the
    // rows and the sort as they are -- unless the sort column is past the new list's end,
    // when the sort is dropped as sort_by(-1) drops it. columns() reflects widths changed by
    // dragging.
    void set_columns(std::vector<TableColumn> columns);
    const std::vector<TableColumn>& columns() const noexcept { return columns_; }

    // Shows `model`, which the table borrows: it must outlive the table or be cleared
    // first. Any set_rows content is discarded, an edit is cancelled and the cursor moves to
    // the first row's first column, without firing on_selection_changed. clear_model()
    // leaves the table empty in the same way; model() is the borrowed provider, or nullptr
    // for a materialized or empty table.
    void set_model(TableModel& model);
    void clear_model();
    TableModel* model() const noexcept { return model_; }
    // Resolves retained identity state after the caller changes its model.
    // The cursor stays on its row id wherever that row now is, and is cleared (not moved to
    // another row) when the model no longer shows it, which also cancels an edit. The
    // scrollbar is updated and the cursor scrolled into view. on_selection_changed does not
    // fire.
    void model_changed();

    // Compact materialized convenience. Each row must have exactly
    // columns().size() cells; a mismatch is a caller contract violation (asserted), and
    // set_columns holds the rows to the same rule.
    // Setting rows detaches any model, re-applies the current sort and puts the cursor on
    // the first displayed row's first column, without firing on_selection_changed. Each
    // value is text: it is shown as given and sorted as a string, whatever the column's type.
    void set_rows(std::vector<std::vector<std::string>> rows);
    // The materialized row shown at `display_index`, after sorting. Only for a table filled
    // by set_rows (asserted), with the index below row_count() (asserted). The reference
    // lasts until the rows change.
    const std::vector<std::string>& row(std::size_t display_index) const;
    // The number of rows, the model's or the materialized ones.
    std::size_t row_count() const;

    // Materialized-only style hook retained for compact static tables. Provider
    // cells instead carry their own optional style.
    // Called while drawing with the row's index in the vector given to set_rows (not its
    // display position), the column index and the table's normal style; it returns the
    // cell's own style. A returned style that differs from the one given styles the cell,
    // and the cursor is drawn over it as the class comment describes. Setting it repaints.
    void set_cell_style_hook(std::function<Style(std::size_t row, std::size_t column, Style base)> hook);

    // The column the table is sorted by, or -1 for the natural order, and its direction.
    // They record the last sort_by call, including one a model has yet to carry out.
    int sort_column() const noexcept { return sort_column_; }
    bool sort_ascending() const noexcept { return sort_ascending_; }
    // `column` must be -1 or index a column (asserted). A materialized table re-sorts at
    // once, stably and by plain string comparison, and its cursor follows its row. A model
    // table passes the request to TableModel::request_sort and then fires on_sort_requested;
    // its owner calls model_changed() once the new order holds.
    void sort_by(int column, bool ascending);  // column == -1 requests natural provider order

    // The cursor's display row, or -1 when there is none, and its column index.
    int cursor_row() const noexcept { return cursor_row_; }
    int cursor_column() const noexcept { return cursor_column_; }
    // The cursor as a row id and column, or nullopt when no row is selected or the column
    // is outside columns().
    std::optional<TableCellRef> selected_cell() const noexcept;
    // Moves the cursor to `reference` and scrolls it into view; a row the table does not
    // show or a column past the end is ignored. When the cell changes, an edit is cancelled
    // and on_selection_changed fires, even though the move was programmatic.
    void set_selected_cell(TableCellRef reference);

    // Whether the cursor cell is being edited, the text typed so far, and why the last
    // commit failed (empty when it did not, and cleared by further typing). edit_text() is
    // meaningful only while editing() is true.
    bool editing() const noexcept { return editing_; }
    const std::string& edit_text() const noexcept { return edit_text_; }
    const std::string& edit_diagnostic() const noexcept { return edit_diagnostic_; }
    // Opens an edit on the cursor cell, starting from the text it shows. Returns false,
    // changing nothing, when no cell is selected or the column or the cell is not editable.
    bool begin_edit();
    // Parses the edit text by the column's TableCellType and commits the value: to the
    // model's commit(), or into the materialized row as format_cell_value text, re-sorting
    // if sorted. A parse failure sets edit_diagnostic() and reaches neither the provider nor
    // on_edit_committed. Otherwise on_edit_committed fires with the result; a refusal keeps
    // the edit open with the provider's diagnostic. Returns true only when the value was
    // accepted, which closes the edit and re-finds the cursor's row.
    bool commit_edit();
    // Abandons an open edit, clears its text and diagnostic and repaints the cell. Does
    // nothing while no edit is open.
    void cancel_edit() noexcept;

    // Fired with the new cursor cell when the cursor moves to another cell by key, click or
    // set_selected_cell. Never fired when the table resets or re-finds the cursor itself:
    // set_model, set_rows, model_changed, sorting or a commit.
    std::function<void(TableCellRef)> on_selection_changed;
    // Fired by every sort_by on a model table, after request_sort: the selected cell
    // (nullopt when none is), the requested column (nullopt for natural order) and the
    // direction.
    std::function<void(std::optional<TableCellRef>, std::optional<std::size_t>, bool)> on_sort_requested;
    // Fired after the provider (or the materialized table) has answered a commit, with the
    // edited cell and the answer, accepted or not.
    std::function<void(TableCellRef, const TableEditResult&)> on_edit_committed;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    // While editing, consumes Escape, Backspace, Delete, printable keys and Enter, whether
    // or not its commit succeeds; every other key passes on, the cursor staying put. Otherwise
    // consumes the navigation keys while there are rows and columns, and Enter, F2 or a
    // printable key only when they begin an edit.
    bool on_key(const KeyEvent& event) override;
    // Appends the text to the edit and consumes it only while editing.
    bool on_text(const TextEvent& event) override;
    // Consumes a primary-button press on a column title, a boundary cell or a data cell, the
    // pointer moves and the release that follow a press on a boundary cell, and the vertical
    // wheel, which scrolls the body ui::kWheelRows rows per notch and leaves the cursor cell.
    // A press of another button and a horizontal wheel are not handled here.
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;
    // The cursor's highlight follows focus, so gaining or losing it repaints.
    void on_focus(const FocusEvent& event) override;
    // The caret at the end of a cell being edited.
    std::optional<CursorState> cursor_state() const override;

private:
    std::size_t model_row_count() const;
    TableRowId row_id_at(std::size_t display_index) const;
    std::optional<std::size_t> index_of(TableRowId row) const;
    TableCell cell_at(TableCellRef reference) const;
    void rebuild_order();
    void resolve_model_identity(bool select_first_when_unset);
    void move_cursor(int row, int column, bool notify = true);
    void ensure_cursor_visible();
    int column_start_x(std::size_t column) const noexcept;
    int column_at_x(int local_x) const noexcept;
    // The columns the data rows may use: the whole width, less the column
    // the scrollbar covers while it shows. The bar paints over that column
    // after the rows, so a cell drawn into it would lose whatever the bar
    // covers — the second half of a wide glyph, say.
    int data_columns() const noexcept;
    // The width of `column`'s cells on screen: its own width, cut short
    // where the data columns end.
    int shown_cell_width(std::size_t column) const noexcept;
    bool parse_edit_value(CellValue& out, std::string& diagnostic) const;
    TableEditResult commit_value(TableCellRef reference, const CellValue& value);

    std::vector<TableColumn> columns_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<std::size_t> order_;  // display index -> materialized backing index
    TableModel* model_ = nullptr;
    int sort_column_ = -1;
    bool sort_ascending_ = true;
    TableRowId cursor_row_id_ = kInvalidTableRowId;
    int cursor_row_ = -1;
    int cursor_column_ = 0;

    bool editing_ = false;
    std::string edit_text_;
    std::string edit_diagnostic_;

    int resizing_column_ = -1;
    int resize_start_x_ = 0;
    int resize_start_width_ = 0;

    Scrollbar* scrollbar_ = nullptr;
    std::function<Style(std::size_t, std::size_t, Style)> cell_style_hook_;

    ui::RoleId header_role_ = ui::kInvalidRole;
    bool column_dividers_ = false;
    ui::RoleId divider_role_ = ui::kInvalidRole;
    bool banded_rows_ = false;
    ui::RoleId banded_role_ = ui::kInvalidRole;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId selected_inactive_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId editing_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
