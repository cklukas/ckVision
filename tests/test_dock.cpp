// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/dock.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::Rect;
using ckv::ui::compute_dock_layout;
using ckv::ui::Dock;
using ckv::ui::DockEdge;
using ckv::ui::DockEdgeExtents;
using ckv::ui::DockRects;
using ckv::ui::View;

// --- compute_dock_layout: pure function ---------------------------------

CK_TEST(no_edges_gives_the_whole_available_rect_to_center) {
    const DockRects rects = compute_dock_layout(Rect{0, 0, 40, 20}, DockEdgeExtents{});
    CK_CHECK(rects.center == (Rect{0, 0, 40, 20}));
}

CK_TEST(a_top_edge_claims_a_full_width_strip_and_shrinks_center_below_it) {
    const DockRects rects = compute_dock_layout(Rect{0, 0, 40, 20}, DockEdgeExtents{.top = 3});
    CK_CHECK(rects.top == (Rect{0, 0, 40, 3}));
    CK_CHECK(rects.center == (Rect{0, 3, 40, 17}));
}

CK_TEST(a_bottom_edge_claims_a_full_width_strip_and_shrinks_center_above_it) {
    const DockRects rects = compute_dock_layout(Rect{0, 0, 40, 20}, DockEdgeExtents{.bottom = 3});
    CK_CHECK(rects.bottom == (Rect{0, 17, 40, 3}));
    CK_CHECK(rects.center == (Rect{0, 0, 40, 17}));
}

CK_TEST(a_left_edge_claims_a_full_height_strip_and_shrinks_center_beside_it) {
    const DockRects rects = compute_dock_layout(Rect{0, 0, 40, 20}, DockEdgeExtents{.left = 5});
    CK_CHECK(rects.left == (Rect{0, 0, 5, 20}));
    CK_CHECK(rects.center == (Rect{5, 0, 35, 20}));
}

CK_TEST(a_right_edge_claims_a_full_height_strip_and_shrinks_center_beside_it) {
    const DockRects rects = compute_dock_layout(Rect{0, 0, 40, 20}, DockEdgeExtents{.right = 5});
    CK_CHECK(rects.right == (Rect{35, 0, 5, 20}));
    CK_CHECK(rects.center == (Rect{0, 0, 35, 20}));
}

CK_TEST(all_four_edges_reserve_top_bottom_left_right_so_left_right_never_touch_top_bottom) {
    DockEdgeExtents edges{.top = 2, .bottom = 3, .left = 4, .right = 5};
    const DockRects rects = compute_dock_layout(Rect{0, 0, 40, 20}, edges);
    CK_CHECK(rects.top == (Rect{0, 0, 40, 2}));
    CK_CHECK(rects.bottom == (Rect{0, 17, 40, 3}));
    // Left/Right only span the vertical gap Top/Bottom left (2..17 = 15 rows), not the full 20.
    CK_CHECK(rects.left == (Rect{0, 2, 4, 15}));
    CK_CHECK(rects.right == (Rect{35, 2, 5, 15}));
    CK_CHECK(rects.center == (Rect{4, 2, 31, 15}));
}

CK_TEST(a_preferred_extent_larger_than_available_is_clamped_never_overflows_or_goes_negative) {
    const DockRects rects = compute_dock_layout(Rect{0, 0, 40, 20}, DockEdgeExtents{.top = 999});
    CK_CHECK(rects.top == (Rect{0, 0, 40, 20}));
    CK_CHECK(rects.center == (Rect{0, 20, 40, 0}));
}

CK_TEST(a_non_zero_available_origin_offsets_every_resulting_rect_the_same_way) {
    const DockRects rects = compute_dock_layout(Rect{10, 5, 40, 20}, DockEdgeExtents{.top = 3});
    CK_CHECK(rects.top == (Rect{10, 5, 40, 3}));
    CK_CHECK(rects.center == (Rect{10, 8, 40, 17}));
}

// --- Dock ------------------------------------------------------------------

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

CK_TEST(a_center_only_child_fills_the_whole_dock) {
    Dock dock(Rect{0, 0, 40, 20});
    auto* child = dock.add_item(std::make_unique<View>(), DockEdge::Center);
    CK_CHECK(child->bounds() == (Rect{0, 0, 40, 20}));
}

CK_TEST(a_top_child_is_sized_to_its_own_preferred_height_full_width) {
    Dock dock(Rect{0, 0, 40, 20});
    auto* child = dock.add_item(std::make_unique<HintedView>(0, 4), DockEdge::Top);
    CK_CHECK(child->bounds() == (Rect{0, 0, 40, 4}));
}

CK_TEST(a_top_child_uses_height_for_the_docks_current_width) {
    Dock dock(Rect{0, 0, 5, 10});
    auto* top = dock.add_item(std::make_unique<HeightForWidthView>(), DockEdge::Top);
    auto* center = dock.add_item(std::make_unique<View>(), DockEdge::Center);
    CK_CHECK(top->bounds() == (Rect{0, 0, 5, 4}));
    CK_CHECK(center->bounds() == (Rect{0, 4, 5, 6}));

    dock.set_bounds(Rect{0, 0, 10, 10});

    CK_CHECK(top->bounds() == (Rect{0, 0, 10, 2}));
    CK_CHECK(center->bounds() == (Rect{0, 2, 10, 8}));
}

CK_TEST(all_five_slots_together_match_the_pure_function_result) {
    Dock dock(Rect{0, 0, 40, 20});
    auto* top = dock.add_item(std::make_unique<HintedView>(0, 2), DockEdge::Top);
    auto* bottom = dock.add_item(std::make_unique<HintedView>(0, 3), DockEdge::Bottom);
    auto* left = dock.add_item(std::make_unique<HintedView>(4, 0), DockEdge::Left);
    auto* right = dock.add_item(std::make_unique<HintedView>(5, 0), DockEdge::Right);
    auto* center = dock.add_item(std::make_unique<View>(), DockEdge::Center);

    CK_CHECK(top->bounds() == (Rect{0, 0, 40, 2}));
    CK_CHECK(bottom->bounds() == (Rect{0, 17, 40, 3}));
    CK_CHECK(left->bounds() == (Rect{0, 2, 4, 15}));
    CK_CHECK(right->bounds() == (Rect{35, 2, 5, 15}));
    CK_CHECK(center->bounds() == (Rect{4, 2, 31, 15}));
}

CK_TEST(remove_item_returns_ownership_and_stops_tracking_its_edge) {
    Dock dock(Rect{0, 0, 40, 20});
    View* raw = dock.add_item(std::make_unique<View>(), DockEdge::Top);

    auto owned = dock.remove_item(raw);
    CK_CHECK(owned != nullptr);
    CK_CHECK(owned.get() == raw);

    dock.set_bounds(Rect{0, 0, 80, 40});  // must not crash touching a stale spec
    CK_CHECK(dock.children().empty());
}

CK_TEST(remove_item_for_a_view_not_owned_by_this_dock_returns_null) {
    Dock dock(Rect{0, 0, 40, 20});
    View stray;
    CK_CHECK(dock.remove_item(&stray) == nullptr);
}

CK_TEST(a_freed_edge_can_be_reused_by_a_new_child_after_remove_item) {
    Dock dock(Rect{0, 0, 40, 20});
    View* first = dock.add_item(std::make_unique<View>(), DockEdge::Top);
    dock.remove_item(first);

    auto* second = dock.add_item(std::make_unique<HintedView>(0, 4), DockEdge::Top);
    CK_CHECK(second->bounds() == (Rect{0, 0, 40, 4}));
}

CK_TEST(resizing_the_dock_relayouts_every_slot_to_the_new_extents) {
    Dock dock(Rect{0, 0, 40, 20});
    auto* top = dock.add_item(std::make_unique<HintedView>(0, 2), DockEdge::Top);
    auto* center = dock.add_item(std::make_unique<View>(), DockEdge::Center);

    dock.set_bounds(Rect{0, 0, 80, 40});

    CK_CHECK(top->bounds() == (Rect{0, 0, 80, 2}));
    CK_CHECK(center->bounds() == (Rect{0, 2, 80, 38}));
}

namespace {
class MutableHeightView : public View {
public:
    explicit MutableHeightView(int height) : height_(height) {}
    void set_height(int height) {
        height_ = height;
        size_hint_changed();
    }
    ckv::ui::SizeHint vertical_size_hint() const override { return {height_, height_, height_}; }

private:
    int height_;
};
}  // namespace

CK_TEST(a_dock_relayouts_when_a_child_grows_its_own_hint_without_a_dock_resize) {
    Dock dock(Rect{0, 0, 40, 20});
    auto* top = static_cast<MutableHeightView*>(
        dock.add_item(std::make_unique<MutableHeightView>(2), DockEdge::Top));
    auto* center = dock.add_item(std::make_unique<View>(), DockEdge::Center);
    CK_CHECK(top->bounds().height == 2);
    CK_CHECK(center->bounds() == (Rect{0, 2, 40, 18}));

    top->set_height(5);  // no dock resize — only the child's own hint changed

    CK_CHECK(top->bounds().height == 5);
    CK_CHECK(center->bounds() == (Rect{0, 5, 40, 15}));
}

CK_TEST(a_second_child_docked_to_an_already_occupied_edge_aborts) {
    CK_EXPECT_ABORT({
        Dock dock(Rect{0, 0, 40, 20});
        dock.add_item(std::make_unique<View>(), DockEdge::Top);
        dock.add_item(std::make_unique<View>(), DockEdge::Top);
    });
}
