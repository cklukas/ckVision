// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/utf8.hpp"

#include "cvision/testing/cktest.hpp"

namespace {

char32_t decode_one(std::string_view text) {
    std::size_t pos = 0;
    return ckv::utf8::decode(text, pos);
}

}  // namespace

CK_TEST(decode_ascii) {
    CK_CHECK(decode_one("A") == U'A');
    CK_CHECK(decode_one(std::string_view("\x00", 1)) == 0u);
}

CK_TEST(decode_multibyte) {
    CK_CHECK(decode_one("\xC2\xA9") == 0x00A9u);          // (c) COPYRIGHT SIGN, 2 bytes
    CK_CHECK(decode_one("\xE4\xB8\xAD") == 0x4E2Du);       // 中, 3 bytes
    CK_CHECK(decode_one("\xF0\x9F\x98\x80") == 0x1F600u);  // 😀, 4 bytes
}

CK_TEST(is_valid_accepts_empty_ascii_multibyte_and_encoded_replacement) {
    CK_CHECK(ckv::utf8::is_valid(""));
    CK_CHECK(ckv::utf8::is_valid("plain ASCII"));
    CK_CHECK(ckv::utf8::is_valid("Gr\xC3\xBC\xC3\x9F"));
    CK_CHECK(ckv::utf8::is_valid("\xEF\xBF\xBD"));
    CK_CHECK(ckv::utf8::is_valid("\xF0\x9F\x98\x80"));
}

CK_TEST(is_valid_rejects_every_malformed_sequence_class) {
    CK_CHECK(!ckv::utf8::is_valid("\x80"));
    CK_CHECK(!ckv::utf8::is_valid("\xC0\x80"));
    CK_CHECK(!ckv::utf8::is_valid("\xE4\xB8"));
    CK_CHECK(!ckv::utf8::is_valid("\xE4\x41\xAD"));
    CK_CHECK(!ckv::utf8::is_valid("\xED\xA0\x80"));
    CK_CHECK(!ckv::utf8::is_valid("\xF4\x90\x80\x80"));
    CK_CHECK(!ckv::utf8::is_valid("ok\xFF"));
}

CK_TEST(decode_advances_pos_by_encoded_length) {
    std::string_view text = "A\xC2\xA9\xE4\xB8\xAD";  // A, ©, 中
    std::size_t pos = 0;
    CK_CHECK(ckv::utf8::decode(text, pos) == U'A');
    CK_CHECK(pos == 1);
    CK_CHECK(ckv::utf8::decode(text, pos) == 0x00A9u);
    CK_CHECK(pos == 3);
    CK_CHECK(ckv::utf8::decode(text, pos) == 0x4E2Du);
    CK_CHECK(pos == 6);
    CK_CHECK(pos == text.size());
}

CK_TEST(decode_rejects_malformed_input_and_still_advances) {
    // Lone continuation byte.
    {
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode("\x80", pos) == ckv::utf8::replacement_char);
        CK_CHECK(pos == 1);
    }
    // Truncated multibyte sequence at end of input.
    {
        std::string_view text = "\xE4\xB8";  // truncated 3-byte sequence
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode(text, pos) == ckv::utf8::replacement_char);
        CK_CHECK(pos == 1);  // forward progress guaranteed
    }
    // Overlong encoding (2-byte encoding of an ASCII codepoint).
    {
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode("\xC0\x80", pos) == ckv::utf8::replacement_char);
    }
    // Encoded surrogate (invalid scalar value).
    {
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode("\xED\xA0\x80", pos) == ckv::utf8::replacement_char);
    }
    // Invalid lead byte: rejected immediately at dispatch (0xFF matches
    // no lead-byte bit pattern at all).
    {
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode("\xFF", pos) == ckv::utf8::replacement_char);
        CK_CHECK(pos == 1);
    }
    // Mid-sequence non-continuation byte: a structurally valid lead byte
    // followed by a byte that isn't a continuation (10xxxxxx) — a
    // different rejection path than a bad lead byte or a length
    // precheck failure.
    {
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode("\xE4\x41\xAD", pos) == ckv::utf8::replacement_char);
        CK_CHECK(pos == 1);
    }
    {
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode("\xF0\x9F\x28\x80", pos) == ckv::utf8::replacement_char);
        CK_CHECK(pos == 1);
    }
    // Overlong encodings at every applicable length: structurally
    // well-formed (every continuation byte valid), rejected only by the
    // min_cp check, not by the lead-byte or continuation-byte checks.
    {
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode("\xE0\x80\x80", pos) == ckv::utf8::replacement_char);
        CK_CHECK(pos == 1);
    }
    {
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode("\xF0\x80\x80\x80", pos) == ckv::utf8::replacement_char);
        CK_CHECK(pos == 1);
    }
    // Codepoint one past the maximum valid scalar value (U+110000):
    // structurally well-formed, rejected only by is_valid_scalar.
    {
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode("\xF4\x90\x80\x80", pos) == ckv::utf8::replacement_char);
        CK_CHECK(pos == 1);
    }
    // Lead byte 0xF5: distinct code path from 0xFF — it passes the
    // 4-byte lead bit-pattern dispatch (0xF0-0xF7 all match) and is
    // rejected later, only once the decoded codepoint is checked.
    {
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode("\xF5\x8F\xBF\xBF", pos) == ckv::utf8::replacement_char);
        CK_CHECK(pos == 1);
    }
}

CK_TEST(decode_accepts_the_valid_codepoints_adjacent_to_the_surrogate_range) {
    CK_CHECK(decode_one("\xED\x9F\xBF") == 0xD7FFu);  // last valid codepoint before surrogates
    CK_CHECK(decode_one("\xEE\x80\x80") == 0xE000u);  // first valid codepoint after surrogates
}

CK_TEST(encode_round_trips_through_decode) {
    for (const char32_t cp : {U'A', char32_t{0x00A9}, char32_t{0x4E2D}, char32_t{0x1F600}}) {
        std::string out;
        ckv::utf8::encode(cp, out);
        CK_CHECK(static_cast<int>(out.size()) == ckv::utf8::encoded_length(cp));
        std::size_t pos = 0;
        CK_CHECK(ckv::utf8::decode(out, pos) == cp);
        CK_CHECK(pos == out.size());
    }
}

CK_TEST(encode_appends_rather_than_overwrites) {
    std::string out = "X";
    ckv::utf8::encode(U'Y', out);
    CK_CHECK(out == "XY");
}

CK_TEST(encode_replaces_invalid_scalars) {
    std::string surrogate;
    ckv::utf8::encode(0xD800, surrogate);
    CK_CHECK(surrogate == "\xEF\xBF\xBD");  // U+FFFD

    std::string too_large;
    ckv::utf8::encode(0x110000, too_large);
    CK_CHECK(too_large == "\xEF\xBF\xBD");
}

CK_TEST(encoded_length_matches_utf8_rules) {
    CK_CHECK(ckv::utf8::encoded_length(U'A') == 1);
    CK_CHECK(ckv::utf8::encoded_length(0x00A9) == 2);
    CK_CHECK(ckv::utf8::encoded_length(0x4E2D) == 3);
    CK_CHECK(ckv::utf8::encoded_length(0x1F600) == 4);
}

CK_TEST(encoded_length_of_invalid_scalars_matches_replacement_char_length) {
    CK_CHECK(ckv::utf8::encoded_length(0xD800) == 3);    // surrogate
    CK_CHECK(ckv::utf8::encoded_length(0x110000) == 3);  // above U+10FFFF
}
