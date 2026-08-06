// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Compositor: assembles the desktop background and z-ordered layers
// into one frame, composing only damaged regions and applying shadows
// as a compositing pass (ARCHITECTURE.md §3).
#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"
#include "cvision/core/style.hpp"
#include "cvision/scene/cursor.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"

namespace ckv::scene {

// One layer to be composed: a non-owning reference to a Surface at a
// frame-absolute position. `id` is the layer's stable identity across
// compose() calls — required to correctly detect moves, additions, and
// removals for damage tracking.
struct Layer {
    int id = 0;
    Surface* surface = nullptr;
    Point position;
    bool casts_shadow = false;
};

// Default shadow dimming: halves each RGB channel; a "default" (no-RGB)
// color goes to black. A placeholder ahead of the M4+ theme system,
// which will supply a semantic "shadow" role instead of this fixed
// math transform — see ShadowSpec::dim.
Style default_dim(Style style) noexcept;

struct ShadowSpec {
    int dx = 2;  // columns right
    int dy = 1;  // rows down
    StyleTransform dim = &default_dim;
};

// The L-shaped footprint (a right strip plus a bottom strip, each
// possibly empty) a shadow casts for a layer occupying `layer_rect`,
// unclipped to any frame bounds.
std::vector<Rect> shadow_footprint(Rect layer_rect, ShadowSpec shadow) noexcept;

// A raster region's visible remainder after occlusion by higher
// layers: zero or more axis-aligned slices of its full anchor
// (ARCHITECTURE.md §3/§7).
//
// Scope note: unlike cells, a visible slice here carries no shadow-
// dimming state. Cell-style shadow dimming (resolve_cell) is a style
// transform on discrete text cells; dimming raster PIXEL content needs
// image-level compositing math (or a policy decision to suppress the
// image and fall back to text under a shadow), which is presenter/
// term-layer territory once real image encoding exists — out of scope
// for M2, which only establishes anchoring/occlusion-slicing/fallback.
struct VisibleRaster {
    int id;
    Rect visible_rect;  // frame-absolute; a sub-rect of full_anchor
    Rect full_anchor;   // frame-absolute; the region's complete anchor
    std::shared_ptr<const Image> image;
    bool fallback_active;
};

class Compositor {
public:
    explicit Compositor(Size frame_size);

    // `layers` must already be in z-order, bottom to top. `background`
    // covers the whole frame; it is never occluded and never
    // shadow-cast. Consumes (and clears) the row damage of every
    // surface involved — background and every layer's surface.
    void compose(const std::vector<Layer>& layers, Surface& background, ShadowSpec shadow = {});

    const Surface& frame() const noexcept { return frame_; }
    void resize(Size new_size);

    void set_cursor(CursorState cursor) noexcept { cursor_ = cursor; }
    CursorState cursor() const noexcept { return cursor_; }

    // Recomputed in full on every compose() call — occlusion slicing is
    // comparatively cheap and is not damage-gated in v1.
    const std::vector<VisibleRaster>& visible_rasters() const noexcept { return visible_rasters_; }

    // Cells resolved during the most recent compose() call — the
    // concrete, machine-independent half of the compose-stage
    // performance budget (ARCHITECTURE.md §8): proportional to damage,
    // zero when nothing changed. A hard, deterministic, CI-checkable
    // number, unlike wall-clock timing.
    std::size_t last_compose_cells_touched() const noexcept { return cells_touched_; }

private:
    struct PreviousLayer {
        int id;
        Rect rect;
        bool casts_shadow;
    };

    std::vector<Rect> compute_damage(const std::vector<Layer>& layers, const Surface& background,
                                      const ShadowSpec& shadow) const;
    Cell resolve_cell(Point p, const std::vector<Layer>& layers, const ShadowSpec& shadow,
                       std::size_t exclusive_top, const Surface& background) const;
    void compute_visible_rasters(const std::vector<Layer>& layers);

    Surface frame_;
    CursorState cursor_;
    std::vector<PreviousLayer> previous_layers_;
    std::vector<VisibleRaster> visible_rasters_;
    std::size_t cells_touched_ = 0;
};

}  // namespace ckv::scene
