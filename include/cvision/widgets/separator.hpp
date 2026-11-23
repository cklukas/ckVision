// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include "cvision/scene/painter.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/orientation.hpp"

namespace ckv::widgets {

// A 1-cell horizontal or vertical hairline rule for Row/Column layouts.
class CVISION_API Separator final : public ui::View {
public:
    // Construct a Separator with the given orientation and line style.
    explicit Separator(Orientation orientation = Orientation::Horizontal,
                       scene::LineStyle style = scene::LineStyle::Single);

    // Return the current orientation (Horizontal or Vertical).
    Orientation orientation() const noexcept { return orientation_; }
    // Change the separator orientation and invalidate layout.
    void set_orientation(Orientation orientation);

    // Resolve the frame theme role on attach.
    void on_attached() override;
    // Draw the 1-cell horizontal or vertical hairline rule.
    void draw(scene::Painter& painter) override;
    // Return horizontal size hint (fixed 1 when Vertical, expanding when Horizontal).
    ui::SizeHint horizontal_size_hint() const override;
    // Return vertical size hint (fixed 1 when Horizontal, expanding when Vertical).
    ui::SizeHint vertical_size_hint() const override;

private:
    Orientation orientation_ = Orientation::Horizontal;
    scene::LineStyle line_style_ = scene::LineStyle::Single;
    ui::RoleId role_ = ui::kInvalidRole;
};

// An invisible flexible filler for Row/Column layouts.
class CVISION_API Spacer final : public ui::View {
public:
    // Construct a Spacer with optional minimum width and height in cells.
    explicit Spacer(int min_width = 0, int min_height = 0);

    // Return expanding horizontal size hint with minimum min_width.
    ui::SizeHint horizontal_size_hint() const override;
    // Return expanding vertical size hint with minimum min_height.
    ui::SizeHint vertical_size_hint() const override;

private:
    int min_width_ = 0;
    int min_height_ = 0;
};

}  // namespace ckv::widgets
