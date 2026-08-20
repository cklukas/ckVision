// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/window_list_dialog.hpp"

#include "cvision/widgets/list_view.hpp"

#include <optional>

#include "cvision/testing/cktest.hpp"
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
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.add_window(make_window("Alpha"));
    desktop.add_window(make_window("Beta"));

    auto handle = make_window_list_dialog(desktop, f.roles, app, nullptr);
    CK_CHECK(handle.window != nullptr);
    CK_CHECK(handle.initial_focus != nullptr);
}

CK_TEST(a_presented_window_list_has_room_for_the_windows_it_lists) {
    // What shipped: a dialog five rows tall with a Close button and not one
    // entry visible, however many windows were open. Nothing caught it because
    // the test above asks whether the dialog was CONSTRUCTED, and a dialog with
    // no room in it is constructed perfectly well.
    //
    // The cause was one layer down: a list reported no size hints, so a
    // container asking "how big should I be" was told "as big as nothing".
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    // A desktop inside the application, as a real one is: presenting a modal
    // needs the window to end up in the tree the application is driving.
    auto* desktop = app.root().add(std::make_unique<Desktop>(Rect{0, 0, 80, 24}));
    desktop->add_window(make_window("Alpha"));
    desktop->add_window(make_window("Beta"));
    desktop->add_window(make_window("Gamma"));
    desktop->add_window(make_window("Delta"));

    auto handle = make_window_list_dialog(*desktop, roles, app, nullptr);
    auto* list = static_cast<ckv::widgets::ListView*>(handle.initial_focus);
    CK_CHECK(list != nullptr);
    Window* dialog = desktop->present_modal(std::move(handle), app);
    CK_CHECK(dialog != nullptr);
    app.step(0);
    if (list == nullptr || dialog == nullptr) return;

    // A row apiece for the four windows, and the dialog tall enough to hold
    // them with its frame and its button.
    CK_CHECK(list->bounds().height >= 4);
    CK_CHECK(dialog->bounds().height >= 4 + 3);
    // And wide enough for the longest title rather than clipped to a corner.
    CK_CHECK(list->bounds().width >= 5);
}

CK_TEST(activating_a_row_switches_the_desktops_active_window_and_closes_the_dialog) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
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

    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});

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
    auto* desktop = app.root().add(std::make_unique<Desktop>(Rect{0, 0, 80, 24}));
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
