// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Golden suites for the M2 exit criteria (the roadmap): clipping/text
// layout, automatic junction merging, shadow occlusion, and raster
// anchoring/occlusion-slicing/fallback-activation, via the symbolic
// golden dump representation (the decision log D-014).
#include "cvision/scene/golden_capture.hpp"

#include <fstream>
#include <sstream>

#include "cvision/scene/compositor.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/testing/cktest.hpp"

using namespace ckv;
using namespace ckv::scene;

namespace {

std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void expect_round_trip(const golden::Document& doc, const char* fixture_path) {
    const std::string expected = read_file(fixture_path);
    CK_CHECK(!expected.empty());
    const std::string actual = golden::serialize(doc);
    CK_CHECK(actual == expected);

    const golden::ParseResult reparsed = golden::parse(actual);
    CK_CHECK(static_cast<bool>(reparsed));
    if (reparsed) CK_CHECK(golden::serialize(*reparsed.document) == actual);
}

}  // namespace

// --- Inline capture correctness -------------------------------------------

CK_TEST(capture_deduplicates_styles_and_orders_them_by_first_appearance) {
    Surface s(Size{3, 1});
    Painter p(s, Rect{0, 0, 3, 1});
    const Style red{Color::rgb(255, 0, 0), Color{}, Attr{}};
    const Style blue{Color::rgb(0, 0, 255), Color{}, Attr{}};
    p.draw_text(Point{0, 0}, "a", red);
    p.draw_text(Point{1, 0}, "b", blue);
    p.draw_text(Point{2, 0}, "c", red);  // reuses the first style

    const golden::Document doc = capture(s);
    CK_CHECK(doc.styles.size() == 2);
    CK_CHECK(doc.stylemap[0] == "010");  // a:0(red) b:1(blue) c:0(red)
}

CK_TEST(capture_reflects_cursor_state) {
    Surface s(Size{2, 2});
    const golden::Document hidden = capture(s, CursorState{});
    CK_CHECK(!hidden.cursor.visible);

    const golden::Document visible =
        capture(s, CursorState{true, Point{1, 0}, CursorShape::Underline});
    CK_CHECK(visible.cursor.visible);
    CK_CHECK(visible.cursor.col == 1);
    CK_CHECK(visible.cursor.row == 0);
    CK_CHECK(visible.cursor.shape == "underline");
}

CK_TEST(capture_records_a_surfaces_own_unsliced_raster_region) {
    Surface s(Size{5, 5});
    Painter p(s, Rect{0, 0, 5, 5});
    const auto image = std::make_shared<Image>(8, 8);
    p.draw_image(Rect{1, 1, 2, 2}, 42, image, [](Painter& fp) {
        fp.fill(Rect{0, 0, 100, 100}, Cell::from_grapheme("#", Style{}));
    });

    const golden::Document doc = capture(s);
    CK_CHECK(doc.rasters.size() == 1);
    CK_CHECK(doc.rasters[0].id == 42);  // capture() keeps the SOURCE id (unsliced)
    CK_CHECK(doc.rasters[0].anchor_col == 1 && doc.rasters[0].anchor_row == 1);
    CK_CHECK(doc.rasters[0].span_cols == 2 && doc.rasters[0].span_rows == 2);
    CK_CHECK(doc.rasters[0].fallback_active);
}

CK_TEST(image_content_hash_is_deterministic_and_content_sensitive) {
    Image a(4, 4);
    Image b(4, 4);
    CK_CHECK(image_content_hash(a) == image_content_hash(b));
    b.set_pixel(0, 0, Image::Rgba{1, 2, 3, 255});
    CK_CHECK(image_content_hash(a) != image_content_hash(b));
}

// --- Golden fixture suites --------------------------------------------------

CK_TEST(golden_box_and_junctions_clipping_and_junction_merging) {
    Surface s(Size{12, 6});
    Painter p(s, Rect{0, 0, 12, 6});
    const Style normal{Color::default_color(), Color::default_color(), Attr{}};
    const Style bold{Color::rgb(255, 255, 255), Color::default_color(), Attr::Bold};
    p.fill(Rect{0, 0, 12, 6}, Cell::from_grapheme(" ", normal));
    p.draw_box(Rect{0, 0, 12, 6}, LineStyle::Single, normal);
    p.draw_text(Point{2, 0}, "Title", bold);
    p.hline(Point{0, 3}, 12, LineStyle::Single, normal);

    expect_round_trip(capture(s, CursorState{true, Point{2, 4}, CursorShape::Block}),
                       "golden/box_and_junctions.dump");
}

CK_TEST(golden_shadow_dims_the_desktop_in_an_l_shaped_footprint) {
    Compositor c(Size{10, 5});
    Surface bg(Size{10, 5}, Cell::from_grapheme(".", Style{}));
    Surface win(Size{5, 3});
    Painter wp(win, Rect{0, 0, 5, 3});
    wp.draw_box(Rect{0, 0, 5, 3}, LineStyle::Single, Style{});
    std::vector<Layer> layers{{1, &win, Point{1, 1}, /*casts_shadow=*/true}};
    c.compose(layers, bg, ShadowSpec{});

    expect_round_trip(capture_frame(c), "golden/shadow.dump");
}

CK_TEST(golden_overlapping_shadows_form_one_binary_union) {
    Compositor c(Size{10, 5});
    const Style bright{Color::rgb(200, 200, 200), Color::rgb(160, 160, 160), Attr{}};
    Surface bg(Size{10, 5}, Cell::from_grapheme(".", bright));
    Surface first(Size{3, 2}, Cell::from_grapheme("A", Style{}));
    Surface second(Size{3, 2}, Cell::from_grapheme("B", Style{}));
    std::vector<Layer> layers{
        {1, &first, Point{0, 0}, /*casts_shadow=*/true},
        {2, &second, Point{1, 0}, /*casts_shadow=*/true},
    };
    c.compose(layers, bg, ShadowSpec{});

    // The right-hand shadow strips overlap at (4,1), while the bottom
    // strips overlap at (3,2) and (4,2). The golden contains one dimmed
    // style and no twice-dimmed style for those intersections.
    expect_round_trip(capture_frame(c), "golden/shadow_binary_union.dump");
}

CK_TEST(golden_raster_region_anchoring_occlusion_slicing_and_fallback) {
    Compositor c(Size{8, 6});
    Surface bg(Size{8, 6}, Cell::from_grapheme(".", Style{}));
    Surface layer_surface(Size{6, 6});
    Painter lp(layer_surface, Rect{0, 0, 6, 6});
    const auto image = std::make_shared<Image>(32, 32);
    lp.draw_image(Rect{0, 0, 4, 4}, 1, image,
                   [](Painter& fp) { fp.fill(Rect{0, 0, 4, 4}, Cell::from_grapheme("#", Style{})); });
    Surface occluder(Size{2, 2}, Cell::from_grapheme("O", Style{}));
    std::vector<Layer> layers{
        {1, &layer_surface, Point{0, 0}, false},
        {2, &occluder, Point{1, 1}, false},
    };
    c.compose(layers, bg);

    // Occlusion splits the one logical region (source id 1) into 4
    // slices, each fallback_active (Compositor never toggles this —
    // that is a future term-layer capability decision).
    CK_CHECK(c.visible_rasters().size() == 4);
    for (const auto& vr : c.visible_rasters()) CK_CHECK(vr.fallback_active);

    expect_round_trip(capture_frame(c), "golden/raster_occlusion.dump");
}

CK_TEST(golden_z_order_keeps_foreground_frame_topology_independent_of_background_frame) {
    ui::RoleRegistry registry;
    const ui::StandardRoles roles = ui::intern_standard_roles(registry);
    const ui::Theme theme = ui::make_classic_theme(registry, roles);
    widgets::Desktop desktop(Rect{0, 0, 26, 12});
    desktop.set_context(ui::Context{&theme, &registry, nullptr});

    widgets::Window* background =
        desktop.add_window(std::make_unique<widgets::Window>("Back"));
    background->set_bounds(Rect{2, 2, 14, 4});
    widgets::Window* foreground =
        desktop.add_window(std::make_unique<widgets::Window>("Front"));
    foreground->set_bounds(Rect{12, 3, 12, 7});

    Surface surface(Size{26, 12});
    Painter painter(surface, Rect{0, 0, 26, 12});
    desktop.draw(painter);
    desktop.paint_children(painter);

    expect_round_trip(capture(surface), "golden/window_z_order_junction.dump");
}
