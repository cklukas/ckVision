// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <string>

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

CK_TEST(graphics_about_dialog_carries_the_project_copyright) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ManualClock clock;
    Application app(term, clock);
    ckv::graphics::GraphicsApp graphics(app);

    CK_CHECK(app.execute_command(app.commands().standard().help));
    app.step(0);
    CK_CHECK(term.written_bytes().find(
                 "Copyright (c) 2026 C. Klukas. All rights reserved.") != std::string::npos);
}

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
    CK_CHECK(graphics.canvas()->pixel_size() == (ckv::PixelSize{108, 30}));
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

namespace {
// A terminal that reports SGR-Pixels mouse positions (mode 1016) over 9 x 18
// pixel cells, drawing Sixel.
ckv::term::Capabilities pixel_mouse_profile() {
    ckv::term::Capabilities caps = ckv::term::headless_sixel_profile();
    caps.pixel_mouse = true;
    return caps;
}

// One SGR-Pixels report as the terminal sends it: 1-based pixel coordinates,
// 'M' for a press and 'm' for a release.
std::string sgr_pixel_report(int button, ckv::PixelPoint pixel, bool press) {
    return "\x1B[<" + std::to_string(button) + ";" + std::to_string(pixel.x + 1) + ";" +
           std::to_string(pixel.y + 1) + (press ? "M" : "m");
}

void click_pixel(ckv::term::HeadlessTerminal& term, Application& app, ckv::PixelPoint pixel) {
    term.inject_bytes(sgr_pixel_report(0, pixel, true), 0);
    app.step(0);
    term.inject_bytes(sgr_pixel_report(0, pixel, false), 0);
    app.step(0);
}
}  // namespace

CK_TEST(graphics_example_pixel_mouse_reaches_image_and_canvas_callbacks_in_both_spaces_mapped_into_the_picture) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, pixel_mouse_profile());
    ManualClock clock;
    Application app(term, clock);
    ckv::graphics::GraphicsApp graphics(app);
    app.step(0);
    CK_CHECK(app.terminal_cell_pixels() == (ckv::PixelSize{9, 18}));

    // The 40 x 20 picture keeps its proportions: over 9 x 18 cells that is a
    // 40 x 10-cell box (360 x 180 pixels, nine screen pixels to a picture
    // pixel), centred in the view.
    const ckv::Rect view = graphics.image_view()->absolute_bounds();
    const ckv::Rect anchor = graphics.image_view()->image_anchor();
    CK_CHECK(anchor == (ckv::Rect{7, 0, 40, 10}));
    const ckv::PixelPoint picture_origin{(view.x + anchor.x) * 9, (view.y + anchor.y) * 18};

    // A press 274 pixels right and 107 down from the picture's corner.
    const ckv::PixelPoint pressed{picture_origin.x + 274, picture_origin.y + 107};
    term.inject_bytes(sgr_pixel_report(0, pressed, true), 0);
    app.step(0);
    const auto& press = graphics.last_image_pointer();
    CK_CHECK(press.has_value());
    if (!press) return;
    CK_CHECK(press->event.action == ckv::MouseAction::Down);
    CK_CHECK(press->event.button == MouseButton::Left);
    CK_CHECK(press->event.pixel == pressed);
    CK_CHECK(press->event.cell == (Point{pressed.x / 9, pressed.y / 18}));
    CK_CHECK(press->event.cell == (Point{view.x + anchor.x + 30, view.y + anchor.y + 5}));
    CK_CHECK(press->image_pixel == (ckv::PixelPoint{274 * 40 / 360, 107 * 20 / 180}));
    CK_CHECK(press->image_pixel == (ckv::PixelPoint{30, 11}));

    // The release arrives too, and the picture's corners map to its first and
    // last pixels.
    term.inject_bytes(sgr_pixel_report(0, pressed, false), 0);
    app.step(0);
    CK_CHECK(graphics.last_image_pointer()->event.action == ckv::MouseAction::Up);
    click_pixel(term, app, picture_origin);
    CK_CHECK(graphics.last_image_pointer()->image_pixel == (ckv::PixelPoint{0, 0}));
    click_pixel(term, app, ckv::PixelPoint{picture_origin.x + 359, picture_origin.y + 179});
    CK_CHECK(graphics.last_image_pointer()->image_pixel == (ckv::PixelPoint{39, 19}));
    // Beside the picture, still over the view: both positions arrive, and
    // there is no picture pixel to name.
    const ckv::PixelPoint beside{picture_origin.x - 5, picture_origin.y + 40};
    click_pixel(term, app, beside);
    CK_CHECK(graphics.last_image_pointer()->event.pixel == beside);
    CK_CHECK(!graphics.last_image_pointer()->image_pixel.has_value());

    // The Canvas tab, clicked with the same pixel mouse.
    int tab_x = -1;
    int tab_y = -1;
    const ckv::FrameView frame = term.display().frame();
    for (int y = 0; y < frame.size().height && tab_x < 0; ++y) {
        std::string row;
        for (int x = 0; x < frame.size().width; ++x) row += frame.at(Point{x, y}).grapheme();
        if (const std::size_t at = row.find("Canvas"); at != std::string::npos) {
            // Every cell up to the tab caption is one byte wide in this row.
            int column = 0;
            for (std::size_t byte = 0; byte < at; ++column) byte += frame.at(Point{column, y}).grapheme().size();
            tab_x = column;
            tab_y = y;
        }
    }
    CK_CHECK(tab_x >= 0);
    click_pixel(term, app, ckv::PixelPoint{tab_x * 9 + 4, tab_y * 18 + 9});
    CK_CHECK(graphics.tabs()->active_index() == 1U);

    // The canvas's backing image is 108 x 30 (its 54 x 10 cells at the 2 x 3
    // metric it was given), shown over 486 x 180 terminal pixels.
    CK_CHECK(graphics.canvas()->pixel_size() == (ckv::PixelSize{108, 30}));
    const ckv::Rect canvas = graphics.canvas()->absolute_bounds();
    const ckv::PixelPoint on_canvas{canvas.x * 9 + 250, canvas.y * 18 + 100};
    term.inject_bytes(sgr_pixel_report(0, on_canvas, true), 0);
    app.step(0);
    const auto& canvas_press = graphics.last_canvas_pointer();
    CK_CHECK(canvas_press.has_value());
    if (!canvas_press) return;
    CK_CHECK(canvas_press->event.pixel == on_canvas);
    CK_CHECK(canvas_press->event.cell == (Point{canvas.x + 27, canvas.y + 5}));
    CK_CHECK(canvas_press->image_pixel == (ckv::PixelPoint{250 * 108 / 486, 100 * 30 / 180}));
    CK_CHECK(canvas_press->image_pixel == (ckv::PixelPoint{55, 16}));
}

CK_TEST(graphics_example_cell_only_mouse_reaches_the_picture_without_a_picture_pixel) {
    // Precision is honest: a terminal that reports cells only yields no
    // pixel field and so no picture pixel, never one estimated from the cell.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, ckv::term::headless_sixel_profile());
    ManualClock clock;
    Application app(term, clock);
    ckv::graphics::GraphicsApp graphics(app);
    app.step(0);
    const ckv::Rect view = graphics.image_view()->absolute_bounds();
    const ckv::Rect anchor = graphics.image_view()->image_anchor();
    const Point cell{view.x + anchor.x + 30, view.y + anchor.y + 5};
    term.inject_bytes("\x1B[<0;" + std::to_string(cell.x + 1) + ";" + std::to_string(cell.y + 1) + "M", 0);
    app.step(0);
    const auto& press = graphics.last_image_pointer();
    CK_CHECK(press.has_value());
    if (!press) return;
    CK_CHECK(press->event.cell == cell);
    CK_CHECK(!press->event.pixel.has_value());
    CK_CHECK(!press->image_pixel.has_value());
}
