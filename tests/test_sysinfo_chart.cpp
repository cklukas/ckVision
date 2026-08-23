// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The SysInfo example's bar chart. A chart is a claim about proportion, so
// what is tested here is the arithmetic at its ends: that nothing measured
// draws as nothing, that nothing unmeasured draws as something, and that a
// row is exactly as wide as the space it was given.
#include <string>
#include <vector>

#include "cvision/core/text.hpp"
#include "cvision/testing/cktest.hpp"

#include "bar_chart_view.hpp"

using ckv::sysinfo::bar_text;
using ckv::sysinfo::BarChartView;
using ckv::sysinfo::ChartBar;

namespace {

const std::string kFull = "\xE2\x96\x88";     // U+2588 FULL BLOCK
const std::string kEighth = "\xE2\x96\x8F";   // U+258F LEFT ONE EIGHTH BLOCK
const std::string kHalf = "\xE2\x96\x8C";     // U+258C LEFT HALF BLOCK
const std::string kThreeEighths = "\xE2\x96\x8D";  // U+258D

int count_of(const std::string& haystack, const std::string& needle) {
    int found = 0;
    for (std::size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + needle.size()))
        ++found;
    return found;
}

std::string repeated(const std::string& unit, int count) {
    std::string text;
    for (int index = 0; index < count; ++index) text += unit;
    return text;
}

}  // namespace

// The chart's whole geometry rests on a block element occupying one
// column. They are East-Asian Ambiguous, which D-019 resolves as narrow;
// if that policy ever changed, every bar in this example would be twice as
// long as its number, and this is where that would be said out loud.
CK_TEST(a_block_element_is_one_column_wide) {
    CK_CHECK(ckv::text::text_width(kFull) == 1);
    CK_CHECK(ckv::text::text_width(kEighth) == 1);
    CK_CHECK(ckv::text::text_width(repeated(kFull, 7)) == 7);
}

CK_TEST(a_bar_is_as_long_a_share_of_its_width_as_its_value_is_of_the_maximum) {
    CK_CHECK(bar_text(10.0, 10.0, 20) == repeated(kFull, 20));
    CK_CHECK(bar_text(5.0, 10.0, 20) == repeated(kFull, 10));
    CK_CHECK(bar_text(2.5, 10.0, 20) == repeated(kFull, 5));
    // An eighth of a cell is the resolution, so a fraction of a cell is
    // visible instead of rounded away: an eighth of eight cells is exactly
    // half of the first one.
    CK_CHECK(bar_text(1.0, 8.0, 4) == kHalf);
    CK_CHECK(bar_text(1.0, 10.0, 4) == kThreeEighths);
}

CK_TEST(a_measured_value_too_small_to_draw_still_draws) {
    // Zero is nothing, and looks like nothing.
    CK_CHECK(bar_text(0.0, 10.0, 20).empty());
    // A thousandth is not zero, and a bar that showed it as zero would be
    // saying the kernel did not run.
    CK_CHECK(bar_text(0.001, 10.0, 20) == kEighth);
}

CK_TEST(a_bar_never_runs_past_its_own_axis) {
    CK_CHECK(bar_text(30.0, 10.0, 12) == repeated(kFull, 12));
    CK_CHECK(bar_text(1.0, 0.0, 12).empty());
    CK_CHECK(bar_text(1.0, 10.0, 0).empty());
    CK_CHECK(bar_text(-5.0, 10.0, 12).empty());
}

CK_TEST(every_chart_row_fills_exactly_the_width_it_was_given) {
    BarChartView chart;
    chart.set_bounds(ckv::Rect{0, 0, 44, 8});
    chart.set_bars({ChartBar{"Integer mix", "48.3", 48.3, true},
                    ChartBar{"Floating point", "24.0", 24.0, false},
                    ChartBar{"Memory bandwidth", "20.0", 20.0, false}});
    chart.set_caption("index: 1.0 = this program's own unit");

    const std::vector<std::string> rows = chart.text_rows();
    CK_CHECK(rows.size() == 4);  // three bars and the caption
    for (std::size_t index = 0; index + 1 < rows.size(); ++index)
        CK_CHECK(ckv::text::text_width(rows[index]) == 44);

    // The label starts the row and the number ends it.
    CK_CHECK(rows[0].rfind("Integer mix", 0) == 0);
    CK_CHECK(rows[0].size() >= 4 && rows[0].compare(rows[0].size() - 4, 4, "48.3") == 0);
    CK_CHECK(rows[2].compare(rows[2].size() - 4, 4, "20.0") == 0);

    // The largest value owns the whole bar column; the others are shorter
    // in proportion, and visibly so.
    const int longest = count_of(rows[0], kFull);
    CK_CHECK(longest >= 8);
    CK_CHECK(count_of(rows[1], kFull) < longest);
    // Half the value, about half the bar -- the proportion is the claim.
    // The tolerance is two cells because the whole blocks counted here
    // stop one partial block short of the bar's true end.
    CK_CHECK(count_of(rows[1], kFull) * 2 >= longest - 2);
    CK_CHECK(count_of(rows[1], kFull) * 2 <= longest + 2);
    CK_CHECK(rows[3] == "index: 1.0 = this program's own unit");
}

CK_TEST(an_empty_chart_says_so_instead_of_drawing_an_empty_axis) {
    BarChartView chart;
    chart.set_bounds(ckv::Rect{0, 0, 40, 6});
    chart.set_placeholder("No measurements yet.");
    const std::vector<std::string> rows = chart.text_rows();
    CK_CHECK(rows.size() == 1);
    CK_CHECK(rows[0] == "No measurements yet.");
}

CK_TEST(bars_that_are_all_zero_draw_empty_rather_than_dividing_by_nothing) {
    BarChartView chart;
    chart.set_bounds(ckv::Rect{0, 0, 40, 6});
    chart.set_bars({ChartBar{"one", "0.0", 0.0, false}, ChartBar{"two", "0.0", 0.0, false}});
    const std::vector<std::string> rows = chart.text_rows();
    CK_CHECK(rows.size() == 2);
    for (const std::string& row : rows) {
        CK_CHECK(ckv::text::text_width(row) == 40);
        CK_CHECK(row.find(kFull) == std::string::npos);
    }
}
