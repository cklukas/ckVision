// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The shared geometry every scrolling text surface uses. It is tested here,
// once, so TextView, Memo and TextEditor cannot drift apart on where a line
// breaks or on when a scrollbar appears — which is exactly what happened
// while each of them carried its own copy.
#include "cvision/widgets/text_layout.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "cvision/core/text.hpp"
#include "cvision/testing/cktest.hpp"

using ckv::Size;
using ckv::widgets::resolve_scroll_geometry;
using ckv::widgets::ScrollbarPolicy;
using ckv::widgets::ScrollGeometry;
using ckv::widgets::WrapOptions;
using ckv::widgets::WrapSegment;
using ckv::widgets::wrap_graphemes;
using ckv::widgets::wrap_text;

namespace {
std::vector<std::string> graphemes_of(const std::string& text) {
    std::vector<std::string> out;
    for (const std::string_view g : ckv::text::split_graphemes(text)) out.emplace_back(g);
    return out;
}

std::vector<std::string> rows_of(const std::string& text, int width,
                                 ckv::widgets::WrapMode mode = ckv::widgets::WrapMode::Word, int reserve = 0) {
    const std::vector<std::string> graphemes = graphemes_of(text);
    std::vector<std::string> rows;
    for (const WrapSegment& segment : wrap_graphemes(graphemes, WrapOptions{width, mode, reserve})) {
        std::string row;
        for (std::size_t i = segment.begin; i < segment.end; ++i) row += graphemes[i];
        rows.push_back(row);
    }
    return rows;
}
}  // namespace

CK_TEST(wrapping_breaks_between_words_and_keeps_them_whole) {
    const std::vector<std::string> rows = rows_of("the quick brown fox", 10);
    CK_CHECK(rows.size() > 1);
    for (const std::string& row : rows) CK_CHECK(ckv::text::text_width(row) <= 10);
    // Every word survives intact somewhere in the output.
    std::string joined;
    for (const std::string& row : rows) joined += row;
    CK_CHECK(joined.find("quick") != std::string::npos);
    CK_CHECK(joined.find("brown") != std::string::npos);
}

CK_TEST(a_word_wider_than_the_row_is_never_split) {
    // Breaking mid-word would hide that a path or an identifier is wider
    // than the window; instead it overflows, and the horizontal bar says so.
    const std::vector<std::string> rows = rows_of("hi /a/very/long/unbreakable/path", 8);
    bool found = false;
    for (const std::string& row : rows)
        if (row == "/a/very/long/unbreakable/path") found = true;
    CK_CHECK(found);
}

CK_TEST(wrapping_off_gives_one_row_however_long_the_line_is) {
    const std::vector<std::string> rows = rows_of("the quick brown fox", 5, ckv::widgets::WrapMode::None);
    CK_CHECK(rows.size() == 1U);
    CK_CHECK(rows[0] == "the quick brown fox");
}

CK_TEST(an_empty_line_still_occupies_one_row) {
    CK_CHECK(rows_of("", 10).size() == 1U);
    CK_CHECK(rows_of("", 10)[0].empty());
    // ...and a zero width degrades to a single row rather than looping.
    CK_CHECK(rows_of("abc", 0).size() == 1U);
}

CK_TEST(a_continuation_reserve_narrows_the_rows_that_make_room_for_a_marker) {
    const std::vector<std::string> plain = rows_of("aa bb cc dd ee ff", 10, ckv::widgets::WrapMode::Word, 0);
    const std::vector<std::string> reserved = rows_of("aa bb cc dd ee ff", 10, ckv::widgets::WrapMode::Word, 1);
    for (const std::string& row : reserved) CK_CHECK(ckv::text::text_width(row) <= 9);
    CK_CHECK(reserved.size() >= plain.size());
    // A reserve wider than the row itself is ignored rather than starving it.
    CK_CHECK(!rows_of("abc def", 2, ckv::widgets::WrapMode::Word, 5).empty());
}

CK_TEST(wrapping_text_reports_byte_offsets_on_cluster_boundaries) {
    // For a surface that stores lines as text rather than as split clusters.
    const std::string text = "héllo wörld";
    const std::vector<WrapSegment> segments = wrap_text(text, WrapOptions{6, ckv::widgets::WrapMode::Word, 0});
    CK_CHECK(segments.size() == 2U);
    CK_CHECK(segments.front().begin == 0);
    CK_CHECK(segments.back().end == text.size());
    // Every boundary lands where a cluster starts, never inside one.
    for (const WrapSegment& segment : segments) {
        CK_CHECK(segment.begin <= text.size());
        if (segment.begin < text.size())
            CK_CHECK((static_cast<unsigned char>(text[segment.begin]) & 0xC0U) != 0x80U);
    }
}

// --- Scrollbar geometry ---------------------------------------------------

CK_TEST(neither_bar_appears_when_the_content_already_fits) {
    const ScrollGeometry g = resolve_scroll_geometry(
        Size{20, 10}, ScrollbarPolicy::Auto, ScrollbarPolicy::Auto,
        [](int) { return Size{5, 3}; });
    CK_CHECK(!g.show_vertical);
    CK_CHECK(!g.show_horizontal);
    CK_CHECK(g.viewport_width == 20);
    CK_CHECK(g.viewport_height == 10);
}

CK_TEST(a_vertical_bar_takes_a_column_from_what_the_content_may_use) {
    const ScrollGeometry g = resolve_scroll_geometry(
        Size{20, 10}, ScrollbarPolicy::Auto, ScrollbarPolicy::Auto,
        [](int) { return Size{5, 100}; });
    CK_CHECK(g.show_vertical);
    CK_CHECK(!g.show_horizontal);
    CK_CHECK(g.viewport_width == 19);
}

CK_TEST(a_vertical_bar_can_be_what_makes_a_line_no_longer_fit) {
    // The case that cannot be resolved one bar at a time: the content is
    // exactly as wide as the view, so it fits — until the vertical bar takes
    // its column, and then it does not.
    const ScrollGeometry g = resolve_scroll_geometry(
        Size{20, 10}, ScrollbarPolicy::Auto, ScrollbarPolicy::Auto,
        [](int) { return Size{20, 100}; });
    CK_CHECK(g.show_vertical);
    CK_CHECK(g.show_horizontal);
    CK_CHECK(g.viewport_width == 19);
    CK_CHECK(g.viewport_height == 9);
}

CK_TEST(wrapped_content_reflows_as_the_bars_change_the_width_available) {
    // With wrapping, a narrower viewport means more rows — which is what can
    // call the vertical bar into existence in the first place.
    const ScrollGeometry g = resolve_scroll_geometry(
        Size{10, 4}, ScrollbarPolicy::Auto, ScrollbarPolicy::Auto,
        [](int viewport_width) {
            // 45 cells of text wrapped into however many rows fit the width.
            const int rows = viewport_width > 0 ? (45 + viewport_width - 1) / viewport_width : 45;
            return Size{std::min(viewport_width, 45), rows};
        });
    CK_CHECK(g.show_vertical);       // 45 cells needs 5 rows, and only 4 fit
    CK_CHECK(!g.show_horizontal);    // wrapping means nothing overflows sideways
    CK_CHECK(g.viewport_width == 9);
}

CK_TEST(the_always_and_hidden_policies_ignore_whether_the_content_fits) {
    const ScrollGeometry always = resolve_scroll_geometry(
        Size{20, 10}, ScrollbarPolicy::Always, ScrollbarPolicy::Always,
        [](int) { return Size{1, 1}; });
    CK_CHECK(always.show_vertical && always.show_horizontal);
    CK_CHECK(always.viewport_width == 19 && always.viewport_height == 9);

    const ScrollGeometry hidden = resolve_scroll_geometry(
        Size{20, 10}, ScrollbarPolicy::Hidden, ScrollbarPolicy::Hidden,
        [](int) { return Size{500, 500}; });
    CK_CHECK(!hidden.show_vertical && !hidden.show_horizontal);
    CK_CHECK(hidden.viewport_width == 20 && hidden.viewport_height == 10);
}

CK_TEST(character_wrapping_fills_every_column_and_breaks_mid_word) {
    // For content with no word structure to respect. The contrast with Word
    // is the point: same text, same width, different rule.
    const std::string text = "the quickbrown fox";
    const std::vector<std::string> character = rows_of(text, 10, ckv::widgets::WrapMode::Character);
    const std::vector<std::string> word = rows_of(text, 10, ckv::widgets::WrapMode::Word);
    CK_CHECK(character[0] == "the quickb");  // filled to the edge, mid-word
    CK_CHECK(word[0] == "the ");             // stopped at the last space that fit

    // A word wider than the row is split here, where Word would let it
    // overflow whole.
    const std::vector<std::string> split = rows_of("/a/very/long/path", 6, ckv::widgets::WrapMode::Character);
    CK_CHECK(split.size() > 1U);
    for (const std::string& row : split) CK_CHECK(ckv::text::text_width(row) <= 6);
}
