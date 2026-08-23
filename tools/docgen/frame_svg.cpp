// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "frame_svg.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/core/palette.hpp"
#include "cvision/scene/box_drawing.hpp"

namespace ckv::docgen {

namespace {

constexpr Color kDefaultFg = Color::rgb(192, 192, 192);
constexpr Color kDefaultBg = Color::rgb(0, 0, 0);

// An SVG needs channels, so this is one of the places a palette index stops
// being an index (core/palette.hpp).
Color resolve(Color c, Color fallback) { return resolved_color(c, fallback); }

// SVG spells the underline shapes as decoration styles, and the names line up
// one for one except that a curl is "wavy" there.
const char* svg_underline_style(UnderlineShape shape) noexcept {
    switch (shape) {
        case UnderlineShape::Straight: return "solid";
        case UnderlineShape::Double: return "double";
        case UnderlineShape::Curly: return "wavy";
        case UnderlineShape::Dotted: return "dotted";
        case UnderlineShape::Dashed: return "dashed";
    }
    return "solid";
}

std::string hex(Color c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r(), c.g(), c.b());
    return buf;
}

// Escapes the handful of characters that would otherwise break either
// SVG's XML syntax or its text content.
std::string xml_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            default:
                out += c;
        }
    }
    return out;
}

// A length that is not a whole pixel — a stroke weight, a dither tile —
// written without the trailing zeros `std::to_string` would leave on
// every one of the thousands of them a screenshot contains.
std::string number(double value) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", value);
    std::string text = buf;
    if (text.find('.') != std::string::npos) {
        text.erase(text.find_last_not_of('0') + 1);
        if (!text.empty() && text.back() == '.') text.pop_back();
    }
    return text;
}

// A terminal cell is about five thirds as wide as it is tall in font
// terms: the 15px monospace face that advances 9px also lines up at 18px,
// which is exactly the 9x18 cell every capture uses. Deriving the size
// from the cell box instead of subtracting a constant from its height
// means the glyphs fill their cells the way they do on a real screen —
// four pixels short of the box, they sat in it with a visible gap above
// and below, which is what made a screen of them look striped. Bounded by
// the width as well, so a cell narrower than it is tall is never handed a
// font whose glyphs run into the next column.
int font_size_px(Size cell_pixels) noexcept {
    return std::max(1, std::min(cell_pixels.width * 5 / 3, cell_pixels.height * 5 / 6));
}

// How thick a rule is drawn, in pixels. In the 9x18 cell a terminal font
// draws ─ about two pixels thick, and ═ as two such rules with a gap of
// the same size between them; deriving that from the cell keeps the
// proportion at any other metric rather than thinning out.
double rule_weight(Size cell_pixels) noexcept {
    return std::max(1.0, std::min(cell_pixels.width, cell_pixels.height) / 4.5);
}

// ---------------------------------------------------------------------
// Block Elements (U+2580..U+259F)
//
// These are not text in a terminal: they ARE the cell, filled. Handing
// them to a font draws a shape sized for the font's em box inside a cell
// sized by the terminal, so a desktop tiled with ░ came out as rows of
// dots separated by gaps — the striping and the "huge gap between rows"
// a reader sees in a screenshot that is meant to show what the screen
// looks like. The shapes are therefore described here, in eighths of the
// cell box, and drawn as geometry, the same treatment box drawing gets.

struct BlockPart {
    int x8 = 0;
    int y8 = 0;
    int w8 = 0;
    int h8 = 0;
};

struct BlockFill {
    // 1, 2, 3 for the light/medium/dark shades, which are an even dither
    // rather than a shape; 0 when `parts` describe the fill instead.
    int shade = 0;
    int part_count = 0;
    BlockPart parts[2]{};

    bool fills_whole_cell() const noexcept {
        return shade != 0 || (part_count == 1 && parts[0].w8 == 8 && parts[0].h8 == 8);
    }
};

BlockFill shade_fill(int level) noexcept {
    BlockFill fill;
    fill.shade = level;
    return fill;
}

BlockFill one_part(BlockPart part) noexcept {
    BlockFill fill;
    fill.part_count = 1;
    fill.parts[0] = part;
    return fill;
}

BlockFill two_parts(BlockPart first, BlockPart second) noexcept {
    BlockFill fill;
    fill.part_count = 2;
    fill.parts[0] = first;
    fill.parts[1] = second;
    return fill;
}

std::optional<BlockFill> classify_block_glyph(std::string_view grapheme) {
    // Lower eighths ▁▂▃▄▅▆▇ and left eighths ▉▊▋▌▍▎▏ are written out
    // one by one rather than decoded arithmetically: the two runs go in
    // opposite directions through the code chart, and a table that shows
    // both is easier to check against the chart than the two off-by-one
    // formulas that would replace it.
    if (grapheme == "▀") return one_part({0, 0, 8, 4});  // ▀ upper half
    if (grapheme == "▁") return one_part({0, 7, 8, 1});  // ▁
    if (grapheme == "▂") return one_part({0, 6, 8, 2});  // ▂
    if (grapheme == "▃") return one_part({0, 5, 8, 3});  // ▃
    if (grapheme == "▄") return one_part({0, 4, 8, 4});  // ▄ lower half
    if (grapheme == "▅") return one_part({0, 3, 8, 5});  // ▅
    if (grapheme == "▆") return one_part({0, 2, 8, 6});  // ▆
    if (grapheme == "▇") return one_part({0, 1, 8, 7});  // ▇
    if (grapheme == "█") return one_part({0, 0, 8, 8});  // █ full block
    if (grapheme == "▉") return one_part({0, 0, 7, 8});  // ▉
    if (grapheme == "▊") return one_part({0, 0, 6, 8});  // ▊
    if (grapheme == "▋") return one_part({0, 0, 5, 8});  // ▋
    if (grapheme == "▌") return one_part({0, 0, 4, 8});  // ▌ left half
    if (grapheme == "▍") return one_part({0, 0, 3, 8});  // ▍
    if (grapheme == "▎") return one_part({0, 0, 2, 8});  // ▎
    if (grapheme == "▏") return one_part({0, 0, 1, 8});  // ▏
    if (grapheme == "▐") return one_part({4, 0, 4, 8});  // ▐ right half
    if (grapheme == "░") return shade_fill(1);           // ░
    if (grapheme == "▒") return shade_fill(2);           // ▒
    if (grapheme == "▓") return shade_fill(3);           // ▓
    if (grapheme == "▔") return one_part({0, 0, 8, 1});  // ▔
    if (grapheme == "▕") return one_part({7, 0, 1, 8});  // ▕
    if (grapheme == "▖") return one_part({0, 4, 4, 4});  // ▖
    if (grapheme == "▗") return one_part({4, 4, 4, 4});  // ▗
    if (grapheme == "▘") return one_part({0, 0, 4, 4});  // ▘
    if (grapheme == "▙") return two_parts({0, 0, 4, 4}, {0, 4, 8, 4});  // ▙
    if (grapheme == "▚") return two_parts({0, 0, 4, 4}, {4, 4, 4, 4});  // ▚
    if (grapheme == "▛") return two_parts({0, 0, 8, 4}, {0, 4, 4, 4});  // ▛
    if (grapheme == "▜") return two_parts({0, 0, 8, 4}, {4, 4, 4, 4});  // ▜
    if (grapheme == "▝") return one_part({4, 0, 4, 4});  // ▝
    if (grapheme == "▞") return two_parts({4, 0, 4, 4}, {0, 4, 4, 4});  // ▞
    if (grapheme == "▟") return two_parts({4, 0, 4, 4}, {0, 4, 8, 4});  // ▟
    return std::nullopt;
}

// Eighths of the cell box to pixels. Rounded rather than truncated, and
// converted as an absolute edge rather than as an origin plus a width, so
// the halves of ▌ and ▐ land on the same boundary from both sides and the
// two meet without a seam or an overlap.
int eighths_to_px(int eighths, int extent) noexcept { return (eighths * extent + 4) / 8; }

// The tile a shade is dithered with, in pixels. One pixel at the 9x18
// cell every capture uses, which is what the font's own shade glyphs do
// there; larger cells get a proportionally coarser dither so the tint
// stays visible rather than dissolving into flat colour.
int dither_unit(Size cell_pixels) noexcept {
    return std::max(1, std::min(cell_pixels.width / 9, cell_pixels.height / 18));
}

std::string dither_pattern_id(int shade, Color fg) {
    return "dither" + std::to_string(shade) + "-" + hex(fg).substr(1);
}

// A shade is drawn as a pattern in USER SPACE, not per cell: the tiles
// then line up across every cell that shares the shade, so a desktop-wide
// backdrop reads as one even tint instead of a grid of separately
// phased patches. The tile carries only the foreground marks — the cell's
// own background rect is already underneath it.
std::string dither_pattern(int shade, Color fg, Size cell_pixels) {
    const int unit = dither_unit(cell_pixels);
    const std::string u = std::to_string(unit);
    const std::string span = std::to_string(2 * unit);
    std::string out = "<pattern id=\"" + dither_pattern_id(shade, fg) + "\" width=\"" + span +
                      "\" height=\"" + span + "\" patternUnits=\"userSpaceOnUse\">\n";
    const auto mark = [&](const std::string& x, const std::string& y) {
        out += "<rect x=\"" + x + "\" y=\"" + y + "\" width=\"" + u + "\" height=\"" + u +
               "\" fill=\"" + hex(fg) + "\" shape-rendering=\"crispEdges\"/>\n";
    };
    mark("0", "0");                    // 1 in 4: ░
    if (shade >= 2) mark(u, u);        // 2 in 4, a checkerboard: ▒
    if (shade >= 3) mark(u, "0");      // 3 in 4: ▓
    out += "</pattern>\n";
    return out;
}

struct SvgPoint {
    int x2 = 0;
    int y2 = 0;
};

std::string half_coordinate(int doubled) {
    const bool negative = doubled < 0;
    const unsigned magnitude = static_cast<unsigned>(negative ? -doubled : doubled);
    std::string value = negative ? "-" : "";
    value += std::to_string(magnitude / 2U);
    if ((magnitude & 1U) != 0) value += ".5";
    return value;
}

std::string svg_point(SvgPoint point) {
    return half_coordinate(point.x2) + " " + half_coordinate(point.y2);
}

const char* line_style_name(scene::LineStyle style) noexcept {
    switch (style) {
        case scene::LineStyle::Single: return "single";
        case scene::LineStyle::Double: return "double";
        case scene::LineStyle::Rounded: return "rounded";
        case scene::LineStyle::Heavy: return "heavy";
    }
    return "single";
}

std::string junction_path(scene::Junction junction, scene::LineStyle style, int px, int py,
                          Size cell_pixels) {
    const SvgPoint center{2 * px + cell_pixels.width, 2 * py + cell_pixels.height};
    const SvgPoint left{2 * px - 1, center.y2};
    const SvgPoint right{2 * (px + cell_pixels.width) + 1, center.y2};
    const SvgPoint top{center.x2, 2 * py - 1};
    const SvgPoint bottom{center.x2, 2 * (py + cell_pixels.height) + 1};

    std::vector<SvgPoint> endpoints;
    if (junction.up) endpoints.push_back(top);
    if (junction.down) endpoints.push_back(bottom);
    if (junction.left) endpoints.push_back(left);
    if (junction.right) endpoints.push_back(right);

    if (style == scene::LineStyle::Rounded && endpoints.size() == 2 &&
        junction.up != junction.down && junction.left != junction.right) {
        const SvgPoint horizontal = junction.left ? left : right;
        const SvgPoint vertical = junction.up ? top : bottom;
        const SvgPoint control{horizontal.x2, vertical.y2};
        return "M " + svg_point(horizontal) + " Q " + svg_point(control) + " " +
               svg_point(vertical);
    }

    if (endpoints.size() == 2) {
        return "M " + svg_point(endpoints[0]) + " L " + svg_point(center) + " L " +
               svg_point(endpoints[1]);
    }

    std::string path;
    for (const SvgPoint endpoint : endpoints) {
        if (!path.empty()) path += " ";
        path += "M " + svg_point(center) + " L " + svg_point(endpoint);
    }
    return path;
}

std::string render_junction_svg(const scene::JunctionGlyphInfo& info, int px, int py,
                                Size cell_pixels, Color fg, Color bg, bool bold) {
    const std::string path = junction_path(info.junction, info.style, px, py, cell_pixels);
    const std::string join = info.style == scene::LineStyle::Rounded ? "round" : "miter";
    std::string out = "<g data-box-drawing=\"" + std::string(line_style_name(info.style)) +
                      "\">\n";
    const auto append_path = [&](Color stroke, double width) {
        // BUTT caps, not square. A square cap runs on past the endpoint by
        // half the stroke width, and the arms already reach half a pixel
        // into the neighbouring cell so that consecutive cells join. With
        // the wide outer stroke overhanging further than the narrow inner
        // one that carves a double rule's gap, every cell boundary got a
        // bridge of foreground painted across that gap by the NEXT cell —
        // which is why a double-line window frame came out as a chain of
        // little rectangles instead of two unbroken lines. Butt caps end
        // the stroke where the path ends, so the overhangs match and the
        // rule runs straight through.
        out += "<path d=\"" + path + "\" fill=\"none\" stroke=\"" + hex(stroke) +
               "\" stroke-width=\"" + number(width) +
               "\" stroke-linecap=\"butt\" stroke-linejoin=\"" + join + "\"/>\n";
    };

    const double weight = rule_weight(cell_pixels);
    if (info.style == scene::LineStyle::Double) {
        // Three weights wide with the middle one cut back out: two rules
        // and a gap, all of the same thickness, which is how a font draws
        // ═ in the cell this is standing in for.
        append_path(fg, weight * (bold ? 3.5 : 3.0));
        append_path(bg, weight);
    } else if (info.style == scene::LineStyle::Heavy) {
        append_path(fg, weight * (bold ? 2.5 : 2.0));
    } else {
        append_path(fg, weight * (bold ? 1.5 : 1.0));
    }
    out += "</g>\n";
    return out;
}

struct PixelBounds {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    bool empty() const noexcept { return right <= left || bottom <= top; }
};

PixelBounds opaque_bounds(const Image& image) noexcept {
    PixelBounds bounds{image.width(), image.height(), 0, 0};
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y).a == 0) continue;
            bounds.left = std::min(bounds.left, x);
            bounds.top = std::min(bounds.top, y);
            bounds.right = std::max(bounds.right, x + 1);
            bounds.bottom = std::max(bounds.bottom, y + 1);
        }
    }
    return bounds;
}

void append_be32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

std::uint32_t crc32_update(std::uint32_t crc, std::uint8_t byte) noexcept {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit)
        crc = (crc & 1U) != 0 ? (crc >> 1) ^ 0xEDB88320U : crc >> 1;
    return crc;
}

void append_png_chunk(std::vector<std::uint8_t>& png, std::string_view type,
                      const std::vector<std::uint8_t>& data) {
    append_be32(png, static_cast<std::uint32_t>(data.size()));
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const char c : type) {
        const auto byte = static_cast<std::uint8_t>(static_cast<unsigned char>(c));
        png.push_back(byte);
        crc = crc32_update(crc, byte);
    }
    for (const std::uint8_t byte : data) {
        png.push_back(byte);
        crc = crc32_update(crc, byte);
    }
    append_be32(png, ~crc);
}

std::vector<std::uint8_t> zlib_stored(std::vector<std::uint8_t> raw) {
    std::vector<std::uint8_t> compressed;
    compressed.reserve(raw.size() + raw.size() / 65535U * 5U + 11U);
    compressed.push_back(0x78);  // zlib: deflate, 32 KiB window
    compressed.push_back(0x01);  // fastest/no-compression profile, valid FCHECK

    std::size_t pos = 0;
    while (pos < raw.size()) {
        const std::size_t remaining = raw.size() - pos;
        const auto length = static_cast<std::uint16_t>(std::min<std::size_t>(remaining, 65535U));
        const bool final = static_cast<std::size_t>(length) == remaining;
        compressed.push_back(final ? 0x01 : 0x00);  // final bit + stored-block type
        compressed.push_back(static_cast<std::uint8_t>(length & 0xFFU));
        compressed.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFFU));
        const std::uint16_t inverse = static_cast<std::uint16_t>(~length);
        compressed.push_back(static_cast<std::uint8_t>(inverse & 0xFFU));
        compressed.push_back(static_cast<std::uint8_t>((inverse >> 8) & 0xFFU));
        compressed.insert(compressed.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                          raw.begin() + static_cast<std::ptrdiff_t>(pos + length));
        pos += length;
    }

    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (const std::uint8_t byte : raw) {
        a = (a + byte) % 65521U;
        b = (b + a) % 65521U;
    }
    append_be32(compressed, (b << 16) | a);
    return compressed;
}

std::vector<std::uint8_t> encode_png(const Image& image, PixelBounds bounds) {
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(height) *
                (1U + static_cast<std::size_t>(width) * 4U));
    for (int y = bounds.top; y < bounds.bottom; ++y) {
        raw.push_back(0);  // PNG filter type: None
        for (int x = bounds.left; x < bounds.right; ++x) {
            const Image::Rgba pixel = image.pixel(x, y);
            raw.push_back(pixel.r);
            raw.push_back(pixel.g);
            raw.push_back(pixel.b);
            raw.push_back(pixel.a);
        }
    }

    std::vector<std::uint8_t> png{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<std::uint8_t> header;
    append_be32(header, static_cast<std::uint32_t>(width));
    append_be32(header, static_cast<std::uint32_t>(height));
    header.insert(header.end(), {8, 6, 0, 0, 0});  // 8-bit RGBA, default PNG methods
    append_png_chunk(png, "IHDR", header);
    append_png_chunk(png, "IDAT", zlib_stored(std::move(raw)));
    append_png_chunk(png, "IEND", {});
    return png;
}

std::string base64(const std::vector<std::uint8_t>& bytes) {
    static constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((bytes.size() + 2U) / 3U * 4U);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::size_t count = std::min<std::size_t>(3, bytes.size() - i);
        std::uint32_t bits = static_cast<std::uint32_t>(bytes[i]) << 16;
        if (count > 1) bits |= static_cast<std::uint32_t>(bytes[i + 1]) << 8;
        if (count > 2) bits |= bytes[i + 2];
        out += alphabet[(bits >> 18) & 0x3FU];
        out += alphabet[(bits >> 12) & 0x3FU];
        out += count > 1 ? alphabet[(bits >> 6) & 0x3FU] : '=';
        out += count > 2 ? alphabet[bits & 0x3FU] : '=';
    }
    return out;
}

// The cell rectangle a capture asked for, reduced to one that exists: an
// unset crop is the whole surface, and a crop that misses the surface
// entirely is treated as unset rather than as a request for an empty
// image — a zero-size SVG is a broken figure on a published page, and the
// whole screen at least still shows what was composed.
Rect cropped_view(Size surface, Rect requested) {
    const Rect whole{0, 0, surface.width, surface.height};
    if (requested.empty()) return whole;
    const Rect clamped = requested.intersected(whole);
    return clamped.empty() ? whole : clamped;
}

// Sweeps a cell grid into maximal rectangles of cells that share a key,
// calling emit(x, y, width, height, key) once per rectangle. Two planes
// need it — the cell backgrounds, and the block-element fills drawn over
// them — because one SVG rectangle per cell leaves hairline seams when
// the SVG is converted to PDF even though the cells themselves are
// opaque, and a desktop-sized dithered backdrop is two thousand of them.
template <typename KeyAt, typename Emit>
void merge_cell_runs(Size size, KeyAt key_at, Emit emit) {
    std::vector<bool> covered(static_cast<std::size_t>(size.width) *
                              static_cast<std::size_t>(size.height));
    const auto index = [size](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(size.width) +
               static_cast<std::size_t>(x);
    };

    for (int y = 0; y < size.height; ++y) {
        for (int x = 0; x < size.width; ++x) {
            if (covered[index(x, y)]) continue;
            const auto key = key_at(x, y);
            int run_width = 1;
            while (x + run_width < size.width && !covered[index(x + run_width, y)] &&
                   key_at(x + run_width, y) == key)
                ++run_width;

            int run_height = 1;
            bool extends = true;
            while (y + run_height < size.height && extends) {
                for (int rx = x; rx < x + run_width; ++rx) {
                    if (covered[index(rx, y + run_height)] || !(key_at(rx, y + run_height) == key)) {
                        extends = false;
                        break;
                    }
                }
                if (extends) ++run_height;
            }

            for (int ry = y; ry < y + run_height; ++ry)
                for (int rx = x; rx < x + run_width; ++rx) covered[index(rx, ry)] = true;

            emit(x, y, run_width, run_height, key);
        }
    }
}

// What a block-element cell paints over its own background, where that is
// one flat thing the whole cell wide: a solid foreground (█) or an even
// dither (░▒▓). Cells that agree on it merge; the partial shapes — halves,
// quadrants, eighths — do not, and are drawn one cell at a time.
struct UniformFill {
    int shade = 0;  // 1..3 for ░▒▓, 4 for a solid █, 0 for "nothing to merge"
    Color fg{};

    bool operator==(const UniformFill& other) const noexcept {
        return shade == other.shade && (shade == 0 || fg == other.fg);
    }
};

std::string render_svg(const FrameView& frame, const Image* raster_plane, Size cell_pixels,
                       const FrameSvgOptions& options) {
    const Rect view = cropped_view(frame.size(), options.crop);
    const int origin_x_px = view.x * cell_pixels.width;
    const int origin_y_px = view.y * cell_pixels.height;
    const int width_px = view.width * cell_pixels.width;
    const int height_px = view.height * cell_pixels.height;

    std::string out;
    out += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + std::to_string(width_px) + "\" height=\"" +
           std::to_string(height_px) + "\" viewBox=\"0 0 " + std::to_string(width_px) + " " +
           std::to_string(height_px) + "\">\n";
    out += "<rect x=\"0\" y=\"0\" width=\"" + std::to_string(width_px) + "\" height=\"" +
           std::to_string(height_px) + "\" fill=\"" + hex(kDefaultBg) + "\"/>\n";

    // The font is the same for every glyph on the surface, and a screen
    // is thousands of glyphs: stating the family and size once in a
    // stylesheet rather than on each <text> roughly halves the file. A
    // documentation build writes a hundred of these.
    out += "<style>text{font-family:" + options.font_family + ";font-size:" +
           std::to_string(font_size_px(cell_pixels)) + "px}</style>\n";

    // Every dither the shades below need, gathered as they are used and
    // put back here — a pattern must be defined once and referenced,
    // and a desktop backdrop refers to the same one a thousand times.
    const std::size_t defs_at = out.size();
    std::string defs;
    std::vector<std::string> defined;
    const auto dither_reference = [&](int shade, Color fg) {
        std::string id = dither_pattern_id(shade, fg);
        if (std::find(defined.begin(), defined.end(), id) == defined.end()) {
            defs += dither_pattern(shade, fg, cell_pixels);
            defined.push_back(id);
        }
        return "url(#" + id + ")";
    };

    const Size size = Size{view.width, view.height};
    const auto styled_colors = [&](int x, int y) {
        const Style style = frame.at(Point{view.x + x, view.y + y}).style();
        Color fg = resolve(style.fg, kDefaultFg);
        Color bg = resolve(style.bg, kDefaultBg);
        if (has_attr(style.attrs, Attr::Reverse)) std::swap(fg, bg);
        return std::pair<Color, Color>{fg, bg};
    };

    merge_cell_runs(
        size, [&](int x, int y) { return styled_colors(x, y).second; },
        [&](int x, int y, int run_width, int run_height, Color bg) {
            if (bg == kDefaultBg) return;
            out += "<rect x=\"" + std::to_string(x * cell_pixels.width) + "\" y=\"" +
                   std::to_string(y * cell_pixels.height) + "\" width=\"" +
                   std::to_string(run_width * cell_pixels.width) + "\" height=\"" +
                   std::to_string(run_height * cell_pixels.height) + "\" fill=\"" + hex(bg) +
                   "\"/>\n";
        });

    // The block-element plane, over the backgrounds and under everything
    // else. Merged the same way and for the same reason: the classic
    // desktop is a screenful of ░, and a screenful of separate rectangles
    // is what a page renderer turns into visible seams.
    merge_cell_runs(
        size,
        [&](int x, int y) {
            const Cell& cell = frame.at(Point{view.x + x, view.y + y});
            const std::optional<BlockFill> fill = classify_block_glyph(cell.grapheme());
            if (!fill.has_value() || !fill->fills_whole_cell()) return UniformFill{};
            return UniformFill{fill->shade != 0 ? fill->shade : 4, styled_colors(x, y).first};
        },
        [&](int x, int y, int run_width, int run_height, UniformFill fill) {
            if (fill.shade == 0) return;
            const std::string paint =
                fill.shade == 4 ? hex(fill.fg) : dither_reference(fill.shade, fill.fg);
            out += "<rect x=\"" + std::to_string(x * cell_pixels.width) + "\" y=\"" +
                   std::to_string(y * cell_pixels.height) + "\" width=\"" +
                   std::to_string(run_width * cell_pixels.width) + "\" height=\"" +
                   std::to_string(run_height * cell_pixels.height) + "\" fill=\"" + paint +
                   "\"/>\n";
        });

    for (int y = view.y; y < view.bottom(); ++y) {
        for (int x = view.x; x < view.right(); ++x) {
            const Cell& cell = frame.at(Point{x, y});
            if (cell.is_continuation()) continue;  // covered by the preceding wide glyph

            Style style = cell.style();
            Color fg = resolve(style.fg, kDefaultFg);
            Color bg = resolve(style.bg, kDefaultBg);
            if (has_attr(style.attrs, Attr::Reverse)) std::swap(fg, bg);

            const int px = x * cell_pixels.width - origin_x_px;
            const int py = y * cell_pixels.height - origin_y_px;

            if (cell.grapheme().empty() || cell.grapheme() == " ") continue;

            if (const std::optional<scene::JunctionGlyphInfo> junction =
                    scene::classify_junction_glyph(cell.grapheme())) {
                out += render_junction_svg(*junction, px, py, cell_pixels, fg, bg,
                                           has_attr(style.attrs, Attr::Bold));
                continue;
            }

            if (const std::optional<BlockFill> block = classify_block_glyph(cell.grapheme())) {
                // The ones that fill their whole cell are already down,
                // merged with their neighbours by the plane above; what is
                // left is the halves, quadrants and eighths, which have a
                // shape of their own and nothing to merge with.
                if (block->fills_whole_cell()) continue;
                for (int part = 0; part < block->part_count; ++part) {
                    const BlockPart p = block->parts[part];
                    const int left = eighths_to_px(p.x8, cell_pixels.width);
                    const int right = eighths_to_px(p.x8 + p.w8, cell_pixels.width);
                    const int top = eighths_to_px(p.y8, cell_pixels.height);
                    const int bottom = eighths_to_px(p.y8 + p.h8, cell_pixels.height);
                    out += "<rect x=\"" + std::to_string(px + left) + "\" y=\"" +
                           std::to_string(py + top) + "\" width=\"" + std::to_string(right - left) +
                           "\" height=\"" + std::to_string(bottom - top) + "\" fill=\"" + hex(fg) +
                           "\"/>\n";
                }
                continue;
            }

            std::string style_attrs = "fill=\"" + hex(fg) + "\"";
            if (has_attr(style.attrs, Attr::Bold)) style_attrs += " font-weight=\"bold\"";
            if (has_attr(style.attrs, Attr::Underline)) {
                // The plain rule is what SVG draws for a bare underline, so
                // saying so again would only add a token to every screenshot
                // that has ever had underlined text in it.
                style_attrs += " text-decoration=\"underline\"";
                if (style.underline != UnderlineShape::Straight) {
                    style_attrs += " text-decoration-style=\"";
                    style_attrs += svg_underline_style(style.underline);
                    style_attrs += "\"";
                }
                if (!style.underline_color.is_default())
                    style_attrs += " text-decoration-color=\"" + hex(resolve(style.underline_color, fg)) + "\"";
            }

            const int baseline_y = py + cell_pixels.height - cell_pixels.height / 4;
            out += "<text x=\"" + std::to_string(px) + "\" y=\"" + std::to_string(baseline_y) +
                   "\" xml:space=\"preserve\" " + style_attrs + ">" + xml_escape(cell.grapheme()) +
                   "</text>\n";
        }
    }

    if (raster_plane != nullptr) {
        out += "<g id=\"raster-plane\">\n";
        PixelBounds bounds = opaque_bounds(*raster_plane);
        // Clip the raster to the cut-out too: a Sixel picture the crop
        // only partly contains must be cut where the cells are, not
        // dragged in whole and drawn over the neighbouring figure.
        bounds.left = std::max(bounds.left, origin_x_px);
        bounds.top = std::max(bounds.top, origin_y_px);
        bounds.right = std::min(bounds.right, origin_x_px + width_px);
        bounds.bottom = std::min(bounds.bottom, origin_y_px + height_px);
        if (!bounds.empty()) {
            const std::vector<std::uint8_t> png = encode_png(*raster_plane, bounds);
            out += "<image x=\"" + std::to_string(bounds.left - origin_x_px) + "\" y=\"" +
                   std::to_string(bounds.top - origin_y_px) + "\" width=\"" +
                   std::to_string(bounds.right - bounds.left) + "\" height=\"" +
                   std::to_string(bounds.bottom - bounds.top) +
                   "\" preserveAspectRatio=\"none\" image-rendering=\"pixelated\" "
                   "href=\"data:image/png;base64," +
                   base64(png) + "\"/>\n";
        }
        out += "</g>\n";
    }

    out += "</svg>\n";
    if (!defs.empty()) out.insert(defs_at, "<defs>\n" + defs + "</defs>\n");
    return out;
}

}  // namespace

std::string render_frame_svg(const FrameView& frame, const FrameSvgOptions& options) {
    return render_svg(frame, nullptr, Size{options.cell_width_px, options.cell_height_px}, options);
}

std::string render_virtual_display_svg(const term::VirtualDisplay& display,
                                       const FrameSvgOptions& options) {
    return render_svg(display.frame(), &display.raster_plane(), display.cell_pixels(), options);
}

}  // namespace ckv::docgen
