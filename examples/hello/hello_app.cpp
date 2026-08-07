// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "hello_app.hpp"

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::hello {

namespace {
// Application-space ids (M9/WP-12: below kFirstApplicationCommandId is
// reserved for the library's own standard set). Menu activation
// references ui::std_commands::kMenu directly instead of defining a
// duplicate — but Quit keeps its own command rather than referencing
// std_commands::kQuit: its title is pinned to "Exit," matching Turbo
// Vision's own hello.cpp tutorial (see this file's own header
// comment), not the framework's generic "Quit" wording.
constexpr ui::CommandId kCmdGreeting = ui::kFirstApplicationCommandId;
constexpr ui::CommandId kCmdQuit = ui::kFirstApplicationCommandId + 1;
}  // namespace

HelloApp::HelloApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds(), app_.theme(), roles_.desktop_background);
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    // Items referencing a command carry no label text of their own
    // (M9/WP-11) — title (with mnemonic), chord hint, and enablement
    // all render live from the registration below.
    std::vector<widgets::MenuBarItem> menus;
    widgets::MenuBarItem hello_menu{"&Hello", {}};
    hello_menu.items.push_back(widgets::MenuItem{"", kCmdGreeting, nullptr, false});
    hello_menu.items.push_back(widgets::MenuItem{"", ui::kInvalidCommand, nullptr, true});
    hello_menu.items.push_back(widgets::MenuItem{"", kCmdQuit, nullptr, false});
    menus.push_back(std::move(hello_menu));
    auto menu_bar = std::make_unique<widgets::MenuBar>(std::move(menus));
    menu_ = desktop_->dock_top(std::move(menu_bar));

    auto status = std::make_unique<widgets::StatusLine>();
    status->set_role_override(roles_.menu_bar_normal);
    status->set_items({{"", kCmdQuit}, {"", ui::std_commands::kMenu}});
    desktop_->dock_bottom(std::move(status));

    // One call each (M9/WP-10) — metadata, chord, and handler together,
    // instead of a register_command()+set_command_handler() pair.
    // kMenu is already registered by Application's constructor (M9/
    // WP-12) — only its handler needs attaching here.
    auto& commands = app_.commands();
    commands.set_handler(ui::std_commands::kMenu, [this] { menu_->activate(); });
    commands.add({.id = kCmdGreeting,
                  .title = "&Greeting...",
                  .category = "Hello",
                  .chord = "Alt+G",
                  .handler = [this] { greeting_box(); }});
    commands.add({.id = kCmdQuit,
                  .title = "E&xit",
                  .category = "Hello",
                  .chord = "Alt+X",
                  .handler = [this] { app_.request_quit(); }});
}

// Turbo Vision's greetingBox() builds a MODAL TDialog (deskTop->execView
// blocks until a button fires cmCancel) with four buttons that all
// carry the SAME command (cmCancel) — every button means "dismiss,"
// none is individually distinguishable by the handler. ckVision's own
// modal push (Application::push_modal, M9/WP-15, D-021) reproduces the
// SAME input scoping — nothing outside the box is reachable while it's
// open — but D-021 deliberately separates modality (loop state) from
// blocking (an opt-in convenience, Desktop::exec_window/
// exec_message_box, for callers that need a result back synchronously):
// greeting_box() has no result to consume, so it stays non-blocking —
// push_modal here, pop_modal from on_closed — rather than reaching for
// exec_window. Observable behavior IS now identical to the original for
// anyone interacting with the box (every button AND Esc dismiss it,
// nothing outside it responds while it's open); the only remaining
// difference is non-observable (whether greeting_box() itself returns
// before or after dismissal, which nothing in this app depends on).
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

    auto content = std::make_unique<ui::View>();
    auto text = std::make_unique<widgets::StaticText>("How are you?");
    text->set_bounds(Rect{2, 4, 14, 1});
    content->add_child(std::move(text));

    const std::pair<const char*, int> labels[] = {{"Terrific", 0}, {"Ok", 2}, {"Lousy", 4}, {"Cancel", 6}};
    widgets::Button* first_button = nullptr;
    for (const auto& [label, y] : labels) {
        auto button = std::make_unique<widgets::Button>(label);
        button->set_bounds(Rect{15, y, 12, 2});
        widgets::Button* raw = content->add(std::move(button));
        if (first_button == nullptr) first_button = raw;
        raw->on_press = [window_ptr] { window_ptr->close(); };
    }

    window->set_content(std::move(content));
    // Esc dismisses too, exactly like every button — the ORIGINAL's
    // own cmCancel-for-everything behavior, left unwired by the prior
    // non-modal version of this box (a real gap, fixed here:
    // docs/turbo-vision-conversion.md's deviation note shrinks
    // accordingly now that this is a genuine modal).
    window->cancel_request = [window_ptr] { window_ptr->close(); };
    window->on_closed = [window_ptr, this] {
        app_.pop_modal();
        schedule_self_detach(*window_ptr, app_);
    };
    widgets::Window* attached =
        desktop_->show_dialog(widgets::WindowHandle{std::move(window), first_button}, app_);
    app_.push_modal(*attached);
}

}  // namespace ckv::hello
