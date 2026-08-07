// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/canvas.hpp"

#include "cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Modifier;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Canvas;

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
    canvas.set_pixel_size(16, 16);  // no callback installed
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    canvas.draw(painter);  // must not crash even with content_dirty_ and no callback to satisfy it
    CK_CHECK(s.at(ckv::Point{0, 0}).grapheme() == "[");
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
    canvas.set_pixel_size(8, 8);
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
    canvas.set_pixel_size(8, 8);
    int calls = 0;
    canvas.set_draw_callback([&](ckv::Image&) { ++calls; });
    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 1);

    canvas.set_pixel_size(16, 16);  // real size change
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
    canvas.set_pixel_size(8, 8);
    int calls = 0;
    canvas.set_draw_callback([&](ckv::Image&) { ++calls; });
    {
        Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(s, Rect{0, 0, 10, 5});
        canvas.draw(painter);
    }
    CK_CHECK(calls == 1);

    canvas.set_pixel_size(8, 8);  // same size — no-op, does not mark dirty
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
    canvas.set_pixel_size(8, 8);
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
    canvas.set_pixel_size(8, 8);
    canvas.set_pixel_size(0, 0);
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
    canvas.set_pixel_size(8, 8);
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
