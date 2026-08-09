// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "hello_app.hpp"

#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/status_line.hpp"

namespace ckv::hello {

constexpr ui::CommandId kCmdGreeting = ui::kFirstApplicationCommandId, kCmdQuit = ui::kFirstApplicationCommandId + 1;

HelloApp::HelloApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);
    desktop_ = app_.root().add(std::make_unique<widgets::Desktop>(app_.root().bounds(), app_.theme(), roles_.desktop_background));

    // F10 already activates the bar by default (M9/WP-13) — no handler
    // to wire here.
    std::vector<widgets::MenuBarItem> menus{{"&Hello", {{.command = kCmdGreeting}, {.separator = true}, {.command = kCmdQuit}}}};
    desktop_->dock_top(std::make_unique<widgets::MenuBar>(std::move(menus)));

    auto* status = desktop_->dock_bottom(std::make_unique<widgets::StatusLine>()); status->set_role_override(roles_.menu_bar_normal); status->set_items({{.command = kCmdQuit}, {.command = ui::std_commands::kMenu}});
    app_.commands().add({.id = kCmdGreeting, .title = "&Greeting...", .category = "Hello", .chord = "Alt+G", .handler = [this] { greeting_box(); }});
    app_.commands().add({.id = kCmdQuit, .title = "E&xit", .category = "Hello", .chord = "Alt+X", .handler = [this] { app_.request_quit(); }});
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
