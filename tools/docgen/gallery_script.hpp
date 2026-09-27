// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// Two event scripts over the shipped Gallery object graph (examples/gallery),
// each pinned as a paired raster script (plane_capture.hpp): every beat's
// golden is the stem of its four files.
//
//   * The picture script is the image demo of the roadmap M3: the Sixel Demo
//     picture scrolled a row by the wheel and a page by the keyboard, then
//     the View > Scheme menu opened over it and closed again (D-081).
//   * The scheme script chooses each of the four built-in schemes through
//     View > Scheme, the way a reader does (D-082).
//
// Played by tests/test_gallery_visual_golden.cpp and by
// generate_gallery_goldens; capture_gallery_screenshots plays the scheme
// script for the documentation's figures.
#pragma once

#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "event_script.hpp"
#include "gallery_app.hpp"

namespace ckv::docgen {

// The 80 x 24 Gallery on `profile`, about to play `beats`.
class GalleryStage {
public:
    GalleryStage(term::Capabilities profile, std::vector<ScriptBeat> beats);

    term::HeadlessTerminal terminal;
    ManualClock clock;
    ui::Application app{terminal, clock};
    gallery::GalleryApp gallery{app};
    ScriptPlayer player;

    // A cell over the Sixel Demo picture, below the rows the open
    // View > Scheme menu covers.
    static constexpr Point kPictureCell{18, 12};
};

// initial; scrolled a row by the wheel; scrolled a page by PageDown once a
// click has focused the viewport; View > Scheme open over the picture; and
// both menus closed again, which is pinned by the files of the frame they
// opened over. Beat names are the stems' suffixes.
std::vector<ScriptBeat> gallery_picture_script();
// Dark, Light, Mono, and back to Classic, each chosen through View > Scheme.
// Beat names are the schemes'.
std::vector<ScriptBeat> gallery_scheme_script();

}  // namespace ckv::docgen
