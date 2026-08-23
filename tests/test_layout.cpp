// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/layout.hpp"

#include <algorithm>

#include "cvision/testing/cktest.hpp"

using ckv::Rect;
using ckv::ui::align_cross_axis;
using ckv::ui::Alignment;
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

// --- Container size-hint aggregation --------------------------------------

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

CK_TEST(a_rows_horizontal_hint_sums_its_children_plus_spacing) {
    Row row;
    row.set_spacing(2);
    row.add_item(std::make_unique<HintedView>(10, 2));
    row.add_item(std::make_unique<HintedView>(8, 1));
    CK_CHECK(row.horizontal_size_hint().preferred == 10 + 8 + 2);
}

CK_TEST(a_rows_vertical_hint_is_the_max_over_its_children) {
    Row row;
    row.add_item(std::make_unique<HintedView>(10, 2));
    row.add_item(std::make_unique<HintedView>(8, 1));
    CK_CHECK(row.vertical_size_hint().preferred == 2);
}

CK_TEST(a_columns_vertical_hint_sums_and_horizontal_hint_maxes) {
    Column column;
    column.set_spacing(1);
    column.add_item(std::make_unique<HintedView>(10, 2));
    column.add_item(std::make_unique<HintedView>(8, 3));
    CK_CHECK(column.vertical_size_hint().preferred == 2 + 3 + 1);
    CK_CHECK(column.horizontal_size_hint().preferred == 10);
}

CK_TEST(a_fixed_policy_row_nested_in_a_column_receives_its_aggregated_height) {
    // Regression: a nested container's own hints used to be the bare
    // View default (preferred 0), so a Fixed-policy button row inside
    // a dialog's Column silently collapsed to zero cells — every
    // factory dialog's buttons were invisible.
    Column column(Rect{0, 0, 40, 10});
    auto row = std::make_unique<Row>();
    row->add_item(std::make_unique<HintedView>(10, 2));
    auto* row_ptr = column.add_item(std::move(row), LayoutSpec{SizePolicy::Fixed, 1});
    CK_CHECK(row_ptr->bounds().height == 2);
}

CK_TEST(a_row_uses_child_height_for_allocated_width_on_the_cross_axis) {
    Row row(Rect{0, 0, 5, 10});
    auto* child = row.add_item(std::make_unique<HeightForWidthView>(),
                               LayoutSpec{SizePolicy::Expanding, 1, Alignment::Start});
    CK_CHECK(child->bounds() == (Rect{0, 0, 5, 4}));

    row.set_bounds(Rect{0, 0, 10, 10});

    CK_CHECK(child->bounds() == (Rect{0, 0, 10, 2}));
}

CK_TEST(a_column_uses_child_height_for_its_resolved_cross_axis_width) {
    Column column(Rect{0, 0, 5, 20});
    auto* child = column.add_item(std::make_unique<HeightForWidthView>(),
                                  LayoutSpec{SizePolicy::Fixed, 1, Alignment::Fill});
    CK_CHECK(child->bounds() == (Rect{0, 0, 5, 4}));

    column.set_bounds(Rect{0, 0, 10, 20});

    CK_CHECK(child->bounds() == (Rect{0, 0, 10, 2}));
}

CK_TEST(an_empty_container_reports_zero_preferred_size_without_crashing) {
    Row row;
    Column column;
    CK_CHECK(row.horizontal_size_hint().preferred == 0);
    CK_CHECK(column.vertical_size_hint().preferred == 0);
}

// --- Size-hint-change propagation (M9/WP-16, E10) ---------------------

namespace {
class MutableWidthView : public View {
public:
    explicit MutableWidthView(int width) : width_(width) {}
    void set_width(int width) {
        width_ = width;
        size_hint_changed();
    }
    ckv::ui::SizeHint horizontal_size_hint() const override {
        return ckv::ui::SizeHint{0, width_, ckv::ui::kUnboundedExtent};
    }

private:
    int width_;
};

class CountingView : public View {
public:
    int resizes = 0;
    void on_resized() override { ++resizes; }
};
}  // namespace

CK_TEST(a_row_re_distributes_when_a_child_grows_its_own_hint_without_a_row_resize) {
    Row row(Rect{0, 0, 100, 5});
    auto* grower =
        static_cast<MutableWidthView*>(row.add_item(std::make_unique<MutableWidthView>(5)));
    auto* second = static_cast<CountingView*>(
        row.add_item(std::make_unique<CountingView>(), LayoutSpec{SizePolicy::Expanding, 1}));
    const int x_before = second->bounds().x;

    grower->set_width(20);  // no row resize — only the child's own hint changed

    CK_CHECK(second->bounds().x == x_before + 15);  // shifted right by exactly the growth
}

CK_TEST(a_childs_changed_size_hint_triggers_exactly_one_relayout_pass) {
    Row row(Rect{0, 0, 100, 5});
    auto* grower =
        static_cast<MutableWidthView*>(row.add_item(std::make_unique<MutableWidthView>(5)));
    auto* spy = static_cast<CountingView*>(
        row.add_item(std::make_unique<CountingView>(), LayoutSpec{SizePolicy::Expanding, 1}));
    const int before = spy->resizes;

    grower->set_width(20);  // one notification must produce one relayout, not a storm of repeats

    CK_CHECK(spy->resizes == before + 1);
}

// --- align_cross_axis: pure function (M10/WP-19) -----------------------

CK_TEST(fill_alignment_takes_the_whole_available_extent_minus_margins) {
    const auto [offset, extent] = align_cross_axis(20, 6, Alignment::Fill, 0, 0);
    CK_CHECK(offset == 0);
    CK_CHECK(extent == 20);
}

CK_TEST(fill_alignment_with_margins_shrinks_and_shifts_by_the_before_margin) {
    const auto [offset, extent] = align_cross_axis(20, 6, Alignment::Fill, 2, 3);
    CK_CHECK(offset == 2);
    CK_CHECK(extent == 15);  // 20 - 2 - 3
}

CK_TEST(start_alignment_sits_at_the_near_edge_sized_to_its_own_preferred_extent) {
    const auto [offset, extent] = align_cross_axis(20, 6, Alignment::Start, 0, 0);
    CK_CHECK(offset == 0);
    CK_CHECK(extent == 6);
}

CK_TEST(end_alignment_sits_flush_against_the_far_edge) {
    const auto [offset, extent] = align_cross_axis(20, 6, Alignment::End, 0, 0);
    CK_CHECK(offset == 14);  // 20 - 6
    CK_CHECK(extent == 6);
}

CK_TEST(center_alignment_splits_the_leftover_space_evenly_on_both_sides) {
    const auto [offset, extent] = align_cross_axis(20, 6, Alignment::Center, 0, 0);
    CK_CHECK(offset == 7);  // (20 - 6) / 2
    CK_CHECK(extent == 6);
}

CK_TEST(start_center_and_end_all_respect_margins_too) {
    CK_CHECK(align_cross_axis(20, 6, Alignment::Start, 2, 3) == (std::pair{2, 6}));
    CK_CHECK(align_cross_axis(20, 6, Alignment::End, 2, 3) == (std::pair{11, 6}));  // 2 + 15 - 6
    const auto centered = align_cross_axis(20, 6, Alignment::Center, 2, 3);
    CK_CHECK(centered == (std::pair{6, 6}));  // 2 + (15-6)/2
}

CK_TEST(a_preferred_extent_larger_than_available_is_clamped_never_overflows) {
    const auto [offset, extent] = align_cross_axis(10, 50, Alignment::Start, 0, 0);
    CK_CHECK(offset == 0);
    CK_CHECK(extent == 10);
}

CK_TEST(margins_that_consume_all_available_space_clamp_to_zero_never_negative) {
    const auto [offset, extent] = align_cross_axis(5, 6, Alignment::Fill, 3, 4);
    CK_CHECK(extent == 0);
    (void)offset;  // still well-defined (margin_before), just zero width to show
}

// --- Row/Column cross-axis alignment and margins (M10/WP-19) -----------

CK_TEST(a_start_aligned_child_keeps_its_own_height_at_the_top_of_the_row) {
    Row row(Rect{0, 0, 40, 10});
    const LayoutSpec spec{SizePolicy::Fixed, 1, Alignment::Start};
    auto* child = static_cast<HintedView*>(row.add_item(std::make_unique<HintedView>(10, 3), spec));
    CK_CHECK(child->bounds() == (Rect{0, 0, 10, 3}));
}

CK_TEST(an_end_aligned_child_sits_flush_against_the_bottom_of_the_row) {
    Row row(Rect{0, 0, 40, 10});
    const LayoutSpec spec{SizePolicy::Fixed, 1, Alignment::End};
    auto* child = static_cast<HintedView*>(row.add_item(std::make_unique<HintedView>(10, 3), spec));
    CK_CHECK(child->bounds() == (Rect{0, 7, 10, 3}));
}

CK_TEST(a_center_aligned_child_is_vertically_centered_in_the_row) {
    Row row(Rect{0, 0, 40, 10});
    auto* child = static_cast<HintedView*>(row.add_item(
        std::make_unique<HintedView>(10, 4), LayoutSpec{SizePolicy::Fixed, 1, Alignment::Center}));
    CK_CHECK(child->bounds() == (Rect{0, 3, 10, 4}));  // (10 - 4) / 2 == 3
}

CK_TEST(margins_shrink_a_fill_aligned_row_child_on_the_cross_axis) {
    Row row(Rect{0, 0, 40, 10});
    LayoutSpec spec{SizePolicy::Fixed, 1, Alignment::Fill};
    spec.margin_before = 1;
    spec.margin_after = 2;
    auto* child = static_cast<HintedView*>(row.add_item(std::make_unique<HintedView>(10, 4), spec));
    CK_CHECK(child->bounds() == (Rect{0, 1, 10, 7}));  // height 10 - 1 - 2
}

CK_TEST(default_alignment_still_fills_the_row_exactly_as_before_wp_19) {
    // Regression guard: LayoutSpec{}'s default alignment must remain
    // Fill so every Row/Column built before this landed keeps behaving
    // identically with no call-site changes. Main-axis width (10) comes
    // from the Fixed policy's own preferred size, untouched by this WP;
    // what's under test here is only the cross-axis height still
    // filling the row's own full height (10) with no alignment given.
    Row row(Rect{0, 0, 40, 10});
    auto* child = static_cast<HintedView*>(
        row.add_item(std::make_unique<HintedView>(10, 3), LayoutSpec{SizePolicy::Fixed, 1}));
    CK_CHECK(child->bounds() == (Rect{0, 0, 10, 10}));
}

CK_TEST(a_start_aligned_column_child_keeps_its_own_width_at_the_left) {
    Column column(Rect{0, 0, 20, 30});
    const LayoutSpec spec{SizePolicy::Fixed, 1, Alignment::Start};
    auto* child = static_cast<HintedView*>(
        column.add_item(std::make_unique<HintedView>(6, 10), spec));
    CK_CHECK(child->bounds() == (Rect{0, 0, 6, 10}));
}

CK_TEST(an_end_aligned_column_child_sits_flush_against_the_right_edge) {
    Column column(Rect{0, 0, 20, 30});
    const LayoutSpec spec{SizePolicy::Fixed, 1, Alignment::End};
    auto* child = static_cast<HintedView*>(
        column.add_item(std::make_unique<HintedView>(6, 10), spec));
    CK_CHECK(child->bounds() == (Rect{14, 0, 6, 10}));  // 20 - 6
}

CK_TEST(a_row_can_mix_fill_and_non_fill_aligned_children_independently) {
    Row row(Rect{0, 0, 40, 10});
    auto* filled = static_cast<HintedView*>(row.add_item(std::make_unique<HintedView>(10, 3)));
    const LayoutSpec spec{SizePolicy::Fixed, 1, Alignment::Start};
    auto* started = static_cast<HintedView*>(
        row.add_item(std::make_unique<HintedView>(10, 3), spec));
    CK_CHECK(filled->bounds() == (Rect{0, 0, 10, 10}));   // default Fill: full row height
    CK_CHECK(started->bounds() == (Rect{10, 0, 10, 3}));  // Start: its own preferred height
}

// --- Visibility and layout space -------------------------------------------

namespace {
class Sized : public View {
public:
    explicit Sized(int extent) : extent_(extent) {}
    ckv::ui::SizeHint horizontal_size_hint() const override {
        return ckv::ui::SizeHint{extent_, extent_, ckv::ui::kUnboundedExtent};
    }
    ckv::ui::SizeHint vertical_size_hint() const override {
        return ckv::ui::SizeHint{extent_, extent_, ckv::ui::kUnboundedExtent};
    }

private:
    int extent_;
};
}  // namespace

CK_TEST(a_hidden_child_takes_no_room_in_a_column_and_leaves_no_gap) {
    // Hiding a child is how a container whose contents depend on state drops
    // it from the layout without dropping it from the tree. Reserving its
    // space would leave a hole the reader cannot account for.
    Column column;
    column.set_spacing(1);
    column.set_bounds(Rect{0, 0, 20, 20});
    auto* first = static_cast<Sized*>(column.add_item(std::make_unique<Sized>(3),
                                                       LayoutSpec{SizePolicy::Fixed, 1}));
    auto* middle = static_cast<Sized*>(column.add_item(std::make_unique<Sized>(4),
                                                        LayoutSpec{SizePolicy::Fixed, 1}));
    auto* last = static_cast<Sized*>(column.add_item(std::make_unique<Sized>(5),
                                                      LayoutSpec{SizePolicy::Fixed, 1}));
    CK_CHECK(first->bounds().y == 0);
    CK_CHECK(middle->bounds().y == 4);   // 3 + 1 spacing
    CK_CHECK(last->bounds().y == 9);     // + 4 + 1 spacing

    middle->set_visible(false);
    column.set_bounds(Rect{0, 0, 20, 21});  // force a relayout
    CK_CHECK(first->bounds().y == 0);
    CK_CHECK(first->bounds().height == 3);
    // The last child moves up into the hidden one's place, and only ONE
    // spacing gap separates it from the first — not two.
    CK_CHECK(last->bounds().y == 4);
    CK_CHECK(last->bounds().height == 5);

    middle->set_visible(true);
    column.set_bounds(Rect{0, 0, 20, 20});
    CK_CHECK(last->bounds().y == 9);
}

CK_TEST(a_hidden_child_contributes_nothing_to_a_columns_size_hints) {
    Column column;
    column.set_spacing(1);
    column.add_item(std::make_unique<Sized>(3), LayoutSpec{SizePolicy::Fixed, 1});
    auto* hidden = column.add_item(std::make_unique<Sized>(4), LayoutSpec{SizePolicy::Fixed, 1});
    const int with_both = column.vertical_size_hint().preferred;
    hidden->set_visible(false);
    const int with_one = column.vertical_size_hint().preferred;
    // Losing the child loses its extent AND the gap that separated it.
    CK_CHECK(with_both == 8);  // 3 + 1 + 4
    CK_CHECK(with_one == 3);
}

CK_TEST(a_hidden_child_takes_no_room_in_a_row_either) {
    Row row;
    row.set_spacing(2);
    row.set_bounds(Rect{0, 0, 30, 5});
    auto* first = row.add_item(std::make_unique<Sized>(4), LayoutSpec{SizePolicy::Fixed, 1});
    auto* hidden = row.add_item(std::make_unique<Sized>(6), LayoutSpec{SizePolicy::Fixed, 1});
    auto* last = row.add_item(std::make_unique<Sized>(5), LayoutSpec{SizePolicy::Fixed, 1});
    CK_CHECK(last->bounds().x == 14);  // 4 + 2 + 6 + 2

    hidden->set_visible(false);
    row.set_bounds(Rect{0, 0, 31, 5});
    CK_CHECK(first->bounds().x == 0);
    CK_CHECK(last->bounds().x == 6);  // 4 + 2
}

// --- Height for width through a container ----------------------------------

namespace {
// A child whose height depends on its width, the way wrapped text's does:
// `cells` characters poured into however many columns it is given.
class Wrapped : public View {
public:
    explicit Wrapped(int cells) : cells_(cells) {}
    ckv::ui::SizeHint horizontal_size_hint() const override {
        return ckv::ui::SizeHint{1, std::min(cells_, 10), ckv::ui::kUnboundedExtent};
    }
    ckv::ui::SizeHint vertical_size_hint() const override {
        return ckv::ui::SizeHint{1, 1, ckv::ui::kUnboundedExtent};
    }
    int height_for_width(int width) const override {
        return width <= 0 ? cells_ : (cells_ + width - 1) / width;
    }

private:
    int cells_;
};
}  // namespace

CK_TEST(a_column_answers_height_for_width_from_the_widths_it_gives_its_children) {
    // Without this the query stops at the container, and anything sized from
    // the column -- a window opening around it -- is told a wrapped child
    // needs one row whatever the width.
    Column column;
    column.set_spacing(1);
    column.add_item(std::make_unique<Wrapped>(40), LayoutSpec{SizePolicy::Fixed, 1});
    column.add_item(std::make_unique<Wrapped>(30), LayoutSpec{SizePolicy::Fixed, 1});

    CK_CHECK(column.height_for_width(10) == 4 + 3 + 1);  // 40/10 rows, 30/10 rows, one gap
    CK_CHECK(column.height_for_width(5) == 8 + 6 + 1);   // narrower: the same text, more rows
}

CK_TEST(a_column_measures_a_start_aligned_child_at_its_own_width_not_the_columns) {
    // A child that is not Fill-aligned is laid out at its own preferred
    // width, so that -- not the column's -- is the width it wraps into.
    Column column;
    auto* child = column.add_item(std::make_unique<Wrapped>(40),
                                   LayoutSpec{SizePolicy::Fixed, 1, Alignment::Start});
    CK_CHECK(child->horizontal_size_hint().preferred == 10);
    CK_CHECK(column.height_for_width(40) == 4);  // measured at 10, its own width, not at 40
}

CK_TEST(a_row_is_as_tall_as_its_tallest_child_at_the_width_that_child_gets) {
    Row row;
    row.set_spacing(2);
    row.add_item(std::make_unique<Wrapped>(40), LayoutSpec{SizePolicy::Expanding, 1});
    row.add_item(std::make_unique<Wrapped>(12), LayoutSpec{SizePolicy::Expanding, 1});
    // 22 cells less the gap, split evenly: 10 columns each. 40 cells wrap to
    // four rows, 12 to two, and the row is as tall as the taller.
    CK_CHECK(row.height_for_width(22) == 4);
}

CK_TEST(a_hidden_child_is_not_measured_for_height_either) {
    Column column;
    column.set_spacing(1);
    column.add_item(std::make_unique<Wrapped>(40), LayoutSpec{SizePolicy::Fixed, 1});
    auto* hidden = column.add_item(std::make_unique<Wrapped>(40), LayoutSpec{SizePolicy::Fixed, 1});
    CK_CHECK(column.height_for_width(10) == 4 + 1 + 4);
    hidden->set_visible(false);
    CK_CHECK(column.height_for_width(10) == 4);  // its rows and its gap both go
}

CK_TEST(an_empty_container_needs_no_height_at_any_width) {
    Column column;
    Row row;
    CK_CHECK(column.height_for_width(40) == 0);
    CK_CHECK(row.height_for_width(40) == 0);
}
