// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/virtual_display.hpp"

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/sixel_encoder.hpp"

using namespace ckv;
using namespace ckv::term;

CK_TEST(named_headless_graphics_profiles_differ_only_in_sixel_support) {
    const Capabilities fallback = headless_no_graphics_profile();
    const Capabilities sixel = headless_sixel_profile();
    CK_CHECK(!fallback.sixel_graphics);
    CK_CHECK(sixel.sixel_graphics);
    CK_CHECK(fallback.cell_pixels == (Size{9, 18}));
    CK_CHECK(sixel.cell_pixels == fallback.cell_pixels);
    Capabilities sixel_without_flag = sixel;
    sixel_without_flag.sixel_graphics = false;
    CK_CHECK(sixel_without_flag == fallback);
}

CK_TEST(virtual_display_decodes_cursor_position_truecolor_style_and_text) {
    VirtualDisplay display(Size{4, 2}, Size{2, 3});
    CK_CHECK(display.write("\x1B[2;2H\x1B[0;1;38;2;10;20;30;48;2;40;50;60mQ\x1B[?25l"));
    const Cell& cell = display.frame().at(Point{1, 1});
    CK_CHECK(cell.grapheme() == "Q");
    CK_CHECK(cell.style().fg == Color::rgb(10, 20, 30));
    CK_CHECK(cell.style().bg == Color::rgb(40, 50, 60));
    CK_CHECK(has_attr(cell.style().attrs, Attr::Bold));
    CK_CHECK(!display.cursor().visible);
}

CK_TEST(virtual_display_decodes_a_hand_authored_sixel_column_into_rgba_pixels) {
    VirtualDisplay display(Size{2, 2}, Size{4, 6});
    // Published Sixel primitives: select/define register 0 as red,
    // then '~' (63 + 63) sets all six vertical pixels in one column.
    CK_CHECK(display.write("\x1B[1;1H\x1BPq#0;2;100;0;0~\x1B\\"));
    CK_CHECK(display.has_raster_pixels());
    for (int y = 0; y < 6; ++y) {
        const Image::Rgba pixel = display.raster_plane().pixel(0, y);
        CK_CHECK(pixel.r == 255);
        CK_CHECK(pixel.g == 0);
        CK_CHECK(pixel.b == 0);
        CK_CHECK(pixel.a == 255);
    }
    CK_CHECK(display.raster_plane().pixel(1, 0).a == 0);
}

CK_TEST(virtual_display_retains_parser_state_across_fragmented_sixel_input) {
    VirtualDisplay display(Size{2, 2}, Size{4, 6});
    CK_CHECK(display.feed("\x1B[1;1H\x1BPq#0;2;"));
    CK_CHECK(display.feed("0;100;0"));
    CK_CHECK(display.feed("@"));  // '@' sets the low bit only
    CK_CHECK(display.feed("\x1B\\"));
    CK_CHECK(display.finish());
    CK_CHECK(display.raster_plane().pixel(0, 0).g == 255);
    CK_CHECK(display.raster_plane().pixel(0, 0).a == 255);
    CK_CHECK(display.raster_plane().pixel(0, 1).a == 0);
}

CK_TEST(virtual_display_retains_parser_state_across_terminal_write_boundaries) {
    HeadlessTerminal term(Size{2, 2}, headless_sixel_profile());
    term.write("\x1BPq#0;2;100;");
    term.write("0;0@");
    term.write("\x1B\\");
    CK_CHECK(term.display().valid());
    CK_CHECK(term.display().raster_plane().pixel(0, 0).r == 255);
    CK_CHECK(term.display().raster_plane().pixel(0, 0).a == 255);

    term.write("X");
    CK_CHECK(term.display().frame().at(Point{0, 0}).grapheme() == "X");
}

CK_TEST(encoder_to_virtual_display_is_opaque_regardless_of_source_alpha) {
    Image image(2, 1);
    image.set_pixel(0, 0, Image::Rgba{255, 0, 0, 0});
    image.set_pixel(1, 0, Image::Rgba{0, 255, 0, 127});
    VirtualDisplay display(Size{2, 1}, Size{2, 6});
    CK_CHECK(display.write("\x1B[1;1H" + encode_sixel(image)));
    CK_CHECK(display.raster_plane().pixel(0, 0).a == 255);
    CK_CHECK(display.raster_plane().pixel(1, 0).a == 255);
}

CK_TEST(virtual_display_rejects_unsupported_output_instead_of_silently_ignoring_it) {
    VirtualDisplay display(Size{2, 2});
    CK_CHECK(!display.write("\x1B]0;title\x07"));
    CK_CHECK(!display.valid());
    CK_CHECK(!display.error().empty());
}

CK_TEST(writing_a_cell_clears_sixel_pixels_in_that_cell) {
    VirtualDisplay display(Size{2, 1}, Size{4, 6});
    CK_CHECK(display.write("\x1B[1;1H\x1BPq#0;2;100;0;0~\x1B\\"));
    CK_CHECK(display.has_raster_pixels());
    CK_CHECK(display.write("\x1B[1;1H\x1B[0mX"));
    CK_CHECK(!display.has_raster_pixels());
    CK_CHECK(display.frame().at(Point{0, 0}).grapheme() == "X");
}
