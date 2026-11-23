// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/grid.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"
#include "ui/layout_metrics.hpp"

namespace ckv::ui {

std::vector<std::pair<int, int>> distribute_grid_tracks(int available, int count, int spacing,
                                                        const std::vector<TrackSpec>& specs) {
    if (count <= 0) return {};
    std::vector<std::pair<int, int>> cells(static_cast<std::size_t>(count), {0, 0});
    const int spacing_total = spacing * (count - 1);
    int usable = std::max(0, available - spacing_total);

    int fixed_total = 0;
    int weight_total = 0;
    for (int i = 0; i < count; ++i) {
        const TrackSpec spec = (static_cast<std::size_t>(i) < specs.size())
                                   ? specs[static_cast<std::size_t>(i)]
                                   : TrackSpec{};
        if (spec.kind == TrackKind::Fixed || spec.kind == TrackKind::Auto) {
            fixed_total += std::max(0, spec.value);
        } else {
            weight_total += std::max(1, spec.value);
        }
    }

    const int flex_available = std::max(0, usable - fixed_total);
    int flex_assigned = 0;
    int last_flex = -1;
    std::vector<int> extents(static_cast<std::size_t>(count), 0);
    for (int i = 0; i < count; ++i) {
        const TrackSpec spec = (static_cast<std::size_t>(i) < specs.size())
                                   ? specs[static_cast<std::size_t>(i)]
                                   : TrackSpec{};
        if (spec.kind == TrackKind::Fixed || spec.kind == TrackKind::Auto) {
            const int take = std::min(std::max(0, spec.value), usable);
            extents[static_cast<std::size_t>(i)] = take;
            usable -= take;
        } else if (weight_total > 0) {
            const int w = std::max(1, spec.value);
            const int share = (flex_available * w) / weight_total;
            extents[static_cast<std::size_t>(i)] = share;
            flex_assigned += share;
            last_flex = i;
        }
    }
    if (last_flex >= 0) {
        int rem = flex_available - flex_assigned;
        for (int i = 0; i < count && rem > 0; ++i) {
            const TrackSpec spec = (static_cast<std::size_t>(i) < specs.size())
                                       ? specs[static_cast<std::size_t>(i)]
                                       : TrackSpec{};
            if (spec.kind == TrackKind::Weight) {
                ++extents[static_cast<std::size_t>(i)];
                --rem;
            }
        }
    }

    int offset = 0;
    for (int i = 0; i < count; ++i) {
        cells[static_cast<std::size_t>(i)] = {offset, extents[static_cast<std::size_t>(i)]};
        offset += extents[static_cast<std::size_t>(i)] + spacing;
    }
    return cells;
}

std::vector<std::pair<int, int>> distribute_grid_cells(int available, int count, int spacing) {
    return distribute_grid_tracks(available, count, spacing, {});
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
    // An attachment callback that detached the child, or destroyed this
    // container, leaves nothing to place and no record to keep.
    if (observer == nullptr) return nullptr;
    specs_[observer] = spec;
    relayout();
    size_hint_changed();
    return observer;
}

std::unique_ptr<View> Grid::remove_child(View* child) {
    std::unique_ptr<View> owned = View::remove_child(child);
    if (owned == nullptr) return nullptr;
    specs_.erase(child);
    relayout();
    size_hint_changed();
    return owned;
}

void Grid::set_row_specs(std::vector<TrackSpec> specs) {
    row_specs_ = std::move(specs);
    relayout();
    size_hint_changed();
}

void Grid::set_column_specs(std::vector<TrackSpec> specs) {
    column_specs_ = std::move(specs);
    relayout();
    size_hint_changed();
}

void Grid::set_spacing(int spacing) {
    CKV_ASSERT(spacing >= 0);
    if (spacing == spacing_) return;
    spacing_ = spacing;
    relayout();
    size_hint_changed();
}

void Grid::set_padding(int horizontal, int vertical) {
    CKV_ASSERT(horizontal >= 0 && vertical >= 0);
    if (horizontal == padding_horizontal_ && vertical == padding_vertical_) return;
    padding_horizontal_ = horizontal;
    padding_vertical_ = vertical;
    relayout();
    size_hint_changed();
}

SizeHint Grid::horizontal_size_hint() const {
    if (preferred_size().width > 0) return View::horizontal_size_hint();
    std::vector<int> col_widths(static_cast<std::size_t>(column_count_), 0);
    for (int c = 0; c < column_count_; ++c) {
        if (static_cast<std::size_t>(c) < column_specs_.size() &&
            column_specs_[static_cast<std::size_t>(c)].kind == TrackKind::Fixed) {
            col_widths[static_cast<std::size_t>(c)] = std::max(0, column_specs_[static_cast<std::size_t>(c)].value);
        }
    }
    for (const auto& child : children()) {
        if (!child->visible()) continue;
        auto it = specs_.find(child.get());
        if (it == specs_.end()) continue;
        if (it->second.column_span == 1 && it->second.column >= 0 && it->second.column < column_count_) {
            const std::size_t c = static_cast<std::size_t>(it->second.column);
            if (c >= column_specs_.size() || column_specs_[c].kind != TrackKind::Fixed) {
                col_widths[c] = std::max(col_widths[c], child->horizontal_size_hint().preferred);
            }
        }
    }
    int total = 2 * padding_horizontal_ + spacing_ * std::max(0, column_count_ - 1);
    for (int w : col_widths) total += w;
    return SizeHint{total, total, kUnboundedExtent};
}

SizeHint Grid::vertical_size_hint() const {
    if (preferred_size().height > 0) return View::vertical_size_hint();
    std::vector<int> row_heights(static_cast<std::size_t>(row_count_), 0);
    for (int r = 0; r < row_count_; ++r) {
        if (static_cast<std::size_t>(r) < row_specs_.size() &&
            row_specs_[static_cast<std::size_t>(r)].kind == TrackKind::Fixed) {
            row_heights[static_cast<std::size_t>(r)] = std::max(0, row_specs_[static_cast<std::size_t>(r)].value);
        }
    }
    for (const auto& child : children()) {
        if (!child->visible()) continue;
        auto it = specs_.find(child.get());
        if (it == specs_.end()) continue;
        if (it->second.row_span == 1 && it->second.row >= 0 && it->second.row < row_count_) {
            const std::size_t r = static_cast<std::size_t>(it->second.row);
            if (r >= row_specs_.size() || row_specs_[r].kind != TrackKind::Fixed) {
                row_heights[r] = std::max(row_heights[r], child->vertical_size_hint().preferred);
            }
        }
    }
    int total = 2 * padding_vertical_ + spacing_ * std::max(0, row_count_ - 1);
    for (int h : row_heights) total += h;
    return SizeHint{total, total, kUnboundedExtent};
}

void Grid::relayout() {
    std::vector<TrackSpec> effective_cols = column_specs_;
    if (static_cast<int>(effective_cols.size()) < column_count_)
        effective_cols.resize(static_cast<std::size_t>(column_count_), TrackSpec{});
    std::vector<TrackSpec> effective_rows = row_specs_;
    if (static_cast<int>(effective_rows.size()) < row_count_)
        effective_rows.resize(static_cast<std::size_t>(row_count_), TrackSpec{});

    for (const auto& child : children()) {
        if (!child->visible()) continue;
        auto it = specs_.find(child.get());
        if (it == specs_.end()) continue;
        const GridSpec& gs = it->second;
        if (gs.column_span == 1 && gs.column >= 0 && gs.column < column_count_) {
            TrackSpec& spec = effective_cols[static_cast<std::size_t>(gs.column)];
            if (spec.kind == TrackKind::Auto) {
                spec.value = std::max(spec.value, child->horizontal_size_hint().preferred);
            }
        }
        if (gs.row_span == 1 && gs.row >= 0 && gs.row < row_count_) {
            TrackSpec& spec = effective_rows[static_cast<std::size_t>(gs.row)];
            if (spec.kind == TrackKind::Auto) {
                spec.value = std::max(spec.value, child->vertical_size_hint().preferred);
            }
        }
    }

    const int avail_w = std::max(0, bounds().width - 2 * padding_horizontal_);
    const int avail_h = std::max(0, bounds().height - 2 * padding_vertical_);
    const auto columns = distribute_grid_tracks(avail_w, column_count_, spacing_, effective_cols);
    const auto rows = distribute_grid_tracks(avail_h, row_count_, spacing_, effective_rows);
    const bool rtl = effective_layout_direction() == LayoutDirection::Rtl;

    std::vector<View*> snapshot;
    snapshot.reserve(children().size());
    for (const auto& child : children()) snapshot.push_back(child.get());

    for (View* child : snapshot) {
        auto it = specs_.find(child);
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

        const int ltr_x = padding_horizontal_ + cell_x + x;
        const int final_x = rtl ? std::max(0, bounds().width - ltr_x - width) : ltr_x;
        child->set_bounds(Rect{final_x, padding_vertical_ + cell_y + y, width, height});
    }
}

}  // namespace ckv::ui
