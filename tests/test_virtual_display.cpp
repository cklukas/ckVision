// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/virtual_display.hpp"

#include <string>

#include "cvision/testing/cktest.hpp"
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
    const Cell cell = display.frame().at(Point{1, 1});
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

CK_TEST(virtual_display_decodes_palette_indices_as_the_indices_they_are) {
    // This decoder is the oracle for what the Presenter wrote, and "the host
    // was told index 4" is the fact worth recording. Resolving it here would
    // invent a palette the receiving terminal never consulted.
    VirtualDisplay display(Size{4, 1}, Size{2, 3});
    CK_CHECK(display.write("\x1B[1;1H\x1B[0;31;104mA\x1B[0;38;5;208;48;5;17mB"));
    CK_CHECK(display.frame().at(Point{0, 0}).style().fg == Color::indexed(1));
    CK_CHECK(display.frame().at(Point{0, 0}).style().bg == Color::indexed(12));
    CK_CHECK(display.frame().at(Point{1, 0}).style().fg == Color::indexed(208));
    CK_CHECK(display.frame().at(Point{1, 0}).style().bg == Color::indexed(17));
}

CK_TEST(virtual_display_decodes_underline_shapes_and_their_colour) {
    // The Presenter emits these to a host that says it can draw them, so the
    // decoder that checks the Presenter has to read them back.
    VirtualDisplay display(Size{4, 1}, Size{2, 3});
    CK_CHECK(display.write("\x1B[1;1H\x1B[0;4:3;58:5:9mA\x1B[0;4:2;58:2::10:20:30mB\x1B[0;4mC"));
    const Style curly = display.frame().at(Point{0, 0}).style();
    CK_CHECK(has_attr(curly.attrs, Attr::Underline));
    CK_CHECK(curly.underline == UnderlineShape::Curly);
    CK_CHECK(curly.underline_color == Color::indexed(9));
    const Style doubled = display.frame().at(Point{1, 0}).style();
    CK_CHECK(doubled.underline == UnderlineShape::Double);
    CK_CHECK(doubled.underline_color == Color::rgb(10, 20, 30));
    const Style plain = display.frame().at(Point{2, 0}).style();
    CK_CHECK(has_attr(plain.attrs, Attr::Underline));
    CK_CHECK(plain.underline == UnderlineShape::Straight);
    CK_CHECK(plain.underline_color.is_default());
}

CK_TEST(virtual_display_rejects_sub_parameters_on_controls_that_have_none) {
    // Accepting a colon wherever one appears would let a malformed Presenter
    // sequence decode as though it had meant something.
    VirtualDisplay display(Size{2, 2});
    CK_CHECK(!display.write("\x1B[1:2H"));
    VirtualDisplay other(Size{2, 2});
    CK_CHECK(!other.write("\x1B[1:2m"));
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

CK_TEST(opaque_sixel_replacement_clears_stale_pixels_outside_the_new_raster) {
    VirtualDisplay display(Size{3, 1}, Size{4, 6});
    CK_CHECK(display.write("\x1B[1;1H\x1BP0;0;0q\"1;1;4;6#0;2;100;0;0~~~~\x1B\\"));
    CK_CHECK(display.raster_plane().pixel(3, 0).r == 255);

    // P2=0 declares an opaque background. Its declared raster extent must
    // clear the old right-hand pixels even when the following data paints
    // only the first two columns.
    CK_CHECK(display.write("\x1B[1;1H\x1BP0;0;0q\"1;1;4;6#0;2;0;100;0~~\x1B\\"));
    CK_CHECK(display.raster_plane().pixel(0, 0).g == 255);
    CK_CHECK(display.raster_plane().pixel(1, 0).g == 255);
    CK_CHECK(display.raster_plane().pixel(2, 0).a == 0);
    CK_CHECK(display.raster_plane().pixel(3, 0).a == 0);
}

CK_TEST(synchronized_output_hides_partial_cell_and_raster_updates_until_the_end_marker) {
    VirtualDisplay display(Size{2, 1}, Size{4, 6});
    CK_CHECK(display.feed("\x1B[?2026h\x1B[1;1HX\x1B[1;2H\x1BP0;0;0q\"1;1;1;6#0;2;0;0;100~\x1B\\"));

    CK_CHECK(display.frame().at(Point{0, 0}).grapheme() == " ");
    CK_CHECK(!display.has_raster_pixels());

    CK_CHECK(display.feed("\x1B[?2026l"));
    CK_CHECK(display.finish());
    CK_CHECK(display.frame().at(Point{0, 0}).grapheme() == "X");
    CK_CHECK(display.raster_plane().pixel(4, 0).b == 255);
    CK_CHECK(display.raster_plane().pixel(4, 0).a == 255);
}

CK_TEST(erase_line_clears_wide_cells_and_their_raster_backing) {
    VirtualDisplay display(Size{3, 1}, Size{4, 6});
    CK_CHECK(display.write("\x1B[1;1H\x1BP0;0;0q\"1;1;8;6#0;2;100;0;0~~~~~~~~\x1B\\"));
    CK_CHECK(display.write("\x1B[1;1H\xE4\xB8\xADX"));  // 中 occupies cells 0 and 1

    CK_CHECK(display.write("\x1B[1;2H\x1B[K"));
    for (int x = 0; x < 3; ++x) CK_CHECK(display.frame().at(Point{x, 0}).grapheme() == " ");
    CK_CHECK(!display.has_raster_pixels());
}

CK_TEST(scroll_controls_move_both_cell_and_raster_planes) {
    VirtualDisplay display(Size{2, 2}, Size{4, 6});
    CK_CHECK(display.write("\x1B[2;1HB\x1B[2;1H\x1BP0;0;0q\"1;1;1;6#0;2;100;0;0~\x1B\\"));
    CK_CHECK(display.write("\x1B[1S"));

    CK_CHECK(display.frame().at(Point{0, 0}).grapheme() == "B");
    CK_CHECK(display.frame().at(Point{0, 1}).grapheme() == " ");
    CK_CHECK(display.raster_plane().pixel(0, 0).r == 255);
    CK_CHECK(display.raster_plane().pixel(0, 0).a == 255);
    CK_CHECK(display.raster_plane().pixel(0, 6).a == 0);
}

CK_TEST(incomplete_and_malformed_sixel_sequences_fail_explicitly) {
    VirtualDisplay incomplete(Size{1, 1});
    CK_CHECK(incomplete.feed("\x1BP0;0;0q#0;2;100;0;0~"));
    CK_CHECK(!incomplete.finish());
    CK_CHECK(!incomplete.valid());

    VirtualDisplay malformed(Size{1, 1});
    CK_CHECK(!malformed.write("\x1BP0;0;0q\"1;1;0;6#0;2;100;0;0~\x1B\\"));
    CK_CHECK(!malformed.valid());
}

CK_TEST(virtual_display_rejects_bounded_control_and_repeat_limit_violations) {
    VirtualDisplay maximal_valid_repeat(Size{1, 1});
    CK_CHECK(maximal_valid_repeat.write("\x1BP0;0;0q#0;2;100;0;0!1000000~\x1B\\"));
    CK_CHECK(maximal_valid_repeat.valid());
    CK_CHECK(maximal_valid_repeat.raster_plane().pixel(0, 0).r == 255);
    CK_CHECK(maximal_valid_repeat.raster_plane().pixel(0, 0).a == 255);

    VirtualDisplay far_cursor(Size{1, 1});
    CK_CHECK(far_cursor.write("\x1B[2147483647;2147483647H"
                              "\x1BP0;0;0q\"1;1;1000000;6#0;2;100;0;0!1000000~\x1B\\"));
    CK_CHECK(far_cursor.valid());
    CK_CHECK(!far_cursor.has_raster_pixels());

    VirtualDisplay excessive_repeat(Size{1, 1});
    CK_CHECK(!excessive_repeat.write("\x1BP0;0;0q#0;2;100;0;0!1000001~\x1B\\"));
    CK_CHECK(!excessive_repeat.valid());

    VirtualDisplay excessive_csi(Size{1, 1});
    std::string csi = "\x1B[";
    csi.append(257, '1');
    CK_CHECK(!excessive_csi.feed(csi));
    CK_CHECK(!excessive_csi.valid());
}

CK_TEST(sixel_raster_is_clipped_at_the_virtual_terminal_pixel_boundary) {
    VirtualDisplay display(Size{2, 1}, Size{4, 6});
    // Eight painted columns begin at the second four-pixel cell. Exactly the
    // final four display pixels are retained; the other four are off-screen.
    CK_CHECK(display.write("\x1B[1;2H\x1BP0;0;0q\"1;1;8;6#0;2;100;0;0~~~~~~~~\x1B\\"));
    for (int x = 0; x < 4; ++x) CK_CHECK(display.raster_plane().pixel(x, 0).a == 0);
    for (int x = 4; x < 8; ++x) {
        CK_CHECK(display.raster_plane().pixel(x, 0).r == 255);
        CK_CHECK(display.raster_plane().pixel(x, 0).a == 255);
    }
}

CK_TEST(multiple_sixel_rasters_remain_independently_positioned_on_one_pixel_plane) {
    VirtualDisplay display(Size{2, 1}, Size{4, 6});
    CK_CHECK(display.write("\x1B[1;1H\x1BP0;0;0q\"1;1;1;6#0;2;100;0;0~\x1B\\"));
    CK_CHECK(display.write("\x1B[1;2H\x1BP0;0;0q\"1;1;1;6#0;2;0;100;0~\x1B\\"));
    CK_CHECK(display.raster_plane().pixel(0, 0).r == 255);
    CK_CHECK(display.raster_plane().pixel(0, 0).g == 0);
    CK_CHECK(display.raster_plane().pixel(4, 0).r == 0);
    CK_CHECK(display.raster_plane().pixel(4, 0).g == 255);
}
