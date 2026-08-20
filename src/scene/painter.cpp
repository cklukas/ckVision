// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Every public method translates its LOCAL-space parameters to
// absolute Surface space once, at the boundary, via to_absolute(); the
// private helpers (merge_junction, draw_line_segment) then operate
// purely in absolute space, unchanged by the local/absolute split.
#include "cvision/scene/painter.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"

namespace ckv::scene {

void Painter::fill(Rect rect, Cell cell) {
    CKV_ASSERT(cell.width() == 1);
    const Rect r = to_absolute(rect).intersected(clip_).intersected(
        Rect{0, 0, surface_.size().width, surface_.size().height});
    for (int y = r.top(); y < r.bottom(); ++y)
        for (int x = r.left(); x < r.right(); ++x) surface_.set_cell(Point{x, y}, cell);
}

void Painter::draw_text(Point pos, std::string_view text, Style style) {
    const Point abs_pos = to_absolute(pos);
    if (abs_pos.y < 0 || abs_pos.y >= surface_.size().height) return;
    if (abs_pos.y < clip_.top() || abs_pos.y >= clip_.bottom()) return;

    const std::string sanitized = text::sanitize_display_text(text);
    const int right_bound = std::min(clip_.right(), surface_.size().width);
    const int left_bound = std::max(clip_.left(), 0);
    int x = abs_pos.x;
    std::size_t byte_pos = 0;

    while (byte_pos < sanitized.size()) {
        const std::size_t end = text::grapheme_end(sanitized, byte_pos);
        const std::string_view grapheme = std::string_view(sanitized).substr(byte_pos, end - byte_pos);
        byte_pos = end;
        const int width = text::grapheme_width(grapheme);

        if (width > 0 && x + width > right_bound) break;  // would not fully fit: stop, draw nothing more
        if (width == 0 && x >= right_bound) break;

        if (x >= left_bound) {
            // Cell::from_grapheme re-sanitizes and re-segments an
            // already-clean, already-single-cluster grapheme — a
            // no-op in practice, kept deliberately rather than adding
            // a bypass constructor: from_grapheme is the sole
            // sanctioned way to construct content-bearing cells
            // (D-040's "enforced by type" guarantee stays uniform).
            surface_.set_cell(Point{x, abs_pos.y}, Cell::from_grapheme(grapheme, style));
            for (int c = 1; c < width; ++c)
                surface_.set_cell(Point{x + c, abs_pos.y}, Cell::continuation(style));
        }
        x += width;
    }
}

void Painter::merge_junction(Point p, Junction contribution, LineStyle style, Style cell_style) {
    // `p` is already absolute — every caller translates before calling.
    if (p.x < 0 || p.x >= surface_.size().width || p.y < 0 || p.y >= surface_.size().height) return;
    if (!clip_.contains(p)) return;

    Junction merged = contribution;
    if (const std::optional<Junction> existing = surface_.junction_in_scope(p, junction_scope_))
        merged = merged | *existing;
    surface_.set_junction_cell(
        p, Cell::from_grapheme(junction_glyph(merged, style), cell_style), junction_scope_, merged);
}

void Painter::draw_line_segment(Point pos, int length, bool horizontal, LineStyle style,
                                 Style cell_style) {
    // `pos` is already absolute — every caller translates before calling.
    if (length <= 0) return;
    for (int i = 0; i < length; ++i) {
        const Point p = horizontal ? Point{pos.x + i, pos.y} : Point{pos.x, pos.y + i};
        // Endpoint-aware: a cell contributes a direction only if this
        // call's own line actually continues that way. Without this, a
        // line's own endpoint would falsely claim to extend past its
        // own span — merging an hline's left endpoint into an existing
        // vline would wrongly produce a 4-way cross instead of a
        // T-junction. A length-1 segment has no interior to distinguish
        // an endpoint from, so it keeps the old both-directions stub
        // behavior (normalize() renders it as a plain straight glyph
        // when nothing else is there to merge with).
        Junction contribution;
        if (horizontal) {
            contribution.left = (i > 0) || (length == 1);
            contribution.right = (i < length - 1) || (length == 1);
        } else {
            contribution.up = (i > 0) || (length == 1);
            contribution.down = (i < length - 1) || (length == 1);
        }
        merge_junction(p, contribution, style, cell_style);
    }
}

void Painter::hline(Point pos, int length, LineStyle style, Style cell_style) {
    draw_line_segment(to_absolute(pos), length, /*horizontal=*/true, style, cell_style);
}

void Painter::vline(Point pos, int length, LineStyle style, Style cell_style) {
    draw_line_segment(to_absolute(pos), length, /*horizontal=*/false, style, cell_style);
}

void Painter::draw_box(Rect local_rect, LineStyle style, Style cell_style) {
    if (local_rect.width < 2 || local_rect.height < 2) return;
    const Rect rect = to_absolute(local_rect);

    merge_junction(Point{rect.left(), rect.top()}, Junction{false, true, false, true}, style,
                    cell_style);
    merge_junction(Point{rect.right() - 1, rect.top()}, Junction{false, true, true, false}, style,
                    cell_style);
    merge_junction(Point{rect.left(), rect.bottom() - 1}, Junction{true, false, false, true}, style,
                    cell_style);
    merge_junction(Point{rect.right() - 1, rect.bottom() - 1}, Junction{true, false, true, false},
                    style, cell_style);

    if (rect.width > 2) {
        draw_line_segment(Point{rect.left() + 1, rect.top()}, rect.width - 2, true, style,
                           cell_style);
        draw_line_segment(Point{rect.left() + 1, rect.bottom() - 1}, rect.width - 2, true, style,
                           cell_style);
    }
    if (rect.height > 2) {
        draw_line_segment(Point{rect.left(), rect.top() + 1}, rect.height - 2, false, style,
                           cell_style);
        draw_line_segment(Point{rect.right() - 1, rect.top() + 1}, rect.height - 2, false, style,
                           cell_style);
    }
}

void Painter::transform_style(Rect rect, StyleTransform transform) {
    const Rect r = to_absolute(rect).intersected(clip_).intersected(
        Rect{0, 0, surface_.size().width, surface_.size().height});
    for (int y = r.top(); y < r.bottom(); ++y) {
        for (int x = r.left(); x < r.right(); ++x) {
            const Point p{x, y};
            Cell updated = surface_.at(p);
            updated.set_style(transform(updated.style()));
            // A style-only pass (for example a window shadow) must not
            // erase the line ownership beneath it. If the owning view
            // resumes drawing in the same scope, its own junctions still
            // merge; a different view still replaces them.
            surface_.set_cell_preserving_junction(p, updated);
        }
    }
}

void Painter::apply_shadow(Rect rect, StyleTransform transform) {
    const Rect r = to_absolute(rect).intersected(clip_).intersected(
        Rect{0, 0, surface_.size().width, surface_.size().height});
    for (int y = r.top(); y < r.bottom(); ++y) {
        for (int x = r.left(); x < r.right(); ++x) {
            const Point p{x, y};
            if (!surface_.begin_shadow(p)) continue;
            Cell updated = surface_.at(p);
            updated.set_style(transform(updated.style()));
            surface_.set_cell_preserving_junction(p, updated);
        }
    }
}

void Painter::draw_image(Rect anchor, int id, std::shared_ptr<const Image> image,
                          const std::function<void(Painter&)>& fallback) {
    CKV_ASSERT(image != nullptr);
    CKV_ASSERT(image->width() > 0 && image->height() > 0);
    Painter fallback_painter = clipped(anchor);  // clipped() already translates via to_absolute
    fallback(fallback_painter);
    // Recorded at full extent, but only as far as this painter may draw.
    // Without the second rect an image simply overhangs the view that drew
    // it: a Sixel keeps painting past a window's frame and out onto the
    // desktop, because nothing downstream knows where that view ended.
    // Suppressed: the fallback above is the whole of this picture for now.
    // Recording no region is what keeps the raster off the wire — and out
    // of the compositor's occlusion work — rather than merely unpainted.
    if (surface_.rasters_suppressed()) return;
    const Rect absolute = to_absolute(anchor);
    const Rect visible = absolute.intersected(clip_);
    // Nothing of it falls inside this painter, so there is nothing to
    // record. The fallback above has already run, clipped to the same
    // nothing.
    if (visible.empty()) return;
    surface_.add_raster_region(RasterRegion{id == 0 ? surface_.allocate_raster_id() : id, absolute,
                                            std::move(image), true, visible});
}

}  // namespace ckv::scene
