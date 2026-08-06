// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Surface: an owned rectangular grid of Cells with per-row damage
// tracking and cell-anchored raster regions (ARCHITECTURE.md §3).
#pragma once

#include <memory>
#include <vector>

#include "cvision/core/cell.hpp"
#include "cvision/core/frame_view.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"

namespace ckv::scene {

// A row's dirty column span, as a half-open interval [lo, hi). `hi <=
// lo` means the row carries no damage.
struct DamageSpan {
    int lo = 0;
    int hi = 0;

    bool empty() const noexcept { return hi <= lo; }
};

// A cell-anchored raster image hosted by a Surface (ARCHITECTURE.md
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
    Rect anchor;  // cell-space position + span
    std::shared_ptr<const Image> image;
    bool fallback_active = true;
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
    // scene (ARCHITECTURE.md §1/§4). Valid only while this Surface is
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

    void add_raster_region(RasterRegion region);
    void remove_raster_region(int id) noexcept;
    void set_raster_fallback_active(int id, bool active) noexcept;
    const std::vector<RasterRegion>& raster_regions() const noexcept { return raster_regions_; }

private:
    std::size_t index(Point p) const noexcept {
        return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(size_.width) +
               static_cast<std::size_t>(p.x);
    }

    Size size_;
    std::vector<Cell> cells_;
    std::vector<DamageSpan> row_damage_;
    std::vector<RasterRegion> raster_regions_;
};

}  // namespace ckv::scene
