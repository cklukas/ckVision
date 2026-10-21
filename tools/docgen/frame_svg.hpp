// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation tooling, NOT part of the cvision library: the one capture
// the public scene::render_svg_snapshot cannot take by itself, because the
// scene layer may not see term::VirtualDisplay. Everything about how cells
// become SVG lives in cvision/scene/svg_snapshot.hpp.
#pragma once

#include <string>

#include "cvision/scene/svg_snapshot.hpp"
#include "cvision/term/virtual_display.hpp"

namespace ckv::ui { class Application; }
namespace ckv::term { class HeadlessTerminal; }

namespace ckv::docgen {

// Renders the independently decoded terminal state, including Sixel
// pixels received through Terminal::write. Raster pixels are drawn
// after the styled-cell plane, matching Presenter/terminal layering.
// The display's fixed cell-pixel metrics replace options.cell_pixels.
std::string render_virtual_display_svg(const term::VirtualDisplay& display,
                                       scene::SvgSnapshotOptions options = {});
// Verify decoded glyphs, widths, links and capability-projected styles against
// the application's composed cells before publishing a text-mode screenshot.
void verify_decoded_cells(const ui::Application& app, const term::HeadlessTerminal& terminal);

}  // namespace ckv::docgen
