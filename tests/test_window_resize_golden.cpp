// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// M10's window-resize acceptance as event scripts: every input enters through
// HeadlessTerminal and Application::step, the geometry each script promises is
// asserted, every composed frame is compared with its pinned dump (written by
// tools/docgen/generate_window_resize_goldens.cpp from the same scripts), and
// after every step the presented display must equal the composed frame.
#include <fstream>
#include <sstream>
#include <string>

#include "cvision/testing/cktest.hpp"
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
#include "presented_frame.hpp"

using ckv::ManualClock;
using ckv::MouseAction;
using ckv::Point;
using ckv::Rect;
using ckv::Size;
using ckv::ui::Application;
using ckv::widgets::Window;

namespace {

std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string capture(const Application& app) {
    return ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
}

// One application on a TrueColor HeadlessTerminal, so the presented display
// can be compared with the composed frame exactly after every step.
struct Session {
    ckv::term::HeadlessTerminal term{Size{80, 24}, ckv::term::headless_no_graphics_profile()};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    ckv::widgets::Desktop* desktop = nullptr;

    Session() {
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        auto owned = std::make_unique<ckv::widgets::Desktop>(app.root().bounds());
        desktop = owned.get();
        app.root().add_child(std::move(owned));
    }

    void step() {
        app.step(0);
        CK_CHECK(cktest_support::presented_equals_composed(term, app));
    }

    void mouse(MouseAction action, Point cell) {
        term.inject_event(
            ckv::MouseEvent{action, ckv::MouseButton::Left, cell, std::nullopt, ckv::Modifier::None});
        step();
    }

    // Presses the window's bottom-right grip, drags it by `delta`, releases.
    void drag_bottom_right_grip(const Window& window, Point delta) {
        const Rect bounds = window.absolute_bounds();
        const Point grip{bounds.x + bounds.width - 1, bounds.y + bounds.height - 1};
        const Point target{grip.x + delta.x, grip.y + delta.y};
        mouse(MouseAction::Down, grip);
        mouse(MouseAction::Move, target);
        mouse(MouseAction::Up, target);
    }
};

// The distance from `child`'s right and bottom edges to its parent's.
Point far_edge_distance(const ckv::ui::View& child) {
    const Rect parent = child.parent()->bounds();
    const Rect bounds = child.bounds();
    return Point{parent.width - (bounds.x + bounds.width), parent.height - (bounds.y + bounds.height)};
}

}  // namespace

CK_TEST(grip_dragging_a_resizable_dialog_keeps_its_corner_child_in_the_corner_and_stretches_its_filling_child) {
    Session s;
    auto window = std::make_unique<Window>("Notes");
    window->set_role_override(s.roles.dialog_frame, s.roles.dialog_background, s.roles.dialog_frame,
                              s.roles.dialog_background);
    window->set_content_margin(1, 1);
    window->set_resizable(true);
    window->set_bounds(Rect{10, 3, 40, 12});
    ckv::ui::AnchorPane& pane = window->content_pane();
    auto memo = std::make_unique<ckv::widgets::Memo>();
    memo->set_bounds(Rect{0, 0, 36, 5});
    ckv::widgets::Memo* const memo_ptr = memo.get();
    pane.add_item(std::move(memo), ckv::ui::Anchors{true, true, true, true});
    auto button = std::make_unique<ckv::widgets::Button>("OK");
    button->set_bounds(Rect{26, 6, 10, 2});
    ckv::ui::View* const button_ptr = pane.add_item(std::move(button), ckv::ui::Anchors{false, false, true, true});
    Window* const window_ptr = s.desktop->add_window(std::move(window));
    s.app.set_focus(memo_ptr);

    s.step();
    const Rect pane_before = pane.bounds();
    const Point corner_distance = far_edge_distance(*button_ptr);
    const Point memo_margin = far_edge_distance(*memo_ptr);
    CK_CHECK(pane_before.width == 36);
    CK_CHECK(pane_before.height == 8);
    CK_CHECK(button_ptr->bounds() == (Rect{26, 6, 10, 2}));
    CK_CHECK(corner_distance == (Point{0, 0}));
    CK_CHECK(memo_ptr->bounds() == (Rect{0, 0, 36, 5}));
    CK_CHECK(capture(s.app) == read_file("golden/anchored_dialog_before.dump"));

    s.drag_bottom_right_grip(*window_ptr, Point{10, 4});

    // The grip drag grew the window by exactly the drag, from its origin.
    CK_CHECK(window_ptr->bounds() == (Rect{10, 3, 50, 16}));
    CK_CHECK(pane.bounds().width == pane_before.width + 10);
    CK_CHECK(pane.bounds().height == pane_before.height + 4);
    // Bottom-right anchored: same size, same distance to the far corner.
    CK_CHECK(button_ptr->bounds() == (Rect{36, 10, 10, 2}));
    CK_CHECK(far_edge_distance(*button_ptr) == corner_distance);
    // All edges anchored: same origin, stretched by the whole drag.
    CK_CHECK(memo_ptr->bounds() == (Rect{0, 0, 46, 9}));
    CK_CHECK(far_edge_distance(*memo_ptr) == memo_margin);
    CK_CHECK(capture(s.app) == read_file("golden/anchored_dialog_after.dump"));
}

CK_TEST(two_frame_overlays_track_a_mouse_resize_and_one_labels_width_change_without_moving_the_other) {
    Session s;
    auto window = std::make_unique<Window>("Overlays");
    window->set_bounds(Rect{4, 3, 36, 8});
    auto input = std::make_unique<ckv::widgets::InputLine>();
    ckv::widgets::InputLine* const input_ptr = input.get();
    window->set_content(std::move(input));
    ckv::widgets::Label* const name = window->add_frame_overlay(
        std::make_unique<ckv::widgets::Label>("Name"),
        ckv::widgets::FrameSlot{ckv::widgets::Edge::Bottom, ckv::ui::Alignment::Start, 1});
    ckv::widgets::Label* const count = window->add_frame_overlay(
        std::make_unique<ckv::widgets::Label>("0 chars"),
        ckv::widgets::FrameSlot{ckv::widgets::Edge::Bottom, ckv::ui::Alignment::End, -1});
    input_ptr->on_edited = [input_ptr, count] {
        count->set_text(std::to_string(input_ptr->text().size()) + " chars");
    };
    Window* const window_ptr = s.desktop->add_window(std::move(window));
    s.app.set_focus(input_ptr);

    s.step();
    const Rect name_before = name->bounds();
    const Rect count_before = count->bounds();
    // Both stand on the bottom border, one from each end, not overlapping.
    CK_CHECK(name_before.y == 7);
    CK_CHECK(count_before.y == 7);
    CK_CHECK(name_before.width == 4);
    CK_CHECK(count_before.width == 7);
    CK_CHECK(name_before.x + name_before.width < count_before.x);
    const int count_right_margin = 36 - (count_before.x + count_before.width);
    CK_CHECK(capture(s.app) == read_file("golden/frame_overlays_initial.dump"));

    s.drag_bottom_right_grip(*window_ptr, Point{12, 3});

    CK_CHECK(window_ptr->bounds() == (Rect{4, 3, 48, 11}));
    // The start overlay keeps its place from the left; the end overlay keeps
    // its distance from the right; both follow the bottom border down.
    CK_CHECK(name->bounds() == (Rect{name_before.x, 10, name_before.width, 1}));
    CK_CHECK(count->bounds() == (Rect{count_before.x + 12, 10, count_before.width, 1}));
    CK_CHECK(48 - (count->bounds().x + count->bounds().width) == count_right_margin);
    CK_CHECK(capture(s.app) == read_file("golden/frame_overlays_resized.dump"));

    const Rect name_resized = name->bounds();
    s.term.inject_event(ckv::TextEvent{"Ada Lovelace", false});
    s.step();

    CK_CHECK(input_ptr->text() == "Ada Lovelace");
    CK_CHECK(count->text() == "12 chars");
    // The label grew by one cell and stays flush to the same right margin,
    // while the other overlay does not move at all.
    CK_CHECK(count->bounds().width == count_before.width + 1);
    CK_CHECK(48 - (count->bounds().x + count->bounds().width) == count_right_margin);
    CK_CHECK(name->bounds() == name_resized);
    CK_CHECK(name->bounds().x + name->bounds().width < count->bounds().x);
    CK_CHECK(capture(s.app) == read_file("golden/frame_overlays_relabelled.dump"));
}
