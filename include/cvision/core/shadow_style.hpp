// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// How a cast shadow darkens what it falls on (D-037, D-106).
//
// A theme states it (ui::Theme::shadow()); the compositor applies it to every
// cell a shadow covers, and the presenter to every pixel of a picture a shadow
// covers, so cells and pictures under one shadow darken alike. It is a core
// value because both of those layers need it and neither may depend on the
// other (the architecture §1). Every transform here is integer arithmetic on
// the colour channels, so a shadow darkens identically on every platform.
#pragma once

#include <cstdint>

#include "cvision/core/color.hpp"
#include "cvision/core/image.hpp"
#include "cvision/core/palette.hpp"
#include "cvision/core/style.hpp"

namespace ckv {

// One shadow transform: halve every colour channel, or recolour to a fixed
// foreground and background. Either way the glyph, the attributes and the
// underline shape are kept, and applying it is the whole effect of a shadow:
// however many shadows cover a cell, it is applied once (D-037).
class ShadowStyle {
public:
    // The two transforms a scheme chooses between.
    enum class Kind : std::uint8_t {
        // Every RGB channel of the foreground and background halved, rounding
        // down: a darker copy of what lies beneath, which lets the colours of
        // the covered content show through. Dark, Light and Mono shadows.
        Halve,
        // The foreground and background replaced by the style's own pair: the
        // covered content's shapes stay, its colours go. The Classic shadow,
        // dark grey on black.
        Recolor,
    };

    // A halving shadow, the default.
    constexpr ShadowStyle() noexcept = default;

    // A halving shadow; the same value the default constructor makes.
    static constexpr ShadowStyle halve() noexcept { return ShadowStyle{}; }
    // A recolouring shadow that draws what it covers in `foreground` on
    // `background`. Either may be a palette index or the terminal's default.
    static constexpr ShadowStyle recolor(Color foreground, Color background) noexcept {
        return ShadowStyle{Kind::Recolor, foreground, background};
    }

    // Which transform this is.
    constexpr Kind kind() const noexcept { return kind_; }
    // A recolouring shadow's foreground and background; the default colour
    // for a halving one, which has none.
    constexpr Color foreground() const noexcept { return foreground_; }
    constexpr Color background() const noexcept { return background_; }

    // `style` under this shadow. Halving resolves a palette index to its
    // default RGB first and takes the terminal's default colour as black, so
    // the result is always a concrete colour. Recolouring sets the foreground
    // and background to this style's pair. An underline colour of the cell's
    // own is darkened with it (halved, or set to the shadow's foreground); one
    // that follows the text stays default and follows the new foreground.
    constexpr Style apply(Style style) const noexcept {
        Style out = style;
        if (kind_ == Kind::Halve) {
            out.fg = halved(style.fg);
            out.bg = halved(style.bg);
            if (!style.underline_color.is_default()) out.underline_color = halved(style.underline_color);
            return out;
        }
        out.fg = foreground_;
        out.bg = background_;
        if (!style.underline_color.is_default()) out.underline_color = foreground_;
        return out;
    }

    // `pixel` under this shadow; alpha is kept. Halving halves each channel,
    // rounding down, as it does a cell's colours. Recolouring maps the pixel's
    // luminance (the integer BT.601 weighting, 299:587:114) linearly into the
    // range from black to the shadow's foreground, so a picture keeps its
    // shapes and shading in the colour the shadow draws glyphs in. A
    // foreground that is the terminal's default has no channels and maps every
    // pixel to black.
    constexpr Image::Rgba apply(Image::Rgba pixel) const noexcept {
        if (kind_ == Kind::Halve)
            return Image::Rgba{static_cast<std::uint8_t>(pixel.r / 2), static_cast<std::uint8_t>(pixel.g / 2),
                               static_cast<std::uint8_t>(pixel.b / 2), pixel.a};
        const int luminance = (299 * pixel.r + 587 * pixel.g + 114 * pixel.b + 500) / 1000;
        const Color ink = resolved_color(foreground_, Color::rgb(0, 0, 0));
        const auto scaled = [luminance](std::uint8_t channel) {
            return static_cast<std::uint8_t>(channel * luminance / 255);
        };
        return Image::Rgba{scaled(ink.r()), scaled(ink.g()), scaled(ink.b()), pixel.a};
    }

    // Equal when the kind matches and so do the colours.
    friend constexpr bool operator==(const ShadowStyle&, const ShadowStyle&) noexcept = default;

private:
    // Built whole rather than by assigning into a default value, as Color is.
    constexpr ShadowStyle(Kind kind, Color foreground, Color background) noexcept
        : kind_(kind), foreground_(foreground), background_(background) {}

    static constexpr Color halved(Color color) noexcept {
        const Color rgb = resolved_color(color, Color::rgb(0, 0, 0));
        return Color::rgb(static_cast<std::uint8_t>(rgb.r() / 2), static_cast<std::uint8_t>(rgb.g() / 2),
                          static_cast<std::uint8_t>(rgb.b() / 2));
    }

    Kind kind_ = Kind::Halve;
    Color foreground_;
    Color background_;
};

}  // namespace ckv
