// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/message_box.hpp"

#include <optional>
#include <vector>

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::Application;
using ckv::ui::FocusPolicy;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::ui::View;
using ckv::widgets::Desktop;
using ckv::widgets::make_message_box;
using ckv::widgets::Window;
using ckv::widgets::MessageBoxButtons;
using ckv::widgets::MessageBoxDescriptor;
using ckv::widgets::MessageBoxResult;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};
}  // namespace

// --- Construction does not touch focus ------------------------------------

CK_TEST(make_message_box_does_not_change_the_applications_focus) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* other = app.root().add_child(std::make_unique<View>());
    other->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(other);

    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Something happened.",
                                     MessageBoxButtons::Ok};
    auto handle = make_message_box(descriptor, f.roles, app, nullptr, nullptr);
    CK_CHECK(app.focused() == other);  // still the pre-existing focus, not the box's default button
    CK_CHECK(handle.initial_focus != nullptr);
}

// --- Ok-only box -----------------------------------------------------------

CK_TEST(an_ok_only_box_fires_ok_and_closes_when_its_button_is_pressed) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    std::optional<MessageBoxResult> result;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr,
                                    [&](MessageBoxResult r) { result = r; });
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(result.has_value());
    CK_CHECK(*result == MessageBoxResult::Ok);
}

CK_TEST(escape_on_an_ok_only_box_is_equivalent_to_ok) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    std::optional<MessageBoxResult> result;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr,
                                    [&](MessageBoxResult r) { result = r; });
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(result.has_value());
    CK_CHECK(*result == MessageBoxResult::Ok);
}

// --- OkCancel ----------------------------------------------------------

CK_TEST(escape_on_an_ok_cancel_box_fires_cancel_not_ok) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Confirm, "Confirm", "Proceed?",
                                     MessageBoxButtons::OkCancel};
    std::optional<MessageBoxResult> result;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr,
                                    [&](MessageBoxResult r) { result = r; });
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(*result == MessageBoxResult::Cancel);
}

CK_TEST(the_ok_button_is_the_default_and_fires_via_enter_regardless_of_where_focus_is) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Confirm, "Confirm", "Proceed?",
                                     MessageBoxButtons::OkCancel};
    std::optional<MessageBoxResult> result;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr,
                                    [&](MessageBoxResult r) { result = r; });
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);  // OK is default and initially focused

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(*result == MessageBoxResult::Ok);
}

CK_TEST(clicking_cancel_directly_fires_cancel_and_does_not_also_fire_ok) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Confirm, "Confirm", "Proceed?",
                                     MessageBoxButtons::OkCancel};
    std::vector<MessageBoxResult> results;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr,
                                    [&](MessageBoxResult r) { results.push_back(r); });
    Window* window = handle.window.get();
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    // Tab from OK (default, first) to Cancel, then activate it via
    // Space (Button::on_key treats Space the same as Enter while the
    // button itself is focused).
    app.focus_next();
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, " "}}));
    CK_CHECK(results.size() == 1);
    CK_CHECK(results[0] == MessageBoxResult::Cancel);
    (void)window;
}

// --- YesNoCancel -------------------------------------------------------

CK_TEST(escape_on_a_yes_no_cancel_box_fires_cancel) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Confirm, "Confirm", "Save changes?",
                                     MessageBoxButtons::YesNoCancel};
    std::optional<MessageBoxResult> result;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr,
                                    [&](MessageBoxResult r) { result = r; });
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    CK_CHECK(*result == MessageBoxResult::Cancel);
}

// --- YesNo (no Cancel at all) --------------------------------------------

CK_TEST(escape_on_a_yes_no_box_fires_no) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Confirm, "Confirm", "Delete this?",
                                     MessageBoxButtons::YesNo};
    std::optional<MessageBoxResult> result;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr,
                                    [&](MessageBoxResult r) { result = r; });
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    CK_CHECK(*result == MessageBoxResult::No);
}

// --- Focus restore and no-callback safety -----------------------------

CK_TEST(closing_the_box_restores_focus_to_the_view_that_invoked_it) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* invoker = app.root().add_child(std::make_unique<View>());
    invoker->set_focus_policy(FocusPolicy::TabStop);

    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    auto handle = make_message_box(descriptor, f.roles, app, invoker, nullptr);
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    CK_CHECK(app.focused() == invoker);
}

CK_TEST(a_null_on_result_callback_does_not_crash_when_a_button_is_pressed) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    auto handle = make_message_box(descriptor, f.roles, app, nullptr, nullptr);
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
}

CK_TEST(only_one_result_ever_fires_even_though_both_accept_and_the_buttons_own_click_could_close) {
    // Regression guard for the double-close hazard documented in
    // message_box.cpp: pressing Enter routes through Window's
    // accept_request, which delegates to the default button's OWN
    // on_press (which itself closes) rather than closing a second time.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    int fire_count = 0;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr,
                                    [&](MessageBoxResult) { ++fire_count; });
    int close_count = 0;
    handle.window->on_closed = [&close_count, previous = handle.window->on_closed]() {
        ++close_count;
        if (previous) previous();
    };
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    CK_CHECK(fire_count == 1);
    CK_CHECK(close_count == 1);
}

CK_TEST(closing_the_box_schedules_its_own_removal_from_whatever_parented_it) {
    // Regression guard: on_closed used to only restore focus, leaving
    // the closed Window live in its parent's child list forever.
    // Removal is deferred via Application::post() (never inline — the
    // Button::on_press call that triggered close() is still executing
    // when on_closed fires), so it must not have happened yet right
    // after dispatch(), only after the next step() drains posted work.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    auto handle = make_message_box(descriptor, f.roles, app, nullptr, nullptr);
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    CK_CHECK(app.root().children().size() == 1);  // not yet removed
    app.step(0);                                  // drains the posted removal
    CK_CHECK(app.root().children().empty());
    (void)window_ptr;
}

// --- Desktop::show_dialog (M8/WP-5) ----------------------------------------
//
// Regression guard for finding F6: attaching a message box's handle
// via plain add_child (as examples/gallery used to) skips Desktop's
// own activation entirely, so the box rendered with INACTIVE frame
// styling while still holding focus and sat outside window cycling.
// show_dialog is the one sanctioned path and must not have that gap.

CK_TEST(show_dialog_attaches_the_box_active_and_focused_over_an_already_active_window) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* background = desktop.add_window(std::make_unique<Window>("Background"));
    CK_CHECK(background->active());

    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    auto handle = make_message_box(descriptor, f.roles, app, nullptr, nullptr);
    Window* box_ptr = handle.window.get();
    View* default_button = handle.initial_focus;
    Window* attached = desktop.show_dialog(std::move(handle), app);

    CK_CHECK(attached == box_ptr);
    CK_CHECK(box_ptr->active());           // NOT the inactive frame add_child alone would leave it with
    CK_CHECK(!background->active());       // the window underneath deactivated
    CK_CHECK(app.focused() == default_button);
    CK_CHECK(desktop.active_window() == box_ptr);
    CK_CHECK(desktop.windows().back() == box_ptr);  // participates in cycling/z-order, not a bare orphan child
}
