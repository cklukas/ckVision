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

// Captures `surface`'s full content (cells, styles, raster regions) and
// `cursor` into a golden Document. The style table is deduplicated on
// capture; a surface with more than 62 distinct styles (the v1 golden
// format's limit) is a test-authoring bug, not a runtime scenario —
// CKV_ASSERT enforces it.
golden::Document capture(const Surface& surface, CursorState cursor = {});

// Captures a Compositor's composed frame, including occlusion-sliced
// raster regions (the architecture §3/§7): each of Compositor's
// visible_rasters() entries becomes its own golden raster record. When
// occlusion has split one logical raster region (one source id) into
// several visible slices, those slices are assigned synthetic
// sequential ids in the dump (1, 2, 3, ... in visible-raster order) —
// the golden format itself requires per-dump id uniqueness
// (docs/golden-format.md) and has no notion of "these N records are one
// logical region," so the original source id is not recoverable from a
// multi-slice capture alone. This is a capture-time convention, not a
// format change: golden.hpp is untouched.
golden::Document capture_frame(const Compositor& compositor, CursorState cursor = {});

// Deterministic content fingerprint (FNV-1a, 64-bit, over the raw pixel
// bytes) used for the golden dump's symbolic raster representation —
// a change-detection tool, not a cryptographic hash.
std::string image_content_hash(const Image& image);

}  // namespace ckv::scene
