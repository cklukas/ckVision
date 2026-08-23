// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Surface: an owned rectangular grid of Cells with per-row damage
// tracking and cell-anchored raster regions (the architecture §3).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "cvision/core/cell.hpp"
#include "cvision/core/frame_view.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"
#include "cvision/scene/box_drawing.hpp"

namespace ckv::scene {

class Painter;

// A row's dirty column span, as a half-open interval [lo, hi). `hi <=
// lo` means the row carries no damage.
struct DamageSpan {
    int lo = 0;
    int hi = 0;

    bool empty() const noexcept { return hi <= lo; }
};

// A cell-anchored raster image hosted by a Surface (the architecture
// §3/§7). `image` is never null for a region actually added to a
// Surface — Painter::draw_image is the sole construction path and
// always supplies one alongside a fallback that has already painted
// equivalent cell content into `anchor` (D-017's mandatory-fallback
// contract). `fallback_active` records which representation is
// currently authoritative — true (the default) until something later
// in the pipeline (a term-layer capability decision) says otherwise.
//
// `id` must be positive and unique among a Surface's own raster
// regions (Surface::add_raster_region enforces both): this keeps the
// numbering space aligned with the golden dump format's own raster-id
// rule (docs/golden-format.md), so every Surface capturable via
// ckv::scene::capture() always round-trips.
struct RasterRegion {
    int id = 0;
    Rect anchor;  // cell-space position + span, at full extent
    std::shared_ptr<const Image> image;
    bool fallback_active = true;
    // The part of `anchor` that may actually be drawn. An image larger than
    // the view holding it -- a picture in a terminal window the reader has
    // narrowed, say -- must be cut off at that view's edge, and only the
    // painter knows where the edge is. Kept apart from `anchor` because the
    // crop is worked out from the two: the full extent says which pixels
    // these cells stand for, and this says which of them show.
    //
    // Always states the truth; it has no "unset" value. Letting an empty
    // rect mean "all of it" reads correctly for a region nobody clipped and
    // backwards for one clipped away entirely -- which is a window narrowed
    // until the picture is off its edge, at which point the picture would
    // spring back to full size.
    Rect visible;
};

class Surface {
public:
    // `fill` is written to every cell; every row starts fully damaged
    // (a freshly constructed Surface has never been composed, so its
    // entire content counts as new).
    explicit Surface(Size size, Cell fill = Cell{});

    Size size() const noexcept { return size_; }

    const Cell& at(Point p) const noexcept;

    // A non-owning core::FrameView over this surface's cells — the
    // hand-off point to term::Presenter, which must not depend on
    // scene (the architecture §1/§4). Valid only while this Surface is
    // alive and unmodified (no resize).
    FrameView view() const noexcept { return FrameView(cells_.data(), size_); }

    // The sole mutation entry point: always keeps damage tracking
    // correct. There is no mutable at() — bypassing set_cell would
    // silently break the damage invariant.
    void set_cell(Point p, Cell cell);

    // Marks `region` (clipped to the surface) as damaged without
    // changing any cell content — for callers that know a region needs
    // redraw for a reason the cell contents alone don't reveal.
    void mark_damage(Rect region) noexcept;
    void clear_damage() noexcept;
    DamageSpan row_damage(int row) const noexcept;
    bool has_damage() const noexcept;

    // Reallocates the surface; every raster region is dropped (its
    // anchor is meaningless against the new size) and every row starts
    // fully damaged again, exactly as at construction.
    void resize(Size new_size, Cell fill = Cell{});

    // Assigns a positive identity unique within this Surface. Painter uses it
    // for callers that pass raster id 0, keeping ordinary widgets free of a
    // process-global id allocator.
    int allocate_raster_id() noexcept;
    void add_raster_region(RasterRegion region);
    void remove_raster_region(int id) noexcept;
    void clear_raster_regions() noexcept;
    void set_raster_fallback_active(int id, bool active) noexcept;

    // Paint this surface's pictures as their fallbacks instead of as
    // rasters, and record no raster regions at all.
    //
    // A picture is the one thing on a surface that a terminal pays for by
    // the pixel — a host decodes it, often in another process, before it
    // can draw anything behind it in the stream. While a window is being
    // dragged or resized there is a new position every pointer report, and
    // sending a picture to each of them costs a decode per position for
    // pixels that are wrong before they are drawn. Suppressing them makes
    // the gesture cost cells alone, which is what a terminal is fast at;
    // clearing the flag and repainting brings the picture back where the
    // gesture left it.
    void set_rasters_suppressed(bool suppressed) noexcept { rasters_suppressed_ = suppressed; }
    bool rasters_suppressed() const noexcept { return rasters_suppressed_; }
    const std::vector<RasterRegion>& raster_regions() const noexcept { return raster_regions_; }

private:
    friend class Painter;

    std::size_t index(Point p) const noexcept {
        return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(size_.width) +
               static_cast<std::size_t>(p.x);
    }

    std::uint64_t create_junction_scope() noexcept;
    std::optional<Junction> junction_in_scope(Point p, std::uint64_t scope) const noexcept;
    bool begin_shadow(Point p) noexcept;
    void set_junction_cell(Point p, Cell cell, std::uint64_t scope, Junction junction);
    void set_cell_preserving_junction(Point p, Cell cell);
    void write_cell(Point p, Cell cell);

    Size size_;
    std::vector<Cell> cells_;
    // Box-drawing connectivity is semantic paint metadata and cannot be
    // reconstructed from a visible glyph. Each cell is packed as the
    // contributing logical paint's 59-bit scope, four direction bits, and a
    // binary shadow-coverage bit. A compact parallel plane avoids separate
    // padded metadata allocations for these paint semantics.
    std::vector<std::uint64_t> junction_provenance_;
    std::uint64_t next_junction_scope_ = 1;
    std::vector<DamageSpan> row_damage_;
    std::vector<RasterRegion> raster_regions_;
    int next_raster_id_ = 1;
    bool rasters_suppressed_ = false;
};

}  // namespace ckv::scene
