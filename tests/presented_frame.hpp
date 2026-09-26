// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The M8 "no stale cells" oracle for event scripts. A composed-frame golden
// proves what the Application meant to show; it cannot see a cell the
// Presenter failed to repaint or erase after a grow, a shrink, or a moved
// window. This compares the other end of the pipe: the screen the
// HeadlessTerminal's VirtualDisplay decoded from the bytes actually written.
//
// The comparison is exact, so the terminal must carry every composed colour
// without loss: a script using it runs on a TrueColor profile such as
// term::headless_no_graphics_profile(). Under the 256-colour baseline the
// Presenter rightly writes the nearest palette index for an RGB style, and
// the display reports that index, not the RGB value that was composed.
#pragma once

#include "cvision/core/frame_view.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"

namespace cktest_support {

// Whether `term` shows exactly the frame `app` composed last: the same grid
// size and, at every cell, the same grapheme, width and style. `term` must
// report ColorDepth::TrueColor (see above); any other depth answers false.
inline bool presented_equals_composed(const ckv::term::HeadlessTerminal& term, const ckv::ui::Application& app) {
    if (term.capabilities().color_depth != ckv::term::ColorDepth::TrueColor) return false;
    const ckv::FrameView presented = term.display().frame();
    const ckv::FrameView composed = app.current_frame();
    if (!(presented.size() == composed.size())) return false;
    for (int y = 0; y < composed.size().height; ++y)
        for (int x = 0; x < composed.size().width; ++x)
            if (!(presented.at(ckv::Point{x, y}) == composed.at(ckv::Point{x, y}))) return false;
    return true;
}

}  // namespace cktest_support
