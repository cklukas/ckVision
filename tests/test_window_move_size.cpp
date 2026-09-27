// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A window's geometry by the reader's own hand, driven through the
// Application the way a reader drives it: the keyboard move/size mode behind
// the standard Size/Move command (Ctrl+F5), resizing from every edge as well
// as the corners, the edges of the desktop holding, and the frame line sets
// the border is drawn in.
#include <memory>
#include <string>
#include <string_view>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/minimized_window_stub.hpp"
#include "cvision/widgets/window.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::MouseAction;
using ckv::MouseButton;
using ckv::Point;
using ckv::PointerShape;
using ckv::Rect;
using ckv::Size;
using ckv::ui::Application;
using ckv::widgets::Desktop;
using ckv::widgets::FrameLines;
using ckv::widgets::ListView;
using ckv::widgets::Window;

namespace {

struct Script {
    ckv::term::HeadlessTerminal term{Size{60, 20}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;

    Script() {
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        desktop = app.root().add(std::make_unique<Desktop>(Rect{0, 0, 60, 20}));
    }

    // A document window whose content is a list: a view that means something
    // by every arrow key, which is exactly what the mode has to get past.
    Window* document(std::string title, Rect bounds, ListView** list_out = nullptr) {
        auto window = std::make_unique<Window>(std::move(title));
        window->set_bounds(bounds);
        auto list = std::make_unique<ListView>();
        list->set_items({"one", "two", "three", "four"});
        ListView* list_ptr = list.get();
        window->set_content(std::move(list));
        Window* raw = desktop->add_window(std::move(window));
        app.set_focus(list_ptr);
        if (list_out != nullptr) *list_out = list_ptr;
        app.step(0);
        return raw;
    }

    void press(Key key, Modifier modifiers = Modifier::None) {
        app.dispatch(ckv::KeyEvent{KeyChord{key, modifiers, ""}});
        app.step(0);
    }

    void mouse(MouseAction action, Point cell, MouseButton button = MouseButton::Left) {
        app.dispatch(ckv::MouseEvent{action, button, cell, std::nullopt, Modifier::None});
        app.step(0);
    }

    // Presses at `from`, drags to `to`, releases there.
    void drag(Point from, Point to) {
        mouse(MouseAction::Down, from);
        mouse(MouseAction::Move, to);
        mouse(MouseAction::Up, to);
    }

    std::string_view glyph(Point cell) const { return app.composed_surface().at(cell).grapheme(); }
};

}  // namespace

// --- The keyboard move/size mode -------------------------------------------

CK_TEST(ctrl_f5_puts_the_active_window_into_the_move_size_mode_past_the_focused_content) {
    Script s;
    ListView* list = nullptr;
    Window* window = s.document("Notes", Rect{4, 2, 24, 8}, &list);
    CK_CHECK(s.app.commands().chord_text(s.app.commands().standard().size_move) == "Ctrl+F5");

    s.press(Key::F5, Modifier::Ctrl);
    CK_CHECK(window->in_move_size_mode());
    // The frame holds the keyboard: the list's Down is not the list's now.
    CK_CHECK(s.app.focused() == window);
    s.press(Key::Right);
    s.press(Key::Right);
    s.press(Key::Down);
    s.press(Key::Right, Modifier::Shift);
    s.press(Key::Down, Modifier::Shift);
    CK_CHECK(window->bounds() == (Rect{6, 3, 25, 9}));
    CK_CHECK(list->cursor() == 0);

    s.press(Key::Enter);
    CK_CHECK(!window->in_move_size_mode());
    CK_CHECK(window->bounds() == (Rect{6, 3, 25, 9}));
    // And gives it back to the view that had it.
    CK_CHECK(s.app.focused() == list);
    s.press(Key::Down);
    CK_CHECK(list->cursor() == 1);
}

CK_TEST(escape_in_the_move_size_mode_restores_the_bounds_and_the_focus) {
    Script s;
    ListView* list = nullptr;
    Window* window = s.document("Notes", Rect{4, 2, 24, 8}, &list);
    s.press(Key::F5, Modifier::Ctrl);
    s.press(Key::Left);
    s.press(Key::Up, Modifier::Shift);
    s.press(Key::Escape);
    CK_CHECK(window->bounds() == (Rect{4, 2, 24, 8}));
    CK_CHECK(!window->in_move_size_mode());
    CK_CHECK(s.app.focused() == list);
}

CK_TEST(the_move_size_mode_draws_its_frame_in_the_moving_role_and_stops_at_the_desktop_edges) {
    Script s;
    Window* window = s.document("Notes", Rect{40, 10, 20, 10});
    const ckv::Style idle = s.app.composed_surface().at(Point{40, 12}).style();
    s.press(Key::F5, Modifier::Ctrl);
    const ckv::Style moving = s.app.composed_surface().at(Point{40, 12}).style();
    CK_CHECK(!(moving == idle));
    CK_CHECK(moving.fg == s.app.theme().resolve(s.roles.window_frame_moving).fg);

    // The window already reaches the right and bottom edges: growing stops.
    s.press(Key::Right, Modifier::Shift);
    s.press(Key::Down, Modifier::Shift);
    CK_CHECK(window->bounds() == (Rect{40, 10, 20, 10}));
    s.press(Key::Enter);
    CK_CHECK(s.app.composed_surface().at(Point{40, 12}).style() == idle);
}

CK_TEST(ctrl_f5_moves_a_modal_dialog_and_leaves_it_modal) {
    Script s;
    Window* document = s.document("Notes", Rect{2, 1, 30, 10});
    auto presentation = ckv::widgets::present_modal_message_box(
        s.app, *s.desktop, s.roles,
        ckv::widgets::MessageBoxDescriptor{ckv::widgets::MessageBoxKind::Info, "Saved", "notes.md was saved.",
                                           ckv::widgets::MessageBoxButtons::Ok});
    s.app.step(0);
    Window* dialog = s.desktop->active_window();
    ckv::ui::View* const ok = s.app.focused();
    CK_CHECK(dialog != nullptr && s.app.is_modal_root(*dialog));
    if (dialog == nullptr) return;
    const Rect before = dialog->bounds();
    // Behind a modal, a window cannot be given the keyboard, so it cannot
    // take the mode either.
    document->enter_move_size_mode();
    CK_CHECK(!document->in_move_size_mode());

    s.press(Key::F5, Modifier::Ctrl);
    CK_CHECK(dialog->in_move_size_mode());
    s.press(Key::Left);
    s.press(Key::Left);
    s.press(Key::Enter);  // keeps the new place; does not press OK
    CK_CHECK(dialog->bounds() == (Rect{before.x - 2, before.y, before.width, before.height}));
    CK_CHECK(!presentation.completed());
    CK_CHECK(s.app.is_modal());
    CK_CHECK(s.app.focused() == ok);
    s.press(Key::Enter);  // now OK
    CK_CHECK(presentation.completed());
}

CK_TEST(activating_another_window_ends_the_move_size_mode_keeping_the_bounds) {
    Script s;
    Window* back = s.document("Back", Rect{1, 1, 20, 8});
    Window* front = s.document("Front", Rect{30, 5, 20, 8});
    s.press(Key::F5, Modifier::Ctrl);
    CK_CHECK(front->in_move_size_mode());
    s.press(Key::Down);
    s.mouse(MouseAction::Down, Point{5, 4});  // inside Back's content
    s.mouse(MouseAction::Up, Point{5, 4});
    CK_CHECK(s.desktop->active_window() == back);
    CK_CHECK(!front->in_move_size_mode());
    CK_CHECK(front->bounds() == (Rect{30, 6, 20, 8}));
}

// --- Resizing from every edge ------------------------------------------------

CK_TEST(a_press_on_a_side_or_bottom_edge_resizes_along_that_edges_axis_only) {
    Script s;
    Window* window = s.document("Notes", Rect{10, 3, 24, 10});
    // Left edge, mid-height: the left edge follows, the right one stays.
    s.drag(Point{10, 7}, Point{6, 9});
    CK_CHECK(window->bounds() == (Rect{6, 3, 28, 10}));
    // Right edge.
    s.drag(Point{33, 6}, Point{36, 2});
    CK_CHECK(window->bounds() == (Rect{6, 3, 31, 10}));
    // Bottom edge, away from the corner grips.
    s.drag(Point{20, 12}, Point{25, 15});
    CK_CHECK(window->bounds() == (Rect{6, 3, 31, 13}));
    // The top edge is the title bar: it moves the window.
    s.drag(Point{20, 3}, Point{21, 4});
    CK_CHECK(window->bounds() == (Rect{7, 4, 31, 13}));
}

CK_TEST(the_edges_show_the_axis_they_resize_along) {
    Script s;
    Window* window = s.document("Notes", Rect{10, 3, 24, 10});
    CK_CHECK(window->pointer_shape_at(Point{0, 4}) == PointerShape::ResizeEastWest);
    CK_CHECK(window->pointer_shape_at(Point{23, 4}) == PointerShape::ResizeEastWest);
    CK_CHECK(window->pointer_shape_at(Point{10, 9}) == PointerShape::ResizeNorthSouth);
    CK_CHECK(window->pointer_shape_at(Point{23, 9}) == PointerShape::ResizeNorthWestSouthEast);
    CK_CHECK(window->pointer_shape_at(Point{12, 0}) == PointerShape::Grab);
    CK_CHECK(!window->pointer_shape_at(Point{10, 4}).has_value());  // content, not frame
}

CK_TEST(an_edge_resize_stops_at_the_minimum_size_and_at_the_desktop_content_area) {
    Script s;
    s.desktop->dock_top(std::make_unique<ckv::widgets::MenuBar>(std::vector<ckv::widgets::MenuBarItem>{}));
    Window* window = s.document("Notes", Rect{10, 3, 24, 10});
    // Past the minimum: the left edge stops ten columns short of the right.
    s.drag(Point{10, 7}, Point{40, 7});
    CK_CHECK(window->bounds() == (Rect{24, 3, 10, 10}));
    // The top-left grip pulled up and out stops at the area under the menu
    // bar and at its left edge.
    s.drag(Point{24, 3}, Point{0, 0});
    CK_CHECK(window->bounds() == (Rect{0, 1, 34, 12}));
    // The bottom edge stops at the bottom of the desktop.
    s.drag(Point{15, 12}, Point{15, 30});
    CK_CHECK(window->bounds() == (Rect{0, 1, 34, 19}));
}

CK_TEST(a_fixed_size_window_offers_no_edge_resize) {
    Script s;
    Window* window = s.document("About", Rect{10, 3, 24, 10});
    window->set_resizable(false);
    s.drag(Point{10, 7}, Point{6, 7});
    CK_CHECK(window->bounds() == (Rect{10, 3, 24, 10}));
    CK_CHECK(!window->pointer_shape_at(Point{0, 4}).has_value());
}

// --- Frame line sets ---------------------------------------------------------

CK_TEST(each_frame_line_set_draws_its_own_corners_active_and_inactive) {
    Script s;
    Window* window = s.document("Notes", Rect{2, 2, 20, 6});
    s.document("Other", Rect{40, 2, 16, 6});  // takes activation away
    const auto corners = [&] {
        return std::string(s.glyph(Point{2, 2})) + std::string(s.glyph(Point{21, 2})) +
               std::string(s.glyph(Point{2, 7})) + std::string(s.glyph(Point{21, 7}));
    };
    CK_CHECK(window->frame_lines() == FrameLines::ByActivation);
    CK_CHECK(corners() == "┌┐└┘");
    s.desktop->activate(window);
    s.app.step(0);
    // The active frame's idle bottom-right corner is the grip.
    CK_CHECK(corners() == "╔╗╚┘");

    window->set_frame_lines(FrameLines::Rounded);
    s.app.step(0);
    CK_CHECK(corners() == "╭╮╰╯");
    CK_CHECK(s.glyph(Point{3, 7}) == "─");
    CK_CHECK(s.glyph(Point{2, 4}) == "│");

    window->set_frame_lines(FrameLines::Double);
    s.app.step(0);
    CK_CHECK(corners() == "╔╗╚┘");
    s.desktop->activate(s.desktop->windows().back());
    s.app.step(0);
    CK_CHECK(corners() == "╔╗╚╝");

    window->set_frame_lines(FrameLines::Single);
    s.app.step(0);
    CK_CHECK(corners() == "┌┐└┘");
    window->set_frame_lines(FrameLines::Rounded);
    s.app.step(0);
    CK_CHECK(corners() == "╭╮╰╯");
}

CK_TEST(a_rounded_frame_keeps_rounded_grips_while_it_is_resized) {
    Script s;
    Window* window = s.document("Notes", Rect{2, 2, 20, 6});
    window->set_frame_lines(FrameLines::Rounded);
    s.app.step(0);
    s.mouse(MouseAction::Down, Point{21, 7});
    s.mouse(MouseAction::Move, Point{22, 8});
    // All four grips show during the resize, in the frame's own line set.
    CK_CHECK(s.glyph(Point{2, 2}) == "╭");
    CK_CHECK(s.glyph(Point{22, 2}) == "╮");
    CK_CHECK(s.glyph(Point{2, 8}) == "╰");
    CK_CHECK(s.glyph(Point{22, 8}) == "╯");
    s.mouse(MouseAction::Up, Point{22, 8});
    CK_CHECK(window->bounds() == (Rect{2, 2, 21, 7}));
}

CK_TEST(a_minimized_rounded_window_parks_as_a_rounded_row) {
    Script s;
    Window* window = s.document("Notes", Rect{2, 2, 20, 6});
    window->set_frame_lines(FrameLines::Rounded);
    s.desktop->set_minimize_animation_duration(0);
    window->set_minimized(true);
    s.app.step(0);
    CK_CHECK(s.desktop->parked_windows().size() == 1U);
    const Rect row = s.desktop->parked_windows().front()->bounds();
    CK_CHECK(s.glyph(Point{row.x, row.y}) == "╭");
    CK_CHECK(s.glyph(Point{row.x + row.width - 1, row.y}) == "╮");
}
