// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/cell.hpp"
#include "cvision/core/color.hpp"
#include "cvision/core/palette.hpp"
#include "cvision/core/shadow_style.hpp"
#include "cvision/core/style.hpp"

#include "cvision/testing/cktest.hpp"

CK_TEST(color_default_and_rgb) {
    CK_CHECK(ckv::Color{}.is_default());
    CK_CHECK(ckv::Color::default_color().is_default());
    const ckv::Color c = ckv::Color::rgb(10, 20, 30);
    CK_CHECK(!c.is_default());
    CK_CHECK(c.is_rgb());
    CK_CHECK(c.r() == 10);
    CK_CHECK(c.g() == 20);
    CK_CHECK(c.b() == 30);
    CK_CHECK(ckv::Color::rgb(1, 2, 3) == ckv::Color::rgb(1, 2, 3));
    CK_CHECK(ckv::Color::rgb(1, 2, 3) != ckv::Color::rgb(1, 2, 4));
    CK_CHECK(ckv::Color::default_color() != ckv::Color::rgb(0, 0, 0));
}

CK_TEST(a_palette_colour_stays_the_index_it_was_asked_for) {
    // The whole point of the third kind: "the palette's red" is a different
    // fact from "this particular red", and only the first can be re-themed.
    const ckv::Color indexed = ckv::Color::indexed(1);
    CK_CHECK(indexed.is_indexed());
    CK_CHECK(!indexed.is_default());
    CK_CHECK(!indexed.is_rgb());
    CK_CHECK(indexed.index() == 1);
    CK_CHECK(indexed == ckv::Color::indexed(1));
    CK_CHECK(indexed != ckv::Color::indexed(2));
    // ...and it is not the same value as the colour it currently resolves to.
    CK_CHECK(indexed != ckv::palette_color(1));
}

CK_TEST(the_palette_resolves_indices_the_way_the_convention_fixed_them) {
    // A program computes a cube entry arithmetically and expects the levels
    // every terminal shares; the grey ramp is the same kind of promise.
    CK_CHECK(ckv::palette_color(16) == ckv::Color::rgb(0, 0, 0));
    CK_CHECK(ckv::palette_color(16 + 36 * 5 + 6 * 5 + 5) == ckv::Color::rgb(255, 255, 255));
    CK_CHECK(ckv::palette_color(16 + 36 * 2) == ckv::Color::rgb(135, 0, 0));
    CK_CHECK(ckv::palette_color(232) == ckv::Color::rgb(8, 8, 8));
    CK_CHECK(ckv::palette_color(255) == ckv::Color::rgb(238, 238, 238));
}

CK_TEST(resolving_a_colour_yields_channels_whatever_kind_it_was) {
    const ckv::Color fallback = ckv::Color::rgb(1, 2, 3);
    CK_CHECK(ckv::resolved_color(ckv::Color::default_color(), fallback) == fallback);
    CK_CHECK(ckv::resolved_color(ckv::Color::rgb(9, 9, 9), fallback) == ckv::Color::rgb(9, 9, 9));
    CK_CHECK(ckv::resolved_color(ckv::Color::indexed(4), fallback) == ckv::palette_color(4));
    // A fallback may itself be a palette entry; the answer is still channels.
    CK_CHECK(ckv::resolved_color(ckv::Color::default_color(), ckv::Color::indexed(2)) ==
             ckv::palette_color(2));
    // And a fallback with nothing to give leaves black rather than an index.
    CK_CHECK(ckv::resolved_color(ckv::Color::default_color(), ckv::Color::default_color()) ==
             ckv::Color::rgb(0, 0, 0));
}

CK_TEST(an_underline_carries_a_shape_and_a_colour_of_its_own) {
    ckv::Style style;
    CK_CHECK(style.underline == ckv::UnderlineShape::Straight);
    CK_CHECK(style.underline_color.is_default());  // the rule follows the text

    style.attrs |= ckv::Attr::Underline;
    style.underline = ckv::UnderlineShape::Curly;
    style.underline_color = ckv::Color::indexed(1);
    ckv::Style other = style;
    CK_CHECK(other == style);
    other.underline = ckv::UnderlineShape::Dotted;
    CK_CHECK(other != style);  // a different rule is a different appearance
}

CK_TEST(style_attrs_combine) {
    ckv::Attr a = ckv::Attr::Bold;
    a |= ckv::Attr::Underline;
    CK_CHECK(ckv::has_attr(a, ckv::Attr::Bold));
    CK_CHECK(ckv::has_attr(a, ckv::Attr::Underline));
    CK_CHECK(!ckv::has_attr(a, ckv::Attr::Italic));
}

CK_TEST(cell_default_is_a_space) {
    const ckv::Cell cell;
    CK_CHECK(cell.grapheme() == " ");
    CK_CHECK(cell.width() == 1);
}

CK_TEST(cell_from_grapheme_computes_width) {
    const ckv::Style style{ckv::Color::rgb(255, 255, 255), ckv::Color::default_color(),
                            ckv::Attr::Bold};
    const ckv::Cell ascii = ckv::Cell::from_grapheme("A", style);
    CK_CHECK(ascii.grapheme() == "A");
    CK_CHECK(ascii.width() == 1);
    CK_CHECK(ckv::has_attr(ascii.style().attrs, ckv::Attr::Bold));

    const ckv::Cell wide = ckv::Cell::from_grapheme("\xE4\xB8\xAD", ckv::Style{});  // U+4E2D
    CK_CHECK(wide.width() == 2);
}

CK_TEST(cell_neutralizes_control_bytes_regardless_of_caller) {
    // A raw control byte handed to Cell must never survive into the
    // stored grapheme (D-040), even if the caller forgot to sanitize.
    const ckv::Cell cell = ckv::Cell::from_grapheme("\x07", ckv::Style{});
    CK_CHECK(cell.grapheme() != "\x07");
    CK_CHECK(cell.grapheme() == "\xEF\xBF\xBD");  // U+FFFD
}

CK_TEST(cell_keeps_only_the_first_grapheme_cluster) {
    // "AB" is two clusters; a Cell holds exactly one.
    const ckv::Cell cell = ckv::Cell::from_grapheme("AB", ckv::Style{});
    CK_CHECK(cell.grapheme() == "A");
}

CK_TEST(cell_set_style) {
    ckv::Cell cell = ckv::Cell::from_grapheme("x", ckv::Style{});
    cell.set_style(ckv::Style{ckv::Color::rgb(1, 2, 3), ckv::Color{}, ckv::Attr{}});
    CK_CHECK(cell.style().fg == ckv::Color::rgb(1, 2, 3));
}

CK_TEST(continuation_cell_is_distinguishable_from_a_lone_combining_mark) {
    const ckv::Cell cont = ckv::Cell::continuation(ckv::Style{});
    CK_CHECK(cont.is_continuation());
    CK_CHECK(cont.width() == 0);
    CK_CHECK(cont.grapheme().empty());

    // A lone combining mark (no base) is also width 0, but is a real,
    // non-empty glyph — must NOT be mistaken for a continuation cell.
    const ckv::Cell lone_mark = ckv::Cell::from_grapheme("\xCC\x81", ckv::Style{});
    CK_CHECK(lone_mark.width() == 0);
    CK_CHECK(!lone_mark.grapheme().empty());
    CK_CHECK(!lone_mark.is_continuation());
}

// --- ShadowStyle (D-037, D-106) -------------------------------------------

CK_TEST(a_halving_shadow_halves_every_channel_and_keeps_glyph_attributes) {
    const ckv::ShadowStyle halve;
    CK_CHECK(halve == ckv::ShadowStyle::halve());
    CK_CHECK(halve.kind() == ckv::ShadowStyle::Kind::Halve);
    const ckv::Style style{ckv::Color::rgb(201, 100, 7), ckv::Color::indexed(15), ckv::Attr::Bold | ckv::Attr::Underline,
                           ckv::UnderlineShape::Curly, ckv::Color::rgb(255, 0, 0)};
    const ckv::Style shadowed = halve.apply(style);
    CK_CHECK(shadowed.fg == ckv::Color::rgb(100, 50, 3));
    CK_CHECK(shadowed.bg == ckv::Color::rgb(127, 127, 127));  // palette white, resolved then halved
    CK_CHECK(shadowed.attrs == style.attrs);
    CK_CHECK(shadowed.underline == ckv::UnderlineShape::Curly);
    CK_CHECK(shadowed.underline_color == ckv::Color::rgb(127, 0, 0));
    // The terminal's default colour has no channels and goes to black.
    CK_CHECK(halve.apply(ckv::Style{}).fg == ckv::Color::rgb(0, 0, 0));
    const ckv::Image::Rgba pixel = halve.apply(ckv::Image::Rgba{255, 128, 1, 77});
    CK_CHECK(pixel.r == 127 && pixel.g == 64 && pixel.b == 0 && pixel.a == 77);
}

CK_TEST(a_recolouring_shadow_sets_its_own_colours_and_keeps_glyph_attributes) {
    const ckv::Color dark_grey = ckv::Color::rgb(85, 85, 85);
    const ckv::Color black = ckv::Color::rgb(0, 0, 0);
    const ckv::ShadowStyle classic = ckv::ShadowStyle::recolor(dark_grey, black);
    CK_CHECK(classic.kind() == ckv::ShadowStyle::Kind::Recolor);
    CK_CHECK(classic != ckv::ShadowStyle::halve());
    CK_CHECK(classic.foreground() == dark_grey && classic.background() == black);
    // Whatever it covers -- a white-on-blue frame, the blue-on-grey desktop --
    // becomes dark grey on black with its own attributes.
    const ckv::Style frame{ckv::Color::rgb(255, 255, 255), ckv::Color::rgb(0, 0, 170), ckv::Attr::Bold};
    CK_CHECK(classic.apply(frame) == (ckv::Style{dark_grey, black, ckv::Attr::Bold}));
    const ckv::Style desktop{ckv::Color::rgb(0, 0, 170), ckv::Color::rgb(200, 200, 200), ckv::Attr{}};
    CK_CHECK(classic.apply(desktop) == (ckv::Style{dark_grey, black, ckv::Attr{}}));
    // An underline colour of its own goes into the shadow's foreground; one
    // that follows the text keeps following it.
    const ckv::Style marked{ckv::Color::rgb(1, 2, 3), ckv::Color{}, ckv::Attr::Underline, ckv::UnderlineShape::Dotted,
                            ckv::Color::rgb(255, 0, 0)};
    CK_CHECK(classic.apply(marked).underline_color == dark_grey);
    CK_CHECK(classic.apply(frame).underline_color.is_default());
}

CK_TEST(a_recolouring_shadow_maps_pixel_luminance_from_black_to_its_foreground) {
    const ckv::ShadowStyle classic =
        ckv::ShadowStyle::recolor(ckv::Color::rgb(85, 85, 85), ckv::Color::rgb(0, 0, 0));
    const auto same = [](ckv::Image::Rgba a, ckv::Image::Rgba b) {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    };
    CK_CHECK(same(classic.apply(ckv::Image::Rgba{255, 255, 255, 255}), ckv::Image::Rgba{85, 85, 85, 255}));
    CK_CHECK(same(classic.apply(ckv::Image::Rgba{0, 0, 0, 9}), ckv::Image::Rgba{0, 0, 0, 9}));
    // BT.601 luminance: pure green is brighter than pure red, which is
    // brighter than pure blue, so a picture keeps its shading.
    const ckv::Image::Rgba red = classic.apply(ckv::Image::Rgba{255, 0, 0, 255});
    const ckv::Image::Rgba green = classic.apply(ckv::Image::Rgba{0, 255, 0, 255});
    const ckv::Image::Rgba blue = classic.apply(ckv::Image::Rgba{0, 0, 255, 255});
    CK_CHECK(same(red, ckv::Image::Rgba{25, 25, 25, 255}));    // luminance 76
    CK_CHECK(same(green, ckv::Image::Rgba{50, 50, 50, 255}));  // luminance 150
    CK_CHECK(same(blue, ckv::Image::Rgba{9, 9, 9, 255}));      // luminance 29
    // A palette foreground is resolved to its channels; the terminal's
    // default has none and maps every pixel to black.
    const ckv::ShadowStyle indexed = ckv::ShadowStyle::recolor(ckv::Color::indexed(15), ckv::Color{});
    CK_CHECK(same(indexed.apply(ckv::Image::Rgba{255, 255, 255, 255}), ckv::Image::Rgba{255, 255, 255, 255}));
    const ckv::ShadowStyle unknown = ckv::ShadowStyle::recolor(ckv::Color{}, ckv::Color{});
    CK_CHECK(same(unknown.apply(ckv::Image::Rgba{255, 255, 255, 255}), ckv::Image::Rgba{0, 0, 0, 255}));
}
