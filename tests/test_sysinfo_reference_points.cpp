// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The comparison bars. Every figure here is published or is arithmetic on
// published figures, and this suite is where that claim is checked: each
// row states the sum that produced it, and the sums are redone below.
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"

#include "benchmark.hpp"
#include "reference_points.hpp"

using ckv::sysinfo::BenchmarkId;
using ckv::sysinfo::reference_points;
using ckv::sysinfo::reference_points_for;
using ckv::sysinfo::ReferencePoint;

namespace {

const ReferencePoint* find(std::string_view label) {
    for (const ReferencePoint& point : reference_points())
        if (point.label == label) return &point;
    return nullptr;
}

}  // namespace

CK_TEST(every_reference_point_states_what_it_is_and_where_it_came_from) {
    CK_CHECK(!reference_points().empty());
    for (const ReferencePoint& point : reference_points()) {
        CK_CHECK(!point.label.empty());
        CK_CHECK(point.rate > 0.0);
        // A bar without its arithmetic and its source is exactly the thing
        // this program refuses to draw: a number nobody can check.
        CK_CHECK(!point.basis.empty());
        CK_CHECK(!point.source.empty());
    }
}

// A 64-bit channel moves eight bytes per transfer, so a generation's peak
// is its headline transfer rate times eight. Every memory row is that one
// sum, and here it is, redone.
CK_TEST(the_memory_rows_are_transfer_rate_times_eight_bytes) {
    struct Expectation {
        const char* label;
        double transfers_per_second;
    };
    const Expectation expectations[] = {
        {"PC100 SDRAM", 100e6},   {"DDR-400 (PC3200)", 400e6}, {"DDR2-800", 800e6},
        {"DDR3-1600", 1600e6},    {"DDR4-3200", 3200e6},       {"DDR5-6400", 6400e6},
    };
    for (const Expectation& expectation : expectations) {
        const ReferencePoint* const point = find(expectation.label);
        CK_CHECK(point != nullptr);
        CK_CHECK(point->kernel == BenchmarkId::MemoryBandwidth);
        CK_CHECK(point->rate == expectation.transfers_per_second * 8.0);
    }

    // And the relations that make the table a table: each JEDEC generation
    // doubles the one before it at its headline rate.
    CK_CHECK(find("DDR4-3200")->rate == 2.0 * find("DDR3-1600")->rate);
    CK_CHECK(find("DDR5-6400")->rate == 2.0 * find("DDR4-3200")->rate);
}

// Peak double-precision throughput is FLOPs per cycle times clock, and the
// first factor is a property of the vector unit: 1 for x87, 4 for SSE2, 8
// for 256-bit AVX and for a two-pipe NEON, 16 for AVX2 with FMA, 32 for
// AVX-512.
CK_TEST(the_processor_rows_are_flops_per_cycle_times_clock) {
    CK_CHECK(find("x87 core, 100 MHz")->rate == 1.0 * 100e6);
    CK_CHECK(find("SSE2 core, 2.0 GHz")->rate == 4.0 * 2.0e9);
    CK_CHECK(find("AVX core, 3.0 GHz")->rate == 8.0 * 3.0e9);
    CK_CHECK(find("NEON core, 3.2 GHz")->rate == 8.0 * 3.2e9);
    CK_CHECK(find("AVX2 FMA core, 3.0 GHz")->rate == 16.0 * 3.0e9);
    CK_CHECK(find("AVX-512 core, 3.0 GHz")->rate == 32.0 * 3.0e9);
    for (const ReferencePoint& point : reference_points_for(BenchmarkId::FloatingPoint))
        CK_CHECK(point.kernel == BenchmarkId::FloatingPoint);
}

CK_TEST(a_chart_gets_only_its_own_references_largest_first) {
    const std::vector<ReferencePoint> memory = reference_points_for(BenchmarkId::MemoryBandwidth);
    CK_CHECK(memory.size() == 6);
    for (std::size_t index = 1; index < memory.size(); ++index)
        CK_CHECK(memory[index - 1].rate > memory[index].rate);

    // The integer kernel is this program's own invention, so there is no
    // published figure for it and none is invented: it has no comparison
    // bars at all rather than a plausible-looking one.
    CK_CHECK(reference_points_for(BenchmarkId::IntegerMix).empty());
}
