// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/scene/rect_ops.hpp"

#include <algorithm>
#include <numeric>

#include "cvision/testing/cktest.hpp"

namespace {

long long area(const ckv::Rect& r) { return static_cast<long long>(r.width) * r.height; }

long long total_area(const std::vector<ckv::Rect>& rects) {
    return std::accumulate(rects.begin(), rects.end(), 0LL,
                            [](long long sum, const ckv::Rect& r) { return sum + area(r); });
}

bool contains_point(const std::vector<ckv::Rect>& rects, ckv::Point p) {
    return std::any_of(rects.begin(), rects.end(), [p](const ckv::Rect& r) { return r.contains(p); });
}

}  // namespace

CK_TEST(subtract_rect_no_overlap_returns_original) {
    const ckv::Rect from{0, 0, 10, 10};
    const ckv::Rect cut{20, 20, 5, 5};
    const auto result = ckv::scene::subtract_rect(from, cut);
    CK_CHECK(result.size() == 1);
    CK_CHECK(result[0] == from);
}

CK_TEST(subtract_rect_full_cover_returns_empty) {
    const ckv::Rect from{0, 0, 10, 10};
    const ckv::Rect cut{-5, -5, 20, 20};
    const auto result = ckv::scene::subtract_rect(from, cut);
    CK_CHECK(result.empty());
}

CK_TEST(subtract_rect_interior_cut_covers_exact_remaining_area) {
    const ckv::Rect from{0, 0, 10, 10};
    const ckv::Rect cut{3, 3, 4, 4};
    const auto result = ckv::scene::subtract_rect(from, cut);
    CK_CHECK(result.size() == 4);
    CK_CHECK(total_area(result) == area(from) - area(cut.intersected(from)));
    // No remaining piece should contain any point inside the cut.
    for (int y = 3; y < 7; ++y)
        for (int x = 3; x < 7; ++x) CK_CHECK(!contains_point(result, ckv::Point{x, y}));
    // Every point outside the cut but inside `from` must be covered.
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            if (!cut.contains(ckv::Point{x, y})) CK_CHECK(contains_point(result, ckv::Point{x, y}));
}

CK_TEST(subtract_rect_corner_overlap) {
    const ckv::Rect from{0, 0, 10, 10};
    const ckv::Rect cut{-5, -5, 10, 10};  // overlaps only the top-left 5x5 corner
    const auto result = ckv::scene::subtract_rect(from, cut);
    CK_CHECK(total_area(result) == 100 - 25);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x) CK_CHECK(!contains_point(result, ckv::Point{x, y}));
}

CK_TEST(subtract_rects_multiple_cuts) {
    const ckv::Rect from{0, 0, 10, 10};
    const std::vector<ckv::Rect> cuts{{0, 0, 3, 10}, {7, 0, 3, 10}};  // left and right strips
    const auto result = ckv::scene::subtract_rects(from, cuts);
    CK_CHECK(total_area(result) == 40);  // middle 4-column strip, 4*10
    for (int y = 0; y < 10; ++y) {
        CK_CHECK(!contains_point(result, ckv::Point{0, y}));
        CK_CHECK(!contains_point(result, ckv::Point{9, y}));
        CK_CHECK(contains_point(result, ckv::Point{5, y}));
    }
}

CK_TEST(subtract_rects_empty_cut_list_returns_original) {
    const ckv::Rect from{1, 2, 3, 4};
    const auto result = ckv::scene::subtract_rects(from, {});
    CK_CHECK(result.size() == 1);
    CK_CHECK(result[0] == from);
}

CK_TEST(subtract_rects_sequential_full_cover) {
    const ckv::Rect from{0, 0, 10, 10};
    const std::vector<ckv::Rect> cuts{{0, 0, 5, 10}, {5, 0, 5, 10}};  // together cover all of `from`
    const auto result = ckv::scene::subtract_rects(from, cuts);
    CK_CHECK(result.empty());
}
