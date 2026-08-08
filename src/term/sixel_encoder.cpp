// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/sixel_encoder.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <vector>

namespace ckv::term {
namespace {

std::uint8_t quantize_level(std::uint8_t c) noexcept {
    constexpr std::uint8_t levels[6] = {0, 51, 102, 153, 204, 255};
    int best = 0;
    int best_dist = 256;
    for (int i = 0; i < 6; ++i) {
        const int dist = std::abs(static_cast<int>(c) - static_cast<int>(levels[i]));
        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }
    return levels[best];
}

int percent(std::uint8_t c) noexcept { return (static_cast<int>(c) * 100 + 127) / 255; }

std::uint32_t pack(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    return (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) | b;
}

}  // namespace

std::string encode_sixel(const Image& image) {
    if (image.empty()) return {};
    const int w = image.width();
    const int h = image.height();

    // Decide quantization mode: exact palette if <= 256 unique colors
    // appear (alpha is ignored — Sixel has no alpha channel), else a
    // deterministic 6-level-per-channel cube (<= 216 registers,
    // content-independent so it never exceeds the register budget).
    std::map<std::uint32_t, int> exact_probe;
    bool use_cube = false;
    for (int y = 0; y < h && !use_cube; ++y) {
        for (int x = 0; x < w; ++x) {
            const Image::Rgba p = image.pixel(x, y);
            const std::uint32_t key = pack(p.r, p.g, p.b);
            if (exact_probe.find(key) == exact_probe.end()) {
                if (exact_probe.size() >= 256) {
                    use_cube = true;
                    break;
                }
                exact_probe.emplace(key, static_cast<int>(exact_probe.size()));
            }
        }
    }

    std::map<std::uint32_t, int> palette;
    std::vector<int> pixel_index(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Image::Rgba p = image.pixel(x, y);
            if (use_cube) {
                p.r = quantize_level(p.r);
                p.g = quantize_level(p.g);
                p.b = quantize_level(p.b);
            }
            const std::uint32_t key = pack(p.r, p.g, p.b);
            const auto [it, inserted] = palette.try_emplace(key, static_cast<int>(palette.size()));
            pixel_index[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                         static_cast<std::size_t>(x)] = it->second;
        }
    }

    std::vector<std::array<std::uint8_t, 3>> palette_colors(palette.size());
    for (const auto& [key, idx] : palette)
        palette_colors[static_cast<std::size_t>(idx)] = {
            static_cast<std::uint8_t>((key >> 16) & 0xFF), static_cast<std::uint8_t>((key >> 8) & 0xFF),
            static_cast<std::uint8_t>(key & 0xFF)};

    // Pa=0 (device pixel aspect), Pb=0 (opaque background), Ph=0.
    // Raster attributes pin the exact width/height so the final partial
    // sixel band cannot paint beyond the source image. Sixel carries no
    // alpha channel; source alpha is deliberately ignored above and every
    // source pixel is represented by an opaque palette color.
    std::string out = "\x1BP0;0;0q\"1;1;";
    out += std::to_string(w);
    out += ';';
    out += std::to_string(h);
    for (std::size_t i = 0; i < palette_colors.size(); ++i) {
        out += '#';
        out += std::to_string(i);
        out += ";2;";
        out += std::to_string(percent(palette_colors[i][0]));
        out += ';';
        out += std::to_string(percent(palette_colors[i][1]));
        out += ';';
        out += std::to_string(percent(palette_colors[i][2]));
    }

    for (int band_y = 0; band_y < h; band_y += 6) {
        const int band_height = std::min(6, h - band_y);
        bool first_color_in_band = true;
        for (std::size_t color_idx = 0; color_idx < palette_colors.size(); ++color_idx) {
            bool present = false;
            for (int dy = 0; dy < band_height && !present; ++dy)
                for (int x = 0; x < w; ++x)
                    if (pixel_index[static_cast<std::size_t>(band_y + dy) * static_cast<std::size_t>(w) +
                                     static_cast<std::size_t>(x)] == static_cast<int>(color_idx)) {
                        present = true;
                        break;
                    }
            if (!present) continue;

            if (!first_color_in_band) out += '$';
            first_color_in_band = false;
            out += '#';
            out += std::to_string(color_idx);

            std::string run;
            run.reserve(static_cast<std::size_t>(w));
            for (int x = 0; x < w; ++x) {
                unsigned char bits = 0;
                for (int dy = 0; dy < band_height; ++dy)
                    if (pixel_index[static_cast<std::size_t>(band_y + dy) * static_cast<std::size_t>(w) +
                                     static_cast<std::size_t>(x)] == static_cast<int>(color_idx))
                        bits |= static_cast<unsigned char>(1u << dy);
                run += static_cast<char>(63 + bits);
            }
            std::size_t i = 0;
            while (i < run.size()) {
                std::size_t j = i;
                while (j < run.size() && run[j] == run[i]) ++j;
                const std::size_t count = j - i;
                if (count >= 4) {
                    out += '!';
                    out += std::to_string(count);
                    out += run[i];
                } else {
                    out.append(count, run[i]);
                }
                i = j;
            }
        }
        if (band_y + 6 < h) out += '-';
    }
    out += "\x1B\\";
    return out;
}

}  // namespace ckv::term
