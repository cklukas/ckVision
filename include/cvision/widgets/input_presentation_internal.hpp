// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/scene/painter.hpp"
#include "cvision/widgets/input_presentation.hpp"

namespace ckv::widgets::detail {

// The same field chrome surrounds an InputLine and composite field content.
// It changes only the surface, never text/editor ownership or event routing.
inline void draw_input_surface(scene::Painter& painter, Size size,
                               InputPresentation presentation, Style style, bool focused) {
    const int width = std::max(0, size.width);
    const int height = std::min(std::max(0, size.height), input_presentation_height(presentation));
    auto clipped = painter.clipped(Rect{0, 0, width, height});
    clipped.fill(Rect{0, 0, width, height}, Cell::from_grapheme(" ", style));
    if (presentation == InputPresentation::Underlined && height > 1) {
        if (focused) style.attrs |= Attr::Bold;
        clipped.fill(Rect{0, 1, width, 1}, Cell::from_grapheme("─", style));
    }
}

}  // namespace ckv::widgets::detail
