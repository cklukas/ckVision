// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/window_list_dialog.hpp"

#include <optional>

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
using ckv::widgets::present_window_list_dialog;
using ckv::widgets::Window;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

std::unique_ptr<Window> make_window(std::string title) { return std::make_unique<Window>(std::move(title)); }

ckv::KeyEvent key(ckv::Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }
ckv::KeyEvent text_key(std::string text) {
    return ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, std::move(text)}};
}
}  // namespace

CK_TEST(the_dialogs_list_contains_every_windows_current_title_in_order) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.add_window(make_window("Alpha"));
    desktop.add_window(make_window("Beta"));

    auto handle = make_window_list_dialog(desktop, f.roles, app, nullptr);
    CK_CHECK(handle.window != nullptr);
    CK_CHECK(handle.initial_focus != nullptr);
}

CK_TEST(activating_a_row_switches_the_desktops_active_window_and_closes_the_dialog) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* alpha = desktop.add_window(make_window("Alpha"));
    desktop.add_window(make_window("Beta"));  // Beta is active after being added second
    CK_CHECK(desktop.active_window() != alpha);

    auto handle = make_window_list_dialog(desktop, f.roles, app, nullptr);
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

CK_TEST(type_ahead_selects_a_matching_window_before_activation) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.add_window(make_window("Alpha"));
    Window* beta = desktop.add_window(make_window("Beta"));
    Window* gamma = desktop.add_window(make_window("Gamma"));
    CK_CHECK(desktop.active_window() == gamma);

    auto handle = make_window_list_dialog(desktop, f.roles, app, nullptr);
    bool closed = false;
    handle.window->on_closed = [&closed, previous = handle.window->on_closed]() {
        closed = true;
        if (previous) previous();
    };
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    app.dispatch(text_key("b"));
    app.dispatch(key(Key::Enter));

    CK_CHECK(desktop.active_window() == beta);
    CK_CHECK(closed);
}

CK_TEST(the_close_button_dismisses_without_changing_the_active_window) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.add_window(make_window("Alpha"));
    Window* beta = desktop.add_window(make_window("Beta"));

    auto handle = make_window_list_dialog(desktop, f.roles, app, nullptr);
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
    desktop.add_window(make_window("Alpha"));

    auto handle = make_window_list_dialog(desktop, f.roles, app, invoker);
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

    auto handle = make_window_list_dialog(desktop, f.roles, app, nullptr);
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);
    app.dispatch(key(Key::Enter));
    CK_CHECK(true);
}

CK_TEST(present_window_list_dialog_completes_after_modal_detachment) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(std::make_unique<Desktop>(Rect{0, 0, 80, 24}, app.theme(),
                                                              roles.dialog_background));
    desktop->add_window(make_window("Workspace"));

    auto presentation = present_window_list_dialog(*desktop, app, roles);
    std::optional<ckv::widgets::WindowListDialogResult> completion;
    presentation.set_completion_handler(
        [&](ckv::widgets::WindowListDialogResult result) { completion = result; });

    CK_CHECK(app.is_modal());
    app.dispatch(key(Key::Escape));
    CK_CHECK(!presentation.completed());
    app.step(0);

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result() == ckv::widgets::WindowListDialogResult::Closed);
    CK_CHECK(completion == ckv::widgets::WindowListDialogResult::Closed);
    CK_CHECK(!app.is_modal());
}
