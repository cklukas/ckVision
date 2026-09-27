// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ASCII classification and case folding that no locale can change. The <cctype>
// functions consult the process locale: under a single-byte locale such as Latin-1,
// std::tolower rewrites bytes 0xC0-0xDE, which are UTF-8 lead bytes, so a library that
// used them would corrupt text and draw differently depending on what the host program
// had set. Every byte outside A-Z, a-z and 0-9 passes through these unchanged.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ckv {

// Whether `c` is an ASCII decimal digit, 0-9.
constexpr bool is_ascii_digit(char c) noexcept { return c >= '0' && c <= '9'; }

// Whether `c` is an ASCII letter, A-Z or a-z.
constexpr bool is_ascii_alpha(char c) noexcept { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

// `c` with A-Z mapped to a-z; every other byte unchanged.
constexpr char ascii_lower(char c) noexcept { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

// `c` with a-z mapped to A-Z; every other byte unchanged.
constexpr char ascii_upper(char c) noexcept { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c; }

// `text` with every ASCII letter lowered, byte for byte; UTF-8 sequences are untouched.
inline std::string ascii_lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = ascii_lower(c);
    return out;
}

// `text` with every ASCII letter raised, byte for byte; UTF-8 sequences are untouched.
inline std::string ascii_upper(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = ascii_upper(c);
    return out;
}

// Whether `a` and `b` are equal once ASCII letters are folded to one case. Bytes outside
// A-Z and a-z must match exactly.
constexpr bool ascii_iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
    return true;
}

}  // namespace ckv
