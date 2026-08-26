// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ckv::utf8 {

inline constexpr char32_t replacement_char = 0xFFFD;

// Returns true when every byte in `text` is part of a well-formed UTF-8
// encoding of a Unicode scalar value. The empty string is valid. Unlike
// `decode`, this function distinguishes malformed input from an encoded
// U+FFFD replacement character.
bool is_valid(std::string_view text) noexcept;

// Decodes the codepoint starting at `text[pos]`. Returns the codepoint
// and advances `pos` past it. On malformed or truncated input, returns
// `replacement_char` and advances `pos` by exactly one byte (so callers
// always make forward progress). `pos` must be < text.size() on entry.
char32_t decode(std::string_view text, std::size_t& pos) noexcept;

// Number of bytes the well-formed UTF-8 encoding of `cp` occupies
// (1-4). Codepoints above U+10FFFF or in the surrogate range are
// encoded as `replacement_char`'s length (3).
int encoded_length(char32_t cp) noexcept;

// Encodes `cp` as UTF-8, appended to `out`. Invalid scalars (above
// U+10FFFF or a surrogate) are encoded as replacement_char instead —
// this function never produces ill-formed UTF-8.
void encode(char32_t cp, std::string& out);

}  // namespace ckv::utf8
