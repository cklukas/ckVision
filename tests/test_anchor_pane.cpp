// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/anchor_pane.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::Rect;
using ckv::ui::AnchorPane;
using ckv::ui::Anchors;
using ckv::ui::apply_anchors;
using ckv::ui::View;

// --- apply_anchors: pure function --------------------------------------

CK_TEST(no_anchors_leaves_bounds_completely_unchanged) {
    const Rect r = apply_anchors(Rect{5, 5, 10, 10}, Anchors{}, 20, 20);
    CK_CHECK(r == (Rect{5, 5, 10, 10}));
}

CK_TEST(left_only_keeps_x_and_width_the_left_edge_distance_is_preserved_implicitly) {
    const Rect r = apply_anchors(Rect{5, 0, 10, 10}, Anchors{.left = true}, 20, 0);
    CK_CHECK(r == (Rect{5, 0, 10, 10}));
}

CK_TEST(right_only_shifts_x_by_the_delta_and_leaves_width_alone) {
    const Rect r = apply_anchors(Rect{5, 0, 10, 10}, Anchors{.right = true}, 20, 0);
    CK_CHECK(r == (Rect{25, 0, 10, 10}));
}

CK_TEST(left_and_right_stretches_width_by_the_delta_x_unchanged) {
    const Rect r = apply_anchors(Rect{5, 0, 10, 10}, Anchors{.left = true, .right = true}, 20, 0);
    CK_CHECK(r == (Rect{5, 0, 30, 10}));
}

CK_TEST(top_only_keeps_y_and_height) {
    const Rect r = apply_anchors(Rect{0, 5, 10, 10}, Anchors{.top = true}, 0, 20);
    CK_CHECK(r == (Rect{0, 5, 10, 10}));
}

CK_TEST(bottom_only_shifts_y_by_the_delta_and_leaves_height_alone) {
    const Rect r = apply_anchors(Rect{0, 5, 10, 10}, Anchors{.bottom = true}, 0, 20);
    CK_CHECK(r == (Rect{0, 25, 10, 10}));
}

CK_TEST(top_and_bottom_stretches_height_by_the_delta_y_unchanged) {
    const Rect r = apply_anchors(Rect{0, 5, 10, 10}, Anchors{.top = true, .bottom = true}, 0, 20);
    CK_CHECK(r == (Rect{0, 5, 10, 30}));
}

CK_TEST(all_four_anchors_stretch_both_axes_and_never_move_the_top_left_corner) {
    const Anchors all{.left = true, .top = true, .right = true, .bottom = true};
    const Rect r = apply_anchors(Rect{5, 5, 10, 10}, all, 8, 4);
    CK_CHECK(r == (Rect{5, 5, 18, 14}));
}

CK_TEST(a_shrinking_delta_shrinks_a_stretched_child_and_shifts_a_right_anchored_one_left) {
    const Anchors both{.left = true, .right = true};
    CK_CHECK(apply_anchors(Rect{5, 0, 20, 10}, both, -8, 0) == (Rect{5, 0, 12, 10}));
    const Rect shifted = apply_anchors(Rect{25, 0, 10, 10}, Anchors{.right = true}, -8, 0);
    CK_CHECK(shifted == (Rect{17, 0, 10, 10}));
}

// --- AnchorPane -------------------------------------------------------

CK_TEST(add_item_keeps_the_childs_own_given_bounds_unchanged) {
    AnchorPane pane(Rect{0, 0, 40, 20});
    auto* child = pane.add_item(std::make_unique<View>(Rect{2, 2, 6, 3}), Anchors{});
    CK_CHECK(child->bounds() == (Rect{2, 2, 6, 3}));
}

CK_TEST(resizing_the_pane_shifts_a_bottom_right_anchored_child_to_keep_its_corner_distance) {
    // The acceptance criterion's literal scenario: a status label
    // anchored to the bottom-right keeps its distance to that corner.
    AnchorPane pane(Rect{0, 0, 40, 20});
    const Anchors bottom_right{.right = true, .bottom = true};
    auto* label = pane.add_item(std::make_unique<View>(Rect{34, 18, 4, 1}), bottom_right);
    const int right_margin = 40 - (34 + 4);
    const int bottom_margin = 20 - (18 + 1);

    pane.set_bounds(Rect{0, 0, 60, 30});

    CK_CHECK(60 - (label->bounds().x + label->bounds().width) == right_margin);
    CK_CHECK(30 - (label->bounds().y + label->bounds().height) == bottom_margin);
    CK_CHECK(label->bounds().width == 4);   // size itself never changes for a corner anchor
    CK_CHECK(label->bounds().height == 1);
}

CK_TEST(resizing_the_pane_stretches_an_all_anchored_child_to_fill_it) {
    AnchorPane pane(Rect{0, 0, 40, 20});
    const Anchors all{.left = true, .top = true, .right = true, .bottom = true};
    auto* list = pane.add_item(std::make_unique<View>(Rect{1, 1, 38, 18}), all);

    pane.set_bounds(Rect{0, 0, 50, 30});

    CK_CHECK(list->bounds() == (Rect{1, 1, 48, 28}));
}

CK_TEST(a_child_with_no_anchors_does_not_move_or_resize_when_the_pane_does) {
    AnchorPane pane(Rect{0, 0, 40, 20});
    auto* child = pane.add_item(std::make_unique<View>(Rect{5, 5, 6, 3}), Anchors{});

    pane.set_bounds(Rect{0, 0, 80, 40});

    CK_CHECK(child->bounds() == (Rect{5, 5, 6, 3}));
}

CK_TEST(repeated_resizes_accumulate_correctly) {
    AnchorPane pane(Rect{0, 0, 40, 20});
    auto* label = pane.add_item(std::make_unique<View>(Rect{34, 0, 4, 1}), Anchors{.right = true});

    pane.set_bounds(Rect{0, 0, 50, 20});
    CK_CHECK(label->bounds().x == 44);
    pane.set_bounds(Rect{0, 0, 60, 20});
    CK_CHECK(label->bounds().x == 54);
}

CK_TEST(remove_item_returns_ownership_and_stops_tracking_its_anchors) {
    AnchorPane pane(Rect{0, 0, 40, 20});
    View* raw = pane.add_item(std::make_unique<View>(Rect{1, 1, 4, 1}), Anchors{.right = true});

    auto owned = pane.remove_item(raw);
    CK_CHECK(owned != nullptr);
    CK_CHECK(owned.get() == raw);

    pane.set_bounds(Rect{0, 0, 80, 40});  // must not crash touching a stale spec
    CK_CHECK(pane.children().empty());
}

CK_TEST(remove_item_for_a_view_not_owned_by_this_pane_returns_null) {
    AnchorPane pane(Rect{0, 0, 40, 20});
    View stray;
    CK_CHECK(pane.remove_item(&stray) == nullptr);
}

CK_TEST(a_child_added_after_the_pane_was_already_resized_once_is_unaffected_by_that_resize) {
    // last_size_ tracks the pane's own size as of the last on_resized()
    // pass, not each child's own insertion time — a child added AFTER
    // a resize must not retroactively see that resize's delta applied
    // to it on the NEXT one.
    AnchorPane pane(Rect{0, 0, 40, 20});
    pane.set_bounds(Rect{0, 0, 60, 20});
    auto* child = pane.add_item(std::make_unique<View>(Rect{34, 0, 4, 1}), Anchors{.right = true});

    pane.set_bounds(Rect{0, 0, 80, 20});  // a SECOND resize, delta = 80-60 = 20

    CK_CHECK(child->bounds().x == 54);  // 34 + 20, not 34 + (80 - 40)
}
