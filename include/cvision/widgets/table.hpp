// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Table: columns with headers, resize, sort indicators, cell styling
// hook (plans/widgets.md — Table has NO baseline column in the
// catalog, its entire listed feature set is beyond-baseline; this
// implements that full listed set except virtualized rows).
//
// Scope note (documented, not an oversight): no virtualized rows — the
// whole row set is held and sorted in memory, matching every other
// M6a/M6b list-shaped widget's "no virtualized model in v1" precedent
// (ListView, TreeView). Sort is a single-column stable string compare
// (lexicographic on the cell text) — typed/numeric sort comparators
// are a natural follow-on once a concrete caller needs one. No
// keyboard column navigation or cell-level selection in v1 — only
// row-level cursor movement, matching ListView's own row model; column
// resize is mouse-drag only.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

struct TableColumn {
    std::string title;
    int width = 10;
    int min_width = 3;
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.table.header"/"ckv.list.normal"/
// "ckv.list.selected" (the latter two shared with ListView/TreeView);
// its embedded Scrollbar resolves its own roles independently.
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

    // Each row must have exactly columns().size() cells; a mismatched
    // row is a CKV_ASSERT contract violation (a caller bug, not a
    // recoverable runtime condition).
    void set_rows(std::vector<std::vector<std::string>> rows);
    const std::vector<std::string>& row(std::size_t display_index) const;
    std::size_t row_count() const noexcept { return rows_.size(); }

    // Optional: overrides a cell's style beyond the normal/selected
    // role default. Receives the UNDERLYING row index (not display
    // order, so it stays valid across re-sorts) and the column index.
    void set_cell_style_hook(std::function<Style(std::size_t row, std::size_t column, Style base)> hook);

    int sort_column() const noexcept { return sort_column_; }
    bool sort_ascending() const noexcept { return sort_ascending_; }
    void sort_by(int column, bool ascending);  // column == -1 clears sorting (restores insertion order)

    int cursor_row() const noexcept { return cursor_row_; }  // display index, -1 if empty

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    void rebuild_order();
    void ensure_cursor_visible();
    int column_start_x(std::size_t column) const noexcept;
    int column_at_x(int local_x) const noexcept;  // -1 if past the last column

    std::vector<TableColumn> columns_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<std::size_t> order_;  // display index -> underlying row index
    int sort_column_ = -1;
    bool sort_ascending_ = true;
    int cursor_row_ = -1;

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
