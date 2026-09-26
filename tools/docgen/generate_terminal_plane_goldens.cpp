// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Portable, raw-byte fixtures from the actual VT/Sixel decode and presenter
// paths. The small text header fixes dimensions; the remaining bytes are
// row-major RGBA, so a matching hash cannot hide a different pixel plane.
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/scene/compositor.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/term/terminal_emulator.hpp"
#include "cvision/term/virtual_display.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/terminal_view.hpp"
#include "cvision/widgets/window.hpp"
#include "gallery_app.hpp"
#include "plane_capture.hpp"
#include "terminal_app.hpp"

namespace {

using namespace ckv;
using namespace ckv::term;

bool write_pixels(const std::filesystem::path& path, const Image& image) {
    return docgen::write_golden(path.parent_path(), path.filename().string(), docgen::rgba_plane_bytes(image));
}

bool write_cells(const std::filesystem::path& path, FrameView frame, CursorState cursor) {
    return docgen::write_golden(path.parent_path(), path.filename().string(),
                                docgen::frame_cells_dump(frame, cursor));
}

bool protocol_case(const std::filesystem::path& directory) {
    VirtualDisplay display(Size{4, 2}, PixelSize{4, 6});
    if (!display.write("\x1B[1;1Htext\x1B[2;2H\x1BP0;0;0q\"1;1;4;6#0;2;100;0;0~~~~\x1B\\"))
        return false;
    return write_pixels(directory / "protocol_sixel.rgba", display.raster_plane()) &&
           write_cells(directory / "protocol_sixel.dump", display.frame(), display.cursor());
}

bool embedded_child_case(const std::filesystem::path& directory) {
    TerminalCapabilityProfile profile = embedded_xterm_sixel_profile();
    profile.cells = Size{4, 3};
    profile.cell_pixels = PixelSize{4, 6};
    TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b[1;1Hchild\x1b[2;2H\x1bPq#0;2;100;0;0!8~-!8~\x1b\\");
    const TerminalSnapshot snapshot = emulator.snapshot();
    if (snapshot.rasters.size() != 1U || snapshot.rasters.front().image == nullptr ||
        snapshot.cell_buffer.size() != static_cast<std::size_t>(snapshot.cells.width * snapshot.cells.height))
        return false;
    scene::Surface cells(snapshot.cells, Cell{});
    for (int y = 0; y < snapshot.cells.height; ++y)
        for (int x = 0; x < snapshot.cells.width; ++x)
            cells.set_cell(Point{x, y}, snapshot.cell_buffer[static_cast<std::size_t>(y * snapshot.cells.width + x)]);
    return write_cells(directory / "embedded_child_sixel.dump", cells.view(), snapshot.cursor) &&
           write_pixels(directory / "embedded_child_sixel.rgba", *snapshot.rasters.front().image);
}

bool gallery_case(const std::filesystem::path& directory) {
    HeadlessTerminal sixel(Size{80, 24}, headless_sixel_profile());
    ManualClock sixel_clock;
    ui::Application sixel_app(sixel, sixel_clock);
    gallery::GalleryApp sixel_gallery(sixel_app);
    sixel_app.step(0);
    if (!sixel.display().has_raster_pixels() ||
        sixel.written_bytes().find("\x1B" "P") == std::string_view::npos ||
        !write_pixels(directory / "gallery_sixel.rgba", sixel.display().raster_plane()) ||
        !write_cells(directory / "gallery_sixel.dump", sixel.display().frame(),
                     sixel.display().cursor()))
        return false;

    HeadlessTerminal fallback(Size{80, 24}, headless_no_graphics_profile());
    ManualClock fallback_clock;
    ui::Application fallback_app(fallback, fallback_clock);
    gallery::GalleryApp fallback_gallery(fallback_app);
    fallback_app.step(0);
    return !fallback.display().has_raster_pixels() &&
           fallback.written_bytes().find("\x1B" "P") == std::string_view::npos &&
           write_cells(directory / "gallery_no_graphics.dump", fallback.display().frame(),
                       fallback.display().cursor());
}

bool terminal_app_cases(const std::filesystem::path& directory) {
    HeadlessTerminal terminal(Size{100, 30}, headless_no_graphics_profile());
    ManualClock clock;
    ui::Application app(terminal, clock);
    terminal_example::TerminalAppServices services;
    TerminalEmulator* child = nullptr;
    services.make_subsession = [&child](TerminalLaunchSpec launch) -> std::unique_ptr<TerminalSubsession> {
        auto session = std::make_unique<TerminalEmulator>(launch.profile);
        session->feed_output("ckvision$ ");
        child = session.get();
        return session;
    };
    services.local_time = [] { return widgets::TimeValue{12, 34, 56}; };
    terminal_example::TerminalApp example(app, std::move(services));
    app.step(0);
    if (child == nullptr || example.desktop().windows().size() != 1U ||
        example.desktop().windows().front()->title() != "Terminal 1" ||
        dynamic_cast<widgets::TerminalView*>(example.desktop().windows().front()->content()) == nullptr)
        return false;

    const auto record = [&](std::string_view name) {
        const VirtualDisplay& display = terminal.display();
        if (display.has_raster_pixels()) return false;
        return write_cells(directory / (std::string(name) + ".dump"), display.frame(), display.cursor()) &&
               write_pixels(directory / (std::string(name) + ".rgba"), display.raster_plane());
    };
    const auto contains = [&](std::string_view text) {
        const FrameView frame = terminal.display().frame();
        for (int y = 0; y < frame.size().height; ++y) {
            std::string row;
            for (int x = 0; x < frame.size().width; ++x)
                row += frame.at(Point{x, y}).grapheme();
            if (row.find(text) != std::string::npos) return true;
        }
        return false;
    };
    if (!record("terminal_initial")) return false;
    for (const std::string_view scheme : {"dark", "light", "mono"}) {
        const std::string key = "terminal.scheme." + std::string(scheme);
        const auto command = app.commands().id_for(key);
        if (!command || !app.commands().execute(*command)) return false;
        app.step(0);
        if (!record("terminal_initial_" + std::string(scheme))) return false;
    }
    const auto scheme = [&](std::string_view name) {
        const std::string key = "terminal.scheme." + std::string(name);
        const auto command = app.commands().id_for(key);
        if (!command || !app.commands().execute(*command)) return false;
        app.step(0);
        return true;
    };
    const auto record_state = [&](std::string_view name) {
        if (!scheme("classic") || !record(name)) return false;
        for (const std::string_view variant : {"dark", "light", "mono"}) {
            if (!scheme(variant) || !record(std::string(name) + "_" + std::string(variant)))
                return false;
        }
        return true;
    };

    child->feed_output("\x1b[?1049h\x1b[2J\x1b[HckVision full-screen child\r\n\r\n"
                       "alternate buffer is private to this window");
    app.root().notify_terminal_subsession_changed(*child);
    app.step(0);
    if (!contains("ckVision full-screen child") || !record_state("terminal_full_screen"))
        return false;

    const Size nested_size = child->snapshot().cells;
    HeadlessTerminal nested(nested_size, headless_no_graphics_profile());
    ManualClock nested_clock;
    ui::Application nested_app(nested, nested_clock);
    gallery::GalleryApp nested_gallery(nested_app);
    nested_app.step(0);
    if (nested.written_bytes().empty()) return false;
    child->feed_output("\x1b[?1049h\x1b[2J\x1b[H");
    child->feed_output(nested.written_bytes());
    app.root().notify_terminal_subsession_changed(*child);
    app.step(0);
    return contains("Controls") && contains("Sixel Demo") && record_state("terminal_nested");
}

bool terminal_nested_sixel_cases(const std::filesystem::path& directory) {
    HeadlessTerminal terminal(Size{100, 30}, headless_sixel_profile());
    ManualClock clock;
    ui::Application app(terminal, clock);
    TerminalEmulator* child = nullptr;
    terminal_example::TerminalAppServices services;
    services.make_subsession = [&child](TerminalLaunchSpec launch) -> std::unique_ptr<TerminalSubsession> {
        auto session = std::make_unique<TerminalEmulator>(launch.profile);
        child = session.get();
        return session;
    };
    services.local_time = [] { return widgets::TimeValue{12, 34, 56}; };
    terminal_example::TerminalApp example(app, std::move(services));
    app.step(0);
    if (child == nullptr || example.desktop().windows().size() != 1U) return false;

    HeadlessTerminal nested(child->snapshot().cells, headless_sixel_profile());
    ManualClock nested_clock;
    ui::Application nested_app(nested, nested_clock);
    gallery::GalleryApp nested_gallery(nested_app);
    nested_app.step(0);
    if (!nested.display().has_raster_pixels() ||
        nested.written_bytes().find("\x1bP") == std::string_view::npos)
        return false;

    child->feed_output("\x1b[?1049h\x1b[2J\x1b[H");
    child->feed_output(nested.written_bytes());
    app.root().notify_terminal_subsession_changed(*child);
    app.step(0);

    const auto record = [&](std::string_view name) {
        const VirtualDisplay& display = terminal.display();
        if (!display.has_raster_pixels()) return false;
        return write_cells(directory / (std::string(name) + ".dump"), display.frame(), display.cursor()) &&
               write_pixels(directory / (std::string(name) + ".rgba"), display.raster_plane());
    };
    if (!record("terminal_nested_sixel")) return false;
    for (const std::string_view scheme : {"dark", "light", "mono"}) {
        const auto command = app.commands().id_for("terminal.scheme." + std::string(scheme));
        if (!command || !app.commands().execute(*command)) return false;
        app.step(0);
        if (!record("terminal_nested_sixel_" + std::string(scheme))) return false;
    }
    return true;
}

bool presenter_cases(const std::filesystem::path& directory) {
    scene::Compositor compositor(Size{4, 1});
    scene::Surface background(Size{4, 1}, Cell::from_grapheme(" ", Style{}));
    scene::Surface raster_layer(Size{4, 1}, Cell::from_grapheme(" ", Style{}));
    auto image = std::make_shared<Image>(PixelSize{8, 18});
    for (int y = 0; y < image->height(); ++y)
        for (int x = 0; x < image->width(); ++x)
            image->set_pixel(x, y, x < 4 ? Image::Rgba{255, 0, 0, 255} : Image::Rgba{0, 0, 255, 255});
    scene::Painter painter(raster_layer, Rect{0, 0, 4, 1});
    painter.draw_image(Rect{0, 0, 4, 1}, 7, image, [](scene::Painter& fallback) {
        fallback.fill(Rect{0, 0, 4, 1}, Cell::from_grapheme("#", Style{}));
    });
    scene::Surface left(Size{1, 1}, Cell::from_grapheme("L", Style{}));
    scene::Surface right(Size{1, 1}, Cell::from_grapheme("R", Style{}));
    const std::vector<scene::Layer> layers{{1, &raster_layer, Point{0, 0}, false},
                                           {2, &left, Point{0, 0}, false},
                                           {3, &right, Point{3, 0}, false}};
    compositor.compose(layers, background);
    if (compositor.visible_rasters().size() != 1U ||
        !(compositor.visible_rasters().front().visible_rect == Rect{1, 0, 2, 1}))
        return false;

    HeadlessTerminal sixel(Size{4, 1}, headless_sixel_profile());
    Presenter sixel_presenter(sixel);
    sixel_presenter.present(compositor.frame().view(), CursorState{}, 0, compositor.visible_rasters());
    if (sixel.written_bytes().find("\x1BP0;0;0q") == std::string_view::npos ||
        !write_pixels(directory / "presenter_occlusion.rgba", sixel.display().raster_plane()) ||
        !write_cells(directory / "presenter_occlusion.dump", sixel.display().frame(),
                     sixel.display().cursor()))
        return false;

    HeadlessTerminal no_graphics(Size{4, 1}, headless_no_graphics_profile());
    Presenter fallback_presenter(no_graphics);
    fallback_presenter.present(compositor.frame().view(), CursorState{}, 0, compositor.visible_rasters());
    if (no_graphics.display().has_raster_pixels() ||
        no_graphics.written_bytes().find("\x1BP0;0;0q") != std::string_view::npos ||
        !write_pixels(directory / "presenter_no_graphics.rgba", no_graphics.display().raster_plane()) ||
        !write_cells(directory / "presenter_no_graphics.dump", no_graphics.display().frame(),
                     no_graphics.display().cursor()))
        return false;

    HeadlessTerminal moved(Size{4, 1}, headless_sixel_profile());
    Presenter moved_presenter(moved);
    scene::Surface blank(Size{4, 1}, Cell::from_grapheme(" ", Style{}));
    auto red = std::make_shared<Image>(PixelSize{9, 18});
    for (int y = 0; y < red->height(); ++y)
        for (int x = 0; x < red->width(); ++x)
            red->set_pixel(x, y, Image::Rgba{255, 0, 0, 255});
    moved_presenter.present(blank.view(), CursorState{}, 0,
                            {{8, Rect{0, 0, 1, 1}, Rect{0, 0, 1, 1}, red}});
    moved_presenter.present(blank.view(), CursorState{}, 0,
                            {{8, Rect{2, 0, 1, 1}, Rect{2, 0, 1, 1}, red}});
    return write_pixels(directory / "presenter_move.rgba", moved.display().raster_plane()) &&
           write_cells(directory / "presenter_move.dump", moved.display().frame(), moved.display().cursor());
}


bool contained_stage(const std::filesystem::path& directory, std::string_view name,
                     const HeadlessTerminal& terminal, const widgets::Window* child_window,
                     int expected_opaque_pixels) {
    const VirtualDisplay& display = terminal.display();
    if (display.has_raster_pixels() != (expected_opaque_pixels > 0)) {
        std::fprintf(stderr, "%.*s: raster presence differs from expected count %d\n",
                     static_cast<int>(name.size()), name.data(), expected_opaque_pixels);
        return false;
    }
    const Rect window = child_window != nullptr ? child_window->absolute_bounds() : Rect{};
    const Rect content = child_window != nullptr ? child_window->content_rect() : Rect{};
    const Rect clip{window.x + content.x, window.y + content.y,
                    content.width, content.height};
    const PixelSize cell_pixels = terminal.capabilities().cell_pixels;
    const Image& plane = display.raster_plane();
    int opaque_pixels = 0;
    for (int y = 0; y < plane.height(); ++y) {
        for (int x = 0; x < plane.width(); ++x) {
            if (plane.pixel(x, y).a == 0) continue;
            ++opaque_pixels;
            const Point cell{x / std::max(1, cell_pixels.width),
                             y / std::max(1, cell_pixels.height)};
            if (child_window == nullptr || !clip.contains(cell)) {
                std::fprintf(stderr, "%.*s: raster pixel (%d,%d) escaped content clip\n",
                             static_cast<int>(name.size()), name.data(), x, y);
                return false;
            }
        }
    }
    if (opaque_pixels != expected_opaque_pixels) {
        std::fprintf(stderr, "%.*s: %d opaque pixels, expected %d\n",
                     static_cast<int>(name.size()), name.data(), opaque_pixels,
                     expected_opaque_pixels);
        return false;
    }
    const std::string base(name);
    return write_pixels(directory / (base + ".rgba"), display.raster_plane()) &&
           write_cells(directory / (base + ".dump"), display.frame(), display.cursor());
}

bool contained_pair_stage(const std::filesystem::path& directory, std::string_view name,
                          const HeadlessTerminal& terminal,
                          const std::vector<const widgets::Window*>& windows,
                          std::array<int, 3> expected_colors) {
    std::vector<Rect> clips;
    clips.reserve(windows.size());
    for (const widgets::Window* window : windows) {
        const Rect bounds = window->absolute_bounds();
        const Rect content = window->content_rect();
        clips.push_back(Rect{bounds.x + content.x, bounds.y + content.y,
                             content.width, content.height});
    }
    const PixelSize cell_pixels = terminal.capabilities().cell_pixels;
    const VirtualDisplay& display = terminal.display();
    const Image& plane = display.raster_plane();
    std::array<int, 3> observed_colors{};
    for (int y = 0; y < plane.height(); ++y) {
        for (int x = 0; x < plane.width(); ++x) {
            const Image::Rgba pixel = plane.pixel(x, y);
            if (pixel.a == 0) continue;
            const Point cell{x / cell_pixels.width, y / cell_pixels.height};
            if (std::none_of(clips.begin(), clips.end(),
                             [cell](Rect clip) { return clip.contains(cell); }))
                return false;
            if (pixel.r == 255 && pixel.g == 0 && pixel.b == 0 && pixel.a == 255)
                ++observed_colors[0];
            else if (pixel.r == 0 && pixel.g == 255 && pixel.b == 0 && pixel.a == 255)
                ++observed_colors[1];
            else if (pixel.r == 0 && pixel.g == 0 && pixel.b == 255 && pixel.a == 255)
                ++observed_colors[2];
            else return false;
        }
    }
    if (observed_colors != expected_colors) return false;
    const std::string base(name);
    return write_pixels(directory / (base + ".rgba"), plane) &&
           write_cells(directory / (base + ".dump"), display.frame(), display.cursor());
}

bool contained_text_over_raster_case(const std::filesystem::path& directory) {
    Capabilities caps = headless_sixel_profile();
    caps.cell_pixels = PixelSize{4, 6};
    HeadlessTerminal terminal(Size{30, 12}, caps);
    ManualClock clock;
    ui::Application app(terminal, clock);
    const ui::StandardRoles roles = ui::intern_standard_roles(app.roles());
    widgets::ApplicationShell shell(app, {.theme = ui::make_classic_theme(app.roles(), roles)});
    TerminalCapabilityProfile profile = embedded_xterm_sixel_profile();
    profile.cells = Size{8, 4};
    profile.cell_pixels = caps.cell_pixels;
    TerminalEmulator child(profile);
    child.set_raster_identity(79);
    auto window = std::make_unique<widgets::Window>("text over picture");
    window->set_bounds(Rect{2, 2, 12, 7});
    window->set_content(std::make_unique<widgets::TerminalView>(child));
    const widgets::Window* const observer = shell.desktop().add_window(std::move(window));
    child.feed_output("\x1bPq#0;2;100;0;0!12~\x1b\\");
    app.root().notify_terminal_subsession_changed(child);
    app.step(0);
    child.feed_output("\x1b[1;2HX");
    app.root().notify_terminal_subsession_changed(child);
    app.step(0);

    const Rect bounds = observer->absolute_bounds();
    const Rect content = observer->content_rect();
    const Point origin{bounds.x + content.x, bounds.y + content.y};
    const VirtualDisplay& display = terminal.display();
    if (display.frame().at(Point{origin.x + 1, origin.y}).grapheme() != "X") return false;
    const Image& plane = display.raster_plane();
    const auto pixel = [&plane, caps, origin](int cell_x) {
        return plane.pixel((origin.x + cell_x) * caps.cell_pixels.width + caps.cell_pixels.width / 2,
                           origin.y * caps.cell_pixels.height + caps.cell_pixels.height / 2);
    };
    const Image::Rgba left = pixel(0);
    const Image::Rgba middle = pixel(1);
    const Image::Rgba right = pixel(2);
    if (left.r != 255 || left.g != 0 || left.b != 0 || left.a != 255 ||
        middle.a != 0 || right.r != 255 || right.g != 0 || right.b != 0 || right.a != 255)
        return false;
    return write_pixels(directory / "contained_text_over_raster.rgba", plane) &&
           write_cells(directory / "contained_text_over_raster.dump", display.frame(), display.cursor());
}

bool contained_pair_cases(const std::filesystem::path& directory) {
    Capabilities caps = headless_sixel_profile();
    caps.cell_pixels = PixelSize{4, 6};
    HeadlessTerminal terminal(Size{30, 12}, caps);
    ManualClock clock;
    ui::Application app(terminal, clock);
    const ui::StandardRoles roles = ui::intern_standard_roles(app.roles());
    widgets::ApplicationShell shell(
        app, {.theme = ui::make_classic_theme(app.roles(), roles)});
    TerminalCapabilityProfile profile = embedded_xterm_sixel_profile();
    profile.cells = Size{12, 5};
    profile.cell_pixels = caps.cell_pixels;
    TerminalEmulator red(profile);
    TerminalEmulator green(profile);
    TerminalEmulator blue(profile);
    red.set_raster_identity(90);
    green.set_raster_identity(91);
    blue.set_raster_identity(92);
    const auto add_window = [&shell](TerminalEmulator& child, std::string title, Rect bounds) {
        auto window = std::make_unique<widgets::Window>(std::move(title));
        window->set_bounds(bounds);
        window->set_content(std::make_unique<widgets::TerminalView>(child));
        widgets::Window* const observer = shell.desktop().add_window(std::move(window));
        observer->on_closed = [&shell, observer] { shell.desktop().remove_window(observer); };
        return observer;
    };
    widgets::Window* const red_window = add_window(red, "red", Rect{2, 2, 12, 7});
    widgets::Window* const green_window = add_window(green, "green", Rect{16, 2, 12, 7});
    red.feed_output("\x1bPq#0;2;100;0;0!48~\x1b\\");
    green.feed_output("\x1bPq#0;2;0;100;0!48~\x1b\\");
    app.root().notify_terminal_subsession_changed(red);
    app.root().notify_terminal_subsession_changed(green);
    app.step(0);
    if (!contained_pair_stage(directory, "contained_pair_initial", terminal,
                              {red_window, green_window}, {240, 240, 0})) return false;

    if (!red_window->close()) return false;
    app.step(0);
    if (!contained_pair_stage(directory, "contained_pair_peer_closed", terminal,
                              {green_window}, {0, 240, 0})) return false;

    widgets::Window* const blue_window = add_window(blue, "blue", Rect{2, 2, 12, 7});
    blue.feed_output("\x1bPq#0;2;0;0;100!48~\x1b\\");
    app.root().notify_terminal_subsession_changed(blue);
    app.step(0);
    return contained_pair_stage(directory, "contained_pair_reopened", terminal,
                                {green_window, blue_window}, {0, 240, 240});
}

bool contained_terminal_cases(const std::filesystem::path& directory) {
    Capabilities caps = headless_sixel_profile();
    caps.cell_pixels = PixelSize{4, 6};
    HeadlessTerminal terminal(Size{30, 12}, caps);
    ManualClock clock;
    ui::Application app(terminal, clock);
    const ui::StandardRoles roles = ui::intern_standard_roles(app.roles());
    widgets::ApplicationShell shell(
        app, {.theme = ui::make_classic_theme(app.roles(), roles)});
    TerminalCapabilityProfile profile = embedded_xterm_sixel_profile();
    profile.cells = Size{12, 5};
    profile.cell_pixels = caps.cell_pixels;
    TerminalEmulator child(profile);
    child.set_raster_identity(79);
    auto window = std::make_unique<widgets::Window>("contained");
    window->set_bounds(Rect{2, 2, 12, 7});
    window->set_content(std::make_unique<widgets::TerminalView>(child));
    widgets::Window* const observer = shell.desktop().add_window(std::move(window));
    observer->on_closed = [&shell, observer] { shell.desktop().remove_window(observer); };
    child.feed_output("\x1bPq#0;2;100;0;0!48~\x1b\\");
    app.root().notify_terminal_subsession_changed(child);
    app.step(0);
    if (terminal.written_bytes().find("\x1bP") == std::string_view::npos ||
        !contained_stage(directory, "contained_initial", terminal, observer, 240)) return false;

    const auto themed_stage = [&](auto factory, std::string_view name) {
        app.set_theme(factory(app.roles(), roles));
        app.step(0);
        return contained_stage(directory, name, terminal, observer, 240);
    };
    if (!themed_stage(ui::make_dark_theme, "contained_initial_dark") ||
        !themed_stage(ui::make_light_theme, "contained_initial_light") ||
        !themed_stage(ui::make_mono_theme, "contained_initial_mono")) return false;
    app.set_theme(ui::make_classic_theme(app.roles(), roles));
    app.step(0);

    observer->set_bounds(Rect{8, 3, 12, 7});
    app.step(0);
    if (!contained_stage(directory, "contained_move", terminal, observer, 240)) return false;

    observer->set_bounds(Rect{8, 3, 18, 9});
    app.step(0);
    if (!contained_stage(directory, "contained_resize", terminal, observer, 240)) return false;

    // A higher window's right-hand shadow crosses the child picture without
    // its body occluding it. Raster pixels take the theme's shadow, as cells
    // do -- Classic's maps them into the black-to-dark-grey range (D-106) --
    // and a second coincident shadow must not darken them twice.
    const Image unshadowed = terminal.display().raster_plane();
    auto shadowing = std::make_unique<widgets::Window>("shadow");
    shadowing->set_bounds(Rect{5, 2, 4, 4});
    widgets::Window* const shadow_observer = shell.desktop().add_window(std::move(shadowing));
    shadow_observer->on_closed = [&shell, shadow_observer] {
        shell.desktop().remove_window(shadow_observer);
    };
    app.step(0);
    if (!contained_stage(directory, "contained_shadow", terminal, observer, 240)) return false;
    const Image shadow_plane = terminal.display().raster_plane();
    const ShadowStyle shadow = app.theme().shadow();
    // A Sixel colour travels in whole percent, so a darkened pixel arrives
    // within a channel step or two of the value the shadow computed.
    const auto close_enough = [](std::uint8_t shown, std::uint8_t expected) {
        return shown + 2 >= expected && shown <= expected + 2;
    };
    int dimmed = 0;
    int unchanged = 0;
    for (int y = 0; y < shadow_plane.height(); ++y) {
        for (int x = 0; x < shadow_plane.width(); ++x) {
            const Image::Rgba before = unshadowed.pixel(x, y);
            const Image::Rgba after = shadow_plane.pixel(x, y);
            const Image::Rgba darkened = shadow.apply(before);
            if (before.a == 0) {
                if (after.a != 0) return false;
            } else if (close_enough(after.r, darkened.r) && close_enough(after.g, darkened.g) && close_enough(after.b, darkened.b) &&
                       after.a == before.a && !(after.r == before.r && after.g == before.g && after.b == before.b)) {
                ++dimmed;
            } else if (after.r == before.r && after.g == before.g &&
                       after.b == before.b && after.a == before.a) {
                ++unchanged;
            } else {
                return false;
            }
        }
    }
    if (dimmed == 0 || unchanged == 0) return false;

    auto second_shadow = std::make_unique<widgets::Window>("shadow");
    second_shadow->set_bounds(Rect{5, 2, 4, 4});
    widgets::Window* const second_shadow_observer = shell.desktop().add_window(std::move(second_shadow));
    second_shadow_observer->on_closed = [&shell, second_shadow_observer] {
        shell.desktop().remove_window(second_shadow_observer);
    };
    app.step(0);
    if (!contained_stage(directory, "contained_shadow_union", terminal, observer, 240) ||
        !std::equal(shadow_plane.data(), shadow_plane.data() + shadow_plane.stride() * shadow_plane.height(),
                    terminal.display().raster_plane().data())) return false;
    if (!second_shadow_observer->close() || !shadow_observer->close()) return false;
    app.step(0);
    const Image& restored_plane = terminal.display().raster_plane();
    if (!contained_stage(directory, "contained_unshadow", terminal, observer, 240) ||
        !std::equal(unshadowed.data(), unshadowed.data() + unshadowed.stride() * unshadowed.height(),
                    restored_plane.data())) return false;

    auto occluder = std::make_unique<widgets::Window>("occluder");
    occluder->set_bounds(Rect{10, 2, 8, 6});
    widgets::Window* const occluder_observer = shell.desktop().add_window(std::move(occluder));
    app.step(0);
    if (!contained_stage(directory, "contained_overlap", terminal, observer, 48)) return false;
    const Rect blocked = occluder_observer->absolute_bounds();
    const Image& plane = terminal.display().raster_plane();
    for (int y = blocked.y * caps.cell_pixels.height;
         y < blocked.bottom() * caps.cell_pixels.height; ++y) {
        for (int x = blocked.x * caps.cell_pixels.width;
             x < blocked.right() * caps.cell_pixels.width; ++x) {
            if (plane.pixel(x, y).a != 0) return false;
        }
    }

    child.feed_output("\x1b[2J");
    app.root().notify_terminal_subsession_changed(child);
    app.step(0);
    if (!contained_stage(directory, "contained_clear", terminal, observer, 0)) return false;

    child.feed_output("\x1bPq#0;2;100;0;0!48~\x1b\\");
    app.root().notify_terminal_subsession_changed(child);
    app.step(0);
    if (!contained_stage(directory, "contained_repaint", terminal, observer, 96)) return false;

    if (!observer->close()) return false;
    app.step(0);
    if (!contained_stage(directory, "contained_close", terminal, nullptr, 0)) return false;
    shell.desktop().remove_window(occluder_observer);
    app.step(0);

    // Exercise a separate child's retained raster through several presented
    // sizes and a burst of bounds changes before one frame. Keeping this
    // sequence separate preserves the earlier overlap/clear/repaint oracles.
    TerminalEmulator storm_child(profile);
    storm_child.set_raster_identity(80);
    auto storm_window = std::make_unique<widgets::Window>("contained");
    storm_window->set_bounds(Rect{2, 2, 12, 7});
    storm_window->set_content(std::make_unique<widgets::TerminalView>(storm_child));
    widgets::Window* const storm_observer = shell.desktop().add_window(std::move(storm_window));
    storm_observer->on_closed = [&shell, storm_observer] { shell.desktop().remove_window(storm_observer); };
    storm_child.feed_output("\x1bPq#0;2;100;0;0!48~\x1b\\");
    app.root().notify_terminal_subsession_changed(storm_child);
    app.step(0);
    if (!contained_stage(directory, "contained_storm_initial", terminal, storm_observer, 240))
        return false;

    storm_observer->set_bounds(Rect{8, 3, 9, 6});
    app.step(0);
    if (!contained_stage(directory, "contained_storm_shrink", terminal, storm_observer, 168))
        return false;

    storm_observer->set_bounds(Rect{3, 4, 14, 7});
    app.step(0);
    if (!contained_stage(directory, "contained_storm_move", terminal, storm_observer, 240))
        return false;

    storm_observer->set_bounds(Rect{8, 3, 18, 9});
    app.step(0);
    if (!contained_stage(directory, "contained_storm_restore", terminal, storm_observer, 240))
        return false;

    storm_observer->set_bounds(Rect{6, 2, 10, 7});
    storm_observer->set_bounds(Rect{4, 3, 9, 6});
    storm_observer->set_bounds(Rect{8, 3, 18, 9});
    app.step(0);
    if (!contained_stage(directory, "contained_storm_burst", terminal, storm_observer, 240))
        return false;
    if (!storm_observer->close()) return false;
    app.step(0);

    Capabilities fallback_caps = headless_no_graphics_profile();
    fallback_caps.cell_pixels = caps.cell_pixels;
    HeadlessTerminal fallback(Size{30, 12}, fallback_caps);
    ManualClock fallback_clock;
    ui::Application fallback_app(fallback, fallback_clock);
    const ui::StandardRoles fallback_roles = ui::intern_standard_roles(fallback_app.roles());
    widgets::ApplicationShell fallback_shell(
        fallback_app, {.theme = ui::make_classic_theme(fallback_app.roles(), fallback_roles)});
    TerminalEmulator fallback_child(profile);
    fallback_child.set_raster_identity(79);
    auto fallback_window = std::make_unique<widgets::Window>("contained");
    fallback_window->set_bounds(Rect{2, 2, 12, 7});
    fallback_window->set_content(std::make_unique<widgets::TerminalView>(fallback_child));
    widgets::Window* const fallback_observer =
        fallback_shell.desktop().add_window(std::move(fallback_window));
    fallback_child.feed_output("\x1bPq#0;2;100;0;0!48~\x1b\\");
    fallback_app.root().notify_terminal_subsession_changed(fallback_child);
    fallback_app.step(0);
    if (fallback.written_bytes().find("\x1bP") != std::string_view::npos ||
        !contained_stage(directory, "contained_no_graphics", fallback,
                         fallback_observer, 0)) return false;
    const auto fallback_themed_stage = [&](auto factory, std::string_view name) {
        fallback_app.set_theme(factory(fallback_app.roles(), fallback_roles));
        fallback_app.step(0);
        return contained_stage(directory, name, fallback, fallback_observer, 0);
    };
    return fallback_themed_stage(ui::make_dark_theme, "contained_no_graphics_dark") &&
           fallback_themed_stage(ui::make_light_theme, "contained_no_graphics_light") &&
           fallback_themed_stage(ui::make_mono_theme, "contained_no_graphics_mono") &&
           fallback.written_bytes().find("\x1bP") == std::string_view::npos;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);
    if (!protocol_case(directory) || !embedded_child_case(directory) ||
        !gallery_case(directory) || !terminal_app_cases(directory) ||
        !terminal_nested_sixel_cases(directory) ||
        !presenter_cases(directory) || !contained_terminal_cases(directory) ||
        !contained_pair_cases(directory) || !contained_text_over_raster_case(directory)) {
        std::fprintf(stderr, "terminal plane capture failed\n");
        return 1;
    }
    return 0;
}
