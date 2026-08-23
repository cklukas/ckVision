// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/geometry.hpp"
#include "cvision/core/style.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

Style accent_style(Style surface, Style accent) noexcept;

void draw_mnemonic(scene::Painter& painter, Point origin, const MnemonicText& text, int max_width,
                   Style normal_style, Style mnemonic_style);

}  // namespace ckv::widgets
