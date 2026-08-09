// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/scene/compositor.hpp"

#include "cktest.hpp"

using ckv::scene::Compositor;
using ckv::scene::Layer;
using ckv::scene::ShadowSpec;
using ckv::scene::Surface;

namespace {

// A freshly constructed Surface starts fully damaged (Surface's own
// invariant) — deliberately left that way here, since Compositor's
// new-layer detection depends on it: a layer with no prior compose()
// history is recognized as "new" precisely because its surface's own
// damage already covers it (see compute_damage's comment).
Surface make_surface(int w, int h, std::string_view glyph, ckv::Style style = {}) {
    return Surface(ckv::Size{w, h}, ckv::Cell::from_grapheme(glyph, style));
}

std::string row_text(const Surface& s, int y) {
    std::string out;
    for (int x = 0; x < s.size().width; ++x) out += s.at(ckv::Point{x, y}).grapheme();
    return out;
}

}  // namespace

CK_TEST(background_only_frame_matches_background_content) {
    Compositor c(ckv::Size{4, 2});
    Surface bg = make_surface(4, 2, ".");
    c.compose({}, bg);
    CK_CHECK(row_text(c.frame(), 0) == "....");
    CK_CHECK(row_text(c.frame(), 1) == "....");
}

CK_TEST(higher_layer_occludes_lower_layer_in_the_overlap) {
    Compositor c(ckv::Size{6, 3});
    Surface bg = make_surface(6, 3, ".");
    Surface low = make_surface(4, 3, "L");
    Surface high = make_surface(3, 3, "H");
    std::vector<Layer> layers{
        {1, &low, ckv::Point{0, 0}, false},
        {2, &high, ckv::Point{2, 0}, false},
    };
    c.compose(layers, bg);
    CK_CHECK(row_text(c.frame(), 0) == "LLHHH.");
    CK_CHECK(c.last_compose_cells_touched() == 6 * 3);  // first compose: everything is new/damaged
}

CK_TEST(steady_state_recompose_touches_zero_cells) {
    Compositor c(ckv::Size{4, 4});
    Surface bg = make_surface(4, 4, ".");
    Surface layer_surface = make_surface(2, 2, "X");
    std::vector<Layer> layers{{1, &layer_surface, ckv::Point{1, 1}, false}};

    c.compose(layers, bg);
    CK_CHECK(c.last_compose_cells_touched() > 0);

    c.compose(layers, bg);  // nothing changed: damage already consumed, same positions/ids
    CK_CHECK(c.last_compose_cells_touched() == 0);
}

CK_TEST(content_change_in_a_layer_only_damages_that_cell) {
    Compositor c(ckv::Size{5, 5});
    Surface bg = make_surface(5, 5, ".");
    Surface layer_surface = make_surface(3, 3, "X");
    std::vector<Layer> layers{{1, &layer_surface, ckv::Point{1, 1}, false}};
    c.compose(layers, bg);

    layer_surface.set_cell(ckv::Point{1, 1}, ckv::Cell::from_grapheme("Y", ckv::Style{}));
    c.compose(layers, bg);
    CK_CHECK(c.last_compose_cells_touched() == 1);
    CK_CHECK(c.frame().at(ckv::Point{2, 2}).grapheme() == "Y");
}

CK_TEST(moving_a_layer_redraws_both_old_and_new_positions) {
    Compositor c(ckv::Size{10, 3});
    Surface bg = make_surface(10, 3, ".");
    Surface layer_surface = make_surface(2, 1, "X");
    std::vector<Layer> layers{{1, &layer_surface, ckv::Point{0, 0}, false}};
    c.compose(layers, bg);
    CK_CHECK(row_text(c.frame(), 0) == "XX........");

    layers[0].position = ckv::Point{6, 0};
    c.compose(layers, bg);
    CK_CHECK(row_text(c.frame(), 0) == "......XX..");  // old spot (cols 0-1) reverted; new at 6-7
}

CK_TEST(removing_a_layer_redraws_its_old_area_from_whats_underneath) {
    Compositor c(ckv::Size{6, 1});
    Surface bg = make_surface(6, 1, ".");
    Surface layer_surface = make_surface(3, 1, "X");
    std::vector<Layer> layers{{1, &layer_surface, ckv::Point{1, 0}, false}};
    c.compose(layers, bg);
    CK_CHECK(row_text(c.frame(), 0) == ".XXX..");

    c.compose({}, bg);  // layer removed
    CK_CHECK(row_text(c.frame(), 0) == "......");
}

CK_TEST(shadow_dims_the_background_beneath_it) {
    Compositor c(ckv::Size{8, 4});
    const ckv::Style bright{ckv::Color::rgb(200, 200, 200), ckv::Color{}, ckv::Attr{}};
    Surface bg = make_surface(8, 4, ".", bright);
    Surface layer_surface = make_surface(3, 2, "W");
    std::vector<Layer> layers{{1, &layer_surface, ckv::Point{0, 0}, /*casts_shadow=*/true}};

    ShadowSpec shadow;  // dx=2, dy=1, default_dim
    c.compose(layers, bg, shadow);

    // Right strip of the shadow: columns 3-4, row 1 (it ends where the
    // non-overlapping bottom strip begins).
    const ckv::Cell shadowed = c.frame().at(ckv::Point{3, 1});
    CK_CHECK(shadowed.grapheme() == ".");      // background's own content shows through
    CK_CHECK(shadowed.style().fg.r() == 100);  // halved from 200
    // Bottom strip: row 2, columns 2-4 (width 3, offset right by dx=2).
    const ckv::Cell bottom_shadowed = c.frame().at(ckv::Point{2, 2});
    CK_CHECK(bottom_shadowed.style().fg.r() == 100);
    // Outside the shadow footprint: untouched.
    CK_CHECK(c.frame().at(ckv::Point{6, 3}).style().fg.r() == 200);
}

CK_TEST(a_layer_shadow_clip_excludes_docked_chrome_from_window_shadow) {
    Compositor c(ckv::Size{8, 4});
    const ckv::Style bright{ckv::Color::rgb(200, 200, 200), ckv::Color{}, ckv::Attr{}};
    Surface bg = make_surface(8, 4, ".", bright);
    Surface window = make_surface(4, 2, "W");
    // The content area ends before row 2. The window's bottom shadow would
    // otherwise dim that row, which represents a Desktop dock/status line.
    std::vector<Layer> layers{{1, &window, ckv::Point{1, 0}, /*casts_shadow=*/true,
                               ckv::Rect{0, 0, 8, 2}}};

    c.compose(layers, bg);
    CK_CHECK(c.frame().at(ckv::Point{5, 1}).style().fg.r() == 100);  // right shadow stays in content
    CK_CHECK(c.frame().at(ckv::Point{3, 2}).style().fg.r() == 200);  // bottom shadow is clipped
}

CK_TEST(a_single_shadow_footprint_is_a_non_overlapping_union) {
    const std::vector<ckv::Rect> footprint =
        ckv::scene::shadow_footprint(ckv::Rect{2, 2, 5, 3}, ShadowSpec{});
    CK_CHECK(footprint.size() == 2);
    CK_CHECK(footprint[0].intersected(footprint[1]).empty());
}

CK_TEST(overlapping_shadows_are_a_binary_union_not_cumulative_dimming) {
    Compositor c(ckv::Size{10, 6});
    const ckv::Style bright{ckv::Color::rgb(200, 200, 200), ckv::Color{}, ckv::Attr{}};
    Surface bg = make_surface(10, 6, ".", bright);
    Surface first = make_surface(3, 2, "A");
    Surface second = make_surface(3, 2, "B");
    std::vector<Layer> layers{
        {1, &first, ckv::Point{0, 0}, /*casts_shadow=*/true},
        {2, &second, ckv::Point{1, 0}, /*casts_shadow=*/true},
    };

    c.compose(layers, bg);

    // (4,1) is in both right-hand shadow strips and in neither window.
    // Shadow coverage is a set membership, not an opacity accumulator.
    CK_CHECK(c.frame().at(ckv::Point{4, 1}).style().fg.r() == 100);
}

CK_TEST(shadow_dims_a_lower_layer_beneath_the_caster_not_just_background) {
    // A layer BELOW the shadow-caster in z-order must still be dimmed
    // where the shadow falls on it — a naive "does any layer cover this
    // point" check run before the shadow check would let the lower
    // layer win undimmed, since its own rect does cover the point.
    Compositor c(ckv::Size{8, 4});
    const ckv::Style bright{ckv::Color::rgb(200, 200, 200), ckv::Color{}, ckv::Attr{}};
    Surface bg = make_surface(8, 4, ".");
    Surface low = make_surface(8, 4, "L", bright);  // covers the whole frame, below the caster
    Surface caster = make_surface(3, 2, "W");
    std::vector<Layer> layers{
        {1, &low, ckv::Point{0, 0}, false},
        {2, &caster, ckv::Point{0, 0}, /*casts_shadow=*/true},
    };
    ShadowSpec shadow;  // dx=2, dy=1
    c.compose(layers, bg, shadow);

    // Right strip of the caster's shadow: columns 3-4, row 1. `low`
    // covers this area too, so its "L" content — dimmed — must show.
    const ckv::Cell cell = c.frame().at(ckv::Point{3, 1});
    CK_CHECK(cell.grapheme() == "L");
    CK_CHECK(cell.style().fg.r() == 100);  // halved from 200, not left at 200
}

CK_TEST(a_newly_added_shadow_caster_damages_its_footprint_on_first_appearance) {
    Compositor c(ckv::Size{8, 4});
    const ckv::Style bright{ckv::Color::rgb(200, 200, 200), ckv::Color{}, ckv::Attr{}};
    Surface bg = make_surface(8, 4, ".", bright);
    c.compose({}, bg);  // nothing yet; consumes bg's initial damage

    Surface caster = make_surface(3, 2, "W");
    std::vector<Layer> layers{{1, &caster, ckv::Point{0, 0}, /*casts_shadow=*/true}};
    c.compose(layers, bg);  // caster appears for the first time

    const ckv::Cell shadow_cell = c.frame().at(ckv::Point{3, 1});  // right strip of its shadow
    CK_CHECK(shadow_cell.style().fg.r() == 100);  // dimmed, not left at bg's original 200
}

CK_TEST(shadow_is_occluded_by_a_higher_layer) {
    Compositor c(ckv::Size{8, 4});
    const ckv::Style bright{ckv::Color::rgb(200, 200, 200), ckv::Color{}, ckv::Attr{}};
    Surface bg = make_surface(8, 4, ".", bright);
    Surface shadow_caster = make_surface(3, 2, "W");
    Surface blocker = make_surface(2, 2, "B");
    std::vector<Layer> layers{
        {1, &shadow_caster, ckv::Point{0, 0}, /*casts_shadow=*/true},
        {2, &blocker, ckv::Point{3, 1}, false},  // sits exactly on the shadow's right strip
    };
    c.compose(layers, bg);

    CK_CHECK(c.frame().at(ckv::Point{3, 1}).grapheme() == "B");  // blocker wins, not the shadow
    CK_CHECK(c.frame().at(ckv::Point{3, 1}).style().fg == ckv::Style{}.fg);  // undimmed
}

CK_TEST(visible_rasters_unoccluded_region_reports_the_full_anchor) {
    Compositor c(ckv::Size{10, 10});
    Surface bg = make_surface(10, 10, ".");
    Surface layer_surface = make_surface(6, 6, ".");
    const auto image = std::make_shared<ckv::Image>(16, 16);
    layer_surface.add_raster_region(
        ckv::scene::RasterRegion{1, ckv::Rect{1, 1, 3, 2}, image, true});
    std::vector<Layer> layers{{1, &layer_surface, ckv::Point{0, 0}, false}};
    c.compose(layers, bg);

    CK_CHECK(c.visible_rasters().size() == 1);
    CK_CHECK(c.visible_rasters()[0].visible_rect == (ckv::Rect{1, 1, 3, 2}));
    CK_CHECK(c.visible_rasters()[0].full_anchor == (ckv::Rect{1, 1, 3, 2}));
    CK_CHECK(c.visible_rasters()[0].fallback_active);
}

CK_TEST(visible_rasters_are_sliced_by_a_higher_occluding_layer) {
    Compositor c(ckv::Size{10, 10});
    Surface bg = make_surface(10, 10, ".");
    Surface layer_surface = make_surface(6, 6, ".");
    const auto image = std::make_shared<ckv::Image>(16, 16);
    layer_surface.add_raster_region(
        ckv::scene::RasterRegion{1, ckv::Rect{0, 0, 4, 4}, image, true});
    Surface occluder = make_surface(2, 2, "O");
    std::vector<Layer> layers{
        {1, &layer_surface, ckv::Point{0, 0}, false},
        {2, &occluder, ckv::Point{1, 1}, false},  // covers the middle of the raster region
    };
    c.compose(layers, bg);

    CK_CHECK(c.visible_rasters().size() == 4);  // an interior cut yields 4 slices (rect_ops)
    long long total_area = 0;
    for (const auto& vr : c.visible_rasters())
        total_area += static_cast<long long>(vr.visible_rect.width) * vr.visible_rect.height;
    CK_CHECK(total_area == 4 * 4 - 2 * 2);  // full anchor minus the occluder's overlap
}

CK_TEST(visible_rasters_fully_occluded_region_yields_no_slices) {
    Compositor c(ckv::Size{10, 10});
    Surface bg = make_surface(10, 10, ".");
    Surface layer_surface = make_surface(6, 6, ".");
    const auto image = std::make_shared<ckv::Image>(16, 16);
    layer_surface.add_raster_region(
        ckv::scene::RasterRegion{1, ckv::Rect{0, 0, 2, 2}, image, true});
    Surface occluder = make_surface(4, 4, "O");
    std::vector<Layer> layers{
        {1, &layer_surface, ckv::Point{0, 0}, false},
        {2, &occluder, ckv::Point{0, 0}, false},  // fully covers the raster region
    };
    c.compose(layers, bg);
    CK_CHECK(c.visible_rasters().empty());
}

CK_TEST(resize_drops_previous_state_and_fully_damages_the_new_frame) {
    Compositor c(ckv::Size{4, 4});
    Surface bg = make_surface(4, 4, ".");
    c.compose({}, bg);
    CK_CHECK(c.frame().size().width == 4);

    c.resize(ckv::Size{8, 8});
    CK_CHECK(c.frame().size().width == 8);
    CK_CHECK(c.visible_rasters().empty());

    Surface bg2 = make_surface(8, 8, "x");
    c.compose({}, bg2);
    CK_CHECK(c.last_compose_cells_touched() == 64);  // everything is new again after resize
    CK_CHECK(row_text(c.frame(), 0) == "xxxxxxxx");
}

CK_TEST(cursor_state_is_stored_and_retrievable) {
    Compositor c(ckv::Size{4, 4});
    CK_CHECK(!c.cursor().visible);
    c.set_cursor(ckv::scene::CursorState{true, ckv::Point{2, 3}, ckv::scene::CursorShape::Bar});
    CK_CHECK(c.cursor().visible);
    CK_CHECK(c.cursor().position == (ckv::Point{2, 3}));
    CK_CHECK(c.cursor().shape == ckv::scene::CursorShape::Bar);
}
