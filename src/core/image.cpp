// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/image.hpp"

#include <algorithm>
#include <cstdint>

namespace ckv {

Image Image::scaled_to(PixelSize target, ImageSampling sampling) const {
    if (empty() || target.width <= 0 || target.height <= 0) return Image{};
    if (target.width == width_ && target.height == height_) return *this;

    Image out(target);
    if (out.empty()) return out;

    const bool downscale = target.width < width_ || target.height < height_;
    if (sampling == ImageSampling::Nearest) {
        for (int y = 0; y < target.height; ++y) {
            const int sy = static_cast<int>(
                (static_cast<std::int64_t>(y) * height_) / target.height);
            for (int x = 0; x < target.width; ++x) {
                const int sx = static_cast<int>(
                    (static_cast<std::int64_t>(x) * width_) / target.width);
                out.set_pixel(x, y, pixel(sx, sy));
            }
        }
        return out;
    }

    if (downscale) {
        for (int y = 0; y < target.height; ++y) {
            const int y0 = static_cast<int>(
                (static_cast<std::int64_t>(y) * height_) / target.height);
            int y1 = static_cast<int>(
                (static_cast<std::int64_t>(y + 1) * height_ + target.height - 1) / target.height);
            y1 = std::clamp(std::max(y0 + 1, y1), 1, height_);
            for (int x = 0; x < target.width; ++x) {
                const int x0 = static_cast<int>(
                    (static_cast<std::int64_t>(x) * width_) / target.width);
                int x1 = static_cast<int>(
                    (static_cast<std::int64_t>(x + 1) * width_ + target.width - 1) / target.width);
                x1 = std::clamp(std::max(x0 + 1, x1), 1, width_);

                std::uint64_t sr = 0, sg = 0, sb = 0, sa = 0;
                const std::uint64_t count =
                    static_cast<std::uint64_t>(y1 - y0) * static_cast<std::uint64_t>(x1 - x0);
                for (int sy = y0; sy < y1; ++sy) {
                    for (int sx = x0; sx < x1; ++sx) {
                        const Rgba p = pixel(sx, sy);
                        sr += p.r;
                        sg += p.g;
                        sb += p.b;
                        sa += p.a;
                    }
                }
                const std::uint64_t half = count / 2;
                out.set_pixel(x, y, Rgba{
                    static_cast<std::uint8_t>((sr + half) / count),
                    static_cast<std::uint8_t>((sg + half) / count),
                    static_cast<std::uint8_t>((sb + half) / count),
                    static_cast<std::uint8_t>((sa + half) / count),
                });
            }
        }
        return out;
    }

    // Smooth upscale: 8-bit fixed-point bilinear interpolation.
    const std::int64_t max_fx = static_cast<std::int64_t>(width_ - 1) * 256;
    const std::int64_t max_fy = static_cast<std::int64_t>(height_ - 1) * 256;
    for (int y = 0; y < target.height; ++y) {
        const std::int64_t raw_fy =
            ((static_cast<std::int64_t>(2 * y + 1) * height_ * 256) / (2 * target.height)) - 128;
        const std::int64_t clamped_fy = std::clamp<std::int64_t>(raw_fy, 0, max_fy);
        const int y0 = static_cast<int>(clamped_fy >> 8);
        const int y1 = std::min(y0 + 1, height_ - 1);
        const std::uint32_t fy = static_cast<std::uint32_t>(clamped_fy & 255);
        const std::uint32_t iy = 256U - fy;

        for (int x = 0; x < target.width; ++x) {
            const std::int64_t raw_fx =
                ((static_cast<std::int64_t>(2 * x + 1) * width_ * 256) / (2 * target.width)) - 128;
            const std::int64_t clamped_fx = std::clamp<std::int64_t>(raw_fx, 0, max_fx);
            const int x0 = static_cast<int>(clamped_fx >> 8);
            const int x1 = std::min(x0 + 1, width_ - 1);
            const std::uint32_t fx = static_cast<std::uint32_t>(clamped_fx & 255);
            const std::uint32_t ix = 256U - fx;

            const Rgba p00 = pixel(x0, y0);
            const Rgba p10 = pixel(x1, y0);
            const Rgba p01 = pixel(x0, y1);
            const Rgba p11 = pixel(x1, y1);

            const std::uint32_t w00 = ix * iy;
            const std::uint32_t w10 = fx * iy;
            const std::uint32_t w01 = ix * fy;
            const std::uint32_t w11 = fx * fy;

            const auto blend = [&](std::uint8_t c00, std::uint8_t c10, std::uint8_t c01,
                                   std::uint8_t c11) -> std::uint8_t {
                const std::uint32_t sum =
                    w00 * c00 + w10 * c10 + w01 * c01 + w11 * c11 + 32768U;
                return static_cast<std::uint8_t>(sum >> 16);
            };
            out.set_pixel(x, y, Rgba{
                blend(p00.r, p10.r, p01.r, p11.r),
                blend(p00.g, p10.g, p01.g, p11.g),
                blend(p00.b, p10.b, p01.b, p11.b),
                blend(p00.a, p10.a, p01.a, p11.a),
            });
        }
    }
    return out;
}

}  // namespace ckv
