// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/sixel_encoder.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <vector>

namespace ckv::term {
namespace {

std::uint8_t quantize_level_6(std::uint8_t c) noexcept {
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

std::uint8_t quantize_level(std::uint8_t c, int levels) noexcept {
    if (levels <= 1) return 0;
    const int index = (static_cast<int>(c) * (levels - 1) + 127) / 255;
    return static_cast<std::uint8_t>((index * 255 + (levels - 2) / 2) / (levels - 1));
}

std::array<int, 3> palette_dimensions(int max_colors) noexcept {
    // Retain the established 6×6×6 fallback whenever it fits. For smaller
    // verified limits, grow a balanced RGB grid one axis at a time without
    // ever exceeding the terminal's register budget.
    if (max_colors >= 216) return {6, 6, 6};
    std::array<int, 3> dimensions{1, 1, 1};
    int product = 1;
    for (;;) {
        int axis = -1;
        for (int candidate = 0; candidate < 3; ++candidate) {
            const int proposed = product / dimensions[static_cast<std::size_t>(candidate)] *
                                 (dimensions[static_cast<std::size_t>(candidate)] + 1);
            if (proposed > max_colors) continue;
            if (axis < 0 || dimensions[static_cast<std::size_t>(candidate)] <
                                dimensions[static_cast<std::size_t>(axis)])
                axis = candidate;
        }
        if (axis < 0) return dimensions;
        product = product / dimensions[static_cast<std::size_t>(axis)] *
                  (dimensions[static_cast<std::size_t>(axis)] + 1);
        ++dimensions[static_cast<std::size_t>(axis)];
    }
}

int percent(std::uint8_t c) noexcept { return (static_cast<int>(c) * 100 + 127) / 255; }

std::uint32_t pack(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    return (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) | b;
}

// Colour → palette index, in first-appearance order. A tree or a hash map
// with allocation per colour is a per-pixel cost on an image with a million
// of them; this is a fixed open-addressed table, sized well above the 256
// registers Sixel can carry, so a lookup is an index and a comparison.
//
// First-appearance order is not an implementation detail to be improved on:
// it is the order the registers are declared in and therefore part of the
// bytes this encoder emits.
class PaletteIndex {
public:
    static constexpr std::size_t kSlots = 1024;  // ≥ 4x the register ceiling

    // The index for `key`, assigning the next one if this is its first sight.
    // Returns -1 when the palette is full, which the callers treat as "this
    // colour has to be quantized" rather than as a failure.
    int intern(std::uint32_t key, int max_entries) noexcept {
        std::size_t slot = (key * 0x9E3779B1U) % kSlots;
        for (;;) {
            if (slots_[slot].index < 0) {
                if (size_ >= max_entries) return -1;
                slots_[slot] = Slot{key, size_};
                return size_++;
            }
            if (slots_[slot].key == key) return slots_[slot].index;
            slot = (slot + 1) % kSlots;
        }
    }

    int size() const noexcept { return size_; }

private:
    struct Slot {
        std::uint32_t key = 0;
        int index = -1;
    };
    std::array<Slot, kSlots> slots_{};
    int size_ = 0;
};

}  // namespace

std::string encode_sixel(const Image& image, int max_color_registers) {
    if (image.empty()) return {};
    const int w = image.width();
    const int h = image.height();
    const int max_colors = std::clamp(max_color_registers > 0 ? max_color_registers : 256, 1, 256);

    // Decide quantization mode: exact palette if <= 256 unique colors
    // appear (alpha is ignored — Sixel has no alpha channel), else a
    // deterministic 6-level-per-channel cube (<= 216 registers,
    // content-independent so it never exceeds the register budget).
    PaletteIndex exact_probe;
    bool use_cube = false;
    for (int y = 0; y < h && !use_cube; ++y) {
        for (int x = 0; x < w; ++x) {
            const Image::Rgba p = image.pixel(x, y);
            if (exact_probe.intern(pack(p.r, p.g, p.b), max_colors) < 0) {
                use_cube = true;
                break;
            }
        }
    }

    const std::array<int, 3> dimensions = palette_dimensions(max_colors);
    // Quantization is a per-channel function of a byte, so it is 256 answers
    // and not a computation to repeat a million times.
    std::array<std::uint8_t, 256> level_r{};
    std::array<std::uint8_t, 256> level_g{};
    std::array<std::uint8_t, 256> level_b{};
    const bool cube_666 = dimensions == std::array<int, 3>{6, 6, 6};
    for (int value = 0; value < 256; ++value) {
        const auto byte = static_cast<std::uint8_t>(value);
        level_r[static_cast<std::size_t>(value)] = cube_666 ? quantize_level_6(byte) : quantize_level(byte, dimensions[0]);
        level_g[static_cast<std::size_t>(value)] = cube_666 ? quantize_level_6(byte) : quantize_level(byte, dimensions[1]);
        level_b[static_cast<std::size_t>(value)] = cube_666 ? quantize_level_6(byte) : quantize_level(byte, dimensions[2]);
    }

    PaletteIndex palette;
    std::vector<std::array<std::uint8_t, 3>> palette_colors;
    palette_colors.reserve(static_cast<std::size_t>(max_colors));
    std::vector<int> pixel_index(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Image::Rgba p = image.pixel(x, y);
            if (use_cube) {
                p.r = level_r[p.r];
                p.g = level_g[p.g];
                p.b = level_b[p.b];
            }
            const int index = palette.intern(pack(p.r, p.g, p.b), max_colors);
            // Every colour reaching here is either one of at most max_colors
            // exact ones or a cube cell, so the palette cannot overflow; the
            // clamp is what makes that a fact rather than a belief.
            const int entry = index < 0 ? 0 : index;
            if (static_cast<std::size_t>(entry) == palette_colors.size())
                palette_colors.push_back({p.r, p.g, p.b});
            pixel_index[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                         static_cast<std::size_t>(x)] = entry;
        }
    }

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

    // One band, one pass. The obvious loop — for every colour, look through
    // the band for it — reads the band once per register, so a photograph
    // costs 216 sweeps of the same pixels before a single byte is written,
    // and the encode grows with the palette rather than with the picture.
    // Instead each pixel is visited once and deposits its bit in its own
    // colour's column mask; only the colours the band actually contains are
    // then written out. The output is unchanged, register order and run
    // lengths included — this is the same encoding, arrived at once.
    const std::size_t palette_size = palette_colors.size();
    std::vector<unsigned char> column_bits(palette_size * static_cast<std::size_t>(w), 0);
    std::vector<bool> present(palette_size, false);
    std::vector<int> present_order;
    present_order.reserve(palette_size);
    std::string run;
    run.reserve(static_cast<std::size_t>(w));

    for (int band_y = 0; band_y < h; band_y += 6) {
        const int band_height = std::min(6, h - band_y);
        present_order.clear();
        for (int dy = 0; dy < band_height; ++dy) {
            const std::size_t row = static_cast<std::size_t>(band_y + dy) * static_cast<std::size_t>(w);
            const auto bit = static_cast<unsigned char>(1u << dy);
            for (int x = 0; x < w; ++x) {
                const auto color = static_cast<std::size_t>(pixel_index[row + static_cast<std::size_t>(x)]);
                if (!present[color]) {
                    present[color] = true;
                    present_order.push_back(static_cast<int>(color));
                }
                column_bits[color * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] |= bit;
            }
        }
        // Ascending register order, which is the order the previous reading
        // of this band produced and therefore the order the bytes are in.
        std::sort(present_order.begin(), present_order.end());

        bool first_color_in_band = true;
        for (const int color_idx : present_order) {
            if (!first_color_in_band) out += '$';
            first_color_in_band = false;
            out += '#';
            out += std::to_string(color_idx);

            unsigned char* const bits = &column_bits[static_cast<std::size_t>(color_idx) * static_cast<std::size_t>(w)];
            run.assign(static_cast<std::size_t>(w), '\0');
            for (int x = 0; x < w; ++x) {
                run[static_cast<std::size_t>(x)] = static_cast<char>(63 + bits[x]);
                bits[x] = 0;  // cleared as it is read, so the next band starts empty
            }
            present[static_cast<std::size_t>(color_idx)] = false;

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
