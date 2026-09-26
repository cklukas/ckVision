// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/geometry.hpp"
#include "cvision/core/style.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

// Drawing helpers the stock widgets share for '&'-marked captions and inert
// states. They are an implementation detail of the widgets layer, not a
// surface applications are meant to build on.
//
// accent_style lays an accent on a surface: `accent`'s foreground on
// `surface`'s background, with both styles' attributes combined. The
// underline shape and colour are left at their defaults. This is how a mnemonic or a disabled
// foreground keeps the surface it is drawn on.
Style accent_style(Style surface, Style accent) noexcept;

// Draws `text.display` at `origin`, clipped to `max_width` columns (a
// negative width draws nothing), in `normal_style`, with the mnemonic
// grapheme in `mnemonic_style`. When there is no mnemonic, or clipping cut
// it off, the whole visible text is drawn in `normal_style`.
void draw_mnemonic(scene::Painter& painter, Point origin, const MnemonicText& text, int max_width,
                   Style normal_style, Style mnemonic_style);

}  // namespace ckv::widgets
