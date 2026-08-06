// Copyright (c) 2026 C. Klukas. All rights reserved.
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

struct Style {
    Color fg;
    Color bg;
    Attr attrs = static_cast<Attr>(0);

    friend bool operator==(const Style&, const Style&) = default;
};

}  // namespace ckv
