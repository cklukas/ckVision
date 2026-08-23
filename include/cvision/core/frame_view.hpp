// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The core-typed frame data boundary between scene (which owns
// Surface/Compositor) and term (whose Presenter must not depend on
// scene — the architecture §1/§4: "The composed frame is core-typed
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
    FrameView() = default;
    FrameView(const Cell* cells, Size size) noexcept : cells_(cells), size_(size) {}

    Size size() const noexcept { return size_; }

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
// per-frame raster output. See scene/compositor.hpp for how this is
// produced (occlusion slicing) and the shadow-dimming scope note.
struct RasterSlice {
    int id = 0;
    Rect visible_rect;  // frame-absolute; a sub-rect of full_anchor
    Rect full_anchor;   // frame-absolute; the region's complete anchor
    std::shared_ptr<const Image> image;
    bool fallback_active = true;
};

}  // namespace ckv
