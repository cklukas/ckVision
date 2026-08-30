// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Captures the contained-terminal view through its normal Application,
// Window backing, TerminalView, scene, and Presenter path. The deterministic
// private emulator supplies the exact child teaching states; POSIX tests cover
// the same Sixel bytes from a real private PTY.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/terminal_emulator.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "frame_svg.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/terminal_view.hpp"
#include "cvision/widgets/window.hpp"
#include "terminal_app.hpp"

namespace {

enum class ThemeKind { Classic, Dark, Light, Mono };

ckv::ui::Theme make_theme(const ckv::ui::RoleRegistry& registry,
                          const ckv::ui::StandardRoles& roles, ThemeKind kind) {
    switch (kind) {
        case ThemeKind::Dark: return ckv::ui::make_dark_theme(registry, roles);
        case ThemeKind::Light: return ckv::ui::make_light_theme(registry, roles);
        case ThemeKind::Mono: return ckv::ui::make_mono_theme(registry, roles);
        case ThemeKind::Classic: return ckv::ui::make_classic_theme(registry, roles);
    }
    return ckv::ui::make_classic_theme(registry, roles);
}

void write_svg(const std::filesystem::path& directory, const std::string& name,
               const ckv::term::VirtualDisplay& display) {
    std::ofstream out(directory / (name + ".svg"), std::ios::binary);
    out << ckv::docgen::render_virtual_display_svg(display);
    std::fprintf(stderr, "wrote %s (%dx%d cells, raster=%s)\n",
                 (directory / (name + ".svg")).c_str(), display.size().width, display.size().height,
                 display.has_raster_pixels() ? "yes" : "no");
}

void capture_profile(const std::filesystem::path& directory, std::string_view name,
                     ckv::term::Capabilities capabilities, bool child_sixel,
                     ThemeKind theme_kind = ThemeKind::Classic) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{100, 30}, capabilities);
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    ckv::widgets::ApplicationShell shell(
        app, {.theme = make_theme(app.roles(), roles, theme_kind)});
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cell_pixels = capabilities.cell_pixels;
    ckv::term::TerminalEmulator session(profile);
    session.set_raster_identity(4'901);
    // ckvision-doc: terminalview
    auto window = std::make_unique<ckv::widgets::Window>(child_sixel ? "Sixel Demo" : "Terminal 1");
    window->set_bounds(ckv::Rect{2, 2, 76, 20});
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    window->set_content(std::move(view));
    shell.desktop().add_window(std::move(window));
    // ckvision-doc-end: terminalview

    if (child_sixel)
        session.feed_output("\x1bPq#0;2;100;0;0!32~-!32~-!32~-!32~-!32~-!32~\x1b\\ckvision$ ");
    else
        session.feed_output("ckvision$ ");
    app.root().notify_terminal_subsession_changed(session);
    app.step(0);
    if (terminal.display().has_raster_pixels() != (child_sixel && capabilities.sixel_graphics)) {
        std::fprintf(stderr, "terminal capture raster result did not match declared outer capability\n");
        std::exit(1);
    }
    write_svg(directory, std::string(name), terminal.display());
}

void capture_menu(const std::filesystem::path& directory) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{100, 30}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    const ckv::ui::CommandId new_terminal = app.commands().declare(
        {.key = "docgen.new-terminal", .title = "&New Terminal", .category = "File", .handler = [] {}});
    const ckv::ui::CommandId new_sixel_demo = app.commands().declare(
        {.key = "docgen.new-sixel-demo", .title = "New &Sixel Demo", .category = "File", .handler = [] {}});
    const ckv::ui::StandardCommands& standard = app.commands().standard();
    ckv::widgets::ApplicationShell shell(
        app, {.theme = ckv::ui::make_classic_theme(app.roles(), roles),
              .menus = {
                  {"&File", {ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{new_terminal}),
                              ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{new_sixel_demo}),
                              ckv::widgets::MenuItem::separator(),
                              ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{standard.quit})}},
                  {"&Window", {ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{standard.next_window}),
                                ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{standard.previous_window}),
                                ckv::widgets::MenuItem::separator(),
                                ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{standard.tile}),
                                ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{standard.cascade})}},
              }});
    ckv::term::TerminalEmulator session(ckv::term::embedded_xterm_sixel_profile());
    session.feed_output("ckvision$ printf 'terminal ready'\r\nterminal ready\r\nckvision$ ");
    auto window = std::make_unique<ckv::widgets::Window>("Terminal 1");
    window->set_bounds(ckv::Rect{2, 2, 76, 20});
    window->set_content(std::make_unique<ckv::widgets::TerminalView>(session));
    shell.desktop().add_window(std::move(window));
    app.root().notify_terminal_subsession_changed(session);
    app.step(0);
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F10, ckv::Modifier::None, ""}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
    app.step(0);
    write_svg(directory, "terminal-menu", terminal.display());
}

void capture_full_screen(const std::filesystem::path& directory) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{100, 30}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cell_pixels = terminal.capabilities().cell_pixels;
    ckv::term::TerminalEmulator session(profile);
    session.feed_output("\x1b[?1049h\x1b[2J\x1b[H");
    session.feed_output("ckVision full-screen child\r\n\r\n");
    session.feed_output("alternate buffer is private to this window");
    auto window = std::make_unique<ckv::widgets::Window>("Full-screen child");
    window->set_bounds(ckv::Rect{2, 2, 76, 20});
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    window->set_content(std::move(view));
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    ckv::widgets::ApplicationShell shell(
        app, {.theme = ckv::ui::make_classic_theme(app.roles(), roles)});
    shell.desktop().add_window(std::move(window));
    app.root().notify_terminal_subsession_changed(session);
    app.step(0);
    write_svg(directory, "terminal-full-screen", terminal.display());
}

void capture_nested(const std::filesystem::path& directory) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{100, 30}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cell_pixels = terminal.capabilities().cell_pixels;
    ckv::term::TerminalEmulator session(profile);
    session.feed_output("\x1b[?1049h\x1b[2J\x1b[H");
    session.feed_output("+--------------------+\r\n| NESTED-CKVISION    |\r\n+--------------------+");
    auto window = std::make_unique<ckv::widgets::Window>("Nested ckVision");
    window->set_bounds(ckv::Rect{2, 2, 76, 20});
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    window->set_content(std::move(view));
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    ckv::widgets::ApplicationShell shell(
        app, {.theme = ckv::ui::make_classic_theme(app.roles(), roles)});
    shell.desktop().add_window(std::move(window));
    app.root().notify_terminal_subsession_changed(session);
    app.step(0);
    write_svg(directory, "terminal-nested", terminal.display());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);
    capture_profile(out_dir, "terminal-initial", ckv::term::headless_no_graphics_profile(), false);
    capture_profile(out_dir, "terminal-initial-dark", ckv::term::headless_no_graphics_profile(), false,
                    ThemeKind::Dark);
    capture_profile(out_dir, "terminal-initial-light", ckv::term::headless_no_graphics_profile(), false,
                    ThemeKind::Light);
    capture_profile(out_dir, "terminal-initial-mono", ckv::term::headless_no_graphics_profile(), false,
                    ThemeKind::Mono);
    capture_menu(out_dir);
    capture_profile(out_dir, "terminal-sixel", ckv::term::headless_sixel_profile(), true);
    capture_profile(out_dir, "terminal-no-graphics", ckv::term::headless_no_graphics_profile(), true);
    capture_full_screen(out_dir);
    capture_nested(out_dir);
    return 0;
}
