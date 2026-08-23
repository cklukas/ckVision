// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The cache-latency series as a picture, for a terminal that can show one.
//
// The same data the cell chart draws in blocks, drawn again in pixels: not
// a different measurement, and not a better one -- a demonstration that a
// ckVision application asks the terminal what it can do and answers with
// whichever rendering that terminal can actually show. Where there are no
// graphics, BarChartView draws this series and nothing is missing but the
// smoothness.
//
// A pure function over an Image, so it is testable without a terminal, a
// theme or a window.
#pragma once

#include <vector>

#include "cvision/core/image.hpp"

#include "benchmark.hpp"

namespace ckv::sysinfo {

struct LatencyPlotPalette {
    Image::Rgba background{16, 24, 48, 255};
    Image::Rgba grid{40, 56, 96, 255};
    Image::Rgba curve{120, 220, 255, 255};
    Image::Rgba fill{40, 96, 144, 255};
};

// Draws `series` into `image`, which the caller has already sized. The
// vertical axis is logarithmic, because the interesting thing about this
// series is that it spans two orders of magnitude: on a linear axis every
// cache level is one flat line at the bottom and only the last bar is
// visible, which is the picture at its least informative.
//
// A series of fewer than two points is drawn as an empty grid: a curve
// through one measurement is a line through one point.
void draw_latency_plot(Image& image, const std::vector<SeriesPoint>& series,
                       const LatencyPlotPalette& palette = {});

}  // namespace ckv::sysinfo
