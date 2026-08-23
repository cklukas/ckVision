// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

namespace ckv {

// Cell-space geometry. Distinct from PixelPoint/PixelSize below so the
// two coordinate spaces never mix silently (the decision log D-018).
struct Point {
    int x = 0;
    int y = 0;

    friend bool operator==(const Point&, const Point&) = default;
};

struct Size {
    int width = 0;
    int height = 0;

    friend bool operator==(const Size&, const Size&) = default;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    friend bool operator==(const Rect&, const Rect&) = default;

    int left() const noexcept { return x; }
    int top() const noexcept { return y; }
    int right() const noexcept { return x + width; }
    int bottom() const noexcept { return y + height; }
    bool empty() const noexcept { return width <= 0 || height <= 0; }

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
    int x = 0;
    int y = 0;

    friend bool operator==(const PixelPoint&, const PixelPoint&) = default;
};

struct PixelSize {
    int width = 0;
    int height = 0;

    friend bool operator==(const PixelSize&, const PixelSize&) = default;
};

}  // namespace ckv
