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
                  .handler = [this] { quit_ = true; }});
}

// Turbo Vision's greetingBox() builds a MODAL TDialog (deskTop->execView
// blocks until a button fires cmCancel) with four buttons that all
// carry the SAME command (cmCancel) — every button means "dismiss,"
// none is individually distinguishable by the handler. ckVision has no
// modal-blocking primitive in v1 (D-021: modality is loop state, a
// push onto a modality stack that completes via callback — not a
// nested native loop a constructor can just block inside), so this is
// built as a non-modal Window instead, and each button's on_press
// closes it directly rather than routing through one shared command —
// the one deliberate structural deviation from the original, discussed
// in docs/turbo-vision-conversion.md.
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
    window->on_closed = [window_ptr, this] { schedule_self_detach(*window_ptr, app_); };
    desktop_->add_window(std::move(window));
    app_.set_focus(first_button);
}

}  // namespace ckv::hello
