// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "cvision/core/color.hpp"

namespace ckv {

enum class Attr : std::uint8_t {
    Bold = 1u << 0,
    Dim = 1u << 1,
    Italic = 1u << 2,
    Underline = 1u << 3,
    Reverse = 1u << 4,
    Strike = 1u << 5,
};

constexpr Attr operator|(Attr a, Attr b) noexcept {
    return static_cast<Attr>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr Attr& operator|=(Attr& a, Attr b) noexcept { return a = a | b; }
constexpr bool has_attr(Attr set, Attr flag) noexcept {
    return (static_cast<std::uint8_t>(set) & static_cast<std::uint8_t>(flag)) != 0;
}

// Which rule an underline is drawn with. Straight is the underline everything
// has always meant; the rest are the shapes the sub-parameter form of SGR 4
// added, and a compiler's diagnostics are what made them worth having — an
// editor marks a spelling mistake with a curly rule and a type error with a
// dotted one, and a terminal that can only draw one rule shows the same mark
// for both.
//
// The shape describes an underline that exists: it is meaningful only while
// `Attr::Underline` is set, and clearing the underline restores Straight so
// that two cells which look alike compare alike.
enum class UnderlineShape : std::uint8_t { Straight, Double, Curly, Dotted, Dashed };

struct Style {
    Color fg;
    Color bg;
    Attr attrs = static_cast<Attr>(0);
    UnderlineShape underline = UnderlineShape::Straight;
    // What the underline itself is drawn in. The default is not a colour: it
    // means the rule follows the text, which is what an underline does unless
    // a program says otherwise. A program says otherwise to put a red curl
    // under an error without recolouring the word.
    Color underline_color{};

    friend bool operator==(const Style&, const Style&) = default;
};

}  // namespace ckv
