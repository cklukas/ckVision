// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Base64 (RFC 4648 §4), the encoding OSC 52 carries clipboard text in — in
// both directions, because ckVision is at both ends of that sequence: it
// writes one to export a copy to the host, and reads one when a child inside
// an embedded terminal asks to put something on the clipboard.
#pragma once

#include <string>
#include <string_view>

namespace ckv::base64 {

// Encodes arbitrary bytes with the standard alphabet and '=' padding, as one unbroken line: the
// result is always a multiple of four characters, and empty input gives an empty string.
std::string encode(std::string_view data);

// Strict by design: the standard alphabet, correct padding with the unused
// bits of a padded group zero (RFC 4648 §3.5), no line breaks, no
// whitespace, no alternative characters. Returns false and leaves `out`
// untouched for anything else.
//
// A lenient decoder is a liability here. The input is a control sequence from
// a program that may be hostile, and "decode what you can" would let two
// spellings of the same bytes through the same size cap — or turn a truncated
// sequence into text that was never sent.
bool decode(std::string_view text, std::string& out);

}  // namespace ckv::base64
