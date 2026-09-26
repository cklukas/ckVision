// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/virtual_display.hpp"

#include <string>
#include <vector>

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
    CK_CHECK(fallback.cell_pixels == (PixelSize{9, 18}));
    CK_CHECK(sixel.cell_pixels == fallback.cell_pixels);
    Capabilities sixel_without_flag = sixel;
    sixel_without_flag.sixel_graphics = false;
    CK_CHECK(sixel_without_flag == fallback);
}

CK_TEST(virtual_display_decodes_cursor_position_truecolor_style_and_text) {
    VirtualDisplay display(Size{4, 2}, PixelSize{2, 3});
    CK_CHECK(display.write("\x1B[2;2H\x1B[0;1;38;2;10;20;30;48;2;40;50;60mQ\x1B[?25l"));
    const Cell cell = display.frame().at(Point{1, 1});
    CK_CHECK(cell.grapheme() == "Q");
    CK_CHECK(cell.style().fg == Color::rgb(10, 20, 30));
    CK_CHECK(cell.style().bg == Color::rgb(40, 50, 60));
    CK_CHECK(has_attr(cell.style().attrs, Attr::Bold));
    CK_CHECK(!display.cursor().visible);
}

CK_TEST(virtual_display_decodes_a_hand_authored_sixel_column_into_rgba_pixels) {
    VirtualDisplay display(Size{2, 2}, PixelSize{4, 6});
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
    VirtualDisplay display(Size{2, 2}, PixelSize{4, 6});
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
    Image image(PixelSize{2, 1});
    image.set_pixel(0, 0, Image::Rgba{255, 0, 0, 0});
    image.set_pixel(1, 0, Image::Rgba{0, 255, 0, 127});
    VirtualDisplay display(Size{2, 1}, PixelSize{2, 6});
    CK_CHECK(display.write("\x1B[1;1H" + encode_sixel(image)));
    CK_CHECK(display.raster_plane().pixel(0, 0).a == 255);
    CK_CHECK(display.raster_plane().pixel(1, 0).a == 255);
}

CK_TEST(virtual_display_decodes_palette_indices_as_the_indices_they_are) {
    // This decoder is the oracle for what the Presenter wrote, and "the host
    // was told index 4" is the fact worth recording. Resolving it here would
    // invent a palette the receiving terminal never consulted.
    VirtualDisplay display(Size{4, 1}, PixelSize{2, 3});
    CK_CHECK(display.write("\x1B[1;1H\x1B[0;31;104mA\x1B[0;38;5;208;48;5;17mB"));
    CK_CHECK(display.frame().at(Point{0, 0}).style().fg == Color::indexed(1));
    CK_CHECK(display.frame().at(Point{0, 0}).style().bg == Color::indexed(12));
    CK_CHECK(display.frame().at(Point{1, 0}).style().fg == Color::indexed(208));
    CK_CHECK(display.frame().at(Point{1, 0}).style().bg == Color::indexed(17));
}

CK_TEST(virtual_display_decodes_underline_shapes_and_their_colour) {
    // The Presenter emits these to a host that says it can draw them, so the
    // decoder that checks the Presenter has to read them back.
    VirtualDisplay display(Size{4, 1}, PixelSize{2, 3});
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
    // OSC 2 sets the window title alone; ckVision emits OSC 0 and never OSC 2.
    CK_CHECK(!display.write("\x1B]2;title\x07"));
    CK_CHECK(!display.valid());
    CK_CHECK(!display.error().empty());
}

CK_TEST(writing_a_cell_clears_sixel_pixels_in_that_cell) {
    VirtualDisplay display(Size{2, 1}, PixelSize{4, 6});
    CK_CHECK(display.write("\x1B[1;1H\x1BPq#0;2;100;0;0~\x1B\\"));
    CK_CHECK(display.has_raster_pixels());
    CK_CHECK(display.write("\x1B[1;1H\x1B[0mX"));
    CK_CHECK(!display.has_raster_pixels());
    CK_CHECK(display.frame().at(Point{0, 0}).grapheme() == "X");
}

CK_TEST(opaque_sixel_replacement_clears_stale_pixels_outside_the_new_raster) {
    VirtualDisplay display(Size{3, 1}, PixelSize{4, 6});
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
    VirtualDisplay display(Size{2, 1}, PixelSize{4, 6});
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
    VirtualDisplay display(Size{3, 1}, PixelSize{4, 6});
    CK_CHECK(display.write("\x1B[1;1H\x1BP0;0;0q\"1;1;8;6#0;2;100;0;0~~~~~~~~\x1B\\"));
    CK_CHECK(display.write("\x1B[1;1H\xE4\xB8\xADX"));  // 中 occupies cells 0 and 1

    CK_CHECK(display.write("\x1B[1;2H\x1B[K"));
    for (int x = 0; x < 3; ++x) CK_CHECK(display.frame().at(Point{x, 0}).grapheme() == " ");
    CK_CHECK(!display.has_raster_pixels());
}

CK_TEST(scroll_controls_move_both_cell_and_raster_planes) {
    VirtualDisplay display(Size{2, 2}, PixelSize{4, 6});
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

    // A cursor sent past the page lands on its last cell (CUP is clamped), so
    // the widest picture the limits allow starts there and is clipped at the
    // plane's edge.
    VirtualDisplay far_cursor(Size{2, 2}, PixelSize{9, 18});
    CK_CHECK(far_cursor.write("\x1B[2147483647;2147483647H"
                              "\x1BP0;0;0q\"1;1;1000000;6#0;2;100;0;0!1000000~\x1B\\"));
    CK_CHECK(far_cursor.valid());
    CK_CHECK(far_cursor.raster_plane().pixel(0, 0).a == 0);
    CK_CHECK(far_cursor.raster_plane().pixel(9, 18).r == 255);
    CK_CHECK(far_cursor.raster_plane().pixel(17, 23).a == 255);

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
    VirtualDisplay display(Size{2, 1}, PixelSize{4, 6});
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
    VirtualDisplay display(Size{2, 1}, PixelSize{4, 6});
    CK_CHECK(display.write("\x1B[1;1H\x1BP0;0;0q\"1;1;1;6#0;2;100;0;0~\x1B\\"));
    CK_CHECK(display.write("\x1B[1;2H\x1BP0;0;0q\"1;1;1;6#0;2;0;100;0~\x1B\\"));
    CK_CHECK(display.raster_plane().pixel(0, 0).r == 255);
    CK_CHECK(display.raster_plane().pixel(0, 0).g == 0);
    CK_CHECK(display.raster_plane().pixel(4, 0).r == 0);
    CK_CHECK(display.raster_plane().pixel(4, 0).g == 255);
}

CK_TEST(a_cursor_position_past_the_page_lands_on_its_last_row_and_column) {
    // ECMA-48 CUP never places the cursor outside the page; the largest
    // parameter an int holds is still just "the last row, the last column".
    VirtualDisplay display(Size{4, 2});
    CK_CHECK(display.write("\x1B[2147483647;2147483647H"));
    CK_CHECK(display.cursor().position == (Point{3, 1}));
    CK_CHECK(display.write("\x1B[99;2H"));
    CK_CHECK(display.cursor().position == (Point{1, 1}));
    CK_CHECK(display.write("\x1B[1;99H"));
    CK_CHECK(display.cursor().position == (Point{3, 0}));
}

CK_TEST(text_after_a_cursor_position_at_int_max_stays_on_the_page) {
    // The fuzz reproducer (fuzz/corpus/virtual_display/
    // cursor-position-at-int-max-then-text.txt): text written after CUP to
    // the largest position used to advance the cursor past the largest int.
    // It now writes the last cell, and each cluster after it wraps and
    // scrolls as a terminal's autowrap does.
    VirtualDisplay display(Size{3, 2});
    CK_CHECK(display.write("\x1B[2147483647;2147483647Habc"));
    CK_CHECK(display.valid());
    CK_CHECK(display.cursor().position == (Point{2, 1}));
    CK_CHECK(display.frame().at(Point{2, 0}).grapheme() == "a");
    CK_CHECK(display.frame().at(Point{0, 1}).grapheme() == "b");
    CK_CHECK(display.frame().at(Point{1, 1}).grapheme() == "c");
}

CK_TEST(text_reaching_the_right_margin_waits_there_and_wraps_with_the_next_cluster) {
    VirtualDisplay display(Size{3, 3});
    CK_CHECK(display.write("\x1B[1;2Hab"));
    // Writing the last column does not move the cursor off the page.
    CK_CHECK(display.cursor().position == (Point{2, 0}));
    // A combining mark joins the cluster in the last column, not the next row.
    CK_CHECK(display.write("\xCC\x81"));
    CK_CHECK(display.frame().at(Point{2, 0}).grapheme() == "b\xCC\x81");
    CK_CHECK(display.write("c"));
    CK_CHECK(display.frame().at(Point{0, 1}).grapheme() == "c");
    CK_CHECK(display.cursor().position == (Point{1, 1}));
    // Cursor addressing cancels a pending wrap.
    CK_CHECK(display.write("\x1B[3;3Hd\x1B[3;1He"));
    CK_CHECK(display.frame().at(Point{2, 2}).grapheme() == "d");
    CK_CHECK(display.frame().at(Point{0, 2}).grapheme() == "e");
}

CK_TEST(a_wide_cluster_that_does_not_fit_the_row_begins_the_next_one) {
    VirtualDisplay display(Size{3, 2});
    // U+4E2D is two columns wide; one column is left after "ab".
    CK_CHECK(display.write("ab\xE4\xB8\xAD"));
    CK_CHECK(display.frame().at(Point{2, 0}).grapheme() == " ");
    CK_CHECK(display.frame().at(Point{0, 1}).grapheme() == "\xE4\xB8\xAD");
    CK_CHECK(display.frame().at(Point{1, 1}).is_continuation());
    CK_CHECK(display.cursor().position == (Point{2, 1}));
}

CK_TEST(a_wrap_from_the_bottom_row_scrolls_the_page_up) {
    VirtualDisplay display(Size{2, 2});
    CK_CHECK(display.write("ab\x1B[2;1Hcd"));
    CK_CHECK(display.cursor().position == (Point{1, 1}));
    CK_CHECK(display.write("e"));
    CK_CHECK(display.frame().at(Point{0, 0}).grapheme() == "c");
    CK_CHECK(display.frame().at(Point{1, 0}).grapheme() == "d");
    CK_CHECK(display.frame().at(Point{0, 1}).grapheme() == "e");
    CK_CHECK(display.frame().at(Point{1, 1}).grapheme() == " ");
    CK_CHECK(display.cursor().position == (Point{1, 1}));
}

namespace {

// The page as text, one string per row, with a blank for an erased cell.
std::vector<std::string> page_rows(const VirtualDisplay& display) {
    std::vector<std::string> rows;
    const FrameView frame = display.frame();
    for (int y = 0; y < frame.size().height; ++y) {
        std::string row;
        for (int x = 0; x < frame.size().width; ++x) row += frame.at(Point{x, y}).grapheme();
        rows.push_back(row);
    }
    return rows;
}

}  // namespace

CK_TEST(erase_in_display_clears_to_the_end_or_from_the_start_of_the_page_not_a_rectangle) {
    // ECMA-48 ED: 0 erases from the cursor to the end of its line and every
    // line below; 1 erases every line above and its own line through the
    // cursor. Neither is the rectangle between the cursor and a corner.
    const std::string fill = "\x1B[1;1Habcd\x1B[2;1Hefgh\x1B[3;1Hijkl";
    VirtualDisplay below(Size{4, 3}, PixelSize{2, 3});
    CK_CHECK(below.write(fill + "\x1B[2;3H\x1B[0J"));
    CK_CHECK(page_rows(below) == (std::vector<std::string>{"abcd", "ef  ", "    "}));
    VirtualDisplay above(Size{4, 3}, PixelSize{2, 3});
    CK_CHECK(above.write(fill + "\x1B[2;2H\x1B[1J"));
    CK_CHECK(page_rows(above) == (std::vector<std::string>{"    ", "  gh", "ijkl"}));
    // xterm's 3 erases the saved lines, which this display does not keep.
    VirtualDisplay saved(Size{4, 3}, PixelSize{2, 3});
    CK_CHECK(saved.write(fill + "\x1B[3J"));
    CK_CHECK(page_rows(saved) == (std::vector<std::string>{"abcd", "efgh", "ijkl"}));
}
