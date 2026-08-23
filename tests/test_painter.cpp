// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/scene/painter.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::scene::LineStyle;
using ckv::scene::Painter;
using ckv::scene::Surface;

namespace {

Surface make_surface(int w, int h) {
    return Surface(ckv::Size{w, h}, ckv::Cell::from_grapheme(".", ckv::Style{}));
}

std::string row_text(const Surface& s, int y) {
    std::string out;
    for (int x = 0; x < s.size().width; ++x) out += s.at(ckv::Point{x, y}).grapheme();
    return out;
}

}  // namespace

// --- View-relative (translated) painters --------------------------------------

CK_TEST(translated_painter_draws_in_local_space_offset_into_the_parent) {
    Surface s = make_surface(10, 5);
    Painter root(s, ckv::Rect{0, 0, 10, 5});
    Painter child = root.translated(ckv::Point{3, 2}, ckv::Rect{0, 0, 4, 2});
    child.draw_text(ckv::Point{0, 0}, "Hi", ckv::Style{});  // child-local (0,0) -> absolute (3,2)
    CK_CHECK(s.at(ckv::Point{3, 2}).grapheme() == "H");
    CK_CHECK(s.at(ckv::Point{4, 2}).grapheme() == "i");
    CK_CHECK(s.at(ckv::Point{0, 2}).grapheme() == ".");  // untouched, outside the child
}

CK_TEST(translated_painter_clip_is_intersected_with_the_parents_clip) {
    Surface s = make_surface(10, 5);
    Painter root(s, ckv::Rect{0, 0, 6, 5});  // parent only allows columns 0-5
    // Child requests a clip that would extend to column 9 — must be
    // capped by the parent's own clip regardless.
    Painter child = root.translated(ckv::Point{3, 0}, ckv::Rect{0, 0, 10, 5});
    child.fill(ckv::Rect{0, 0, 10, 1}, ckv::Cell::from_grapheme("#", ckv::Style{}));
    CK_CHECK(row_text(s, 0) == "...###....");  // wait: width 10, cols 3-5 filled, 6-9 untouched
}

CK_TEST(nested_translation_composes_offsets) {
    Surface s = make_surface(10, 5);
    Painter root(s, ckv::Rect{0, 0, 10, 5});
    Painter level1 = root.translated(ckv::Point{2, 1}, ckv::Rect{0, 0, 8, 4});
    Painter level2 = level1.translated(ckv::Point{1, 1}, ckv::Rect{0, 0, 5, 2});
    level2.draw_text(ckv::Point{0, 0}, "X", ckv::Style{});  // (2+1, 1+1) = absolute (3,2)
    CK_CHECK(s.at(ckv::Point{3, 2}).grapheme() == "X");
}

CK_TEST(draw_box_and_lines_are_view_relative) {
    Surface s = make_surface(10, 6);
    Painter root(s, ckv::Rect{0, 0, 10, 6});
    Painter child = root.translated(ckv::Point{2, 1}, ckv::Rect{0, 0, 5, 4});
    child.draw_box(ckv::Rect{0, 0, 5, 4}, LineStyle::Single, ckv::Style{});
    CK_CHECK(s.at(ckv::Point{2, 1}).grapheme() == "┌");   // top-left of the box, offset by (2,1)
    CK_CHECK(s.at(ckv::Point{6, 4}).grapheme() == "┘");   // bottom-right: (2+4, 1+3)
}

CK_TEST(draw_image_anchor_is_view_relative_and_stored_absolute) {
    Surface s = make_surface(10, 6);
    Painter root(s, ckv::Rect{0, 0, 10, 6});
    Painter child = root.translated(ckv::Point{3, 2}, ckv::Rect{0, 0, 4, 3});
    const auto image = std::make_shared<ckv::Image>(8, 8);
    child.draw_image(ckv::Rect{0, 0, 2, 2}, 5, image,
                      [](Painter& fp) { fp.fill(ckv::Rect{0, 0, 2, 2}, ckv::Cell::from_grapheme("#", ckv::Style{})); });
    CK_CHECK(s.raster_regions().size() == 1);
    CK_CHECK(s.raster_regions()[0].anchor == (ckv::Rect{3, 2, 2, 2}));  // absolute, not local
    CK_CHECK(s.at(ckv::Point{3, 2}).grapheme() == "#");
}

CK_TEST(fill_writes_the_cell_within_the_clip) {
    Surface s = make_surface(5, 3);
    Painter p(s, ckv::Rect{0, 0, 5, 3});
    p.fill(ckv::Rect{1, 1, 3, 1}, ckv::Cell::from_grapheme("#", ckv::Style{}));
    CK_CHECK(row_text(s, 1) == ".###.");
    CK_CHECK(row_text(s, 0) == ".....");
}

CK_TEST(fill_is_clipped_to_the_painters_clip_rect) {
    Surface s = make_surface(5, 1);
    Painter p(s, ckv::Rect{1, 0, 3, 1});  // columns 1..3 only
    p.fill(ckv::Rect{0, 0, 5, 1}, ckv::Cell::from_grapheme("#", ckv::Style{}));
    CK_CHECK(row_text(s, 0) == ".###.");
}

CK_TEST(draw_text_ascii) {
    Surface s = make_surface(10, 1);
    Painter p(s, ckv::Rect{0, 0, 10, 1});
    p.draw_text(ckv::Point{1, 0}, "Hi", ckv::Style{});
    CK_CHECK(row_text(s, 0) == ".Hi.......");
}

CK_TEST(draw_text_places_a_continuation_cell_after_a_wide_glyph) {
    Surface s = make_surface(5, 1);
    Painter p(s, ckv::Rect{0, 0, 5, 1});
    p.draw_text(ckv::Point{0, 0}, "\xE4\xB8\xAD", ckv::Style{});  // 中, width 2
    CK_CHECK(s.at(ckv::Point{0, 0}).grapheme() == "\xE4\xB8\xAD");
    CK_CHECK(s.at(ckv::Point{1, 0}).is_continuation());
    CK_CHECK(s.at(ckv::Point{2, 0}).grapheme() == ".");  // untouched beyond the glyph
}

CK_TEST(draw_text_truncates_a_wide_glyph_that_would_not_fully_fit) {
    Surface s = make_surface(5, 1);
    Painter p(s, ckv::Rect{0, 0, 5, 1});
    // "AB" (width 2) + 中 (width 2) starting at column 3 in a 5-wide
    // surface: 中 would need columns 3-4... that DOES fit exactly.
    // Use column 4 instead so it does not fit (needs columns 4-5, but
    // the surface is only 5 wide, columns 0-4).
    p.draw_text(ckv::Point{4, 0}, "\xE4\xB8\xAD", ckv::Style{});
    CK_CHECK(row_text(s, 0) == ".....");  // nothing drawn: no partial wide glyphs
}

CK_TEST(draw_text_stops_drawing_everything_after_a_glyph_that_does_not_fit) {
    Surface s = make_surface(4, 1);
    Painter p(s, ckv::Rect{0, 0, 4, 1});
    // "A" fits at col 3, but "B" following it would need col 4 (out of
    // bounds) -- wait, use a wide glyph to force the stop, then ASCII
    // after it that would otherwise fit.
    p.draw_text(ckv::Point{0, 0}, "AB\xE4\xB8\xAD" "C", ckv::Style{});  // A B 中 C
    // A(0) B(1) then 中 needs cols 2-3, fits exactly (surface width 4).
    CK_CHECK(s.at(ckv::Point{0, 0}).grapheme() == "A");
    CK_CHECK(s.at(ckv::Point{1, 0}).grapheme() == "B");
    CK_CHECK(s.at(ckv::Point{2, 0}).grapheme() == "\xE4\xB8\xAD");
    CK_CHECK(s.at(ckv::Point{3, 0}).is_continuation());
    // "C" after it has no room and is never drawn.
}

CK_TEST(draw_text_respects_the_left_clip_edge_while_advancing_correctly) {
    Surface s = make_surface(6, 1);
    Painter p(s, ckv::Rect{3, 0, 3, 1});  // only columns 3-5 are drawable
    p.draw_text(ckv::Point{1, 0}, "ABCDE", ckv::Style{});
    // A(1) B(2) are off the left edge (not drawn, but still advance x);
    // C lands at col 3, D at col 4, E at col 5.
    CK_CHECK(row_text(s, 0) == "...CDE");
}

CK_TEST(hline_and_vline_form_a_cross_via_automatic_junction_merging) {
    Surface s = make_surface(5, 5);
    Painter p(s, ckv::Rect{0, 0, 5, 5});
    p.hline(ckv::Point{0, 2}, 5, LineStyle::Single, ckv::Style{});
    p.vline(ckv::Point{2, 0}, 5, LineStyle::Single, ckv::Style{});
    CK_CHECK(s.at(ckv::Point{2, 2}).grapheme() == "┼");
    CK_CHECK(s.at(ckv::Point{0, 2}).grapheme() == "─");
    CK_CHECK(s.at(ckv::Point{2, 0}).grapheme() == "│");
}

CK_TEST(isolated_painters_replace_unrelated_lines_instead_of_forming_a_junction) {
    Surface s = make_surface(5, 5);
    Painter background(s, ckv::Rect{0, 0, 5, 5});
    background.hline(ckv::Point{0, 2}, 5, LineStyle::Single, ckv::Style{});

    Painter foreground = background.isolated();
    foreground.vline(ckv::Point{2, 0}, 5, LineStyle::Double, ckv::Style{});

    CK_CHECK(s.at(ckv::Point{2, 2}).grapheme() == "║");
}

CK_TEST(resize_does_not_alias_a_live_painters_scope_with_a_new_painter) {
    Surface s = make_surface(5, 5);
    Painter before_resize(s, ckv::Rect{0, 0, 5, 5});
    s.resize(ckv::Size{5, 5});
    before_resize.hline(ckv::Point{0, 2}, 5, LineStyle::Single, ckv::Style{});

    Painter after_resize(s, ckv::Rect{0, 0, 5, 5});
    after_resize.vline(ckv::Point{2, 0}, 5, LineStyle::Double, ckv::Style{});

    CK_CHECK(s.at(ckv::Point{2, 2}).grapheme() == "║");
}

CK_TEST(translated_and_clipped_derivatives_keep_their_logical_junction_scope) {
    Surface s = make_surface(7, 5);
    Painter p(s, ckv::Rect{0, 0, 7, 5});
    p.draw_box(ckv::Rect{0, 0, 7, 5}, LineStyle::Single, ckv::Style{});

    Painter content = p.translated(ckv::Point{0, 0}, ckv::Rect{0, 0, 7, 5})
                          .clipped(ckv::Rect{0, 0, 7, 5});
    content.hline(ckv::Point{0, 2}, 7, LineStyle::Single, ckv::Style{});

    CK_CHECK(s.at(ckv::Point{0, 2}).grapheme() == "├");
    CK_CHECK(s.at(ckv::Point{6, 2}).grapheme() == "┤");
}

CK_TEST(an_ordinary_box_drawing_glyph_does_not_claim_junction_provenance) {
    Surface s = make_surface(5, 5);
    Painter p(s, ckv::Rect{0, 0, 5, 5});
    p.hline(ckv::Point{0, 2}, 5, LineStyle::Single, ckv::Style{});
    p.draw_text(ckv::Point{2, 2}, "─", ckv::Style{});
    p.vline(ckv::Point{2, 0}, 5, LineStyle::Single, ckv::Style{});

    CK_CHECK(s.at(ckv::Point{2, 2}).grapheme() == "│");
}

CK_TEST(vline_ending_partway_through_an_hline_forms_a_tee) {
    Surface s = make_surface(5, 5);
    Painter p(s, ckv::Rect{0, 0, 5, 5});
    p.hline(ckv::Point{0, 2}, 5, LineStyle::Single, ckv::Style{});
    p.vline(ckv::Point{2, 0}, 3, LineStyle::Single, ckv::Style{});  // rows 0,1,2 — stops AT the hline
    CK_CHECK(s.at(ckv::Point{2, 2}).grapheme() == "┴");  // up+left+right, no "down" contributed
}

CK_TEST(draw_box_produces_a_correct_frame_with_all_four_corners) {
    Surface s = make_surface(6, 4);
    Painter p(s, ckv::Rect{0, 0, 6, 4});
    p.draw_box(ckv::Rect{0, 0, 6, 4}, LineStyle::Single, ckv::Style{});
    CK_CHECK(row_text(s, 0) == "┌────┐");
    CK_CHECK(row_text(s, 1) == "│....│");
    CK_CHECK(row_text(s, 2) == "│....│");
    CK_CHECK(row_text(s, 3) == "└────┘");
}

CK_TEST(draw_box_double_style) {
    Surface s = make_surface(4, 3);
    Painter p(s, ckv::Rect{0, 0, 4, 3});
    p.draw_box(ckv::Rect{0, 0, 4, 3}, LineStyle::Double, ckv::Style{});
    CK_CHECK(row_text(s, 0) == "╔══╗");
    CK_CHECK(row_text(s, 2) == "╚══╝");
}

CK_TEST(draw_box_rounded_style_uses_rounded_corners_and_shared_straight_lines) {
    Surface s = make_surface(4, 3);
    Painter p(s, ckv::Rect{0, 0, 4, 3});
    p.draw_box(ckv::Rect{0, 0, 4, 3}, LineStyle::Rounded, ckv::Style{});
    CK_CHECK(row_text(s, 0) == "╭──╮");
    CK_CHECK(row_text(s, 2) == "╰──╯");
}

CK_TEST(nested_boxes_merge_into_a_double_frame_with_junctions_where_they_touch) {
    Surface s = make_surface(7, 5);
    Painter p(s, ckv::Rect{0, 0, 7, 5});
    p.draw_box(ckv::Rect{0, 0, 7, 5}, LineStyle::Single, ckv::Style{});
    p.hline(ckv::Point{0, 2}, 7, LineStyle::Single, ckv::Style{});  // divider through both side walls
    CK_CHECK(s.at(ckv::Point{0, 2}).grapheme() == "├");
    CK_CHECK(s.at(ckv::Point{6, 2}).grapheme() == "┤");
}

CK_TEST(transform_style_changes_style_but_not_grapheme) {
    Surface s = make_surface(3, 1);
    Painter p(s, ckv::Rect{0, 0, 3, 1});
    p.draw_text(ckv::Point{0, 0}, "abc", ckv::Style{ckv::Color::rgb(200, 200, 200), ckv::Color{}, ckv::Attr{}});
    p.transform_style(ckv::Rect{1, 0, 1, 1}, [](ckv::Style st) noexcept {
        st.fg = ckv::Color::rgb(st.fg.r() / 2, st.fg.g() / 2, st.fg.b() / 2);
        return st;
    });
    CK_CHECK(s.at(ckv::Point{0, 0}).style().fg == ckv::Color::rgb(200, 200, 200));  // untouched
    CK_CHECK(s.at(ckv::Point{1, 0}).style().fg == ckv::Color::rgb(100, 100, 100));  // dimmed
    CK_CHECK(s.at(ckv::Point{1, 0}).grapheme() == "b");                             // grapheme unchanged
}

CK_TEST(transform_style_preserves_junction_provenance) {
    Surface s = make_surface(5, 5);
    Painter p(s, ckv::Rect{0, 0, 5, 5});
    p.hline(ckv::Point{0, 2}, 5, LineStyle::Single, ckv::Style{});
    p.transform_style(ckv::Rect{0, 2, 5, 1}, [](ckv::Style style) noexcept {
        style.attrs |= ckv::Attr::Bold;
        return style;
    });
    p.vline(ckv::Point{2, 0}, 5, LineStyle::Single, ckv::Style{});

    CK_CHECK(s.at(ckv::Point{2, 2}).grapheme() == "┼");
}

CK_TEST(draw_image_paints_fallback_and_registers_a_raster_region) {
    Surface s = make_surface(6, 4);
    Painter p(s, ckv::Rect{0, 0, 6, 4});
    const auto image = std::make_shared<ckv::Image>(32, 16);

    p.draw_image(ckv::Rect{1, 1, 3, 2}, /*id=*/7, image, [](Painter& fp) {
        fp.fill(ckv::Rect{0, 0, 100, 100}, ckv::Cell::from_grapheme("#", ckv::Style{}));
    });

    // Fallback content is clamped to the anchor even though it tried to
    // paint a much larger rect.
    CK_CHECK(row_text(s, 1) == ".###..");
    CK_CHECK(row_text(s, 2) == ".###..");
    CK_CHECK(row_text(s, 0) == "......");  // outside the anchor: untouched
    CK_CHECK(row_text(s, 3) == "......");

    CK_CHECK(s.raster_regions().size() == 1);
    CK_CHECK(s.raster_regions()[0].id == 7);
    CK_CHECK(s.raster_regions()[0].anchor == (ckv::Rect{1, 1, 3, 2}));
    CK_CHECK(s.raster_regions()[0].image == image);
    CK_CHECK(s.raster_regions()[0].fallback_active);
}
