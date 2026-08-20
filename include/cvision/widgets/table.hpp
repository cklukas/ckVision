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

using TableRowId = std::uint64_t;
inline constexpr TableRowId kInvalidTableRowId = 0;

using CellValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

enum class TableCellType {
    Text,
    Boolean,
    Integer,
    Real,
};

struct TableCell {
    CellValue value;
    // An application may supply a display representation independent of the
    // portable value (for example an ISO date or formatted currency).
    std::string display;
    std::optional<Style> style;
    bool editable = true;
};

struct TableCellRef {
    TableRowId row = kInvalidTableRowId;
    std::size_t column = 0;

    friend bool operator==(const TableCellRef&, const TableCellRef&) = default;
};

struct TableEditResult {
    bool accepted = false;
    std::string diagnostic;

    static TableEditResult accept() { return {true, {}}; }
    static TableEditResult reject(std::string message) { return {false, std::move(message)}; }
};

struct TableColumn {
    // Keep the compact title/width/minimum construction order used by the
    // materialized convenience API; metadata follows it.
    std::string title;
    int width = 10;
    int min_width = 3;
    TableCellType type = TableCellType::Text;
    bool editable = false;
};

// A synchronous visible-slice provider. A model may page, cache, or query its
// own storage, but it must provide a reverse identity lookup so a Table can
// preserve state through sorting/filtering/refresh without enumerating rows.
class TableModel {
public:
    virtual ~TableModel() = default;

    virtual std::size_t row_count() const = 0;
    virtual TableRowId row_id_at(std::size_t display_index) const = 0;
    virtual std::optional<std::size_t> index_of(TableRowId row) const = 0;
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
std::string format_cell_value(const CellValue& value);

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.table.header"/"ckv.list.normal"/
// "ckv.list.selected". Its embedded Scrollbar resolves independently.
class Table : public ui::View {
public:
    Table();

    void set_role_override(ui::RoleId header_role, ui::RoleId normal_role, ui::RoleId selected_role) noexcept {
        header_role_ = header_role;
        normal_role_ = normal_role;
        selected_role_ = selected_role;
    }

    void set_columns(std::vector<TableColumn> columns);
    const std::vector<TableColumn>& columns() const noexcept { return columns_; }

    void set_model(TableModel& model);
    void clear_model();
    TableModel* model() const noexcept { return model_; }
    // Resolves retained identity state after the caller changes its model.
    void model_changed();

    // Compact materialized convenience. Each row must have exactly
    // columns().size() cells; a mismatch is a caller contract violation.
    void set_rows(std::vector<std::vector<std::string>> rows);
    const std::vector<std::string>& row(std::size_t display_index) const;
    std::size_t row_count() const;

    // Materialized-only style hook retained for compact static tables. Provider
    // cells instead carry their own optional style.
    void set_cell_style_hook(std::function<Style(std::size_t row, std::size_t column, Style base)> hook);

    int sort_column() const noexcept { return sort_column_; }
    bool sort_ascending() const noexcept { return sort_ascending_; }
    void sort_by(int column, bool ascending);  // column == -1 requests natural provider order

    int cursor_row() const noexcept { return cursor_row_; }
    int cursor_column() const noexcept { return cursor_column_; }
    std::optional<TableCellRef> selected_cell() const noexcept;
    void set_selected_cell(TableCellRef reference);

    bool editing() const noexcept { return editing_; }
    const std::string& edit_text() const noexcept { return edit_text_; }
    const std::string& edit_diagnostic() const noexcept { return edit_diagnostic_; }
    bool begin_edit();
    bool commit_edit();
    void cancel_edit() noexcept;

    std::function<void(TableCellRef)> on_selection_changed;
    std::function<void(TableCellRef, std::optional<std::size_t>, bool)> on_sort_requested;
    std::function<void(TableCellRef, const TableEditResult&)> on_edit_committed;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

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
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
