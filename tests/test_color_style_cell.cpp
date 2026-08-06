// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/core/cell.hpp"
#include "cvision/core/color.hpp"
#include "cvision/core/style.hpp"

#include "cktest.hpp"

CK_TEST(color_default_and_rgb) {
    CK_CHECK(ckv::Color{}.is_default());
    CK_CHECK(ckv::Color::default_color().is_default());
    const ckv::Color c = ckv::Color::rgb(10, 20, 30);
    CK_CHECK(!c.is_default());
    CK_CHECK(c.r() == 10);
    CK_CHECK(c.g() == 20);
    CK_CHECK(c.b() == 30);
    CK_CHECK(ckv::Color::rgb(1, 2, 3) == ckv::Color::rgb(1, 2, 3));
    CK_CHECK(ckv::Color::rgb(1, 2, 3) != ckv::Color::rgb(1, 2, 4));
    CK_CHECK(ckv::Color::default_color() != ckv::Color::rgb(0, 0, 0));
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
    // stored grapheme (D-023), even if the caller forgot to sanitize.
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

