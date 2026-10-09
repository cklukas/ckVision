// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "cvision/widgets/progress.hpp"

namespace ckv::widgets::detail {
// Return meter unit width and the half-open activity/fill span.
inline std::array<int, 3> progress_span(int width, double fraction, bool unknown, std::int64_t phase,
                                 ProgressPresentation presentation, ProgressActivityStyle activity) {
    const int unit = presentation == ProgressPresentation::Segmented ? 2 : 1;
    const int count = width / unit + (width % unit != 0 ? 1 : 0);
    int begin = 0;
    int end = static_cast<int>(std::llround(std::clamp(fraction, 0.0, 1.0) * count));
    if (unknown) {
        const int block = std::max(1, count / 4);
        const auto mod = [](std::int64_t value, std::int64_t size) {
            return (value % size + size) % size;
        };
        if (activity == ProgressActivityStyle::Bounce) {
            const std::int64_t distance = count - block;
            const auto step = mod(phase, std::max<std::int64_t>(1, distance * 2));
            begin = static_cast<int>(step <= distance ? step : distance * 2 - step);
        } else if (activity == ProgressActivityStyle::Pulse) {
            const auto step = mod(phase, std::max<std::int64_t>(1, count * 2LL));
            end = static_cast<int>(step <= count ? step : count * 2LL - step);
            begin = 0;
        } else begin = static_cast<int>(mod(phase, count + static_cast<std::int64_t>(block))) - block;
        if (activity != ProgressActivityStyle::Pulse) end = begin + block;
    }
    return {unit, begin, end};
}
// Shared meter painting for standalone and multi-task views. No allocation.
inline void paint_progress(scene::Painter& painter, Rect area, double fraction,
                           bool unknown, std::int64_t phase, ProgressPresentation presentation,
                           ProgressActivityStyle activity, ProgressGlyphs glyphs,
                           Style track, Style fill) {
    if (area.width <= 0 || area.height <= 0) return;
    auto out = painter.clipped(area);
    out.fill(area, Cell::from_grapheme(" ", track));
    const auto [unit, begin, end] = progress_span(area.width, fraction, unknown, phase, presentation, activity);
    Style ink = fill;
    ink.fg = fill.bg;
    ink.bg = track.bg;
    constexpr std::array<std::string_view, 8> partial{" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
    const auto eighths = static_cast<std::int64_t>(std::floor(std::clamp(fraction, 0.0, 1.0) * area.width * 8.0));
    for (int x = 0; x < area.width; x += unit) {
        const bool lit = x / unit >= begin && x / unit < end;
        if (presentation == ProgressPresentation::Solid && glyphs == ProgressGlyphs::Unicode) {
            if (lit) out.fill(Rect{area.x + x, area.y, 1, 1}, Cell::from_grapheme(" ", fill));
        } else {
            std::string_view glyph = lit ? (glyphs == ProgressGlyphs::Ascii ? "#" : "█")
                                        : (glyphs == ProgressGlyphs::Ascii ? "-" : "░");
            bool filled = lit;
            if (!unknown && presentation == ProgressPresentation::Smooth && glyphs == ProgressGlyphs::Unicode) {
                const auto amount = std::clamp<std::int64_t>(eighths - x * 8LL, 0, 8);
                filled = amount > 0;
                glyph = amount == 8 ? "█" : amount == 0 ? "░" : partial[static_cast<std::size_t>(amount)];
            }
            out.draw_text(Point{area.x + x, area.y}, glyph, filled ? ink : track);
        }
    }
}
} // namespace ckv::widgets::detail
