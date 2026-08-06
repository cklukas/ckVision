// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <cstdint>
#include <string_view>

namespace ckv::utf8 {

inline constexpr char32_t replacement_char = 0xFFFD;

// Decodes the codepoint starting at `text[pos]`. Returns the codepoint
// and advances `pos` past it. On malformed or truncated input, returns
// `replacement_char` and advances `pos` by exactly one byte (so callers
// always make forward progress). `pos` must be < text.size() on entry.
char32_t decode(std::string_view text, std::size_t& pos) noexcept;

// Number of bytes the well-formed UTF-8 encoding of `cp` occupies
// (1-4). Codepoints above U+10FFFF or in the surrogate range are
// encoded as `replacement_char`'s length (3).
int encoded_length(char32_t cp) noexcept;

}  // namespace ckv::utf8
