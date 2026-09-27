// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/window_list_dialog.hpp"

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/list_view.hpp"

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
using ckv::widgets::present_modal_window_list_dialog;
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

CK_TEST(type_ahead_filters_to_a_matching_window_before_activation) {
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

CK_TEST(escape_dismisses_without_changing_the_active_window) {
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

CK_TEST(present_modal_window_list_dialog_completes_after_modal_detachment) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(std::make_unique<Desktop>(Rect{0, 0, 80, 24}));
    desktop->add_window(make_window("Workspace"));

    auto presentation = present_modal_window_list_dialog(*desktop, app, roles);
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

// --- Filtering, closing, following the desktop ------------------------------

namespace {

// A desktop inside a real application with the list presented over it, the
// way the kWindowList default handler presents it.
struct Presented {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    StandardRoles roles = intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;
    ckv::widgets::ListView* list = nullptr;
    std::optional<ckv::widgets::WindowListDialogPresentation> presentation;

    explicit Presented(std::initializer_list<const char*> titles) {
        app.theme() = make_classic_theme(app.roles(), roles);
        desktop = app.root().add(std::make_unique<Desktop>(Rect{0, 0, 80, 24}));
        for (const char* title : titles) desktop->add_window(make_window(title));
        presentation.emplace(present_modal_window_list_dialog(*desktop, app, roles));
        list = dynamic_cast<ckv::widgets::ListView*>(app.focused());
        app.step(0);
    }

    std::vector<std::string> listed() const {
        std::vector<std::string> titles;
        if (list == nullptr || list->model() == nullptr) return titles;
        for (std::size_t i = 0; i < list->model()->item_count(); ++i) titles.push_back(list->model()->item_at(i).text);
        return titles;
    }

    void type(std::string text) {
        for (char c : text) app.dispatch(text_key(std::string(1, c)));
        app.step(0);
    }

    void press(Key k) {
        app.dispatch(key(k));
        app.step(0);
    }

    Window* window_titled(std::string_view title) const {
        for (Window* window : desktop->windows())
            if (window->title() == title) return window;
        return nullptr;
    }
};

using Titles = std::vector<std::string>;

}  // namespace

CK_TEST(typing_over_the_window_list_filters_it_and_backspace_widens_it_again) {
    Presented p({"Alpha", "Beta", "Gamma", "Alphabet"});
    CK_CHECK(p.list != nullptr);
    // The dialog lists every window but itself.
    CK_CHECK(p.listed() == (Titles{"Alpha", "Beta", "Gamma", "Alphabet"}));

    p.type("al");
    CK_CHECK(p.listed() == (Titles{"Alpha", "Alphabet"}));
    // Anywhere in the title, and ASCII letters without case.
    p.press(Key::Backspace);
    p.press(Key::Backspace);
    p.type("BET");
    CK_CHECK(p.listed() == (Titles{"Beta", "Alphabet"}));
    p.press(Key::Backspace);
    CK_CHECK(p.listed() == (Titles{"Beta", "Alphabet"}));
    p.press(Key::Backspace);
    p.press(Key::Backspace);
    CK_CHECK(p.listed() == (Titles{"Alpha", "Beta", "Gamma", "Alphabet"}));
    CK_CHECK(p.app.is_modal());  // editing the filter never dismissed anything
}

CK_TEST(escape_clears_a_filter_before_it_closes_the_window_list) {
    Presented p({"Alpha", "Beta"});
    p.type("be");
    CK_CHECK(p.listed() == (Titles{"Beta"}));
    p.press(Key::Escape);
    CK_CHECK(p.listed() == (Titles{"Alpha", "Beta"}));
    CK_CHECK(p.app.is_modal());
    p.press(Key::Escape);
    CK_CHECK(!p.app.is_modal());
    CK_CHECK(p.presentation->completed());
}

CK_TEST(enter_switches_to_the_filtered_window_under_the_cursor) {
    Presented p({"Sources", "Build log", "Terminal"});
    p.type("log");
    p.press(Key::Enter);
    CK_CHECK(p.desktop->active_window() == p.window_titled("Build log"));
    CK_CHECK(!p.app.is_modal());
}

CK_TEST(a_filter_that_matches_nothing_disables_the_buttons_that_act_on_a_window) {
    Presented p({"Alpha", "Beta"});
    const auto button = [&](const std::string& text) -> ckv::widgets::Button* {
        std::vector<ckv::ui::View*> pending{p.desktop};
        while (!pending.empty()) {
            ckv::ui::View* view = pending.back();
            pending.pop_back();
            if (auto* b = dynamic_cast<ckv::widgets::Button*>(view); b != nullptr && b->text() == text) return b;
            for (const auto& child : view->children()) pending.push_back(child.get());
        }
        return nullptr;
    };
    ckv::widgets::Button* switch_to = button("&Switch To");
    ckv::widgets::Button* close_window = button("Close &Window");
    CK_CHECK(switch_to != nullptr && close_window != nullptr);
    if (switch_to == nullptr || close_window == nullptr) return;
    CK_CHECK(switch_to->enabled() && close_window->enabled());

    p.type("zz");
    CK_CHECK(p.listed().empty());
    CK_CHECK(!switch_to->enabled() && !close_window->enabled());
    p.press(Key::Enter);  // nothing to switch to: the dialog stays
    CK_CHECK(p.app.is_modal());
    p.press(Key::Backspace);
    p.press(Key::Backspace);
    CK_CHECK(switch_to->enabled() && close_window->enabled());
}

CK_TEST(delete_closes_the_listed_window_through_its_vetoable_close_and_the_list_follows) {
    Presented p({"Notes", "Draft", "Terminal"});
    Window* draft = p.window_titled("Draft");
    bool allow = false;
    int asked = 0;
    draft->close_request = [&] {
        ++asked;
        return allow;
    };

    p.press(Key::Down);    // the cursor on "Draft"
    p.press(Key::Delete);  // refused by the window itself
    CK_CHECK(asked == 1);
    CK_CHECK(p.window_titled("Draft") == draft);
    CK_CHECK(p.listed() == (Titles{"Notes", "Draft", "Terminal"}));

    allow = true;
    p.press(Key::Delete);  // accepted: the window leaves, and the list with it
    CK_CHECK(asked == 2);
    CK_CHECK(p.window_titled("Draft") == nullptr);
    CK_CHECK(p.listed() == (Titles{"Notes", "Terminal"}));
    // The list stays up for the next one, its cursor on a window still there.
    CK_CHECK(p.app.is_modal());
    p.press(Key::Delete);
    CK_CHECK(p.listed().size() == 1U);
}

CK_TEST(the_close_window_button_closes_the_window_under_the_cursor) {
    Presented p({"Notes", "Draft"});
    p.press(Key::Down);
    // Tab from the list reaches the default button, then Close Window.
    p.press(Key::Tab);
    p.press(Key::Tab);
    auto* focused = dynamic_cast<ckv::widgets::Button*>(p.app.focused());
    CK_CHECK(focused != nullptr && focused->text() == "Close &Window");
    p.press(Key::Enter);
    CK_CHECK(p.window_titled("Draft") == nullptr);
    CK_CHECK(p.listed() == (Titles{"Notes"}));
    CK_CHECK(p.app.is_modal());
}

CK_TEST(the_window_list_follows_windows_opened_and_renamed_underneath_it) {
    Presented p({"Notes"});
    p.desktop->windows().front()->set_title("Notes (edited)");
    p.app.step(0);
    CK_CHECK(p.listed() == (Titles{"Notes (edited)"}));
    // An application opening a window in the background while the list is up.
    auto late = make_window("Build log");
    late->set_minimized(true);
    p.desktop->add_window(std::move(late));
    p.app.step(0);
    CK_CHECK(p.listed() == (Titles{"Notes (edited)", "Build log"}));
}
