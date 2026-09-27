// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// A scrolled picture is a re-anchored raster region (the decision log D-081):
// the scrolling widget repaints what it shows, and the picture inside it is
// drawn again at its new anchor, clipped to what is visible. This stage
// holds the two ways a picture scrolls in ckVision, side by side on one
// desktop, built from the public API alone:
//
//   * a "Viewport" window, whose ScrollViewport wraps an ImageView twice as
//     tall as the viewport, scrolled a row by Down once a click on the
//     picture has focused it, then a wheel notch's worth by the wheel;
//   * a "Flow" window, whose FlowView holds a picture inline between two
//     paragraphs, scrolled by the arrow keys once a click has focused it.
//
// The script scrolls each by one row and then by three more. Every beat is
// pinned as a paired raster script (plane_capture.hpp): the stem of its
// four files is its golden. Played by tests/test_raster_scroll_golden.cpp
// and by generate_raster_scroll_goldens.
#pragma once

#include <memory>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/core/image.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/view.hpp"
#include "event_script.hpp"

namespace ckv::widgets {
class FlowView;
class ScrollViewport;
}  // namespace ckv::widgets

namespace ckv::docgen {

// A 48 x 14 desktop with no menu bar or status line, on `profile`.
class RasterScrollStage {
public:
    explicit RasterScrollStage(term::Capabilities profile);

    term::HeadlessTerminal terminal;
    ManualClock clock;
    ui::Application app{terminal, clock};
    ScriptPlayer player;

    widgets::ScrollViewport& viewport() const noexcept { return *viewport_; }
    widgets::FlowView& flow() const noexcept { return *flow_; }
    // The two pictures, which identify their slices among the compositor's
    // visible rasters.
    const std::shared_ptr<const Image>& viewport_picture() const noexcept { return viewport_picture_; }
    const std::shared_ptr<const Image>& flow_picture() const noexcept { return flow_picture_; }

    // Cells the script aims at: one over the viewport's picture, and one on
    // the flow's first paragraph, where a click focuses the view and follows
    // no link.
    static constexpr Point kViewportPictureCell{10, 6};
    static constexpr Point kFlowTextCell{28, 2};
    // How many rows the second scroll of each view adds to the first: one
    // wheel notch's worth, which the viewport's second scroll is.
    static constexpr int kSecondScrollRows = ui::kWheelRows;

private:
    widgets::ScrollViewport* viewport_ = nullptr;
    widgets::FlowView* flow_ = nullptr;
    std::shared_ptr<const Image> viewport_picture_;
    std::shared_ptr<const Image> flow_picture_;
};

// initial; viewport scrolled by 1 and then by 4 rows; flow scrolled by 1 and
// then by 4 rows. Beat names are the stems' suffixes ("viewport_1"...).
std::vector<ScriptBeat> raster_scroll_script();

}  // namespace ckv::docgen
