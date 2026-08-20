// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/message_box.hpp"
#include "terminal_app.hpp"

#include <cstdlib>
#include "cvision/widgets/common_components.hpp"
#include <ctime>
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
    //
    // PS1 belongs to that second group and is deliberately absent. A prompt
    // is something its reader has already decided, in the shell startup
    // files they wrote; a terminal that replaces it announces itself in the
    // one place its user is looking and makes every session look unlike
    // every other terminal on the machine. This example is also the pattern
    // adopters copy, so overriding the prompt here would teach the mistake
    // rather than the rule: state what the terminal is (TERM, COLORTERM),
    // never what the shell inside it should look like.
    return {{"TERM", "xterm-256color"}, {"COLORTERM", "truecolor"}};
}

std::string environment_value(const char* name) {
    const char* const value = std::getenv(name);
    return value != nullptr && *value != '\0' ? std::string(value) : std::string();
}

// The shell its reader actually uses, since that is what a terminal is for.
// Reading the environment is the host application's job and never the
// library's: a TerminalLaunchSpec has to name its program outright, and only
// the application around it knows whose machine this is. /bin/sh is the
// fallback because a POSIX system is required to have one, not because it is
// anybody's preference.
std::string user_shell() {
    std::string shell = environment_value("SHELL");
    return shell.empty() ? std::string("/bin/sh") : shell;
}

// A shell opens where a shell opens.
std::string home_directory() {
    std::string home = environment_value("HOME");
    return home.empty() ? std::string("/") : home;
}

}  // namespace

TerminalApp::TerminalApp(ui::Application& app) : app_(app) {
    const ui::StandardRoles roles = ui::intern_standard_roles(app_.roles());
    new_terminal_command_ = app_.commands().declare(ui::CommandDescriptor{
        .key = std::string(kNewTerminalKey),
        .title = "&New Terminal",
        .category = "File",
        .handler = [this] { (void)new_terminal(); },
    });
    new_sixel_demo_command_ = app_.commands().declare(ui::CommandDescriptor{
        .key = std::string(kNewSixelDemoKey),
        .title = "New &Sixel Demo",
        .category = "File",
        .handler = [this] { (void)new_sixel_demo(); },
    });

    widgets::MenuBarItem file_menu{
        "&File",
        {
            widgets::MenuItem::command(widgets::CommandPresentation{new_terminal_command_}),
            widgets::MenuItem::command(widgets::CommandPresentation{new_sixel_demo_command_}),
            widgets::MenuItem::separator(),
            widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().quit}),
        },
    };
    widgets::MenuBarItem window_menu{
        "&Window",
        {
            widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().next_window}),
            widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().previous_window}),
            widgets::MenuItem::separator(),
            widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().tile}),
            widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().cascade}),
        },
    };
    // A focused terminal forwards its keys to the child, F1 included --
    // which is what a terminal is for. So this application also carries the
    // About where it can always be reached.
    widgets::MenuBarItem help_menu{
        "&Help",
        {widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().help,
                                                        "&About..."})},
    };
    widgets::ApplicationShell shell(
        app_, {.theme = ui::make_classic_theme(app_.roles(), roles),
               .menus = {std::move(file_menu), std::move(window_menu), std::move(help_menu)},
               .status_items = {
                   widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().menu}},
                   widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().next_window}},
                   widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().previous_window}},
                   widgets::StatusLineItem{"Ctrl+Alt+Space: parent commands"},
               }});
    desktop_ = &shell.desktop();

    // A clock at the right end of the menu bar, which stays at the right end
    // when the terminal is resized because the bar re-places it rather than
    // remembering where the edge was.
    if (widgets::MenuBar* const menu_bar = shell.menu_bar()) {
        auto clock = std::make_unique<widgets::ClockView>();
        clock->set_time_provider([] {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
#if defined(_WIN32)
            ::localtime_s(&local, &now);
#else
            ::localtime_r(&now, &local);
#endif
            return widgets::TimeValue{local.tm_hour, local.tm_min, local.tm_sec};
        });
        clock->set_blinking_separator(true);
        clock_ = menu_bar->set_trailing_view(std::move(clock));
        // Clicking the clock drops a calendar out of it -- the two widgets
        // are independent, and this is the whole of the wiring between them.
        clock_->on_click = [this] { open_calendar(); };
    }

    (void)new_terminal();

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(app_, *desktop_, ui::intern_standard_roles(app_.roles()),
                                "ckVision Terminal example",
                                "Child processes in contained windows, with scrollback and Sixel graphics.");
}

widgets::Window* TerminalApp::new_terminal() {
    term::TerminalLaunchSpec launch = term::TerminalLaunchSpec::program(user_shell(), {"-i"});
    // Bounded, and this example is exactly why the policy must be named. The
    // child is an INTERACTIVE shell: it ignores SIGTERM by design, so the
    // unbounded policy would let a single unclosed terminal block teardown for
    // as long as that shell felt like living. A demo that hangs on exit teaches
    // the wrong thing twice — once about terminals, once about ckVision.
    launch.exit_policy = core::TerminalExitPolicy::TerminateAfterGrace;
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
    // /bin/sh here is the interpreter for the four lines of setup below, not
    // a choice about anybody's shell: the script is written to POSIX sh so it
    // runs the same everywhere, and its last act is to hand the window over
    // to the reader's own shell.
    term::TerminalLaunchSpec launch = term::TerminalLaunchSpec::program("/bin/sh", {});
    // Bounded for the same reason as the plain terminal above: this script's
    // last act is to exec the reader's own shell, so what this window ends up
    // holding is interactive whatever it started as.
    launch.exit_policy = core::TerminalExitPolicy::TerminateAfterGrace;
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
        "printf '%s' \"$1\"; printf '\n\n'; exec \"$2\" -i",
        "sixel-demo", picture, user_shell()};
    launch.environment = demo_environment();
    launch.working_directory = home_directory();
    return open_terminal(std::move(launch), "Sixel Demo " + std::to_string(next_terminal_number_++));
}

void TerminalApp::open_calendar() {
    if (desktop_ == nullptr || clock_ == nullptr) return;
    // An open calendar holds the mouse, so a click on the clock dismisses it
    // before the clock ever hears about it -- the way clicking an open menu
    // title closes its menu. Reopening takes the next click.
    widgets::CalendarDropdown* const dropdown =
        widgets::show_calendar_dropdown(*clock_, app_, *desktop_);
    clock_->set_open(true);
    widgets::ClockView* const clock = clock_;
    dropdown->on_closed = [clock] { clock->set_open(false); };
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    ::localtime_s(&local, &now);
#else
    ::localtime_r(&now, &local);
#endif
    const widgets::DateValue today{local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
    dropdown->show_month(today);  // the month picker and the year field follow
    dropdown->calendar().set_selected(today);
    dropdown->calendar().set_today(today);
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
