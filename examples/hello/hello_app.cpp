// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "hello_app.hpp"

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::hello {

namespace {
constexpr ui::CommandId kCmdGreeting = 1;
constexpr ui::CommandId kCmdQuit = 2;
}  // namespace

HelloApp::HelloApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds(), app_.theme(), roles_.desktop_background);
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    std::vector<widgets::MenuBarItem> menus;
    widgets::MenuBarItem hello_menu{"&Hello", {}};
    hello_menu.items.push_back(widgets::MenuItem{"&Greeting...", kCmdGreeting, nullptr, false});
    hello_menu.items.push_back(widgets::MenuItem{"", ui::kInvalidCommand, nullptr, true});
    hello_menu.items.push_back(widgets::MenuItem{"E&xit", kCmdQuit, nullptr, false});
    menus.push_back(std::move(hello_menu));
    auto menu_bar = std::make_unique<widgets::MenuBar>(std::move(menus), app_.theme(), roles_.menu_bar_normal,
                                                         roles_.menu_bar_active, roles_.menu_dropdown_normal,
                                                         roles_.menu_dropdown_highlighted,
                                                         roles_.menu_dropdown_disabled, app_, *desktop_);
    desktop_->dock_top(std::move(menu_bar));

    auto status = std::make_unique<widgets::StatusLine>(app_.theme(), roles_.menu_bar_normal, app_);
    status->set_items({{"Alt-X Exit", kCmdQuit}, {"F10 Menu", ui::kInvalidCommand}});
    desktop_->dock_bottom(std::move(status));

    app_.commands().register_command(kCmdGreeting, "Greeting...", "Hello",
                                      KeyChord{Key::Char, Modifier::Alt, "g"});
    app_.commands().register_command(kCmdQuit, "Exit", "Hello", KeyChord{Key::Char, Modifier::Alt, "x"});
    app_.set_command_handler(kCmdGreeting, [this] { greeting_box(); });
    app_.set_command_handler(kCmdQuit, [this] { quit_ = true; });
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
    auto window = std::make_unique<widgets::Window>("Hello, World!", app_.theme(), roles_.dialog_frame,
                                                      roles_.dialog_background, roles_.dialog_frame,
                                                      roles_.dialog_background);
    window->set_bounds(Rect{25, 5, 30, 11});
    widgets::Window* window_ptr = window.get();

    auto content = std::make_unique<ui::View>();
    auto text = std::make_unique<widgets::StaticText>("How are you?", app_.theme(), roles_.static_text);
    text->set_bounds(Rect{2, 4, 14, 1});
    content->add_child(std::move(text));

    const std::pair<const char*, int> labels[] = {{"Terrific", 0}, {"Ok", 2}, {"Lousy", 4}, {"Cancel", 6}};
    widgets::Button* first_button = nullptr;
    for (const auto& [label, y] : labels) {
        auto button = std::make_unique<widgets::Button>(label, app_.theme(), roles_.button_normal,
                                                          roles_.button_focused, roles_.button_default, roles_.button_shadow);
        button->set_bounds(Rect{15, y, 12, 2});
        button->set_focus_policy(ui::FocusPolicy::TabStop);
        widgets::Button* raw = static_cast<widgets::Button*>(content->add_child(std::move(button)));
        if (first_button == nullptr) first_button = raw;
        raw->on_press = [window_ptr] { window_ptr->close(); };
    }

    window->set_content(std::move(content));
    window->on_closed = [window_ptr, this] { schedule_self_detach(*window_ptr, app_); };
    desktop_->add_window(std::move(window));
    app_.set_focus(first_button);
}

}  // namespace ckv::hello
