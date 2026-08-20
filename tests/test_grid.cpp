// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/grid.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::Rect;
using ckv::ui::Alignment;
using ckv::ui::distribute_grid_cells;
using ckv::ui::Grid;
using ckv::ui::GridSpec;
using ckv::ui::View;

// --- distribute_grid_cells: pure function -------------------------------

CK_TEST(zero_or_negative_count_produces_an_empty_result) {
    CK_CHECK(distribute_grid_cells(100, 0, 0).empty());
    CK_CHECK(distribute_grid_cells(100, -1, 0).empty());
}

CK_TEST(available_space_that_divides_evenly_gives_every_cell_the_same_extent) {
    const auto cells = distribute_grid_cells(30, 3, 0);
    CK_CHECK(cells.size() == 3);
    CK_CHECK(cells[0] == (std::pair{0, 10}));
    CK_CHECK(cells[1] == (std::pair{10, 10}));
    CK_CHECK(cells[2] == (std::pair{20, 10}));
}

CK_TEST(a_remainder_from_uneven_division_is_handed_one_pixel_each_to_the_first_cells) {
    const auto cells = distribute_grid_cells(10, 3, 0);  // 10/3 = 3 remainder 1
    CK_CHECK(cells[0] == (std::pair{0, 4}));
    CK_CHECK(cells[1] == (std::pair{4, 3}));
    CK_CHECK(cells[2] == (std::pair{7, 3}));
}

CK_TEST(spacing_is_subtracted_before_dividing_and_inserted_as_gaps_between_offsets) {
    const auto cells = distribute_grid_cells(26, 3, 2);  // gaps: 2*2=4, usable 22, 22/3=7 r1
    CK_CHECK(cells[0] == (std::pair{0, 8}));
    CK_CHECK(cells[1] == (std::pair{10, 7}));  // 8 + spacing(2)
    CK_CHECK(cells[2] == (std::pair{19, 7}));  // 10 + 7 + spacing(2)
}

CK_TEST(available_space_too_small_for_the_gaps_clamps_every_cell_to_zero) {
    const auto cells = distribute_grid_cells(1, 3, 5);  // spacing alone already exceeds available
    CK_CHECK(cells[0].second == 0);
    CK_CHECK(cells[1].second == 0);
    CK_CHECK(cells[2].second == 0);
}

// --- Grid ----------------------------------------------------------------

namespace {
class HintedView : public View {
public:
    HintedView(int w, int h) : w_(w), h_(h) {}
    ckv::ui::SizeHint horizontal_size_hint() const override { return {w_, w_, w_}; }
    ckv::ui::SizeHint vertical_size_hint() const override { return {h_, h_, h_}; }

private:
    int w_;
    int h_;
};

class HeightForWidthView : public View {
public:
    ckv::ui::SizeHint horizontal_size_hint() const override {
        return {0, 10, ckv::ui::kUnboundedExtent};
    }
    ckv::ui::SizeHint vertical_size_hint() const override {
        return {1, 1, ckv::ui::kUnboundedExtent};
    }
    int height_for_width(int width) const override { return width <= 5 ? 4 : 2; }
};
}  // namespace

CK_TEST(a_single_cell_child_defaults_to_filling_its_whole_cell) {
    Grid grid(Rect{0, 0, 30, 20}, 2, 3);
    auto* child = grid.add_item(std::make_unique<View>(), GridSpec{.row = 0, .column = 1});
    CK_CHECK(child->bounds() == (Rect{10, 0, 10, 10}));  // column 1 of 3, row 0 of 2
}

CK_TEST(a_column_span_covers_multiple_cells_plus_the_gap_between_them) {
    Grid grid(Rect{0, 0, 30, 20}, 1, 3);
    grid.set_spacing(1);  // usable width 28, cells 10/9/9
    auto* child =
        grid.add_item(std::make_unique<View>(), GridSpec{.row = 0, .column = 0, .column_span = 2});
    CK_CHECK(child->bounds() == (Rect{0, 0, 20, 20}));  // 10 + spacing(1) + 9
}

CK_TEST(a_row_span_covers_multiple_cells_plus_the_gap_between_them) {
    Grid grid(Rect{0, 0, 10, 30}, 3, 1);
    grid.set_spacing(1);  // usable height 28, cells 10/9/9
    auto* child =
        grid.add_item(std::make_unique<View>(), GridSpec{.row = 0, .column = 0, .row_span = 2});
    CK_CHECK(child->bounds() == (Rect{0, 0, 10, 20}));  // 10 + spacing(1) + 9
}

CK_TEST(a_start_aligned_child_sits_at_its_cells_near_edge_sized_to_its_own_preferred_hint) {
    Grid grid(Rect{0, 0, 30, 20}, 1, 3);
    const GridSpec spec{.row = 0, .column = 1, .horizontal_alignment = Alignment::Start};
    auto* child = grid.add_item(std::make_unique<HintedView>(4, 6), spec);
    CK_CHECK(child->bounds() == (Rect{10, 0, 4, 20}));  // column 1 starts at x=10
}

CK_TEST(an_end_aligned_child_sits_flush_against_its_cells_far_edge) {
    Grid grid(Rect{0, 0, 30, 20}, 1, 3);
    const GridSpec spec{.row = 0, .column = 1, .horizontal_alignment = Alignment::End};
    auto* child = grid.add_item(std::make_unique<HintedView>(4, 6), spec);
    CK_CHECK(child->bounds() == (Rect{16, 0, 4, 20}));  // 10 + (10 - 4)
}

CK_TEST(a_center_aligned_child_is_centered_on_both_axes_within_its_cell) {
    Grid grid(Rect{0, 0, 30, 20}, 1, 3);
    const GridSpec spec{.row = 0,
                         .column = 1,
                         .horizontal_alignment = Alignment::Center,
                         .vertical_alignment = Alignment::Center};
    auto* child = grid.add_item(std::make_unique<HintedView>(4, 6), spec);
    CK_CHECK(child->bounds() == (Rect{13, 7, 4, 6}));  // 10 + (10-4)/2, 0 + (20-6)/2
}

CK_TEST(a_grid_cell_uses_child_height_for_its_resolved_width) {
    Grid grid(Rect{0, 0, 5, 10}, 1, 1);
    const GridSpec spec{.row = 0, .column = 0, .vertical_alignment = Alignment::Start};
    auto* child = grid.add_item(std::make_unique<HeightForWidthView>(), spec);
    CK_CHECK(child->bounds() == (Rect{0, 0, 5, 4}));

    grid.set_bounds(Rect{0, 0, 10, 10});

    CK_CHECK(child->bounds() == (Rect{0, 0, 10, 2}));
}

CK_TEST(remove_item_returns_ownership_and_stops_tracking_its_spec) {
    Grid grid(Rect{0, 0, 30, 20}, 1, 3);
    View* raw = grid.add_item(std::make_unique<View>(), GridSpec{.row = 0, .column = 0});

    auto owned = grid.remove_item(raw);
    CK_CHECK(owned != nullptr);
    CK_CHECK(owned.get() == raw);

    grid.set_bounds(Rect{0, 0, 60, 40});  // must not crash touching a stale spec
    CK_CHECK(grid.children().empty());
}

CK_TEST(remove_item_for_a_view_not_owned_by_this_grid_returns_null) {
    Grid grid(Rect{0, 0, 30, 20}, 1, 3);
    View stray;
    CK_CHECK(grid.remove_item(&stray) == nullptr);
}

CK_TEST(resizing_the_grid_relayouts_every_cell_to_the_new_uniform_extents) {
    Grid grid(Rect{0, 0, 30, 20}, 1, 3);
    auto* child = grid.add_item(std::make_unique<View>(), GridSpec{.row = 0, .column = 2});
    CK_CHECK(child->bounds() == (Rect{20, 0, 10, 20}));

    grid.set_bounds(Rect{0, 0, 60, 20});

    CK_CHECK(child->bounds() == (Rect{40, 0, 20, 20}));
}

namespace {
class MutableWidthView : public View {
public:
    explicit MutableWidthView(int width) : width_(width) {}
    void set_width(int width) {
        width_ = width;
        size_hint_changed();
    }
    ckv::ui::SizeHint horizontal_size_hint() const override { return {width_, width_, width_}; }

private:
    int width_;
};
}  // namespace

CK_TEST(a_grid_relayouts_when_a_child_grows_its_own_hint_without_a_grid_resize) {
    Grid grid(Rect{0, 0, 30, 20}, 1, 3);
    const GridSpec spec{.row = 0, .column = 1, .horizontal_alignment = Alignment::Start};
    auto* child = static_cast<MutableWidthView*>(
        grid.add_item(std::make_unique<MutableWidthView>(2), spec));
    CK_CHECK(child->bounds().width == 2);

    child->set_width(6);  // no grid resize — only the child's own hint changed

    CK_CHECK(child->bounds().width == 6);
}

CK_TEST(a_span_reaching_past_the_grids_own_row_or_column_count_aborts) {
    CK_EXPECT_ABORT({
        Grid grid(Rect{0, 0, 30, 20}, 2, 2);
        grid.add_item(std::make_unique<View>(), GridSpec{.row = 0, .column = 1, .column_span = 2});
    });
}
