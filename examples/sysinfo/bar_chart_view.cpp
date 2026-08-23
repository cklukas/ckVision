// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "bar_chart_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"

namespace ckv::sysinfo {
namespace {

// The eighths of a cell, so a short bar is still visibly a bar and a
// difference of a few percent is visible at all.
constexpr std::string_view kFullBlock = "\xE2\x96\x88";  // U+2588
constexpr std::string_view kPartialBlocks[] = {
    "",                  // 0/8
    "\xE2\x96\x8F",      // U+258F LEFT ONE EIGHTH BLOCK
    "\xE2\x96\x8E",      // U+258E
    "\xE2\x96\x8D",      // U+258D
    "\xE2\x96\x8C",      // U+258C LEFT HALF BLOCK
    "\xE2\x96\x8B",      // U+258B
    "\xE2\x96\x8A",      // U+258A
    "\xE2\x96\x89",      // U+2589
};

constexpr int kLabelColumnMinimum = 8;
constexpr int kLabelColumnMaximum = 22;

std::string padded_right(const std::string& text, int width) {
    const std::string clipped = text::clip_to_width(text, width);
    const int used = text::text_width(clipped);
    return clipped + std::string(static_cast<std::size_t>(std::max(0, width - used)), ' ');
}

std::string padded_left(const std::string& text, int width) {
    const std::string clipped = text::clip_to_width(text, width);
    const int used = text::text_width(clipped);
    return std::string(static_cast<std::size_t>(std::max(0, width - used)), ' ') + clipped;
}

}  // namespace

std::string bar_text(double value, double maximum, int width) {
    if (width <= 0 || !std::isfinite(value) || !std::isfinite(maximum) || maximum <= 0.0 || value <= 0.0)
        return std::string();
    // Clamped rather than scaled: a value past the maximum is a caller's
    // mistake about its own scale, and a bar that ran past its axis would
    // hide it.
    const double share = std::min(1.0, value / maximum);
    const int eighths = static_cast<int>(std::llround(share * static_cast<double>(width) * 8.0));
    // A measured value that rounds to nothing still gets its eighth: a bar
    // of zero cells says "not measured", which is a different claim.
    if (eighths <= 0) return std::string(kPartialBlocks[1]);

    const int full = std::min(width, eighths / 8);
    const int remainder = full >= width ? 0 : eighths % 8;
    std::string bar;
    for (int cell = 0; cell < full; ++cell) bar += kFullBlock;
    bar += kPartialBlocks[remainder];
    return bar;
}

BarChartView::BarChartView() { set_preferred_size(Size{40, 6}); }

void BarChartView::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.list.normal");
    if (highlight_role_ == ui::kInvalidRole) highlight_role_ = context().roles->find("ckv.list.selected");
}

void BarChartView::set_bars(std::vector<ChartBar> bars) {
    bars_ = std::move(bars);
    invalidate();
    size_hint_changed();
}

void BarChartView::set_caption(std::string caption) {
    if (caption_ == caption) return;
    caption_ = std::move(caption);
    invalidate();
}

void BarChartView::set_placeholder(std::string text) {
    placeholder_ = std::move(text);
    invalidate();
}

BarChartView::Layout BarChartView::layout_for(int width) const {
    Layout layout;
    for (const ChartBar& bar : bars_) {
        layout.label_width = std::max(layout.label_width, text::text_width(bar.label));
        layout.value_width = std::max(layout.value_width, text::text_width(bar.value_text));
    }
    layout.label_width = std::clamp(layout.label_width, kLabelColumnMinimum, kLabelColumnMaximum);
    // Two single spaces separate the three columns.
    layout.bar_width = width - layout.label_width - layout.value_width - 2;
    if (layout.bar_width < kMinimumBarWidth) {
        // The label is what gives way: a chart with no bar is not one, and
        // a truncated label is still a label.
        const int shortfall = kMinimumBarWidth - layout.bar_width;
        layout.label_width = std::max(kLabelColumnMinimum, layout.label_width - shortfall);
        layout.bar_width = width - layout.label_width - layout.value_width - 2;
    }
    return layout;
}

std::string BarChartView::row_text(const ChartBar& bar, const Layout& layout, double maximum) const {
    const std::string drawn = bar_text(bar.value, maximum, layout.bar_width);
    const int used = text::text_width(drawn);
    return padded_right(bar.label, layout.label_width) + " " + drawn +
           std::string(static_cast<std::size_t>(std::max(0, layout.bar_width - used)), ' ') + " " +
           padded_left(bar.value_text, layout.value_width);
}

std::vector<std::string> BarChartView::text_rows() const {
    const int width = bounds().width;
    std::vector<std::string> rows;
    if (width <= 0) return rows;
    if (bars_.empty()) {
        rows.push_back(text::clip_to_width(placeholder_, width));
        return rows;
    }

    double maximum = 0.0;
    for (const ChartBar& bar : bars_)
        if (std::isfinite(bar.value)) maximum = std::max(maximum, bar.value);
    // Every bar at zero would divide by nothing; they are all drawn empty,
    // which is exactly what they measured.
    if (maximum <= 0.0) maximum = 1.0;

    const Layout layout = layout_for(width);
    rows.reserve(bars_.size() + 1);
    for (const ChartBar& bar : bars_) rows.push_back(row_text(bar, layout, maximum));
    if (!caption_.empty()) rows.push_back(text::clip_to_width(caption_, width));
    return rows;
}

void BarChartView::draw(scene::Painter& painter) {
    const int width = bounds().width;
    const int height = bounds().height;
    if (width <= 0 || height <= 0) return;

    const Style normal = context().theme->resolve(normal_role_);
    const Style highlight = context().theme->resolve(highlight_role_);
    painter.fill(Rect{0, 0, width, height}, Cell::from_grapheme(" ", normal));

    const std::vector<std::string> rows = text_rows();
    for (int row = 0; row < height && static_cast<std::size_t>(row) < rows.size(); ++row) {
        const bool is_bar = static_cast<std::size_t>(row) < bars_.size();
        const Style style = is_bar && bars_[static_cast<std::size_t>(row)].highlighted ? highlight : normal;
        painter.draw_text(Point{0, row}, rows[static_cast<std::size_t>(row)], style);
    }
}

ui::SizeHint BarChartView::horizontal_size_hint() const {
    const Layout layout = layout_for(kLabelColumnMaximum + kMinimumBarWidth + 8);
    const int minimum = kLabelColumnMinimum + kMinimumBarWidth + layout.value_width + 2;
    return ui::SizeHint{minimum, std::max(minimum, 48), ui::kUnboundedExtent};
}

ui::SizeHint BarChartView::vertical_size_hint() const {
    const int rows = static_cast<int>(bars_.size()) + (caption_.empty() ? 0 : 1);
    const int wanted = std::max(1, rows);
    return ui::SizeHint{1, wanted, ui::kUnboundedExtent};
}

}  // namespace ckv::sysinfo
