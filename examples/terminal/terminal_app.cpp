// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "terminal_app.hpp"

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
    // The library deliberately requires an explicit environment. Keep the
    // demo deterministic and include both Homebrew locations so tools such as
    // btop resolve by name without any hidden ambient-environment read.
    return {{"TERM", "xterm-256color"},
            {"COLORTERM", "truecolor"},
            {"PATH", "/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"},
            {"PS1", "ckvision$ "}};
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
    widgets::ApplicationShell shell(
        app_, {.theme = ui::make_classic_theme(app_.roles(), roles),
               .menus = {std::move(file_menu), std::move(window_menu)},
               .status_items = {
                   widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kMenu}},
                   widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kNextWindow}},
                   widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kPreviousWindow}},
                   widgets::StatusLineItem{"Ctrl+Alt+Space: parent commands"},
               }});
    desktop_ = &shell.desktop();
    (void)new_terminal();
}

widgets::Window* TerminalApp::new_terminal() {
    term::TerminalLaunchSpec launch = term::TerminalLaunchSpec::program("/bin/sh", {"-i"});
    launch.profile = term::embedded_xterm_sixel_profile();
    launch.environment = demo_environment();
    return open_terminal(std::move(launch), "Terminal " + std::to_string(next_terminal_number_++));
}

widgets::Window* TerminalApp::new_sixel_demo() {
    // The private child owns these bytes. The Sixel decoder converts them to
    // a scene raster before the outer presenter can see them; then an
    // interactive shell keeps the demonstration window useful.
    term::TerminalLaunchSpec launch = term::TerminalLaunchSpec::program(
        "/bin/sh", {"-c", "printf '\\033Pq#0;2;100;0;0!32~-!32~-!32~-!32~-!32~-!32~\\033\\\\'; exec /bin/sh -i"});
    launch.profile = term::embedded_xterm_sixel_profile();
    launch.environment = demo_environment();
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
