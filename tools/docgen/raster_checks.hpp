// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// Oracles over a decoded pixel plane (VirtualDisplay::raster_plane()) and
// the compositor's visible slices of the frame that produced it. They ask
// what a reader would see go wrong: a pixel left behind where no picture is
// any longer shown, a hole where one is, or a picture that did not move by
// exactly as many rows as its anchor did. Every picture they are used with
// is fully opaque, as an encoded Sixel is.
#pragma once

#include <optional>
#include <vector>

#include "cvision/core/frame_view.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"

namespace ckv::docgen {

// The slices of `slices` cut from `picture`, in frame order.
std::vector<RasterSlice> slices_of(const std::vector<RasterSlice>& slices, const Image* picture);
// The full anchor `picture` has in the frame, which every slice of it
// shares; empty when no part of it is visible.
std::optional<Rect> full_anchor_of(const std::vector<RasterSlice>& slices, const Image* picture);

// How many pixels of `plane` contradict `slices`: opaque where no slice's
// visible cells are, or not opaque where one's are. Zero means the plane
// shows every visible slice and nothing else -- no stale pixel survives.
int stray_pixels(const Image& plane, const std::vector<RasterSlice>& slices, PixelSize cell);

// Compares the pixels of one picture in two frames, where its anchor moved
// from `before_anchor` to `after_anchor` between them. Every pixel the
// picture shows in `after` whose place in the picture was also shown in
// `before` must be the same pixel, exactly. That holds while the Sixel
// encoder colours both frames' slices alike: all within its registers, or
// all beyond them (see covered_pixels_compared). Returns how many pixels
// were compared, or -1 at the first that differs.
int moved_pixels_compared(const Image& before, Rect before_anchor, const std::vector<RasterSlice>& before_slices,
                          const Image& after, Rect after_anchor, const std::vector<RasterSlice>& after_slices,
                          PixelSize cell);

// Compares a frame in which a picture that did not move is partly covered
// with the frame before it was: every pixel of `after_slices` must show
// `before`'s pixel at the same place where the slice lies in the open, and
// that pixel passed through the slice's shadow (RasterSlice::shadow) where it
// lies under one, which must also leave it darker unless it was black. Both
// frames went through the Sixel encoder, which keeps a slice's own colours
// when they fit its registers and otherwise rounds each channel to one of six
// levels, by at most 25.5. Covering a picture cuts it into slices with fewer
// colours, so one frame may hold a pixel rounded that the other holds
// exactly: an open pixel may differ by that much, and a shadowed one,
// compared with the shadow of a pixel that may itself have been rounded, by
// half as much again under the built-in shadows (halving halves a rounding
// error, and the Classic recolouring, whose foreground is a third of white,
// shrinks it to a third). Returns how many pixels were compared, or -1 at the
// first that disagrees.
int covered_pixels_compared(const Image& before, const Image& after, const std::vector<RasterSlice>& after_slices,
                            PixelSize cell);

}  // namespace ckv::docgen
