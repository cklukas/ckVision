// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/mnemonic.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::widgets::parse_mnemonic;

CK_TEST(text_with_no_ampersand_has_no_mnemonic) {
    auto r = parse_mnemonic("Save");
    CK_CHECK(r.display == "Save");
    CK_CHECK(r.mnemonic.empty());
}

CK_TEST(a_single_ampersand_marks_the_following_grapheme_as_the_mnemonic) {
    auto r = parse_mnemonic("&Save");
    CK_CHECK(r.display == "Save");
    CK_CHECK(r.mnemonic == "S");
    CK_CHECK(r.mnemonic_byte_offset == 0);
}

CK_TEST(the_mnemonic_marker_in_the_middle_of_text_is_stripped_and_offset_reported) {
    auto r = parse_mnemonic("Sa&ve");
    CK_CHECK(r.display == "Save");
    CK_CHECK(r.mnemonic == "v");
    CK_CHECK(r.mnemonic_byte_offset == 2);
}

CK_TEST(double_ampersand_collapses_to_a_literal_ampersand_with_no_mnemonic) {
    auto r = parse_mnemonic("Fish && Chips");
    CK_CHECK(r.display == "Fish & Chips");
    CK_CHECK(r.mnemonic.empty());
}

CK_TEST(only_the_first_ampersand_marked_grapheme_becomes_the_mnemonic) {
    auto r = parse_mnemonic("&One &Two");
    CK_CHECK(r.display == "One Two");
    CK_CHECK(r.mnemonic == "O");
}

CK_TEST(a_trailing_lone_ampersand_with_nothing_after_it_marks_nothing) {
    auto r = parse_mnemonic("Save&");
    CK_CHECK(r.display == "Save");
    CK_CHECK(r.mnemonic.empty());
}

CK_TEST(empty_input_produces_empty_output) {
    auto r = parse_mnemonic("");
    CK_CHECK(r.display.empty());
    CK_CHECK(r.mnemonic.empty());
}

CK_TEST(mnemonic_over_a_multi_byte_grapheme_captures_the_whole_cluster_not_a_partial_byte) {
    auto r = parse_mnemonic("&\xC3\xA9""cole");  // "&école" (e-acute)
    CK_CHECK(r.mnemonic == "\xC3\xA9");
    CK_CHECK(r.display == "\xC3\xA9""cole");
}

CK_TEST(ampersand_immediately_followed_by_another_marker_style_ampersand_pair_then_text) {
    auto r = parse_mnemonic("&&&Bold");  // literal '&' then a real mnemonic on 'B'
    CK_CHECK(r.display == "&Bold");
    CK_CHECK(r.mnemonic == "B");
}
