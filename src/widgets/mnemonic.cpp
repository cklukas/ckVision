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
