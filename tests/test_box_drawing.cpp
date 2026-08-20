// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/scene/box_drawing.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::scene::Junction;
using ckv::scene::JunctionGlyphInfo;
using ckv::scene::classify_junction_glyph;
using ckv::scene::junction_glyph;
using ckv::scene::junction_of;
using ckv::scene::LineStyle;

CK_TEST(single_style_straight_lines_and_corners) {
    CK_CHECK(junction_glyph(Junction{false, false, true, true}, LineStyle::Single) == "─");
    CK_CHECK(junction_glyph(Junction{true, true, false, false}, LineStyle::Single) == "│");
    CK_CHECK(junction_glyph(Junction{false, true, false, true}, LineStyle::Single) == "┌");
    CK_CHECK(junction_glyph(Junction{false, true, true, false}, LineStyle::Single) == "┐");
    CK_CHECK(junction_glyph(Junction{true, false, false, true}, LineStyle::Single) == "└");
    CK_CHECK(junction_glyph(Junction{true, false, true, false}, LineStyle::Single) == "┘");
    CK_CHECK(junction_glyph(Junction{true, true, false, true}, LineStyle::Single) == "├");
    CK_CHECK(junction_glyph(Junction{true, true, true, false}, LineStyle::Single) == "┤");
    CK_CHECK(junction_glyph(Junction{false, true, true, true}, LineStyle::Single) == "┬");
    CK_CHECK(junction_glyph(Junction{true, false, true, true}, LineStyle::Single) == "┴");
    CK_CHECK(junction_glyph(Junction{true, true, true, true}, LineStyle::Single) == "┼");
}

CK_TEST(rounded_style_shares_straight_and_t_glyphs_with_single_but_has_its_own_corners) {
    CK_CHECK(junction_glyph(Junction{false, false, true, true}, LineStyle::Rounded) == "─");
    CK_CHECK(junction_glyph(Junction{true, true, false, false}, LineStyle::Rounded) == "│");
    CK_CHECK(junction_glyph(Junction{true, true, true, true}, LineStyle::Rounded) == "┼");
    CK_CHECK(junction_glyph(Junction{false, true, false, true}, LineStyle::Rounded) == "╭");
    CK_CHECK(junction_glyph(Junction{false, true, true, false}, LineStyle::Rounded) == "╮");
    CK_CHECK(junction_glyph(Junction{true, false, false, true}, LineStyle::Rounded) == "╰");
    CK_CHECK(junction_glyph(Junction{true, false, true, false}, LineStyle::Rounded) == "╯");
}

CK_TEST(double_style_is_fully_self_contained) {
    CK_CHECK(junction_glyph(Junction{false, false, true, true}, LineStyle::Double) == "═");
    CK_CHECK(junction_glyph(Junction{true, true, false, false}, LineStyle::Double) == "║");
    CK_CHECK(junction_glyph(Junction{false, true, false, true}, LineStyle::Double) == "╔");
    CK_CHECK(junction_glyph(Junction{false, true, true, false}, LineStyle::Double) == "╗");
    CK_CHECK(junction_glyph(Junction{true, false, false, true}, LineStyle::Double) == "╚");
    CK_CHECK(junction_glyph(Junction{true, false, true, false}, LineStyle::Double) == "╝");
    CK_CHECK(junction_glyph(Junction{true, true, false, true}, LineStyle::Double) == "╠");
    CK_CHECK(junction_glyph(Junction{true, true, true, false}, LineStyle::Double) == "╣");
    CK_CHECK(junction_glyph(Junction{false, true, true, true}, LineStyle::Double) == "╦");
    CK_CHECK(junction_glyph(Junction{true, false, true, true}, LineStyle::Double) == "╩");
    CK_CHECK(junction_glyph(Junction{true, true, true, true}, LineStyle::Double) == "╬");
}

CK_TEST(heavy_style_is_fully_self_contained) {
    CK_CHECK(junction_glyph(Junction{false, false, true, true}, LineStyle::Heavy) == "━");
    CK_CHECK(junction_glyph(Junction{true, true, false, false}, LineStyle::Heavy) == "┃");
    CK_CHECK(junction_glyph(Junction{false, true, false, true}, LineStyle::Heavy) == "┏");
    CK_CHECK(junction_glyph(Junction{false, true, true, false}, LineStyle::Heavy) == "┓");
    CK_CHECK(junction_glyph(Junction{true, false, false, true}, LineStyle::Heavy) == "┗");
    CK_CHECK(junction_glyph(Junction{true, false, true, false}, LineStyle::Heavy) == "┛");
    CK_CHECK(junction_glyph(Junction{true, true, false, true}, LineStyle::Heavy) == "┣");
    CK_CHECK(junction_glyph(Junction{true, true, true, false}, LineStyle::Heavy) == "┫");
    CK_CHECK(junction_glyph(Junction{false, true, true, true}, LineStyle::Heavy) == "┳");
    CK_CHECK(junction_glyph(Junction{true, false, true, true}, LineStyle::Heavy) == "┻");
    CK_CHECK(junction_glyph(Junction{true, true, true, true}, LineStyle::Heavy) == "╋");
}

CK_TEST(single_direction_stubs_normalize_to_the_straight_line_glyph) {
    CK_CHECK(junction_glyph(Junction{false, false, false, true}, LineStyle::Single) == "─");  // right only
    CK_CHECK(junction_glyph(Junction{false, false, true, false}, LineStyle::Single) == "─");  // left only
    CK_CHECK(junction_glyph(Junction{true, false, false, false}, LineStyle::Single) == "│");  // up only
    CK_CHECK(junction_glyph(Junction{false, true, false, false}, LineStyle::Single) == "│");  // down only
}

CK_TEST(junction_or_combines_directions) {
    const Junction a{true, false, false, true};   // up+right (└)
    const Junction b{false, false, true, true};   // horizontal
    const Junction combined = a | b;
    CK_CHECK(combined == (Junction{true, false, true, true}));  // up+left+right (┴)
}

CK_TEST(reverse_lookup_recognizes_every_forward_glyph) {
    for (const LineStyle style :
         {LineStyle::Single, LineStyle::Double, LineStyle::Rounded, LineStyle::Heavy}) {
        for (const Junction j : {Junction{false, false, true, true}, Junction{true, true, false, false},
                                  Junction{false, true, false, true}, Junction{false, true, true, false},
                                  Junction{true, false, false, true}, Junction{true, false, true, false},
                                  Junction{true, true, true, true}}) {
            const std::string_view glyph = junction_glyph(j, style);
            const std::optional<Junction> reversed = junction_of(glyph);
            CK_CHECK(reversed.has_value());
            if (reversed) CK_CHECK(*reversed == j);
        }
    }
}

CK_TEST(reverse_lookup_rejects_plain_text) {
    CK_CHECK(!junction_of("A").has_value());
    CK_CHECK(!junction_of(" ").has_value());
    CK_CHECK(!junction_of("").has_value());
}

CK_TEST(glyph_classification_preserves_concrete_line_style_for_visual_renderers) {
    const std::optional<JunctionGlyphInfo> double_corner = classify_junction_glyph("╝");
    CK_CHECK(double_corner.has_value());
    if (double_corner) {
        CK_CHECK(double_corner->junction == (Junction{true, false, true, false}));
        CK_CHECK(double_corner->style == LineStyle::Double);
    }

    const std::optional<JunctionGlyphInfo> rounded_corner = classify_junction_glyph("╭");
    CK_CHECK(rounded_corner.has_value());
    if (rounded_corner) CK_CHECK(rounded_corner->style == LineStyle::Rounded);

    CK_CHECK(!classify_junction_glyph("A").has_value());
}
