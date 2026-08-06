// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/scene/compositor.hpp"

#include <cstdint>

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
    return Rect{l.position.x, l.position.y, l.surface->size().width, l.surface->size().height};
}

}  // namespace

Style default_dim(Style style) noexcept {
    const auto dim_channel = [](std::uint8_t c) -> std::uint8_t {
        return static_cast<std::uint8_t>(c / 2);
    };
    const auto dim_color = [&](Color c) -> Color {
        if (c.is_default()) return Color::rgb(0, 0, 0);
        return Color::rgb(dim_channel(c.r()), dim_channel(c.g()), dim_channel(c.b()));
    };
    Style out = style;
    out.fg = dim_color(style.fg);
    out.bg = dim_color(style.bg);
    return out;
}

std::vector<Rect> shadow_footprint(Rect layer_rect, ShadowSpec shadow) noexcept {
    std::vector<Rect> result;
    if (shadow.dx > 0)
        result.push_back(
            Rect{layer_rect.right(), layer_rect.top() + shadow.dy, shadow.dx, layer_rect.height});
    if (shadow.dy > 0)
        result.push_back(
            Rect{layer_rect.left() + shadow.dx, layer_rect.bottom(), layer_rect.width, shadow.dy});
    return result;
}

Compositor::Compositor(Size frame_size) : frame_(frame_size) {}

std::vector<Rect> Compositor::compute_damage(const std::vector<Layer>& layers,
                                              const Surface& background,
                                              const ShadowSpec& shadow) const {
    std::vector<Rect> damage;

    accumulate_surface_damage(damage, background, Point{0, 0});
    for (const Layer& l : layers) accumulate_surface_damage(damage, *l.surface, l.position);

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
            if (l.casts_shadow) {
                const auto pieces = shadow_footprint(new_rect, shadow);
                damage.insert(damage.end(), pieces.begin(), pieces.end());
            }
            continue;
        }

        if (prev->rect != new_rect) {
            damage.push_back(prev->rect);
            damage.push_back(new_rect);
            if (prev->casts_shadow) {
                const auto pieces = shadow_footprint(prev->rect, shadow);
                damage.insert(damage.end(), pieces.begin(), pieces.end());
            }
            if (l.casts_shadow) {
                const auto pieces = shadow_footprint(new_rect, shadow);
                damage.insert(damage.end(), pieces.begin(), pieces.end());
            }
        } else if (prev->casts_shadow != l.casts_shadow) {
            const auto pieces = shadow_footprint(new_rect, shadow);
            damage.insert(damage.end(), pieces.begin(), pieces.end());
        }
    }
    for (const PreviousLayer& prev : previous_layers_) {
        if (find_current(prev.id)) continue;
        damage.push_back(prev.rect);
        if (prev.casts_shadow) {
            const auto pieces = shadow_footprint(prev.rect, shadow);
            damage.insert(damage.end(), pieces.begin(), pieces.end());
        }
    }

    const Rect bounds{0, 0, frame_.size().width, frame_.size().height};
    std::vector<Rect> clipped;
    clipped.reserve(damage.size());
    for (const Rect& r : damage) {
        const Rect c = r.intersected(bounds);
        if (!c.empty()) clipped.push_back(c);
    }

    // Damage sources (background, each layer, structural diffs, shadow
    // footprints) commonly overlap — a naive rect list would resolve
    // and touch the same cells more than once. Merge into a
    // non-overlapping set via the same subtract-based technique
    // visible-raster occlusion uses: each new rect keeps only the part
    // of itself not already covered by an accepted rect.
    std::vector<Rect> merged;
    merged.reserve(clipped.size());
    for (const Rect& r : clipped) {
        for (const Rect& piece : subtract_rects(r, merged)) merged.push_back(piece);
    }
    return merged;
}

Cell Compositor::resolve_cell(Point p, const std::vector<Layer>& layers, const ShadowSpec& shadow,
                               std::size_t exclusive_top, const Surface& background) const {
    // A single descending pass, not two separate ones: at each layer,
    // from top to bottom, check its own rect FIRST, then whether it
    // casts a shadow over p. Two separate passes (own-rect scan over
    // every layer, THEN a shadow scan) would let a layer far BELOW a
    // shadow-caster win outright before the shadow is ever considered
    // — meaning a shadow could only ever fall on bare background, never
    // dim another layer sitting underneath its caster, contradicting
    // ARCHITECTURE.md §3's "dimming whatever content is there... every
    // layer's own rect wins over any shadow at or below it in z-order".
    for (std::size_t i = exclusive_top; i-- > 0;) {
        const Layer& l = layers[i];
        const Rect rect = layer_rect(l);
        if (rect.contains(p))
            return l.surface->at(Point{p.x - l.position.x, p.y - l.position.y});

        if (!l.casts_shadow) continue;
        const std::vector<Rect> footprint = shadow_footprint(rect, shadow);
        bool covered = false;
        for (const Rect& fr : footprint)
            if (fr.contains(p)) {
                covered = true;
                break;
            }
        if (!covered) continue;
        // Whatever is under the shadow, excluding layer i and above —
        // recursion terminates because i is strictly smaller than
        // exclusive_top on every call, and a layer's own shadow
        // footprint never overlaps its own rect (it starts at the
        // rect's own right/bottom edge), so there is no self-recursion.
        Cell under = resolve_cell(p, layers, shadow, i, background);
        under.set_style(shadow.dim(under.style()));
        return under;
    }
    return background.at(p);
}

void Compositor::compute_visible_rasters(const std::vector<Layer>& layers) {
    visible_rasters_.clear();
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const Layer& layer = layers[i];
        for (const RasterRegion& region : layer.surface->raster_regions()) {
            const Rect full_anchor{region.anchor.x + layer.position.x,
                                    region.anchor.y + layer.position.y, region.anchor.width,
                                    region.anchor.height};
            std::vector<Rect> occluders;
            for (std::size_t j = i + 1; j < layers.size(); ++j) occluders.push_back(layer_rect(layers[j]));
            for (const Rect& visible : subtract_rects(full_anchor, occluders))
                visible_rasters_.push_back(
                    VisibleRaster{region.id, visible, full_anchor, region.image, region.fallback_active});
        }
    }
}

void Compositor::compose(const std::vector<Layer>& layers, Surface& background,
                          ShadowSpec shadow) {
    const std::vector<Rect> damage = compute_damage(layers, background, shadow);
    cells_touched_ = 0;
    for (const Rect& r : damage) {
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
        previous_layers_.push_back(PreviousLayer{l.id, layer_rect(l), l.casts_shadow});

    compute_visible_rasters(layers);
}

void Compositor::resize(Size new_size) {
    frame_.resize(new_size);
    previous_layers_.clear();
    visible_rasters_.clear();
}

}  // namespace ckv::scene
