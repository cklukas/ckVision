// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The terminal scrollbar binding: a Scrollbar on the window frame, driven by
// the TerminalView's scroll state and driving it back. What is pinned here is
// the CONTRACT of attach_terminal_scrollbar — when the bar is on screen, what
// its range says, and that the two stay agreed whichever end moves — not the
// bar's own painting, which test_scrollbar.cpp owns.
#include "cvision/widgets/terminal_scrollbar.hpp"

#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/terminal_emulator.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Point;
using ckv::Rect;
using ckv::widgets::Scrollbar;
using ckv::widgets::TerminalView;
using ckv::widgets::Window;

namespace {

struct Fixture {
    ckv::ui::RoleRegistry registry;
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);

    ckv::term::TerminalEmulator session{[] {
        ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
        profile.cells = ckv::Size{10, 4};
        return profile;
    }()};
    std::unique_ptr<Window> window = std::make_unique<Window>("Term");
    TerminalView* view = nullptr;
    Scrollbar* bar = nullptr;

    Fixture() {
        window->set_context(ckv::ui::Context{&theme, &registry, nullptr});
        window->set_bounds(Rect{0, 0, 12, 6});  // interior 10x4
        auto content = std::make_unique<TerminalView>(session);
        view = content.get();
        window->set_content(std::move(content));
        bar = ckv::widgets::attach_terminal_scrollbar(*window, *view);
    }

    void print_lines(int lines) {
        for (int line = 0; line < lines; ++line)
            session.feed_output("line " + std::to_string(line) + "\r\n");
        window->notify_terminal_subsession_changed(session);
    }
};

}  // namespace

CK_TEST(the_bar_appears_exactly_while_history_is_off_screen) {
    Fixture f;
    // An empty terminal has nowhere to go, and says so by showing no bar.
    CK_CHECK(!f.bar->visible());

    // Enough printing pushes rows into history; now there is somewhere.
    f.print_lines(6);
    CK_CHECK(f.bar->visible());
    CK_CHECK(f.bar->bounds() == (Rect{11, 1, 1, 4}));  // the right border, between the corners
    // The range is the whole document, the viewport the screen, and the
    // thumb rests at the live edge.
    CK_CHECK(f.bar->content_size() == f.view->scroll_state().total_rows);
    CK_CHECK(f.bar->viewport_size() == 4);
    CK_CHECK(f.bar->position() == f.bar->max_position());
}

CK_TEST(the_bar_and_the_view_stay_agreed_whichever_end_moves) {
    Fixture f;
    f.print_lines(6);
    const int maximum = f.bar->max_position();
    CK_CHECK(maximum == f.view->scroll_state().total_rows - 4);

    // Drag the thumb to the top: the view stands on the oldest rows.
    f.bar->set_position(0);
    CK_CHECK(f.view->scroll_state().offset == maximum);

    // Wheel the view back toward live: the thumb follows.
    const ckv::MouseEvent wheel_down{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                     Point{2, 2}, {}, ckv::Modifier::None};
    CK_CHECK(f.view->on_mouse(wheel_down));
    CK_CHECK(f.bar->position() == maximum - f.view->scroll_state().offset);

    // And typing snaps both to the live edge.
    CK_CHECK(f.view->on_text(ckv::TextEvent{"x"}));
    CK_CHECK(f.view->scroll_state().offset == 0);
    CK_CHECK(f.bar->position() == f.bar->max_position());
}

CK_TEST(a_full_screen_child_stands_the_bar_down) {
    Fixture f;
    f.print_lines(6);
    CK_CHECK(f.bar->visible());

    // The child goes full-screen: the bar has nothing to say there, however
    // much history the prompt behind it accumulated.
    f.session.feed_output("\x1b[?1049h");
    f.window->notify_terminal_subsession_changed(f.session);
    CK_CHECK(!f.bar->visible());

    // Back on the primary screen the history is still there, and so is the bar.
    f.session.feed_output("\x1b[?1049l");
    f.window->notify_terminal_subsession_changed(f.session);
    CK_CHECK(f.bar->visible());
    CK_CHECK(f.bar->position() == f.bar->max_position());
}
