// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/canvas.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

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
using ckv::widgets::Canvas;
using ckv::widgets::fit_image_cells;
using ckv::Size;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};
}  // namespace

// --- No pixel size / no callback -------------------------------------

CK_TEST(with_no_pixel_size_set_draw_falls_back_to_a_plain_fill) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 10, 5});
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    canvas.draw(painter);
    CK_CHECK(s.at(ckv::Point{0, 0}).grapheme() == " ");
}

CK_TEST(setting_pixel_size_with_no_draw_callback_still_renders_via_the_mandatory_fallback) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 10, 5});
    canvas.set_pixel_size(PixelSize{16, 16});  // no callback installed
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    canvas.draw(painter);  // must not crash even with content_dirty_ and no callback to satisfy it
    CK_CHECK(s.at(ckv::Point{0, 0}).grapheme() == "[");
}

CK_TEST(cell_metrics_derive_the_canvas_pixel_size_from_the_widget_bounds) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 4, 3});
    canvas.set_cell_metrics(ckv::PixelSize{2, 3});
    CK_CHECK(canvas.pixel_size() == (ckv::PixelSize{8, 9}));
    CK_CHECK(canvas.cell_metrics() == (ckv::PixelSize{2, 3}));
}

CK_TEST(resizing_a_metric_driven_canvas_reallocates_and_redraws_at_the_new_pixel_size) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 4, 3});
    canvas.set_cell_metrics(ckv::PixelSize{2, 3});
    int calls = 0;
    PixelSize seen;
    canvas.set_draw_callback([&](ckv::Image& image) {
        ++calls;
        seen = image.size();
    });
    {
        Surface s(ckv::Size{10, 10}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 10});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 1);
    CK_CHECK(seen == (PixelSize{8, 9}));

    canvas.set_bounds(Rect{0, 0, 5, 4});
    {
        Surface s(ckv::Size{10, 10}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 10});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 2);
    CK_CHECK(seen == (PixelSize{10, 12}));
}

CK_TEST(canvas_draw_adds_a_raster_region_with_the_widget_cell_anchor) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 4, 3});
    canvas.set_cell_metrics(ckv::PixelSize{2, 2});
    Surface s(ckv::Size{4, 3}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 4, 3});
    canvas.draw(painter);

    CK_CHECK(s.raster_regions().size() == 1);
    CK_CHECK(s.raster_regions()[0].anchor == (Rect{0, 0, 4, 3}));
    CK_CHECK(s.raster_regions()[0].image->width() == 8);
    CK_CHECK(s.raster_regions()[0].image->height() == 6);
}

// --- Draw callback invocation ------------------------------------------

// Each canvas.draw() call adds a raster region to whatever Surface its
// Painter targets, and Surface::add_raster_region asserts the id is
// unique FOR THE LIFETIME OF THAT SURFACE (no automatic per-frame
// reset — that's the compositor's job in a real render loop, not
// something a widget's own draw() does). These tests therefore give
// each draw() call its own fresh Surface/Painter, matching how a real
// frame-by-frame render loop would actually invoke draw() — reusing
// one Surface across multiple draw() calls in a test would trip that
// assertion for reasons that have nothing to do with the widget logic
// under test.
CK_TEST(the_draw_callback_runs_once_per_content_invalidation_not_once_per_frame) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 10, 5});
    canvas.set_pixel_size(PixelSize{8, 8});
    int calls = 0;
    canvas.set_draw_callback([&](ckv::Image&) { ++calls; });

    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 1);
    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);  // a second frame with nothing invalidated
    }
    CK_CHECK(calls == 1);  // must NOT re-run the callback
    canvas.invalidate_content();
    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 2);  // re-runs after an explicit content invalidation
}

CK_TEST(changing_pixel_size_marks_content_dirty_and_reruns_the_callback) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 10, 5});
    canvas.set_pixel_size(PixelSize{8, 8});
    int calls = 0;
    canvas.set_draw_callback([&](ckv::Image&) { ++calls; });
    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 1);

    canvas.set_pixel_size(PixelSize{16, 16});  // real size change
    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 2);
}

CK_TEST(setting_pixel_size_to_the_same_value_again_is_a_no_op) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 10, 5});
    canvas.set_pixel_size(PixelSize{8, 8});
    int calls = 0;
    canvas.set_draw_callback([&](ckv::Image&) { ++calls; });
    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 1);

    canvas.set_pixel_size(PixelSize{8, 8});  // same size — no-op, does not mark dirty
    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 1);
}

CK_TEST(installing_a_new_draw_callback_marks_content_dirty) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 10, 5});
    canvas.set_pixel_size(PixelSize{8, 8});
    canvas.set_draw_callback([](ckv::Image&) {});
    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);
    }

    int calls = 0;
    canvas.set_draw_callback([&](ckv::Image&) { ++calls; });  // replacing the callback
    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 1);  // the NEW callback ran on the very next draw
}

// --- Degenerate sizes ------------------------------------------------

CK_TEST(setting_a_zero_pixel_size_falls_back_to_the_plain_fill) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 10, 5});
    canvas.set_pixel_size(PixelSize{8, 8});
    canvas.set_pixel_size(PixelSize{0, 0});
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    canvas.draw(painter);
    CK_CHECK(s.at(ckv::Point{0, 0}).grapheme() == " ");  // not "[canvas]" — fell back to the no-image path
}

CK_TEST(a_zero_size_view_with_content_does_not_crash) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 0, 0});
    canvas.set_pixel_size(PixelSize{8, 8});
    canvas.set_draw_callback([](ckv::Image&) {});
    Surface s(ckv::Size{1, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 1, 1});
    canvas.draw(painter);
    CK_CHECK(true);
}

// --- Mouse: dual-space forwarding --------------------------------------

CK_TEST(on_click_forwards_the_full_mouse_event) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    bool called = false;
    canvas.on_click = [&](const ckv::MouseEvent&) { called = true; };
    canvas.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 1}, std::nullopt,
                                     Modifier::None});
    CK_CHECK(called);
}

CK_TEST(with_no_on_click_handler_mouse_events_are_still_reported_as_handled) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    CK_CHECK(canvas.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 1},
                                              std::nullopt, Modifier::None}));
}

CK_TEST(a_wheel_notch_is_reported_to_on_click_and_left_for_an_enclosing_scroller) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    int reported = 0;
    canvas.on_click = [&reported](const ckv::MouseEvent&) { ++reported; };
    CK_CHECK(!canvas.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelUp, ckv::Point{1, 1},
                                               std::nullopt, Modifier::None}));
    CK_CHECK(reported == 1);
}

// --- fit_image_cells ------------------------------------------------------

CK_TEST(a_pictures_proportions_survive_a_cell_that_is_nothing_like_square) {
    // 1528x1029 artwork (w:h 1.485) on a 11x51 cell — a real terminal
    // report, and nearly five times taller than wide.
    const Size box = fit_image_cells(PixelSize{1528, 1029}, PixelSize{11, 51}, Size{64, 12});
    const double drawn = static_cast<double>(box.width * 11) / (box.height * 51);
    CK_CHECK(drawn > 1.33 && drawn < 1.65);  // still the artwork's shape
}

CK_TEST(the_same_picture_on_a_conventional_cell_keeps_the_same_proportions) {
    const Size box = fit_image_cells(PixelSize{1528, 1029}, PixelSize{11, 24}, Size{64, 12});
    const double drawn = static_cast<double>(box.width * 11) / (box.height * 24);
    CK_CHECK(drawn > 1.33 && drawn < 1.65);
}

CK_TEST(a_fitted_box_never_exceeds_the_cells_it_was_allowed) {
    for (const PixelSize cell : {PixelSize{11, 51}, PixelSize{11, 24}, PixelSize{6, 12}, PixelSize{20, 20}}) {
        const Size box = fit_image_cells(PixelSize{1528, 1029}, cell, Size{64, 12});
        CK_CHECK(box.width >= 1 && box.width <= 64);
        CK_CHECK(box.height >= 1 && box.height <= 12);
    }
}

CK_TEST(a_terminal_that_reports_no_cell_is_fitted_against_the_assumed_one) {
    CK_CHECK(fit_image_cells(PixelSize{1528, 1029}, PixelSize{0, 0}, Size{64, 12}) ==
             fit_image_cells(PixelSize{1528, 1029}, ckv::widgets::kAssumedCellPixels, Size{64, 12}));
}

CK_TEST(an_empty_image_or_an_empty_allowance_yields_no_box) {
    CK_CHECK(fit_image_cells(PixelSize{0, 0}, PixelSize{11, 24}, Size{64, 12}) == Size{});
    CK_CHECK(fit_image_cells(PixelSize{1528, 1029}, PixelSize{11, 24}, Size{0, 0}) == Size{});
}

// --- The application's own fallback ----------------------------------

CK_TEST(an_installed_fallback_painter_replaces_the_placeholder_name) {
    // A reader whose terminal cannot show the picture is the reader who
    // needs its information most, so the application gets to say what
    // stands in for it.
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 20, 3});
    canvas.set_pixel_size(PixelSize{8, 8});
    canvas.set_draw_callback([](ckv::Image&) {});
    canvas.set_fallback_painter([](Painter& painter, Rect area) {
        painter.draw_text(ckv::Point{0, 0}, "t = 10.000", ckv::Style{});
        CK_CHECK(area.width == 20 && area.height == 3);  // its own area, in cells
    });
    Surface s(ckv::Size{20, 3}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 3});
    canvas.draw(painter);
    CK_CHECK(s.at(ckv::Point{0, 0}).grapheme() == "t");
    CK_CHECK(s.at(ckv::Point{4, 0}).grapheme() == "1");
}

CK_TEST(without_a_fallback_painter_the_canvas_still_names_itself) {
    Fixture f;
    Canvas canvas;
    canvas.set_context(f.ctx());
    canvas.set_bounds(Rect{0, 0, 20, 3});
    canvas.set_pixel_size(PixelSize{8, 8});
    canvas.set_draw_callback([](ckv::Image&) {});
    Surface s(ckv::Size{20, 3}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 3});
    canvas.draw(painter);
    CK_CHECK(s.at(ckv::Point{0, 0}).grapheme() == "[");
}

// --- Mouse: image-local mapping -----------------------------------------

CK_TEST(image_pixel_at_maps_through_the_terminal_cell_metric_to_the_backing_image_the_canvas_fills) {
    ckv::term::HeadlessTerminal term(ckv::Size{40, 12}, ckv::term::headless_no_graphics_profile());  // 9 x 18 cells
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    auto owned = std::make_unique<Canvas>();
    Canvas* const canvas = owned.get();
    canvas->set_bounds(Rect{2, 1, 10, 4});
    // A backing image sized by a 2 x 3 metric the application chose, shown
    // over the terminal's own 9 x 18 cells: 20 x 12 pixels over 90 x 72.
    canvas->set_cell_metrics(PixelSize{2, 3});
    // Placed inside a container: a direct child of the root is laid out to fill it.
    app.root().add_child(std::make_unique<ckv::ui::View>())->add_child(std::move(owned));
    CK_CHECK(canvas->pixel_size() == (PixelSize{20, 12}));
    ckv::MouseEvent event{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 1}, ckv::PixelPoint{18, 18},
                          Modifier::None};
    CK_CHECK(canvas->image_pixel_at(event) == (ckv::PixelPoint{0, 0}));
    event.pixel = ckv::PixelPoint{18 + 89, 18 + 71};
    CK_CHECK(canvas->image_pixel_at(event) == (ckv::PixelPoint{19, 11}));
    event.pixel = ckv::PixelPoint{18 + 45, 18 + 30};  // floor(45 * 20 / 90), floor(30 * 12 / 72)
    CK_CHECK(canvas->image_pixel_at(event) == (ckv::PixelPoint{10, 5}));
    event.pixel = ckv::PixelPoint{18 + 90, 18};
    CK_CHECK(!canvas->image_pixel_at(event).has_value());
    event.pixel.reset();
    CK_CHECK(!canvas->image_pixel_at(event).has_value());
}

CK_TEST(a_scripted_canvas_redraws_at_its_new_pixel_size_and_forwards_both_coordinate_spaces) {
    // Application-level script: step() is what draws the canvas, a resize
    // between two steps redraws it at the new pixel size, and a click
    // dispatched with pixels arrives at on_click with both of its spaces.
    ckv::term::HeadlessTerminal term(ckv::Size{20, 8});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* canvas = app.root().add(std::make_unique<Canvas>());
    canvas->set_bounds(Rect{2, 1, 4, 3});
    canvas->set_cell_metrics(PixelSize{2, 3});
    int calls = 0;
    PixelSize seen;
    canvas->set_draw_callback([&](ckv::Image& image) {
        ++calls;
        seen = image.size();
    });
    std::vector<ckv::MouseEvent> clicks;
    canvas->on_click = [&](const ckv::MouseEvent& event) { clicks.push_back(event); };
    app.step(0);
    CK_CHECK(calls == 1);
    CK_CHECK(seen == (PixelSize{8, 9}));
    app.step(0);
    CK_CHECK(calls == 1);  // nothing changed, nothing redrawn

    canvas->set_bounds(Rect{2, 1, 5, 4});
    app.step(0);
    CK_CHECK(calls == 2);
    CK_CHECK(seen == (PixelSize{10, 12}));

    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{3, 2},
                                          ckv::PixelPoint{7, 5}, Modifier::None}));
    CK_CHECK(clicks.size() == 1U);
    if (!clicks.empty()) {
        CK_CHECK(clicks[0].cell == (ckv::Point{3, 2}));
        CK_CHECK(clicks[0].pixel.has_value());
        if (clicks[0].pixel) CK_CHECK(clicks[0].pixel->x == 7 && clicks[0].pixel->y == 5);
    }
}
