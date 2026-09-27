// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// WP-30: widget clipping never splits a grapheme cluster. Each text-drawing
// widget family draws one mixed-cluster string (tools/docgen/clip_sweep.hpp)
// at every width from 0 until all of it shows. The stacked frames are pinned
// byte for byte in tests/golden/clip_sweep_<family>.dump, and every cell of
// every width is checked here: no cell holds part of a cluster, a wide glyph
// never keeps its lead without its continuation or the other way round, and
// nothing is drawn outside the widget.
#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "clip_sweep.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/core/text.hpp"
#include "cvision/core/utf8.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/testing/cktest.hpp"

using ckv::Cell;
using ckv::Point;
using ckv::Rect;
using ckv::docgen::clip_sweep::kMixedText;
using ckv::docgen::clip_sweep::Sweep;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// `ok`, after saying on stderr what failed when it is not: one CK_CHECK line
// covers many cells, and the report has to name the cell.
bool explained(bool ok, const std::string& what) {
    if (!ok) std::fprintf(stderr, "  %s\n", what.c_str());
    return ok;
}

const std::vector<Sweep>& sweeps() {
    static const std::vector<Sweep> rendered = ckv::docgen::clip_sweep::render_all();
    return rendered;
}

// The mixed text's clusters of more than one code point. Their code points
// appear nowhere else in any family's drawing, so a cell holding one of them
// must hold its whole cluster.
constexpr std::array<std::string_view, 3> kMultiCodePointClusters{
    "e\xCC\x81",
    "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7",
    "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8",
};

bool contains_code_point(std::string_view text, char32_t wanted) {
    for (std::size_t pos = 0; pos < text.size();)
        if (ckv::utf8::decode(text, pos) == wanted) return true;
    return false;
}

// Whether `grapheme` is a piece of a multi-code-point cluster rather than
// the whole of it.
bool is_partial_cluster(std::string_view grapheme) {
    for (const std::string_view cluster : kMultiCodePointClusters) {
        if (grapheme == cluster) return false;
        for (std::size_t pos = 0; pos < cluster.size();)
            if (contains_code_point(grapheme, ckv::utf8::decode(cluster, pos))) return true;
    }
    return false;
}

// Every cell of `sweep` against the three rules, one failure per broken cell.
void expect_no_split_clusters(const Sweep& sweep) {
    const ckv::scene::Surface& surface = sweep.surface;
    for (std::size_t width = 0; width < sweep.widget_bounds.size(); ++width) {
        const Rect widget = sweep.widget_bounds[width];
        for (int y = widget.top(); y < widget.bottom(); ++y) {
            for (int x = 0; x < surface.size().width; ++x) {
                const Cell& cell = surface.at(Point{x, y});
                const bool inside = x >= widget.left() && x < widget.right();
                if (!inside) {
                    CK_CHECK(explained(cell == Cell{}, sweep.family + " width " + std::to_string(width) +
                                                     ": drawn outside the widget at column " + std::to_string(x)));
                    continue;
                }
                const std::string where =
                    sweep.family + " width " + std::to_string(width) + " cell (" + std::to_string(x) + "," +
                    std::to_string(y - widget.top()) + ")";
                CK_CHECK(explained(!is_partial_cluster(cell.grapheme()), where + " holds part of a cluster"));
                if (cell.width() == 2) {
                    CK_CHECK(explained(x + 1 < widget.right() && surface.at(Point{x + 1, y}).is_continuation(),
                                 where + " is a wide glyph without its continuation"));
                }
                if (cell.is_continuation()) {
                    CK_CHECK(explained(x > widget.left() && surface.at(Point{x - 1, y}).width() == 2,
                                 where + " is a continuation without its wide glyph"));
                }
            }
        }
    }
}

}  // namespace

CK_TEST(clip_sweep_text_mixes_every_cluster_kind) {
    CK_CHECK(ckv::text::split_graphemes(kMixedText).size() == 6);
    CK_CHECK(ckv::text::text_width(kMixedText) == 9);
}

CK_TEST(clip_sweeps_cover_every_text_drawing_widget_family) {
    std::vector<std::string> names;
    for (const Sweep& sweep : sweeps()) names.push_back(sweep.family);
    CK_CHECK((names == std::vector<std::string>{"label", "static_text", "button", "input_line", "list_view",
                                                "table", "menu", "status_line", "text_view", "tab_control"}));
}

CK_TEST(clip_sweeps_match_their_pinned_goldens) {
    for (const Sweep& sweep : sweeps()) {
        const std::string path = "golden/clip_sweep_" + sweep.family + ".dump";
        const std::string expected = read_file(path);
        CK_CHECK(explained(!expected.empty(), path + " is missing"));
        const std::string actual = ckv::golden::serialize(ckv::scene::capture(sweep.surface));
        CK_CHECK(explained(actual == expected, path + " differs from the rendered sweep"));
    }
}

CK_TEST(clip_sweeps_reach_the_width_that_shows_the_whole_text) {
    // The widest band of every family shows all six clusters, so the sweep
    // ends where clipping stops rather than short of it.
    for (const Sweep& sweep : sweeps()) {
        const Rect widest = sweep.widget_bounds.back();
        std::string shown;
        for (int y = widest.top(); y < widest.bottom(); ++y)
            for (int x = widest.left(); x < widest.right(); ++x) shown += sweep.surface.at(Point{x, y}).grapheme();
        for (const std::string_view cluster : ckv::text::split_graphemes(kMixedText))
            CK_CHECK(explained(shown.find(cluster) != std::string::npos,
                         sweep.family + ": the widest band does not show every cluster"));
    }
}

CK_TEST(no_width_of_any_family_splits_a_cluster_or_a_wide_glyph) {
    for (const Sweep& sweep : sweeps()) expect_no_split_clusters(sweep);
}
