// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// What a palette index names, and how to get concrete pixels out of a Color.
//
// `Color` keeps an indexed colour as an index (see color.hpp), which is the
// only representation from which a palette can still be re-themed later. This
// header is the one place that turns an index into RGB, so anything that
// genuinely needs channels — dimming a shadow, writing an SVG, quantising for
// a host with fewer colours — goes through the same table.
#pragma once

#include <array>
#include <cstdint>

#include "cvision/core/color.hpp"

namespace ckv {

// The default 256-entry terminal palette.
//
// 0-15 are the ANSI colours; the values are ckVision's own default scheme
// rather than the 1980s VGA ones, because those eight dark primaries were
// chosen for a CRT and read as muddy on a modern display. 16-231 are the
// 6x6x6 cube and 232-255 the 24-step grey ramp, both at the level sequences
// the 256-colour convention fixed and every terminal shares — a program that
// computes an index arithmetically (`16 + 36*r + 6*g + b`) depends on them.
//
// An out-of-range index resolves to entry 0 rather than reading past the
// table; `Color::indexed` is byte-valued, so this is unreachable from a
// colour that exists.
constexpr Color palette_color(int index) noexcept {
    constexpr std::array<Color, 16> ansi{
        Color::rgb(0, 0, 0),       Color::rgb(205, 49, 49),   Color::rgb(13, 188, 121),
        Color::rgb(229, 229, 16),  Color::rgb(36, 114, 200),  Color::rgb(188, 63, 188),
        Color::rgb(17, 168, 205),  Color::rgb(229, 229, 229), Color::rgb(102, 102, 102),
        Color::rgb(241, 76, 76),   Color::rgb(35, 209, 139),  Color::rgb(245, 245, 67),
        Color::rgb(59, 142, 234),  Color::rgb(214, 112, 214), Color::rgb(41, 184, 219),
        Color::rgb(255, 255, 255),
    };
    if (index < 0 || index > 255) return ansi[0];
    if (index < 16) return ansi[static_cast<std::size_t>(index)];
    if (index < 232) {
        constexpr std::array<std::uint8_t, 6> levels{{0, 95, 135, 175, 215, 255}};
        const int cube = index - 16;
        return Color::rgb(levels[static_cast<std::size_t>((cube / 36) % 6)],
                          levels[static_cast<std::size_t>((cube / 6) % 6)],
                          levels[static_cast<std::size_t>(cube % 6)]);
    }
    const auto value = static_cast<std::uint8_t>(8 + (index - 232) * 10);
    return Color::rgb(value, value, value);
}

// The concrete colour `color` stands for: `fallback` where it defers to the
// terminal's own, the palette entry where it names one, itself otherwise. The
// fallback is resolved too, so a caller may pass an indexed one; the result
// is always an RGB colour, and `r()`/`g()`/`b()` may be read from it. A
// fallback that is itself the terminal's default has no value to give, and
// the result is black.
constexpr Color resolved_color(Color color, Color fallback) noexcept {
    if (color.is_default()) color = fallback;
    if (color.is_default()) return Color::rgb(0, 0, 0);
    if (color.is_indexed()) return palette_color(color.index());
    return color;
}

}  // namespace ckv
