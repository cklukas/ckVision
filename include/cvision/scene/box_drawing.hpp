// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Box-drawing connector glyphs and automatic junction merging, from the
// public Unicode Box Drawing block (U+2500-257F). Scope: v1 merges
// junctions by connector shape within a single, consistent line style
// per merge point (Single/Rounded share their straight, T, and cross
// glyphs — the real Unicode design; Double and Heavy are each fully
// self-contained). Mixed-weight junctions (e.g. a heavy line meeting a
// light line) are out of v1 scope: drawing a different style over an
// existing junction re-renders it in the new style using the merged
// connector directions, discarding the old style's contribution.
#pragma once

#include <optional>
#include <string_view>

namespace ckv::scene {

enum class LineStyle : unsigned char {
    Single,
    Double,
    Rounded,
    Heavy,
};

// Which of a cell's four edges connect to a neighboring line segment.
// At least one direction must be set for junction_glyph.
struct Junction {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    friend bool operator==(const Junction&, const Junction&) = default;

    Junction operator|(const Junction& other) const noexcept {
        return Junction{up || other.up, down || other.down, left || other.left,
                         right || other.right};
    }
};

// The glyph for `j` under `style`. `j` must have at least one direction
// set (a single-direction "stub" renders as the straight-line glyph for
// that axis — the Box Drawing block has no dedicated dead-end glyphs).
std::string_view junction_glyph(Junction j, LineStyle style) noexcept;

// Reverse lookup: if `grapheme` is one of the recognized box-drawing
// glyphs (any style), returns its connector pattern — used to merge a
// newly drawn segment with whatever is already in a cell. Returns
// std::nullopt for anything else, including plain text and box-drawing
// glyphs this module doesn't recognize (mixed-weight junctions, e.g.).
std::optional<Junction> junction_of(std::string_view grapheme) noexcept;

}  // namespace ckv::scene
