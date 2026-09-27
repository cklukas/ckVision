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
#include <optional>
#include <string_view>

#include "cvision/core/assert.hpp"
#include "cvision/core/cell.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/hyperlink.hpp"
#include "cvision/core/image.hpp"
#include "cvision/core/shadow_style.hpp"

namespace ckv {

// A non-owning view of a rectangular cell grid and the LinkTable its cells'
// link ids refer to. The viewed storage (typically a scene::Surface's cell
// array and link table) must outlive the view.
class FrameView {
public:
    // An empty 0x0 view over nothing; at() may not be called on it. The
    // second form views `size.width * size.height` cells stored row-major
    // from `cells`, which the caller keeps alive and unmoved for the view's
    // lifetime, with `links` resolving their link ids. A view without a
    // table holds no links: every one of its cells must carry kNoLink.
    FrameView() = default;
    FrameView(const Cell* cells, Size size, const LinkTable* links = nullptr) noexcept
        : cells_(cells), size_(size), links_(links) {}

    // The viewed grid's extent in cells.
    Size size() const noexcept { return size_; }

    // The table the cells' link ids refer to; null for a view without links.
    const LinkTable* links() const noexcept { return links_; }

    // The hyperlink target of the cell at `p` (which must lie inside size()),
    // or an empty view when it is not part of a link.
    std::string_view link_target(Point p) const noexcept {
        const LinkId link = at(p).link();
        if (link == kNoLink) return {};
        CKV_ASSERT(links_ != nullptr);
        return links_->target(link);
    }

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
    const LinkTable* links_ = nullptr;
};

// Whether the cell at `p` in `a` and the cell at `q` in `b` would look and
// act the same on a terminal: the same grapheme, style and width, and the same
// hyperlink target. Targets are compared rather than ids, so the two views may
// come from different grids — a frame and the one presented before it, or a
// composed frame and a decoded display.
inline bool same_cell(FrameView a, Point p, FrameView b, Point q) noexcept {
    const Cell& first = a.at(p);
    const Cell& second = b.at(q);
    if (!first.same_content(second)) return false;
    if (first.link() == kNoLink && second.link() == kNoLink) return true;
    return a.link_target(p) == b.link_target(q);
}

// A raster region's visible remainder after occlusion, in frame-
// absolute coordinates — the core-typed counterpart of a compositor's
// per-frame raster output. The compositor splits slices at occlusion and
// shadow boundaries so the presenter can darken only the shadowed pixels.
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
    // The shadow covering this slice, if a higher layer's shadow does: the
    // binary union of the higher-layer footprints (D-037). The presenter
    // passes every pixel of the slice through it, the same transform the
    // compositor applied to the cells beneath the same shadow (D-106).
    std::optional<ShadowStyle> shadow{};
};

}  // namespace ckv
