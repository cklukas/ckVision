// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/utf8.hpp"

#include "cvision/core/assert.hpp"

namespace ckv::utf8 {
namespace {

bool is_continuation(unsigned char b) noexcept { return (b & 0xC0) == 0x80; }

bool is_valid_scalar(char32_t cp) noexcept {
    if (cp > 0x10FFFF) return false;
    if (cp >= 0xD800 && cp <= 0xDFFF) return false;  // surrogate range
    return true;
}

}  // namespace

char32_t decode(std::string_view text, std::size_t& pos) noexcept {
    CKV_ASSERT(pos < text.size());
    const auto byte = [&](std::size_t i) -> unsigned char {
        return static_cast<unsigned char>(text[i]);
    };
    const std::size_t start = pos;
    const unsigned char b0 = byte(start);

    int extra = 0;
    char32_t cp = 0;
    char32_t min_cp = 0;

    if (b0 < 0x80) {
        pos = start + 1;
        return b0;
    } else if ((b0 & 0xE0) == 0xC0) {
        extra = 1;
        cp = b0 & 0x1Fu;
        min_cp = 0x80;
    } else if ((b0 & 0xF0) == 0xE0) {
        extra = 2;
        cp = b0 & 0x0Fu;
        min_cp = 0x800;
    } else if ((b0 & 0xF8) == 0xF0) {
        extra = 3;
        cp = b0 & 0x07u;
        min_cp = 0x10000;
    } else {
        pos = start + 1;  // stray continuation or invalid lead byte
        return replacement_char;
    }

    if (start + 1 + static_cast<std::size_t>(extra) > text.size()) {
        pos = start + 1;
        return replacement_char;
    }

    for (int i = 1; i <= extra; ++i) {
        const unsigned char b = byte(start + static_cast<std::size_t>(i));
        if (!is_continuation(b)) {
            pos = start + 1;
            return replacement_char;
        }
        cp = (cp << 6) | (b & 0x3Fu);
    }

    if (cp < min_cp || !is_valid_scalar(cp)) {
        pos = start + 1;
        return replacement_char;
    }

    pos = start + 1 + static_cast<std::size_t>(extra);
    return cp;
}

int encoded_length(char32_t cp) noexcept {
    if (!is_valid_scalar(cp)) return 3;  // replacement_char length
    if (cp < 0x80) return 1;
    if (cp < 0x800) return 2;
    if (cp < 0x10000) return 3;
    return 4;
}

void encode(char32_t cp, std::string& out) {
    if (!is_valid_scalar(cp)) cp = replacement_char;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

}  // namespace ckv::utf8
