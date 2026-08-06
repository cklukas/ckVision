// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// A correct, public-protocol Sixel encoder (ARCHITECTURE.md §4/§7:
// "graphics encoders"). v1 scope: a deterministic, always-in-budget
// color quantization (exact palette up to 256 unique colors, else a
// 6-level-per-channel color cube — at most 216 registers, always
// within Sixel's typical 256-register limit). Content-adaptive
// palette optimization (better fidelity for images with structure a
// fixed cube doesn't serve well) is explicitly deferred to M8
// (ROADMAP.md: "Sixel palette optimization").
#pragma once

#include <string>

#include "cvision/core/image.hpp"

namespace ckv::term {

// Encodes `image` as a complete Sixel DCS sequence (DCS q ... ST),
// ready to interleave into presented output. Empty for an empty image.
std::string encode_sixel(const Image& image);

}  // namespace ckv::term
