// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/scene/compositor.hpp"

#include <algorithm>
#include <cstdint>

#include "cvision/core/palette.hpp"
#include "cvision/scene/rect_ops.hpp"

namespace ckv::scene {
namespace {

void accumulate_surface_damage(std::vector<Rect>& damage, const Surface& surface, Point offset) {
    for (int row = 0; row < surface.size().height; ++row) {
        const DamageSpan span = surface.row_damage(row);
        if (span.empty()) continue;
        damage.push_back(Rect{offset.x + span.lo, offset.y + row, span.hi - span.lo, 1});
    }
}

Rect layer_rect(const Layer& l) {
    const Rect full{l.position.x, l.position.y, l.surface->size().width, l.surface->size().height};
    return l.content_clip ? full.intersected(*l.content_clip) : full;
}

void append_shadow_footprint(Rect layer, ShadowSpec shadow, const std::optional<Rect>& shadow_clip,
                             std::vector<Rect>& out) {
    const auto append = [&out, &shadow_clip](Rect footprint) {
        if (shadow_clip) footprint = footprint.intersected(*shadow_clip);
        if (!footprint.empty()) out.push_back(footprint);
    };
    const int right_height = std::max(0, layer.height - shadow.dy);
    if (shadow.dx > 0 && right_height > 0)
        append(Rect{layer.right(), layer.top() + shadow.dy, shadow.dx, right_height});
    if (shadow.dy > 0)
        append(Rect{layer.left() + shadow.dx, layer.bottom(), layer.width, shadow.dy});
}

bool shadow_covers(Rect layer, ShadowSpec shadow, const std::optional<Rect>& shadow_clip,
                   Point p) noexcept {
    if (shadow_clip && !shadow_clip->contains(p)) return false;
    const int right_height = std::max(0, layer.height - shadow.dy);
    const Rect right{layer.right(), layer.top() + shadow.dy, shadow.dx, right_height};
    const Rect bottom{layer.left() + shadow.dx, layer.bottom(), layer.width, shadow.dy};
    return right.contains(p) || bottom.contains(p);
}

void append_difference(Rect from, Rect cut, std::vector<Rect>& out) {
    const Rect overlap = from.intersected(cut);
    if (overlap.empty()) {
        out.push_back(from);
        return;
    }
    if (overlap.top() > from.top())
        out.push_back(Rect{from.x, from.y, from.width, overlap.top() - from.top()});
    if (overlap.bottom() < from.bottom())
        out.push_back(Rect{from.x, overlap.bottom(), from.width, from.bottom() - overlap.bottom()});
    if (overlap.left() > from.left())
        out.push_back(Rect{from.x, overlap.top(), overlap.left() - from.left(),
                           overlap.bottom() - overlap.top()});
    if (overlap.right() < from.right())
        out.push_back(Rect{overlap.right(), overlap.top(), from.right() - overlap.right(),
                           overlap.bottom() - overlap.top()});
}

}  // namespace

Style default_dim(Style style) noexcept {
    const auto dim_channel = [](std::uint8_t c) -> std::uint8_t {
        return static_cast<std::uint8_t>(c / 2);
    };
    // A shadow halves whatever colour is under it, so it needs the channels
    // rather than the name: a palette index is resolved here, at the point
    // where pixels are actually being computed, and the dimmed result is the
    // specific colour it became.
    const auto dim_color = [&](Color c) -> Color {
        const Color rgb = resolved_color(c, Color::rgb(0, 0, 0));
        return Color::rgb(dim_channel(rgb.r()), dim_channel(rgb.g()), dim_channel(rgb.b()));
    };
    Style out = style;
    out.fg = dim_color(style.fg);
    out.bg = dim_color(style.bg);
    // An underline that follows the text keeps following it; one with a
    // colour of its own is in shadow like everything else.
    if (!style.underline_color.is_default()) out.underline_color = dim_color(style.underline_color);
    return out;
}

std::vector<Rect> shadow_footprint(Rect layer_rect, ShadowSpec shadow) noexcept {
    std::vector<Rect> result;
    const int right_height = std::max(0, layer_rect.height - shadow.dy);
    if (shadow.dx > 0 && right_height > 0)
        result.push_back(Rect{layer_rect.right(), layer_rect.top() + shadow.dy, shadow.dx,
                              right_height});
    if (shadow.dy > 0)
        result.push_back(
            Rect{layer_rect.left() + shadow.dx, layer_rect.bottom(), layer_rect.width, shadow.dy});
    return result;
}

Compositor::Compositor(Size frame_size) : frame_(frame_size) {}

void Compositor::compute_damage(const std::vector<Layer>& layers, const Surface& background,
                                const ShadowSpec& shadow) {
    damage_.clear();

    accumulate_surface_damage(damage_, background, Point{0, 0});
    for (const Layer& l : layers) accumulate_surface_damage(damage_, *l.surface, l.position);

    const auto find_previous = [this](int id) -> const PreviousLayer* {
        for (const PreviousLayer& p : previous_layers_)
            if (p.id == id) return &p;
        return nullptr;
    };
    const auto find_current = [&layers](int id) -> bool {
        for (const Layer& l : layers)
            if (l.id == id) return true;
        return false;
    };

    for (const Layer& l : layers) {
        const Rect new_rect = layer_rect(l);
        const PreviousLayer* prev = find_previous(l.id);
        if (!prev) {
            // A brand new layer's own Surface starts fully damaged
            // (Surface ctor), which accumulate_surface_damage above
            // already covers — but its shadow footprint falls OUTSIDE
            // its own rect (over background or other layers), so a
            // freshly appearing shadow needs its own explicit damage,
            // exactly like the removal path below already does.
            if (l.casts_shadow) append_shadow_footprint(new_rect, shadow, l.shadow_clip, damage_);
            continue;
        }

        if (prev->rect != new_rect) {
            damage_.push_back(prev->rect);
            damage_.push_back(new_rect);
            if (prev->casts_shadow)
                append_shadow_footprint(prev->rect, shadow, prev->shadow_clip, damage_);
            if (l.casts_shadow) append_shadow_footprint(new_rect, shadow, l.shadow_clip, damage_);
        } else if (prev->casts_shadow != l.casts_shadow || prev->shadow_clip != l.shadow_clip) {
            if (prev->casts_shadow)
                append_shadow_footprint(prev->rect, shadow, prev->shadow_clip, damage_);
            if (l.casts_shadow) append_shadow_footprint(new_rect, shadow, l.shadow_clip, damage_);
        }
    }
    for (const PreviousLayer& prev : previous_layers_) {
        if (find_current(prev.id)) continue;
        damage_.push_back(prev.rect);
        if (prev.casts_shadow) append_shadow_footprint(prev.rect, shadow, prev.shadow_clip, damage_);
    }

    const Rect bounds{0, 0, frame_.size().width, frame_.size().height};
    clipped_damage_.clear();
    for (const Rect& r : damage_) {
        const Rect c = r.intersected(bounds);
        if (!c.empty()) clipped_damage_.push_back(c);
    }

    // Damage sources (background, each layer, structural diffs, shadow
    // footprints) commonly overlap — a naive rect list would resolve
    // and touch the same cells more than once. Merge into a
    // non-overlapping set via the same subtract-based technique
    // visible-raster occlusion uses: each new rect keeps only the part
    // of itself not already covered by an accepted rect.
    damage_.clear();
    for (const Rect& r : clipped_damage_) {
        rect_scratch_b_.clear();
        rect_scratch_b_.push_back(r);
        for (const Rect& covered : damage_) {
            rect_scratch_a_.clear();
            for (const Rect& candidate : rect_scratch_b_)
                append_difference(candidate, covered, rect_scratch_a_);
            rect_scratch_a_.swap(rect_scratch_b_);
            if (rect_scratch_b_.empty()) break;
        }
        damage_.insert(damage_.end(), rect_scratch_b_.begin(), rect_scratch_b_.end());
    }
}

Cell Compositor::resolve_cell(Point p, const std::vector<Layer>& layers, const ShadowSpec& shadow,
                               std::size_t exclusive_top, const Surface& background) const {
    // A single descending pass resolves both visible content and binary
    // shadow coverage. Shadows encountered above the winning content set
    // one flag; any number of overlapping shadows still applies exactly
    // one transform. Returning as soon as a layer owns `p` also means a
    // lower layer's shadow can never affect content above it.
    bool shadowed = false;
    for (std::size_t i = exclusive_top; i-- > 0;) {
        const Layer& l = layers[i];
        const Rect rect = layer_rect(l);
        if (rect.contains(p)) {
            Cell result = l.surface->at(Point{p.x - l.position.x, p.y - l.position.y});
            if (shadowed) result.set_style(shadow.dim(result.style()));
            return result;
        }

        if (!l.casts_shadow) continue;
        if (shadow_covers(rect, shadow, l.shadow_clip, p)) shadowed = true;
    }
    Cell result = background.at(p);
    if (shadowed) result.set_style(shadow.dim(result.style()));
    return result;
}

void Compositor::compute_visible_rasters(const std::vector<Layer>& layers, const Surface& background) {
    visible_rasters_.clear();
    // Every slice is clipped to the frame. A window dragged past an edge
    // still anchors its raster at the window's own position, so the
    // difference against occluders alone can leave a rect reaching off
    // screen: reading those cells is out of bounds, and handing the whole
    // anchor to the presenter with an off-screen visible rect makes it fit
    // the image to what remains instead of cropping. Keeping full_anchor
    // intact while clipping `visible` is what makes the crop a crop.
    const Rect frame_bounds{0, 0, frame_.size().width, frame_.size().height};
    const auto push_visible = [this, &frame_bounds](const Rect& visible, const Rect& full_anchor,
                                                     const RasterRegion& region) {
        const Rect clipped = visible.intersected(frame_bounds);
        if (clipped.empty()) return;
        visible_rasters_.push_back(
            RasterSlice{region.id, clipped, full_anchor, region.image, region.fallback_active});
    };
    // The root/base surface is also a legal Painter target. Its rasters have
    // no layer offset, but windows and popups above them still occlude them
    // exactly as they occlude base cells.
    for (const RasterRegion& region : background.raster_regions()) {
        const Rect full_anchor = region.anchor;
        rect_scratch_a_.clear();
        rect_scratch_a_.push_back(region.visible);
        for (const Layer& occluder : layers) {
            rect_scratch_b_.clear();
            const Rect occluder_rect = layer_rect(occluder);
            for (const Rect& candidate : rect_scratch_a_)
                append_difference(candidate, occluder_rect, rect_scratch_b_);
            rect_scratch_a_.swap(rect_scratch_b_);
            if (rect_scratch_a_.empty()) break;
        }
        for (const Rect& visible : rect_scratch_a_) push_visible(visible, full_anchor, region);
    }
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const Layer& layer = layers[i];
        for (const RasterRegion& region : layer.surface->raster_regions()) {
            const Rect full_anchor{region.anchor.x + layer.position.x,
                                    region.anchor.y + layer.position.y, region.anchor.width,
                                    region.anchor.height};
            rect_scratch_a_.clear();
            rect_scratch_a_.push_back(Rect{region.visible.x + layer.position.x,
                                           region.visible.y + layer.position.y,
                                           region.visible.width, region.visible.height}
                                          .intersected(layer_rect(layer)));
            for (std::size_t j = i + 1; j < layers.size(); ++j) {
                rect_scratch_b_.clear();
                const Rect occluder = layer_rect(layers[j]);
                for (const Rect& candidate : rect_scratch_a_)
                    append_difference(candidate, occluder, rect_scratch_b_);
                rect_scratch_a_.swap(rect_scratch_b_);
                if (rect_scratch_a_.empty()) break;
            }
            for (const Rect& visible : rect_scratch_a_) push_visible(visible, full_anchor, region);
        }
    }
}

void Compositor::compose(const std::vector<Layer>& layers, Surface& background,
                          ShadowSpec shadow) {
    compute_damage(layers, background, shadow);
    cells_touched_ = 0;
    for (const Rect& r : damage_) {
        for (int y = r.top(); y < r.bottom(); ++y) {
            for (int x = r.left(); x < r.right(); ++x) {
                const Point p{x, y};
                frame_.set_cell(p, resolve_cell(p, layers, shadow, layers.size(), background));
                ++cells_touched_;
            }
        }
    }

    background.clear_damage();
    for (const Layer& l : layers) l.surface->clear_damage();

    previous_layers_.clear();
    previous_layers_.reserve(layers.size());
    for (const Layer& l : layers)
        previous_layers_.push_back(PreviousLayer{l.id, layer_rect(l), l.casts_shadow, l.shadow_clip});

    compute_visible_rasters(layers, background);
}

void Compositor::resize(Size new_size) {
    frame_.resize(new_size);
    previous_layers_.clear();
    visible_rasters_.clear();
}

}  // namespace ckv::scene
