// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/sixel_encoder.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace ckv::term {
namespace {

int percent(std::uint8_t c) noexcept { return (static_cast<int>(c) * 100 + 127) / 255; }

std::uint32_t pack_percent(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    return (static_cast<std::uint32_t>(percent(r)) << 16) |
           (static_cast<std::uint32_t>(percent(g)) << 8) |
           static_cast<std::uint32_t>(percent(b));
}

std::array<std::uint8_t, 3> resolve_background(Color bg) noexcept {
    if (bg.is_rgb()) return {bg.r(), bg.g(), bg.b()};
    if (bg.is_indexed() && bg.index() < 16) {
        static constexpr std::array<std::array<std::uint8_t, 3>, 16> kAnsi{{
            {0, 0, 0},       {170, 0, 0},     {0, 170, 0},     {170, 85, 0},
            {0, 0, 170},     {170, 0, 170},   {0, 170, 170},   {170, 170, 170},
            {85, 85, 85},    {255, 85, 85},   {85, 255, 85},   {255, 255, 85},
            {85, 85, 255},   {255, 85, 255},  {85, 255, 255},  {255, 255, 255},
        }};
        return kAnsi[bg.index()];
    }
    return {0, 0, 0};
}

class PaletteIndex {
public:
    static constexpr std::size_t kSlots = 1024;  // >= 4x the register ceiling

    int intern(std::uint32_t key, int max_entries) noexcept {
        static_assert((kSlots & (kSlots - 1)) == 0, "kSlots must be a power of two");
        constexpr std::size_t kMask = kSlots - 1;
        std::size_t slot = (key * 0x9E3779B1U) & kMask;
        for (;;) {
            if (slots_[slot].index < 0) {
                if (size_ >= max_entries) return -1;
                slots_[slot] = Slot{key, size_};
                return size_++;
            }
            if (slots_[slot].key == key) return slots_[slot].index;
            slot = (slot + 1) & kMask;
        }
    }

private:
    struct Slot {
        std::uint32_t key = 0;
        int index = -1;
    };
    std::array<Slot, kSlots> slots_{};
    int size_ = 0;
};

struct HistBin {
    std::uint32_t count = 0;
    std::uint64_t sum_r = 0;
    std::uint64_t sum_g = 0;
    std::uint64_t sum_b = 0;
    std::uint8_t r5 = 0;
    std::uint8_t g5 = 0;
    std::uint8_t b5 = 0;
};

struct ColorBox {
    std::size_t begin = 0;
    std::size_t end = 0;
    std::uint8_t min_r = 31, max_r = 0;
    std::uint8_t min_g = 31, max_g = 0;
    std::uint8_t min_b = 31, max_b = 0;
    std::uint64_t count = 0;
};

ColorBox make_box(const std::vector<HistBin>& bins, std::size_t begin, std::size_t end) noexcept {
    ColorBox box;
    box.begin = begin;
    box.end = end;
    for (std::size_t i = begin; i < end; ++i) {
        box.min_r = std::min(box.min_r, bins[i].r5);
        box.max_r = std::max(box.max_r, bins[i].r5);
        box.min_g = std::min(box.min_g, bins[i].g5);
        box.max_g = std::max(box.max_g, bins[i].g5);
        box.min_b = std::min(box.min_b, bins[i].b5);
        box.max_b = std::max(box.max_b, bins[i].b5);
        box.count += bins[i].count;
    }
    return box;
}

std::vector<std::array<std::uint8_t, 3>> build_adaptive_palette(
    const std::vector<std::array<std::uint8_t, 3>>& pixels,
    const std::vector<std::uint8_t>& opaque,
    int max_colors) {
    static constexpr std::size_t kBins = 32768;  // 5-5-5 RGB
    std::vector<HistBin> hist(kBins);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        if (!opaque[i]) continue;
        const auto& c = pixels[i];
        const std::uint8_t r5 = c[0] >> 3;
        const std::uint8_t g5 = c[1] >> 3;
        const std::uint8_t b5 = c[2] >> 3;
        const std::size_t idx = (static_cast<std::size_t>(r5) << 10) |
                                (static_cast<std::size_t>(g5) << 5) |
                                static_cast<std::size_t>(b5);
        HistBin& bin = hist[idx];
        if (bin.count == 0) {
            bin.r5 = r5;
            bin.g5 = g5;
            bin.b5 = b5;
        }
        ++bin.count;
        bin.sum_r += c[0];
        bin.sum_g += c[1];
        bin.sum_b += c[2];
    }

    std::vector<HistBin> active;
    for (const HistBin& bin : hist) {
        if (bin.count > 0) active.push_back(bin);
    }
    if (active.empty()) return {{0, 0, 0}};

    std::vector<ColorBox> boxes;
    boxes.reserve(static_cast<std::size_t>(max_colors));
    boxes.push_back(make_box(active, 0, active.size()));

    while (static_cast<int>(boxes.size()) < max_colors) {
        int best_idx = -1;
        std::uint64_t best_score = 0;
        for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
            const ColorBox& b = boxes[static_cast<std::size_t>(i)];
            if (b.end - b.begin <= 1) continue;
            const std::uint64_t range = std::max({
                b.max_r - b.min_r, b.max_g - b.min_g, b.max_b - b.min_b});
            const std::uint64_t score = range * b.count + 1ULL;
            if (best_idx < 0 || score > best_score) {
                best_score = score;
                best_idx = i;
            }
        }
        if (best_idx < 0) break;

        ColorBox box = boxes[static_cast<std::size_t>(best_idx)];
        const int dr = box.max_r - box.min_r;
        const int dg = box.max_g - box.min_g;
        const int db = box.max_b - box.min_b;
        const int axis = (dr >= dg && dr >= db) ? 0 : (dg >= db ? 1 : 2);

        std::sort(active.begin() + static_cast<std::ptrdiff_t>(box.begin),
                  active.begin() + static_cast<std::ptrdiff_t>(box.end),
                  [axis](const HistBin& a, const HistBin& b) {
                      const std::uint8_t va = axis == 0 ? a.r5 : (axis == 1 ? a.g5 : a.b5);
                      const std::uint8_t vb = axis == 0 ? b.r5 : (axis == 1 ? b.g5 : b.b5);
                      if (va != vb) return va < vb;
                      if (a.r5 != b.r5) return a.r5 < b.r5;
                      if (a.g5 != b.g5) return a.g5 < b.g5;
                      return a.b5 < b.b5;
                  });

        const std::uint64_t half_count = box.count / 2;
        std::uint64_t running = 0;
        std::size_t split = box.begin + 1;
        for (std::size_t i = box.begin; i + 1 < box.end; ++i) {
            running += active[i].count;
            split = i + 1;
            if (running >= half_count) break;
        }

        boxes[static_cast<std::size_t>(best_idx)] = make_box(active, box.begin, split);
        boxes.push_back(make_box(active, split, box.end));
    }

    std::vector<std::array<std::uint8_t, 3>> palette;
    palette.reserve(boxes.size());
    PaletteIndex dedup;
    for (const ColorBox& box : boxes) {
        std::uint64_t sr = 0, sg = 0, sb = 0, cnt = 0;
        for (std::size_t i = box.begin; i < box.end; ++i) {
            sr += active[i].sum_r;
            sg += active[i].sum_g;
            sb += active[i].sum_b;
            cnt += active[i].count;
        }
        if (cnt == 0) continue;
        const auto r = static_cast<std::uint8_t>((sr + cnt / 2) / cnt);
        const auto g = static_cast<std::uint8_t>((sg + cnt / 2) / cnt);
        const auto b = static_cast<std::uint8_t>((sb + cnt / 2) / cnt);
        if (dedup.intern(pack_percent(r, g, b), max_colors) >= 0 &&
            static_cast<int>(palette.size()) < max_colors) {
            // Only push when newly interned
            if (palette.size() < static_cast<std::size_t>(max_colors)) {
                bool exists = false;
                for (const auto& existing : palette) {
                    if (pack_percent(existing[0], existing[1], existing[2]) == pack_percent(r, g, b)) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) palette.push_back({r, g, b});
            }
        }
    }
    if (palette.empty()) palette.push_back({0, 0, 0});
    return palette;
}

int nearest_palette_color(const std::vector<std::array<std::uint8_t, 3>>& palette,
                          int r, int g, int b) noexcept {
    int best = 0;
    int best_dist = std::numeric_limits<int>::max();
    for (int i = 0; i < static_cast<int>(palette.size()); ++i) {
        const auto& c = palette[static_cast<std::size_t>(i)];
        const int dr = r - static_cast<int>(c[0]);
        const int dg = g - static_cast<int>(c[1]);
        const int db = b - static_cast<int>(c[2]);
        const int dist = dr * dr + dg * dg + db * db;
        if (dist < best_dist) {
            best_dist = dist;
            best = i;
            if (dist == 0) break;
        }
    }
    return best;
}

}  // namespace

std::string encode_sixel(const Image& image, int max_color_registers) {
    SixelEncodeOptions options;
    options.max_color_registers = max_color_registers;
    return encode_sixel(image, options);
}

std::string encode_sixel(const Image& image, const SixelEncodeOptions& options) {
    if (image.empty()) return {};
    const int w = image.width();
    const int h = image.height();
    const int max_colors = std::clamp(
        options.max_color_registers > 0 ? options.max_color_registers : 256, 1, 256);

    const std::size_t pixel_count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    std::vector<std::array<std::uint8_t, 3>> composited(pixel_count);
    std::vector<std::uint8_t> opaque(pixel_count, 1);
    bool has_transparent_hole = false;
    const std::array<std::uint8_t, 3> bg = resolve_background(options.background);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t idx =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                static_cast<std::size_t>(x);
            const Image::Rgba p = image.pixel(x, y);
            if (options.preserve_transparent_holes && p.a == 0) {
                opaque[idx] = 0;
                has_transparent_hole = true;
                composited[idx] = bg;
            } else if (!options.preserve_transparent_holes || p.a == 255) {
                composited[idx] = {p.r, p.g, p.b};
            } else {
                const int a = p.a;
                const int inv = 255 - a;
                composited[idx] = {
                    static_cast<std::uint8_t>((static_cast<int>(p.r) * a + bg[0] * inv + 127) / 255),
                    static_cast<std::uint8_t>((static_cast<int>(p.g) * a + bg[1] * inv + 127) / 255),
                    static_cast<std::uint8_t>((static_cast<int>(p.b) * a + bg[2] * inv + 127) / 255),
                };
            }
        }
    }

    PaletteIndex palette;
    std::vector<std::array<std::uint8_t, 3>> palette_colors;
    palette_colors.reserve(static_cast<std::size_t>(max_colors));
    std::vector<std::uint8_t> pixel_index(pixel_count, 0);

    bool overflowed = false;
    for (std::size_t i = 0; i < pixel_count; ++i) {
        if (!opaque[i]) continue;
        const auto& c = composited[i];
        const int index = palette.intern(pack_percent(c[0], c[1], c[2]), max_colors);
        if (index < 0) {
            overflowed = true;
            break;
        }
        if (static_cast<std::size_t>(index) == palette_colors.size())
            palette_colors.push_back(c);
        pixel_index[i] = static_cast<std::uint8_t>(index);
    }

    if (overflowed) {
        palette_colors = build_adaptive_palette(composited, opaque, max_colors);
        // Stateless 4x4 Bayer ordered dithering keyed on global (anchor_px_x + x, anchor_px_y + y)
        // so cropped/sliced regions share identical phase across boundaries.
        static constexpr int kBayer4x4[4][4] = {
            {-8, 0, -6, 2},
            {4, -4, 6, -2},
            {-5, 3, -7, 1},
            {7, -1, 5, -3},
        };
        std::unordered_map<std::uint32_t, std::uint8_t> lut;
        lut.reserve(512);
        for (int y = 0; y < h; ++y) {
            const int by = (options.anchor_px_y + y) & 3;
            for (int x = 0; x < w; ++x) {
                const std::size_t idx =
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                    static_cast<std::size_t>(x);
                if (!opaque[idx]) continue;
                const int dither = kBayer4x4[by][(options.anchor_px_x + x) & 3] * 2;
                const auto& c = composited[idx];
                const int r = std::clamp(static_cast<int>(c[0]) + dither, 0, 255);
                const int g = std::clamp(static_cast<int>(c[1]) + dither, 0, 255);
                const int b = std::clamp(static_cast<int>(c[2]) + dither, 0, 255);
                const std::uint32_t key =
                    (static_cast<std::uint32_t>(r) << 16) |
                    (static_cast<std::uint32_t>(g) << 8) |
                    static_cast<std::uint32_t>(b);
                auto [it, inserted] = lut.try_emplace(key, std::uint8_t{0});
                if (inserted) {
                    it->second = static_cast<std::uint8_t>(
                        nearest_palette_color(palette_colors, r, g, b));
                }
                pixel_index[idx] = it->second;
            }
        }
    }

    std::string out = has_transparent_hole ? "\x1BP0;1;0q\"1;1;" : "\x1BP0;0;0q\"1;1;";
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

    const std::size_t palette_size = palette_colors.size();
    std::vector<unsigned char> column_bits(palette_size * static_cast<std::size_t>(w), 0);
    std::vector<unsigned char> present(palette_size, 0);
    std::vector<int> present_order;
    present_order.reserve(palette_size);

    for (int band_y = 0; band_y < h; band_y += 6) {
        const int band_height = std::min(6, h - band_y);
        present_order.clear();
        for (int dy = 0; dy < band_height; ++dy) {
            const std::size_t row = static_cast<std::size_t>(band_y + dy) * static_cast<std::size_t>(w);
            const auto bit = static_cast<unsigned char>(1u << dy);
            for (int x = 0; x < w; ++x) {
                const std::size_t idx = row + static_cast<std::size_t>(x);
                if (!opaque[idx]) continue;
                const auto color = static_cast<std::size_t>(pixel_index[idx]);
                if (present[color] == 0) {
                    present[color] = 1;
                    present_order.push_back(static_cast<int>(color));
                }
                column_bits[color * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] |= bit;
            }
        }
        std::sort(present_order.begin(), present_order.end());

        bool first_color_in_band = true;
        for (const int color_idx : present_order) {
            if (!first_color_in_band) out += '$';
            first_color_in_band = false;
            out += '#';
            out += std::to_string(color_idx);

            const unsigned char* bits =
                &column_bits[static_cast<std::size_t>(color_idx) * static_cast<std::size_t>(w)];
            int x = 0;
            while (x < w) {
                const unsigned char b = bits[x];
                int run = 1;
                while (x + run < w && bits[x + run] == b) ++run;
                const char sixel_char = static_cast<char>(63 + b);
                if (run <= 3) {
                    out.append(static_cast<std::size_t>(run), sixel_char);
                } else {
                    out += '!';
                    out += std::to_string(run);
                    out += sixel_char;
                }
                x += run;
            }
            std::fill(column_bits.begin() + static_cast<std::size_t>(color_idx) * static_cast<std::size_t>(w),
                      column_bits.begin() + static_cast<std::size_t>(color_idx + 1) * static_cast<std::size_t>(w),
                      static_cast<unsigned char>(0));
            present[static_cast<std::size_t>(color_idx)] = 0;
        }
        if (band_y + 6 < h) out += '-';
    }

    out += "\x1B\\";
    return out;
}

}  // namespace ckv::term
