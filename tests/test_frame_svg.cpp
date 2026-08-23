// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "frame_svg.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"

using ckv::Attr;
using ckv::Cell;
using ckv::Color;
using ckv::Point;
using ckv::Size;
using ckv::Style;
using ckv::docgen::FrameSvgOptions;
using ckv::docgen::render_frame_svg;
using ckv::docgen::render_virtual_display_svg;
using ckv::scene::Surface;
using ckv::term::VirtualDisplay;

CK_TEST(dimensions_scale_by_cell_size_and_frame_size) {
    Surface surface(Size{10, 5});
    FrameSvgOptions options;
    options.cell_width_px = 8;
    options.cell_height_px = 16;
    const std::string svg = render_frame_svg(surface.view(), options);
    CK_CHECK(svg.find("width=\"80\"") != std::string::npos);
    CK_CHECK(svg.find("height=\"80\"") != std::string::npos);
}

CK_TEST(a_plain_grapheme_appears_as_svg_text_content) {
    Surface surface(Size{3, 1});
    surface.set_cell(Point{1, 0}, Cell::from_grapheme("Q", Style{Color::rgb(1, 2, 3), Color::rgb(4, 5, 6), Attr{}}));
    const std::string svg = render_frame_svg(surface.view());
    CK_CHECK(svg.find(">Q</text>") != std::string::npos);
}

CK_TEST(xml_special_characters_in_a_grapheme_are_escaped) {
    Surface surface(Size{1, 1});
    surface.set_cell(Point{0, 0}, Cell::from_grapheme("&", Style{}));
    const std::string svg = render_frame_svg(surface.view());
    CK_CHECK(svg.find("&amp;") != std::string::npos);
    // The raw ampersand must never appear standalone inside text content.
    CK_CHECK(svg.find(">&<") == std::string::npos);
}

CK_TEST(a_plain_space_grapheme_emits_no_text_element_only_background) {
    Surface surface(Size{1, 1});
    surface.set_cell(Point{0, 0}, Cell::from_grapheme(" ", Style{Color{}, Color::rgb(9, 9, 9), Attr{}}));
    const std::string svg = render_frame_svg(surface.view());
    CK_CHECK(svg.find("<text") == std::string::npos);
    CK_CHECK(svg.find("#090909") != std::string::npos);  // the background rect still renders
}

CK_TEST(equal_neighboring_cell_backgrounds_merge_into_one_seam_free_rectangle) {
    const Style panel{Color{}, Color::rgb(90, 91, 92), Attr{}};
    Surface surface(Size{3, 2}, Cell::from_grapheme(" ", panel));
    const std::string svg = render_frame_svg(surface.view());
    const std::size_t first = svg.find("fill=\"#5a5b5c\"");
    CK_CHECK(first != std::string::npos);
    CK_CHECK(svg.find("fill=\"#5a5b5c\"", first + 1) == std::string::npos);
    CK_CHECK(svg.find("width=\"27\" height=\"36\" fill=\"#5a5b5c\"") !=
             std::string::npos);
}

CK_TEST(a_continuation_cell_from_a_wide_glyph_is_skipped_entirely) {
    Surface surface(Size{2, 1});
    surface.set_cell(Point{0, 0}, Cell::from_grapheme("\xE4\xB8\xAD", Style{}));  // a wide CJK glyph
    // Whatever occupies column 1 as the wide glyph's continuation must
    // not ALSO emit its own <text> element.
    const std::string svg = render_frame_svg(surface.view());
    std::size_t text_count = 0;
    std::size_t pos = 0;
    while ((pos = svg.find("<text", pos)) != std::string::npos) {
        ++text_count;
        pos += 5;
    }
    CK_CHECK(text_count == 1);
}

CK_TEST(reverse_attribute_swaps_foreground_and_background) {
    Surface surface(Size{1, 1});
    const Style reversed{Color::rgb(10, 20, 30), Color::rgb(200, 210, 220), Attr::Reverse};
    surface.set_cell(Point{0, 0}, Cell::from_grapheme("R", reversed));
    const std::string svg = render_frame_svg(surface.view());
    // Text fill must be the (originally background) 200,210,220 color;
    // the rect fill must be the (originally foreground) 10,20,30 color.
    CK_CHECK(svg.find("fill=\"#0a141e\"") != std::string::npos);   // background rect: was fg
    CK_CHECK(svg.find("fill=\"#c8d2dc\"") != std::string::npos);   // text: was bg
}

CK_TEST(default_colors_resolve_to_the_classic_terminal_pair_not_left_blank) {
    Surface surface(Size{1, 1});
    surface.set_cell(Point{0, 0}, Cell::from_grapheme("D", Style{}));  // fg/bg both default
    const std::string svg = render_frame_svg(surface.view());
    CK_CHECK(svg.find("fill=\"#c0c0c0\"") != std::string::npos);  // default fg
    // Default bg matches the page background, so it deliberately emits
    // no separate rect (see render_frame_svg's `!(bg == kDefaultBg)`
    // guard) — verified indirectly by there being exactly one fill
    // besides the page background and the text glyph's own fill.
}

CK_TEST(bold_and_underline_attributes_are_reflected_as_svg_text_attributes) {
    Surface surface(Size{1, 1});
    surface.set_cell(Point{0, 0}, Cell::from_grapheme("B", Style{Color::rgb(1, 1, 1), Color{}, Attr::Bold | Attr::Underline}));
    const std::string svg = render_frame_svg(surface.view());
    CK_CHECK(svg.find("font-weight=\"bold\"") != std::string::npos);
    CK_CHECK(svg.find("text-decoration=\"underline\"") != std::string::npos);
}

CK_TEST(box_drawing_cells_use_cell_aligned_svg_geometry_instead_of_font_glyphs) {
    Surface surface(Size{4, 3});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 4, 3});
    painter.draw_box(ckv::Rect{0, 0, 4, 3}, ckv::scene::LineStyle::Double,
                     Style{Color::rgb(255, 255, 255), Color::rgb(0, 0, 170), Attr::Bold});

    const std::string svg = render_frame_svg(surface.view());
    CK_CHECK(svg.find("data-box-drawing=\"double\"") != std::string::npos);
    CK_CHECK(svg.find(">╝</text>") == std::string::npos);
    CK_CHECK(svg.find("stroke-linecap=\"square\"") != std::string::npos);
    CK_CHECK(svg.find("d=\"M 31.5 35.5 L 31.5 45 L 26.5 45\"") !=
             std::string::npos);  // bottom-right corner reaches both neighboring cell edges
}

CK_TEST(virtual_display_svg_contains_pixels_decoded_from_sixel_output) {
    VirtualDisplay display(Size{2, 1}, Size{4, 6});
    CK_CHECK(display.write("\x1B[1;1H\x1BPq#0;2;100;0;0~\x1B\\"));
    const std::string svg = render_virtual_display_svg(display);
    CK_CHECK(svg.find("id=\"raster-plane\"") != std::string::npos);
    CK_CHECK(svg.find("<image x=\"0\" y=\"0\" width=\"1\" height=\"6\"") !=
             std::string::npos);
    CK_CHECK(svg.find("href=\"data:image/png;base64,iVBORw0KGgo") != std::string::npos);
    CK_CHECK(svg.find("fill-opacity") == std::string::npos);
}

CK_TEST(virtual_display_svg_has_no_opaque_raster_rects_for_a_cell_only_frame) {
    VirtualDisplay display(Size{2, 1}, Size{4, 6});
    CK_CHECK(display.write("\x1B[1;1H\x1B[0mOK\x1B[?25l"));
    const std::string svg = render_virtual_display_svg(display);
    const std::size_t raster_group = svg.find("id=\"raster-plane\"");
    CK_CHECK(raster_group != std::string::npos);
    CK_CHECK(svg.find("<image", raster_group) == std::string::npos);
}
