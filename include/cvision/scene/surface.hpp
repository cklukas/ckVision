// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Surface: an owned rectangular grid of Cells with per-row damage
// tracking and cell-anchored raster regions (the architecture §3).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "cvision/core/cell.hpp"
#include "cvision/core/frame_view.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/hyperlink.hpp"
#include "cvision/core/image.hpp"
#include "cvision/scene/box_drawing.hpp"

namespace ckv::scene {

class Painter;

// A row's dirty column span, as a half-open interval [lo, hi). `hi <=
// lo` means the row carries no damage.
struct DamageSpan {
    // The first damaged column and the first column past the damage.
    int lo = 0;
    int hi = 0;

    // True when the span covers no column.
    bool empty() const noexcept { return hi <= lo; }
};

// A cell-anchored raster image hosted by a Surface (the architecture
// §3/§7). `image` is never null for a region actually added to a
// Surface — Painter::draw_image is the sole construction path and
// always supplies one alongside a fallback that has already painted
// equivalent cell content into `anchor` (D-017's mandatory-fallback
// contract). Whether the picture or its fallback reaches the screen is
// the presenter's decision, from the terminal's capabilities (D-080).
//
// `id` must be positive and unique among a Surface's own raster
// regions (Surface::add_raster_region enforces both): this keeps the
// numbering space aligned with the golden dump format's own raster-id
// rule (docs/golden-format.md), so every Surface capturable via
// ckv::scene::capture() always round-trips.
struct RasterRegion {
    // The region's identity, under the rules above.
    int id = 0;
    // The cells the whole picture spans, in the hosting Surface's coordinates.
    Rect anchor;  // cell-space position + span, at full extent
    // The picture, shared and immutable; add_raster_region asserts that it is
    // non-null and has non-zero dimensions.
    std::shared_ptr<const Image> image;
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
    // Optional row-major cell coverage of the full anchor. A zero byte means
    // a child terminal has since written text over that picture cell; the
    // compositor excludes it before producing opaque Sixel slices.
    std::shared_ptr<const std::vector<std::uint8_t>> live_cells = {};
};

// Append non-overlapping rectangles of live cells, coalescing equal horizontal
// runs across adjacent rows. An empty mask means the whole visible anchor is
// live. Both `anchor` and `visible` use the same coordinate space.
void append_raster_coverage_rectangles(std::vector<Rect>& out, Rect anchor, Rect visible,
                                       std::span<const std::uint8_t> live_cells);

// A layer's or frame's drawing target: a row-major grid of Cells that records
// which columns of each row changed since the damage was last cleared, plus
// the raster regions painted onto it. Content changes through Painter or
// set_cell; the compositor reads the damage and clears it.
class Surface {
public:
    // `fill` is written to every cell; every row starts fully damaged
    // (a freshly constructed Surface has never been composed, so its
    // entire content counts as new). A negative extent, and a `fill` that
    // carries a link, are asserted against.
    explicit Surface(Size size, Cell fill = Cell{});

    // The grid's extent in cells.
    Size size() const noexcept { return size_; }

    // The cell at column p.x, row p.y; `p` must lie inside size() (asserted,
    // not clamped). The reference is invalidated by resize().
    const Cell& at(Point p) const noexcept;

    // A non-owning core::FrameView over this surface's cells and link
    // table — the hand-off point to term::Presenter, which must not depend
    // on scene (the architecture §1/§4). Valid only while this Surface is
    // alive and unmodified (no resize).
    FrameView view() const noexcept { return FrameView(cells_.data(), size_, &links_); }

    // The hyperlink targets this surface's cells refer to by id. An entry
    // lives exactly as long as some cell uses it, so the table never holds
    // more targets than the surface shows (D-088).
    const LinkTable& links() const noexcept { return links_; }
    // The link target of the cell at `p`, or an empty view when it is not
    // part of a link. `p` must lie inside size() (asserted).
    std::string_view link_target(Point p) const noexcept { return links_.target(at(p).link()); }

    // The sole public mutation entry points: always keep damage tracking
    // correct. There is no mutable at() — bypassing set_cell would
    // silently break the damage invariant. `p` must lie inside size()
    // (asserted). The cell is damaged even when the new content equals the
    // old, and any box-drawing junction Painter recorded there is forgotten.
    //
    // The first form writes a cell that is not a link: `cell` must carry
    // kNoLink (asserted), because an id from another grid would name
    // whatever this surface happens to file under the same number. The
    // second makes the written cell part of a link to `link_target`,
    // replacing whatever id `cell` carried — which is how a cell read from
    // another grid is copied with its link: `set_cell(p, view.at(q),
    // view.link_target(q))`. An empty target, or one that is not a valid
    // terminal hyperlink (is_valid_hyperlink_target), writes no link.
    void set_cell(Point p, Cell cell);
    void set_cell(Point p, Cell cell, std::string_view link_target);

    // Marks `region` (clipped to the surface) as damaged without
    // changing any cell content — for callers that know a region needs
    // redraw for a reason the cell contents alone don't reveal.
    void mark_damage(Rect region) noexcept;
    // Forgets all damage; Compositor::compose calls it on every surface it
    // consumed.
    void clear_damage() noexcept;
    // The damaged column span of `row`, which must lie in [0, height)
    // (asserted). A row's span grows to cover every damaged column, so it may
    // include undamaged columns between two changes.
    DamageSpan row_damage(int row) const noexcept;
    // Whether any row carries damage.
    bool has_damage() const noexcept;

    // Reallocates the surface; every raster region is dropped (its
    // anchor is meaningless against the new size) and every row starts
    // fully damaged again, exactly as at construction. Every cell becomes
    // `fill`, even when the size is unchanged, every link is forgotten, and
    // raster id allocation restarts at 1. `fill` must carry kNoLink.
    void resize(Size new_size, Cell fill = Cell{});

    // Assigns a positive identity unique within this Surface. Painter uses it
    // for callers that pass raster id 0, keeping ordinary widgets free of a
    // process-global id allocator.
    int allocate_raster_id() noexcept;
    // Records a region and damages its anchor. Asserts a positive id not
    // already in use here, a non-null non-empty image, and, when present, a
    // `live_cells` mask of exactly anchor.width * anchor.height entries.
    // Painter::draw_image is the ordinary way in.
    void add_raster_region(RasterRegion region);
    // Removes the region with `id` and damages its anchor; an unknown id is
    // ignored.
    void remove_raster_region(int id) noexcept;
    // Removes every region, damaging each anchor, and restarts raster id
    // allocation at 1.
    void clear_raster_regions() noexcept;

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
    // The regions currently recorded, in the order they were added.
    const std::vector<RasterRegion>& raster_regions() const noexcept { return raster_regions_; }

private:
    // Painter writes line cells with their junction metadata and applies
    // shadow coverage through the private helpers below.
    friend class Painter;

    std::size_t index(Point p) const noexcept {
        return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(size_.width) +
               static_cast<std::size_t>(p.x);
    }

    // Painter's way of writing linked text: hold_link takes one reference
    // to `target` for the duration of a paint call (kNoLink when it is empty
    // or invalid), set_cell_with_link writes a cell carrying such an id, and
    // drop_link gives the hold back.
    LinkId hold_link(std::string_view target) { return target.empty() ? kNoLink : links_.acquire(target); }
    void drop_link(LinkId link) noexcept { links_.release(link); }
    void set_cell_with_link(Point p, Cell cell, LinkId link);

    std::uint64_t create_junction_scope() noexcept;
    std::optional<Junction> junction_in_scope(Point p, std::uint64_t scope) const noexcept;
    bool begin_shadow(Point p) noexcept;
    void set_junction_cell(Point p, Cell cell, std::uint64_t scope, Junction junction);
    void set_cell_preserving_junction(Point p, Cell cell);
    void write_cell(Point p, Cell cell);

    Size size_;
    std::vector<Cell> cells_;
    // Every link id in cells_ is a live entry here, referenced once per cell
    // that carries it; write_cell keeps the counts.
    LinkTable links_;
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
