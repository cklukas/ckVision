// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "cvision/core/color.hpp"

namespace ckv {

// Text attributes as independent bit flags; an Attr value holds any
// combination, and `static_cast<Attr>(0)` is the empty set. Each flag is one
// SGR rendition the presenter emits: bold (1), dim (2), italic (3),
// underline (4, shaped by Style::underline), reverse video (7) and
// strikethrough (9).
enum class Attr : std::uint8_t {
    Bold = 1u << 0,
    Dim = 1u << 1,
    Italic = 1u << 2,
    Underline = 1u << 3,
    Reverse = 1u << 4,
    Strike = 1u << 5,
};

// The union of two attribute sets.
constexpr Attr operator|(Attr a, Attr b) noexcept {
    return static_cast<Attr>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
// Adds the flags of `b` to `a` and returns `a`.
constexpr Attr& operator|=(Attr& a, Attr b) noexcept { return a = a | b; }
// Whether `set` contains any flag of `flag`. With a single flag, as intended,
// that is simply "is it set"; the empty set is never contained.
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

// Everything about how a cell's content looks apart from the glyph itself.
// A default-constructed Style is the terminal's own colours with no
// attributes.
struct Style {
    // Foreground (glyph) and background colours; the default Color leaves
    // the terminal's own foreground or background in place.
    Color fg;
    Color bg;
    // The attribute flags set on this style; none by default.
    Attr attrs = static_cast<Attr>(0);
    // The underline's shape, read only while `attrs` includes
    // Attr::Underline.
    UnderlineShape underline = UnderlineShape::Straight;
    // What the underline itself is drawn in. The default is not a colour: it
    // means the rule follows the text, which is what an underline does unless
    // a program says otherwise. A program says otherwise to put a red curl
    // under an error without recolouring the word.
    Color underline_color{};

    // Memberwise equality. The underline shape and colour take part even
    // while Attr::Underline is clear, so a producer that clears the underline
    // resets them too if equal-looking cells are to compare equal.
    friend bool operator==(const Style&, const Style&) = default;
};

}  // namespace ckv
