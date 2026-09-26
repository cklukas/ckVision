// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The core-typed frame data boundary between scene (which owns
// Surface/Compositor) and term (whose Presenter must not depend on
// scene — ARCHITECTURE.md §1/§4: "The composed frame is core-typed
// frame data"). Both layers share these value types; only scene knows
// how to PRODUCE them (from a Surface/Compositor), and only term knows
// how to CONSUME them (into terminal bytes).
#pragma once

#include <memory>

#include "cvision/core/assert.hpp"
#include "cvision/core/cell.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"

namespace ckv {

// A non-owning view of a rectangular cell grid. The viewed storage
// (typically a scene::Surface's cell array) must outlive the view.
class FrameView {
public:
    // An empty 0x0 view over nothing; at() may not be called on it. The
    // second form views `size.width * size.height` cells stored row-major
    // from `cells`, which the caller keeps alive and unmoved for the view's
    // lifetime.
    FrameView() = default;
    FrameView(const Cell* cells, Size size) noexcept : cells_(cells), size_(size) {}

    // The viewed grid's extent in cells.
    Size size() const noexcept { return size_; }

    // The cell at column p.x, row p.y. Both must lie inside size(); that and
    // a non-null grid are asserted, not clamped.
    const Cell& at(Point p) const noexcept {
        CKV_ASSERT(cells_ != nullptr);
        CKV_ASSERT(p.x >= 0 && p.x < size_.width && p.y >= 0 && p.y < size_.height);
        return cells_[static_cast<std::size_t>(p.y) * static_cast<std::size_t>(size_.width) +
                       static_cast<std::size_t>(p.x)];
    }

private:
    const Cell* cells_ = nullptr;
    Size size_;
};

// A raster region's visible remainder after occlusion, in frame-
// absolute coordinates — the core-typed counterpart of a compositor's
// per-frame raster output. The compositor splits slices at occlusion and
// shadow boundaries so the presenter can dim only the shadowed pixels.
struct RasterSlice {
    // The id of the raster region this slice was cut from. Several slices
    // share an id when occlusion splits one region, and ids are unique only
    // within the Surface that hosted the region, not across a frame.
    int id = 0;
    // The cells this slice shows, and the cells the whole picture spans.
    // Together they say which part of `image` the slice crops out.
    Rect visible_rect;  // frame-absolute; a sub-rect of full_anchor
    Rect full_anchor;   // frame-absolute; the region's complete anchor
    // The complete picture, shared with the region it came from; never null
    // in a slice the compositor produced.
    std::shared_ptr<const Image> image;
    // The region's RasterRegion::fallback_active, carried over unchanged. The
    // presenter does not read it; golden frame captures record it.
    bool fallback_active = true;
    // Whether a higher layer's shadow covers this slice. The presenter then
    // halves each RGB channel of the slice's pixels, whatever style transform
    // the shadow applies to cells.
    bool shadowed = false;  // binary union of higher-layer shadow footprints
};

}  // namespace ckv
