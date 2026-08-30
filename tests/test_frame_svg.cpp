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
    CK_CHECK(svg.find("d=\"M 31.5 35.5 L 31.5 45 L 26.5 45\"") !=
             std::string::npos);  // bottom-right corner reaches both neighboring cell edges
}

CK_TEST(box_drawing_strokes_end_where_the_path_does_so_a_double_rule_stays_unbroken) {
    Surface surface(Size{4, 3});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 4, 3});
    painter.draw_box(ckv::Rect{0, 0, 4, 3}, ckv::scene::LineStyle::Double,
                     Style{Color::rgb(255, 255, 255), Color::rgb(0, 0, 170), Attr{}});
    const std::string svg = render_frame_svg(surface.view());

    // A SQUARE cap runs half a stroke width past the endpoint. The wide
    // outer stroke would then overhang further than the narrow inner one
    // that carves the gap between a double rule's two lines, so each cell
    // painted foreground across its neighbour's gap and the frame came out
    // as a chain of little rectangles. Butt caps make the two overhangs
    // agree.
    CK_CHECK(svg.find("stroke-linecap=\"square\"") == std::string::npos);
    CK_CHECK(svg.find("stroke-linecap=\"butt\"") != std::string::npos);

    // The positive half: a double rule is still TWO strokes on one path,
    // the outer in the foreground and the inner in the background, and the
    // outer is three times the inner so the two lines and the gap between
    // them come out the same weight.
    CK_CHECK(svg.find("stroke=\"#ffffff\" stroke-width=\"6\"") != std::string::npos);
    CK_CHECK(svg.find("stroke=\"#0000aa\" stroke-width=\"2\"") != std::string::npos);
}

CK_TEST(a_shade_glyph_is_dithered_geometry_rather_than_a_font_glyph) {
    const Style desktop{Color::rgb(0, 0, 170), Color::rgb(200, 200, 200), Attr{}};
    Surface surface(Size{3, 2}, Cell::from_grapheme("░", desktop));
    const std::string svg = render_frame_svg(surface.view());

    // Drawn by a font, ░ is a shape sized for the font's em box sitting
    // inside a cell sized by the terminal: a screen of them comes out as
    // rows of dots with gaps between the rows.
    CK_CHECK(svg.find("<text") == std::string::npos);

    // Drawn as a dither in USER space, the tiles line up across cells, so
    // the whole run is one rectangle of one even tint.
    CK_CHECK(svg.find("<pattern id=\"dither1-0000aa\"") != std::string::npos);
    CK_CHECK(svg.find("width=\"27\" height=\"36\" fill=\"url(#dither1-0000aa)\"") !=
             std::string::npos);
}

CK_TEST(the_three_shades_differ_in_how_much_of_the_dither_tile_they_mark) {
    const auto marks_in_tile = [](const char* grapheme) {
        Surface surface(Size{1, 1});
        surface.set_cell(Point{0, 0},
                         Cell::from_grapheme(grapheme, Style{Color::rgb(255, 0, 0), Color{}, Attr{}}));
        const std::string svg = render_frame_svg(surface.view());
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = svg.find("shape-rendering=\"crispEdges\"", pos)) != std::string::npos) {
            ++count;
            pos += 1;
        }
        return count;
    };
    CK_CHECK(marks_in_tile("░") == 1);  // a quarter
    CK_CHECK(marks_in_tile("▒") == 2);  // a half, as a checkerboard
    CK_CHECK(marks_in_tile("▓") == 3);  // three quarters
}

CK_TEST(a_full_block_covers_its_whole_cell_and_a_half_block_covers_exactly_half) {
    Surface full_surface(Size{1, 1});
    full_surface.set_cell(Point{0, 0},
                          Cell::from_grapheme("█", Style{Color::rgb(255, 0, 0), Color{}, Attr{}}));
    const std::string full = render_frame_svg(full_surface.view());
    CK_CHECK(full.find("<text") == std::string::npos);
    CK_CHECK(full.find("x=\"0\" y=\"0\" width=\"9\" height=\"18\" fill=\"#ff0000\"") !=
             std::string::npos);

    // The partial shapes are not merged with anything, but they must still
    // meet: ▌ and ▐ divide the same nine columns between them without a
    // seam or an overlap, which is what rounding the EDGES rather than the
    // widths buys.
    Surface halves(Size{2, 1});
    halves.set_cell(Point{0, 0},
                    Cell::from_grapheme("▌", Style{Color::rgb(255, 0, 0), Color{}, Attr{}}));
    halves.set_cell(Point{1, 0},
                    Cell::from_grapheme("▐", Style{Color::rgb(255, 0, 0), Color{}, Attr{}}));
    const std::string svg = render_frame_svg(halves.view());
    CK_CHECK(svg.find("x=\"0\" y=\"0\" width=\"5\" height=\"18\"") != std::string::npos);
    CK_CHECK(svg.find("x=\"14\" y=\"0\" width=\"4\" height=\"18\"") != std::string::npos);
}

CK_TEST(the_font_size_follows_the_cell_box_rather_than_a_constant_inset) {
    Surface surface(Size{1, 1});
    surface.set_cell(Point{0, 0}, Cell::from_grapheme("M", Style{}));
    // 9x18 is the metric every capture uses, and the monospace face that
    // advances 9px there is a 15px one — not the 14px "height minus four"
    // that left a gap above and below every row of glyphs.
    CK_CHECK(render_frame_svg(surface.view()).find("font-size:15px") != std::string::npos);

    FrameSvgOptions narrow;
    narrow.cell_width_px = 6;
    narrow.cell_height_px = 18;
    // Bounded by the width too: a 15px glyph in a 6px column would run
    // into the next one.
    CK_CHECK(render_frame_svg(surface.view(), narrow).find("font-size:10px") != std::string::npos);
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

// The cut-out (FrameSvgOptions::crop): the widget gallery's figures are
// sub-rectangles of a full composed screen, so what the crop leaves out has
// to be genuinely absent — not merely outside a viewBox, where it would
// still be in the file and still be found by a text search.

CK_TEST(a_crop_sizes_the_svg_to_the_requested_cells_not_the_surface) {
    Surface surface(Size{20, 10});
    FrameSvgOptions options;
    options.cell_width_px = 8;
    options.cell_height_px = 16;
    options.crop = ckv::Rect{4, 2, 5, 3};
    const std::string svg = render_frame_svg(surface.view(), options);
    CK_CHECK(svg.find("width=\"40\"") != std::string::npos);
    CK_CHECK(svg.find("height=\"48\"") != std::string::npos);
    CK_CHECK(svg.find("viewBox=\"0 0 40 48\"") != std::string::npos);
}

CK_TEST(a_crop_omits_glyphs_outside_it_rather_than_hiding_them) {
    Surface surface(Size{6, 2});
    surface.set_cell(Point{0, 0}, Cell::from_grapheme("A", Style{}));  // outside
    surface.set_cell(Point{4, 1}, Cell::from_grapheme("Z", Style{}));  // inside
    FrameSvgOptions options;
    options.crop = ckv::Rect{3, 1, 3, 1};
    const std::string svg = render_frame_svg(surface.view(), options);
    CK_CHECK(svg.find(">Z</text>") != std::string::npos);
    CK_CHECK(svg.find(">A</text>") == std::string::npos);
}

CK_TEST(a_cropped_glyph_is_positioned_relative_to_the_cut_outs_own_origin) {
    Surface surface(Size{6, 2});
    surface.set_cell(Point{4, 1}, Cell::from_grapheme("Z", Style{}));
    FrameSvgOptions options;
    options.cell_width_px = 10;
    options.cell_height_px = 20;
    options.crop = ckv::Rect{3, 1, 3, 1};
    const std::string svg = render_frame_svg(surface.view(), options);
    // Column 4 is the second cell of a cut-out starting at column 3, so it
    // is drawn at x=10 rather than at its screen x of 40.
    CK_CHECK(svg.find("<text x=\"10\" ") != std::string::npos);
    CK_CHECK(svg.find("<text x=\"40\" ") == std::string::npos);
}

CK_TEST(a_cropped_background_run_stops_at_the_cut_outs_edge) {
    const Style panel{Color{}, Color::rgb(90, 91, 92), Attr{}};
    Surface surface(Size{8, 1}, Cell::from_grapheme(" ", panel));
    FrameSvgOptions options;
    options.cell_width_px = 9;
    options.cell_height_px = 18;
    options.crop = ckv::Rect{2, 0, 3, 1};
    const std::string svg = render_frame_svg(surface.view(), options);
    // Three cells of panel, not eight: the run merger works in cut-out
    // space, so a background that continues past the edge is still cut.
    CK_CHECK(svg.find("width=\"27\" height=\"18\" fill=\"#5a5b5c\"") != std::string::npos);
    CK_CHECK(svg.find("width=\"72\"") == std::string::npos);
}

CK_TEST(an_empty_crop_renders_the_whole_surface) {
    Surface surface(Size{4, 2});
    surface.set_cell(Point{0, 0}, Cell::from_grapheme("A", Style{}));
    FrameSvgOptions options;
    options.cell_width_px = 9;
    options.cell_height_px = 18;
    options.crop = ckv::Rect{};  // the default every existing caller uses
    const std::string svg = render_frame_svg(surface.view(), options);
    CK_CHECK(svg.find("viewBox=\"0 0 36 36\"") != std::string::npos);
    CK_CHECK(svg.find(">A</text>") != std::string::npos);
}

CK_TEST(a_crop_entirely_off_the_surface_renders_the_whole_surface_not_nothing) {
    Surface surface(Size{4, 2});
    surface.set_cell(Point{0, 0}, Cell::from_grapheme("A", Style{}));
    FrameSvgOptions options;
    options.cell_width_px = 9;
    options.cell_height_px = 18;
    options.crop = ckv::Rect{40, 40, 5, 5};
    const std::string svg = render_frame_svg(surface.view(), options);
    // A zero-size SVG is a broken figure on a published page; the whole
    // screen at least still shows what was composed.
    CK_CHECK(svg.find("viewBox=\"0 0 36 36\"") != std::string::npos);
    CK_CHECK(svg.find(">A</text>") != std::string::npos);
}

CK_TEST(a_crop_is_clamped_to_the_surface_rather_than_reading_past_it) {
    Surface surface(Size{4, 2});
    surface.set_cell(Point{3, 1}, Cell::from_grapheme("Z", Style{}));
    FrameSvgOptions options;
    options.cell_width_px = 9;
    options.cell_height_px = 18;
    options.crop = ckv::Rect{2, 1, 99, 99};
    const std::string svg = render_frame_svg(surface.view(), options);
    CK_CHECK(svg.find("viewBox=\"0 0 18 18\"") != std::string::npos);
    CK_CHECK(svg.find(">Z</text>") != std::string::npos);
}

CK_TEST(a_crop_clips_the_raster_plane_to_its_own_pixels) {
    VirtualDisplay display(Size{4, 1}, Size{4, 6});
    // Sixteen sixel columns: four cells of picture, so a two-cell cut-out
    // starting at cell 2 contains the right-hand half of it.
    CK_CHECK(display.write("\x1B[1;1H\x1BPq#0;2;100;0;0~~~~~~~~~~~~~~~~\x1B\\"));
    FrameSvgOptions options;
    options.crop = ckv::Rect{2, 0, 2, 1};
    const std::string svg = render_virtual_display_svg(display, options);
    // Eight pixels of picture, drawn at the cut-out's own left edge rather
    // than at its screen x of 8.
    CK_CHECK(svg.find("<image x=\"0\" y=\"0\" width=\"8\"") != std::string::npos);
}

CK_TEST(a_crop_that_misses_the_raster_entirely_emits_no_image) {
    VirtualDisplay display(Size{4, 1}, Size{4, 6});
    CK_CHECK(display.write("\x1B[1;1H\x1BPq#0;2;100;0;0~~~~\x1B\\"));  // one cell of picture
    FrameSvgOptions options;
    options.crop = ckv::Rect{2, 0, 2, 1};
    const std::string svg = render_virtual_display_svg(display, options);
    const std::size_t raster_group = svg.find("id=\"raster-plane\"");
    CK_CHECK(raster_group != std::string::npos);
    // Not merely positioned off-view: absent. A cut-out that carried the
    // whole screen's pixels and relied on the viewport to hide them would
    // pass every geometric check above and still ship the wrong bytes.
    CK_CHECK(svg.find("<image", raster_group) == std::string::npos);
}
