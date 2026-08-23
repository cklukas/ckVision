// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/overlay.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::Rect;
using ckv::ui::Overlay;
using ckv::ui::OverlayMode;
using ckv::ui::View;

CK_TEST(a_fill_child_is_resized_to_the_overlays_full_extent_on_insertion) {
    Overlay overlay(Rect{0, 0, 40, 20});
    auto* child = overlay.add_item(std::make_unique<View>(Rect{5, 5, 3, 3}));
    CK_CHECK(child->bounds() == (Rect{0, 0, 40, 20}));
}

CK_TEST(fill_is_the_default_mode_when_none_is_given) {
    Overlay overlay(Rect{0, 0, 40, 20});
    auto* child = overlay.add_item(std::make_unique<View>());
    CK_CHECK(child->bounds() == (Rect{0, 0, 40, 20}));
}

CK_TEST(a_manual_child_keeps_its_own_given_bounds_on_insertion) {
    Overlay overlay(Rect{0, 0, 40, 20});
    auto* child =
        overlay.add_item(std::make_unique<View>(Rect{5, 5, 3, 3}), OverlayMode::Manual);
    CK_CHECK(child->bounds() == (Rect{5, 5, 3, 3}));
}

CK_TEST(resizing_the_overlay_re_fills_every_fill_child_but_leaves_manual_children_untouched) {
    Overlay overlay(Rect{0, 0, 40, 20});
    auto* background = overlay.add_item(std::make_unique<View>());
    auto* badge =
        overlay.add_item(std::make_unique<View>(Rect{35, 15, 4, 1}), OverlayMode::Manual);

    overlay.set_bounds(Rect{0, 0, 80, 30});

    CK_CHECK(background->bounds() == (Rect{0, 0, 80, 30}));
    CK_CHECK(badge->bounds() == (Rect{35, 15, 4, 1}));  // manual: entirely unaffected by resize
}

CK_TEST(later_added_children_come_after_earlier_ones_in_the_z_order_child_list) {
    // Z-order is exactly View::children()' own list order (later = on
    // top, per paint_children()/topmost_view_at()'s convention) —
    // Overlay adds no separate z-index concept of its own.
    Overlay overlay(Rect{0, 0, 40, 20});
    auto* bottom = overlay.add_item(std::make_unique<View>());
    auto* top = overlay.add_item(std::make_unique<View>());

    CK_CHECK(overlay.children().size() == 2);
    CK_CHECK(overlay.children()[0].get() == bottom);
    CK_CHECK(overlay.children()[1].get() == top);
}

CK_TEST(raise_to_front_restacks_an_existing_child_above_its_siblings) {
    Overlay overlay(Rect{0, 0, 40, 20});
    auto* bottom = overlay.add_item(std::make_unique<View>());
    auto* top = overlay.add_item(std::make_unique<View>());

    overlay.raise_to_front(bottom);

    CK_CHECK(overlay.children()[0].get() == top);
    CK_CHECK(overlay.children()[1].get() == bottom);
}

CK_TEST(lower_to_back_restacks_an_existing_child_below_its_siblings) {
    Overlay overlay(Rect{0, 0, 40, 20});
    auto* bottom = overlay.add_item(std::make_unique<View>());
    auto* top = overlay.add_item(std::make_unique<View>());

    overlay.lower_to_back(top);

    CK_CHECK(overlay.children()[0].get() == top);
    CK_CHECK(overlay.children()[1].get() == bottom);
}

CK_TEST(remove_item_returns_ownership_and_stops_tracking_its_mode) {
    Overlay overlay(Rect{0, 0, 40, 20});
    View* raw = overlay.add_item(std::make_unique<View>());

    auto owned = overlay.remove_item(raw);
    CK_CHECK(owned != nullptr);
    CK_CHECK(owned.get() == raw);

    overlay.set_bounds(Rect{0, 0, 80, 40});  // must not crash touching a stale spec
    CK_CHECK(overlay.children().empty());
}

CK_TEST(remove_item_for_a_view_not_owned_by_this_overlay_returns_null) {
    Overlay overlay(Rect{0, 0, 40, 20});
    View stray;
    CK_CHECK(overlay.remove_item(&stray) == nullptr);
}
