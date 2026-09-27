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
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/core/frame_view.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"
#include "cvision/core/shadow_style.hpp"
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
    // The stable identity described above; unique among the layers of one
    // compose() call.
    int id = 0;
    // The layer's content. compose() dereferences it without a check, so it
    // must be non-null and stay alive for the call; the compositor keeps no
    // pointer to it afterwards.
    Surface* surface = nullptr;
    // Where the surface's top-left cell lands in the frame.
    Point position;
    // Whether the layer casts the ShadowSpec shadow onto what lies beneath.
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

    // A default layer (id 0, null surface) is only a placeholder to assign
    // into; the second form sets every field in declaration order.
    Layer() = default;
    Layer(int layer_id, Surface* layer_surface, Point layer_position, bool layer_casts_shadow,
          std::optional<Rect> layer_shadow_clip = std::nullopt,
          std::optional<Rect> layer_content_clip = std::nullopt)
        : id(layer_id), surface(layer_surface), position(layer_position),
          casts_shadow(layer_casts_shadow), shadow_clip(std::move(layer_shadow_clip)),
          content_clip(std::move(layer_content_clip)) {}
};

// How a layer's cast shadow is placed and what it does to the cells it
// covers.
struct ShadowSpec {
    // The shadow's offset from the layer in cells: a strip `dx` columns wide
    // down the right side, starting `dy` rows below the top, and a strip `dy`
    // rows tall along the bottom, starting `dx` columns in (see
    // shadow_footprint). A zero offset suppresses that strip.
    int dx = 2;  // columns right
    int dy = 1;  // rows down
    // The transform applied once to each cell the shadow covers, and handed
    // to the presenter with every picture slice it covers (RasterSlice::
    // shadow). Application composes with its theme's (ui::Theme::shadow()).
    ShadowStyle style;

    // Equal when the offset and the style are.
    friend bool operator==(const ShadowSpec&, const ShadowSpec&) = default;
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
// A slice is split at higher-layer shadow footprints. A covered slice
// carries the ShadowSpec's style, which the presenter applies to its pixels
// as the compositor applies it to the cells beside them.

// Owns the composed frame and turns a background surface plus a z-ordered
// layer list into it, re-resolving only the cells that changed since the
// previous compose(). It remembers each layer's id, rectangle and shadow from
// one call to the next so that moved, added and removed layers repaint what
// they uncovered, and the ShadowSpec, so that a change of shadow (a theme
// switched from halving to recolouring) repaints every shadowed cell.
class Compositor {
public:
    // A compositor whose frame is `frame_size` cells of blank Cells, all
    // damaged, with no previous layers and a hidden cursor.
    explicit Compositor(Size frame_size);

    // `layers` must already be in z-order, bottom to top. `background`
    // covers the whole frame; its cells are the base layer, while any
    // background rasters are correctly occluded by the supplied layers.
    // Consumes (and clears) the row damage of every surface involved —
    // background and every layer's surface.
    void compose(const std::vector<Layer>& layers, Surface& background, ShadowSpec shadow = {});

    // The composed frame's cells. It holds no raster regions of its own:
    // the pictures in it are reported by visible_rasters(). The compositor
    // never clears this surface's row damage.
    const Surface& frame() const noexcept { return frame_; }
    // Reallocates the frame at `new_size` with blank cells and forgets the
    // previous layers and visible rasters; the cursor is kept. compose()
    // stays damage-driven afterwards, so the caller must hand it surfaces
    // that report their whole content as damaged — freshly resized ones do.
    void resize(Size new_size);

    // The cursor that goes with the frame, stored as given: compose() and
    // resize() neither move, clip nor hide it.
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
    struct RasterFragment {
        Rect rect;
        bool shadowed = false;
    };

    struct PreviousLayer {
        int id;
        Rect rect;
        bool casts_shadow;
        std::optional<Rect> shadow_clip;
    };

    void compute_damage(const std::vector<Layer>& layers, const Surface& background,
                        const ShadowSpec& shadow);
    // The visible content at a frame cell, with its hyperlink target read
    // through the table of the surface it came from: link ids are surface-
    // local, so a link crosses into the frame by target.
    struct ResolvedCell {
        Cell cell;
        std::string_view link_target;
    };

    ResolvedCell resolve_cell(Point p, const std::vector<Layer>& layers, const ShadowSpec& shadow,
                              std::size_t exclusive_top, const Surface& background) const;
    void compute_visible_rasters(const std::vector<Layer>& layers, const Surface& background,
                                 const ShadowSpec& shadow);

    Surface frame_;
    CursorState cursor_;
    std::vector<PreviousLayer> previous_layers_;
    // The shadow the frame was last composed with.
    ShadowSpec previous_shadow_;
    std::vector<RasterSlice> visible_rasters_;
    // Instance-owned scratch space makes steady-state composition allocation
    // free. Capacity may grow only when the scene's layer/raster complexity
    // itself grows; no process-global cache is involved.
    std::vector<Rect> damage_;
    std::vector<Rect> clipped_damage_;
    std::vector<Rect> rect_scratch_a_;
    std::vector<Rect> rect_scratch_b_;
    std::vector<RasterFragment> raster_scratch_a_;
    std::vector<RasterFragment> raster_scratch_b_;
    std::size_t cells_touched_ = 0;
};

}  // namespace ckv::scene
