// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/geometry.hpp"

#include "cvision/testing/cktest.hpp"

CK_TEST(rect_basic_accessors) {
    const ckv::Rect r{2, 3, 10, 5};
    CK_CHECK(r.left() == 2);
    CK_CHECK(r.top() == 3);
    CK_CHECK(r.right() == 12);
    CK_CHECK(r.bottom() == 8);
    CK_CHECK(!r.empty());
}

CK_TEST(rect_empty_when_nonpositive) {
    CK_CHECK((ckv::Rect{0, 0, 0, 5}).empty());
    CK_CHECK((ckv::Rect{0, 0, 5, 0}).empty());
    CK_CHECK((ckv::Rect{0, 0, -1, 5}).empty());
}

CK_TEST(rect_contains) {
    const ckv::Rect r{0, 0, 10, 10};
    CK_CHECK(r.contains(ckv::Point{0, 0}));
    CK_CHECK(r.contains(ckv::Point{9, 9}));
    CK_CHECK(!r.contains(ckv::Point{10, 5}));   // right edge is exclusive
    CK_CHECK(!r.contains(ckv::Point{5, 10}));   // bottom edge is exclusive
    CK_CHECK(!r.contains(ckv::Point{-1, 5}));
    CK_CHECK(!(ckv::Rect{}).contains(ckv::Point{0, 0}));  // empty rect contains nothing
}

CK_TEST(rect_intersected) {
    const ckv::Rect a{0, 0, 10, 10};
    const ckv::Rect b{5, 5, 10, 10};
    const ckv::Rect i = a.intersected(b);
    CK_CHECK(i.x == 5);
    CK_CHECK(i.y == 5);
    CK_CHECK(i.width == 5);
    CK_CHECK(i.height == 5);

    const ckv::Rect c{20, 20, 5, 5};
    CK_CHECK(a.intersected(c).empty());
}

CK_TEST(point_and_size_equality) {
    CK_CHECK((ckv::Point{1, 2}) == (ckv::Point{1, 2}));
    CK_CHECK((ckv::Point{1, 2}) != (ckv::Point{1, 3}));
    CK_CHECK((ckv::Size{4, 5}) == (ckv::Size{4, 5}));
    CK_CHECK((ckv::PixelPoint{1, 2}) == (ckv::PixelPoint{1, 2}));
    CK_CHECK((ckv::PixelSize{4, 5}) != (ckv::PixelSize{4, 6}));
}

