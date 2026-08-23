// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The cases below are mechanically generated from the Unicode 15.1.0
// GraphemeBreakTest conformance corpus. Their source hash is recorded in the
// generated include and docs/text-width.md.
#include "cvision/core/text.hpp"

#include <iterator>
#include <string_view>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/utf8.hpp"

namespace {

struct GraphemeBreakCase {
    std::string_view text;
    // One ASCII character per Unicode scalar: '1' means a break follows it.
    std::string_view breaks;
};

constexpr GraphemeBreakCase kCases[] = {
#include "generated_unicode_grapheme_break_15_1.inc"
};

std::vector<std::size_t> expected_ends(const GraphemeBreakCase& test) {
    std::vector<std::size_t> ends;
    std::size_t byte = 0;
    std::size_t scalar = 0;
    while (byte < test.text.size()) {
        static_cast<void>(ckv::utf8::decode(test.text, byte));
        if (test.breaks[scalar] == '1')
            ends.push_back(byte);
        ++scalar;
    }
    CK_CHECK(scalar == test.breaks.size());
    return ends;
}

std::vector<std::size_t> actual_ends(std::string_view text) {
    std::vector<std::size_t> ends;
    std::size_t byte = 0;
    while (byte < text.size()) {
        byte = ckv::text::grapheme_end(text, byte);
        ends.push_back(byte);
    }
    return ends;
}

} // namespace

CK_TEST(unicode_15_1_official_extended_grapheme_cluster_corpus_passes) {
    static_assert(std::size(kCases) == 1187);
    for (const GraphemeBreakCase& test : kCases)
        CK_CHECK(actual_ends(test.text) == expected_ends(test));
}
