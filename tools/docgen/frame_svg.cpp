// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "frame_svg.hpp"

namespace ckv::docgen {

std::string render_virtual_display_svg(const term::VirtualDisplay& display,
                                       scene::SvgSnapshotOptions options) {
    options.cell_pixels = display.cell_pixels();
    return scene::render_svg_snapshot(display.frame(), options, &display.raster_plane());
}

}  // namespace ckv::docgen
