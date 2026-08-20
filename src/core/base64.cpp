// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/base64.hpp"

#include <array>
#include <cstdint>

namespace ckv::base64 {
namespace {

constexpr std::string_view kAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// The reverse table, built once from the alphabet above so the two cannot
// disagree. -1 is "not a base64 character at all".
constexpr std::array<signed char, 256> reverse_table() {
    std::array<signed char, 256> table{};
    for (std::size_t i = 0; i < table.size(); ++i) table[i] = -1;
    for (std::size_t i = 0; i < kAlphabet.size(); ++i)
        table[static_cast<unsigned char>(kAlphabet[i])] = static_cast<signed char>(i);
    return table;
}

}  // namespace

std::string encode(std::string_view data) {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        const std::uint32_t group = (static_cast<unsigned char>(data[i]) << 16) |
                                     (static_cast<unsigned char>(data[i + 1]) << 8) |
                                     static_cast<unsigned char>(data[i + 2]);
        out += kAlphabet[(group >> 18) & 0x3F];
        out += kAlphabet[(group >> 12) & 0x3F];
        out += kAlphabet[(group >> 6) & 0x3F];
        out += kAlphabet[group & 0x3F];
    }
    const std::size_t remaining = data.size() - i;
    if (remaining == 1) {
        const std::uint32_t group = static_cast<std::uint32_t>(static_cast<unsigned char>(data[i]))
                                    << 16;
        out += kAlphabet[(group >> 18) & 0x3F];
        out += kAlphabet[(group >> 12) & 0x3F];
        out += "==";
    } else if (remaining == 2) {
        const std::uint32_t group = (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i]))
                                     << 16) |
                                    (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 1]))
                                     << 8);
        out += kAlphabet[(group >> 18) & 0x3F];
        out += kAlphabet[(group >> 12) & 0x3F];
        out += kAlphabet[(group >> 6) & 0x3F];
        out += '=';
    }
    return out;
}

bool decode(std::string_view text, std::string& out) {
    static constexpr std::array<signed char, 256> reverse = reverse_table();
    if (text.size() % 4 != 0) return false;
    std::string decoded;
    decoded.reserve(text.size() / 4 * 3);
    for (std::size_t i = 0; i < text.size(); i += 4) {
        std::uint32_t group = 0;
        int bytes = 3;
        for (std::size_t j = 0; j < 4; ++j) {
            const char c = text[i + j];
            if (c == '=') {
                // Padding exists only at the very end, and only as one or two
                // characters. Anything else is a spelling nobody produced.
                const bool last_group = i + 4 == text.size();
                if (!last_group || j < 2) return false;
                if (j == 2 && text[i + 3] != '=') return false;
                bytes = static_cast<int>(j) - 1;
                group <<= 6 * (4 - j);
                break;
            }
            const signed char value = reverse[static_cast<unsigned char>(c)];
            if (value < 0) return false;
            group = (group << 6) | static_cast<std::uint32_t>(value);
        }
        decoded += static_cast<char>((group >> 16) & 0xFF);
        if (bytes > 1) decoded += static_cast<char>((group >> 8) & 0xFF);
        if (bytes > 2) decoded += static_cast<char>(group & 0xFF);
    }
    out = std::move(decoded);
    return true;
}

}  // namespace ckv::base64
