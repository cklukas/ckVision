// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A correct, public-protocol Sixel encoder (ARCHITECTURE.md §4/§7:
// "graphics encoders"). v1 scope: a deterministic, always-in-budget
// color quantization (exact palette while the unique colors fit the
// register limit, else a 6-level-per-channel color cube — at most 216
// registers, always within Sixel's typical 256-register limit — or, under
// a verified limit below 216, a smaller balanced RGB grid). Content-adaptive
// palette optimization (better fidelity for images with structure a
// fixed cube doesn't serve well) is explicitly deferred to M8
// (ROADMAP.md: "Sixel palette optimization").
#pragma once

#include <string>

#include "cvision/core/color.hpp"
#include "cvision/core/image.hpp"

namespace ckv::term {

// Options controlling adaptive Sixel encoding, stateless slice dithering,
// and alpha handling.
struct SixelEncodeOptions {
    // Positive XTSMGRAPHICS color register cap; 0 uses 256.
    int max_color_registers = 0;
    // Global pixel X offset for stateless 4x4 Bayer dither alignment across slices.
    int anchor_px_x = 0;
    // Global pixel Y offset for stateless 4x4 Bayer dither alignment across slices.
    int anchor_px_y = 0;
    // Background color used for straight-alpha compositing when opaque.
    Color background = Color::default_color();
    // When true, emit P2=1 and leave alpha==0 pixels transparent instead of compositing.
    bool preserve_transparent_holes = false;
};

// Encodes `image` as a complete Sixel DCS sequence (DCS 0;<P2>;0 q ... ST)
// with an exact raster extent, ready to interleave into presented output.
// Empty for an empty image. `max_color_registers` honors a positive
// XTSMGRAPHICS-reported limit; zero uses 256.
std::string encode_sixel(const Image& image, const SixelEncodeOptions& options);
std::string encode_sixel(const Image& image, int max_color_registers = 0);

}  // namespace ckv::term
