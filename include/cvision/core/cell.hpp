// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>

#include "cvision/core/style.hpp"

namespace ckv {

// One grapheme cluster, its style, and its precomputed column width —
// the scene layer's atomic drawing unit (the architecture §2).
class Cell {
public:
    Cell() : grapheme_(" "), style_(), width_(1) {}

    // `grapheme` need not already be sanitized or a single verified
    // cluster: this factory sanitizes it (the decision log D-040 — a Cell
    // can never carry a raw control character regardless of caller
    // discipline) and keeps only the first resulting grapheme cluster,
    // since a Cell holds exactly one.
    static Cell from_grapheme(std::string_view grapheme, Style style);

    // A zero-width, empty-grapheme placeholder marking the second (and
    // further) column of a wide grapheme drawn in the preceding column.
    // Never drawn on its own — Surface/Painter/Compositor treat it as
    // "covered by the predecessor", not as its own paintable cell.
    // Distinguishable from a genuine zero-width glyph (e.g. a lone
    // combining mark with no base, which has width 0 but a non-empty
    // grapheme) by is_continuation().
    static Cell continuation(Style style) noexcept { return Cell(std::string(), style, 0); }
    bool is_continuation() const noexcept { return width_ == 0 && grapheme_.empty(); }

    std::string_view grapheme() const noexcept { return grapheme_; }
    const Style& style() const noexcept { return style_; }
    void set_style(Style style) noexcept { style_ = style; }
    int width() const noexcept { return width_; }

    friend bool operator==(const Cell&, const Cell&) = default;

private:
    Cell(std::string grapheme, Style style, int width)
        : grapheme_(std::move(grapheme)), style_(style), width_(width) {}

    std::string grapheme_;
    Style style_;
    int width_;
};

}  // namespace ckv
