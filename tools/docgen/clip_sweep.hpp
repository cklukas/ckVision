// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Width sweeps for the text-drawing widget families (WP-30: widget clipping
// must never split a grapheme cluster). Each family draws one mixed-cluster
// string — ASCII, a combining sequence, a CJK wide character, an emoji ZWJ
// sequence and a regional-indicator flag — at every width from 0 to the
// width at which all of it shows. Every width is its own Application on a
// HeadlessTerminal, built through public API only; its composed frame is
// copied into one band of the family's sweep surface, the bands stacked in
// width order. generate_clip_sweep_goldens pins each sweep as
// tests/golden/clip_sweep_<family>.dump, and tests/test_clip_sweep_golden.cpp
// compares the same sweeps with those files and checks that no cell holds a
// partial cluster.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/geometry.hpp"
#include "cvision/scene/surface.hpp"

namespace ckv::docgen::clip_sweep {

// The five cluster kinds, arranged so the wide ones start on odd and on even
// columns: A, 中 (U+4E2D), e + U+0301, the ZWJ family U+1F468 U+200D U+1F469
// U+200D U+1F467, z, and the flag U+1F1FA U+1F1F8. Nine columns in all.
inline constexpr std::string_view kMixedText =
    "A\xE4\xB8\xAD"
    "e\xCC\x81"
    "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7"
    "z\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8";
// The same text for the families that read '&' as a mnemonic marker, which
// marks the combining sequence: the accent is drawn as its own run, so it is
// one more place a cluster could be cut.
inline constexpr std::string_view kMnemonicText =
    "A\xE4\xB8\xAD"
    "&e\xCC\x81"
    "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7"
    "z\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8";

// One family's sweep: the stacked frames, and where the widget stood in each.
struct Sweep {
    // The golden's name: tests/golden/clip_sweep_<family>.dump.
    std::string family;
    // One band per width, top to bottom in width order. Each band is as wide
    // as the widest width plus one column, so a cell drawn past the widget
    // would show, and as tall as the widget at that width.
    scene::Surface surface;
    // The widget's bounds within `surface`, index = width. A width-0 widget
    // has an empty rectangle at the start of its band.
    std::vector<Rect> widget_bounds;
};

// Every family, in a fixed order: label, static_text, button, input_line,
// list_view, table, menu, status_line, text_view, tab_control.
std::vector<Sweep> render_all();

}  // namespace ckv::docgen::clip_sweep
