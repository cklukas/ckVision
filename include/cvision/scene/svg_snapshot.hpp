// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A cell grid as a self-contained SVG image: the screenshot format for
// documentation, release notes and bug reports, taken from what a frame
// actually holds rather than from a host terminal's pixels. Whatever produced
// the grid — an Application's last presented frame, a virtual display, a
// child terminal's decoded snapshot — the picture is the same function of its
// cells, so two runs over identical cells give byte-identical files on every
// platform.
//
// SVG rather than a raster format keeps the library free of image-encoder
// dependencies while producing something every browser and most document
// pipelines render natively. A raster plane (decoded Sixel pixels) is the one
// part that cannot be vector graphics; it is embedded as a PNG data URI,
// encoded here without compression so no codec is needed.
#pragma once

#include <string>

#include "cvision/core/frame_view.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"

namespace ckv::scene {

// How render_svg_snapshot lays the grid out.
struct SvgSnapshotOptions {
    // The size of one cell in the SVG, in pixels. Glyph size, rule weight and
    // the geometry of a raster plane all follow from it.
    PixelSize cell_pixels{9, 18};
    // The CSS font-family list every glyph is drawn with, stated once in the
    // image's stylesheet. It is emitted verbatim, so it must not contain '}'.
    std::string font_family =
        "ui-monospace, 'SF Mono', 'Cascadia Code', 'DejaVu Sans Mono', monospace";

    // The cell rectangle to emit — a cut-out of the grid, for a figure that
    // shows one control with its real surroundings instead of the whole
    // screen. An empty rect (the default) renders the whole grid. The rect is
    // clamped to the grid; one that falls entirely outside it renders the
    // whole grid rather than an empty image.
    Rect crop{};
};

// Renders every cell of `frame` as a background plus (for non-continuation
// cells) a glyph, honoring Attr::Bold, Attr::Underline with its shape and
// color, and Attr::Reverse. Box-drawing junctions and block elements are
// drawn as cell-edge geometry rather than font glyphs, so frames join
// seamlessly whatever font the viewer has. A default (unset) foreground or
// background resolves to light gray on black, since an SVG has no terminal
// default to defer to.
//
// `raster_plane`, when given, is drawn over the cells at
// `options.cell_pixels` per cell — the layering a terminal uses for Sixel
// pixels — and clipped to the crop. Only its non-transparent bounds are
// embedded. The result is the complete SVG document text.
std::string render_svg_snapshot(const FrameView& frame, const SvgSnapshotOptions& options = {},
                                const Image* raster_plane = nullptr);

}  // namespace ckv::scene
