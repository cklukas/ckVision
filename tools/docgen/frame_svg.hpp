// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation tooling, NOT part of the cvision library: renders a
// composed FrameView (whatever a real Application last presented) to
// a self-contained SVG image — the "automatically generated
// screenshots (from the virtual terminal rendering)" the example-apps
// documentation embeds. SVG rather than a rasterized format so this
// stays true to ckVision's own zero-dependency rule (no PNG encoder
// needed) while still producing something every browser and most
// document pipelines render natively.
#pragma once

#include <string>

#include "cvision/core/frame_view.hpp"
#include "cvision/term/virtual_display.hpp"

namespace ckv::docgen {

struct FrameSvgOptions {
    int cell_width_px = 9;
    int cell_height_px = 18;
    std::string font_family = "ui-monospace, 'SF Mono', 'Cascadia Code', 'DejaVu Sans Mono', monospace";
};

// Renders every cell of `frame` as a background rect plus (for
// non-continuation cells) a glyph, honoring Attr::Bold/Underline and
// swapping fg/bg for Attr::Reverse. Recognized box-drawing cells use
// deterministic cell-edge SVG geometry instead of host-font metrics. A
// default (unset) fg/bg resolves to the classic terminal default pair
// (light gray on black) since SVG has no "inherit the terminal's own
// color" concept to defer to.
std::string render_frame_svg(const FrameView& frame, const FrameSvgOptions& options = {});

// Renders the independently decoded terminal state, including Sixel
// pixels received through Terminal::write. Raster pixels are drawn
// after the styled-cell plane, matching Presenter/terminal layering.
// The display's fixed cell-pixel metrics define SVG geometry.
std::string render_virtual_display_svg(const term::VirtualDisplay& display,
                                       const FrameSvgOptions& options = {});

}  // namespace ckv::docgen
