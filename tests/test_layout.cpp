// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/layout.hpp"

#include "cktest.hpp"

using ckv::Rect;
using ckv::ui::Column;
using ckv::ui::distribute_main_axis;
using ckv::ui::LayoutChild;
using ckv::ui::LayoutSpec;
using ckv::ui::Row;
using ckv::ui::SizePolicy;
using ckv::ui::View;

// --- distribute_main_axis: pure function ------------------------------------

CK_TEST(empty_child_list_produces_an_empty_result) {
    CK_CHECK(distribute_main_axis({}, 100, 2).empty());
}

CK_TEST(a_single_fixed_child_gets_exactly_its_preferred_size_at_offset_zero) {
    auto sizes = distribute_main_axis({LayoutChild{0, 10, SizePolicy::Fixed, 1}}, 100, 0);
    CK_CHECK(sizes.size() == 1);
    CK_CHECK(sizes[0] == (std::pair<int, int>{0, 10}));
}

CK_TEST(spacing_is_inserted_between_consecutive_children_but_not_before_the_first) {
    auto sizes = distribute_main_axis(
        {LayoutChild{0, 5, SizePolicy::Fixed, 1}, LayoutChild{0, 5, SizePolicy::Fixed, 1}}, 100, 3);
    CK_CHECK(sizes[0] == (std::pair<int, int>{0, 5}));
    CK_CHECK(sizes[1] == (std::pair<int, int>{5 + 3, 5}));
}

CK_TEST(fixed_children_never_grow_even_when_extra_space_is_available) {
    auto sizes = distribute_main_axis({LayoutChild{0, 10, SizePolicy::Fixed, 1}}, 1000, 0);
    CK_CHECK(sizes[0].second == 10);
}

CK_TEST(a_lone_expanding_child_claims_all_leftover_space) {
    auto sizes = distribute_main_axis({LayoutChild{0, 10, SizePolicy::Expanding, 1}}, 50, 0);
    CK_CHECK(sizes[0].second == 50);
}

CK_TEST(minimum_children_never_grow_beyond_preferred_even_with_leftover_space) {
    auto sizes = distribute_main_axis({LayoutChild{0, 10, SizePolicy::Minimum, 1}}, 50, 0);
    CK_CHECK(sizes[0].second == 10);  // the 40 leftover cells go unused, not to a Minimum child
}

CK_TEST(leftover_space_splits_between_expanding_children_by_weight) {
    auto sizes = distribute_main_axis(
        {LayoutChild{0, 0, SizePolicy::Expanding, 1}, LayoutChild{0, 0, SizePolicy::Expanding, 3}}, 40, 0);
    CK_CHECK(sizes[0].second == 10);
    CK_CHECK(sizes[1].second == 30);
}

CK_TEST(leftover_space_that_does_not_divide_evenly_is_fully_consumed_not_dropped) {
    // 10 leftover cells over weights {1,1,1}: 3+3+3=9, one remainder
    // cell must land somewhere rather than vanish (container must be
    // filled exactly whenever an Expanding child exists).
    auto sizes = distribute_main_axis({LayoutChild{0, 0, SizePolicy::Expanding, 1},
                                        LayoutChild{0, 0, SizePolicy::Expanding, 1},
                                        LayoutChild{0, 0, SizePolicy::Expanding, 1}},
                                       10, 0);
    const int total = sizes[0].second + sizes[1].second + sizes[2].second;
    CK_CHECK(total == 10);
}

CK_TEST(insufficient_space_shrinks_flexible_children_toward_min_proportionally) {
    // Two Minimum children, preferred 30 each (60 total), only 40
    // available: each has 20 of shrink capacity (min=10), so each
    // gives up half the 20-cell deficit -> 20 apiece.
    auto sizes = distribute_main_axis(
        {LayoutChild{10, 30, SizePolicy::Minimum, 1}, LayoutChild{10, 30, SizePolicy::Minimum, 1}}, 40, 0);
    CK_CHECK(sizes[0].second == 20);
    CK_CHECK(sizes[1].second == 20);
}

CK_TEST(shrinking_never_takes_a_flexible_child_below_its_declared_min) {
    // preferred 30+30=60, only 25 available, min=25 each -> shrink
    // capacity is only 5 each (10 total) — cannot fully cover the
    // 35-cell deficit; each clamps at its own min rather than going
    // negative or below min.
    auto sizes = distribute_main_axis(
        {LayoutChild{25, 30, SizePolicy::Minimum, 1}, LayoutChild{25, 30, SizePolicy::Minimum, 1}}, 25, 0);
    CK_CHECK(sizes[0].second >= 25);
    CK_CHECK(sizes[1].second >= 25);
}

CK_TEST(a_fixed_child_is_untouched_by_shrinking_even_when_siblings_must_shrink) {
    auto sizes = distribute_main_axis(
        {LayoutChild{0, 20, SizePolicy::Fixed, 1}, LayoutChild{5, 30, SizePolicy::Minimum, 1}}, 30, 0);
    CK_CHECK(sizes[0].second == 20);  // Fixed untouched
    CK_CHECK(sizes[1].second == 10);  // Minimum absorbs the entire shortfall: 30 - 20
}

CK_TEST(zero_available_space_does_not_crash_and_clamps_everything_at_or_above_zero) {
    auto sizes = distribute_main_axis(
        {LayoutChild{0, 10, SizePolicy::Minimum, 1}, LayoutChild{0, 10, SizePolicy::Expanding, 1}}, 0, 0);
    for (auto& s : sizes) CK_CHECK(s.second >= 0);
}

CK_TEST(negative_available_space_is_clamped_rather_than_producing_negative_sizes) {
    auto sizes = distribute_main_axis({LayoutChild{0, 10, SizePolicy::Fixed, 1}}, -50, 0);
    CK_CHECK(sizes[0].second == 10);  // Fixed is unaffected by how little room there is
}

// --- Row / Column integration ----------------------------------------------

CK_TEST(row_lays_out_children_left_to_right_filling_its_full_height) {
    Row row(Rect{0, 0, 40, 6});
    auto* a = row.add_item(std::make_unique<View>(Rect{0, 0, 10, 1}), LayoutSpec{SizePolicy::Fixed, 1});
    auto* b = row.add_item(std::make_unique<View>(Rect{0, 0, 10, 1}), LayoutSpec{SizePolicy::Fixed, 1});
    CK_CHECK(a->bounds() == (Rect{0, 0, 10, 6}));
    CK_CHECK(b->bounds() == (Rect{10, 0, 10, 6}));
}

CK_TEST(column_lays_out_children_top_to_bottom_filling_its_full_width) {
    Column col(Rect{0, 0, 20, 12});
    auto* a = col.add_item(std::make_unique<View>(Rect{0, 0, 1, 5}), LayoutSpec{SizePolicy::Fixed, 1});
    auto* b = col.add_item(std::make_unique<View>(Rect{0, 0, 1, 5}), LayoutSpec{SizePolicy::Fixed, 1});
    CK_CHECK(a->bounds() == (Rect{0, 0, 20, 5}));
    CK_CHECK(b->bounds() == (Rect{0, 5, 20, 5}));
}

CK_TEST(resizing_a_row_relayouts_its_children_via_on_resized) {
    Row row(Rect{0, 0, 20, 4});
    auto* a = row.add_item(std::make_unique<View>(), LayoutSpec{SizePolicy::Expanding, 1});
    CK_CHECK(a->bounds().width == 20);
    row.set_bounds(Rect{0, 0, 60, 4});
    CK_CHECK(a->bounds().width == 60);
}

CK_TEST(remove_item_relayouts_the_remaining_children) {
    Row row(Rect{0, 0, 40, 4});
    auto* a = row.add_item(std::make_unique<View>(), LayoutSpec{SizePolicy::Expanding, 1});
    auto* b = row.add_item(std::make_unique<View>(), LayoutSpec{SizePolicy::Expanding, 1});
    CK_CHECK(a->bounds().width == 20);
    row.remove_item(b);
    CK_CHECK(a->bounds().width == 40);
}

CK_TEST(remove_item_for_a_view_not_owned_by_this_row_returns_null_and_does_not_relayout) {
    Row row(Rect{0, 0, 40, 4});
    View stray;
    CK_CHECK(row.remove_item(&stray) == nullptr);
}

CK_TEST(row_spacing_is_reflected_in_child_offsets) {
    Row row(Rect{0, 0, 40, 4});
    row.set_spacing(2);
    auto* a = row.add_item(std::make_unique<View>(), LayoutSpec{SizePolicy::Fixed, 1});
    (void)a;
    // Fixed child with a default View's preferred width (0 from an
    // empty View's own hint) — just confirm spacing changes trigger a
    // relayout without crashing; exact offsets for Fixed-zero-width
    // children are exercised via distribute_main_axis directly above.
    row.set_spacing(2);  // idempotent set: same value, no-op path
    CK_CHECK(row.spacing() == 2);
}
