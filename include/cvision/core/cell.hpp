// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <string>
#include <string_view>

#include "cvision/core/style.hpp"

namespace ckv {

// One grapheme cluster, its style, and its precomputed column width —
// the scene layer's atomic drawing unit (ARCHITECTURE.md §2).
class Cell {
public:
    Cell() : grapheme_(" "), style_(), width_(1) {}

    // `grapheme` need not already be sanitized or a single verified
    // cluster: this factory sanitizes it (DECISIONS.md D-023 — a Cell
    // can never carry a raw control character regardless of caller
    // discipline) and keeps only the first resulting grapheme cluster,
    // since a Cell holds exactly one.
    static Cell from_grapheme(std::string_view grapheme, Style style);

    std::string_view grapheme() const noexcept { return grapheme_; }
    const Style& style() const noexcept { return style_; }
    void set_style(Style style) noexcept { style_ = style; }
    int width() const noexcept { return width_; }

    friend bool operator==(const Cell&, const Cell&) = default;

private:
    Cell(std::string grapheme, Style style, int width)
        : grapheme_(std::move(grapheme)), style_(style), width_(width) {}

    std::string grapheme_;
    Style style_;
    int width_;
};

}  // namespace ckv
