// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/geometry.hpp"
#include "cvision/core/style.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

// Drawing helpers the stock widgets share for '&'-marked captions, inert
// states and highlights. They are an implementation detail of the widgets
// layer, not a surface applications are meant to build on.
//
// accent_style lays an accent on a surface: `accent`'s foreground on
// `surface`'s background, with both styles' attributes combined. The
// underline shape and colour are left at their defaults. This is how a mnemonic or a disabled
// foreground keeps the surface it is drawn on.
Style accent_style(Style surface, Style accent) noexcept;

// Whether text in `foreground` reads on `background`: the two differ, and the
// contrast -- the lighter luminance over the darker, each lifted by five
// percent of the scale -- reaches 3:1 by an integer luminance estimate. A
// colour that cannot be measured (an indexed one, or the terminal's default)
// is taken as readable: it says something the terminal knows and the widget
// does not.
bool readable(const Color& foreground, const Color& background) noexcept;

// How a cursor or a selection is drawn over content that styles itself
// (D-067), so its colouring stays visible under the highlight and a theme
// still decides what a highlight is. `own` is the content's style and
// `own_has_color` whether it sets a colour of its own; `highlight` is the
// highlight role's style.
//   * Content with no colour of its own wears `highlight`'s colours, with
//     `highlight`'s attributes added to its own.
//   * Content with a colour keeps both of its colours and swaps them; when
//     the swapped pair is not readable, `highlight`'s colours are used
//     instead. A cursor (`cursor` true) is then additionally bold and
//     underlined while its view holds the keyboard (`active`), and only
//     underlined otherwise, so it stays distinguishable inside a selection
//     drawn from the same two colours.
Style highlight_over(Style own, bool own_has_color, Style highlight, bool cursor, bool active) noexcept;
// Whether a whole style sets a colour of its own: a foreground or a
// background other than the terminal's default. The question highlight_over
// asks of content styled by a complete Style rather than field by field.
bool sets_color(const Style& style) noexcept;

// Draws `text.display` at `origin`, clipped to `max_width` columns (a
// negative width draws nothing), in `normal_style`, with the mnemonic
// grapheme in `mnemonic_style`. When there is no mnemonic, or clipping cut
// it off, the whole visible text is drawn in `normal_style`.
void draw_mnemonic(scene::Painter& painter, Point origin, const MnemonicText& text, int max_width,
                   Style normal_style, Style mnemonic_style);

}  // namespace ckv::widgets
