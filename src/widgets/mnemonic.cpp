// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/mnemonic_internal.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"

namespace ckv::widgets {

MnemonicText parse_mnemonic(std::string_view raw) {
    MnemonicText result;
    result.display.reserve(raw.size());
    bool marked_next = false;
    for (std::size_t i = 0; i < raw.size();) {
        if (raw[i] == '&') {
            if (i + 1 < raw.size() && raw[i + 1] == '&') {
                result.display.push_back('&');  // "&&" -> literal '&'
                i += 2;
                continue;
            }
            marked_next = true;
            ++i;
            continue;
        }
        const std::size_t end = text::grapheme_end(raw, i);
        const std::string_view grapheme = raw.substr(i, end - i);
        if (marked_next && result.mnemonic.empty()) {
            result.mnemonic_byte_offset = result.display.size();
            result.mnemonic = std::string(grapheme);
        }
        marked_next = false;
        result.display.append(grapheme);
        i = end;
    }
    return result;
}

Style accent_style(Style surface, Style accent) noexcept {
    return Style{accent.fg, surface.bg, surface.attrs | accent.attrs};
}

namespace {

// Relative luminance on the 0..255 scale, from the encoded channels with the
// usual perceptual weights and no linearisation: the question is "do these
// two read together", not "what is the exact ratio", and an integer answer
// is the same on every platform.
int luminance(const Color& color) noexcept {
    return (2126 * color.r() + 7152 * color.g() + 722 * color.b()) / 10000;
}

}  // namespace

bool readable(const Color& foreground, const Color& background) noexcept {
    if (foreground == background) return false;
    if (!foreground.is_rgb() || !background.is_rgb()) return true;
    const int lighter = std::max(luminance(foreground), luminance(background));
    const int darker = std::min(luminance(foreground), luminance(background));
    return lighter * 100 + 1275 >= 3 * (darker * 100 + 1275);
}

bool sets_color(const Style& style) noexcept { return style.fg != Color{} || style.bg != Color{}; }

Style highlight_over(Style own, bool own_has_color, Style highlight, bool cursor, bool active) noexcept {
    if (!own_has_color) {
        own.fg = highlight.fg;
        own.bg = highlight.bg;
        own.attrs |= highlight.attrs;
        return own;
    }
    Style swapped = own;
    swapped.fg = own.bg;
    swapped.bg = own.fg;
    if (!readable(swapped.fg, swapped.bg)) {
        swapped.fg = highlight.fg;
        swapped.bg = highlight.bg;
    }
    if (cursor) swapped.attrs |= active ? Attr::Bold | Attr::Underline : Attr::Underline;
    return swapped;
}

void draw_mnemonic(scene::Painter& painter, Point origin, const MnemonicText& text, int max_width,
                   Style normal_style, Style mnemonic_style) {
    const std::string shown = text::clip_to_width(text.display, std::max(0, max_width));
    if (text.mnemonic_byte_offset == std::string::npos) {
        painter.draw_text(origin, shown, normal_style);
        return;
    }
    const std::size_t mnemonic_end = text.mnemonic_byte_offset + text.mnemonic.size();
    if (mnemonic_end > shown.size()) {
        painter.draw_text(origin, shown, normal_style);
        return;
    }
    const std::string_view before{shown.data(), text.mnemonic_byte_offset};
    const std::string_view marked{shown.data() + text.mnemonic_byte_offset, text.mnemonic.size()};
    const std::string_view after{shown.data() + mnemonic_end, shown.size() - mnemonic_end};
    painter.draw_text(origin, before, normal_style);
    const int marked_x = origin.x + text::text_width(before);
    painter.draw_text(Point{marked_x, origin.y}, marked, mnemonic_style);
    painter.draw_text(Point{marked_x + text::text_width(marked), origin.y}, after, normal_style);
}

}  // namespace ckv::widgets
