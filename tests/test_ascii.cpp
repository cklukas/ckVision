// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/ascii.hpp"

#include <string>

#include "cvision/testing/cktest.hpp"

using ckv::ascii_iequals;
using ckv::ascii_lower;
using ckv::ascii_upper;
using ckv::is_ascii_alpha;
using ckv::is_ascii_digit;

CK_TEST(only_ascii_letters_change_case_and_every_other_byte_passes_through) {
    for (int value = 0; value < 256; ++value) {
        const char c = static_cast<char>(value);
        const bool upper = value >= 'A' && value <= 'Z';
        const bool lower = value >= 'a' && value <= 'z';
        CK_CHECK(ascii_lower(c) == (upper ? static_cast<char>(value + 32) : c));
        CK_CHECK(ascii_upper(c) == (lower ? static_cast<char>(value - 32) : c));
        CK_CHECK(is_ascii_alpha(c) == (upper || lower));
        CK_CHECK(is_ascii_digit(c) == (value >= '0' && value <= '9'));
    }
}

CK_TEST(folding_utf8_text_leaves_every_multibyte_sequence_intact) {
    // "Äpfel Öl ß" — its lead bytes 0xC3 and continuation bytes are exactly the
    // bytes a Latin-1 locale's tolower would rewrite.
    const std::string text = "\xC3\x84pfel \xC3\x96l \xC3\x9F";
    CK_CHECK(ascii_lower(text) == "\xC3\x84pfel \xC3\x96l \xC3\x9F");
    CK_CHECK(ascii_upper(text) == "\xC3\x84PFEL \xC3\x96L \xC3\x9F");
}

CK_TEST(case_insensitive_equality_folds_letters_and_nothing_else) {
    CK_CHECK(ascii_iequals("Ctrl+Q", "ctrl+q"));
    CK_CHECK(!ascii_iequals("Ctrl+Q", "ctrl+w"));
    CK_CHECK(!ascii_iequals("abc", "abcd"));
    CK_CHECK(ascii_iequals("", ""));
    // U+00C4 and U+00E4 differ in a continuation byte: not an ASCII case pair.
    CK_CHECK(!ascii_iequals("\xC3\x84", "\xC3\xA4"));
    static_assert(ascii_iequals("F10", "f10"));
}
