// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>

#include "cvision/core/style.hpp"

namespace ckv {

// One grapheme cluster, its style, and its precomputed column width —
// the scene layer's atomic drawing unit (ARCHITECTURE.md §2).
class Cell {
public:
    // A blank: one space in the default Style, one column wide.
    Cell() : grapheme_(" "), style_(), width_(1) {}

    // `grapheme` need not already be sanitized or a single verified
    // cluster: this factory sanitizes it (DECISIONS.md D-040 — a Cell
    // can never carry a raw control character regardless of caller
    // discipline) and keeps only the first resulting grapheme cluster,
    // since a Cell holds exactly one. An empty `grapheme` yields a space. The
    // width is computed from the kept cluster: 0, 1 or 2 columns.
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

    // The cell's UTF-8 grapheme cluster; empty only for a continuation. The
    // view points into this Cell and is invalidated when the Cell changes or
    // is destroyed.
    std::string_view grapheme() const noexcept { return grapheme_; }
    // The style the grapheme is drawn in. set_style replaces it and leaves
    // the grapheme and width untouched, which is how style-only passes such
    // as shadows restyle existing content.
    const Style& style() const noexcept { return style_; }
    void set_style(Style style) noexcept { style_ = style; }
    // Columns the grapheme occupies: 1 or 2 for ordinary content, 0 for a
    // continuation or a lone zero-width cluster.
    int width() const noexcept { return width_; }

    // Equal when grapheme, style and width all match.
    friend bool operator==(const Cell&, const Cell&) = default;

private:
    Cell(std::string grapheme, Style style, int width)
        : grapheme_(std::move(grapheme)), style_(style), width_(width) {}

    std::string grapheme_;
    Style style_;
    int width_;
};

}  // namespace ckv
