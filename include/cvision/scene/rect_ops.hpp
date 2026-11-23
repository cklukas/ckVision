// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Small, self-contained rectangle-set arithmetic used for raster-region
// occlusion slicing (ARCHITECTURE.md §3/§7) — plain computational
// geometry, not derived from any external source.
#pragma once

#include "cvision/core/export.hpp"

#include <vector>

#include "cvision/core/geometry.hpp"

namespace ckv::scene {

// The 0-4 axis-aligned rects covering `from` minus its overlap with
// `cut`: {from} unchanged if they don't overlap, empty if `cut` fully
// covers `from`.
CVISION_API std::vector<Rect> subtract_rect(Rect from, Rect cut);

// Subtracts each of `cuts` from `from` in turn, feeding each step's
// remainder into the next.
CVISION_API std::vector<Rect> subtract_rects(Rect from, const std::vector<Rect>& cuts);

}  // namespace ckv::scene
