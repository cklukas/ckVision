// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <algorithm>

#include "cvision/ui/view.hpp"

namespace ckv::ui::detail {

inline int preferred_height_for_width(const View& view, int width) {
    const SizeHint vertical = view.vertical_size_hint();
    const int measured = view.height_for_width(std::max(0, width));
    return std::max(vertical.min, std::max(vertical.preferred, measured));
}

}  // namespace ckv::ui::detail
