// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/scene/box_drawing.hpp"

#include <array>

#include "cvision/core/assert.hpp"

namespace ckv::scene {
namespace {

struct Entry {
    Junction junction;
    LineStyle style;
    std::string_view glyph;
};

constexpr Junction kHoriz{false, false, true, true};
constexpr Junction kVert{true, true, false, false};
constexpr Junction kDownRight{false, true, false, true};
constexpr Junction kDownLeft{false, true, true, false};
constexpr Junction kUpRight{true, false, false, true};
constexpr Junction kUpLeft{true, false, true, false};
constexpr Junction kTeeRight{true, true, false, true};   // up+down+right
constexpr Junction kTeeLeft{true, true, true, false};    // up+down+left
constexpr Junction kTeeDown{false, true, true, true};    // down+left+right
constexpr Junction kTeeUp{true, false, true, true};      // up+left+right
constexpr Junction kCross{true, true, true, true};

// Single (light) and Rounded share every glyph except the four corners
// (the real Unicode Box Drawing design: only U+256D-2570 are dedicated
// rounded-corner glyphs). Double and Heavy are each fully self-
// contained. All codepoints fall in the BMP Box Drawing block
// (U+2500-257F), verified against the public Unicode block chart.
constexpr std::array<Entry, 37> kTable{{
    // Single / Rounded shared shapes.
    {kHoriz, LineStyle::Single, "─"},
    {kVert, LineStyle::Single, "│"},
    {kTeeRight, LineStyle::Single, "├"},
    {kTeeLeft, LineStyle::Single, "┤"},
    {kTeeDown, LineStyle::Single, "┬"},
    {kTeeUp, LineStyle::Single, "┴"},
    {kCross, LineStyle::Single, "┼"},
    // Single square corners.
    {kDownRight, LineStyle::Single, "┌"},
    {kDownLeft, LineStyle::Single, "┐"},
    {kUpRight, LineStyle::Single, "└"},
    {kUpLeft, LineStyle::Single, "┘"},
    // Rounded corners (straight/T/cross reuse the Single entries above
    // via the style-agnostic reverse lookup and the style-normalizing
    // forward lookup below).
    {kDownRight, LineStyle::Rounded, "╭"},
    {kDownLeft, LineStyle::Rounded, "╮"},
    {kUpRight, LineStyle::Rounded, "╰"},
    {kUpLeft, LineStyle::Rounded, "╯"},
    // Double.
    {kHoriz, LineStyle::Double, "═"},
    {kVert, LineStyle::Double, "║"},
    {kDownRight, LineStyle::Double, "╔"},
    {kDownLeft, LineStyle::Double, "╗"},
    {kUpRight, LineStyle::Double, "╚"},
    {kUpLeft, LineStyle::Double, "╝"},
    {kTeeRight, LineStyle::Double, "╠"},
    {kTeeLeft, LineStyle::Double, "╣"},
    {kTeeDown, LineStyle::Double, "╦"},
    {kTeeUp, LineStyle::Double, "╩"},
    {kCross, LineStyle::Double, "╬"},
    // Heavy.
    {kHoriz, LineStyle::Heavy, "━"},
    {kVert, LineStyle::Heavy, "┃"},
    {kDownRight, LineStyle::Heavy, "┏"},
    {kDownLeft, LineStyle::Heavy, "┓"},
    {kUpRight, LineStyle::Heavy, "┗"},
    {kUpLeft, LineStyle::Heavy, "┛"},
    {kTeeRight, LineStyle::Heavy, "┣"},
    {kTeeLeft, LineStyle::Heavy, "┫"},
    {kTeeDown, LineStyle::Heavy, "┳"},
    {kTeeUp, LineStyle::Heavy, "┻"},
    {kCross, LineStyle::Heavy, "╋"},
}};

// Normalizes a one-direction "stub" (no dedicated dead-end glyph exists
// in the Box Drawing block) onto the straight-line glyph for its axis.
Junction normalize(Junction j) noexcept {
    const int count = (j.up ? 1 : 0) + (j.down ? 1 : 0) + (j.left ? 1 : 0) + (j.right ? 1 : 0);
    if (count == 1) {
        if (j.up || j.down) return kVert;
        return kHoriz;
    }
    return j;
}

}  // namespace

std::string_view junction_glyph(Junction j, LineStyle style) noexcept {
    CKV_ASSERT(j.up || j.down || j.left || j.right);
    const Junction normalized = normalize(j);
    // Rounded reuses Single's straight/T/cross entries; only look up
    // Rounded directly for the four corner shapes it owns.
    const bool is_corner = (normalized == kDownRight || normalized == kDownLeft ||
                             normalized == kUpRight || normalized == kUpLeft);
    const LineStyle table_style = (style == LineStyle::Rounded && !is_corner)
                                       ? LineStyle::Single
                                       : style;
    for (const Entry& e : kTable)
        if (e.junction == normalized && e.style == table_style) return e.glyph;
    CKV_ASSERT(false);  // every normalized pattern has an entry in every style
    return "?";
}

std::optional<Junction> junction_of(std::string_view grapheme) noexcept {
    const std::optional<JunctionGlyphInfo> info = classify_junction_glyph(grapheme);
    if (!info) return std::nullopt;
    return info->junction;
}

std::optional<JunctionGlyphInfo> classify_junction_glyph(std::string_view grapheme) noexcept {
    for (const Entry& e : kTable)
        if (e.glyph == grapheme) return JunctionGlyphInfo{e.junction, e.style};
    return std::nullopt;
}

}  // namespace ckv::scene
