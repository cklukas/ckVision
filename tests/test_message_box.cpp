// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/message_box.hpp"

#include <optional>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/static_text.hpp"

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
using ckv::widgets::exec_message_box;
using ckv::widgets::make_message_box;
using ckv::widgets::present_message_box;
using ckv::widgets::Window;
using ckv::widgets::MessageBoxButtons;
using ckv::widgets::MessageBoxDescriptor;
using ckv::widgets::MessageBoxKind;
using ckv::widgets::MessageBoxResult;

namespace {

// The framework's own commands, by name. A test names the concept and
// asks the registry that assigned the ids, exactly as application code
// does — no test knows or states a command's number.
const ckv::ui::StandardCommands& standard(const ckv::ui::Application& app) {
    return app.commands().standard();
}
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

CK_TEST(message_box_kind_selects_the_matching_message_text_role) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{MessageBoxKind::Error, "Error", "Failure.", MessageBoxButtons::Ok};
    auto handle = make_message_box(descriptor, f.roles, app, nullptr, nullptr);
    handle.window->set_context(ckv::ui::Context{&f.theme, &f.registry, &app});
    Window* box = handle.window.get();
    auto* column = dynamic_cast<ckv::ui::Column*>(box->content());
    CK_CHECK(column != nullptr);
    if (column == nullptr) return;
    CK_CHECK(!column->children().empty());
    if (column->children().empty()) return;
    auto* message = dynamic_cast<ckv::widgets::StaticText*>(column->children()[0].get());
    CK_CHECK(message != nullptr);
    if (message == nullptr) return;
    message->set_bounds(Rect{0, 0, 20, 1});

    ckv::scene::Surface surface(ckv::Size{20, 1});
    ckv::scene::Painter painter(surface, Rect{0, 0, 20, 1});
    message->draw(painter);
    CK_CHECK(surface.at(ckv::Point{0, 0}).style() == f.theme.resolve(f.roles.message_error_text));
}

CK_TEST(message_box_supports_deliberate_centered_graphic_composition) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 25});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{MessageBoxKind::Info, "About", "Identity", MessageBoxButtons::Ok};
    descriptor.minimum_content_width = 70;
    descriptor.message_alignment = ckv::ui::Alignment::Center;
    descriptor.button_alignment = ckv::ui::Alignment::Center;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr, nullptr);
    // 70 content columns, the two frame cells, and the alert's one-cell
    // margin on each side.
    CK_CHECK(handle.window->horizontal_size_hint().preferred == 74);
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

CK_TEST(a_result_callback_may_destroy_its_message_box_before_the_factory_returns) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    Window* box = nullptr;
    int results = 0;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr, [&](MessageBoxResult result) {
        CK_CHECK(result == MessageBoxResult::Ok);
        ++results;
        std::unique_ptr<View> detached = app.root().remove_child(box);
        CK_CHECK(detached.get() == box);
        detached.reset();
    });
    box = static_cast<Window*>(app.root().add_child(std::move(handle.window)));
    app.set_focus(handle.initial_focus);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(results == 1);
}

CK_TEST(a_message_box_result_callback_cannot_deliver_a_reentrant_second_result) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    Window* box = nullptr;
    int results = 0;
    auto handle = make_message_box(descriptor, f.roles, app, nullptr, [&](MessageBoxResult) {
        ++results;
        box->cancel_request();
    });
    box = static_cast<Window*>(app.root().add_child(std::move(handle.window)));
    app.set_focus(handle.initial_focus);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(results == 1);
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

CK_TEST(a_deferred_self_detach_is_safe_if_the_closed_box_is_destroyed_first) {
    // The normal close path posts its self-detach because the button callback
    // is still executing. An embedding application may nevertheless destroy
    // the closed box before the next turn; the stale post must become a
    // harmless no-op rather than dereference its former Window address.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    auto handle = make_message_box(descriptor, f.roles, app, nullptr, nullptr);
    Window* const box = static_cast<Window*>(app.root().add_child(std::move(handle.window)));
    app.set_focus(handle.initial_focus);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    std::unique_ptr<View> externally_destroyed = app.root().remove_child(box);
    CK_CHECK(externally_destroyed.get() == box);
    externally_destroyed.reset();

    app.step(0);  // drains the stale self-detach post under ASan
    CK_CHECK(app.root().children().empty());
}

// --- Desktop::present_modeless (M8/WP-5) -----------------------------------
//
// present_modeless is the concise attach+focus convenience. Desktop's
// generic attachment is intentionally just as correct for a caller
// that needs to retain a WindowHandle while composing its own flow.

CK_TEST(present_modeless_attaches_the_box_active_and_focused_over_an_already_active_window) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* background = desktop.add_window(std::make_unique<Window>("Background"));
    CK_CHECK(background->active());

    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    auto handle = make_message_box(descriptor, f.roles, app, nullptr, nullptr);
    Window* box_ptr = handle.window.get();
    View* default_button = handle.initial_focus;
    Window* attached = desktop.present_modeless(std::move(handle), app);

    CK_CHECK(attached == box_ptr);
    CK_CHECK(box_ptr->active());           // NOT the inactive frame add_child alone would leave it with
    CK_CHECK(!background->active());       // the window underneath deactivated
    CK_CHECK(app.focused() == default_button);
    CK_CHECK(desktop.active_window() == box_ptr);
    CK_CHECK(desktop.windows().back() == box_ptr);  // participates in cycling/z-order, not a bare orphan child
}

CK_TEST(generic_desktop_attachment_of_a_message_box_preserves_window_management_and_safe_close) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(app.root().add_child(std::move(desktop_owned)));

    auto background_owned = std::make_unique<Window>("Background");
    auto input_owned = std::make_unique<ckv::widgets::InputLine>();
    ckv::widgets::InputLine* background_focus = input_owned.get();
    background_owned->set_content(std::move(input_owned));
    Window* background = desktop->add_window(std::move(background_owned));
    app.set_focus(background_focus);

    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.", MessageBoxButtons::Ok};
    auto handle = make_message_box(descriptor, roles, app, background_focus, nullptr);
    Window* box = handle.window.get();
    View* button = handle.initial_focus;
    Window* attached = static_cast<Window*>(desktop->add_child(std::move(handle.window)));
    app.set_focus(button);  // present_modeless's focus step, explicit here by choice

    CK_CHECK(attached == box);
    CK_CHECK(box->active());
    CK_CHECK(!background->active());
    CK_CHECK(desktop->windows().size() == 2);
    desktop->activate_previous();
    CK_CHECK(desktop->active_window() == background);
    desktop->activate_next();
    CK_CHECK(desktop->active_window() == box);

    term.inject_event(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    CK_CHECK(app.step(0));
    CK_CHECK(desktop->windows().size() == 1);
    CK_CHECK(desktop->active_window() == background);
    CK_CHECK(background->active());
    CK_CHECK(app.focused() == background_focus);
}

// --- present_message_box (D-038's non-blocking modal path) ----------------

CK_TEST(present_message_box_scopes_input_immediately_and_completes_after_detach) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(app.root().add_child(std::move(desktop_owned)));

    bool background_command_ran = false;
    app.commands().declare({.key = "test.background",
                            .title = "&Background",
                            .category = "Test",
                            .chord = "Alt+B",
                            .handler = [&] { background_command_ran = true; }});

    const MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.",
                                           MessageBoxButtons::Ok};
    auto presentation = present_message_box(app, *desktop, roles, descriptor);
    std::optional<MessageBoxResult> completion;
    bool modal_was_gone_at_completion = false;
    presentation.set_completion_handler([&](MessageBoxResult result) {
        completion = result;
        modal_was_gone_at_completion = !app.is_modal();
    });

    CK_CHECK(app.is_modal());
    CK_CHECK(desktop->windows().size() == 1);
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "b"}});
    CK_CHECK(!background_command_ran);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    CK_CHECK(!presentation.completed());  // close schedules detachment; scope remains until then
    app.step(0);

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result() == MessageBoxResult::Ok);
    CK_CHECK(completion == MessageBoxResult::Ok);
    CK_CHECK(modal_was_gone_at_completion);
    CK_CHECK(desktop->windows().empty());
}

CK_TEST(present_message_box_external_detach_uses_the_documented_escape_result) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(app.root().add_child(std::move(desktop_owned)));

    const MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Confirm, "Confirm", "Proceed?",
                                           MessageBoxButtons::OkCancel};
    auto presentation = present_message_box(app, *desktop, roles, descriptor);
    Window* const box = desktop->windows().back();
    std::unique_ptr<Window> detached = desktop->remove_window(box);

    CK_CHECK(detached != nullptr);
    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result() == MessageBoxResult::Cancel);
    CK_CHECK(!app.is_modal());

    int completion_count = 0;
    presentation.set_completion_handler([&](MessageBoxResult result) {
        ++completion_count;
        CK_CHECK(result == MessageBoxResult::Cancel);
    });
    CK_CHECK(completion_count == 1);
}

CK_TEST(present_message_box_quit_sweep_completes_with_its_escape_result_after_detach) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(
        std::make_unique<Desktop>(Rect{0, 0, 80, 24}));

    const MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Confirm, "Confirm", "Proceed?",
                                           MessageBoxButtons::OkCancel};
    auto presentation = present_message_box(app, *desktop, roles, descriptor);

    CK_CHECK(app.execute_command(standard(app).quit));
    CK_CHECK(app.quit_requested());
    CK_CHECK(!presentation.completed());
    app.step(0);

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result() == MessageBoxResult::Cancel);
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());
}

CK_TEST(a_vetoed_presented_message_box_remains_modal_until_a_later_close_succeeds) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(
        std::make_unique<Desktop>(Rect{0, 0, 80, 24}));

    auto presentation = present_message_box(
        app, *desktop, roles,
        MessageBoxDescriptor{ckv::widgets::MessageBoxKind::Confirm, "Confirm", "Proceed?", MessageBoxButtons::OkCancel});
    Window* const box = desktop->windows().back();
    int close_requests = 0;
    box->close_request = [&] {
        ++close_requests;
        return false;
    };

    CK_CHECK(!box->close());
    CK_CHECK(close_requests == 1);
    CK_CHECK(!presentation.completed());
    CK_CHECK(app.is_modal());
    CK_CHECK(desktop->windows().size() == 1);

    box->close_request = {};
    CK_CHECK(box->close());
    CK_CHECK(!presentation.completed());
    app.step(0);

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result() == MessageBoxResult::Cancel);
    CK_CHECK(!app.is_modal());
    CK_CHECK(desktop->windows().empty());
}

CK_TEST(a_vetoed_presented_message_box_also_vetoes_its_quit_sweep) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(
        std::make_unique<Desktop>(Rect{0, 0, 80, 24}));

    auto presentation = present_message_box(
        app, *desktop, roles,
        MessageBoxDescriptor{ckv::widgets::MessageBoxKind::Confirm, "Confirm", "Proceed?", MessageBoxButtons::OkCancel});
    Window* const box = desktop->windows().back();
    box->close_request = [] { return false; };

    CK_CHECK(app.execute_command(standard(app).quit));
    CK_CHECK(!app.quit_requested());
    CK_CHECK(!presentation.completed());
    CK_CHECK(app.is_modal());
    CK_CHECK(desktop->windows().size() == 1);
}

CK_TEST(a_message_box_completion_handler_can_present_a_nested_modal_after_outer_detachment) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(
        std::make_unique<Desktop>(Rect{0, 0, 80, 24}));

    const MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.",
                                           MessageBoxButtons::Ok};
    auto outer = present_message_box(app, *desktop, roles, descriptor);
    std::optional<ckv::widgets::MessageBoxPresentation> inner;
    outer.set_completion_handler([&](MessageBoxResult result) {
        CK_CHECK(result == MessageBoxResult::Ok);
        CK_CHECK(!app.is_modal());
        // The outer window's focused button is already inside the subtree
        // being detached. Completion must never expose that soon-to-die
        // pointer to a nested presentation as its focus-restore target.
        CK_CHECK(app.focused() == nullptr);
        inner.emplace(present_message_box(app, *desktop, roles, descriptor));
    });

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    app.step(0);
    CK_CHECK(outer.completed());
    CK_CHECK(inner.has_value());
    CK_CHECK(app.is_modal());
    CK_CHECK(desktop->windows().size() == 1);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    app.step(0);
    CK_CHECK(!app.is_modal());
    CK_CHECK(desktop->windows().empty());
}

// --- exec_message_box (M9/WP-15, D-021's blocking convenience) -------------
//
// Unlike the tests above, these need `app`'s OWN theme/roles actually
// styled (not a separate Fixture's) — exec_message_box's pump calls
// step(), which paints the box for real, and a real paint resolves
// roles through context().theme — the theme that propagated from
// `app.root()` when the box attached, not whatever Fixture happens to
// sit alongside it unconnected. Every real application does this same
// `app.theme() = make_classic_theme(app.roles(), roles)` dance in its
// own constructor; these tests just do it inline.

CK_TEST(exec_message_box_returns_the_pressed_result_in_a_headless_script) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(app.root().add_child(std::move(desktop_owned)));

    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.",
                                     MessageBoxButtons::Ok};
    // exec_message_box blocks until the box closes — nothing can inject
    // the dismissing key from outside a single-threaded call, so it's
    // queued via post() ahead of time and drains during the pump's own
    // first step() call, exactly like a headless script would arrange.
    app.post([&app] { app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}); });

    const MessageBoxResult result = exec_message_box(app, *desktop, roles, descriptor);
    CK_CHECK(result == MessageBoxResult::Ok);
    CK_CHECK(desktop->windows().empty());  // closed and self-detached, not left lingering
}

CK_TEST(exec_message_box_returns_the_escape_result_when_dismissed_via_esc) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(app.root().add_child(std::move(desktop_owned)));

    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Confirm", "Proceed?",
                                     MessageBoxButtons::OkCancel};
    app.post([&app] { app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}); });

    const MessageBoxResult result = exec_message_box(app, *desktop, roles, descriptor);
    CK_CHECK(result == MessageBoxResult::Cancel);
}

CK_TEST(exec_message_box_restores_focus_to_whatever_was_focused_before_the_call) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(app.root().add_child(std::move(desktop_owned)));

    auto* invoker = app.root().add_child(std::make_unique<View>());
    invoker->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(invoker);

    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Info", "Done.",
                                     MessageBoxButtons::Ok};
    app.post([&app] { app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}); });

    exec_message_box(app, *desktop, roles, descriptor);
    CK_CHECK(app.focused() == invoker);
}

CK_TEST(exec_message_box_host_quit_returns_fallback_and_detaches_the_open_box) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(app.root().add_child(std::move(desktop_owned)));

    MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Confirm", "Proceed?",
                                     MessageBoxButtons::YesNoCancel};
    app.post([&app] { app.request_quit(); });

    const MessageBoxResult result = exec_message_box(app, *desktop, roles, descriptor);
    CK_CHECK(result == MessageBoxResult::Cancel);
    CK_CHECK(app.quit_requested());
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());
}

// --- How wide, and how tall, an alert opens --------------------------------

namespace {

// The shape of an About: an identity line, a rule about it, and a second
// paragraph -- prose, with no line breaks of its own inside a paragraph.
const char* const kAboutText =
    "ckVision Example\n\nA window per rotating solid. Every frame is drawn on a worker thread, "
    "handed back through the application, and shown by invalidating one view -- no polling, and "
    "no forced redraw. Drag a window's corner to resize it: the next frame is rendered at the "
    "new pixel size, on the window's own background.";

// Presents an Ok box on a desktop of the given size and answers the window
// it opened. The presentation is deliberately dropped: these tests ask about
// geometry, and nothing here dismisses the box.
Window* open_about(Application& app, Desktop& desktop, const StandardRoles& roles,
                   const char* message) {
    MessageBoxDescriptor descriptor{MessageBoxKind::Info, "About", message, MessageBoxButtons::Ok};
    descriptor.emphasized_leading_lines = 1;
    auto presentation = present_message_box(app, desktop, roles, descriptor);
    presentation.set_completion_handler([](MessageBoxResult) {});
    return desktop.active_window();
}

// The message inside a box, for the question "did all of it fit".
ckv::widgets::StaticText* message_of(Window& box) {
    auto* column = dynamic_cast<ckv::ui::Column*>(box.content());
    if (column == nullptr) return nullptr;
    for (const std::unique_ptr<View>& child : column->children())
        if (auto* text = dynamic_cast<ckv::widgets::StaticText*>(child.get())) return text;
    return nullptr;
}

}  // namespace

CK_TEST(a_prose_alert_opens_at_a_readable_measure_however_wide_the_terminal_is) {
    // A paragraph asked for its unwrapped length, and an alert built around
    // it spanned the whole terminal: two lines of text ruled across 200
    // columns. The measure is what it asks for now, and the terminal getting
    // wider no longer widens the alert.
    ckv::term::HeadlessTerminal term(ckv::Size{200, 40});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = static_cast<Desktop*>(
        app.root().add_child(std::make_unique<Desktop>(Rect{0, 0, 200, 40})));

    Window* box = open_about(app, *desktop, roles, kAboutText);
    CK_CHECK(box != nullptr);
    if (box == nullptr) return;
    // The measure, the alert's one-cell margin either side, and two frame cells.
    CK_CHECK(box->bounds().width == ckv::widgets::kProseMeasureCells + 4);
    // And centred on the desktop rather than pinned to its left edge.
    CK_CHECK(box->bounds().x > 0);
}

CK_TEST(an_alert_opens_tall_enough_for_the_text_it_wrapped) {
    // The height came from counting the message's own line breaks, which for
    // a paragraph is one -- so the box opened short and cut its own prose off
    // at the frame. It is sized from the wrapped text now.
    ckv::term::HeadlessTerminal term(ckv::Size{200, 40});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = static_cast<Desktop*>(
        app.root().add_child(std::make_unique<Desktop>(Rect{0, 0, 200, 40})));

    Window* box = open_about(app, *desktop, roles, kAboutText);
    CK_CHECK(box != nullptr);
    if (box == nullptr) return;
    app.step(0);

    ckv::widgets::StaticText* message = message_of(*box);
    CK_CHECK(message != nullptr);
    if (message == nullptr) return;
    CK_CHECK(message->bounds().height >= message->height_for_width(message->bounds().width));
    CK_CHECK(box->bounds().height <= 40);
}

CK_TEST(a_short_alert_is_no_wider_than_the_sentence_it_carries) {
    // The measure is a ceiling on what prose asks for, not a width every
    // alert is padded out to.
    ckv::term::HeadlessTerminal term(ckv::Size{200, 40});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = static_cast<Desktop*>(
        app.root().add_child(std::make_unique<Desktop>(Rect{0, 0, 200, 40})));

    Window* box = open_about(app, *desktop, roles, "Saved.");
    CK_CHECK(box != nullptr);
    if (box == nullptr) return;
    CK_CHECK(box->bounds().width < ckv::widgets::kProseMeasureCells);
}

CK_TEST(an_alert_that_would_not_fit_the_desktops_height_widens_until_it_does) {
    // Widening is what the box has left when the measure leaves it taller
    // than the room there is: prose given more columns needs fewer rows.
    // Clipping the text instead would be the one outcome a reader cannot
    // recover from -- there is nothing to scroll.
    // Ten rows: at the measure this About needs more than that, so the box
    // has to find its own way to fit.
    ckv::term::HeadlessTerminal term(ckv::Size{200, 10});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = static_cast<Desktop*>(
        app.root().add_child(std::make_unique<Desktop>(Rect{0, 0, 200, 10})));

    Window* box = open_about(app, *desktop, roles, kAboutText);
    CK_CHECK(box != nullptr);
    if (box == nullptr) return;
    CK_CHECK(box->height_for_width(ckv::widgets::kProseMeasureCells + 4) > 10);  // the premise
    CK_CHECK(box->bounds().width > ckv::widgets::kProseMeasureCells + 4);
    CK_CHECK(box->bounds().height <= 10);
    // No wider than it had to be, either: one column narrower and the text
    // would not have fitted the desktop.
    CK_CHECK(box->height_for_width(box->bounds().width) <= 10);
    CK_CHECK(box->height_for_width(box->bounds().width - 1) > 10);
}
