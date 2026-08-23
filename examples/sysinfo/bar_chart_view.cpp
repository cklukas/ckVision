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

// A published ceiling, in ink nobody mistakes for a measurement.
constexpr std::string_view kShadeBlock = "\xE2\x96\x92";  // U+2592 MEDIUM SHADE

constexpr int kLabelColumnMinimum = 8;
constexpr int kLabelColumnMaximum = 24;

// Grouped rows are indented under their heading.
constexpr std::string_view kGroupIndent = "  ";

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

std::string bar_text(double value, double maximum, int width, BarKind kind) {
    if (width <= 0 || !std::isfinite(value) || !std::isfinite(maximum) || maximum <= 0.0 || value <= 0.0)
        return std::string();
    // Clamped rather than scaled: a value past the maximum is a caller's
    // mistake about its own scale, and a bar that ran past its axis would
    // hide it.
    const double share = std::min(1.0, value / maximum);
    if (kind == BarKind::Reference) {
        // Whole cells only: a figure computed from a standard is exact to
        // the standard and to nothing finer, and eighths of a cell would
        // dress it up as a measurement.
        const int cells = std::max(1, static_cast<int>(std::llround(share * static_cast<double>(width))));
        std::string bar;
        for (int cell = 0; cell < std::min(width, cells); ++cell) bar += kShadeBlock;
        return bar;
    }

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

void BarChartView::set_legend(std::string legend) {
    if (legend_ == legend) return;
    legend_ = std::move(legend);
    invalidate();
}

void BarChartView::set_placeholder(std::string text) {
    placeholder_ = std::move(text);
    invalidate();
}

BarChartView::Layout BarChartView::layout_for(int width) const {
    Layout layout;
    const int indent = has_groups() ? text::text_width(std::string(kGroupIndent)) : 0;
    for (const ChartBar& bar : bars_) {
        layout.label_width = std::max(layout.label_width, text::text_width(bar.label) + indent);
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
    const std::string drawn = bar_text(bar.value, maximum, layout.bar_width, bar.kind);
    const int used = text::text_width(drawn);
    const std::string label = has_groups() ? std::string(kGroupIndent) + bar.label : bar.label;
    return padded_right(label, layout.label_width) + " " + drawn +
           std::string(static_cast<std::size_t>(std::max(0, layout.bar_width - used)), ' ') + " " +
           padded_left(bar.value_text, layout.value_width);
}

std::vector<std::string> chart_rows(const std::vector<ChartBar>& bars, int width) {
    BarChartView chart;
    chart.set_bounds(Rect{0, 0, width, 1});
    chart.set_bars(bars);
    return chart.text_rows();
}

std::vector<std::string> BarChartView::text_rows() const {
    const int width = bounds().width;
    std::vector<std::string> rows;
    if (width <= 0) return rows;
    if (bars_.empty()) {
        rows.push_back(text::clip_to_width(placeholder_, width));
        return rows;
    }

    const Layout layout = layout_for(width);
    rows.reserve(bars_.size() + 3);
    std::size_t index = 0;
    while (index < bars_.size()) {
        // One group at a time, each normalized to its own longest bar. A
        // group's heading is emitted from the group name rather than by the
        // caller, so a chart cannot show a heading over the wrong bars.
        const std::string& group = bars_[index].group;
        std::size_t end = index;
        double maximum = 0.0;
        while (end < bars_.size() && bars_[end].group == group) {
            if (std::isfinite(bars_[end].value)) maximum = std::max(maximum, bars_[end].value);
            ++end;
        }
        // Every bar in the group at zero would divide by nothing; they are
        // all drawn empty, which is exactly what they measured.
        if (maximum <= 0.0) maximum = 1.0;
        if (!group.empty()) rows.push_back(text::clip_to_width(group, width));
        for (std::size_t row = index; row < end; ++row) rows.push_back(row_text(bars_[row], layout, maximum));
        index = end;
    }
    if (!legend_.empty()) rows.push_back(text::clip_to_width(legend_, width));
    if (!caption_.empty()) rows.push_back(text::clip_to_width(caption_, width));
    return rows;
}

bool BarChartView::has_groups() const noexcept {
    for (const ChartBar& bar : bars_)
        if (!bar.group.empty()) return true;
    return false;
}

void BarChartView::draw(scene::Painter& painter) {
    const int width = bounds().width;
    const int height = bounds().height;
    if (width <= 0 || height <= 0) return;

    const Style normal = context().theme->resolve(normal_role_);
    const Style highlight = context().theme->resolve(highlight_role_);
    painter.fill(Rect{0, 0, width, height}, Cell::from_grapheme(" ", normal));

    // Which rows are highlighted is decided from the bars, not from the row
    // index: the group headings between them mean the two no longer line up.
    std::vector<bool> row_highlighted;
    row_highlighted.reserve(bars_.size() + 3);
    std::size_t index = 0;
    while (index < bars_.size()) {
        const std::string& group = bars_[index].group;
        if (!group.empty()) row_highlighted.push_back(false);
        while (index < bars_.size() && bars_[index].group == group) {
            row_highlighted.push_back(bars_[index].highlighted);
            ++index;
        }
    }

    const std::vector<std::string> rows = text_rows();
    for (int row = 0; row < height && static_cast<std::size_t>(row) < rows.size(); ++row) {
        const std::size_t at = static_cast<std::size_t>(row);
        // A chart with more rows than box says how many it could not draw.
        // Silently stopping at the bottom edge is how a reader comes to
        // believe they have seen the whole comparison.
        if (row == height - 1 && rows.size() > static_cast<std::size_t>(height)) {
            const std::size_t hidden = rows.size() - static_cast<std::size_t>(height) + 1;
            painter.draw_text(Point{0, row},
                              text::clip_to_width("... " + std::to_string(hidden) + " more rows - resize or zoom",
                                                  width),
                              normal);
            break;
        }
        const Style style = at < row_highlighted.size() && row_highlighted[at] ? highlight : normal;
        painter.draw_text(Point{0, row}, rows[at], style);
    }
}

ui::SizeHint BarChartView::horizontal_size_hint() const {
    const Layout layout = layout_for(kLabelColumnMaximum + kMinimumBarWidth + 8);
    const int minimum = kLabelColumnMinimum + kMinimumBarWidth + layout.value_width + 2;
    return ui::SizeHint{minimum, std::max(minimum, 48), ui::kUnboundedExtent};
}

ui::SizeHint BarChartView::vertical_size_hint() const {
    // Counted from the bars rather than from text_rows(), which needs a
    // width -- and a size hint asked for before the first layout has none.
    int rows = static_cast<int>(bars_.size());
    std::string previous;
    bool first = true;
    for (const ChartBar& bar : bars_) {
        if (!bar.group.empty() && (first || bar.group != previous)) ++rows;
        previous = bar.group;
        first = false;
    }
    if (!legend_.empty()) ++rows;
    if (!caption_.empty()) ++rows;
    return ui::SizeHint{1, std::max(1, rows), ui::kUnboundedExtent};
}

}  // namespace ckv::sysinfo
