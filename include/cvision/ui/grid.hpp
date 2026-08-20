// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Grid: the row/column-span container from D-006's remaining set
// (M10/WP-19). Unlike Row/Column, whose per-child SizePolicy lets each
// child claim a different share of the main axis, Grid v1 deliberately
// uses a UNIFORM sizing model: `rows`/`columns` are fixed at
// construction, and every row shares the container's height evenly
// (likewise every column shares its width evenly) — a per-cell content-
// driven track-sizing pass (the way most grid systems size tracks from
// their widest/tallest cell) is real future scope, not implemented
// here, the same honest "v1 scope" precedent Row/Column's own now-
// retired fill-only cross-axis limitation set (see layout.hpp). A
// child may still SPAN multiple consecutive rows/columns, and is
// positioned within its spanned cell via the same Alignment values
// Row/Column use (Fill/Start/Center/End). Once the child's cell width is
// resolved, vertical Start/Center/End placement uses height_for_width(width)
// for wrapped content — hence no separate alignment or wrapping enum here,
// per layout.hpp's own doc comment.
//
// Grid does not aggregate its children's size hints into its own
// (unlike Row/Column): a grid's own size is normally given explicitly
// (its constructor Rect, or set_preferred_size), the same as
// AnchorPane — a placement container, not a flow container.
#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include "cvision/ui/layout.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::ui {

struct GridSpec {
    int row = 0;
    int column = 0;
    int row_span = 1;
    int column_span = 1;
    Alignment horizontal_alignment = Alignment::Fill;
    Alignment vertical_alignment = Alignment::Fill;
};

// Assigns each of `count` cells along one axis an [offset, extent] pair,
// evenly dividing `available` space minus the `spacing * (count - 1)`
// gaps between cells. Any leftover pixel from the integer division is
// handed to the first cells, one each, so the cells' own extents plus
// the gaps between them exactly fill `available` — the grid analogue
// of distribute_main_axis (layout.hpp), pure and exhaustively testable
// in isolation the same way. `count <= 0` returns an empty vector.
std::vector<std::pair<int, int>> distribute_grid_cells(int available, int count, int spacing);

class Grid : public View {
public:
    Grid(Rect bounds, int rows, int columns);

    // `spec.row`/`spec.column` must be non-negative and, together with
    // `spec.row_span`/`spec.column_span` (each >= 1), must stay within
    // the grid's own fixed row_count()/column_count() — checked here
    // via CKV_ASSERT, not at relayout time.
    View* add_item(std::unique_ptr<View> child, GridSpec spec);
    std::unique_ptr<View> remove_item(View* child);

    int row_count() const noexcept { return row_count_; }
    int column_count() const noexcept { return column_count_; }

    int spacing() const noexcept { return spacing_; }
    void set_spacing(int spacing);

    void on_resized() override { relayout(); }
    // See Row/Column: a child's own hint changing (M9/WP-16) needs the
    // same response as this grid's own bounds changing.
    void on_child_size_hint_changed(View&) override { relayout(); }

private:
    void relayout();

    int row_count_;
    int column_count_;
    int spacing_ = 0;
    std::unordered_map<View*, GridSpec> specs_;
};

}  // namespace ckv::ui
