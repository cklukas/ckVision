// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/scene/compositor.hpp"

#include <algorithm>
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

std::vector<Rect> shadow_footprint(Rect layer_rect, ShadowSpec shadow) {
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

    const auto find_previous_index = [this](int id) -> std::optional<std::size_t> {
        for (std::size_t idx = 0; idx < previous_layers_.size(); ++idx)
            if (previous_layers_[idx].id == id) return idx;
        return std::nullopt;
    };
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

    for (std::size_t i = 0; i < layers.size(); ++i) {
        const Layer& l = layers[i];
        const Rect new_rect = layer_rect(l);
        const PreviousLayer* prev = find_previous(l.id);
        if (!prev) {
            damage_.push_back(new_rect);
            if (l.casts_shadow) append_shadow_footprint(new_rect, shadow, l.shadow_clip, damage_);
            continue;
        }

        if (prev->rect != new_rect) {
            damage_.push_back(prev->rect);
            damage_.push_back(new_rect);
            if (prev->casts_shadow)
                append_shadow_footprint(prev->rect, shadow, prev->shadow_clip, damage_);
            if (l.casts_shadow) append_shadow_footprint(new_rect, shadow, l.shadow_clip, damage_);
        } else if (prev->position != l.position) {
            damage_.push_back(new_rect);
            if (prev->casts_shadow != l.casts_shadow || prev->shadow_clip != l.shadow_clip) {
                if (prev->casts_shadow)
                    append_shadow_footprint(prev->rect, shadow, prev->shadow_clip, damage_);
                if (l.casts_shadow) append_shadow_footprint(new_rect, shadow, l.shadow_clip, damage_);
            }
        } else if (prev->casts_shadow != l.casts_shadow || prev->shadow_clip != l.shadow_clip) {
            if (prev->casts_shadow)
                append_shadow_footprint(prev->rect, shadow, prev->shadow_clip, damage_);
            if (l.casts_shadow) append_shadow_footprint(new_rect, shadow, l.shadow_clip, damage_);
        }

        const std::optional<std::size_t> prev_i = find_previous_index(l.id);
        if (prev_i) {
            for (std::size_t j = i + 1; j < layers.size(); ++j) {
                const std::optional<std::size_t> prev_j = find_previous_index(layers[j].id);
                if (!prev_j || *prev_i < *prev_j) continue;
                // Layers i and j swapped relative z-order without necessarily moving.
                const Rect other_rect = layer_rect(layers[j]);
                const Rect overlap = new_rect.intersected(other_rect);
                if (!overlap.empty()) damage_.push_back(overlap);
                if (l.casts_shadow) append_shadow_footprint(new_rect, shadow, l.shadow_clip, damage_);
                if (layers[j].casts_shadow)
                    append_shadow_footprint(other_rect, shadow, layers[j].shadow_clip, damage_);
            }
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
        Rect c = r.intersected(bounds);
        if (c.empty()) continue;
        // Expand horizontally by 1 cell when a damage edge touches a 2-column
        // wide glyph in the existing frame so split glyphs are re-resolved.
        bool expand_left = false;
        bool expand_right = false;
        if (c.left() > 0) {
            const int lx = c.left() - 1;
            for (int y = c.top(); y < c.bottom(); ++y) {
                const Point lp{lx, y};
                if (frame_.at(lp).width() == 2 ||
                    (background.size().width > lx && background.size().height > y &&
                     background.at(lp).width() == 2)) {
                    expand_left = true;
                    break;
                }
                for (const Layer& l : layers) {
                    const Rect lr = layer_rect(l);
                    if (lr.contains(lp) &&
                        l.surface->at(Point{lp.x - l.position.x, lp.y - l.position.y}).width() == 2) {
                        expand_left = true;
                        break;
                    }
                }
                if (expand_left) break;
            }
        }
        if (c.right() < bounds.right()) {
            const int rx = c.right();
            for (int y = c.top(); y < c.bottom(); ++y) {
                const Point rp{rx, y};
                if (frame_.at(rp).is_continuation() ||
                    (background.size().width > rx && background.size().height > y &&
                     background.at(rp).is_continuation())) {
                    expand_right = true;
                    break;
                }
                for (const Layer& l : layers) {
                    const Rect lr = layer_rect(l);
                    if (lr.contains(rp) &&
                        l.surface->at(Point{rp.x - l.position.x, rp.y - l.position.y}).is_continuation()) {
                        expand_right = true;
                        break;
                    }
                }
                if (expand_right) break;
            }
        }
        if (expand_left) {
            c.x -= 1;
            c.width += 1;
        }
        if (expand_right) {
            c.width += 1;
        }
        clipped_damage_.push_back(c);
    }

    // Merge into a non-overlapping set via subtract-based deduplication.
    damage_.clear();
    for (const Rect& r : clipped_damage_) {
        rect_scratch_b_.clear();
        rect_scratch_b_.push_back(r);
        for (const Rect& covered : damage_) {
            if (covered.intersected(r).empty()) continue;
            rect_scratch_a_.clear();
            for (const Rect& candidate : rect_scratch_b_)
                append_difference(candidate, covered, rect_scratch_a_);
            rect_scratch_a_.swap(rect_scratch_b_);
            if (rect_scratch_b_.empty()) break;
        }
        damage_.insert(damage_.end(), rect_scratch_b_.begin(), rect_scratch_b_.end());
    }
}

Compositor::ResolvedCell Compositor::resolve_cell(Point p, const std::vector<Layer>& layers,
                                                  const ShadowSpec& shadow,
                                                  const Surface& background) const {
    // A single descending pass resolves both visible content and binary
    // shadow coverage. Shadows encountered above the winning content set
    // one flag; any number of overlapping shadows still applies exactly
    // one transform. Returning as soon as a layer owns `p` also means a
    // lower layer's shadow can never affect content above it.
    bool shadowed = false;
    for (std::size_t i = layers.size(); i-- > 0;) {
        const Layer& l = layers[i];
        const Rect rect = layer_rect(l);
        if (rect.contains(p)) {
            const Point local{p.x - l.position.x, p.y - l.position.y};
            ResolvedCell result{l.surface->at(local), l.surface->link_target(local)};
            auto occluded_above = [&](Point neighbor) noexcept {
                for (std::size_t j = i + 1; j < layers.size(); ++j) {
                    if (layer_rect(layers[j]).contains(neighbor)) return true;
                }
                return false;
            };
            if (result.cell.width() == 2) {
                const Point right{p.x + 1, p.y};
                if (right.x >= frame_.size().width || !rect.contains(right) ||
                    occluded_above(right)) {
                    const Style s = result.cell.style();
                    result.cell = Cell::from_grapheme(" ", Style{.bg = (s.attrs & Attr::Reverse) != Attr{} ? s.fg : s.bg});
                    result.link_target = {};
                }
            } else if (result.cell.is_continuation()) {
                const Point left{p.x - 1, p.y};
                if (p.x == 0 || !rect.contains(left) || occluded_above(left)) {
                    const Style s = result.cell.style();
                    result.cell = Cell::from_grapheme(" ", Style{.bg = (s.attrs & Attr::Reverse) != Attr{} ? s.fg : s.bg});
                    result.link_target = {};
                }
            }
            // A shadow dims what lies beneath it; it does not unlink it.
            if (shadowed) result.cell.set_style(shadow.style.apply(result.cell.style()));
            return result;
        }

        if (!l.casts_shadow) continue;
        if (shadow_covers(rect, shadow, l.shadow_clip, p)) shadowed = true;
    }
    ResolvedCell result{background.at(p), background.link_target(p)};
    auto occluded_by_layer = [&](Point neighbor) noexcept {
        for (std::size_t j = 0; j < layers.size(); ++j) {
            if (layer_rect(layers[j]).contains(neighbor)) return true;
        }
        return false;
    };
    if (result.cell.width() == 2) {
        const Point right{p.x + 1, p.y};
        if (right.x >= frame_.size().width || occluded_by_layer(right)) {
            const Style s = result.cell.style();
            result.cell = Cell::from_grapheme(" ", Style{.bg = (s.attrs & Attr::Reverse) != Attr{} ? s.fg : s.bg});
            result.link_target = {};
        }
    } else if (result.cell.is_continuation()) {
        const Point left{p.x - 1, p.y};
        if (p.x == 0 || occluded_by_layer(left)) {
            const Style s = result.cell.style();
            result.cell = Cell::from_grapheme(" ", Style{.bg = (s.attrs & Attr::Reverse) != Attr{} ? s.fg : s.bg});
            result.link_target = {};
        }
    }
    if (shadowed) result.cell.set_style(shadow.style.apply(result.cell.style()));
    return result;
}

void Compositor::compute_visible_rasters(const std::vector<Layer>& layers, const Surface& background,
                                         const ShadowSpec& shadow) {
    visible_rasters_.clear();
    // Every slice is clipped to the frame. A window dragged past an edge
    // still anchors its raster at the window's own position, so the
    // difference against occluders alone can leave a rect reaching off
    // screen: reading those cells is out of bounds, and handing the whole
    // anchor to the presenter with an off-screen visible rect makes it fit
    // the image to what remains instead of cropping. Keeping full_anchor
    // intact while clipping `visible` is what makes the crop a crop.
    const Rect frame_bounds{0, 0, frame_.size().width, frame_.size().height};
    const auto push_visible = [this, &frame_bounds, &shadow](const Rect& visible, const Rect& full_anchor,
                                                              const RasterRegion& region, bool shadowed) {
        const Rect clipped = visible.intersected(frame_bounds);
        if (clipped.empty()) return;
        visible_rasters_.push_back(RasterSlice{region.id, clipped, full_anchor, region.image,
                                               shadowed ? std::optional{shadow.style} : std::nullopt});
    };
    const auto push_shadowed = [this, &layers, &shadow, &push_visible](
                                   const Rect& full_anchor, const RasterRegion& region,
                                   std::size_t first_higher_layer) {
        raster_scratch_a_.clear();
        for (const Rect& visible : rect_scratch_a_)
            raster_scratch_a_.push_back(RasterFragment{visible, false});
        for (std::size_t index = first_higher_layer; index < layers.size(); ++index) {
            const Layer& higher = layers[index];
            if (!higher.casts_shadow) continue;
            rect_scratch_b_.clear();
            append_shadow_footprint(layer_rect(higher), shadow, higher.shadow_clip,
                                    rect_scratch_b_);
            for (const Rect& footprint : rect_scratch_b_) {
                raster_scratch_b_.clear();
                for (const RasterFragment& fragment : raster_scratch_a_) {
                    const Rect covered = fragment.rect.intersected(footprint);
                    if (fragment.shadowed || covered.empty()) {
                        raster_scratch_b_.push_back(fragment);
                        continue;
                    }
                    rect_scratch_a_.clear();
                    append_difference(fragment.rect, footprint, rect_scratch_a_);
                    for (const Rect& remainder : rect_scratch_a_)
                        raster_scratch_b_.push_back(RasterFragment{remainder, false});
                    raster_scratch_b_.push_back(RasterFragment{covered, true});
                }
                raster_scratch_a_.swap(raster_scratch_b_);
            }
        }
        for (const RasterFragment& fragment : raster_scratch_a_)
            push_visible(fragment.rect, full_anchor, region, fragment.shadowed);
    };
    // The root/base surface is also a legal Painter target. Its rasters have
    // no layer offset, but windows and popups above them still occlude them
    // exactly as they occlude base cells.
    for (const RasterRegion& region : background.raster_regions()) {
        const Rect full_anchor = region.anchor;
        rect_scratch_a_.clear();
        append_raster_coverage_rectangles(
            rect_scratch_a_, full_anchor, region.visible,
            region.live_cells ? std::span<const std::uint8_t>(*region.live_cells)
                              : std::span<const std::uint8_t>{},
            raster_cov_prev_, raster_cov_curr_);
        for (const Layer& occluder : layers) {
            rect_scratch_b_.clear();
            const Rect occluder_rect = layer_rect(occluder);
            for (const Rect& candidate : rect_scratch_a_)
                append_difference(candidate, occluder_rect, rect_scratch_b_);
            rect_scratch_a_.swap(rect_scratch_b_);
            if (rect_scratch_a_.empty()) break;
        }
        push_shadowed(full_anchor, region, 0);
    }
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const Layer& layer = layers[i];
        for (const RasterRegion& region : layer.surface->raster_regions()) {
            const Rect full_anchor{region.anchor.x + layer.position.x,
                                    region.anchor.y + layer.position.y, region.anchor.width,
                                    region.anchor.height};
            rect_scratch_a_.clear();
            const Rect visible = Rect{region.visible.x + layer.position.x,
                                      region.visible.y + layer.position.y,
                                      region.visible.width, region.visible.height}
                                     .intersected(layer_rect(layer));
            append_raster_coverage_rectangles(
                rect_scratch_a_, full_anchor, visible,
                region.live_cells ? std::span<const std::uint8_t>(*region.live_cells)
                                  : std::span<const std::uint8_t>{},
                raster_cov_prev_, raster_cov_curr_);
            for (std::size_t j = i + 1; j < layers.size(); ++j) {
                rect_scratch_b_.clear();
                const Rect occluder = layer_rect(layers[j]);
                for (const Rect& candidate : rect_scratch_a_)
                    append_difference(candidate, occluder, rect_scratch_b_);
                rect_scratch_a_.swap(rect_scratch_b_);
                if (rect_scratch_a_.empty()) break;
            }
            push_shadowed(full_anchor, region, i + 1);
        }
    }
}

void Compositor::compose(const std::vector<Layer>& layers, Surface& background,
                          ShadowSpec shadow) {
    compute_damage(layers, background, shadow);
    // A different shadow restyles, and may move, every shadowed cell of the
    // frame. Which cells those are depends on both the old and the new spec,
    // and a change is a theme switch rather than a per-frame event, so the
    // whole frame is resolved again.
    if (shadow != previous_shadow_) {
        damage_.clear();
        damage_.push_back(Rect{0, 0, frame_.size().width, frame_.size().height});
        previous_shadow_ = shadow;
    }
    cells_touched_ = 0;
    for (const Rect& r : damage_) {
        for (int y = r.top(); y < r.bottom(); ++y) {
            for (int x = r.left(); x < r.right(); ++x) {
                const Point p{x, y};
                ResolvedCell resolved = resolve_cell(p, layers, shadow, background);
                if (!resolved.cell.same_content(frame_.at(p)) ||
                    resolved.link_target != frame_.link_target(p)) {
                    frame_.set_cell(p, std::move(resolved.cell), resolved.link_target);
                }
                ++cells_touched_;
            }
        }
    }

    background.clear_damage();
    for (const Layer& l : layers) l.surface->clear_damage();

    previous_layers_.clear();
    previous_layers_.reserve(layers.size());
    for (const Layer& l : layers)
        previous_layers_.push_back(
            PreviousLayer{l.id, layer_rect(l), l.position, l.casts_shadow, l.shadow_clip});

    compute_visible_rasters(layers, background, shadow);
}

void Compositor::resize(Size new_size) {
    frame_.resize(new_size);
    previous_layers_.clear();
    visible_rasters_.clear();
}

}  // namespace ckv::scene
