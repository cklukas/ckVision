// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

namespace ckv {

// Cell-space geometry. Distinct from PixelPoint/PixelSize below so the
// two coordinate spaces never mix silently (DECISIONS.md D-018).
struct Point {
    // Column (x) and row (y), in cells, relative to whatever origin the API
    // taking the point names. Signed: a position left of or above that
    // origin is legitimate, for example a window dragged partly off screen.
    int x = 0;
    int y = 0;

    // Memberwise equality.
    friend bool operator==(const Point&, const Point&) = default;
};

// A cell-space extent: columns by rows.
struct Size {
    // Columns (width) and rows (height). The type enforces no sign; storage
    // sized from it, such as scene::Surface, asserts that neither is negative.
    int width = 0;
    int height = 0;

    // Memberwise equality.
    friend bool operator==(const Size&, const Size&) = default;
};

// A cell-space rectangle: top-left corner plus extent, covering the columns
// [x, x + width) and the rows [y, y + height).
struct Rect {
    // The top-left cell and the extent in cells. A zero or negative extent
    // is allowed and makes the rectangle empty.
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    // Memberwise equality: two empty rectangles at different positions or
    // with different negative extents compare unequal.
    friend bool operator==(const Rect&, const Rect&) = default;

    // The edges as half-open bounds: left() and top() are the first covered
    // column and row, right() and bottom() the first ones past the rectangle.
    int left() const noexcept { return x; }
    int top() const noexcept { return y; }
    int right() const noexcept { return x + width; }
    int bottom() const noexcept { return y + height; }
    // True when the rectangle covers no cell (either extent is <= 0).
    bool empty() const noexcept { return width <= 0 || height <= 0; }

    // Whether `p` lies inside the half-open bounds; always false for an empty
    // rectangle.
    bool contains(Point p) const noexcept {
        return !empty() && p.x >= left() && p.x < right() && p.y >= top() && p.y < bottom();
    }

    // Intersection; empty (width/height <= 0) when the rects do not overlap.
    Rect intersected(const Rect& other) const noexcept {
        const int ix = x > other.x ? x : other.x;
        const int iy = y > other.y ? y : other.y;
        const int ir = right() < other.right() ? right() : other.right();
        const int ib = bottom() < other.bottom() ? bottom() : other.bottom();
        return Rect{ix, iy, ir - ix, ib - iy};
    }
};

// Pixel-space geometry (image content, dual-space mouse coordinates —
// D-018). Never implicitly convertible to/from cell-space types: a
// conversion always requires the terminal's reported cell pixel metrics
// (term-layer capability), which core does not have.
struct PixelPoint {
    // Horizontal (x) and vertical (y) position in pixels from the origin the
    // producing API names; for a mouse report, the text area's top-left.
    int x = 0;
    int y = 0;

    // Memberwise equality.
    friend bool operator==(const PixelPoint&, const PixelPoint&) = default;
};

// A pixel-space extent. No library interface takes or returns one at present:
// the terminal's cell and text-area pixel metrics are reported as Size (see
// term::Capabilities).
struct PixelSize {
    // Width and height in pixels.
    int width = 0;
    int height = 0;

    // Memberwise equality.
    friend bool operator==(const PixelSize&, const PixelSize&) = default;
};

}  // namespace ckv
