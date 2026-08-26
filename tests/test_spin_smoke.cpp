// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <string>

#include "cvision/core/palette.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/canvas.hpp"

#include "mesh.hpp"
#include "renderer.hpp"
#include "spin_app.hpp"

using ckv::ManualClock;
using ckv::Rect;
using ckv::Size;
using ckv::ui::Application;
using ckv::spin::FrameSpec;
using ckv::spin::MeshLibrary;
using ckv::spin::Renderer;
using ckv::spin::ShapeEntry;
using ckv::spin::ShapeId;
using ckv::spin::shape_catalog;
using ckv::spin::SpinApp;
using ckv::spin::SpinView;

namespace {

constexpr ckv::Image::Rgba kBackground{36, 114, 200, 255};

bool same_color(ckv::Image::Rgba a, ckv::Image::Rgba b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

// The tightest rectangle holding every pixel that is not the background.
Rect painted_bounds(const ckv::Image& frame, ckv::Image::Rgba background) {
    int left = frame.width();
    int top = frame.height();
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            if (same_color(frame.pixel(x, y), background)) continue;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }
    return right < left ? Rect{} : Rect{left, top, right - left + 1, bottom - top + 1};
}

std::size_t unique_colors(const ckv::Image& frame) {
    std::vector<std::uint32_t> seen;
    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            const ckv::Image::Rgba pixel = frame.pixel(x, y);
            const auto key = static_cast<std::uint32_t>(pixel.r) << 16 |
                             static_cast<std::uint32_t>(pixel.g) << 8 | pixel.b;
            if (std::find(seen.begin(), seen.end(), key) == seen.end()) seen.push_back(key);
        }
    }
    return seen.size();
}

struct Fixture {
    explicit Fixture(ckv::term::Capabilities caps = ckv::term::headless_sixel_profile())
        : term(ckv::Size{100, 30}, caps) {}

    // Renders every frame the application has asked for and delivers it,
    // which is one step() after the workers go idle.
    void settle() {
        spin.frames().wait_until_idle();
        app.step(clock.now_nanos());
    }

    // Moves time on and lets the loop run, which fires whatever tick is
    // armed. This is how a test plays a busy host: a tick that arrives far
    // later than it asked for is exactly what an overloaded loop delivers.
    void run_for(std::int64_t nanos) {
        clock.advance(nanos);
        settle();
    }

    // One frame delivered `nanos` after the previous one, without waiting
    // for the tick — for driving the delivered-rate readout to a known
    // value.
    void deliver_frame_after(std::int64_t nanos) {
        clock.advance(nanos);
        spin.view_at(0)->request_frame(spin.frames(), clock.now_nanos());
        settle();
    }

    // What the virtual display currently shows on one row, as text.
    std::string display_row(int y) const {
        const ckv::FrameView frame = term.display().frame();
        std::string row;
        for (int x = 0; x < frame.size().width; ++x) row += frame.at(ckv::Point{x, y}).grapheme();
        return row;
    }

    ckv::term::HeadlessTerminal term;
    ManualClock clock;
    Application app{term, clock};
    SpinApp spin{app};
};

}  // namespace

// --- The renderer, with nothing else running ---------------------------

CK_TEST(spin_renderer_paints_the_requested_background_behind_every_shape) {
    MeshLibrary meshes;
    Renderer renderer;
    for (const ShapeEntry& entry : shape_catalog()) {
        FrameSpec spec;
        spec.pixels = Size{240, 160};
        spec.background = kBackground;
        spec.yaw = 0.6;
        spec.pitch = 0.24;
        const ckv::Image frame = renderer.render(meshes.mesh(entry.id), spec);

        CK_CHECK(frame.width() == 240 && frame.height() == 160);
        // The corners are the surrounding surface, exactly as handed in:
        // an approximated background is a visible seam against the cells
        // around the picture.
        CK_CHECK(same_color(frame.pixel(0, 0), kBackground));
        CK_CHECK(same_color(frame.pixel(239, 159), kBackground));
        CK_CHECK(!painted_bounds(frame, kBackground).empty());
    }
}

CK_TEST(spin_renderer_stays_within_the_hosts_color_register_budget) {
    MeshLibrary meshes;
    Renderer renderer;
    for (const ShapeEntry& entry : shape_catalog()) {
        for (const int budget : {256, 64, 16}) {
            FrameSpec spec;
            spec.pixels = Size{200, 140};
            spec.background = kBackground;
            spec.yaw = 1.1;
            spec.pitch = 0.44;
            spec.color_budget = budget;
            const ckv::Image frame = renderer.render(meshes.mesh(entry.id), spec);
            // Exceeding the budget is what makes the Sixel encoder quantize
            // the whole picture — background included — to its fallback
            // color cube, so this bound is the seam's real guarantee.
            CK_CHECK(renderer.palette().size() <= budget);
            CK_CHECK(unique_colors(frame) <= static_cast<std::size_t>(budget));
            CK_CHECK(same_color(frame.pixel(0, 0), kBackground));
        }
    }
}

CK_TEST(spin_renderer_centres_the_object_at_any_frame_shape) {
    MeshLibrary meshes;
    Renderer renderer;
    for (const Size pixels : {Size{240, 160}, Size{90, 300}, Size{400, 96}}) {
        FrameSpec spec;
        spec.pixels = pixels;
        spec.background = kBackground;
        spec.yaw = 0.35;
        spec.pitch = 0.15;
        // A sphere is what pins centring exactly: its silhouette is a
        // circle about the projection of the model origin, whichever way it
        // is turned. A faceted solid's outline is legitimately lopsided
        // under perspective — the near corners project further out than the
        // far ones — so it could only pin centring to within its own
        // asymmetry.
        const ckv::Image frame = renderer.render(meshes.mesh(ShapeId::WireGlobe), spec);
        const Rect drawn = painted_bounds(frame, kBackground);
        CK_CHECK(!drawn.empty());
        CK_CHECK(std::abs(drawn.x - (pixels.width - drawn.right())) <= 2);
        CK_CHECK(std::abs(drawn.y - (pixels.height - drawn.bottom())) <= 2);
        // Framed against the shorter side, so the same object arrives the
        // same size in a wide window and a tall one.
        const int shorter = std::min(pixels.width, pixels.height);
        CK_CHECK(drawn.width <= shorter && drawn.height <= shorter);
        CK_CHECK(drawn.width >= shorter * 3 / 4);
    }
}

CK_TEST(spin_renderer_keeps_every_shape_clear_of_the_frame_edges) {
    MeshLibrary meshes;
    Renderer renderer;
    for (const ShapeEntry& entry : shape_catalog()) {
        for (const Size pixels : {Size{240, 160}, Size{90, 300}, Size{400, 96}}) {
            FrameSpec spec;
            spec.pixels = pixels;
            spec.background = kBackground;
            spec.yaw = 0.35;
            spec.pitch = 0.15;
            const ckv::Image frame = renderer.render(meshes.mesh(entry.id), spec);
            const Rect drawn = painted_bounds(frame, kBackground);
            CK_CHECK(!drawn.empty());
            // Nothing touches an edge: a rotating shape that reached the
            // frame would be clipped by the window it is sitting in.
            CK_CHECK(drawn.x > 0 && drawn.y > 0);
            CK_CHECK(drawn.right() < pixels.width && drawn.bottom() < pixels.height);
        }
    }
}

CK_TEST(spin_renderer_is_deterministic_for_the_same_request) {
    MeshLibrary meshes;
    Renderer first;
    Renderer second;
    FrameSpec spec;
    spec.pixels = Size{120, 80};
    spec.background = kBackground;
    spec.yaw = 2.0;
    spec.pitch = 0.8;
    const ckv::Image a = first.render(meshes.mesh(ShapeId::Torus), spec);
    // The same renderer again, so a reused scratch buffer cannot leak the
    // previous frame into the next one.
    (void)first.render(meshes.mesh(ShapeId::WireGlobe), spec);
    const ckv::Image b = first.render(meshes.mesh(ShapeId::Torus), spec);
    const ckv::Image c = second.render(meshes.mesh(ShapeId::Torus), spec);
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            CK_CHECK(same_color(a.pixel(x, y), b.pixel(x, y)));
            CK_CHECK(same_color(a.pixel(x, y), c.pixel(x, y)));
        }
}

// --- The application ---------------------------------------------------

CK_TEST(spin_example_opens_a_window_and_presents_its_frame_as_a_raster) {
    Fixture f;
    CK_CHECK(f.spin.open_windows() == 1);
    f.settle();

    SpinView* const view = f.spin.view_at(0);
    CK_CHECK(view != nullptr);
    CK_CHECK(view->frames_shown() == 1);
    CK_CHECK(view->image() != nullptr);
    CK_CHECK(!view->frame_in_flight());
    CK_CHECK(f.term.written_bytes().find("\x1B" "P") != std::string::npos);
    CK_CHECK(f.term.display().has_raster_pixels());
}

CK_TEST(spin_example_renders_at_the_windows_own_pixel_size_across_a_resize) {
    Fixture f;
    f.settle();
    SpinView* const view = f.spin.view_at(0);
    CK_CHECK(view->frame_pixels() == view->target_pixels());

    ckv::widgets::Window* const window = f.spin.desktop().windows().front();
    const Size before = view->target_pixels();
    window->set_bounds(Rect{2, 2, window->bounds().width + 9, window->bounds().height + 3});
    CK_CHECK(!(view->target_pixels() == before));

    view->request_frame(f.spin.frames(), f.clock.now_nanos());
    f.settle();
    CK_CHECK(view->frames_shown() == 2);
    CK_CHECK(view->frame_pixels() == view->target_pixels());
}

CK_TEST(spin_example_paints_frames_on_the_windows_own_background) {
    Fixture f;
    f.settle();
    SpinView* const view = f.spin.view_at(0);

    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(f.app.roles());
    const ckv::Color expected =
        resolved_color(f.app.theme().resolve(roles.window_frame_active).bg, ckv::Color::rgb(0, 0, 0));
    const ckv::Image::Rgba surface = view->surface_color();
    CK_CHECK(surface.r == expected.r() && surface.g == expected.g() && surface.b == expected.b());
    // And that colour is what the frame's own edge is painted in, so the
    // picture ends where the window's cells carry on.
    CK_CHECK(same_color(view->image()->pixel(0, 0), surface));
}

CK_TEST(spin_example_keeps_exactly_one_frame_in_flight_per_window) {
    Fixture f;
    SpinView* const view = f.spin.view_at(0);
    // Opening the window already asked for a frame, and nothing has
    // delivered it yet.
    CK_CHECK(view->frame_in_flight());

    for (int extra = 0; extra < 5; ++extra) {
        f.clock.advance(ckv::spin::kFrameIntervalNanos);
        view->request_frame(f.spin.frames(), f.clock.now_nanos());
    }
    f.spin.frames().wait_until_idle();
    // Five ticks arriving while one frame was outstanding produced no work
    // at all: a slow host renders fewer frames, never a backlog of stale
    // ones.
    CK_CHECK(f.spin.frames().frames_rendered() == 1);

    f.app.step(f.clock.now_nanos());
    CK_CHECK(!view->frame_in_flight());
    CK_CHECK(view->frames_shown() == 1);
}

CK_TEST(spin_example_animation_clock_runs_only_while_a_window_is_open) {
    Fixture f;
    f.settle();
    CK_CHECK(f.spin.animating());
    CK_CHECK(f.app.next_timer_deadline_nanos().has_value());

    (void)f.spin.desktop().windows().front()->close();
    // The window detaches through the ordinary posted self-detach, so one
    // step completes the close.
    f.app.step(f.clock.now_nanos());
    CK_CHECK(f.spin.open_windows() == 0);
    CK_CHECK(!f.spin.animating());
    CK_CHECK(!f.app.next_timer_deadline_nanos().has_value());

    (void)f.spin.open_window(ShapeId::Torus);
    CK_CHECK(f.spin.animating());
    CK_CHECK(f.spin.view_at(0)->shape() == ShapeId::Torus);
}

CK_TEST(spin_example_advances_the_shape_between_frames) {
    Fixture f;
    f.settle();
    SpinView* const view = f.spin.view_at(0);
    const std::shared_ptr<const ckv::Image> first = view->image();

    f.clock.advance(500'000'000);
    view->request_frame(f.spin.frames(), f.clock.now_nanos());
    f.settle();
    const std::shared_ptr<const ckv::Image> second = view->image();

    CK_CHECK(first != second);
    CK_CHECK(first->width() == second->width());
    bool differs = false;
    for (int y = 0; y < first->height() && !differs; ++y)
        for (int x = 0; x < first->width(); ++x)
            if (!same_color(first->pixel(x, y), second->pixel(x, y))) {
                differs = true;
                break;
            }
    CK_CHECK(differs);
}

CK_TEST(spin_example_opens_one_window_per_catalog_entry_from_its_commands) {
    Fixture f;
    for (const ShapeEntry& entry : shape_catalog()) {
        const ckv::ui::CommandId command = f.spin.new_window_command(entry.id);
        CK_CHECK(command != ckv::ui::kInvalidCommand);
        // Declared under its documented key, so a script or a test that
        // does not hold the application object can still reach it.
        CK_CHECK(f.app.commands().id_for(entry.command_key) == command);
        CK_CHECK(f.app.execute_command(command));
    }
    CK_CHECK(f.spin.open_windows() == 1 + ckv::spin::kShapeCount);
    f.settle();
    for (std::size_t index = 0; index < f.spin.open_windows(); ++index)
        CK_CHECK(f.spin.view_at(index)->frames_shown() == 1);
}

// --- The frame readout ------------------------------------------------

CK_TEST(spin_readout_shows_the_delivered_rate_on_the_windows_bottom_border) {
    Fixture f;
    f.settle();
    // One frame is not a rate: two frames far enough apart are.
    CK_CHECK(f.spin.view_at(0)->frames_per_second() == 0.0);
    CK_CHECK(f.display_row(f.spin.desktop().windows().front()->bounds().bottom() - 1).find("-- fps") !=
             std::string::npos);

    f.deliver_frame_after(100'000'000);
    CK_CHECK(std::abs(f.spin.view_at(0)->frames_per_second() - 10.0) < 0.001);

    const ckv::widgets::Window* const window = f.spin.desktop().windows().front();
    const ckv::spin::FrameReadout* const readout = f.spin.readout_at(0);
    // On the bottom border row of its own window, and standing clear of
    // the corner rather than crowding the resize grip that lives there.
    CK_CHECK(readout->bounds().y == window->bounds().height - 1);
    CK_CHECK(readout->bounds().right() == window->bounds().width - 4);
    // And the number is actually on that row of the terminal.
    CK_CHECK(f.display_row(window->bounds().bottom() - 1).find("10 fps") != std::string::npos);
}

CK_TEST(spin_readout_sits_in_the_border_with_one_space_on_each_side) {
    Fixture f;
    f.settle();
    f.deliver_frame_after(100'000'000);
    const ckv::widgets::Window* const window = f.spin.desktop().windows().front();
    const std::string border = f.display_row(window->bounds().bottom() - 1);

    // Set into the line the way a window's own title is: one space, the
    // text, one space — and border either side of that, never a wider gap.
    const std::size_t at = border.find(" 10 fps ");
    CK_CHECK(at != std::string::npos);
    CK_CHECK(border.find("  10 fps") == std::string::npos);
    CK_CHECK(border.substr(at - 1, 1) != " ");
    CK_CHECK(border.substr(at + 8, 1) != " ");
}

CK_TEST(spin_readout_keeps_one_width_however_many_digits_it_shows) {
    Fixture f;
    f.settle();
    const ckv::spin::FrameReadout* const readout = f.spin.readout_at(0);
    const Rect placed = readout->bounds();
    CK_CHECK(readout->horizontal_size_hint().preferred == placed.width);

    f.deliver_frame_after(100'000'000);
    const std::string ten = f.display_row(f.spin.desktop().windows().front()->bounds().bottom() - 1);
    // Four frames at a quarter of the rate: enough for the smoothed value
    // to reach a number of a different width.
    for (int frame = 0; frame < 8; ++frame) f.deliver_frame_after(400'000'000);
    const std::string slower = f.display_row(f.spin.desktop().windows().front()->bounds().bottom() - 1);

    CK_CHECK(ten.find("10 fps") != std::string::npos);
    CK_CHECK(slower.find("3 fps") != std::string::npos);
    // A readout repainted with the frame it describes, in a box that never
    // moved: the border around it is identical, so nothing was re-laid-out
    // to show a shorter number.
    CK_CHECK(!(ten == slower));
    CK_CHECK(readout->bounds() == placed);
}

CK_TEST(spin_help_menu_offers_about_rather_than_an_item_named_after_its_menu) {
    Fixture f;
    f.settle();
    // F10 activates the bar on File; Left wraps to the last menu, Help.
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F10, ckv::Modifier::None, ""}});
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Left, ckv::Modifier::None, ""}});
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
    f.app.step(f.clock.now_nanos());

    bool offers_about = false;
    bool repeats_the_menu_title = false;
    for (int row = 1; row < 30; ++row) {
        const std::string text = f.display_row(row);
        offers_about = offers_about || text.find("About...") != std::string::npos;
        // The standard help command is titled "Help", which is the right
        // name for a command and the wrong name for the one item in a menu
        // already called Help.
        repeats_the_menu_title = repeats_the_menu_title || text.find("Help") != std::string::npos;
    }
    CK_CHECK(offers_about);
    CK_CHECK(!repeats_the_menu_title);
}

// --- Pacing ------------------------------------------------------------

CK_TEST(spin_widens_its_frame_interval_when_ticks_arrive_late) {
    Fixture f;
    f.settle();
    CK_CHECK(f.spin.frame_interval_nanos() == ckv::spin::kFrameIntervalNanos);

    // The first tick only establishes when ticks started arriving.
    f.run_for(ckv::spin::kFrameIntervalNanos);
    CK_CHECK(f.spin.frame_interval_nanos() == ckv::spin::kFrameIntervalNanos);

    // A tick 400 ms after the previous one is a loop that was busy with
    // something else for 400 ms.
    f.run_for(400'000'000);
    CK_CHECK(f.spin.frame_interval_nanos() == 400'000'000);

    // Ticks landing on time again ease it back, geometrically rather than
    // in one jump.
    const std::int64_t backed_off = f.spin.frame_interval_nanos();
    f.run_for(backed_off);
    CK_CHECK(f.spin.frame_interval_nanos() < backed_off);
    for (int tick = 0; tick < 40; ++tick) f.run_for(f.spin.frame_interval_nanos());
    CK_CHECK(f.spin.frame_interval_nanos() == ckv::spin::kFrameIntervalNanos);
}

CK_TEST(spin_frame_interval_stops_widening_at_its_documented_floor) {
    Fixture f;
    f.settle();
    f.run_for(ckv::spin::kFrameIntervalNanos);
    f.run_for(30'000'000'000);  // thirty seconds: a suspended host, not a slow one
    CK_CHECK(f.spin.frame_interval_nanos() == ckv::spin::kSlowestFrameIntervalNanos);
}

CK_TEST(spin_never_owes_the_loop_a_backlog_of_ticks) {
    Fixture f;
    f.settle();
    // However long the loop was away, the tick it arms next is always in
    // the future. A repeating timer reschedules from its own fire time and
    // would instead come due once per missed interval, all at once.
    for (const std::int64_t stall : {5'000'000'000LL, 900'000'000LL, 120'000'000LL}) {
        f.run_for(stall);
        CK_CHECK(f.app.next_timer_deadline_nanos().has_value());
        CK_CHECK(*f.app.next_timer_deadline_nanos() > f.clock.now_nanos());
    }
}

CK_TEST(spin_keeps_at_most_one_outstanding_frame_per_open_window) {
    Fixture f;
    (void)f.spin.open_window(ShapeId::Torus);
    (void)f.spin.open_window(ShapeId::WireGlobe);
    CK_CHECK(f.spin.open_windows() == 3);

    // Twenty ticks with nothing delivered in between: every one of them
    // finds all three windows already waiting for a frame, and asks for
    // nothing.
    for (int tick = 0; tick < 20; ++tick) {
        f.clock.advance(ckv::spin::kFrameIntervalNanos);
        for (std::size_t index = 0; index < f.spin.open_windows(); ++index)
            f.spin.view_at(index)->request_frame(f.spin.frames(), f.clock.now_nanos());
    }
    f.spin.frames().wait_until_idle();
    CK_CHECK(f.spin.frames().frames_rendered() == f.spin.open_windows());
}

// --- What the host will accept -----------------------------------------

CK_TEST(spin_renders_within_a_reported_maximum_sixel_geometry) {
    Fixture f;
    f.settle();
    SpinView* const view = f.spin.view_at(0);
    const Size unlimited = view->target_pixels();
    CK_CHECK(unlimited.width > 320);

    // A host that answers XTSMGRAPHICS with a limit refuses anything
    // larger outright — the window would show the cell fallback where a
    // picture belongs.
    ckv::term::Capabilities caps = f.term.capabilities();
    caps.sixel_max_geometry = Size{320, 240};
    f.term.inject_capability_change(caps);
    f.app.step(f.clock.now_nanos());

    const Size limited = view->target_pixels();
    CK_CHECK(limited.width <= 320 && limited.height <= 240);
    CK_CHECK(limited.width > 0 && limited.height > 0);
    // Smaller, but still the picture the window asked for: the cell box it
    // is shown in is unchanged, so the terminal layer spreads it over the
    // whole window rather than leaving a margin.
    const Rect box = view->bounds();
    CK_CHECK(ckv::widgets::fit_image_cells(limited, f.term.capabilities().cell_pixels,
                                           Size{box.width, box.height}) ==
             (Size{box.width, box.height}));

    view->request_frame(f.spin.frames(), f.clock.now_nanos());
    f.settle();
    CK_CHECK(view->frame_pixels() == limited);
    CK_CHECK(f.term.display().has_raster_pixels());
}

CK_TEST(spin_about_box_reports_what_this_terminal_said_about_pictures) {
    Fixture f;
    f.settle();
    ckv::term::Capabilities caps = f.term.capabilities();
    caps.sixel_color_registers = 256;
    caps.sixel_max_geometry = Size{1000, 1000};
    f.term.inject_capability_change(caps);
    f.app.step(f.clock.now_nanos());
    const std::string summary = f.spin.graphics_summary();
    CK_CHECK(summary.find("draws Sixel graphics") != std::string::npos);
    CK_CHECK(summary.find("256 colour registers") != std::string::npos);
    CK_CHECK(summary.find("1000x1000") != std::string::npos);

    Fixture without{ckv::term::headless_no_graphics_profile()};
    without.settle();
    // The question a reader asks when a window shows "[image]" is whether
    // the application is broken or the host cannot show a picture, and the
    // answer belongs where they are already looking.
    CK_CHECK(without.spin.graphics_summary().find("no Sixel graphics") != std::string::npos);
}

CK_TEST(spin_about_dialog_carries_the_project_copyright) {
    Fixture f;
    f.settle();
    CK_CHECK(f.app.execute_command(f.app.commands().standard().help));
    f.app.step(f.clock.now_nanos());

    bool found = false;
    for (int row = 0; row < 30; ++row)
        found = found || f.display_row(row).find(
                               "Copyright (c) 2026 C. Klukas. All rights reserved.") !=
                               std::string::npos;
    CK_CHECK(found);
}

CK_TEST(spin_example_runs_the_same_object_graph_without_terminal_graphics) {
    Fixture f{ckv::term::headless_no_graphics_profile()};
    f.settle();
    SpinView* const view = f.spin.view_at(0);
    // The frame is still rendered — the application does not have a second
    // code path — and the view falls back to cells, on the window's own
    // surface rather than on a foreign one.
    CK_CHECK(view->frames_shown() == 1);
    CK_CHECK(f.term.written_bytes().find("[image]") != std::string::npos);
    CK_CHECK(f.term.written_bytes().find("\x1B" "P") == std::string::npos);
    CK_CHECK(!f.term.display().has_raster_pixels());
}

CK_TEST(spin_asks_for_nothing_while_the_terminal_has_not_finished_the_last_frame) {
    Fixture f;
    f.settle();
    CK_CHECK(f.app.frame_completion_tracking());
    // A frame has been presented and this terminal has not answered for it.
    CK_CHECK(f.app.frames_awaiting_terminal() >= 1U);

    const std::size_t rendered = f.spin.frames().frames_rendered();
    for (int tick = 0; tick < 5; ++tick) f.run_for(f.spin.frame_interval_nanos());
    // Five ticks, and not one of them asked for a frame: the terminal is
    // still behind, and the animation clock is still running.
    CK_CHECK(f.spin.frames().frames_rendered() == rendered);
    CK_CHECK(f.spin.animating());

    // It catches up, and the next tick asks again.
    f.term.inject_bytes("\x1B[0n", f.clock.now_nanos());
    f.run_for(f.spin.frame_interval_nanos());
    f.settle();
    CK_CHECK(f.spin.frames().frames_rendered() > rendered);
}

CK_TEST(spin_keeps_its_pixel_budget_even_when_the_terminal_reports_completion) {
    Fixture f;
    f.settle();
    SpinView* const view = f.spin.view_at(0);
    const Size pixels = view->frame_pixels();
    CK_CHECK(pixels.width > 0);
    const std::int64_t budgeted = f.spin.raster_paced_interval_nanos();
    CK_CHECK(budgeted > 0);
    const double budgeted_pixels =
        static_cast<double>(pixels.width) * pixels.height /
        (static_cast<double>(budgeted) * 1e-9);
    CK_CHECK(std::abs(budgeted_pixels - f.spin.raster_pixel_rate()) < f.spin.raster_pixel_rate() * 0.05);

    // The completion reply proves the frame was decoded, not drawn. Pacing
    // on it alone runs the animation at the host's decode-saturation rate,
    // which is measurably the rate its renderer starts losing pictures to
    // replacement races — so the budget stays in charge, and the reply
    // only ever slows things further.
    f.term.inject_bytes("\x1B[0n", f.clock.now_nanos());
    f.app.step(f.clock.now_nanos());
    CK_CHECK(f.app.last_terminal_round_trip_nanos() >= 0);
    CK_CHECK(f.spin.raster_paced_interval_nanos() == budgeted);
}

CK_TEST(spin_sends_no_picture_while_a_window_is_being_dragged_and_one_when_it_lands) {
    Fixture f;
    f.settle();
    ckv::widgets::Window* const window = f.spin.desktop().windows().front();
    window->set_bounds(Rect{2, 2, 40, 18});
    f.settle();

    const auto pictures_since = [&](std::size_t from) {
        const std::string written(f.term.written_bytes().substr(from));
        std::size_t count = 0, pos = 0;
        while ((pos = written.find("\x1B" "P", pos)) != std::string::npos) {
            ++count;
            pos += 2;
        }
        return count;
    };
    const auto pointer = [&](ckv::MouseAction action, int x) {
        f.app.dispatch(ckv::MouseEvent{action, ckv::MouseButton::Left, ckv::Point{x, 2}, std::nullopt,
                                       ckv::Modifier::None});
    };

    pointer(ckv::MouseAction::Down, 10);  // the title bar
    CK_CHECK(window->rasters_suppressed());
    const std::size_t before_drag = f.term.written_bytes().size();
    for (int step = 1; step <= 12; ++step) {
        pointer(ckv::MouseAction::Move, 10 + step);
        f.clock.advance(8'000'000);
        f.app.step(f.clock.now_nanos());
    }
    // Twelve positions, no pictures. A host pays for a picture by the pixel
    // and decodes it before it can draw anything behind it; a gesture that
    // sent one per position would spend all of that on pixels that are
    // wrong before they are drawn.
    CK_CHECK(pictures_since(before_drag) == 0U);
    CK_CHECK(window->bounds().x == 14);

    const std::size_t before_drop = f.term.written_bytes().size();
    pointer(ckv::MouseAction::Up, 22);
    f.clock.advance(8'000'000);
    f.app.step(f.clock.now_nanos());
    // And exactly one when it lands, at the position it landed on.
    CK_CHECK(!window->rasters_suppressed());
    CK_CHECK(pictures_since(before_drop) == 1U);
}

CK_TEST(spin_dragging_one_window_rests_every_windows_pictures_until_the_drop) {
    Fixture f;
    (void)f.spin.open_window(ShapeId::Torus);
    ckv::widgets::Window* const lower = f.spin.desktop().windows()[0];
    ckv::widgets::Window* const upper = f.spin.desktop().windows()[1];
    lower->set_bounds(Rect{2, 2, 40, 18});
    upper->set_bounds(Rect{50, 2, 40, 18});
    f.settle();

    const auto pictures_since = [&](std::size_t from) {
        const std::string written(f.term.written_bytes().substr(from));
        std::size_t count = 0, pos = 0;
        while ((pos = written.find("\x1B" "P", pos)) != std::string::npos) {
            ++count;
            pos += 2;
        }
        return count;
    };
    const auto pointer = [&](ckv::MouseAction action, int x) {
        f.app.dispatch(ckv::MouseEvent{action, ckv::MouseButton::Left, ckv::Point{x, 2}, std::nullopt,
                                       ckv::Modifier::None});
    };

    // Grab the UPPER window and drag it across the lower one. Every step
    // re-slices the lower window's picture — and every slice is a
    // replacement its host pays to decode — so the gesture rests both.
    pointer(ckv::MouseAction::Down, 60);
    CK_CHECK(upper->rasters_suppressed());
    CK_CHECK(lower->rasters_suppressed());
    const std::size_t during_mark = f.term.written_bytes().size();
    for (int step = 1; step <= 14; ++step) {
        pointer(ckv::MouseAction::Move, 60 - step * 3);
        f.clock.advance(8'000'000);
        f.app.step(f.clock.now_nanos());
    }
    CK_CHECK(pictures_since(during_mark) == 0U);

    const std::size_t drop_mark = f.term.written_bytes().size();
    pointer(ckv::MouseAction::Up, 60 - 14 * 3);
    f.clock.advance(8'000'000);
    f.app.step(f.clock.now_nanos());
    CK_CHECK(!upper->rasters_suppressed());
    CK_CHECK(!lower->rasters_suppressed());
    // Both pictures return at the drop — the upper whole, the lower as the
    // slices its new occluder leaves visible.
    CK_CHECK(pictures_since(drop_mark) >= 2U);
}
