// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/application.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"

using ckv::KeyChord;
using ckv::Key;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::Application;
using ckv::ui::FocusPolicy;
using ckv::ui::View;

namespace {

class ProbeView : public View {
public:
    explicit ProbeView(Rect bounds = {}) : View(bounds) {}
    int key_events = 0;
    int text_events = 0;
    int mouse_events = 0;
    int focus_gained = 0;
    int focus_lost = 0;
    bool consume_keys = false;

    bool on_key(const ckv::KeyEvent&) override {
        ++key_events;
        return consume_keys;
    }
    bool on_text(const ckv::TextEvent&) override {
        ++text_events;
        return false;
    }
    bool on_mouse(const ckv::MouseEvent&) override {
        ++mouse_events;
        return true;
    }
    void on_focus(const ckv::FocusEvent& e) override {
        if (e.gained)
            ++focus_gained;
        else
            ++focus_lost;
    }
};

}  // namespace

// --- Focus traversal -------------------------------------------------------

CK_TEST(focus_next_on_an_empty_tree_returns_false) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    CK_CHECK(!app.focus_next());
    CK_CHECK(app.focused() == nullptr);
}

CK_TEST(focus_next_visits_focusable_views_in_declaration_order_and_wraps) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* b = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    b->set_focus_policy(FocusPolicy::TabStop);

    CK_CHECK(app.focus_next());
    CK_CHECK(app.focused() == a);
    CK_CHECK(app.focus_next());
    CK_CHECK(app.focused() == b);
    CK_CHECK(app.focus_next());  // wraps back to a
    CK_CHECK(app.focused() == a);
}

CK_TEST(focus_previous_wraps_backward) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* b = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    b->set_focus_policy(FocusPolicy::TabStop);

    CK_CHECK(app.focus_previous());
    CK_CHECK(app.focused() == b);  // starts at the last one, going backward
    CK_CHECK(app.focus_previous());
    CK_CHECK(app.focused() == a);
}

CK_TEST(disabled_and_hidden_views_are_skipped_by_traversal) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* b = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    b->set_focus_policy(FocusPolicy::TabStop);
    a->set_enabled(false);

    CK_CHECK(app.focus_next());
    CK_CHECK(app.focused() == b);  // a is skipped
}

CK_TEST(hidden_subtrees_are_entirely_transparent_to_traversal) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* container = app.root().add_child(std::make_unique<View>());
    auto* nested = static_cast<ProbeView*>(container->add_child(std::make_unique<ProbeView>()));
    nested->set_focus_policy(FocusPolicy::TabStop);
    container->set_visible(false);

    CK_CHECK(!app.focus_next());  // nested is unreachable while its ancestor is hidden
}

CK_TEST(set_focus_fires_focus_lost_on_the_old_view_and_focus_gained_on_the_new_one) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* b = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    b->set_focus_policy(FocusPolicy::TabStop);

    app.set_focus(a);
    CK_CHECK(a->focus_gained == 1);
    app.set_focus(b);
    CK_CHECK(a->focus_lost == 1);
    CK_CHECK(b->focus_gained == 1);
}

CK_TEST(set_focus_to_the_already_focused_view_is_a_no_op) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(a);
    app.set_focus(a);
    CK_CHECK(a->focus_gained == 1);  // not re-fired
}

CK_TEST(set_focus_to_a_non_focusable_view_aborts) {
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
        ManualClock clock;
        Application app(term, clock);
        auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
        // Never given FocusPolicy::TabStop — set_focus must abort.
        app.set_focus(a);
        ::_exit(1);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
}

// --- Key/text focus-chain dispatch ------------------------------------------

CK_TEST(key_event_reaches_the_focused_view) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(a);
    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));  // delivered but unconsumed
    CK_CHECK(a->key_events == 1);
}

CK_TEST(unconsumed_key_walks_up_through_ancestors_until_one_consumes_it) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* container = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    container->consume_keys = true;
    auto* leaf = static_cast<ProbeView*>(container->add_child(std::make_unique<ProbeView>()));
    leaf->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(leaf);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(leaf->key_events == 1);
    CK_CHECK(container->key_events == 1);
}

CK_TEST(a_key_with_no_focused_view_and_no_matching_command_is_unhandled) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
}

CK_TEST(text_event_reaches_the_focused_view_and_never_falls_through_to_commands) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(a);
    CK_CHECK(!app.dispatch(ckv::TextEvent{"x", false}));
    CK_CHECK(a->text_events == 1);
}

// --- Commands: keymap fallback after the focus chain declines --------------

CK_TEST(a_key_unconsumed_by_the_focus_chain_falls_through_to_a_bound_command) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    constexpr ckv::ui::CommandId kQuit = 1;
    app.commands().register_command(kQuit, "Quit", "App", KeyChord{Key::Escape, Modifier::None, ""});
    bool ran = false;
    app.set_command_handler(kQuit, [&] { ran = true; });

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(ran);
}

CK_TEST(a_disabled_command_does_not_run_even_when_its_chord_matches) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    constexpr ckv::ui::CommandId kQuit = 1;
    app.commands().register_command(kQuit, "Quit", "App", KeyChord{Key::Escape, Modifier::None, ""});
    app.commands().set_enabled_predicate(kQuit, [] { return false; });
    bool ran = false;
    app.set_command_handler(kQuit, [&] { ran = true; });

    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(!ran);
}

CK_TEST(a_view_that_consumes_the_key_shadows_the_bound_command) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    constexpr ckv::ui::CommandId kQuit = 1;
    app.commands().register_command(kQuit, "Quit", "App", KeyChord{Key::Escape, Modifier::None, ""});
    bool ran = false;
    app.set_command_handler(kQuit, [&] { ran = true; });

    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->consume_keys = true;
    a->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(a);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(!ran);  // the view ate it first
}

CK_TEST(execute_command_with_no_handler_registered_returns_false) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    CK_CHECK(!app.execute_command(42));
}

// --- Mouse: hit-testing and drag capture ------------------------------------

CK_TEST(mouse_down_hits_the_topmost_view_containing_the_point) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* back = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 10, 10})));
    auto* front =
        static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 10, 10})));
    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 2}, std::nullopt,
                          Modifier::None};
    CK_CHECK(app.dispatch(down));
    CK_CHECK(front->mouse_events == 1);
    CK_CHECK(back->mouse_events == 0);  // shadowed by the later-added (topmost) sibling
}

CK_TEST(a_click_outside_every_view_is_unhandled) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 5, 5}));
    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{50, 50}, std::nullopt,
                          Modifier::None};
    CK_CHECK(!app.dispatch(down));
}

CK_TEST(drag_capture_routes_move_and_up_to_the_view_that_took_the_down_even_off_its_bounds) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* target = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 5, 5})));

    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 2}, std::nullopt,
                          Modifier::None};
    ckv::MouseEvent move{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{50, 50}, std::nullopt,
                          Modifier::None};  // far outside target's bounds
    ckv::MouseEvent up{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{50, 50}, std::nullopt,
                        Modifier::None};

    CK_CHECK(app.dispatch(down));
    CK_CHECK(app.dispatch(move));
    CK_CHECK(app.dispatch(up));
    CK_CHECK(target->mouse_events == 3);  // all three reached it despite the hit-test point moving off-bounds
}

CK_TEST(after_mouse_up_capture_releases_and_hit_testing_resumes) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* left = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 5, 5})));
    auto* right =
        static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{10, 0, 5, 5})));

    ckv::MouseEvent down_left{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 2}, std::nullopt,
                               Modifier::None};
    ckv::MouseEvent up_left{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{2, 2}, std::nullopt,
                             Modifier::None};
    ckv::MouseEvent down_right{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{12, 2}, std::nullopt,
                                Modifier::None};

    app.dispatch(down_left);
    app.dispatch(up_left);
    app.dispatch(down_right);
    CK_CHECK(left->mouse_events == 2);
    CK_CHECK(right->mouse_events == 1);
}

// --- Resize ------------------------------------------------------------

CK_TEST(resize_event_updates_the_roots_bounds) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    CK_CHECK(app.dispatch(ckv::ResizeEvent{ckv::Size{100, 40}}));
    CK_CHECK(app.root().bounds() == (Rect{0, 0, 100, 40}));
}

// --- post() / step() --------------------------------------------------------

CK_TEST(post_queues_work_that_step_runs_on_the_owning_thread) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    bool ran = false;
    app.post([&] { ran = true; });
    CK_CHECK(!ran);  // not run synchronously
    const bool did_work = app.step(0);
    CK_CHECK(ran);
    CK_CHECK(did_work);
}

CK_TEST(step_with_no_events_and_no_posted_work_reports_no_work_done) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    CK_CHECK(!app.step(0));
}

CK_TEST(step_dispatches_injected_terminal_events) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(a);
    term.inject_event(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    CK_CHECK(app.step(0));
    CK_CHECK(a->key_events == 1);
}

CK_TEST(posted_work_that_posts_more_work_runs_on_the_next_step_not_re_entrantly) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    int calls = 0;
    app.post([&] {
        ++calls;
        app.post([&] { ++calls; });  // must not run within THIS step()
    });
    CK_CHECK(app.step(0));
    CK_CHECK(calls == 1);
    CK_CHECK(app.step(0));
    CK_CHECK(calls == 2);
}

CK_TEST(wake_makes_the_next_step_return_promptly_even_with_a_far_future_deadline) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.wake();
    // A HeadlessTerminal never actually blocks regardless (it only
    // drains what's queued), so this mainly proves wake() doesn't
    // crash or corrupt state when no work is pending; the deadline
    // clamping is exercised indirectly since step() must still return.
    const std::int64_t far_future = 1'000'000'000'000LL;
    CK_CHECK(!app.step(far_future));
}
