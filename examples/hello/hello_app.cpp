// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "hello_app.hpp"

#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/status_line.hpp"

namespace ckv::hello {

constexpr ui::CommandId kCmdGreeting = ui::kFirstApplicationCommandId, kCmdQuit = ui::kFirstApplicationCommandId + 1;

HelloApp::HelloApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.commands().add({.id = kCmdGreeting, .title = "&Greeting...", .category = "Hello", .chord = "Alt+G", .handler = [this] { greeting_box(); }});
    app_.commands().add({.id = kCmdQuit, .title = "&Quit", .category = "Hello", .chord = "Alt+X", .handler = [this] { app_.request_quit(); }});
    widgets::ApplicationShell shell(app_, {.theme = ui::make_classic_theme(app_.roles(), roles_),
                                           .menus = {{"&File",
                                                      {widgets::MenuItem{widgets::CommandPresentation{kCmdGreeting}},
                                                       widgets::MenuItem{"", ui::kInvalidCommand, {}, true},
                                                       widgets::MenuItem{widgets::CommandPresentation{kCmdQuit, "E&xit"}}}}},
                                           .status_items = {
                                               widgets::StatusLineItem{widgets::CommandPresentation{kCmdQuit, "&Quit"}},
                                               widgets::StatusLineItem{widgets::CommandPresentation{ui::std_commands::kMenu}}}});
    desktop_ = &shell.desktop();
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
