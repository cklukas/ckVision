// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "cvision/core/assert.hpp"

namespace ckv {

// One of three things a colour can be, and the distinction is kept rather
// than collapsed:
//
//   * the terminal's own foreground or background, carrying no value of its
//     own — what a program means by "put it back the way it was";
//   * an entry in the terminal palette, named by its index — what a program
//     means by `SGR 31` or `SGR 38;5;208`;
//   * a specific 24-bit colour the program chose itself.
//
// Resolving a palette index to RGB on the way in would answer the palette
// question once, permanently, at the moment the byte arrived: the cell would
// remember red-as-it-was-then rather than "red", and re-theming a palette
// afterwards could no longer reach it. `palette.hpp` resolves an index when
// something actually needs pixels; everything in between carries the index.
//
// Degradation to 256/16 colours for a host that has fewer is a term-layer
// presentation concern (the architecture §4).
class Color {
public:
    constexpr Color() noexcept = default;

    static constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
        Color c;
        c.kind_ = Kind::Rgb;
        c.a_ = r;
        c.b_ = g;
        c.c_ = b;
        return c;
    }

    // A palette entry, 0-255: the 16 ANSI colours, the 6x6x6 cube, then the
    // grayscale ramp. `palette_color()` says which RGB each one names by
    // default.
    static constexpr Color indexed(std::uint8_t index) noexcept {
        Color c;
        c.kind_ = Kind::Indexed;
        c.a_ = index;
        return c;
    }

    static constexpr Color default_color() noexcept { return Color{}; }

    constexpr bool is_default() const noexcept { return kind_ == Kind::Default; }
    constexpr bool is_indexed() const noexcept { return kind_ == Kind::Indexed; }
    constexpr bool is_rgb() const noexcept { return kind_ == Kind::Rgb; }

    // Only an indexed colour has an index, and only an RGB one has channels.
    // Asking the wrong one is a contract violation rather than a defensible
    // default: silently answering 0 would turn a missing resolution step into
    // a black cell somewhere far away from the code that forgot it.
    constexpr std::uint8_t index() const noexcept {
        CKV_ASSERT(kind_ == Kind::Indexed);
        return a_;
    }
    constexpr std::uint8_t r() const noexcept {
        CKV_ASSERT(kind_ == Kind::Rgb);
        return a_;
    }
    constexpr std::uint8_t g() const noexcept {
        CKV_ASSERT(kind_ == Kind::Rgb);
        return b_;
    }
    constexpr std::uint8_t b() const noexcept {
        CKV_ASSERT(kind_ == Kind::Rgb);
        return c_;
    }

    friend constexpr bool operator==(const Color& a, const Color& b) noexcept {
        if (a.kind_ != b.kind_) return false;
        switch (a.kind_) {
            case Kind::Default: return true;
            case Kind::Indexed: return a.a_ == b.a_;
            case Kind::Rgb: return a.a_ == b.a_ && a.b_ == b.b_ && a.c_ == b.c_;
        }
        return false;
    }
    friend constexpr bool operator!=(const Color& a, const Color& b) noexcept {
        return !(a == b);
    }

private:
    enum class Kind : std::uint8_t { Default, Indexed, Rgb };

    // Three payload bytes, read through the accessors above: red/green/blue
    // for an RGB colour, the index in the first byte for a palette one, and
    // nothing at all for the default. Kept as one shape so a Color stays
    // four bytes — it is a member of every cell on the screen.
    Kind kind_ = Kind::Default;
    std::uint8_t a_ = 0;
    std::uint8_t b_ = 0;
    std::uint8_t c_ = 0;
};

}  // namespace ckv
