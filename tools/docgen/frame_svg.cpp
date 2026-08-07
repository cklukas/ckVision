// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "frame_svg.hpp"

#include <cstdio>

namespace ckv::docgen {

namespace {

constexpr Color kDefaultFg = Color::rgb(192, 192, 192);
constexpr Color kDefaultBg = Color::rgb(0, 0, 0);

Color resolve(Color c, Color fallback) { return c.is_default() ? fallback : c; }

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

}  // namespace

std::string render_frame_svg(const FrameView& frame, const FrameSvgOptions& options) {
    const int width_px = frame.size().width * options.cell_width_px;
    const int height_px = frame.size().height * options.cell_height_px;

    std::string out;
    out += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + std::to_string(width_px) + "\" height=\"" +
           std::to_string(height_px) + "\" viewBox=\"0 0 " + std::to_string(width_px) + " " +
           std::to_string(height_px) + "\">\n";
    out += "<rect x=\"0\" y=\"0\" width=\"" + std::to_string(width_px) + "\" height=\"" +
           std::to_string(height_px) + "\" fill=\"" + hex(kDefaultBg) + "\"/>\n";

    for (int y = 0; y < frame.size().height; ++y) {
        for (int x = 0; x < frame.size().width; ++x) {
            const Cell& cell = frame.at(Point{x, y});
            if (cell.is_continuation()) continue;  // covered by the preceding wide glyph

            Style style = cell.style();
            Color fg = resolve(style.fg, kDefaultFg);
            Color bg = resolve(style.bg, kDefaultBg);
            if (has_attr(style.attrs, Attr::Reverse)) std::swap(fg, bg);

            const int px = x * options.cell_width_px;
            const int py = y * options.cell_height_px;
            const int cell_span_px = options.cell_width_px * (cell.width() > 0 ? cell.width() : 1);

            if (!(bg == kDefaultBg)) {
                out += "<rect x=\"" + std::to_string(px) + "\" y=\"" + std::to_string(py) + "\" width=\"" +
                       std::to_string(cell_span_px) + "\" height=\"" + std::to_string(options.cell_height_px) +
                       "\" fill=\"" + hex(bg) + "\"/>\n";
            }

            if (cell.grapheme().empty() || cell.grapheme() == " ") continue;

            std::string style_attrs = "fill=\"" + hex(fg) + "\"";
            if (has_attr(style.attrs, Attr::Bold)) style_attrs += " font-weight=\"bold\"";
            if (has_attr(style.attrs, Attr::Underline)) style_attrs += " text-decoration=\"underline\"";

            const int baseline_y = py + options.cell_height_px - options.cell_height_px / 4;
            out += "<text x=\"" + std::to_string(px) + "\" y=\"" + std::to_string(baseline_y) +
                   "\" font-family=\"" + options.font_family + "\" font-size=\"" +
                   std::to_string(options.cell_height_px - 4) + "\" xml:space=\"preserve\" " + style_attrs +
                   ">" + xml_escape(cell.grapheme()) + "</text>\n";
        }
    }

    out += "</svg>\n";
    return out;
}

}  // namespace ckv::docgen
