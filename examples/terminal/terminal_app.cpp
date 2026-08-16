// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/message_box.hpp"
#include "terminal_app.hpp"

#include <cstdlib>
#include "cvision/term/sixel_encoder.hpp"
#include <iterator>
#include <cstdint>
#include <algorithm>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/terminal_view.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::terminal_example {

namespace {

std::vector<std::pair<std::string, std::string>> demo_environment() {
    // Only what this terminal itself has an opinion about. The rest --
    // HOME, USER, LANG, PATH, whatever the shell was configured with --
    // comes from the environment the user already has, because a terminal
    // that does not offer that is not a terminal, and a shell without HOME
    // reports its own basic commands as broken.
    return {{"TERM", "xterm-256color"},
            {"COLORTERM", "truecolor"},
            {"PS1", "ckvision$ "}};
}

// A shell opens where a shell opens.
std::string home_directory() {
    const char* home = std::getenv("HOME");
    return home != nullptr && *home != '\0' ? std::string(home) : std::string("/");
}

}  // namespace

TerminalApp::TerminalApp(ui::Application& app) : app_(app) {
    const ui::StandardRoles roles = ui::intern_standard_roles(app_.roles());
    app_.commands().add(ui::CommandDescriptor{
        .id = kNewTerminalCommand,
        .title = "&New Terminal",
        .category = "File",
        .handler = [this] { (void)new_terminal(); },
    });
    app_.commands().add(ui::CommandDescriptor{
        .id = kNewSixelDemoCommand,
        .title = "New &Sixel Demo",
        .category = "File",
        .handler = [this] { (void)new_sixel_demo(); },
    });

    widgets::MenuBarItem file_menu{
        "&File",
        {
            widgets::MenuItem{widgets::CommandPresentation{kNewTerminalCommand}},
            widgets::MenuItem{widgets::CommandPresentation{kNewSixelDemoCommand}},
            widgets::MenuItem{"", ui::kInvalidCommand, {}, true},
            widgets::MenuItem{widgets::CommandPresentation{ui::std_commands::kQuit}},
        },
    };
    widgets::MenuBarItem window_menu{
        "&Window",
        {
            widgets::MenuItem{widgets::CommandPresentation{ui::std_commands::kNextWindow}},
            widgets::MenuItem{widgets::CommandPresentation{ui::std_commands::kPreviousWindow}},
            widgets::MenuItem{"", ui::kInvalidCommand, {}, true},
            widgets::MenuItem{widgets::CommandPresentation{ui::std_commands::kTile}},
            widgets::MenuItem{widgets::CommandPresentation{ui::std_commands::kCascade}},
        },
    };
    // A focused terminal forwards its keys to the child, F1 included --
    // which is what a terminal is for. So this application also carries the
    // About where it can always be reached.
    widgets::MenuBarItem help_menu{
        "&Help",
        {widgets::MenuItem{widgets::CommandPresentation{ui::std_commands::kHelp}}},
    };
    widgets::ApplicationShell shell(
        app_, {.theme = ui::make_classic_theme(app_.roles(), roles),
               .menus = {std::move(file_menu), std::move(window_menu), std::move(help_menu)},
               .status_items = {
                   widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kMenu}},
                   widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kNextWindow}},
                   widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kPreviousWindow}},
                   widgets::StatusLineItem{"Ctrl+Alt+Space: parent commands"},
               }});
    desktop_ = &shell.desktop();
    (void)new_terminal();

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(app_, *desktop_, ui::intern_standard_roles(app_.roles()),
                                "ckVision Terminal example",
                                "Child processes in contained windows, with scrollback and Sixel graphics.");
}

widgets::Window* TerminalApp::new_terminal() {
    term::TerminalLaunchSpec launch = term::TerminalLaunchSpec::program("/bin/sh", {"-i"});
    launch.profile = term::embedded_xterm_sixel_profile();
    launch.environment = demo_environment();
    launch.working_directory = home_directory();
    return open_terminal(std::move(launch), "Terminal " + std::to_string(next_terminal_number_++));
}

namespace {

// A picture worth putting on screen: blocks of flat colour, in a grid, with
// a pale border so the image's own extent is visible against the terminal
// behind it. The demonstration is that a child process's graphics arrive as
// scene rasters -- a solid rectangle a few cells wide does not show that.
//
// Flat colour is not only a look. Sixel encodes one pass per colour and
// run-length encodes each pass, so a picture of a few flat blocks costs a
// couple of kilobytes, while the same picture carrying a smooth gradient
// costs well over a hundred and is dropped by the subsession's output
// budget before it can be decoded -- which presents as an empty window,
// with the reason recorded in a diagnostic nobody was looking at.
Image demo_picture(Size cell_pixels, Size cells) {
    const int width = std::max(1, cell_pixels.width * cells.width);
    const int height = std::max(1, cell_pixels.height * cells.height);
    Image image(width, height);
    constexpr Image::Rgba hues[] = {
        {220, 60, 60, 255},  {220, 150, 60, 255}, {220, 220, 60, 255},
        {60, 200, 90, 255},  {60, 160, 220, 255}, {150, 90, 210, 255},
    };
    constexpr int columns = static_cast<int>(std::size(hues));
    constexpr int rows = 4;
    for (int y = 0; y < height; ++y) {
        const int row = std::min(rows - 1, y * rows / height);
        // Four discrete steps, not a gradient: still visibly a ramp, and
        // still only twenty-four colours in the whole image.
        const int shade = 255 - row * 55;
        for (int x = 0; x < width; ++x) {
            const Image::Rgba hue = hues[std::min(columns - 1, x * columns / width)];
            const bool edge = x < 2 || x >= width - 2 || y < 2 || y >= height - 2;
            image.set_pixel(x, y,
                            edge ? Image::Rgba{235, 235, 235, 255}
                                 : Image::Rgba{static_cast<std::uint8_t>(hue.r * shade / 255),
                                               static_cast<std::uint8_t>(hue.g * shade / 255),
                                               static_cast<std::uint8_t>(hue.b * shade / 255), 255});
        }
    }
    return image;
}

}  // namespace

widgets::Window* TerminalApp::new_sixel_demo() {
    term::TerminalLaunchSpec launch = term::TerminalLaunchSpec::program("/bin/sh", {});
    launch.profile = term::embedded_xterm_sixel_profile();
    // Sized from the child's own cell metric, so the picture lands on whole
    // cells whatever the profile is configured with.
    const std::string picture =
        term::encode_sixel(demo_picture(launch.profile.cell_pixels, Size{44, 10}), 256);
    // The bytes travel as an argument rather than inside the command text.
    // They contain an ESC and a good deal of punctuation, and quoting that
    // into a shell string is how a demo comes to break silently. The child
    // owns them; the Sixel decoder turns them into a scene raster before the
    // outer presenter sees anything, and the interactive shell afterwards
    // leaves the window usable.
    launch.arguments = {
        "-c",
        "printf '%s\n\n' 'ckVision embedded terminal: Sixel from a child process'; "
        "printf '%s' \"$1\"; printf '\n\n'; exec /bin/sh -i",
        "sixel-demo", picture};
    launch.environment = demo_environment();
    launch.working_directory = home_directory();
    return open_terminal(std::move(launch), "Sixel Demo " + std::to_string(next_terminal_number_++));
}

widgets::Window* TerminalApp::open_terminal(term::TerminalLaunchSpec launch, std::string title) {
    auto window = std::make_unique<widgets::Window>(std::move(title));
    window->set_bounds(Rect{2, 2, 76, 20});

    term::TerminalSubsession& session = app_.launch_terminal_subsession(std::move(launch));
    auto view = std::make_unique<widgets::TerminalView>(session);
    widgets::TerminalView* const terminal_view = view.get();
    view->set_bounds(window->content_rect());
    view->set_parent_escape(KeyChord{Key::Char, Modifier::Ctrl | Modifier::Alt, " "});
    view->on_parent_escape = [this] {
        if (auto* const menu = dynamic_cast<widgets::MenuBar*>(desktop_->top_dock())) app_.set_focus(menu);
    };
    view->on_selection_copy = [this](std::string text) { app_.set_clipboard_text(std::move(text)); };
    window->set_content(std::move(view));
    widgets::Window* const terminal_window = window.get();
    term::TerminalSubsession* const terminal_session = &session;
    window->on_closed = [this, terminal_window, terminal_session] {
        terminal_session->close();
        widgets::schedule_self_detach(*terminal_window, app_);
    };
    desktop_->add_window(std::move(window));
    app_.set_focus(terminal_view);
    return terminal_window;
}

}  // namespace ckv::terminal_example
