// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/canvas.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "graphics_app.hpp"

using ckv::ManualClock;
using ckv::Modifier;
using ckv::MouseAction;
using ckv::MouseButton;
using ckv::Point;
using ckv::ui::Application;

CK_TEST(graphics_example_emits_sixel_when_the_terminal_supports_raster_graphics) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, ckv::term::headless_sixel_profile());
    ManualClock clock;
    Application app(term, clock);
    ckv::graphics::GraphicsApp graphics(app);

    app.step(0);
    CK_CHECK(term.written_bytes().find("\x1B" "P") != std::string::npos);
    CK_CHECK(term.display().has_raster_pixels());
    CK_CHECK(graphics.demo_image()->width() == 40);
}

CK_TEST(graphics_example_uses_cell_fallback_when_graphics_are_unavailable) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ManualClock clock;
    Application app(term, clock);
    ckv::graphics::GraphicsApp graphics(app);

    app.step(0);
    CK_CHECK(term.written_bytes().find("\x1B" "P") == std::string::npos);
    CK_CHECK(term.written_bytes().find("[image]") != std::string::npos);
    CK_CHECK(!term.display().has_raster_pixels());
    CK_CHECK(graphics.canvas()->pixel_size() == (ckv::Size{108, 30}));
}

CK_TEST(graphics_example_forwards_mouse_to_image_and_canvas_public_callbacks) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app(term, clock);
    ckv::graphics::GraphicsApp graphics(app);
    app.step(0);

    CK_CHECK(graphics.image_view()->on_mouse(
        ckv::MouseEvent{MouseAction::Down, MouseButton::Left, Point{1, 1}, std::nullopt, Modifier::None}));
    graphics.tabs()->set_active_index(1);
    app.step(0);
    CK_CHECK(graphics.canvas()->on_mouse(
        ckv::MouseEvent{MouseAction::Down, MouseButton::Left, Point{1, 1}, std::nullopt, Modifier::None}));

    CK_CHECK(graphics.image_clicks() == 1);
    CK_CHECK(graphics.canvas_clicks() == 1);
}
