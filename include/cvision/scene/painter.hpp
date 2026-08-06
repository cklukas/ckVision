// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Painter: the one drawing API (ARCHITECTURE.md §3). Widgets never
// emit escape sequences and never touch cells directly — every mutation
// to a Surface's visible content goes through here.
#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "cvision/core/cell.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"
#include "cvision/core/style.hpp"
#include "cvision/scene/box_drawing.hpp"
#include "cvision/scene/surface.hpp"

namespace ckv::scene {

// A stateless style transform (e.g. the compositor's shadow dim pass).
// A plain function pointer, not std::function: transforms used on hot
// paths must not allocate, and every current use is stateless.
using StyleTransform = Style (*)(Style) noexcept;

class Painter {
public:
    // `clip` is the absolute drawable region within `surface`; every
    // draw call additionally clips to the surface's own bounds.
    Painter(Surface& surface, Rect clip) : surface_(surface), clip_(clip) {}

    Rect clip() const noexcept { return clip_; }

    // A Painter restricted to `sub_clip` (intersected with this
    // Painter's own clip) over the same Surface — the mechanism
    // draw_image uses to keep a fallback callback inside its anchor.
    Painter clipped(Rect sub_clip) const noexcept { return Painter(surface_, clip_.intersected(sub_clip)); }

    // Fills `rect` with a repeated cell. `cell` must be width 1 (a
    // repeating wide- or zero-width pattern has no well-defined
    // meaning here); draw_text is the path for wide content.
    void fill(Rect rect, Cell cell);

    // Writes `text` starting at `pos`, one row, left to right,
    // segmenting into grapheme clusters and honoring each cluster's
    // width (a wide cluster also writes a continuation cell after it).
    // Sanitizes `text` first (D-023), so hostile input cannot reach the
    // surface as control data. A cluster that would only partially fit
    // within the clip's right edge is not drawn, and nothing after it
    // is drawn either — no partial wide glyphs, ever.
    void draw_text(Point pos, std::string_view text, Style style);

    // Straight-line runs. Each cell's connector directions merge with
    // whatever box-drawing glyph (of any style) already occupies it —
    // this is the automatic junction merging (box_drawing.hpp): drawing
    // an hline across an existing vline produces a cross or a T
    // automatically, with no separate junction API needed.
    void hline(Point pos, int length, LineStyle style, Style cell_style);
    void vline(Point pos, int length, LineStyle style, Style cell_style);

    // A rectangular frame: four corners plus the four (possibly empty)
    // interior edges, each merging with existing content exactly like
    // hline/vline. No-op if rect is smaller than 2x2.
    void draw_box(Rect rect, LineStyle style, Style cell_style);

    // Applies `transform` to the existing style of every cell in `rect`,
    // leaving each cell's grapheme untouched.
    void transform_style(Rect rect, StyleTransform transform);

    // Places a cell-anchored raster image (ARCHITECTURE.md §7). `id`
    // must be unique among this surface's raster regions (encoder cache
    // identity, term-layer concern later). `fallback` is invoked with a
    // Painter clipped to `anchor` and MUST paint equivalent cell
    // content — this is what makes the fallback mandatory by
    // construction (D-017): there is no other way to place raster
    // content, and the call always happens, so cell content is always
    // present regardless of what a later presenter decides to do with
    // the image.
    void draw_image(Rect anchor, int id, std::shared_ptr<const Image> image,
                     const std::function<void(Painter&)>& fallback);

private:
    void merge_junction(Point p, Junction contribution, LineStyle style, Style cell_style);
    void draw_line_segment(Point pos, int length, bool horizontal, LineStyle style,
                            Style cell_style);

    Surface& surface_;
    Rect clip_;
};

}  // namespace ckv::scene
