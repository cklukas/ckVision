// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/frame_view.hpp"

#include "cvision/scene/surface.hpp"
#include "cvision/testing/cktest.hpp"

CK_TEST(frame_view_over_a_surface_reflects_its_cells) {
    ckv::scene::Surface s(ckv::Size{3, 2});
    s.set_cell(ckv::Point{1, 0}, ckv::Cell::from_grapheme("X", ckv::Style{}));

    const ckv::FrameView view = s.view();
    CK_CHECK(view.size().width == 3);
    CK_CHECK(view.size().height == 2);
    CK_CHECK(view.at(ckv::Point{1, 0}).grapheme() == "X");
    CK_CHECK(view.at(ckv::Point{0, 0}).grapheme() == " ");
}

CK_TEST(frame_view_default_constructed_has_zero_size) {
    const ckv::FrameView view;
    CK_CHECK(view.size().width == 0);
    CK_CHECK(view.size().height == 0);
}

CK_TEST(raster_slice_default_state) {
    const ckv::RasterSlice slice;
    CK_CHECK(slice.id == 0);
    CK_CHECK(slice.fallback_active);
    CK_CHECK(slice.image == nullptr);
}
