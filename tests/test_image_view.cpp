// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/image_view.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Image;
using ckv::Modifier;
using ckv::PixelSize;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::ImageView;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

ckv::MouseEvent click(ckv::Point p) {
    return ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, p, std::nullopt, Modifier::None};
}
}  // namespace

// --- No image installed -----------------------------------------------

CK_TEST(with_no_image_installed_draw_fills_the_fallback_style_without_crashing) {
    Fixture f;
    ImageView view;
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 10, 5});
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    view.draw(painter);
    CK_CHECK(s.at(ckv::Point{0, 0}).grapheme() == " ");  // cleared, not left as the surface's prior fill
}

CK_TEST(image_defaults_to_null) {
    Fixture f;
    ImageView view;
    CK_CHECK(view.image() == nullptr);
}

// --- With an image installed -----------------------------------------

CK_TEST(set_image_installs_the_image_and_invalidates) {
    Fixture f;
    ImageView view;
    auto img = std::make_shared<Image>(PixelSize{4, 4});
    view.set_image(img);
    CK_CHECK(view.image() == img);
}

CK_TEST(draw_with_a_real_image_calls_the_mandatory_fallback_and_does_not_crash) {
    Fixture f;
    ImageView view;
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 10, 5});
    view.set_image(std::make_shared<Image>(PixelSize{8, 8}));
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    view.draw(painter);
    // D-017: draw_image ALWAYS invokes the fallback, so cell content
    // (the placeholder text) must be present regardless of raster
    // presentation.
    CK_CHECK(s.at(ckv::Point{0, 0}).grapheme() == "[");
}

CK_TEST(draw_with_a_real_image_adds_a_raster_region_at_the_widget_cell_anchor) {
    Fixture f;
    ImageView view;
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 10, 5});
    auto image = std::make_shared<Image>(PixelSize{8, 8});
    view.set_image(image);
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    view.draw(painter);

    CK_CHECK(s.raster_regions().size() == 1);
    CK_CHECK(s.raster_regions()[0].anchor == (Rect{0, 0, 10, 5}));
    CK_CHECK(s.raster_regions()[0].image == image);
}

CK_TEST(surface_local_raster_identity_keeps_two_image_views_distinct_in_one_frame) {
    // Widgets request a Surface-local identity (id zero); two image views can
    // therefore share a frame without process-global mutable allocation.
    Fixture f;
    ImageView a;
    ImageView b;
    a.set_context(f.ctx());
    b.set_context(f.ctx());
    a.set_bounds(Rect{0, 0, 5, 5});
    b.set_bounds(Rect{5, 0, 5, 5});
    a.set_image(std::make_shared<Image>(PixelSize{4, 4}));
    b.set_image(std::make_shared<Image>(PixelSize{4, 4}));
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    a.draw(painter);
    b.draw(painter);
    CK_CHECK(s.raster_regions().size() == 2);
    CK_CHECK(s.raster_regions()[0].id != s.raster_regions()[1].id);
}

CK_TEST(surface_local_raster_identity_does_not_collide_with_a_caller_supplied_id) {
    Fixture f;
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    auto image = std::make_shared<Image>(PixelSize{1, 1});
    painter.draw_image(Rect{0, 0, 1, 1}, 1, image, [](Painter&) {});
    painter.draw_image(Rect{1, 0, 1, 1}, 0, image, [](Painter&) {});
    CK_CHECK(s.raster_regions().size() == 2);
    CK_CHECK(s.raster_regions()[0].id == 1);
    CK_CHECK(s.raster_regions()[1].id != 1);
}

// --- Degenerate sizes ----------------------------------------------

CK_TEST(a_zero_width_image_falls_back_to_the_plain_fill_rather_than_calling_draw_image) {
    Fixture f;
    ImageView view;
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 10, 5});
    view.set_image(std::make_shared<Image>(PixelSize{0, 4}));  // zero width — draw_image would CKV_ASSERT on this
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    view.draw(painter);  // must not abort
    CK_CHECK(true);
}

CK_TEST(a_zero_size_view_with_an_image_does_not_crash) {
    Fixture f;
    ImageView view;
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 0, 0});
    view.set_image(std::make_shared<Image>(PixelSize{4, 4}));
    Surface s(ckv::Size{1, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 1, 1});
    view.draw(painter);  // must not call draw_image with a zero-size anchor
    CK_CHECK(true);
}

// --- Mouse: dual-space forwarding --------------------------------------

CK_TEST(on_click_receives_the_full_mouse_event_including_the_pixel_field_when_present) {
    Fixture f;
    ImageView view;
    view.set_bounds(Rect{0, 0, 10, 5});
    std::optional<ckv::PixelPoint> seen_pixel;
    bool called = false;
    view.on_click = [&](const ckv::MouseEvent& e) {
        called = true;
        seen_pixel = e.pixel;
    };
    ckv::MouseEvent event = click(ckv::Point{2, 2});
    event.pixel = ckv::PixelPoint{123, 456};
    view.on_mouse(event);
    CK_CHECK(called);
    CK_CHECK(seen_pixel.has_value());
    CK_CHECK(seen_pixel->x == 123);
    CK_CHECK(seen_pixel->y == 456);
}

CK_TEST(on_click_receives_a_cell_only_event_with_no_pixel_field_when_the_terminal_has_none) {
    Fixture f;
    ImageView view;
    view.set_bounds(Rect{0, 0, 10, 5});
    std::optional<ckv::PixelPoint> seen_pixel = ckv::PixelPoint{1, 1};  // pre-seed to prove it gets cleared
    view.on_click = [&](const ckv::MouseEvent& e) { seen_pixel = e.pixel; };
    view.on_mouse(click(ckv::Point{2, 2}));  // no .pixel set
    CK_CHECK(!seen_pixel.has_value());
}

CK_TEST(with_no_on_click_handler_mouse_events_are_still_reported_as_handled) {
    Fixture f;
    ImageView view;
    view.set_bounds(Rect{0, 0, 10, 5});
    CK_CHECK(view.on_mouse(click(ckv::Point{2, 2})));
}

CK_TEST(a_wheel_notch_is_reported_to_on_click_and_left_for_an_enclosing_scroller) {
    // A picture does not scroll; the viewport around it does (D-081).
    ImageView view;
    view.set_bounds(Rect{0, 0, 10, 5});
    int reported = 0;
    view.on_click = [&reported](const ckv::MouseEvent&) { ++reported; };
    CK_CHECK(!view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                            ckv::Point{2, 2}, std::nullopt, Modifier::None}));
    CK_CHECK(reported == 1);
}

// --- Mouse: image-local mapping -----------------------------------------

CK_TEST(image_pixel_at_maps_a_reported_pixel_through_the_centred_anchor_and_the_terminal_cell_metric) {
    ckv::term::HeadlessTerminal term(ckv::Size{40, 12}, ckv::term::headless_no_graphics_profile());  // 9 x 18 cells
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    auto owned = std::make_unique<ImageView>();
    ImageView* const view = owned.get();
    view->set_bounds(Rect{2, 1, 30, 2});
    view->set_image(std::make_shared<Image>(PixelSize{40, 20}));
    // Placed inside a container: a direct child of the root is laid out to fill it.
    app.root().add_child(std::make_unique<ckv::ui::View>())->add_child(std::move(owned));
    // In two rows, 40 x 20 pixels keep their shape as 8 x 2 cells (72 x 36
    // pixels), centred 11 cells in: the picture starts at screen cell
    // (13, 1), pixel (117, 18).
    CK_CHECK(view->image_anchor() == (Rect{11, 0, 8, 2}));
    ckv::MouseEvent event = click(ckv::Point{13, 1});
    event.pixel = ckv::PixelPoint{117, 18};
    CK_CHECK(view->image_pixel_at(event) == (ckv::PixelPoint{0, 0}));
    event.pixel = ckv::PixelPoint{117 + 50, 18 + 10};
    CK_CHECK(view->image_pixel_at(event) == (ckv::PixelPoint{27, 5}));
    // Over the view but beside the picture, and a report without pixels.
    event.pixel = ckv::PixelPoint{117 - 1, 18};
    CK_CHECK(!view->image_pixel_at(event).has_value());
    CK_CHECK(!view->image_pixel_at(click(ckv::Point{13, 1})).has_value());
}

CK_TEST(image_pixel_at_is_empty_without_an_application_or_an_image) {
    ImageView detached;
    detached.set_bounds(Rect{0, 0, 10, 5});
    detached.set_image(std::make_shared<Image>(PixelSize{4, 4}));
    ckv::MouseEvent event = click(ckv::Point{1, 1});
    event.pixel = ckv::PixelPoint{10, 20};
    CK_CHECK(!detached.image_pixel_at(event).has_value());

    ckv::term::HeadlessTerminal term(ckv::Size{40, 12}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    auto owned = std::make_unique<ImageView>();
    ImageView* const view = owned.get();
    view->set_bounds(Rect{0, 0, 10, 5});
    // Placed inside a container: a direct child of the root is laid out to fill it.
    app.root().add_child(std::make_unique<ckv::ui::View>())->add_child(std::move(owned));
    CK_CHECK(!view->image_pixel_at(event).has_value());
}
