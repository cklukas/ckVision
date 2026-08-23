// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/scene/surface.hpp"

#include <algorithm>
#include <limits>

#include "cvision/core/assert.hpp"

namespace ckv::scene {
namespace {

constexpr std::uint64_t kDirectionBits = 4;
constexpr std::uint64_t kDirectionMask = (std::uint64_t{1} << kDirectionBits) - 1U;
constexpr std::uint64_t kShadowBit = std::uint64_t{1} << 63U;
constexpr std::uint64_t kMaxJunctionScope = (kShadowBit - 1U) >> kDirectionBits;
constexpr std::uint64_t kJunctionScopeMask = kMaxJunctionScope << kDirectionBits;

std::uint64_t pack_junction(std::uint64_t scope, Junction junction) noexcept {
    CKV_ASSERT(scope > 0 && scope <= kMaxJunctionScope);
    const std::uint64_t directions =
        (junction.up ? 1U : 0U) | (junction.down ? 2U : 0U) | (junction.left ? 4U : 0U) |
        (junction.right ? 8U : 0U);
    CKV_ASSERT(directions != 0);
    return (scope << kDirectionBits) | directions;
}

Junction unpack_junction(std::uint64_t packed) noexcept {
    const std::uint64_t directions = packed & kDirectionMask;
    CKV_ASSERT(directions != 0);
    return Junction{(directions & 1U) != 0, (directions & 2U) != 0,
                    (directions & 4U) != 0, (directions & 8U) != 0};
}

}  // namespace

Surface::Surface(Size size, Cell fill)
    : size_(size),
      cells_(static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height), fill),
      junction_provenance_(static_cast<std::size_t>(size.width) *
                           static_cast<std::size_t>(size.height)),
      row_damage_(static_cast<std::size_t>(size.height), DamageSpan{0, size.width}) {
    CKV_ASSERT(size.width >= 0 && size.height >= 0);
}

const Cell& Surface::at(Point p) const noexcept {
    CKV_ASSERT(p.x >= 0 && p.x < size_.width && p.y >= 0 && p.y < size_.height);
    return cells_[index(p)];
}

void Surface::set_cell(Point p, Cell cell) {
    CKV_ASSERT(p.x >= 0 && p.x < size_.width && p.y >= 0 && p.y < size_.height);
    junction_provenance_[index(p)] = 0;
    write_cell(p, std::move(cell));
}

std::uint64_t Surface::create_junction_scope() noexcept {
    // Scope zero is reserved for ordinary cells. Exhausting 2^59-1
    // logical paints on one Surface is not a recoverable rendering state,
    // and must fail rather than silently alias live provenance.
    CKV_ASSERT(next_junction_scope_ <= kMaxJunctionScope);
    return next_junction_scope_++;
}

std::optional<Junction> Surface::junction_in_scope(Point p, std::uint64_t scope) const noexcept {
    CKV_ASSERT(p.x >= 0 && p.x < size_.width && p.y >= 0 && p.y < size_.height);
    CKV_ASSERT(scope != 0);
    const std::uint64_t provenance = junction_provenance_[index(p)];
    if (((provenance & kJunctionScopeMask) >> kDirectionBits) != scope)
        return std::nullopt;
    return unpack_junction(provenance);
}

bool Surface::begin_shadow(Point p) noexcept {
    CKV_ASSERT(p.x >= 0 && p.x < size_.width && p.y >= 0 && p.y < size_.height);
    std::uint64_t& provenance = junction_provenance_[index(p)];
    if ((provenance & kShadowBit) != 0) return false;
    provenance |= kShadowBit;
    return true;
}

void Surface::set_junction_cell(Point p, Cell cell, std::uint64_t scope, Junction junction) {
    CKV_ASSERT(p.x >= 0 && p.x < size_.width && p.y >= 0 && p.y < size_.height);
    CKV_ASSERT(scope != 0);
    junction_provenance_[index(p)] = pack_junction(scope, junction);
    write_cell(p, std::move(cell));
}

void Surface::set_cell_preserving_junction(Point p, Cell cell) { write_cell(p, std::move(cell)); }

void Surface::write_cell(Point p, Cell cell) {
    CKV_ASSERT(p.x >= 0 && p.x < size_.width && p.y >= 0 && p.y < size_.height);
    cells_[index(p)] = std::move(cell);
    DamageSpan& span = row_damage_[static_cast<std::size_t>(p.y)];
    if (span.empty()) {
        span.lo = p.x;
        span.hi = p.x + 1;
    } else {
        span.lo = std::min(span.lo, p.x);
        span.hi = std::max(span.hi, p.x + 1);
    }
}

void Surface::mark_damage(Rect region) noexcept {
    const Rect clipped = region.intersected(Rect{0, 0, size_.width, size_.height});
    if (clipped.empty()) return;
    for (int y = clipped.top(); y < clipped.bottom(); ++y) {
        DamageSpan& span = row_damage_[static_cast<std::size_t>(y)];
        if (span.empty()) {
            span.lo = clipped.left();
            span.hi = clipped.right();
        } else {
            span.lo = std::min(span.lo, clipped.left());
            span.hi = std::max(span.hi, clipped.right());
        }
    }
}

void Surface::clear_damage() noexcept {
    for (DamageSpan& span : row_damage_) span = DamageSpan{};
}

DamageSpan Surface::row_damage(int row) const noexcept {
    CKV_ASSERT(row >= 0 && row < size_.height);
    return row_damage_[static_cast<std::size_t>(row)];
}

bool Surface::has_damage() const noexcept {
    for (const DamageSpan& span : row_damage_)
        if (!span.empty()) return true;
    return false;
}

void Surface::resize(Size new_size, Cell fill) {
    CKV_ASSERT(new_size.width >= 0 && new_size.height >= 0);
    size_ = new_size;
    cells_.assign(
        static_cast<std::size_t>(new_size.width) * static_cast<std::size_t>(new_size.height),
        fill);
    junction_provenance_.assign(
        static_cast<std::size_t>(new_size.width) * static_cast<std::size_t>(new_size.height),
        0);
    row_damage_.assign(static_cast<std::size_t>(new_size.height), DamageSpan{0, new_size.width});
    raster_regions_.clear();
    next_raster_id_ = 1;
}

int Surface::allocate_raster_id() noexcept {
    int candidate = next_raster_id_;
    for (;;) {
        CKV_ASSERT(candidate > 0);
        const bool in_use = std::any_of(raster_regions_.begin(), raster_regions_.end(),
                                        [candidate](const RasterRegion& region) { return region.id == candidate; });
        if (!in_use) {
            next_raster_id_ = candidate == std::numeric_limits<int>::max() ? 1 : candidate + 1;
            return candidate;
        }
        CKV_ASSERT(candidate != std::numeric_limits<int>::max());
        ++candidate;
    }
}

void Surface::add_raster_region(RasterRegion region) {
    CKV_ASSERT(region.id >= 1);
    CKV_ASSERT(region.image != nullptr);
    CKV_ASSERT(region.image->width() > 0 && region.image->height() > 0);
    for (const RasterRegion& existing : raster_regions_) CKV_ASSERT(existing.id != region.id);
    const Rect anchor = region.anchor;
    raster_regions_.push_back(std::move(region));
    mark_damage(anchor);
}

void Surface::remove_raster_region(int id) noexcept {
    const auto it = std::find_if(raster_regions_.begin(), raster_regions_.end(),
                                  [id](const RasterRegion& r) { return r.id == id; });
    if (it == raster_regions_.end()) return;
    const Rect anchor = it->anchor;
    raster_regions_.erase(it);
    mark_damage(anchor);
}

void Surface::clear_raster_regions() noexcept {
    for (const RasterRegion& region : raster_regions_) mark_damage(region.anchor);
    raster_regions_.clear();
    next_raster_id_ = 1;
}

void Surface::set_raster_fallback_active(int id, bool active) noexcept {
    for (RasterRegion& r : raster_regions_) {
        if (r.id != id) continue;
        if (r.fallback_active != active) {
            r.fallback_active = active;
            mark_damage(r.anchor);
        }
        return;
    }
}

}  // namespace ckv::scene
