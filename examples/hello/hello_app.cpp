// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "hello_app.hpp"

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"

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

// The example's modal scope is D-021: all four buttons and Esc dismiss
// the dialog, while background input remains inactive until it closes.
void HelloApp::greeting_box() {
    auto window = std::make_unique<widgets::Window>("Hello, World!");
    window->set_role_override(roles_.dialog_frame, roles_.dialog_background, roles_.dialog_frame, roles_.dialog_background);
    window->set_bounds(Rect{25, 5, 30, 11}); widgets::Window* window_ptr = window.get();

    auto* text = window->add(std::make_unique<widgets::StaticText>("How are you?")); text->set_bounds(Rect{3, 5, 12, 1});

    widgets::Button* first_button = nullptr;
    const char* const labels[] = {"Terrific", "Ok", "Lousy", "Cancel"};
    for (int i = 0; i < 4; ++i) {
        auto* raw = window->add(std::make_unique<widgets::Button>(labels[i]));
        raw->set_bounds(Rect{16, 2 * i + 1, 12, 2});
        if (first_button == nullptr) first_button = raw;
        raw->on_press = [window_ptr] { window_ptr->close(); };
    }

    window->cancel_request = [window_ptr] { window_ptr->close(); };
    window->on_closed = [window_ptr, this] { schedule_self_detach(*window_ptr, app_); };
    desktop_->exec_window(app_, widgets::WindowHandle{std::move(window), first_button});
}

}  // namespace ckv::hello
