// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "raster_checks.hpp"

#include <cstdint>

namespace ckv::docgen {

namespace {

bool same(Image::Rgba a, Image::Rgba b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// The slice whose visible cells hold `cell`, or nullptr.
const RasterSlice* slice_at(const std::vector<RasterSlice>& slices, Point cell) {
    for (const RasterSlice& slice : slices)
        if (slice.visible_rect.contains(cell)) return &slice;
    return nullptr;
}

// Calls visit(pixel, slice) for every plane pixel inside a slice's visible
// cells; stops, returning false, as soon as visit does.
template <class Visit>
bool for_each_slice_pixel(const std::vector<RasterSlice>& slices, PixelSize cell, Visit visit) {
    for (const RasterSlice& slice : slices) {
        const Rect& cells = slice.visible_rect;
        for (int y = cells.y * cell.height; y < cells.bottom() * cell.height; ++y)
            for (int x = cells.x * cell.width; x < cells.right() * cell.width; ++x)
                if (!visit(PixelPoint{x, y}, slice)) return false;
    }
    return true;
}

}  // namespace

std::vector<RasterSlice> slices_of(const std::vector<RasterSlice>& slices, const Image* picture) {
    std::vector<RasterSlice> found;
    for (const RasterSlice& slice : slices)
        if (slice.image.get() == picture) found.push_back(slice);
    return found;
}

std::optional<Rect> full_anchor_of(const std::vector<RasterSlice>& slices, const Image* picture) {
    for (const RasterSlice& slice : slices)
        if (slice.image.get() == picture) return slice.full_anchor;
    return std::nullopt;
}

int stray_pixels(const Image& plane, const std::vector<RasterSlice>& slices, PixelSize cell) {
    int stray = 0;
    for (int y = 0; y < plane.height(); ++y) {
        for (int x = 0; x < plane.width(); ++x) {
            const bool shown = slice_at(slices, Point{x / cell.width, y / cell.height}) != nullptr;
            const std::uint8_t alpha = plane.pixel(x, y).a;
            if (shown ? alpha != 255 : alpha != 0) ++stray;
        }
    }
    return stray;
}

int moved_pixels_compared(const Image& before, Rect before_anchor, const std::vector<RasterSlice>& before_slices,
                          const Image& after, Rect after_anchor, const std::vector<RasterSlice>& after_slices,
                          PixelSize cell) {
    const int shift_x = (after_anchor.x - before_anchor.x) * cell.width;
    const int shift_y = (after_anchor.y - before_anchor.y) * cell.height;
    int compared = 0;
    const bool agrees = for_each_slice_pixel(after_slices, cell, [&](PixelPoint pixel, const RasterSlice& slice) {
        const PixelPoint source{pixel.x - shift_x, pixel.y - shift_y};
        if (source.x < 0 || source.y < 0) return true;
        const RasterSlice* earlier =
            slice_at(before_slices, Point{source.x / cell.width, source.y / cell.height});
        if (earlier == nullptr || earlier->shadow != slice.shadow) return true;
        ++compared;
        return same(after.pixel(pixel.x, pixel.y), before.pixel(source.x, source.y));
    });
    return agrees ? compared : -1;
}

int covered_pixels_compared(const Image& before, const Image& after, const std::vector<RasterSlice>& after_slices,
                            PixelSize cell) {
    // The rounding bounds above, in whole channel steps.
    constexpr int kOpenRounding = 26;
    constexpr int kShadowedRounding = 39;
    const auto close_enough = [](std::uint8_t shown, int expected, int rounding) {
        const int difference = shown - expected;
        return difference >= -rounding && difference <= rounding;
    };
    int compared = 0;
    const bool agrees = for_each_slice_pixel(after_slices, cell, [&](PixelPoint pixel, const RasterSlice& slice) {
        const Image::Rgba open = before.pixel(pixel.x, pixel.y);
        const Image::Rgba shown = after.pixel(pixel.x, pixel.y);
        ++compared;
        if (shown.a != open.a) return false;
        if (!slice.shadow)
            return close_enough(shown.r, open.r, kOpenRounding) && close_enough(shown.g, open.g, kOpenRounding) &&
                   close_enough(shown.b, open.b, kOpenRounding);
        const Image::Rgba expected = slice.shadow->apply(open);
        const int open_sum = open.r + open.g + open.b;
        return close_enough(shown.r, expected.r, kShadowedRounding) && close_enough(shown.g, expected.g, kShadowedRounding) &&
               close_enough(shown.b, expected.b, kShadowedRounding) &&
               (open_sum == 0 || shown.r + shown.g + shown.b < open_sum);
    });
    return agrees ? compared : -1;
}

}  // namespace ckv::docgen
