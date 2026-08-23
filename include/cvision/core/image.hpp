// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

namespace ckv {

// Owned RGBA (8 bits/channel, straight alpha, row-major, no padding
// beyond `stride` bytes) pixel buffer — the raster primitive shared by
// scene raster regions and the widgets/gfx layers (the architecture §7).
class Image {
public:
    Image() = default;

    // Zero-initialized (transparent black) buffer of the given size.
    Image(int width, int height) : width_(width < 0 ? 0 : width), height_(height < 0 ? 0 : height) {
        stride_ = width_ * 4;
        pixels_.assign(static_cast<std::size_t>(stride_) * static_cast<std::size_t>(height_), 0);
    }

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    int stride() const noexcept { return stride_; }
    bool empty() const noexcept { return width_ == 0 || height_ == 0; }

    const std::uint8_t* data() const noexcept { return pixels_.data(); }
    std::uint8_t* data() noexcept { return pixels_.data(); }

    // Pointer to the first byte (R) of row `y`. `y` must be in [0, height).
    const std::uint8_t* row(int y) const noexcept {
        return pixels_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_);
    }
    std::uint8_t* row(int y) noexcept {
        return pixels_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_);
    }

    struct Rgba {
        std::uint8_t r, g, b, a;
    };

    // `x`/`y` must be in bounds; no bounds checking (hot path).
    Rgba pixel(int x, int y) const noexcept {
        const std::uint8_t* p = row(y) + static_cast<std::size_t>(x) * 4;
        return Rgba{p[0], p[1], p[2], p[3]};
    }
    void set_pixel(int x, int y, Rgba value) noexcept {
        std::uint8_t* p = row(y) + static_cast<std::size_t>(x) * 4;
        p[0] = value.r;
        p[1] = value.g;
        p[2] = value.b;
        p[3] = value.a;
    }

private:
    int width_ = 0;
    int height_ = 0;
    int stride_ = 0;
    std::vector<std::uint8_t> pixels_;
};

}  // namespace ckv
