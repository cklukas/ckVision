// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Bridges the scene layer to the golden dump format (the decision log
// D-014): captures a Surface into a core::golden::Document, the
// project's specification medium for appearance and behavior tests.
#pragma once

#include <string>

#include "cvision/core/golden.hpp"
#include "cvision/core/image.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/cursor.hpp"
#include "cvision/scene/surface.hpp"

namespace ckv::scene {

// Captures `surface`'s full content (cells, styles, hyperlink runs, raster
// regions) and `cursor` into a golden Document. The style table is
// deduplicated on capture; a surface with more than 62 distinct styles (the
// v1 golden format's limit) is a test-authoring bug, not a runtime scenario
// — CKV_ASSERT enforces it. Of the cursor, only its visibility and, when
// visible, its cell and shape are recorded (here and in capture_frame);
// the blink settings are not.
golden::Document capture(const Surface& surface, CursorState cursor = {});

// Captures a Compositor's composed frame, including occlusion-sliced
// raster regions (the architecture §3/§7): each of Compositor's
// visible_rasters() entries becomes its own golden raster record, anchored
// at the slice's visible rect, whose pixel extent and hash are those of the
// part of the picture the slice shows -- its visible rect's place within the
// full anchor, in the picture's own pixels, and at least one pixel each way.
// A picture shown whole records the picture; one scrolled or occluded
// records which part is on screen, so moving a picture under a fixed clip
// changes its record even where its visible cells do not move. Every record is assigned a synthetic
// sequential id in the dump (1, 2, 3, ... in visible-raster order), whether
// or not occlusion split its region into several slices — the golden
// format itself requires per-dump id uniqueness (docs/golden-format.md)
// and has no notion of "these N records are one logical region," so the
// original source id is not recoverable from a frame capture. This is a
// capture-time convention, not a format change: golden.hpp is untouched.
golden::Document capture_frame(const Compositor& compositor, CursorState cursor = {});

// Deterministic content fingerprint (FNV-1a, 64-bit, over the raw pixel
// bytes) used for the golden dump's symbolic raster representation —
// a change-detection tool, not a cryptographic hash. Returned as 16
// lowercase hexadecimal digits; the dimensions are not hashed, only the bytes.
std::string image_content_hash(const Image& image);

}  // namespace ckv::scene
