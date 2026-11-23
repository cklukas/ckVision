// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/separator.hpp"

#include <algorithm>

#include "cvision/ui/layout.hpp"

namespace ckv::widgets {

Separator::Separator(Orientation orientation, scene::LineStyle style)
    : orientation_(orientation), line_style_(style) {}

void Separator::set_orientation(Orientation orientation) {
    if (orientation_ == orientation) return;
    orientation_ = orientation;
    size_hint_changed();
    invalidate();
}

void Separator::on_attached() { role_ = context().roles->find("ckv.window.frame.active"); }

void Separator::draw(scene::Painter& painter) {
    const int w = bounds().width;
    const int h = bounds().height;
    if (w <= 0 || h <= 0) return;
    const Style style = context().theme->resolve(role_);
    if (orientation_ == Orientation::Horizontal) {
        painter.hline(Point{0, h / 2}, w, line_style_, style);
    } else {
        painter.vline(Point{w / 2, 0}, h, line_style_, style);
    }
}

ui::SizeHint Separator::horizontal_size_hint() const {
    if (orientation_ == Orientation::Horizontal) return ui::SizeHint{1, 1, ui::kUnboundedExtent};
    return ui::SizeHint{1, 1, 1};
}

ui::SizeHint Separator::vertical_size_hint() const {
    if (orientation_ == Orientation::Horizontal) return ui::SizeHint{1, 1, 1};
    return ui::SizeHint{1, 1, ui::kUnboundedExtent};
}

Spacer::Spacer(int min_width, int min_height)
    : min_width_(std::max(0, min_width)), min_height_(std::max(0, min_height)) {}

ui::SizeHint Spacer::horizontal_size_hint() const {
    return ui::SizeHint{min_width_, min_width_, ui::kUnboundedExtent};
}

ui::SizeHint Spacer::vertical_size_hint() const {
    return ui::SizeHint{min_height_, min_height_, ui::kUnboundedExtent};
}

}  // namespace ckv::widgets
