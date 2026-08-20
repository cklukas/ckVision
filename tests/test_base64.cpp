// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/base64.hpp"

#include <string>

#include "cvision/testing/cktest.hpp"

CK_TEST(base64_round_trips_every_group_length) {
    // The three group lengths are the whole of the encoding: a full group, and
    // the two that need padding.
    for (const std::string_view sample : {"", "a", "ab", "abc", "abcd", "hello, world"}) {
        const std::string encoded = ckv::base64::encode(sample);
        std::string decoded;
        CK_CHECK(ckv::base64::decode(encoded, decoded));
        CK_CHECK(decoded == sample);
    }
}

CK_TEST(base64_encodes_the_published_examples) {
    CK_CHECK(ckv::base64::encode("f") == "Zg==");
    CK_CHECK(ckv::base64::encode("fo") == "Zm8=");
    CK_CHECK(ckv::base64::encode("foo") == "Zm9v");
    CK_CHECK(ckv::base64::encode("foobar") == "Zm9vYmFy");
}

CK_TEST(base64_carries_bytes_that_are_not_text) {
    const std::string binary("\x00\x01\xFF\xFE\x80", 5);
    std::string decoded;
    CK_CHECK(ckv::base64::decode(ckv::base64::encode(binary), decoded));
    CK_CHECK(decoded == binary);
}

CK_TEST(base64_decoding_is_strict_because_its_input_may_be_hostile) {
    // The input is a control sequence from a program that may mean harm, so
    // "decode what you can" would let two spellings of the same bytes through
    // the same size cap, or turn a truncated sequence into text nobody sent.
    std::string out;
    CK_CHECK(!ckv::base64::decode("Zm9vYmF", out));     // truncated group
    CK_CHECK(!ckv::base64::decode("Zm9v YmFy", out));   // whitespace
    CK_CHECK(!ckv::base64::decode("Zm9v\nYmFy", out));  // line break
    CK_CHECK(!ckv::base64::decode("Zm9-YmFy", out));    // URL-safe alphabet
    CK_CHECK(!ckv::base64::decode("Zg=a", out));        // padding in the middle
    CK_CHECK(!ckv::base64::decode("Z===", out));        // three padding characters
    CK_CHECK(!ckv::base64::decode("Zm==Zm9v", out));    // padding before the end
    // A failed decode leaves the caller's buffer alone rather than half-filled.
    out = "untouched";
    CK_CHECK(!ckv::base64::decode("!!!!", out));
    CK_CHECK(out == "untouched");
}
