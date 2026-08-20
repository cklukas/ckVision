// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "frame_svg.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string_view>
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
    const auto append_path = [&](Color stroke, std::string_view width) {
        out += "<path d=\"" + path + "\" fill=\"none\" stroke=\"" + hex(stroke) +
               "\" stroke-width=\"" + std::string(width) +
               "\" stroke-linecap=\"square\" stroke-linejoin=\"" + join + "\"/>\n";
    };

    if (info.style == scene::LineStyle::Double) {
        append_path(fg, bold ? "4.5" : "4");
        append_path(bg, "1.5");
    } else if (info.style == scene::LineStyle::Heavy) {
        append_path(fg, bold ? "3.5" : "3");
    } else {
        append_path(fg, bold ? "2" : "1.5");
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

std::string render_svg(const FrameView& frame, const Image* raster_plane, Size cell_pixels,
                       const FrameSvgOptions& options) {
    const int width_px = frame.size().width * cell_pixels.width;
    const int height_px = frame.size().height * cell_pixels.height;

    std::string out;
    out += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + std::to_string(width_px) + "\" height=\"" +
           std::to_string(height_px) + "\" viewBox=\"0 0 " + std::to_string(width_px) + " " +
           std::to_string(height_px) + "\">\n";
    out += "<rect x=\"0\" y=\"0\" width=\"" + std::to_string(width_px) + "\" height=\"" +
           std::to_string(height_px) + "\" fill=\"" + hex(kDefaultBg) + "\"/>\n";

    // Merge equal neighboring cell backgrounds into maximal rectangles.
    // Emitting one SVG rectangle per cell creates hairline seams when SVG is
    // converted to PDF, even though the logical cells are fully opaque.
    const Size size = frame.size();
    std::vector<bool> covered(static_cast<std::size_t>(size.width) *
                              static_cast<std::size_t>(size.height));
    const auto background_at = [&](int x, int y) {
        const Style style = frame.at(Point{x, y}).style();
        Color fg = resolve(style.fg, kDefaultFg);
        Color bg = resolve(style.bg, kDefaultBg);
        if (has_attr(style.attrs, Attr::Reverse)) std::swap(fg, bg);
        return bg;
    };
    const auto background_index = [size](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(size.width) +
               static_cast<std::size_t>(x);
    };

    for (int y = 0; y < size.height; ++y) {
        for (int x = 0; x < size.width; ++x) {
            if (covered[background_index(x, y)]) continue;
            const Color bg = background_at(x, y);
            int run_width = 1;
            while (x + run_width < size.width &&
                   !covered[background_index(x + run_width, y)] &&
                   background_at(x + run_width, y) == bg)
                ++run_width;

            int run_height = 1;
            bool extends = true;
            while (y + run_height < size.height && extends) {
                for (int rx = x; rx < x + run_width; ++rx) {
                    if (covered[background_index(rx, y + run_height)] ||
                        !(background_at(rx, y + run_height) == bg)) {
                        extends = false;
                        break;
                    }
                }
                if (extends) ++run_height;
            }

            for (int ry = y; ry < y + run_height; ++ry)
                for (int rx = x; rx < x + run_width; ++rx)
                    covered[background_index(rx, ry)] = true;

            if (!(bg == kDefaultBg)) {
                out += "<rect x=\"" + std::to_string(x * cell_pixels.width) + "\" y=\"" +
                       std::to_string(y * cell_pixels.height) + "\" width=\"" +
                       std::to_string(run_width * cell_pixels.width) + "\" height=\"" +
                       std::to_string(run_height * cell_pixels.height) + "\" fill=\"" +
                       hex(bg) + "\"/>\n";
            }
        }
    }

    for (int y = 0; y < frame.size().height; ++y) {
        for (int x = 0; x < frame.size().width; ++x) {
            const Cell& cell = frame.at(Point{x, y});
            if (cell.is_continuation()) continue;  // covered by the preceding wide glyph

            Style style = cell.style();
            Color fg = resolve(style.fg, kDefaultFg);
            Color bg = resolve(style.bg, kDefaultBg);
            if (has_attr(style.attrs, Attr::Reverse)) std::swap(fg, bg);

            const int px = x * cell_pixels.width;
            const int py = y * cell_pixels.height;

            if (cell.grapheme().empty() || cell.grapheme() == " ") continue;

            if (const std::optional<scene::JunctionGlyphInfo> junction =
                    scene::classify_junction_glyph(cell.grapheme())) {
                out += render_junction_svg(*junction, px, py, cell_pixels, fg, bg,
                                           has_attr(style.attrs, Attr::Bold));
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
                   "\" font-family=\"" + options.font_family + "\" font-size=\"" +
                   std::to_string(std::max(1, cell_pixels.height - 4)) + "\" xml:space=\"preserve\" " + style_attrs +
                   ">" + xml_escape(cell.grapheme()) + "</text>\n";
        }
    }

    if (raster_plane != nullptr) {
        out += "<g id=\"raster-plane\">\n";
        const PixelBounds bounds = opaque_bounds(*raster_plane);
        if (!bounds.empty()) {
            const std::vector<std::uint8_t> png = encode_png(*raster_plane, bounds);
            out += "<image x=\"" + std::to_string(bounds.left) + "\" y=\"" +
                   std::to_string(bounds.top) + "\" width=\"" +
                   std::to_string(bounds.right - bounds.left) + "\" height=\"" +
                   std::to_string(bounds.bottom - bounds.top) +
                   "\" preserveAspectRatio=\"none\" image-rendering=\"pixelated\" "
                   "href=\"data:image/png;base64," +
                   base64(png) + "\"/>\n";
        }
        out += "</g>\n";
    }

    out += "</svg>\n";
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
