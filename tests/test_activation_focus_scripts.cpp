// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// D-107, second half: activation carries the focus. Each script drives a
// real Application through a HeadlessTerminal -- keys and pointer presses
// injected, Application::step() turning them into dispatch -- and asserts,
// after every beat, that the focus lies in the active window and is the view
// that window last had (or its first focus stop, the first time).
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

using ckv::FocusEvent;
using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::MouseAction;
using ckv::MouseButton;
using ckv::Point;
using ckv::Rect;
using ckv::Size;
using ckv::ui::Application;
using ckv::ui::View;
using ckv::widgets::Desktop;
using ckv::widgets::Window;

namespace {

// A focus stop that counts the times it gained the focus, so a script can
// tell a view the reader pointed at from one the focus merely passed through.
class Stop : public View {
public:
    Stop() { set_focus_policy(ckv::ui::FocusPolicy::TabStop); }
    void on_focus(const FocusEvent& event) override {
        if (event.gained) ++gains;
    }
    int gains = 0;
};

// A window holding two focus stops, one above the other.
struct Pane {
    Window* window = nullptr;
    Stop* first = nullptr;
    Stop* second = nullptr;
};

bool inside(const View* view, const View* ancestor) {
    for (; view != nullptr; view = view->parent())
        if (view == ancestor) return true;
    return false;
}

struct Script {
    ckv::term::HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    Application app{terminal, clock};
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;
    // Side by side and never overlapping, so every title bar is in the open.
    Pane alpha;
    Pane beta;
    Pane gamma;

    Script() {
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        auto owned = std::make_unique<Desktop>(app.root().bounds());
        desktop = owned.get();
        app.root().add_child(std::move(owned));
        alpha = pane("Alpha", Rect{0, 0, 26, 10});
        beta = pane("Beta", Rect{27, 0, 26, 10});
        gamma = pane("Gamma", Rect{54, 0, 26, 10});
        app.step(0);
    }

    Pane pane(std::string title, Rect bounds) {
        auto window = std::make_unique<Window>(std::move(title));
        window->set_bounds(bounds);
        auto content = std::make_unique<View>();
        auto first = std::make_unique<Stop>();
        auto second = std::make_unique<Stop>();
        first->set_bounds(Rect{0, 0, 10, 1});
        second->set_bounds(Rect{0, 2, 10, 1});
        Pane made;
        made.first = first.get();
        made.second = second.get();
        content->add_child(std::move(first));
        content->add_child(std::move(second));
        window->set_content(std::move(content));
        made.window = desktop->add_window(std::move(window));
        return made;
    }

    void key(Key k, Modifier modifiers = Modifier::None, std::string text = {}) {
        terminal.inject_event(ckv::KeyEvent{KeyChord{k, modifiers, std::move(text)}});
        app.step(0);
    }
    void type(std::string text) { key(Key::Char, Modifier::None, std::move(text)); }
    void click(Point cell) {
        terminal.inject_event(ckv::MouseEvent{MouseAction::Down, MouseButton::Left, cell, std::nullopt, Modifier::None});
        app.step(0);
        terminal.inject_event(ckv::MouseEvent{MouseAction::Up, MouseButton::Left, cell, std::nullopt, Modifier::None});
        app.step(0);
    }

    // The D-107 invariant: a focus inside any window is inside the active one.
    bool focus_is_in_the_active_window() const {
        const View* const focused = app.focused();
        for (const Window* window : desktop->windows())
            if (inside(focused, window)) return window == desktop->active_window();
        return true;
    }
    bool holds(const Pane& pane, const Stop* stop) const {
        return desktop->active_window() == pane.window && app.focused() == stop && focus_is_in_the_active_window();
    }
};

}  // namespace

CK_TEST(adding_a_window_activates_it_and_hands_the_keyboard_to_its_first_focus_stop) {
    Script s;
    CK_CHECK(s.holds(s.gamma, s.gamma.first));
}

CK_TEST(f6_and_shift_f6_carry_the_focus_to_the_view_each_window_last_had) {
    Script s;
    s.app.set_focus(s.alpha.second);  // focusing inside a window activates it (D-107, first half)
    CK_CHECK(s.holds(s.alpha, s.alpha.second));
    s.app.set_focus(s.beta.second);
    CK_CHECK(s.holds(s.beta, s.beta.second));

    s.key(Key::F6);  // Beta -> Gamma: the view Gamma had when it was added
    CK_CHECK(s.holds(s.gamma, s.gamma.first));
    s.key(Key::F6);  // Gamma -> Alpha, wrapping: Alpha's second view, not its first stop
    CK_CHECK(s.holds(s.alpha, s.alpha.second));
    s.key(Key::F6, Modifier::Shift);  // back to Gamma
    CK_CHECK(s.holds(s.gamma, s.gamma.first));
    s.key(Key::F6, Modifier::Shift);  // and to Beta
    CK_CHECK(s.holds(s.beta, s.beta.second));
}

CK_TEST(alt_digit_selects_a_window_by_number_and_returns_the_focus_it_last_had) {
    Script s;
    s.app.set_focus(s.alpha.second);
    s.app.set_focus(s.beta.second);
    s.key(Key::Char, Modifier::Alt, "1");
    CK_CHECK(s.holds(s.alpha, s.alpha.second));
    s.key(Key::Char, Modifier::Alt, "3");
    CK_CHECK(s.holds(s.gamma, s.gamma.first));
    s.key(Key::Char, Modifier::Alt, "2");
    CK_CHECK(s.holds(s.beta, s.beta.second));
}

CK_TEST(a_title_bar_click_activates_the_window_and_brings_back_its_last_focus) {
    Script s;
    s.app.set_focus(s.alpha.second);
    s.app.set_focus(s.gamma.second);
    const int alpha_second_gains = s.alpha.second->gains;
    s.click(Point{12, 0});  // Alpha's title bar
    CK_CHECK(s.holds(s.alpha, s.alpha.second));
    CK_CHECK(s.alpha.second->gains == alpha_second_gains + 1);
    s.click(Point{64, 0});  // Gamma's
    CK_CHECK(s.holds(s.gamma, s.gamma.second));
}

CK_TEST(a_press_on_a_focus_stop_of_an_inactive_window_focuses_that_stop_and_no_other) {
    Script s;
    s.app.set_focus(s.alpha.second);
    s.app.set_focus(s.beta.first);
    const int second_gains = s.alpha.second->gains;
    const int first_gains = s.alpha.first->gains;
    // Alpha's first stop: the window's top-left content cell. The remembered
    // second stop never has the keyboard in between.
    const Point first_stop{s.alpha.first->absolute_bounds().x, s.alpha.first->absolute_bounds().y};
    s.click(first_stop);
    CK_CHECK(s.holds(s.alpha, s.alpha.first));
    CK_CHECK(s.alpha.second->gains == second_gains);
    CK_CHECK(s.alpha.first->gains == first_gains + 1);
}

CK_TEST(the_window_lists_switch_to_hands_the_focus_to_the_chosen_windows_last_view) {
    Script s;
    s.app.set_focus(s.alpha.second);
    s.app.set_focus(s.beta.second);
    CK_CHECK(s.app.execute_command(s.app.commands().standard().window_list));
    s.app.step(0);
    CK_CHECK(s.desktop->windows().size() == 4U);
    const int beta_gains = s.beta.second->gains;
    s.type("l");  // "Alpha" is the only title with an l in it
    s.key(Key::Enter);
    s.app.step(0);
    CK_CHECK(s.desktop->windows().size() == 3U);  // the list has gone
    CK_CHECK(!s.app.is_modal());
    // Not back to Beta, where the list was invoked: the switch is the later
    // word, and the list's restoration to Beta is not even passed through.
    CK_CHECK(s.holds(s.alpha, s.alpha.second));
    CK_CHECK(s.beta.second->gains == beta_gains);
}

CK_TEST(dismissing_the_window_list_returns_the_focus_to_where_the_reader_was) {
    Script s;
    s.app.set_focus(s.alpha.second);
    s.app.set_focus(s.beta.second);
    CK_CHECK(s.app.execute_command(s.app.commands().standard().window_list));
    s.app.step(0);
    s.key(Key::Escape);
    s.app.step(0);
    CK_CHECK(s.desktop->windows().size() == 3U);
    CK_CHECK(s.holds(s.beta, s.beta.second));
}

CK_TEST(closing_the_active_window_hands_the_focus_to_its_successors_last_view) {
    Script s;
    s.app.set_focus(s.alpha.second);
    s.app.set_focus(s.gamma.first);
    CK_CHECK(s.gamma.window->close());
    s.app.step(0);
    CK_CHECK(s.desktop->windows().size() == 2U);
    // Alpha was raised last before Gamma, so it is the topmost that remains.
    CK_CHECK(s.holds(s.alpha, s.alpha.second));
}

CK_TEST(activating_a_window_without_a_focus_stop_takes_the_focus_out_of_the_one_that_lost_activation) {
    Script s;
    s.app.set_focus(s.beta.second);
    auto bare = std::make_unique<Window>("Bare");
    bare->set_bounds(Rect{0, 12, 30, 8});
    Window* const bare_window = s.desktop->add_window(std::move(bare));
    CK_CHECK(s.desktop->active_window() == bare_window);
    CK_CHECK(s.app.focused() == nullptr);
    CK_CHECK(s.focus_is_in_the_active_window());
    s.key(Key::Char, Modifier::Alt, "2");
    CK_CHECK(s.holds(s.beta, s.beta.second));
}

CK_TEST(a_restored_snapshot_carries_the_focus_into_the_window_it_leaves_active) {
    Script s;
    s.app.set_focus(s.alpha.second);
    const Desktop::Snapshot snapshot = s.desktop->snapshot();
    s.app.set_focus(s.gamma.second);
    s.desktop->restore(snapshot);
    CK_CHECK(s.holds(s.alpha, s.alpha.second));
}

CK_TEST(a_remembered_view_that_went_away_falls_back_to_the_first_focus_stop) {
    Script s;
    s.app.set_focus(s.alpha.second);
    s.app.set_focus(s.beta.first);
    std::unique_ptr<View> removed = s.alpha.second->parent()->remove_child(s.alpha.second);
    removed.reset();
    s.key(Key::Char, Modifier::Alt, "1");
    CK_CHECK(s.holds(s.alpha, s.alpha.first));
}

CK_TEST(a_focus_request_refused_by_a_modal_waits_for_the_modal_to_end_and_replaces_its_restore) {
    // The Application half of Switch To: a request made while a modal scope
    // excludes its view is kept across routing points, and when the scope
    // ends it is honoured in place of the scope's own restoration.
    Script s;
    s.app.set_focus(s.beta.second);
    auto dialog = std::make_unique<Window>("Dialog");
    dialog->set_bounds(Rect{20, 12, 30, 8});
    auto dialog_stop = std::make_unique<Stop>();
    dialog_stop->set_bounds(Rect{0, 0, 10, 1});
    Stop* const inside_dialog = dialog_stop.get();
    dialog->set_content(std::move(dialog_stop));
    Window* const modal = s.desktop->present_modal(ckv::widgets::WindowHandle{std::move(dialog), inside_dialog}, s.app);
    CK_CHECK(modal != nullptr);
    CK_CHECK(s.app.focused() == inside_dialog);

    s.app.set_focus(s.alpha.second);  // refused while the dialog is modal
    CK_CHECK(s.app.focused() == inside_dialog);
    s.app.step(0);
    s.key(Key::Tab);  // routing points pass; the request waits
    CK_CHECK(s.app.focused() == inside_dialog);

    std::unique_ptr<Window> gone = s.desktop->remove_window(modal);
    s.app.step(0);
    CK_CHECK(!s.app.is_modal());
    // Beta, where the modal found the focus, is where its restoration would
    // have gone; the later request wins.
    CK_CHECK(s.holds(s.alpha, s.alpha.second));
}

CK_TEST(first_focus_stop_walks_the_subtree_in_tab_order_past_hidden_and_disabled_views) {
    Script s;
    CK_CHECK(Application::first_focus_stop(*s.alpha.window) == s.alpha.first);
    s.alpha.first->set_visible(false);
    CK_CHECK(Application::first_focus_stop(*s.alpha.window) == s.alpha.second);
    s.alpha.second->set_enabled(false);
    CK_CHECK(Application::first_focus_stop(*s.alpha.window) == nullptr);
}
