// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/scene/golden_capture.hpp"

#include <cstdint>

#include "cvision/core/assert.hpp"

namespace ckv::scene {
namespace {

golden::Color to_golden_color(Color c) {
    golden::Color out;
    if (c.is_indexed()) {
        out.kind = golden::Color::Kind::Indexed;
        out.index = c.index();
    } else if (c.is_rgb()) {
        out.kind = golden::Color::Kind::Rgb;
        out.r = c.r();
        out.g = c.g();
        out.b = c.b();
    }
    return out;
}

const char* underline_shape_name(UnderlineShape shape) noexcept {
    switch (shape) {
        case UnderlineShape::Straight: return "";
        case UnderlineShape::Double: return "double";
        case UnderlineShape::Curly: return "curly";
        case UnderlineShape::Dotted: return "dotted";
        case UnderlineShape::Dashed: return "dashed";
    }
    return "";
}

std::vector<std::string> attrs_to_names(Attr attrs) {
    std::vector<std::string> names;
    if (has_attr(attrs, Attr::Bold)) names.emplace_back("bold");
    if (has_attr(attrs, Attr::Dim)) names.emplace_back("dim");
    if (has_attr(attrs, Attr::Italic)) names.emplace_back("italic");
    if (has_attr(attrs, Attr::Underline)) names.emplace_back("underline");
    if (has_attr(attrs, Attr::Reverse)) names.emplace_back("reverse");
    if (has_attr(attrs, Attr::Strike)) names.emplace_back("strike");
    return names;
}

const char* cursor_shape_name(CursorShape shape) noexcept {
    switch (shape) {
        case CursorShape::Block: return "block";
        case CursorShape::Bar: return "bar";
        case CursorShape::Underline: return "underline";
    }
    return "block";
}

void set_cursor(golden::Document& doc, CursorState cursor) {
    doc.cursor.visible = cursor.visible;
    if (cursor.visible) {
        doc.cursor.col = cursor.position.x;
        doc.cursor.row = cursor.position.y;
        doc.cursor.shape = cursor_shape_name(cursor.shape);
    }
}

// Fills doc.cols/rows/grid/stylemap/styles from `surface`'s cells,
// deduplicating styles. Shared by capture() and capture_frame().
void capture_grid_and_styles(const Surface& surface, golden::Document& doc) {
    doc.cols = surface.size().width;
    doc.rows = surface.size().height;

    std::vector<Style> styles;
    const auto style_index = [&](Style s) -> int {
        for (std::size_t i = 0; i < styles.size(); ++i)
            if (styles[i] == s) return static_cast<int>(i);
        CKV_ASSERT(styles.size() < golden::style_alphabet.size());
        styles.push_back(s);
        return static_cast<int>(styles.size() - 1);
    };

    doc.grid.reserve(static_cast<std::size_t>(doc.rows));
    doc.stylemap.reserve(static_cast<std::size_t>(doc.rows));
    for (int y = 0; y < doc.rows; ++y) {
        std::string grid_row;
        std::string style_row;
        style_row.reserve(static_cast<std::size_t>(doc.cols));
        for (int x = 0; x < doc.cols; ++x) {
            const Cell& cell = surface.at(Point{x, y});
            grid_row += cell.grapheme();
            const int idx = style_index(cell.style());
            style_row += golden::style_alphabet[static_cast<std::size_t>(idx)];
        }
        doc.grid.push_back(std::move(grid_row));
        doc.stylemap.push_back(std::move(style_row));
    }

    doc.styles.reserve(styles.size());
    for (const Style& s : styles) {
        golden::StyleSpec spec;
        spec.fg = to_golden_color(s.fg);
        spec.bg = to_golden_color(s.bg);
        spec.attrs = attrs_to_names(s.attrs);
        // An underline's shape and colour describe a rule that is being
        // drawn; with no underline there is nothing for them to describe.
        if (has_attr(s.attrs, Attr::Underline)) {
            spec.underline = underline_shape_name(s.underline);
            spec.underline_color = to_golden_color(s.underline_color);
        }
        doc.styles.push_back(std::move(spec));
    }
}

}  // namespace

std::string image_content_hash(const Image& image) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;  // FNV-1a 64-bit offset basis
    constexpr std::uint64_t prime = 0x100000001b3ULL;
    const std::uint8_t* data = image.data();
    const std::size_t byte_count =
        static_cast<std::size_t>(image.stride()) * static_cast<std::size_t>(image.height());
    for (std::size_t i = 0; i < byte_count; ++i) {
        hash ^= data[i];
        hash *= prime;
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = digits[hash & 0xF];
        hash >>= 4;
    }
    return out;
}

golden::Document capture(const Surface& surface, CursorState cursor) {
    golden::Document doc;
    set_cursor(doc, cursor);
    capture_grid_and_styles(surface, doc);

    for (const RasterRegion& region : surface.raster_regions()) {
        // Surface::add_raster_region enforces image != nullptr and
        // non-empty as an invariant — no defensive fallback needed here.
        golden::RasterRegion g;
        g.id = region.id;
        g.anchor_col = region.anchor.x;
        g.anchor_row = region.anchor.y;
        g.span_cols = region.anchor.width;
        g.span_rows = region.anchor.height;
        g.pixel_width = region.image->width();
        g.pixel_height = region.image->height();
        g.hash = image_content_hash(*region.image);
        g.fallback_active = region.fallback_active;
        doc.rasters.push_back(std::move(g));
    }

    return doc;
}

golden::Document capture_frame(const Compositor& compositor, CursorState cursor) {
    golden::Document doc;
    set_cursor(doc, cursor);
    capture_grid_and_styles(compositor.frame(), doc);

    int synthetic_id = 1;
    for (const RasterSlice& vr : compositor.visible_rasters()) {
        golden::RasterRegion g;
        g.id = synthetic_id++;
        g.anchor_col = vr.visible_rect.x;
        g.anchor_row = vr.visible_rect.y;
        g.span_cols = vr.visible_rect.width;
        g.span_rows = vr.visible_rect.height;
        g.pixel_width = vr.image->width();
        g.pixel_height = vr.image->height();
        g.hash = image_content_hash(*vr.image);
        g.fallback_active = vr.fallback_active;
        doc.rasters.push_back(std::move(g));
    }

    return doc;
}

}  // namespace ckv::scene
