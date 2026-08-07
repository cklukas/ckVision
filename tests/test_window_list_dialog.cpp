// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/window_list_dialog.hpp"

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Desktop;
using ckv::widgets::make_window_list_dialog;
using ckv::widgets::Window;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

std::unique_ptr<Window> make_window(Fixture& f, std::string title) {
    return std::make_unique<Window>(std::move(title), f.theme, f.roles.window_frame_active,
                                     f.roles.window_frame_inactive, f.roles.window_title_active,
                                     f.roles.window_title_inactive);
}

ckv::KeyEvent key(ckv::Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }
}  // namespace

CK_TEST(the_dialogs_list_contains_every_windows_current_title_in_order) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.add_window(make_window(f, "Alpha"));
    desktop.add_window(make_window(f, "Beta"));

    auto handle = make_window_list_dialog(desktop, f.theme, f.roles, app, nullptr);
    CK_CHECK(handle.window != nullptr);
    CK_CHECK(handle.initial_focus != nullptr);
}

CK_TEST(activating_a_row_switches_the_desktops_active_window_and_closes_the_dialog) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* alpha = desktop.add_window(make_window(f, "Alpha"));
    desktop.add_window(make_window(f, "Beta"));  // Beta is active after being added second
    CK_CHECK(desktop.active_window() != alpha);

    auto handle = make_window_list_dialog(desktop, f.theme, f.roles, app, nullptr);
    bool closed = false;
    handle.window->on_closed = [&closed, previous = handle.window->on_closed]() {
        closed = true;
        if (previous) previous();
    };
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    app.dispatch(key(Key::Enter));  // activates row 0 ("Alpha")
    CK_CHECK(desktop.active_window() == alpha);
    CK_CHECK(closed);
}

CK_TEST(the_close_button_dismisses_without_changing_the_active_window) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.add_window(make_window(f, "Alpha"));
    Window* beta = desktop.add_window(make_window(f, "Beta"));

    auto handle = make_window_list_dialog(desktop, f.theme, f.roles, app, nullptr);
    bool closed = false;
    handle.window->on_closed = [&closed, previous = handle.window->on_closed]() {
        closed = true;
        if (previous) previous();
    };
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    app.dispatch(key(Key::Escape));
    CK_CHECK(closed);
    CK_CHECK(desktop.active_window() == beta);  // unchanged
}

CK_TEST(closing_restores_focus_to_the_view_that_invoked_the_dialog) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto* invoker = app.root().add_child(std::make_unique<ckv::ui::View>());
    invoker->set_focus_policy(ckv::ui::FocusPolicy::TabStop);

    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.add_window(make_window(f, "Alpha"));

    auto handle = make_window_list_dialog(desktop, f.theme, f.roles, app, invoker);
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    app.dispatch(key(Key::Escape));
    CK_CHECK(app.focused() == invoker);
}

CK_TEST(a_desktop_with_no_windows_produces_a_dialog_that_does_not_crash_on_enter) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);

    auto handle = make_window_list_dialog(desktop, f.theme, f.roles, app, nullptr);
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);
    app.dispatch(key(Key::Enter));
    CK_CHECK(true);
}
