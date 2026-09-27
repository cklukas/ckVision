// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
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
//
// A rounded corner joins a light divider exactly as a square corner does,
// because Rounded IS the light weight: the rounded form exists only for the
// four corner shapes (U+256D-2570), so a corner that a line gives a third or
// fourth direction becomes the light tee or cross that shape needs, and a
// divider meeting a rounded frame's side is the same light tee it would be
// on a square one. Only a corner that stays a corner can be round.
#pragma once

#include <optional>
#include <string_view>

namespace ckv::scene {

// The line weight and corner form of box-drawing glyphs.
enum class LineStyle : unsigned char {
    // Light lines with square corners.
    Single,
    // Double lines.
    Double,
    // Light lines with rounded corners; identical to Single except at the
    // four corner shapes.
    Rounded,
    // Heavy (bold-weight) lines.
    Heavy,
};

// Which of a cell's four edges connect to a neighboring line segment.
// At least one direction must be set for junction_glyph.
struct Junction {
    // One flag per edge of the cell; true where the line leaves the cell
    // through that edge.
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    // Memberwise equality.
    friend bool operator==(const Junction&, const Junction&) = default;

    // The union of both connector sets: every edge either one connects.
    // This is how a line drawn across an existing one becomes a T or a cross.
    Junction operator|(const Junction& other) const noexcept {
        return Junction{up || other.up, down || other.down, left || other.left,
                         right || other.right};
    }
};

// What classify_junction_glyph recognizes in a glyph.
struct JunctionGlyphInfo {
    // The glyph's connector directions and its line style.
    Junction junction;
    LineStyle style = LineStyle::Single;
};

// The glyph for `j` under `style`. `j` must have at least one direction
// set (a single-direction "stub" renders as the straight-line glyph for
// that axis — the Box Drawing block has no dedicated dead-end glyphs).
std::string_view junction_glyph(Junction j, LineStyle style) noexcept;

// Classifies a recognized glyph with both its connector topology and its
// concrete line style. Shared Single/Rounded straight and junction glyphs
// classify as Single; Rounded is distinguishable only for its four dedicated
// corner glyphs, matching Unicode's actual repertoire.
std::optional<JunctionGlyphInfo> classify_junction_glyph(std::string_view grapheme) noexcept;

// Reverse lookup: if `grapheme` is one of the recognized box-drawing
// glyphs (any style), returns its connector pattern. This is a glyph
// classification utility only: visible glyph shape carries no paint
// ownership, so Painter never uses reverse lookup to merge lines from
// a Surface. Returns std::nullopt for anything else, including plain
// text and box-drawing glyphs this module doesn't recognize
// (mixed-weight junctions, e.g.).
std::optional<Junction> junction_of(std::string_view grapheme) noexcept;

}  // namespace ckv::scene
