// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "latency_plot.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ckv::sysinfo {
namespace {

void fill_column(Image& image, int x, int from_y, int to_y, Image::Rgba colour) {
    const int low = std::max(0, std::min(from_y, to_y));
    const int high = std::min(image.height() - 1, std::max(from_y, to_y));
    for (int y = low; y <= high; ++y) image.set_pixel(x, y, colour);
}

}  // namespace

void draw_latency_plot(Image& image, const std::vector<SeriesPoint>& series,
                       const LatencyPlotPalette& palette) {
    const int width = image.width();
    const int height = image.height();
    if (width <= 0 || height <= 0) return;

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) image.set_pixel(x, y, palette.background);

    // Four horizontal rules, so a reader can see that the curve is climbing
    // rather than only that it is high.
    for (int rule = 1; rule < 4; ++rule) {
        const int y = height * rule / 4;
        for (int x = 0; x < width; ++x) image.set_pixel(x, y, palette.grid);
    }

    if (series.size() < 2) return;

    double lowest = series.front().value;
    double highest = series.front().value;
    for (const SeriesPoint& point : series) {
        if (!std::isfinite(point.value) || point.value <= 0.0) continue;
        lowest = std::min(lowest, point.value);
        highest = std::max(highest, point.value);
    }
    if (!(lowest > 0.0) || !(highest > lowest)) return;

    // Log scale, with a decade of headroom above and below so the extremes
    // are not drawn on the frame itself.
    const double low_log = std::log10(lowest) - 0.1;
    const double high_log = std::log10(highest) + 0.1;
    const double span = high_log - low_log;

    const std::size_t points = series.size();
    const int column_width = std::max(1, width / static_cast<int>(points));
    int previous_y = -1;
    for (std::size_t index = 0; index < points; ++index) {
        const double value = series[index].value;
        if (!std::isfinite(value) || value <= 0.0) continue;
        const double share = (std::log10(value) - low_log) / span;
        const int top = std::clamp(static_cast<int>(std::llround((1.0 - share) * (height - 1))), 0, height - 1);
        const int start_x = static_cast<int>(index) * column_width;
        const int end_x = std::min(width, start_x + column_width);

        for (int x = start_x; x < end_x; ++x) {
            // Filled beneath the curve: the eye reads area faster than it
            // reads a line's height, and the step between two cache levels
            // is the thing worth reading fast.
            fill_column(image, x, top + 1, height - 1, palette.fill);
            image.set_pixel(x, top, palette.curve);
        }
        // The riser between one tread and the next, which is where a cache
        // boundary actually is.
        if (previous_y >= 0 && start_x < width) fill_column(image, start_x, previous_y, top, palette.curve);
        previous_y = top;
    }
}

}  // namespace ckv::sysinfo
