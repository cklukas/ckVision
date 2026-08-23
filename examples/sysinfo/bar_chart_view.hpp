// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A labelled horizontal bar chart, drawn in cells.
//
// Cells, not pixels, on purpose: this is the chart every reader gets,
// whatever their terminal can or cannot decode, and the Sixel rendering
// added in WP-53 is drawn from the same data beside it rather than instead
// of it. Block elements are East-Asian Ambiguous, which ckVision resolves
// as one column by D-019, so a bar is exactly as wide as it says it is.
//
// This is example code, not a ckVision widget: it lives here because a
// client can copy it, and because a chart in include/cvision would be a
// The decision log matter with a catalog, golden and documentation obligation
// this package does not need to take on to make its point.
#pragma once

#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::sysinfo {

// What a bar is a picture of. The distinction is drawn in the ink: a
// measured bar is solid, a published ceiling is shaded, and a reader can
// tell them apart across the room without reading a legend -- which is the
// point, because the two are not the same kind of claim.
enum class BarKind {
    Measured,
    Reference,
};

struct ChartBar {
    // Bars sharing a group are drawn against each other and against
    // nothing else: each group is normalized to its own longest bar and
    // introduced by a heading. Comparing a memory bandwidth with a
    // floating-point rate on one axis would be arithmetic with no meaning,
    // however tidy the picture.
    std::string group;
    std::string label;
    // The number, as text, printed past the end of the bar. Carried rather
    // than derived: only the producer of the value knows what unit it is
    // in and how many digits of it were actually measured.
    std::string value_text;
    double value = 0.0;
    BarKind kind = BarKind::Measured;
    // The row a reader's eye should land on -- this machine's own result
    // among comparisons, the newest run among older ones.
    bool highlighted = false;
};

// The chart as lines of text at `width`, grouped and normalized exactly as
// BarChartView draws it. A free function because two things need it: the
// view, and the fallback a Canvas must paint where the terminal cannot show
// its picture -- and those two showing the same data differently is the one
// outcome a capability fallback must not produce.
std::vector<std::string> chart_rows(const std::vector<ChartBar>& bars, int width);

class BarChartView : public ui::View {
public:
    BarChartView();

    void set_bars(std::vector<ChartBar> bars);
    const std::vector<ChartBar>& bars() const noexcept { return bars_; }

    // A line under the chart naming the scale. A chart of unlabelled
    // numbers is a picture of nothing.
    void set_caption(std::string caption);
    const std::string& caption() const noexcept { return caption_; }

    // The line that says which ink means what. Drawn above the caption,
    // and only while the chart actually carries both kinds of bar.
    void set_legend(std::string legend);
    const std::string& legend() const noexcept { return legend_; }

    // What stands in the chart's place before there is anything to draw.
    void set_placeholder(std::string text);

    // The chart as lines of text at the current width, exactly as draw()
    // paints them. This is what the tests assert against and what the
    // exported report (WP-54) writes out, so the picture in the file and
    // the picture on the screen cannot drift apart.
    std::vector<std::string> text_rows() const;

    void on_attached() override;
    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

    // The narrowest chart that is still a chart: without room for a bar,
    // the rows would be labels and numbers with a gap between them.
    static constexpr int kMinimumBarWidth = 8;

private:
    struct Layout {
        int label_width = 0;
        int bar_width = 0;
        int value_width = 0;
    };

    bool has_groups() const noexcept;
    Layout layout_for(int width) const;
    std::string row_text(const ChartBar& bar, const Layout& layout, double maximum) const;

    std::vector<ChartBar> bars_;
    std::string caption_;
    std::string legend_;
    std::string placeholder_ = "No measurements yet.";
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId highlight_role_ = ui::kInvalidRole;
};

// `value` of `maximum`, as a bar `width` cells wide. A measured bar is
// drawn to the nearest eighth of a cell; a reference bar is shaded and
// drawn in whole cells, because a published ceiling is not accurate to an
// eighth of anything. Exposed for its own test: the rounding at the ends is
// where a bar chart lies, by drawing an empty bar as one cell or a full one
// as short.
std::string bar_text(double value, double maximum, int width, BarKind kind = BarKind::Measured);

}  // namespace ckv::sysinfo
