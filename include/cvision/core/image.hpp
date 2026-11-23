// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <cstdint>
#include <limits>
#include <vector>

#include "cvision/core/assert.hpp"
#include "cvision/core/geometry.hpp"

namespace ckv {

// Resampling mode for Image::scaled_to() and raster presentation.
// Auto uses deterministic integer box-averaging when downscaling and bilinear
// interpolation when upscaling.
enum class ImageSampling : std::uint8_t {
    Auto,
    Nearest,
    Smooth,
};

// Owned RGBA (8 bits/channel, straight alpha, row-major, no padding
// beyond `stride` bytes) pixel buffer — the raster primitive shared by
// scene raster regions and the widgets/gfx layers (ARCHITECTURE.md §7).
class CVISION_TYPE_API Image {
public:
    // An empty 0x0 image with no pixel storage.
    CVISION_MEMBER_API Image() = default;

    // Zero-initialized (transparent black) buffer of the given size in
    // pixels. Negative or overflowing dimensions are clamped to 0, giving an
    // empty image.
    explicit Image(PixelSize size) {
        if (size.width <= 0 || size.height <= 0) return;
        const std::int64_t stride64 = static_cast<std::int64_t>(size.width) * 4;
        if (stride64 > std::numeric_limits<int>::max()) return;
        const std::uint64_t total64 =
            static_cast<std::uint64_t>(stride64) * static_cast<std::uint64_t>(size.height);
        if (total64 > std::numeric_limits<std::size_t>::max()) return;
        width_ = size.width;
        height_ = size.height;
        stride_ = static_cast<int>(stride64);
        pixels_.assign(static_cast<std::size_t>(total64), 0);
        content_id_dirty_ = true;
    }

    // Deterministic content identity for O(1) raster caching (D-156, D-159).
    // Returns 0 for an empty image; computed lazily from dimensions and pixel
    // bytes so no process-global mutable state is needed.
    std::uint64_t content_id() const noexcept {
        if (empty()) return 0;
        if (content_id_dirty_) {
            content_id_ = compute_content_hash();
            content_id_dirty_ = false;
        }
        return content_id_;
    }
    void mark_modified() noexcept {
        if (empty()) {
            content_id_ = 0;
            content_id_dirty_ = false;
        } else {
            content_id_dirty_ = true;
        }
    }

    // The extent in pixels, as one value.
    PixelSize size() const noexcept { return PixelSize{width_, height_}; }
    // Dimensions in pixels, and the distance in bytes between the starts of
    // consecutive rows (always width() * 4). empty() is true when either
    // dimension is 0.
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    int stride() const noexcept { return stride_; }
    bool empty() const noexcept { return width_ == 0 || height_ == 0; }

    // Deterministic scaled copy in straight RGBA. Empty if either target
    // dimension is non-positive or this image is empty.
    CVISION_MEMBER_API Image scaled_to(PixelSize target, ImageSampling sampling = ImageSampling::Auto) const;

    // The whole buffer, stride() * height() bytes starting with the R byte of
    // the top-left pixel. May be null for an empty image.
    const std::uint8_t* data() const noexcept { return pixels_.data(); }
    std::uint8_t* data() noexcept {
        if (!empty()) content_id_dirty_ = true;
        return pixels_.data();
    }

    // Pointer to the first byte (R) of row `y`. `y` must be in [0, height).
    const std::uint8_t* row(int y) const noexcept {
        CKV_ASSERT(y >= 0 && y < height_);
        return pixels_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_);
    }
    std::uint8_t* row(int y) noexcept {
        CKV_ASSERT(y >= 0 && y < height_);
        content_id_dirty_ = true;
        return pixels_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_);
    }

    // One pixel: 8-bit red, green, blue and straight (not premultiplied)
    // alpha, where alpha 0 is fully transparent and 255 fully opaque.
    struct Rgba {
        // Red, green, blue and alpha, 0-255 each, in memory order.
        std::uint8_t r, g, b, a;
        bool operator==(const Rgba&) const noexcept = default;
    };

    // `x`/`y` must be in bounds; checked with CKV_ASSERT.
    Rgba pixel(int x, int y) const noexcept {
        CKV_ASSERT(x >= 0 && x < width_ && y >= 0 && y < height_);
        const std::uint8_t* p = row(y) + static_cast<std::size_t>(x) * 4;
        return Rgba{p[0], p[1], p[2], p[3]};
    }
    void set_pixel(int x, int y, Rgba value) noexcept {
        CKV_ASSERT(x >= 0 && x < width_ && y >= 0 && y < height_);
        std::uint8_t* p = row(y) + static_cast<std::size_t>(x) * 4;
        p[0] = value.r;
        p[1] = value.g;
        p[2] = value.b;
        p[3] = value.a;
    }

private:
    std::uint64_t compute_content_hash() const noexcept {
        std::uint64_t h = 14695981039346656037ull;
        auto mix = [&h](std::uint64_t v) noexcept {
            h ^= v;
            h *= 1099511628211ull;
        };
        mix(static_cast<std::uint64_t>(static_cast<std::uint32_t>(width_)));
        mix(static_cast<std::uint64_t>(static_cast<std::uint32_t>(height_)));
        for (const std::uint8_t b : pixels_) mix(b);
        return h == 0 ? 1 : h;
    }

    int width_ = 0;
    int height_ = 0;
    int stride_ = 0;
    mutable std::uint64_t content_id_ = 0;
    mutable bool content_id_dirty_ = false;
    std::vector<std::uint8_t> pixels_;
};

}  // namespace ckv
