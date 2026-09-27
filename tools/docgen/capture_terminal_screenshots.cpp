// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Captures the shipped TerminalApp and its owned child through the normal
// Application, Window backing, TerminalView, scene, and Presenter path.
// Deterministic private emulators supply the child teaching states.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/terminal_emulator.hpp"
#include "cvision/widgets/desktop.hpp"
#include "frame_svg.hpp"
#include "gallery_app.hpp"
#include "terminal_app.hpp"

namespace {

void write_svg(const std::filesystem::path& directory, const std::string& name,
               const ckv::term::VirtualDisplay& display) {
    std::ofstream out(directory / (name + ".svg"), std::ios::binary);
    out << ckv::docgen::render_virtual_display_svg(display);
    std::fprintf(stderr, "wrote %s (%dx%d cells, raster=%s)\n",
                 (directory / (name + ".svg")).c_str(), display.size().width, display.size().height,
                 display.has_raster_pixels() ? "yes" : "no");
}

void capture_profile(const std::filesystem::path& directory, std::string_view name,
                     ckv::term::Capabilities capabilities) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{100, 30}, capabilities);
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::term::TerminalEmulator* demo_session = nullptr;
    std::string demo_bytes;
    ckv::terminal_example::TerminalAppServices services;
    services.make_subsession = [&demo_session, &demo_bytes](ckv::term::TerminalLaunchSpec launch)
        -> std::unique_ptr<ckv::term::TerminalSubsession> {
        auto session = std::make_unique<ckv::term::TerminalEmulator>(launch.profile);
        if (launch.arguments.size() > 3 && launch.arguments[2] == "sixel-demo") {
            demo_bytes = launch.arguments[3];
            demo_session = session.get();
        } else {
            session->feed_output("ckvision$ ");
        }
        return session;
    };
    services.local_time = [] { return ckv::widgets::TimeValue{12, 34, 56}; };
    ckv::terminal_example::TerminalApp example(app, std::move(services));
    app.step(0);
    if (example.new_sixel_demo() == nullptr || demo_session == nullptr) std::exit(1);
    demo_session->feed_output("ckVision embedded terminal: Sixel from a child process\r\n\r\n");
    demo_session->feed_output(demo_bytes);
    demo_session->feed_output("ckvision$ ");
    app.root().notify_terminal_subsession_changed(*demo_session);
    app.step(0);
    if (terminal.display().has_raster_pixels() != capabilities.sixel_graphics) {
        std::fprintf(stderr, "terminal capture raster result did not match declared outer capability\n");
        std::exit(1);
    }
    write_svg(directory, std::string(name), terminal.display());
}

void capture_terminal_app_states(const std::filesystem::path& directory) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{100, 30}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::term::TerminalEmulator* child = nullptr;
    ckv::terminal_example::TerminalAppServices services;
    services.make_subsession = [&child](ckv::term::TerminalLaunchSpec launch)
        -> std::unique_ptr<ckv::term::TerminalSubsession> {
        auto session = std::make_unique<ckv::term::TerminalEmulator>(launch.profile);
        session->feed_output("ckvision$ ");
        child = session.get();
        return session;
    };
    services.local_time = [] { return ckv::widgets::TimeValue{12, 34, 56}; };
    ckv::terminal_example::TerminalApp example(app, std::move(services));
    app.step(0);
    if (child == nullptr) std::exit(1);
    write_svg(directory, "terminal-initial", terminal.display());
    for (const std::string_view scheme : {"dark", "light", "mono"}) {
        const auto command = app.commands().id_for("terminal.scheme." + std::string(scheme));
        if (!command || !app.commands().execute(*command)) std::exit(1);
        app.step(0);
        write_svg(directory, "terminal-initial-" + std::string(scheme), terminal.display());
    }
    const auto classic = app.commands().id_for("terminal.scheme.classic");
    if (!classic || !app.commands().execute(*classic)) std::exit(1);
    app.step(0);

    app.set_focus(example.desktop().top_dock());
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Down, ckv::Modifier::None, ""}});
    app.step(0);
    write_svg(directory, "terminal-menu", terminal.display());
    // Esc closes one level: the dropdown, then the bar walk.
    for (int level = 0; level < 2; ++level)
        app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Escape, ckv::Modifier::None, ""}});
    app.step(0);

    child->feed_output("\x1b[?1049h\x1b[2J\x1b[HckVision full-screen child\r\n\r\n"
                       "alternate buffer is private to this window");
    app.root().notify_terminal_subsession_changed(*child);
    app.step(0);
    write_svg(directory, "terminal-full-screen", terminal.display());
    const ckv::Size nested_size = child->snapshot().cells;
    ckv::term::HeadlessTerminal nested(nested_size, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock nested_clock;
    ckv::ui::Application nested_app(nested, nested_clock);
    ckv::gallery::GalleryApp gallery(nested_app);
    nested_app.step(0);
    child->feed_output("\x1b[?1049h\x1b[2J\x1b[H");
    child->feed_output(nested.written_bytes());
    app.root().notify_terminal_subsession_changed(*child);
    app.step(0);
    write_svg(directory, "terminal-nested", terminal.display());
}

void capture_nested_sixel(const std::filesystem::path& directory) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{100, 30}, ckv::term::headless_sixel_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::term::TerminalEmulator* child = nullptr;
    ckv::terminal_example::TerminalAppServices services;
    services.make_subsession = [&child](ckv::term::TerminalLaunchSpec launch)
        -> std::unique_ptr<ckv::term::TerminalSubsession> {
        auto session = std::make_unique<ckv::term::TerminalEmulator>(launch.profile);
        child = session.get();
        return session;
    };
    services.local_time = [] { return ckv::widgets::TimeValue{12, 34, 56}; };
    ckv::terminal_example::TerminalApp example(app, std::move(services));
    app.step(0);
    if (child == nullptr) std::exit(1);

    ckv::term::HeadlessTerminal nested(child->snapshot().cells, ckv::term::headless_sixel_profile());
    ckv::ManualClock nested_clock;
    ckv::ui::Application nested_app(nested, nested_clock);
    ckv::gallery::GalleryApp gallery(nested_app);
    nested_app.step(0);
    if (!nested.display().has_raster_pixels()) std::exit(1);

    child->feed_output("\x1b[?1049h\x1b[2J\x1b[H");
    child->feed_output(nested.written_bytes());
    app.root().notify_terminal_subsession_changed(*child);
    app.step(0);
    if (!terminal.display().has_raster_pixels()) std::exit(1);
    write_svg(directory, "terminal-nested-sixel", terminal.display());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);
    capture_terminal_app_states(out_dir);
    capture_nested_sixel(out_dir);
    capture_profile(out_dir, "terminal-sixel", ckv::term::headless_sixel_profile());
    capture_profile(out_dir, "terminal-no-graphics", ckv::term::headless_no_graphics_profile());
    return 0;
}
