// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <cstdint>

namespace ckv {

// 24-bit RGB, with a distinguished "default" (terminal's own foreground
// or background) that carries no RGB value. Degradation to 256/16 colors
// is a term-layer presentation concern (ARCHITECTURE.md §4); core only
// carries the truecolor request.
class Color {
public:
    constexpr Color() noexcept = default;

    static constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
        Color c;
        c.is_default_ = false;
        c.r_ = r;
        c.g_ = g;
        c.b_ = b;
        return c;
    }

    static constexpr Color default_color() noexcept { return Color{}; }

    constexpr bool is_default() const noexcept { return is_default_; }
    constexpr std::uint8_t r() const noexcept { return r_; }
    constexpr std::uint8_t g() const noexcept { return g_; }
    constexpr std::uint8_t b() const noexcept { return b_; }

    friend constexpr bool operator==(const Color& a, const Color& b) noexcept {
        if (a.is_default_ != b.is_default_) return false;
        if (a.is_default_) return true;
        return a.r_ == b.r_ && a.g_ == b.g_ && a.b_ == b.b_;
    }
    friend constexpr bool operator!=(const Color& a, const Color& b) noexcept {
        return !(a == b);
    }

private:
    bool is_default_ = true;
    std::uint8_t r_ = 0;
    std::uint8_t g_ = 0;
    std::uint8_t b_ = 0;
};

}  // namespace ckv
