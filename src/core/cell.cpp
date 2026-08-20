// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/cell.hpp"

#include "cvision/core/text.hpp"

namespace ckv {

Cell Cell::from_grapheme(std::string_view grapheme, Style style) {
    std::string sanitized = text::sanitize_display_text(grapheme);
    if (sanitized.empty()) sanitized = " ";
    const std::size_t end = text::grapheme_end(sanitized, 0);
    sanitized.resize(end);
    const int width = text::grapheme_width(sanitized);
    return Cell(std::move(sanitized), style, width);
}

}  // namespace ckv
