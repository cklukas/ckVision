// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "hello_app.hpp"

#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/status_line.hpp"

namespace ckv::hello {

HelloApp::HelloApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    // Each command names itself; the registry hands back the id this
    // application then references. Nothing here picks a number.
    const ui::CommandId greeting_command = app_.commands().declare({.key = "hello.greeting", .title = "&Greeting...", .category = "Hello", .chord = "Alt+G", .handler = [this] { greeting_box(); }});
    // Its own quit command rather than the framework's, so this example
    // can present the concept as "Exit" in the reference vocabulary it
    // follows -- see docs/standard-commands.md on when that is the right
    // call and when CommandPresentation is.
    const ui::CommandId quit_command = app_.commands().declare({.key = "hello.quit", .title = "&Quit", .category = "Hello", .chord = "Alt+X", .handler = [this] { app_.request_quit(); }});
    widgets::ApplicationShell shell(app_, {.theme = ui::make_classic_theme(app_.roles(), roles_),
                                           .menus = {{"&File",
                                                      {widgets::MenuItem::command(widgets::CommandPresentation{greeting_command}),
                                                       widgets::MenuItem::separator(),
                                                       widgets::MenuItem::command(widgets::CommandPresentation{quit_command, "E&xit"})}}},
                                           .status_items = {
                                               widgets::StatusLineItem{widgets::CommandPresentation{quit_command, "&Quit"}},
                                               widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().menu}}}});
    desktop_ = &shell.desktop();

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(app_, *desktop_, roles_,
                                "ckVision Hello example",
                                "The smallest complete ckVision application: a shell, a local command, and a dialog.");
}

// A command handler presents the typed, non-blocking standard dialog and
// returns; completion is intentionally observed without retaining a Window.
void HelloApp::greeting_box() {
    auto greeting = widgets::present_message_box(app_, *desktop_, roles_,
                                                  {widgets::MessageBoxKind::Info, "Hello, World!", "How are you?",
                                                   widgets::MessageBoxButtons::Ok});
    greeting.set_completion_handler([](widgets::MessageBoxResult) {});
}

}  // namespace ckv::hello
