// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>

#include "cvision/core/hyperlink.hpp"
#include "cvision/core/style.hpp"

namespace ckv {

// One grapheme cluster, its style, its precomputed column width, and the
// hyperlink it belongs to, if any — the scene layer's atomic drawing unit
// (the architecture §2).
class Cell {
public:
    // A blank: one space in the default Style, one column wide.
    Cell() : grapheme_(" "), style_(), width_(1) {}

    // `grapheme` need not already be sanitized or a single verified
    // cluster: this factory sanitizes it (the decision log D-040 — a Cell
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

    // The hyperlink the cell is part of, as an id into the LinkTable of the
    // grid that holds it (Surface::links(), FrameView::links()); kNoLink for
    // a cell that is not a link, which is what every factory above makes.
    // The id means nothing on its own: a cell moving to another grid takes
    // its link along by target, never by id (Surface::set_cell). set_link
    // replaces the id and leaves everything else untouched; a grid asserts
    // that an id it is handed is one of its own.
    LinkId link() const noexcept { return link_; }
    void set_link(LinkId link) noexcept { link_ = link; }

    // Whether grapheme, style and width match, whatever the links: how a
    // cell from one grid is compared with a cell from another, whose link ids
    // cannot be compared (same_cell in frame_view.hpp adds the targets).
    bool same_content(const Cell& other) const noexcept {
        return width_ == other.width_ && style_ == other.style_ && grapheme_ == other.grapheme_;
    }

    // Equal when grapheme, style, width and link id all match — a comparison
    // of two cells of one grid. Across grids, use same_cell (frame_view.hpp).
    friend bool operator==(const Cell&, const Cell&) = default;

private:
    Cell(std::string grapheme, Style style, int width)
        : grapheme_(std::move(grapheme)), style_(style), width_(width) {}

    std::string grapheme_;
    Style style_;
    int width_;
    // Occupies the padding after width_, so a Cell is no larger for having it.
    LinkId link_ = kNoLink;
};

}  // namespace ckv
