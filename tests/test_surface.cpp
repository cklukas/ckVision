// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/scene/surface.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::scene::RasterRegion;
using ckv::scene::Surface;

namespace {

std::shared_ptr<ckv::Image> make_image(int w, int h) {
    return std::make_shared<ckv::Image>(w, h);
}

}  // namespace

CK_TEST(new_surface_is_filled_and_fully_damaged) {
    const Surface s(ckv::Size{4, 3}, ckv::Cell::from_grapheme("x", ckv::Style{}));
    CK_CHECK(s.size().width == 4);
    CK_CHECK(s.size().height == 3);
    CK_CHECK(s.at(ckv::Point{2, 1}).grapheme() == "x");
    CK_CHECK(s.has_damage());
    for (int y = 0; y < 3; ++y) {
        const auto span = s.row_damage(y);
        CK_CHECK(span.lo == 0);
        CK_CHECK(span.hi == 4);
    }
}

CK_TEST(set_cell_updates_content_and_expands_row_damage) {
    Surface s(ckv::Size{5, 2});
    s.clear_damage();
    CK_CHECK(!s.has_damage());

    s.set_cell(ckv::Point{2, 0}, ckv::Cell::from_grapheme("A", ckv::Style{}));
    CK_CHECK(s.at(ckv::Point{2, 0}).grapheme() == "A");
    auto span = s.row_damage(0);
    CK_CHECK(span.lo == 2 && span.hi == 3);
    CK_CHECK(s.row_damage(1).empty());

    s.set_cell(ckv::Point{4, 0}, ckv::Cell::from_grapheme("B", ckv::Style{}));
    span = s.row_damage(0);
    CK_CHECK(span.lo == 2 && span.hi == 5);  // span grows to cover both writes

    s.set_cell(ckv::Point{0, 0}, ckv::Cell::from_grapheme("C", ckv::Style{}));
    span = s.row_damage(0);
    CK_CHECK(span.lo == 0 && span.hi == 5);
}

CK_TEST(mark_damage_clips_to_surface_bounds_and_unions_across_rows) {
    Surface s(ckv::Size{5, 5});
    s.clear_damage();
    s.mark_damage(ckv::Rect{-2, 1, 4, 2});  // partially off the left edge
    CK_CHECK(s.row_damage(0).empty());
    CK_CHECK(s.row_damage(1).lo == 0 && s.row_damage(1).hi == 2);
    CK_CHECK(s.row_damage(2).lo == 0 && s.row_damage(2).hi == 2);
    CK_CHECK(s.row_damage(3).empty());
}

CK_TEST(clear_damage_resets_every_row) {
    Surface s(ckv::Size{3, 3});
    CK_CHECK(s.has_damage());
    s.clear_damage();
    CK_CHECK(!s.has_damage());
    for (int y = 0; y < 3; ++y) CK_CHECK(s.row_damage(y).empty());
}

CK_TEST(resize_reallocates_clears_raster_regions_and_fully_damages) {
    Surface s(ckv::Size{2, 2});
    s.clear_damage();
    s.add_raster_region(RasterRegion{1, ckv::Rect{0, 0, 1, 1}, make_image(4, 4), true, ckv::Rect{0, 0, 1, 1}});
    CK_CHECK(s.raster_regions().size() == 1);

    s.resize(ckv::Size{6, 4}, ckv::Cell::from_grapheme("z", ckv::Style{}));
    CK_CHECK(s.size().width == 6);
    CK_CHECK(s.size().height == 4);
    CK_CHECK(s.at(ckv::Point{5, 3}).grapheme() == "z");
    CK_CHECK(s.raster_regions().empty());
    CK_CHECK(s.has_damage());
    CK_CHECK(s.row_damage(0).lo == 0 && s.row_damage(0).hi == 6);
}

CK_TEST(raster_region_lifecycle_marks_damage_and_supports_removal) {
    Surface s(ckv::Size{10, 10});
    s.clear_damage();

    s.add_raster_region(RasterRegion{1, ckv::Rect{2, 2, 3, 3}, make_image(8, 8), true, ckv::Rect{2, 2, 3, 3}});
    CK_CHECK(s.row_damage(2).lo == 2 && s.row_damage(2).hi == 5);
    CK_CHECK(s.row_damage(4).lo == 2 && s.row_damage(4).hi == 5);
    CK_CHECK(s.row_damage(0).empty());
    s.clear_damage();

    s.set_raster_fallback_active(1, false);
    CK_CHECK(s.raster_regions()[0].fallback_active == false);
    CK_CHECK(s.row_damage(2).lo == 2 && s.row_damage(2).hi == 5);  // toggling re-damages the anchor
    s.clear_damage();

    // Toggling to the same value already in effect must not re-damage.
    s.set_raster_fallback_active(1, false);
    CK_CHECK(!s.has_damage());

    s.remove_raster_region(1);
    CK_CHECK(s.raster_regions().empty());
    CK_CHECK(s.row_damage(2).lo == 2 && s.row_damage(2).hi == 5);  // removal damages the old anchor
}

CK_TEST(raster_regions_can_stack_independently) {
    Surface s(ckv::Size{10, 10});
    s.add_raster_region(RasterRegion{1, ckv::Rect{0, 0, 2, 2}, make_image(4, 4), true, ckv::Rect{0, 0, 2, 2}});
    s.add_raster_region(RasterRegion{2, ckv::Rect{5, 5, 2, 2}, make_image(4, 4), false, ckv::Rect{5, 5, 2, 2}});
    CK_CHECK(s.raster_regions().size() == 2);
    CK_CHECK(s.raster_regions()[0].id == 1);
    CK_CHECK(s.raster_regions()[1].fallback_active == false);
}
