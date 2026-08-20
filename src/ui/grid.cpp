// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/grid.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"
#include "cvision/ui/layout_metrics.hpp"

namespace ckv::ui {

std::vector<std::pair<int, int>> distribute_grid_cells(int available, int count, int spacing) {
    if (count <= 0) return {};
    std::vector<std::pair<int, int>> cells(static_cast<std::size_t>(count), {0, 0});

    const int spacing_total = spacing * (count - 1);
    const int usable = std::max(0, available - spacing_total);
    const int base = usable / count;
    const int remainder = usable % count;

    int offset = 0;
    for (int i = 0; i < count; ++i) {
        const int extent = base + (i < remainder ? 1 : 0);
        cells[static_cast<std::size_t>(i)] = {offset, extent};
        offset += extent + spacing;
    }
    return cells;
}

namespace {

// The [offset, extent] spanned by `span` consecutive cells starting at
// `index`, derived directly from distribute_grid_cells' own per-cell
// results (extent includes the gaps the span swallows).
std::pair<int, int> span_extent(const std::vector<std::pair<int, int>>& cells, int index,
                                 int span) {
    const auto& first = cells[static_cast<std::size_t>(index)];
    const auto& last = cells[static_cast<std::size_t>(index + span - 1)];
    return {first.first, (last.first + last.second) - first.first};
}

}  // namespace

Grid::Grid(Rect bounds, int rows, int columns)
    : View(bounds), row_count_(rows), column_count_(columns) {
    CKV_ASSERT(rows >= 1);
    CKV_ASSERT(columns >= 1);
}

View* Grid::add_item(std::unique_ptr<View> child, GridSpec spec) {
    CKV_ASSERT(child != nullptr);
    CKV_ASSERT(spec.row >= 0 && spec.column >= 0);
    CKV_ASSERT(spec.row_span >= 1 && spec.column_span >= 1);
    CKV_ASSERT(spec.row + spec.row_span <= row_count_);
    CKV_ASSERT(spec.column + spec.column_span <= column_count_);

    View* observer = add_child(std::move(child));
    specs_[observer] = spec;
    relayout();
    return observer;
}

std::unique_ptr<View> Grid::remove_item(View* child) {
    std::unique_ptr<View> owned = remove_child(child);
    if (owned) {
        specs_.erase(child);
        relayout();
    }
    return owned;
}

void Grid::set_spacing(int spacing) {
    CKV_ASSERT(spacing >= 0);
    if (spacing == spacing_) return;
    spacing_ = spacing;
    relayout();
}

void Grid::relayout() {
    const auto columns = distribute_grid_cells(bounds().width, column_count_, spacing_);
    const auto rows = distribute_grid_cells(bounds().height, row_count_, spacing_);

    for (const auto& child : children()) {
        auto it = specs_.find(child.get());
        if (it == specs_.end()) continue;
        const GridSpec& spec = it->second;

        const auto [cell_x, cell_width] = span_extent(columns, spec.column, spec.column_span);
        const auto [cell_y, cell_height] = span_extent(rows, spec.row, spec.row_span);

        const int preferred_w = child->horizontal_size_hint().preferred;
        const auto [x, width] =
            align_cross_axis(cell_width, preferred_w, spec.horizontal_alignment, 0, 0);
        const int preferred_h = detail::preferred_height_for_width(*child, width);
        const auto [y, height] =
            align_cross_axis(cell_height, preferred_h, spec.vertical_alignment, 0, 0);

        child->set_bounds(Rect{cell_x + x, cell_y + y, width, height});
    }
}

}  // namespace ckv::ui
