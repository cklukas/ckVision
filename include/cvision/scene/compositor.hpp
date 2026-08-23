// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Compositor: assembles the desktop background and z-ordered layers
// into one frame, composing only damaged regions and applying shadows
// as a compositing pass (the architecture §3).
#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "cvision/core/frame_view.hpp"
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
    // Optional frame-absolute boundary for this layer's shadow only. Desktop
    // uses this to keep window shadows out of docked chrome.
    std::optional<Rect> shadow_clip;
    // Optional frame-absolute boundary for the layer's own content. A layer
    // composites above the background, so a window dragged over a docked
    // menu bar covers it; bounding the layer instead sends the window under
    // the chrome, which is where a desktop's furniture belongs. Everything
    // that asks where a layer is -- damage, occlusion, cell resolution,
    // raster visibility -- reads it through one function, so bounding it
    // here bounds all of them.
    std::optional<Rect> content_clip;

    Layer() = default;
    Layer(int layer_id, Surface* layer_surface, Point layer_position, bool layer_casts_shadow,
          std::optional<Rect> layer_shadow_clip = std::nullopt,
          std::optional<Rect> layer_content_clip = std::nullopt)
        : id(layer_id), surface(layer_surface), position(layer_position),
          casts_shadow(layer_casts_shadow), shadow_clip(std::move(layer_shadow_clip)),
          content_clip(std::move(layer_content_clip)) {}
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

// The L-shaped footprint (a non-overlapping right strip plus bottom
// strip, each possibly empty) a shadow casts for a layer occupying
// `layer_rect`, unclipped to any frame bounds. Multiple shadows compose
// as a binary union: covered or not covered, never cumulative dimming.
std::vector<Rect> shadow_footprint(Rect layer_rect, ShadowSpec shadow) noexcept;

// Compositor produces ckv::RasterSlice (core/frame_view.hpp), zero or
// more per logical raster region, as its occlusion-sliced output — the
// core-typed hand-off to term::Presenter (the architecture §1/§4).
//
// Scope note: unlike cells, a slice here carries no shadow-dimming
// state. Cell-style shadow dimming (resolve_cell) is a style transform
// on discrete text cells; dimming raster PIXEL content needs image-
// level compositing math (or a policy decision to suppress the image
// and fall back to text under a shadow), which is presenter/term-layer
// territory once real image encoding exists — out of scope for M2/M3,
// which only establish anchoring/occlusion-slicing/fallback.

class Compositor {
public:
    explicit Compositor(Size frame_size);

    // `layers` must already be in z-order, bottom to top. `background`
    // covers the whole frame; its cells are the base layer, while any
    // background rasters are correctly occluded by the supplied layers.
    // Consumes (and clears) the row damage of every surface involved —
    // background and every layer's surface.
    void compose(const std::vector<Layer>& layers, Surface& background, ShadowSpec shadow = {});

    const Surface& frame() const noexcept { return frame_; }
    void resize(Size new_size);

    void set_cursor(CursorState cursor) noexcept { cursor_ = cursor; }
    CursorState cursor() const noexcept { return cursor_; }

    // Recomputed in full on every compose() call — occlusion slicing is
    // comparatively cheap and is not damage-gated in v1.
    const std::vector<RasterSlice>& visible_rasters() const noexcept { return visible_rasters_; }

    // Cells resolved during the most recent compose() call — the
    // concrete, machine-independent half of the compose-stage
    // performance budget (the architecture §8): proportional to damage,
    // zero when nothing changed. A hard, deterministic, CI-checkable
    // number, unlike wall-clock timing.
    std::size_t last_compose_cells_touched() const noexcept { return cells_touched_; }

private:
    struct PreviousLayer {
        int id;
        Rect rect;
        bool casts_shadow;
        std::optional<Rect> shadow_clip;
    };

    void compute_damage(const std::vector<Layer>& layers, const Surface& background,
                        const ShadowSpec& shadow);
    Cell resolve_cell(Point p, const std::vector<Layer>& layers, const ShadowSpec& shadow,
                       std::size_t exclusive_top, const Surface& background) const;
    void compute_visible_rasters(const std::vector<Layer>& layers, const Surface& background);

    Surface frame_;
    CursorState cursor_;
    std::vector<PreviousLayer> previous_layers_;
    std::vector<RasterSlice> visible_rasters_;
    // Instance-owned scratch space makes steady-state composition allocation
    // free. Capacity may grow only when the scene's layer/raster complexity
    // itself grows; no process-global cache is involved.
    std::vector<Rect> damage_;
    std::vector<Rect> clipped_damage_;
    std::vector<Rect> rect_scratch_a_;
    std::vector<Rect> rect_scratch_b_;
    std::size_t cells_touched_ = 0;
};

}  // namespace ckv::scene
