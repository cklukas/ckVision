// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Hyperlinks in the scene layer (D-088): Painter writes a target into the
// cells of a text run, a Surface keeps exactly the targets its cells use,
// the Compositor carries links into the frame by target through occlusion,
// shadows and moves, and a golden capture records them as link runs.
#include "cvision/core/golden.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"

#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"

using namespace ckv;
using ckv::scene::Compositor;
using ckv::scene::Layer;
using ckv::scene::Painter;
using ckv::scene::ShadowSpec;
using ckv::scene::Surface;

namespace {

constexpr std::string_view kDocs = "https://example.test/docs";
constexpr std::string_view kHome = "https://example.test/";

// The row's link targets, one character per cell: the first letter after
// "https://example.test/" of each target ('/' for kHome itself), '.' for none.
std::string link_row(const Surface& surface, int y) {
    std::string out;
    for (int x = 0; x < surface.size().width; ++x) {
        const std::string_view target = surface.link_target(Point{x, y});
        if (target.empty()) out += '.';
        else if (target == kHome) out += '/';
        else out += target[kHome.size()];
    }
    return out;
}

}  // namespace

// --- Painter and Surface ------------------------------------------------------

CK_TEST(draw_text_with_a_target_links_every_cell_it_writes_and_nothing_else) {
    Surface surface(Size{10, 1});
    Painter painter(surface, Rect{0, 0, 10, 1});
    painter.draw_text(Point{0, 0}, "go ", Style{});
    painter.draw_text(Point{3, 0}, "docs", Style{}, kDocs);
    CK_CHECK(link_row(surface, 0) == "...dddd...");
    CK_CHECK(surface.links().size() == 1);
    CK_CHECK(surface.at(Point{3, 0}).grapheme() == "d");
}

CK_TEST(a_wide_glyphs_continuation_column_belongs_to_the_same_link) {
    Surface surface(Size{4, 1});
    Painter painter(surface, Rect{0, 0, 4, 1});
    painter.draw_text(Point{0, 0}, "\xE6\x97\xA5x", Style{}, kDocs);  // U+65E5, two columns
    CK_CHECK(surface.at(Point{1, 0}).is_continuation());
    CK_CHECK(link_row(surface, 0) == "ddd.");
}

CK_TEST(an_invalid_target_draws_the_text_without_a_link) {
    Surface surface(Size{8, 1});
    Painter painter(surface, Rect{0, 0, 8, 1});
    painter.draw_text(Point{0, 0}, "evil", Style{}, "https://x.test/\x1B]8;;y\x07");
    painter.draw_text(Point{4, 0}, "rel", Style{}, "guide.md");
    CK_CHECK(link_row(surface, 0) == "........");
    CK_CHECK(surface.links().empty());
}

CK_TEST(a_surface_forgets_a_target_once_no_cell_uses_it) {
    Surface surface(Size{6, 1});
    Painter painter(surface, Rect{0, 0, 6, 1});
    painter.draw_text(Point{0, 0}, "abc", Style{}, kDocs);
    painter.draw_text(Point{3, 0}, "def", Style{}, kHome);
    CK_CHECK(surface.links().size() == 2);
    painter.draw_text(Point{0, 0}, "xyz", Style{});
    CK_CHECK(surface.links().size() == 1);
    CK_CHECK(link_row(surface, 0) == "...///");
}

CK_TEST(a_clipped_away_run_leaves_no_entry_behind) {
    Surface surface(Size{4, 2});
    Painter painter(surface, Rect{0, 0, 4, 1});
    painter.draw_text(Point{0, 1}, "gone", Style{}, kDocs);  // row outside the clip
    painter.draw_text(Point{5, 0}, "gone", Style{}, kDocs);  // columns outside the clip
    CK_CHECK(surface.links().empty());
}

CK_TEST(a_style_pass_restyles_a_linked_cell_and_keeps_its_link) {
    Surface surface(Size{4, 1});
    Painter painter(surface, Rect{0, 0, 4, 1});
    painter.draw_text(Point{0, 0}, "link", Style{}, kDocs);
    painter.apply_shadow(Rect{0, 0, 2, 1}, ShadowStyle::recolor(Color::rgb(85, 85, 85), Color::rgb(0, 0, 0)));
    painter.transform_style(Rect{2, 0, 2, 1}, [](Style style) noexcept { return ShadowStyle::halve().apply(style); });
    CK_CHECK(link_row(surface, 0) == "dddd");
    CK_CHECK(surface.links().size() == 1);
    CK_CHECK(!(surface.at(Point{0, 0}).style() == Style{}));
}

CK_TEST(set_cell_takes_a_link_by_target_and_resize_forgets_every_link) {
    Surface source(Size{2, 1});
    Painter(source, Rect{0, 0, 2, 1}).draw_text(Point{0, 0}, "ab", Style{}, kDocs);
    Surface copy(Size{2, 1});
    for (int x = 0; x < 2; ++x)
        copy.set_cell(Point{x, 0}, source.at(Point{x, 0}), source.link_target(Point{x, 0}));
    CK_CHECK(link_row(copy, 0) == "dd");
    copy.set_cell(Point{0, 0}, Cell::from_grapheme("z", Style{}), "");
    CK_CHECK(link_row(copy, 0) == ".d");
    copy.resize(Size{3, 1});
    CK_CHECK(copy.links().empty());
    CK_CHECK(copy.has_damage());
}

// --- Compositor ------------------------------------------------------------------

CK_TEST(the_compositor_carries_a_layers_links_into_the_frame_by_target) {
    Compositor compositor(Size{8, 2});
    Surface background(Size{8, 2});
    Painter(background, Rect{0, 0, 8, 2}).draw_text(Point{0, 1}, "homehome", Style{}, kHome);
    Surface window(Size{4, 1});
    Painter(window, Rect{0, 0, 4, 1}).draw_text(Point{0, 0}, "docs", Style{}, kDocs);
    const std::vector<Layer> layers{{1, &window, Point{2, 1}, false}};
    compositor.compose(layers, background);

    // The window occludes the middle of the background's link.
    CK_CHECK(link_row(compositor.frame(), 0) == "........");
    CK_CHECK(link_row(compositor.frame(), 1) == "//dddd//");
    CK_CHECK(compositor.frame().links().size() == 2);
}

CK_TEST(a_shadow_dims_a_linked_cell_beneath_it_without_unlinking_it) {
    Compositor compositor(Size{6, 3});
    Surface background(Size{6, 3});
    Painter(background, Rect{0, 0, 6, 3}).draw_text(Point{0, 2}, "linked", Style{}, kDocs);
    Surface window(Size{3, 2}, Cell::from_grapheme("W", Style{}));
    const std::vector<Layer> layers{{1, &window, Point{0, 0}, true}};
    compositor.compose(layers, background, ShadowSpec{});

    CK_CHECK(link_row(compositor.frame(), 2) == "dddddd");
    // The default shadow falls one row below the window, from column 2.
    CK_CHECK(!(compositor.frame().at(Point{2, 2}).style() == background.at(Point{2, 2}).style()));
    CK_CHECK(compositor.frame().at(Point{0, 2}).style() == background.at(Point{0, 2}).style());
}

CK_TEST(moving_a_window_away_restores_the_links_it_covered_and_frees_its_own) {
    Compositor compositor(Size{6, 1});
    Surface background(Size{6, 1});
    Painter(background, Rect{0, 0, 6, 1}).draw_text(Point{0, 0}, "home..", Style{}, kHome);
    Surface window(Size{2, 1});
    Painter(window, Rect{0, 0, 2, 1}).draw_text(Point{0, 0}, "dd", Style{}, kDocs);
    compositor.compose({{1, &window, Point{0, 0}, false}}, background);
    CK_CHECK(link_row(compositor.frame(), 0) == "dd////");

    compositor.compose({{1, &window, Point{4, 0}, false}}, background);
    CK_CHECK(link_row(compositor.frame(), 0) == "////dd");

    compositor.compose({}, background);
    CK_CHECK(link_row(compositor.frame(), 0) == "//////");
    CK_CHECK(compositor.frame().links().size() == 1);
}

// --- Golden capture ------------------------------------------------------------

CK_TEST(a_capture_records_maximal_link_runs_in_row_major_order) {
    Surface surface(Size{8, 2});
    Painter painter(surface, Rect{0, 0, 8, 2});
    painter.draw_text(Point{0, 0}, "ab", Style{}, kDocs);
    painter.draw_text(Point{2, 0}, "cd", Style{}, kDocs);  // a second paint, one run
    painter.draw_text(Point{4, 0}, "ef", Style{}, kHome);
    painter.draw_text(Point{1, 1}, "wrapped", Style{}, kDocs);
    const golden::Document doc = scene::capture(surface);

    CK_CHECK(doc.links.size() == 3);
    CK_CHECK(doc.links[0].col == 0 && doc.links[0].row == 0 && doc.links[0].cols == 4);
    CK_CHECK(doc.links[0].target == kDocs);
    CK_CHECK(doc.links[1].col == 4 && doc.links[1].cols == 2 && doc.links[1].target == kHome);
    CK_CHECK(doc.links[2].col == 1 && doc.links[2].row == 1 && doc.links[2].cols == 7);
    // What is captured parses back, so the record is canonical.
    const std::string text = golden::serialize(doc);
    const golden::ParseResult parsed = golden::parse(text);
    CK_CHECK(static_cast<bool>(parsed));
    if (parsed) CK_CHECK(golden::serialize(*parsed.document) == text);
}

CK_TEST(a_capture_of_an_unlinked_surface_has_no_link_records) {
    Surface surface(Size{3, 1}, Cell::from_grapheme("x", Style{}));
    const std::string text = golden::serialize(scene::capture(surface));
    CK_CHECK(text.find("\nlink ") == std::string::npos);
}
