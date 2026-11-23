// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Grid: row/column-span layout container (D-006, D-160).
// Supports TrackSpec (Weight, Fixed, Auto) track sizing, outer padding,
// cell spans, Alignment (Fill/Start/Center/End), LayoutDirection::Rtl,
// and intrinsic size-hint aggregation.
#pragma once

#include "cvision/core/export.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

#include "cvision/ui/layout.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::ui {

// Where one Grid child sits and how it fills its cell, given to Grid::add_item().
struct CVISION_API GridSpec {
    // The zero-based top-left cell and how many rows and columns the child spans (at least 1
    // each). The spanned area includes the spacing gaps between its cells. Overlapping spans are
    // not rejected; later children simply paint over earlier ones.
    int row = 0;
    int column = 0;
    int row_span = 1;
    int column_span = 1;
    // Placement inside the spanned area on each axis. Fill takes the whole extent; the others
    // size the child to its preferred width, and to its height at that width.
    Alignment horizontal_alignment = Alignment::Fill;
    Alignment vertical_alignment = Alignment::Fill;
};

// Assigns each of `count` cells along one axis an [offset, extent] pair,
// evenly dividing `available` space minus the `spacing * (count - 1)`
// gaps between cells. Any leftover cell from the integer division is
// handed to the first cells, one each, so the cells' own extents plus
// the gaps between them exactly fill `available` — the grid analogue
// of distribute_main_axis (layout.hpp), pure and exhaustively testable
// in isolation the same way. `count <= 0` returns an empty vector.
// Sizing rule for a single Grid row or column track.
enum class TrackKind : std::uint8_t {
    Weight,
    Fixed,
    Auto,
};

// Track sizing descriptor: proportional weight (>= 1), fixed cell count (>= 0), or Auto (from child hints).
struct CVISION_API TrackSpec {
    // Sizing policy for this track (Weight, Fixed, or Auto).
    TrackKind kind = TrackKind::Weight;
    // Weight (>= 1) or fixed cell count (>= 0). Ignored for Auto.
    int value = 1;
};

// Divides `available` cells into `count` consecutive tracks respecting `specs` and `spacing`.
CVISION_API std::vector<std::pair<int, int>> distribute_grid_tracks(int available, int count, int spacing,
                                                        const std::vector<TrackSpec>& specs);

// `count <= 0` returns an empty vector.
CVISION_API std::vector<std::pair<int, int>> distribute_grid_cells(int available, int count, int spacing);

// A container of `rows` x `columns` tracks that places each child added with add_item() by
// its GridSpec, on every resize, spacing/padding/track change, item change, and child size-hint change.
// Children added with add_child() instead are not placed. Every child's cell is fixed by its
// spec alone, so a hidden child takes nothing from its siblings and keeps its cell.
class CVISION_API Grid : public View {
public:
    // `bounds` is parent-local; `rows` and `columns` must each be at least 1 (asserted) and are
    // fixed for the grid's lifetime.
    Grid(Rect bounds, int rows, int columns);

    // `spec.row`/`spec.column` must be non-negative and, together with
    // `spec.row_span`/`spec.column_span` (each >= 1), must stay within
    // the grid's own fixed row_count()/column_count() — checked here
    // via CKV_ASSERT, not at relayout time.
    View* add_item(std::unique_ptr<View> child, GridSpec spec);
    // Typed convenience overload returning the concrete child pointer (or nullptr if detached).
    template <typename T>
        requires std::derived_from<T, View>
    T* add_item(std::unique_ptr<T> child, GridSpec spec) {
        return static_cast<T*>(add_item(std::unique_ptr<View>(std::move(child)), spec));
    }
    // Construct a child of type T in place and attach it at `spec`.
    template <typename T, typename... Args>
        requires std::derived_from<T, View>
    T* make_item(GridSpec spec, Args&&... args) {
        return add_item(std::make_unique<T>(std::forward<Args>(args)...), spec);
    }
    // Detaches `child` exactly as View::remove_child() does, then forgets its spec, relays out
    // and returns ownership; nullptr (and no relayout) when `child` is not a child of this grid.
    // Every way a child leaves comes through here, so a view added back with add_child() is
    // not placed by the spec it had before. add_item() keeps no spec for a child its
    // attachment callback detached, and returns nullptr.
    std::unique_ptr<View> remove_child(View* child) override;

    // The fixed track counts given to the constructor.
    int row_count() const noexcept { return row_count_; }
    int column_count() const noexcept { return column_count_; }

    // Configure per-row or per-column TrackSpecs (defaults to uniform Weight{1}).
    void set_row_specs(std::vector<TrackSpec> specs);
    // Configure per-column TrackSpecs (defaults to uniform Weight{1}).
    void set_column_specs(std::vector<TrackSpec> specs);

    // The gap between adjacent rows and between adjacent columns, in cells (0 by default).
    // set_spacing() asserts `spacing` >= 0 and relays out when it changes.
    int spacing() const noexcept { return spacing_; }
    void set_spacing(int spacing);
    // Outer padding inside the container (0, 0 by default).
    void set_padding(int horizontal, int vertical);
    int horizontal_padding() const noexcept { return padding_horizontal_; }
    int vertical_padding() const noexcept { return padding_vertical_; }

    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    void on_resized() override { relayout(); }
    void on_layout_direction_changed() override { relayout(); }
    // See Row/Column: a child's own hint changing (M9/WP-16) needs the
    // same response as this grid's own bounds changing.
    void on_child_size_hint_changed(View&) override {
        relayout();
        size_hint_changed();
    }

protected:
    using View::add_child;
    using View::add;
    using View::make;

private:
    void relayout();

    int row_count_;
    int column_count_;
    int spacing_ = 0;
    int padding_horizontal_ = 0;
    int padding_vertical_ = 0;
    std::vector<TrackSpec> row_specs_;
    std::vector<TrackSpec> column_specs_;
    std::unordered_map<View*, GridSpec> specs_;
};

}  // namespace ckv::ui
