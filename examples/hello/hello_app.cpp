// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "hello_app.hpp"

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::hello {

// Quit keeps its own command rather than referencing std_commands::
// kQuit: its title is pinned to "Exit," matching Turbo Vision's own
// hello.cpp tutorial, not the framework's generic "Quit" wording — a
// deliberate fidelity choice kept even under WP-17's line budget.
constexpr ui::CommandId kCmdGreeting = ui::kFirstApplicationCommandId;
constexpr ui::CommandId kCmdQuit = ui::kFirstApplicationCommandId + 1;

HelloApp::HelloApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);
    desktop_ = app_.root().add(std::make_unique<widgets::Desktop>(
        app_.root().bounds(), app_.theme(), roles_.desktop_background));

    // F10 already activates the bar by default (M9/WP-13) — no handler
    // to wire here.
    std::vector<widgets::MenuBarItem> menus{
        {"&Hello", {{.command = kCmdGreeting}, {.separator = true}, {.command = kCmdQuit}}}};
    desktop_->dock_top(std::make_unique<widgets::MenuBar>(std::move(menus)));

    auto* status = desktop_->dock_bottom(std::make_unique<widgets::StatusLine>());
    status->set_role_override(roles_.menu_bar_normal);
    status->set_items({{.command = kCmdQuit}, {.command = ui::std_commands::kMenu}});

    auto& commands = app_.commands();
    commands.add({.id = kCmdGreeting, .title = "&Greeting...", .category = "Hello",
                  .chord = "Alt+G", .handler = [this] { greeting_box(); }});
    commands.add({.id = kCmdQuit, .title = "E&xit", .category = "Hello",
                  .chord = "Alt+X", .handler = [this] { app_.request_quit(); }});
}

// Turbo Vision's greetingBox() builds a MODAL TDialog (deskTop->execView
// blocks until a button fires cmCancel) with four buttons that all
// carry the SAME command (cmCancel) — every button means "dismiss,"
// none is individually distinguishable by the handler. ckVision's own
// blocking modal convenience (Desktop::exec_window, M9/WP-15, D-021)
// reproduces this precisely: input is scoped to the box, everything
// else in the app is inert until it closes, and the call blocks the
// caller — greeting_box() itself — exactly like the original's
// execView(), without a nested native loop (exec_window is built
// entirely over Application::step()).
void HelloApp::greeting_box() {
    // Dialog-gray chrome, not the blue document-window roles — the
    // original's greeting box is a dialog, and dialogs are the gray
    // family (see standard_roles.cpp), which is also what the buttons'
    // black drop shadow composites against.
    auto window = std::make_unique<widgets::Window>("Hello, World!");
    window->set_role_override(roles_.dialog_frame, roles_.dialog_background, roles_.dialog_frame,
                               roles_.dialog_background);
    window->set_bounds(Rect{25, 5, 30, 11});
    widgets::Window* window_ptr = window.get();

    // Added directly (no separate content() wrapper): this dialog's
    // bounds never change after construction, so content_rect()'s
    // auto-reposition-on-resize has nothing to do here.
    auto* text = window->add(std::make_unique<widgets::StaticText>("How are you?"));
    text->set_bounds(Rect{3, 5, 12, 1});

    widgets::Button* first_button = nullptr;
    const char* const labels[] = {"Terrific", "Ok", "Lousy", "Cancel"};
    for (int i = 0; i < 4; ++i) {
        auto* raw = window->add(std::make_unique<widgets::Button>(labels[i]));
        raw->set_bounds(Rect{16, 2 * i + 1, 12, 2});
        if (first_button == nullptr) first_button = raw;
        raw->on_press = [window_ptr] { window_ptr->close(); };
    }

    // Esc dismisses too, exactly like every button — the ORIGINAL's
    // own cmCancel-for-everything behavior.
    window->cancel_request = [window_ptr] { window_ptr->close(); };
    window->on_closed = [window_ptr, this] { schedule_self_detach(*window_ptr, app_); };
    desktop_->exec_window(app_, widgets::WindowHandle{std::move(window), first_button});
}

}  // namespace ckv::hello
