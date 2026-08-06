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

// Every drawing method below takes coordinates in this Painter's own
// LOCAL space (origin at its own top-left) — the view-relative
// addressing every widget author actually wants (ARCHITECTURE.md §5:
// "easy things easy"). `clip()` and internal bookkeeping are in
// absolute Surface space; translated()/clipped() are how a parent
// hands a child its own Painter.
class Painter {
public:
    // `clip` is the ABSOLUTE drawable region within `surface`; `origin`
    // is this Painter's own top-left in absolute Surface coordinates
    // (0,0 by default: a top-level Painter's local space already is
    // absolute space). Every draw call additionally clips to the
    // surface's own bounds.
    Painter(Surface& surface, Rect clip, Point origin = {})
        : surface_(surface), clip_(clip), origin_(origin) {}

    // The absolute clip rect, still in Surface space (not translated).
    Rect clip() const noexcept { return clip_; }

    // A Painter restricted to `sub_clip` (given in THIS Painter's local
    // space, intersected with its own clip) over the same Surface, same
    // origin — the mechanism draw_image uses to keep a fallback
    // callback inside its anchor.
    Painter clipped(Rect sub_clip) const noexcept {
        return Painter(surface_, clip_.intersected(to_absolute(sub_clip)), origin_);
    }

    // A Painter for a child view: `offset` (in this Painter's local
    // space) becomes the child's new origin, and `local_clip` (also in
    // this Painter's local space) becomes its clip, intersected with
    // this Painter's own clip — a child can never paint outside its
    // parent's allowed region no matter what clip it requests.
    Painter translated(Point offset, Rect local_clip) const noexcept {
        const Point new_origin{origin_.x + offset.x, origin_.y + offset.y};
        const Rect absolute_clip = Rect{new_origin.x + local_clip.x, new_origin.y + local_clip.y,
                                         local_clip.width, local_clip.height};
        return Painter(surface_, clip_.intersected(absolute_clip), new_origin);
    }

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
    Point to_absolute(Point local) const noexcept { return {local.x + origin_.x, local.y + origin_.y}; }
    Rect to_absolute(Rect local) const noexcept {
        return Rect{local.x + origin_.x, local.y + origin_.y, local.width, local.height};
    }

    void merge_junction(Point p, Junction contribution, LineStyle style, Style cell_style);
    void draw_line_segment(Point pos, int length, bool horizontal, LineStyle style,
                            Style cell_style);

    Surface& surface_;
    Rect clip_;      // absolute
    Point origin_;   // this Painter's local (0,0) in absolute Surface space
};

}  // namespace ckv::scene
