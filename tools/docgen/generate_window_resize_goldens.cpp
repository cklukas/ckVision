// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for the M10 window-resize scripts. Both scripts
// enter input only through HeadlessTerminal and Application::step:
//   * a resizable dialog-styled window holding one bottom-right-anchored
//     button and one all-edges-anchored memo, grip-dragged larger by mouse;
//   * a window with two bottom frame overlays, grip-dragged larger by mouse,
//     after which typing changes the width of one overlay's label.
// tests/test_window_resize_golden.cpp runs the same scripts and compares each
// composed frame with these dumps.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/anchor_pane.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/widgets/window.hpp"

namespace {

void write_dump(const std::filesystem::path& directory, const char* name, const ckv::ui::Application& app) {
    std::ofstream output(directory / name, std::ios::binary);
    output << ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
}

void inject_mouse(ckv::term::HeadlessTerminal& terminal, ckv::ui::Application& app, ckv::MouseAction action,
                  ckv::Point cell) {
    terminal.inject_event(
        ckv::MouseEvent{action, ckv::MouseButton::Left, cell, std::nullopt, ckv::Modifier::None});
    app.step(0);
}

// Presses the window's bottom-right grip, drags it by `delta`, and releases.
void drag_bottom_right_grip(ckv::term::HeadlessTerminal& terminal, ckv::ui::Application& app,
                            const ckv::widgets::Window& window, ckv::Point delta) {
    const ckv::Rect bounds = window.absolute_bounds();
    const ckv::Point grip{bounds.x + bounds.width - 1, bounds.y + bounds.height - 1};
    const ckv::Point target{grip.x + delta.x, grip.y + delta.y};
    inject_mouse(terminal, app, ckv::MouseAction::Down, grip);
    inject_mouse(terminal, app, ckv::MouseAction::Move, target);
    inject_mouse(terminal, app, ckv::MouseAction::Up, target);
}

struct Session {
    ckv::term::HeadlessTerminal terminal{ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile()};
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    ckv::widgets::Desktop* desktop = nullptr;

    Session() {
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        auto owned = std::make_unique<ckv::widgets::Desktop>(app.root().bounds());
        desktop = owned.get();
        app.root().add_child(std::move(owned));
    }
};

void anchored_dialog_script(const std::filesystem::path& directory) {
    Session session;
    auto window = std::make_unique<ckv::widgets::Window>("Notes");
    window->set_role_override(session.roles.dialog_frame, session.roles.dialog_background,
                              session.roles.dialog_frame, session.roles.dialog_background);
    window->set_content_margin(1, 1);
    window->set_resizable(true);
    window->set_bounds(ckv::Rect{10, 3, 40, 12});
    ckv::ui::AnchorPane& pane = window->content_pane();
    auto memo = std::make_unique<ckv::widgets::Memo>();
    memo->set_bounds(ckv::Rect{0, 0, 36, 5});
    ckv::widgets::Memo* const memo_ptr = memo.get();
    pane.add_item(std::move(memo), ckv::ui::Anchors{true, true, true, true});
    auto button = std::make_unique<ckv::widgets::Button>("OK");
    button->set_bounds(ckv::Rect{26, 6, 10, 2});
    pane.add_item(std::move(button), ckv::ui::Anchors{false, false, true, true});
    ckv::widgets::Window* const window_ptr = session.desktop->add_window(std::move(window));
    session.app.set_focus(memo_ptr);

    session.app.step(0);
    write_dump(directory, "anchored_dialog_before.dump", session.app);
    drag_bottom_right_grip(session.terminal, session.app, *window_ptr, ckv::Point{10, 4});
    write_dump(directory, "anchored_dialog_after.dump", session.app);
}

void frame_overlays_script(const std::filesystem::path& directory) {
    Session session;
    auto window = std::make_unique<ckv::widgets::Window>("Overlays");
    window->set_bounds(ckv::Rect{4, 3, 36, 8});
    auto input = std::make_unique<ckv::widgets::InputLine>();
    ckv::widgets::InputLine* const input_ptr = input.get();
    window->set_content(std::move(input));
    window->add_frame_overlay(std::make_unique<ckv::widgets::Label>("Name"),
                              ckv::widgets::FrameSlot{ckv::widgets::Edge::Bottom, ckv::ui::Alignment::Start, 1});
    ckv::widgets::Label* const count = window->add_frame_overlay(
        std::make_unique<ckv::widgets::Label>("0 chars"),
        ckv::widgets::FrameSlot{ckv::widgets::Edge::Bottom, ckv::ui::Alignment::End, -1});
    input_ptr->on_edited = [input_ptr, count] {
        count->set_text(std::to_string(input_ptr->text().size()) + " chars");
    };
    ckv::widgets::Window* const window_ptr = session.desktop->add_window(std::move(window));
    session.app.set_focus(input_ptr);

    session.app.step(0);
    write_dump(directory, "frame_overlays_initial.dump", session.app);
    drag_bottom_right_grip(session.terminal, session.app, *window_ptr, ckv::Point{12, 3});
    write_dump(directory, "frame_overlays_resized.dump", session.app);
    session.terminal.inject_event(ckv::TextEvent{"Ada Lovelace", false});
    session.app.step(0);
    write_dump(directory, "frame_overlays_relabelled.dump", session.app);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);
    anchored_dialog_script(directory);
    frame_overlays_script(directory);
    return 0;
}
