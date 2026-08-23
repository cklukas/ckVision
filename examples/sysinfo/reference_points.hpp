// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The comparison bars, and the rule that keeps them honest.
//
// The diagnostic tools this example takes its shape from drew your computer
// against a fixed table of other computers. Those bars are what makes a
// score mean anything -- a number with nothing beside it is not a
// measurement, it is a number -- so this program has them too. What it will
// not do is present them as measurements it took.
//
// Every reference point here is one of two things:
//
//   * a figure published in a standard (DDR4-3200 transfers 3200 MT/s over
//     a 64-bit channel), or
//   * a figure computed from published figures by arithmetic the reader can
//     redo (that channel therefore carries 3200 x 8 = 25.6 GB/s).
//
// Each carries the arithmetic that produced it and the standard it came
// from, both shown in the application, and the chart draws it in a
// different character from a measured bar. A ceiling is not a score: it is
// what the hardware can do at best, which no kernel reaches.
#pragma once

#include <string_view>
#include <vector>

#include "benchmark.hpp"

namespace ckv::sysinfo {

struct ReferencePoint {
    BenchmarkId kernel = BenchmarkId::MemoryBandwidth;
    // What the row is called on the chart.
    std::string_view label;
    // In the kernel's own unit, so it shares the measurement's index.
    double rate = 0.0;
    // The arithmetic, so a reader can redo it: "3200 MT/s x 8 B/transfer".
    std::string_view basis;
    // Where the input figures come from.
    std::string_view source;
};

// Every reference point this program knows, in no particular order.
const std::vector<ReferencePoint>& reference_points();

// Those belonging to one chart, largest first.
std::vector<ReferencePoint> reference_points_for(BenchmarkId kernel);

}  // namespace ckv::sysinfo
