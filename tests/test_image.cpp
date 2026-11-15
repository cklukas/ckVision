// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/image.hpp"

#include "cvision/testing/cktest.hpp"

CK_TEST(image_default_is_empty) {
    const ckv::Image img;
    CK_CHECK(img.empty());
    CK_CHECK(img.width() == 0);
    CK_CHECK(img.height() == 0);
}

CK_TEST(image_sized_is_zeroed) {
    ckv::Image img(ckv::PixelSize{4, 3});
    CK_CHECK(!img.empty());
    CK_CHECK(img.width() == 4);
    CK_CHECK(img.height() == 3);
    CK_CHECK(img.stride() == 16);
    const ckv::Image::Rgba p = img.pixel(2, 1);
    CK_CHECK(p.r == 0);
    CK_CHECK(p.g == 0);
    CK_CHECK(p.b == 0);
    CK_CHECK(p.a == 0);
}

CK_TEST(image_set_and_get_pixel_roundtrip) {
    ckv::Image img(ckv::PixelSize{2, 2});
    img.set_pixel(1, 1, ckv::Image::Rgba{10, 20, 30, 255});
    const ckv::Image::Rgba p = img.pixel(1, 1);
    CK_CHECK(p.r == 10);
    CK_CHECK(p.g == 20);
    CK_CHECK(p.b == 30);
    CK_CHECK(p.a == 255);
    // Neighboring pixels are untouched.
    const ckv::Image::Rgba q = img.pixel(0, 0);
    CK_CHECK(q.r == 0 && q.a == 0);
}

CK_TEST(image_negative_dimensions_clamp_to_empty) {
    const ckv::Image img(ckv::PixelSize{-5, 3});
    CK_CHECK(img.empty());
    CK_CHECK(img.width() == 0);
}

CK_TEST(image_overflow_clamps_to_empty_and_content_id_is_monotonic) {
    const ckv::Image empty;
    CK_CHECK(empty.content_id() == 0);

    const ckv::Image overflowed(ckv::PixelSize{std::numeric_limits<int>::max(), 1});
    CK_CHECK(overflowed.empty());
    CK_CHECK(overflowed.content_id() == 0);

    ckv::Image a(ckv::PixelSize{2, 2});
    ckv::Image b(ckv::PixelSize{2, 2});
    CK_CHECK(a.content_id() > 0);
    CK_CHECK(b.content_id() > a.content_id());

    const ckv::Image copy = a;
    CK_CHECK(copy.content_id() == a.content_id());

    const std::uint64_t before = a.content_id();
    a.mark_modified();
    CK_CHECK(a.content_id() > before);
}

