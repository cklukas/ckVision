// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The Spin example's software renderer: one Mesh, one orientation, one
// RGBA Image. It depends on nothing above it — no view, no terminal, no
// clock — which is what lets a worker thread call it while the
// application keeps running, and a test call it with nothing else running
// at all.
//
// Two of its properties exist specifically so that the result sits INSIDE
// a window rather than on top of one:
//
//   * The frame is painted on the caller's own background color, and the
//     object is centred in it. Sixel carries no alpha channel, so a
//     picture that does not bring the surrounding surface with it arrives
//     as a rectangle of some other color pasted over the window.
//   * The frame is built from a bounded color table. The Sixel encoder
//     reproduces colors exactly while it has a register per color, and
//     otherwise quantizes the WHOLE picture to a fixed color cube —
//     including that background, which would then no longer match the
//     cells around it. Staying inside the register budget is what keeps
//     the seam invisible.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"

#include "mesh.hpp"

namespace ckv::spin {

// Edges are anti-aliased by rasterizing into a grid of this many samples
// per axis and averaging. Four samples per pixel is the whole of it:
// silhouette edges land on quarters rather than on whole pixels, which is
// the difference between an object that sits in the window and one that
// was cut out with scissors.
inline constexpr int kSupersample = 2;
inline constexpr int kSupersampleCount = kSupersample * kSupersample;

// The color table one frame is allowed to use, with the background as
// entry 0. Indices are bytes: a Sixel register budget never exceeds 256,
// so the same byte addresses both this table and the picture the encoder
// will build from it.
class FramePalette {
public:
    // Starts a new frame. `budget` is clamped to [2, 256].
    void reset(Image::Rgba background, int budget) noexcept;

    // The index for `color`: the matching entry when one exists, a fresh
    // one while the table has room, and otherwise the closest entry
    // already present. Exhausting the budget coarsens the shading; it
    // never costs the frame its background or its geometry.
    std::uint8_t intern(Image::Rgba color) noexcept;

    Image::Rgba color(std::uint8_t index) const noexcept {
        return entries_[static_cast<std::size_t>(index)];
    }
    int size() const noexcept { return size_; }
    int budget() const noexcept { return budget_; }

private:
    std::array<Image::Rgba, 256> entries_{};
    int size_ = 0;
    int budget_ = 256;
};

struct FrameSpec {
    Size pixels{0, 0};
    // The surface the object is standing on — normally the resolved
    // background of the window that will show the frame.
    Image::Rgba background{0, 0, 0, 255};
    double yaw = 0.0;    // radians about the vertical axis
    double pitch = 0.0;  // radians about the horizontal axis
    // How many colors the host's Sixel decoder holds at once.
    int color_budget = 256;
};

class Renderer {
public:
    // Draws `mesh` into a fresh Image of `spec.pixels`. Scratch buffers
    // are reused between calls, so a warmed renderer allocates only the
    // frame it hands back.
    //
    // One Renderer per thread. It is deliberately not thread-safe: shared
    // scratch would be the one place this example needed a lock, and a
    // renderer is a few hundred kilobytes, not a resource worth sharing.
    Image render(const Mesh& mesh, const FrameSpec& spec);

    const FramePalette& palette() const noexcept { return palette_; }

private:
    struct Point2 {
        double x = 0.0;
        double y = 0.0;
    };
    // One face or edge, with what the painter needs to place and light it:
    // its distance from the viewer, and how squarely it faces them.
    struct Ordered {
        int index = 0;
        double depth = 0.0;
        double light = 0.0;
    };

    void project(const Mesh& mesh, const FrameSpec& spec, Size raster);
    void draw_faces(const Mesh& mesh, int levels, Size raster);
    void draw_edges(const Mesh& mesh, int levels, int width, Size raster);
    void fill_polygon(std::span<const int> loop, std::uint8_t index, Size raster);
    void draw_segment(Point2 from, Point2 to, std::uint8_t index, int width, Size raster);
    void stamp(int x, int y, int width, std::uint8_t index, Size raster) noexcept;
    void resolve(Image& frame, int scale);
    std::uint8_t coverage_index(std::uint8_t index, int covered, int samples);

    FramePalette palette_;
    std::vector<std::uint8_t> subpixels_;  // one palette index per supersample
    std::vector<Vec3> view_;               // vertices in camera space
    std::vector<Point2> screen_;           // vertices projected to subpixels
    std::vector<Ordered> order_;           // faces or edges, far to near
    std::vector<double> crossings_;        // scanline intersections
    // The blend of one palette entry over the background at one coverage
    // level, indexed [entry * kSupersampleCount + covered]: interned once
    // per frame rather than once per pixel that needs it.
    std::array<std::int16_t, 256 * kSupersampleCount> coverage_cache_{};
};

}  // namespace ckv::spin
