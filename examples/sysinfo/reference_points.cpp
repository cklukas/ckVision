// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "reference_points.hpp"

#include <algorithm>

namespace ckv::sysinfo {

const std::vector<ReferencePoint>& reference_points() {
    // The memory rows are one arithmetic rule applied to each generation's
    // headline transfer rate: a 64-bit channel moves eight bytes per
    // transfer, so MT/s x 8 is the channel's peak in MB/s. That is why
    // DDR4-3200 is 25.6 GB/s and DDR5-6400 is 51.2 -- the same sum, twice.
    //
    // The processor rows are the other standard arithmetic of this trade:
    // peak double-precision throughput is (FLOPs per cycle) x clock, and
    // the first factor is a property of the vector unit -- 1 for x87, 4 for
    // SSE2, 8 for 256-bit AVX or a two-pipe NEON, 16 for AVX2 with FMA, 32
    // for AVX-512. The clocks are round numbers chosen to span the era, not
    // claims about any particular product.
    static const std::vector<ReferencePoint> points = {
        {BenchmarkId::MemoryBandwidth, "PC100 SDRAM", 0.8e9, "100 MT/s x 8 B/transfer",
         "JEDEC PC100 SDRAM"},
        {BenchmarkId::MemoryBandwidth, "DDR-400 (PC3200)", 3.2e9, "400 MT/s x 8 B/transfer",
         "JEDEC DDR SDRAM"},
        {BenchmarkId::MemoryBandwidth, "DDR2-800", 6.4e9, "800 MT/s x 8 B/transfer", "JEDEC DDR2 (JESD79-2)"},
        {BenchmarkId::MemoryBandwidth, "DDR3-1600", 12.8e9, "1600 MT/s x 8 B/transfer",
         "JEDEC DDR3 (JESD79-3)"},
        {BenchmarkId::MemoryBandwidth, "DDR4-3200", 25.6e9, "3200 MT/s x 8 B/transfer",
         "JEDEC DDR4 (JESD79-4)"},
        {BenchmarkId::MemoryBandwidth, "DDR5-6400", 51.2e9, "6400 MT/s x 8 B/transfer",
         "JEDEC DDR5 (JESD79-5)"},

        {BenchmarkId::FloatingPoint, "x87 core, 100 MHz", 0.1e9, "1 FLOP/cycle x 100 MHz",
         "x87 scalar FPU"},
        {BenchmarkId::FloatingPoint, "SSE2 core, 2.0 GHz", 8.0e9, "4 FLOP/cycle x 2.0 GHz",
         "SSE2, 128-bit, two double-precision lanes"},
        {BenchmarkId::FloatingPoint, "AVX core, 3.0 GHz", 24.0e9, "8 FLOP/cycle x 3.0 GHz",
         "AVX, 256-bit, four double-precision lanes"},
        {BenchmarkId::FloatingPoint, "NEON core, 3.2 GHz", 25.6e9, "8 FLOP/cycle x 3.2 GHz",
         "NEON, two 128-bit FMA pipes"},
        {BenchmarkId::FloatingPoint, "AVX2 FMA core, 3.0 GHz", 48.0e9, "16 FLOP/cycle x 3.0 GHz",
         "AVX2 with FMA, two 256-bit FMA pipes"},
        {BenchmarkId::FloatingPoint, "AVX-512 core, 3.0 GHz", 96.0e9, "32 FLOP/cycle x 3.0 GHz",
         "AVX-512, two 512-bit FMA pipes"},
    };
    return points;
}

std::vector<ReferencePoint> reference_points_for(BenchmarkId kernel) {
    std::vector<ReferencePoint> selected;
    for (const ReferencePoint& point : reference_points())
        if (point.kernel == kernel) selected.push_back(point);
    std::sort(selected.begin(), selected.end(),
              [](const ReferencePoint& left, const ReferencePoint& right) { return left.rate > right.rate; });
    return selected;
}

}  // namespace ckv::sysinfo
