// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The two kernels that answer with a curve, and the picture one of them is
// drawn as. As everywhere in this example, what is asserted is what the
// code computes and never how fast it was: a pointer chase is checked for
// being a single cycle through every slot, and a plot for being a picture
// of its own data.
#include <cstdint>
#include <set>
#include <vector>

#include "cvision/core/image.hpp"
#include "cvision/testing/cktest.hpp"

#include "benchmark.hpp"
#include "latency_plot.hpp"

using ckv::Image;
using ckv::sysinfo::draw_latency_plot;
using ckv::sysinfo::LatencyPlotPalette;
using ckv::sysinfo::make_pointer_chase;
using ckv::sysinfo::SeriesPoint;
using ckv::sysinfo::walk_pointer_chase;

namespace {
// Image::Rgba is four bytes with no comparison of its own.
bool same(Image::Rgba left, Image::Rgba right) {
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}
}  // namespace

// The property the whole latency measurement rests on: one cycle through
// every slot. A permutation that broke into several cycles would leave the
// walk circling a handful of slots that stay in cache forever, and the
// chart would show a flat line and call it a memory hierarchy.
CK_TEST(the_pointer_chase_is_a_single_cycle_through_every_slot) {
    constexpr std::size_t kElements = 4096;
    const std::vector<std::uint32_t> ring = make_pointer_chase(kElements, 12345);
    CK_CHECK(ring.size() == kElements);

    std::set<std::uint32_t> visited;
    std::uint32_t at = 0;
    for (std::size_t step = 0; step < kElements; ++step) {
        at = ring[at];
        visited.insert(at);
    }
    // Every slot exactly once, and back to the start on the last step.
    CK_CHECK(visited.size() == kElements);
    CK_CHECK(at == 0);
}

CK_TEST(the_pointer_chase_is_not_a_sequential_walk) {
    constexpr std::size_t kElements = 1024;
    const std::vector<std::uint32_t> ring = make_pointer_chase(kElements, 999);
    std::size_t sequential = 0;
    for (std::size_t index = 0; index + 1 < kElements; ++index)
        if (ring[index] == index + 1) ++sequential;
    // A prefetcher follows a walk that mostly steps forward by one. A
    // handful of such steps is chance; a majority is a broken shuffle.
    CK_CHECK(sequential < kElements / 8);

    // Deterministic for a given seed, so a rerun measures the same walk.
    CK_CHECK(make_pointer_chase(kElements, 999) == ring);
    CK_CHECK(make_pointer_chase(kElements, 1000) != ring);
}

CK_TEST(a_ring_too_small_to_have_a_cycle_is_not_walked) {
    CK_CHECK(make_pointer_chase(0, 1).empty());
    CK_CHECK(make_pointer_chase(1, 1).size() == 1);
    CK_CHECK(walk_pointer_chase({}, 100) == 0);
}

CK_TEST(the_latency_plot_draws_its_own_data_and_stays_inside_its_image) {
    Image image(64, 32);
    const LatencyPlotPalette palette;
    const std::vector<SeriesPoint> series{
        SeriesPoint{"4 KiB", 1.5, "1.5 ns", 0.0},   SeriesPoint{"1 MiB", 10.0, "10.0 ns", 0.0},
        SeriesPoint{"64 MiB", 100.0, "100.0 ns", 0.0},
    };
    draw_latency_plot(image, series, palette);

    // The curve climbs: the column for the slowest working set carries a
    // curve pixel higher up the image than the column for the fastest.
    const auto highest_curve_row = [&](int x) {
        for (int y = 0; y < image.height(); ++y)
            if (same(image.pixel(x, y), palette.curve)) return y;
        return image.height();
    };
    const int fast = highest_curve_row(2);
    const int slow = highest_curve_row(image.width() - 2);
    CK_CHECK(fast < image.height());
    CK_CHECK(slow < image.height());
    CK_CHECK(slow < fast);  // higher on screen is a smaller row number
}

CK_TEST(a_plot_with_nothing_to_draw_is_an_empty_grid_rather_than_a_line) {
    Image image(32, 16);
    const LatencyPlotPalette palette;
    draw_latency_plot(image, {}, palette);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            CK_CHECK(same(image.pixel(x, y), palette.background) || same(image.pixel(x, y), palette.grid));

    // One measurement is not a curve either.
    draw_latency_plot(image, {SeriesPoint{"4 KiB", 1.5, "1.5 ns", 0.0}}, palette);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            CK_CHECK(!same(image.pixel(x, y), palette.curve));
}

CK_TEST(a_plot_of_one_repeated_value_does_not_divide_by_a_zero_span) {
    Image image(32, 16);
    const LatencyPlotPalette palette;
    const std::vector<SeriesPoint> flat{SeriesPoint{"a", 5.0, "5.0", 0.0}, SeriesPoint{"b", 5.0, "5.0", 0.0}};
    draw_latency_plot(image, flat, palette);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            CK_CHECK(same(image.pixel(x, y), palette.background) || same(image.pixel(x, y), palette.grid));
}
