// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>

#include "cvision/core/geometry.hpp"

namespace ckv::widgets {

// Color identifies the field in every presentation. Padded adds one column
// at each side; Underlined adds that padding and a second row for a rule.
// None draws brackets or a surrounding frame (D-119).
enum class InputPresentation { Flat, Padded, Underlined };

// The requested height. Small actual bounds clip, never change presentation.
constexpr int input_presentation_height(InputPresentation presentation) noexcept {
    return presentation == InputPresentation::Underlined ? 2 : 1;
}

// Content geometry shared by fields and their composite owners. Coordinates
// are local to the field. Width/height remain nonnegative in tiny bounds.
constexpr Rect input_content_rect(InputPresentation presentation, Size actual) noexcept {
    const int width = std::max(0, actual.width);
    const int inset = presentation == InputPresentation::Flat ? 0 : 1;
    return Rect{std::min(inset, width), 0, std::max(0, width - 2 * inset), actual.height > 0 ? 1 : 0};
}

}  // namespace ckv::widgets
