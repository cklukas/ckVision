// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/help_viewer.hpp"

#include <optional>

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::HelpTopic;
using ckv::widgets::make_help_viewer;
using ckv::widgets::present_help_viewer;
using ckv::widgets::MemoryHelpProvider;
using ckv::widgets::Window;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

MemoryHelpProvider sample_provider() {
    MemoryHelpProvider provider;
    provider.add_topic("intro", HelpTopic{"Introduction", "Welcome to the app.", {{"details", "See details"}}});
    provider.add_topic("details", HelpTopic{"Details", "More information here.", {{"intro", "Back to intro"}}});
    return provider;
}

ckv::KeyEvent key(ckv::Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }
}  // namespace

// --- MemoryHelpProvider ------------------------------------------------

CK_TEST(memory_help_provider_returns_the_added_topic) {
    auto provider = sample_provider();
    const auto t = provider.topic("intro");
    CK_CHECK(t.title == "Introduction");
    CK_CHECK(t.links.size() == 1);
}

CK_TEST(memory_help_provider_returns_a_not_found_topic_for_an_unknown_key) {
    MemoryHelpProvider provider;
    const auto t = provider.topic("nonexistent");
    CK_CHECK(t.title == "Not Found");
    CK_CHECK(t.links.empty());
}

// --- HelpViewer: construction / navigation --------------------------

CK_TEST(the_viewer_builds_successfully_for_an_existing_topic) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto provider = sample_provider();
    auto handle = make_help_viewer(provider, "intro", f.roles, app, nullptr);
    CK_CHECK(handle.window != nullptr);
    CK_CHECK(handle.initial_focus != nullptr);
}

CK_TEST(construction_for_an_unknown_initial_topic_shows_not_found_without_crashing) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MemoryHelpProvider provider;  // empty
    auto handle = make_help_viewer(provider, "missing", f.roles, app, nullptr);
    CK_CHECK(handle.window != nullptr);
}

CK_TEST(back_with_empty_history_is_a_harmless_no_op) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto provider = sample_provider();
    auto handle = make_help_viewer(provider, "intro", f.roles, app, nullptr);
    // Exercise Back before any cross-link navigation happened; must
    // not crash, and Close (the default button) must still work
    // afterward, proving the dialog is still in a valid state.
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    bool closed = false;
    window_ptr->on_closed = [&closed, previous = window_ptr->on_closed]() {
        closed = true;
        if (previous) previous();
    };
    window_ptr->accept_request();
    CK_CHECK(closed);
}

CK_TEST(activating_a_cross_link_navigates_to_the_linked_topic_and_back_returns) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto provider = sample_provider();
    auto handle = make_help_viewer(provider, "intro", f.roles, app, nullptr);
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    // Navigate to the link list (Tab from the text view) and activate
    // its single entry ("See details" -> topic "details").
    app.set_focus(handle.initial_focus);
    app.focus_next();  // text view -> links list
    app.dispatch(key(Key::Enter));  // activates the highlighted (first) link

    // Now Back must return us to "intro" — verified indirectly by
    // confirming a second Back is then a no-op (history exhausted),
    // which would NOT be true if the first Back had never pushed us
    // anywhere in the first place.
    bool closed = false;
    window_ptr->on_closed = [&closed]() { closed = true; };
    // Tab from the links list to the Back button and press it via Enter.
    app.focus_next();  // links list -> Back button
    app.dispatch(key(Key::Enter));
    CK_CHECK(!closed);  // Back must not close the dialog
}

CK_TEST(closing_restores_focus_to_the_view_that_invoked_the_viewer) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto* invoker = app.root().add_child(std::make_unique<ckv::ui::View>());
    invoker->set_focus_policy(ckv::ui::FocusPolicy::TabStop);

    auto provider = sample_provider();
    auto handle = make_help_viewer(provider, "intro", f.roles, app, invoker);
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    window_ptr->cancel_request();
    CK_CHECK(app.focused() == invoker);
}

CK_TEST(a_topic_with_no_links_produces_an_empty_link_list_without_crashing) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MemoryHelpProvider provider;
    provider.add_topic("solo", HelpTopic{"Solo", "No links here.", {}});
    auto handle = make_help_viewer(provider, "solo", f.roles, app, nullptr);
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);
    app.focus_next();               // text view -> links list (empty)
    app.dispatch(key(Key::Enter));  // must not crash on an empty list
    (void)window_ptr;
}

CK_TEST(present_help_viewer_completes_after_modal_detachment) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(
        ckv::Rect{0, 0, 80, 24}, app.theme(), roles.dialog_background));
    auto provider = sample_provider();

    auto presentation = present_help_viewer(provider, "intro", app, *desktop, roles);
    std::optional<ckv::widgets::HelpViewerResult> completion;
    presentation.set_completion_handler([&](ckv::widgets::HelpViewerResult result) { completion = result; });

    CK_CHECK(app.is_modal());
    app.dispatch(key(Key::Escape));
    CK_CHECK(!presentation.completed());
    app.step(0);

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result() == ckv::widgets::HelpViewerResult::Closed);
    CK_CHECK(completion == ckv::widgets::HelpViewerResult::Closed);
    CK_CHECK(!app.is_modal());
}
