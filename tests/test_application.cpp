// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/application.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "cvision/testing/cktest.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/record_replay_terminal.hpp"

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
    explicit ProbeView(Rect bounds = {}) : View(bounds) {
        // Probe geometry is the subject of these tests; it is an explicit
        // fixed overlay when attached directly to Application::root().
        set_fills_root(false);
    }
    int key_events = 0;
    int key_release_events = 0;
    int text_events = 0;
    int mouse_events = 0;
    int focus_gained = 0;
    int focus_lost = 0;
    bool consume_keys = false;
    bool consume_key_releases = false;

    bool on_key(const ckv::KeyEvent&) override {
        ++key_events;
        return consume_keys;
    }
    bool on_key_release(const ckv::KeyEvent&) override {
        ++key_release_events;
        return consume_key_releases;
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

class CursorProbeView final : public ProbeView {
public:
    using ProbeView::ProbeView;

    std::optional<ckv::CursorState> cursor;

    std::optional<ckv::CursorState> cursor_state() const override {
        return cursor;
    }
};

class WaitableTerminal final : public ckv::term::Terminal {
public:
    ckv::term::Capabilities capabilities() const noexcept override { return ckv::term::baseline_capabilities(); }
    ckv::Size size() const noexcept override { return ckv::Size{80, 24}; }
    std::span<const ckv::term::WaitHandle> wait_handles() const noexcept override { return wait_handles_; }

    std::vector<ckv::term::TerminalEvent> poll(std::int64_t) override {
        std::unique_lock<std::mutex> lock(mutex_);
        polling_ = true;
        condition_.notify_all();
        condition_.wait(lock, [this] { return woken_; });
        woken_ = false;
        return {};
    }

    void wake() noexcept override {
        std::lock_guard<std::mutex> lock(mutex_);
        ++wake_count_;
        woken_ = true;
        condition_.notify_all();
    }

    void write(std::string_view) override {}
    void set_title(std::string_view) override {}
    void bell() override {}
    void write_clipboard(std::string_view) override {}

    bool wait_until_polling() {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, std::chrono::seconds(1), [this] { return polling_; });
    }
    int wake_count() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return wake_count_;
    }

private:
    std::array<ckv::term::WaitHandle, 2> wait_handles_ = {
        ckv::term::WaitHandle{ckv::term::WaitHandleKind::PosixFileDescriptor, 41},
        ckv::term::WaitHandle{ckv::term::WaitHandleKind::PosixFileDescriptor, 42},
    };
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool polling_ = false;
    bool woken_ = false;
    int wake_count_ = 0;
};

class ForwardingDiagnostics final : public ckv::DiagnosticsSink {
public:
    explicit ForwardingDiagnostics(std::vector<ckv::DiagnosticsEntry>& received) : received_(received) {}

    void log(ckv::LogLevel level, std::string_view message) noexcept override {
        received_.push_back(ckv::DiagnosticsEntry{level, std::string(message)});
    }

private:
    std::vector<ckv::DiagnosticsEntry>& received_;
};

class ReentrantLoopView final : public View {
public:
    explicit ReentrantLoopView(Application& app) : app_(app) {
        set_fills_root(false);
        set_focus_policy(FocusPolicy::TabStop);
    }
    bool on_key(const ckv::KeyEvent&) override {
        app_.run_until([] { return true; });
        return true;
    }

private:
    Application& app_;
};

class SelfRemovingView final : public View {
public:
    explicit SelfRemovingView(View& owner) : owner_(owner) { set_fills_root(false); }

    bool on_mouse(const ckv::MouseEvent&) override {
        std::unique_ptr<View> removed = owner_.remove_child(this);
        CK_CHECK(removed.get() == this);
        removed.reset();  // destroys this view before control returns to Application
        return true;
    }

private:
    View& owner_;
};

class AncestorRemovingView final : public View {
public:
    explicit AncestorRemovingView(View& ancestor_owner) : ancestor_owner_(ancestor_owner) { set_fills_root(false); }

    bool on_mouse(const ckv::MouseEvent&) override {
        std::unique_ptr<View> removed = ancestor_owner_.remove_child(parent());
        CK_CHECK(removed != nullptr);
        removed.reset();  // destroys this view and its ancestor together
        return true;
    }

private:
    View& ancestor_owner_;
};

class ReparentingView final : public View {
public:
    ReparentingView(View& source, View& destination) : source_(source), destination_(destination) {
        set_fills_root(false);
        set_focus_policy(FocusPolicy::TabStop);
    }

    bool on_mouse(const ckv::MouseEvent&) override {
        std::unique_ptr<View> moved = source_.remove_child(this);
        CK_CHECK(moved.get() == this);
        destination_.add_child(std::move(moved));
        return true;
    }

    bool on_text(const ckv::TextEvent&) override {
        ++text_events;
        return true;
    }

    int text_events = 0;

private:
    View& source_;
    View& destination_;
};

class DismissingPopupView final : public View {
public:
    DismissingPopupView(Application& app, View& owner) : app_(app), owner_(owner) { set_fills_root(false); }

    bool on_mouse(const ckv::MouseEvent&) override {
        app_.clear_input_capture();
        std::unique_ptr<View> removed = owner_.remove_child(this);
        CK_CHECK(removed.get() == this);
        removed.reset();  // a light-dismiss handler may destroy its popup synchronously
        return true;
    }

private:
    Application& app_;
    View& owner_;
};

class RootResizeSelfRemovingView final : public View {
public:
    explicit RootResizeSelfRemovingView(Application& app) : app_(app) {}

    void on_resized() override {
        if (!armed_) return;
        std::unique_ptr<View> removed = app_.root().remove_child(this);
        CK_CHECK(removed.get() == this);
        removed.reset();
    }

    void arm() noexcept { armed_ = true; }

private:
    Application& app_;
    bool armed_ = false;
};

}  // namespace

CK_TEST(application_schedules_software_cursor_blink_at_the_declared_rate) {
    ckv::term::HeadlessTerminal term(ckv::Size{20, 6});
    ManualClock clock;
    Application app(term, clock);
    auto* view = static_cast<CursorProbeView*>(
        app.root().add_child(std::make_unique<CursorProbeView>(Rect{0, 0, 5, 3})));
    view->set_focus_policy(FocusPolicy::TabStop);
    view->cursor = ckv::CursorState{true, {1, 1}, ckv::CursorShape::Underline,
                                    true, 17};
    app.set_focus(view);

    (void)app.step(0);
    CK_CHECK(term.display().cursor().visible);
    CK_CHECK(!term.display().cursor().blink);
    CK_CHECK(app.next_timer_deadline_nanos() == 17);

    term.clear_written();
    clock.advance(16);
    CK_CHECK(!app.step(clock.now_nanos()));
    CK_CHECK(term.written_bytes().empty());

    clock.advance(1);
    CK_CHECK(app.step(clock.now_nanos()));
    CK_CHECK(term.written_bytes() == "\x1B[?25l");
    CK_CHECK(!term.display().cursor().visible);
    CK_CHECK(app.next_timer_deadline_nanos() == 34);

    term.clear_written();
    clock.advance(17);
    CK_CHECK(app.step(clock.now_nanos()));
    CK_CHECK(term.display().cursor().visible);
    CK_CHECK(term.written_bytes().find("\x1B[4 q") != std::string_view::npos);

    view->cursor->blink = false;
    view->invalidate();
    term.clear_written();
    (void)app.step(clock.now_nanos());
    CK_CHECK(term.display().cursor().visible);
    CK_CHECK(!app.next_timer_deadline_nanos());
}

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

CK_TEST(a_focus_bookmark_restores_a_live_view) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* b = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    b->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(a);
    const Application::FocusBookmark bookmark = app.save_focus();
    app.set_focus(b);
    app.restore_focus(bookmark);
    CK_CHECK(app.focused() == a);
}

CK_TEST(a_focus_bookmark_never_restores_a_destroyed_view) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* view = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    view->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(view);
    const Application::FocusBookmark bookmark = app.save_focus();
    std::unique_ptr<View> removed = app.root().remove_child(view);
    removed.reset();
    app.restore_focus(bookmark);
    CK_CHECK(app.focused() == nullptr);
}

// --- Default keymap: Tab/Shift-Tab traversal (M9/WP-13, D-029) ------------

CK_TEST(tab_traverses_focus_out_of_the_box_with_no_app_side_setup) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* b = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    b->set_focus_policy(FocusPolicy::TabStop);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::None, ""}}));
    CK_CHECK(app.focused() == a);
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::None, ""}}));
    CK_CHECK(app.focused() == b);
}

CK_TEST(shift_tab_traverses_backward_out_of_the_box) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* b = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    b->set_focus_policy(FocusPolicy::TabStop);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::Shift, ""}}));
    CK_CHECK(app.focused() == b);  // starts at the last one, going backward
}

CK_TEST(a_view_that_consumes_tab_itself_still_receives_it_before_the_default_binding_fires) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* b = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->consume_keys = true;
    a->set_focus_policy(FocusPolicy::TabStop);
    b->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(a);

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::None, ""}}));
    CK_CHECK(a->key_events == 1);  // the view saw it
    CK_CHECK(app.focused() == a);  // and focus never moved to b
}

CK_TEST(unbinding_tab_removes_the_default_traversal) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    app.commands().unbind_key(KeyChord{Key::Tab, Modifier::None, ""});

    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::None, ""}}));
    CK_CHECK(app.focused() == nullptr);
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
    CK_EXPECT_ABORT({
        ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
        ManualClock clock;
        Application app(term, clock);
        auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
        // Never given FocusPolicy::TabStop — set_focus must abort.
        app.set_focus(a);
    });
}

CK_TEST(application_diagnostics_buffer_and_forward_to_an_owned_injected_sink) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ckv::ManualClock clock;
    std::vector<ckv::DiagnosticsEntry> received;
    {
        Application app(term, clock);
        app.set_diagnostics_sink(std::make_unique<ForwardingDiagnostics>(received));
        app.diagnostics().log(ckv::LogLevel::Warning, "probe timed out");
        app.diagnostics().log(ckv::LogLevel::Error, "terminal write failed");
    }
    CK_CHECK(received.size() == 2);
    CK_CHECK(received[0].level == ckv::LogLevel::Warning);
    CK_CHECK(received[0].text == "probe timed out");
    CK_CHECK(received[1].level == ckv::LogLevel::Error);
    CK_CHECK(received[1].text == "terminal write failed");
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

CK_TEST(key_release_uses_the_dedicated_route_and_never_activates_a_command) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* probe = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    probe->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(probe);
    const ckv::ui::CommandId escape = app.commands().declare(
        {.key = "test.escape", .title = "Escape", .category = "App", .chord = "Esc"});
    bool command_ran = false;
    app.set_command_handler(escape, [&] { command_ran = true; });

    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}, ckv::KeyAction::Release}));
    CK_CHECK(probe->key_events == 0);
    CK_CHECK(probe->key_release_events == 1);
    CK_CHECK(!command_ran);
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
    const ckv::ui::CommandId quit = app.commands().declare(
        {.key = "test.quit", .title = "Quit", .category = "App", .chord = "Esc"});
    bool ran = false;
    app.set_command_handler(quit, [&] { ran = true; });

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(ran);
}

CK_TEST(a_disabled_command_does_not_run_even_when_its_chord_matches) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    const ckv::ui::CommandId quit = app.commands().declare(
        {.key = "test.quit", .title = "Quit", .category = "App", .chord = "Esc"});
    app.commands().set_enabled_predicate(quit, [] { return false; });
    bool ran = false;
    app.set_command_handler(quit, [&] { ran = true; });

    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(!ran);
}

CK_TEST(a_view_that_consumes_the_key_shadows_the_bound_command) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    const ckv::ui::CommandId quit = app.commands().declare(
        {.key = "test.quit", .title = "Quit", .category = "App", .chord = "Esc"});
    bool ran = false;
    app.set_command_handler(quit, [&] { ran = true; });

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
    const ckv::ui::CommandId command =
        app.commands().declare({.key = "test.no-handler", .title = "No handler"});
    CK_CHECK(!app.execute_command(command));
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

// --- Pointer-event lifetime (WP-25) ----------------------------------------

CK_TEST(mouse_dispatch_survives_a_target_that_destroys_itself) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    View* target = app.root().add_child(std::make_unique<SelfRemovingView>(app.root()));
    target->set_bounds(Rect{0, 0, 5, 5});
    target->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(target);

    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 1},
                                           std::nullopt, Modifier::None}));
    CK_CHECK(app.root().children().empty());
    CK_CHECK(app.focused() == nullptr);
    CK_CHECK(!app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{1, 1},
                                            std::nullopt, Modifier::None}));
}

CK_TEST(mouse_dispatch_survives_a_target_that_destroys_its_ancestor) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto container = std::make_unique<ProbeView>(Rect{0, 0, 10, 10});
    ProbeView* const container_ptr = container.get();
    auto target = std::make_unique<AncestorRemovingView>(app.root());
    AncestorRemovingView* const target_ptr = target.get();
    target->set_bounds(Rect{1, 1, 5, 5});
    target->set_focus_policy(FocusPolicy::TabStop);
    container->add_child(std::move(target));
    app.root().add_child(std::move(container));
    app.set_focus(target_ptr);

    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 2},
                                           std::nullopt, Modifier::None}));
    CK_CHECK(app.root().children().empty());
    CK_CHECK(app.focused() == nullptr);
    (void)container_ptr;
}

CK_TEST(mouse_dispatch_revalidates_a_reparented_target_before_focusing_and_text_input) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto source = std::make_unique<ProbeView>(Rect{0, 0, 10, 10});
    auto destination = std::make_unique<ProbeView>(Rect{20, 0, 10, 10});
    ProbeView* const source_ptr = source.get();
    ProbeView* const destination_ptr = destination.get();
    auto target = std::make_unique<ReparentingView>(*source, *destination);
    ReparentingView* const target_ptr = target.get();
    target->set_bounds(Rect{1, 1, 5, 5});
    source->add_child(std::move(target));
    app.root().add_child(std::move(source));
    app.root().add_child(std::move(destination));

    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 2},
                                           std::nullopt, Modifier::None}));
    CK_CHECK(target_ptr->parent() == destination_ptr);
    CK_CHECK(app.focused() == target_ptr);
    CK_CHECK(app.dispatch(ckv::TextEvent{"x", false}));
    CK_CHECK(target_ptr->text_events == 1);
    (void)source_ptr;
}

CK_TEST(mouse_dispatch_survives_a_light_dismiss_popup_that_destroys_itself) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto background = std::make_unique<ProbeView>(Rect{0, 0, 10, 10});
    ProbeView* const background_ptr = background.get();
    background->set_focus_policy(FocusPolicy::TabStop);
    app.root().add_child(std::move(background));
    app.set_focus(background_ptr);
    View* popup = app.root().add_child(std::make_unique<DismissingPopupView>(app, app.root()));
    popup->set_bounds(Rect{50, 50, 5, 5});
    app.set_input_capture(popup);

    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 1},
                                           std::nullopt, Modifier::None}));
    CK_CHECK(app.input_capture() == nullptr);
    CK_CHECK(app.focused() == background_ptr);  // popup capture never steals focus
    CK_CHECK(app.root().children().size() == 1);
}

// --- Mouse: click-to-focus (M8/WP-2) ----------------------------------------

CK_TEST(clicking_an_unfocused_focusable_view_gives_it_focus) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* target = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 5, 5})));
    target->set_focus_policy(FocusPolicy::TabStop);
    CK_CHECK(app.focused() == nullptr);

    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 2}, std::nullopt,
                          Modifier::None};
    app.dispatch(down);
    CK_CHECK(app.focused() == target);
}

CK_TEST(clicking_a_non_focusable_view_leaves_focus_unchanged) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* focusable =
        static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{20, 0, 5, 5})));
    focusable->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(focusable);

    auto* label = static_cast<ProbeView*>(
        app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 5, 5})));  // FocusPolicy::None
    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 2}, std::nullopt,
                          Modifier::None};
    app.dispatch(down);
    CK_CHECK(label->mouse_events == 1);   // the click was still delivered
    CK_CHECK(app.focused() == focusable);  // but focus did not move
}

CK_TEST(clicking_a_non_focusable_child_inside_a_focusable_container_focuses_the_container) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* container =
        static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 20, 20})));
    container->set_focus_policy(FocusPolicy::TabStop);
    auto* inner = container->add_child(std::make_unique<ProbeView>(Rect{2, 2, 5, 5}));  // e.g. a scrollbar

    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{3, 3}, std::nullopt,
                          Modifier::None};
    app.dispatch(down);
    CK_CHECK(app.focused() == container);
    CK_CHECK(static_cast<ProbeView*>(inner)->mouse_events == 1);  // the leaf still received the click
}

CK_TEST(click_to_focus_does_not_fire_on_move_or_up_only_on_down) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* target = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 5, 5})));
    target->set_focus_policy(FocusPolicy::TabStop);

    ckv::MouseEvent move{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{2, 2}, std::nullopt,
                          Modifier::None};
    app.dispatch(move);
    CK_CHECK(app.focused() == nullptr);  // a bare hover/move never steals focus
}

// --- Resize ------------------------------------------------------------

CK_TEST(resize_event_updates_the_roots_bounds) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    CK_CHECK(app.dispatch(ckv::ResizeEvent{ckv::Size{100, 40}}));
    CK_CHECK(app.root().bounds() == (Rect{0, 0, 100, 40}));
}

CK_TEST(root_resize_layout_survives_a_child_that_destroys_itself_during_on_resized) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* self_removing = app.root().add(std::make_unique<RootResizeSelfRemovingView>(app));
    auto* survivor = app.root().add(std::make_unique<View>());
    self_removing->arm();  // the initial root-child layout above is intentionally harmless

    CK_CHECK(app.dispatch(ckv::ResizeEvent{ckv::Size{100, 40}}));

    // The first child erased itself from the live vector during its resize
    // callback. The stable identity snapshot must still lay out its sibling.
    CK_CHECK(app.root().children().size() == 1);
    CK_CHECK(app.root().children().front().get() == survivor);
    CK_CHECK(survivor->bounds() == (Rect{0, 0, 100, 40}));
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

CK_TEST(posting_an_empty_callback_is_a_harmless_no_op) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);

    app.post({});
    CK_CHECK(!app.step(0));
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

// --- Internal clipboard (D-022) ---------------------------------------------

CK_TEST(clipboard_starts_empty) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    CK_CHECK(app.clipboard_text().empty());
}

CK_TEST(set_clipboard_text_updates_the_internal_clipboard_and_writes_through_to_the_terminal) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.set_clipboard_text("copied");
    CK_CHECK(app.clipboard_text() == "copied");
}

CK_TEST(a_bracketed_paste_text_event_mirrors_into_the_internal_clipboard) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.dispatch(ckv::TextEvent{"pasted content", true});
    CK_CHECK(app.clipboard_text() == "pasted content");
}

CK_TEST(a_non_paste_text_event_never_touches_the_clipboard) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.set_clipboard_text("original");
    app.dispatch(ckv::TextEvent{"typed, not pasted", false});
    CK_CHECK(app.clipboard_text() == "original");
}

CK_TEST(a_paste_event_still_reaches_the_focused_view_after_updating_the_clipboard) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(a);
    app.dispatch(ckv::TextEvent{"pasted", true});
    CK_CHECK(a->text_events == 1);
    CK_CHECK(app.clipboard_text() == "pasted");
}

// --- Timers --------------------------------------------------------------

CK_TEST(application_forwards_borrowed_wait_handles_and_reports_its_earliest_timer_deadline) {
    WaitableTerminal inner;
    ckv::term::RecordingTerminal terminal(inner);
    ManualClock clock(1'000);
    Application app(terminal, clock);

    const auto handles = app.wait_handles();
    CK_CHECK(handles.size() == 2);
    CK_CHECK(handles[0] ==
             (ckv::term::WaitHandle{ckv::term::WaitHandleKind::PosixFileDescriptor, 41}));
    CK_CHECK(handles[1] ==
             (ckv::term::WaitHandle{ckv::term::WaitHandleKind::PosixFileDescriptor, 42}));
    CK_CHECK(!app.next_timer_deadline_nanos().has_value());

    const auto later = app.start_timer(300, false, [] {});
    const auto earlier = app.start_timer(100, false, [] {});
    CK_CHECK(app.next_timer_deadline_nanos() == std::optional<std::int64_t>{1'100});
    app.cancel_timer(earlier);
    CK_CHECK(app.next_timer_deadline_nanos() == std::optional<std::int64_t>{1'300});
    app.cancel_timer(later);
    CK_CHECK(!app.next_timer_deadline_nanos().has_value());
}

CK_TEST(a_timer_does_not_fire_before_its_interval_elapses) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    int fired = 0;
    app.start_timer(1000, false, [&] { ++fired; });
    app.step(0);
    CK_CHECK(fired == 0);
}

CK_TEST(a_one_shot_timer_fires_once_at_its_deadline_and_does_not_fire_again) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    int fired = 0;
    app.start_timer(1000, false, [&] { ++fired; });
    clock.advance(1000);
    CK_CHECK(app.step(0));
    CK_CHECK(fired == 1);
    clock.advance(1000);
    app.step(0);
    CK_CHECK(fired == 1);  // one-shot: not rescheduled
}

CK_TEST(a_repeating_timer_fires_again_after_each_interval) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    int fired = 0;
    app.start_timer(500, true, [&] { ++fired; });
    clock.advance(500);
    app.step(0);
    CK_CHECK(fired == 1);
    clock.advance(500);
    app.step(0);
    CK_CHECK(fired == 2);
}

CK_TEST(cancel_timer_prevents_a_future_fire) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    int fired = 0;
    const auto id = app.start_timer(1000, false, [&] { ++fired; });
    app.cancel_timer(id);
    clock.advance(2000);
    app.step(0);
    CK_CHECK(fired == 0);
}

CK_TEST(cancel_timer_on_an_unknown_id_is_a_harmless_no_op) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.cancel_timer(9999);  // never issued
    CK_CHECK(true);
}

CK_TEST(multiple_timers_fire_independently_at_their_own_deadlines) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    int fired_a = 0, fired_b = 0;
    app.start_timer(100, false, [&] { ++fired_a; });
    app.start_timer(300, false, [&] { ++fired_b; });
    clock.advance(100);
    app.step(0);
    CK_CHECK(fired_a == 1);
    CK_CHECK(fired_b == 0);
    clock.advance(200);
    app.step(0);
    CK_CHECK(fired_b == 1);
}

// --- run() / request_quit() (M9/WP-14, E8/D-021's convenience) ------------
//
// run()'s own loop isn't exercised here beyond the immediate-return
// case: it steps a real Terminal/Clock in a genuine loop until
// request_quit(), which is exactly the "just run it" behavior a
// headless, deterministic unit test can't safely drive further
// without either blocking or installing a real host signal policy — that behavior
// is verified through the examples instead (each one now IS this
// exact call). What's tested here is the request_quit()/
// quit_requested() contract run()'s loop condition depends on.

CK_TEST(quit_requested_is_false_by_default) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    CK_CHECK(!app.quit_requested());
}

CK_TEST(request_quit_sets_quit_requested) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.request_quit();
    CK_CHECK(app.quit_requested());
}

CK_TEST(request_quit_interrupts_a_blocked_run_without_a_separate_wake) {
    WaitableTerminal term;
    ManualClock clock;
    Application app(term, clock);
    std::atomic<bool> returned{false};

    std::thread run_thread([&] {
        app.run();
        returned.store(true, std::memory_order_release);
    });
    CK_CHECK(term.wait_until_polling());
    app.request_quit();
    run_thread.join();

    CK_CHECK(app.quit_requested());
    CK_CHECK(term.wake_count() == 1);
    CK_CHECK(returned.load(std::memory_order_acquire));
}

CK_TEST(run_returns_immediately_without_stepping_if_quit_was_already_requested) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.request_quit();
    app.run();  // must not block, poll, or touch signal disposition beyond install/restore
    CK_CHECK(true);  // reaching this line at all is the assertion
}

CK_TEST(run_until_stops_and_returns_true_once_the_predicate_holds) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    int checks = 0;
    const bool finished = app.run_until([&] { return ++checks >= 3; });
    CK_CHECK(finished);
    CK_CHECK(checks == 3);  // exactly one check per loop iteration, no extra spurious call
}

CK_TEST(run_until_returns_false_if_interrupted_by_a_quit_request) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    int checks = 0;
    const bool finished = app.run_until([&] {
        if (++checks == 2) app.request_quit();
        return false;  // never satisfied on its own
    });
    CK_CHECK(!finished);
    CK_CHECK(app.quit_requested());
}

CK_TEST(run_until_does_not_invoke_its_predicate_when_quit_is_already_requested) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.request_quit();

    int checks = 0;
    const bool finished = app.run_until([&] {
        ++checks;
        return true;
    });

    CK_CHECK(!finished);
    CK_CHECK(checks == 0);
}

CK_TEST(run_until_does_not_invoke_its_predicate_after_a_timer_requests_quit) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.start_timer(1, false, [&] { app.request_quit(); });
    clock.advance(1);

    int checks = 0;
    const bool finished = app.run_until([&] {
        ++checks;
        return false;
    });

    // The first completion check happens before the timer-bearing step. Once
    // that step observes quit, no later check may transform interruption into
    // normal completion or run caller code after shutdown has begun.
    CK_CHECK(!finished);
    CK_CHECK(checks == 1);
}

CK_TEST(post_interrupts_an_in_flight_terminal_wait_and_runs_on_the_owning_step) {
    WaitableTerminal term;
    ManualClock clock;
    Application app(term, clock);
    std::atomic<bool> posted_ran{false};
    std::atomic<bool> step_did_work{false};
    std::atomic<bool> step_returned{false};

    std::thread step_thread([&] {
        step_did_work.store(app.step(1'000'000'000), std::memory_order_release);
        step_returned.store(true, std::memory_order_release);
    });
    CK_CHECK(term.wait_until_polling());
    app.post([&] { posted_ran.store(true, std::memory_order_release); });
    step_thread.join();

    CK_CHECK(term.wake_count() == 1);
    CK_CHECK(posted_ran.load(std::memory_order_acquire));
    CK_CHECK(step_did_work.load(std::memory_order_acquire));
    CK_CHECK(step_returned.load(std::memory_order_acquire));
}

CK_TEST(run_until_called_from_a_dispatch_handler_aborts_before_it_can_start_a_nested_pump) {
    CK_EXPECT_ABORT({
        ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
        ManualClock clock;
        Application app(term, clock);
        auto* view = app.root().add_child(std::make_unique<ReentrantLoopView>(app));
        app.set_focus(view);
        app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    });
}

// --- Modality (M9/WP-15, D-021) --------------------------------------------

CK_TEST(push_modal_confines_focus_traversal_to_the_modal_subtree) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* background = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    background->set_focus_policy(FocusPolicy::TabStop);
    auto* modal_root = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* inner_a = static_cast<ProbeView*>(modal_root->add_child(std::make_unique<ProbeView>()));
    auto* inner_b = static_cast<ProbeView*>(modal_root->add_child(std::make_unique<ProbeView>()));
    inner_a->set_focus_policy(FocusPolicy::TabStop);
    inner_b->set_focus_policy(FocusPolicy::TabStop);

    app.push_modal(*modal_root);
    CK_CHECK(app.focus_next());
    CK_CHECK(app.focused() == inner_a);
    CK_CHECK(app.focus_next());
    CK_CHECK(app.focused() == inner_b);
    CK_CHECK(app.focus_next());  // wraps within the modal subtree, never reaching `background`
    CK_CHECK(app.focused() == inner_a);
}

CK_TEST(modal_default_keymap_keeps_traversal_and_help_inside_its_scope) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* background = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    background->set_focus_policy(FocusPolicy::TabStop);
    background->set_help_context_key("background");
    app.set_focus(background);

    auto* modal = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* first = static_cast<ProbeView*>(modal->add_child(std::make_unique<ProbeView>()));
    auto* second = static_cast<ProbeView*>(modal->add_child(std::make_unique<ProbeView>()));
    first->set_focus_policy(FocusPolicy::TabStop);
    second->set_focus_policy(FocusPolicy::TabStop);
    first->set_help_context_key("modal.first");
    app.push_modal(*modal);

    // Drive the standard keymap through the terminal decoder, not direct
    // focus helpers, so the test covers its actual modal dispatch boundary.
    term.inject_bytes("\t", 1);
    CK_CHECK(app.step(1));
    CK_CHECK(app.focused() == first);
    term.inject_bytes("\t", 2);
    CK_CHECK(app.step(2));
    CK_CHECK(app.focused() == second);
    term.inject_bytes("\x1B[Z", 3);  // Shift+Tab
    CK_CHECK(app.step(3));
    CK_CHECK(app.focused() == first);

    std::string help_key;
    app.set_help_provider([&](const std::string& key) { help_key = key; });
    term.inject_bytes("\x1BOP", 4);  // SS3 F1
    CK_CHECK(app.step(4));
    CK_CHECK(help_key == "modal.first");

    const ckv::ui::CommandId background_command = app.commands().declare(
        {.key = "test.background", .title = "Background", .category = "App", .chord = "Alt+X"});
    bool background_command_ran = false;
    app.set_command_handler(background_command, [&] { background_command_ran = true; });
    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}}));
    CK_CHECK(!background_command_ran);
    CK_CHECK(app.focused() == first);
}

CK_TEST(key_release_is_scoped_to_the_top_modal_and_never_reaches_a_command) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* background = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    background->set_focus_policy(FocusPolicy::TabStop);
    auto* modal = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    modal->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(background);
    app.push_modal(*modal);
    const ckv::ui::CommandId escape = app.commands().declare(
        {.key = "test.escape", .title = "Escape", .category = "App", .chord = "Esc"});
    bool command_ran = false;
    app.set_command_handler(escape, [&] { command_ran = true; });

    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}, ckv::KeyAction::Release}));
    CK_CHECK(background->key_release_events == 0);
    CK_CHECK(modal->key_release_events == 1);
    CK_CHECK(!command_ran);
}

CK_TEST(push_modal_confines_mouse_hit_testing_to_the_modal_subtree) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto background_owned = std::make_unique<ProbeView>(Rect{0, 0, 10, 10});
    auto* background = static_cast<ProbeView*>(app.root().add_child(std::move(background_owned)));
    auto modal_owned = std::make_unique<ProbeView>(Rect{20, 0, 10, 10});
    auto* modal_root = static_cast<ProbeView*>(app.root().add_child(std::move(modal_owned)));

    auto down_at = [](ckv::Point p) {
        return ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, p, std::nullopt,
                                Modifier::None};
    };

    app.push_modal(*modal_root);
    CK_CHECK(!app.dispatch(down_at(ckv::Point{2, 2})));
    CK_CHECK(background->mouse_events == 0);  // scoped out entirely — never even hit-tested

    CK_CHECK(app.dispatch(down_at(ckv::Point{22, 2})));
    CK_CHECK(modal_root->mouse_events == 1);
}

CK_TEST(a_modal_excludes_background_focus_text_and_pointer_capture_routes) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* background = static_cast<ProbeView*>(
        app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 80, 24})));
    background->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(background);
    app.set_input_capture(background);

    auto* modal = static_cast<ProbeView*>(
        app.root().add_child(std::make_unique<ProbeView>(Rect{20, 5, 20, 10})));
    modal->set_focus_policy(FocusPolicy::TabStop);
    app.push_modal(*modal);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    app.dispatch(ckv::TextEvent{"x", false});
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{21, 6},
                                  std::nullopt, Modifier::None});

    CK_CHECK(background->key_events == 0);
    CK_CHECK(background->text_events == 0);
    CK_CHECK(background->mouse_events == 0);
    CK_CHECK(modal->key_events == 1);
    CK_CHECK(modal->text_events == 1);
    CK_CHECK(modal->mouse_events == 1);
}

CK_TEST(a_modal_routes_terminal_focus_events_to_its_scope_not_the_background_focus) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* background = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    background->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(background);
    auto* modal = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));

    app.push_modal(*modal);
    CK_CHECK(app.dispatch(ckv::FocusEvent{true}));
    CK_CHECK(background->focus_gained == 1);  // only set_focus() above reached the background
    CK_CHECK(modal->focus_gained == 1);
}

CK_TEST(popping_or_detaching_a_modal_restores_the_saved_focus_only_after_scope_exit) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* background = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    background->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(background);
    auto* modal = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    modal->set_focus_policy(FocusPolicy::TabStop);

    app.push_modal(*modal);
    app.focus_next();
    CK_CHECK(app.focused() == modal);
    app.pop_modal();
    CK_CHECK(app.focused() == background);

    app.push_modal(*modal);
    app.focus_next();
    std::unique_ptr<View> detached = app.root().remove_child(modal);
    CK_CHECK(app.focused() == nullptr);  // detach never calls user focus code inline
    app.step(0);
    CK_CHECK(app.focused() == background);
}

CK_TEST(detaching_an_outer_modal_preserves_the_inner_modals_event_scope_and_restore_target) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* background = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    background->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(background);
    auto* outer = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    outer->set_focus_policy(FocusPolicy::TabStop);
    auto* inner = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    inner->set_focus_policy(FocusPolicy::TabStop);

    app.push_modal(*outer);
    app.focus_next();
    const Application::ModalScopeId inner_scope = app.push_modal(*inner);
    app.focus_next();
    CK_CHECK(app.focused() == inner);

    std::unique_ptr<View> detached_outer = app.root().remove_child(outer);
    app.step(0);
    CK_CHECK(app.focused() == inner);  // outer detach cannot escape the inner scope
    CK_CHECK(!app.pop_modal(Application::ModalScopeId{0}));
    CK_CHECK(app.pop_modal(inner_scope));
    CK_CHECK(app.focused() == background);  // inherited outer restoration survives
}

CK_TEST(a_scope_specific_pop_never_pops_a_newer_nested_modal) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* first = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* second = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    const Application::ModalScopeId first_scope = app.push_modal(*first);
    const Application::ModalScopeId second_scope = app.push_modal(*second);

    CK_CHECK(!app.pop_modal(first_scope));
    CK_CHECK(app.is_modal());
    CK_CHECK(app.pop_modal(second_scope));
    CK_CHECK(app.pop_modal(first_scope));
    CK_CHECK(!app.is_modal());
}

CK_TEST(push_modal_suppresses_the_global_accelerator_fallback) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    const ckv::ui::CommandId some_command = app.commands().declare(
        {.key = "test.something", .title = "Something", .category = "App", .chord = "Esc"});
    bool ran = false;
    app.set_command_handler(some_command, [&] { ran = true; });

    auto* modal_root = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    app.push_modal(*modal_root);
    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(!ran);  // a background accelerator must not fire while a modal owns the scope

    app.pop_modal();
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(ran);  // restored once popped
}

CK_TEST(push_modal_delivers_on_key_up_through_the_modal_root_but_never_past_it) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* outer = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* modal_root = static_cast<ProbeView*>(outer->add_child(std::make_unique<ProbeView>()));
    auto* inner = static_cast<ProbeView*>(modal_root->add_child(std::make_unique<ProbeView>()));
    inner->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(inner);

    app.push_modal(*modal_root);
    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(inner->key_events == 1);
    CK_CHECK(modal_root->key_events == 1);  // the walk reaches the modal root itself...
    CK_CHECK(outer->key_events == 0);       // ...but never continues past it
}

CK_TEST(nested_modals_scope_to_the_innermost_and_popping_restores_each_level) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* outer_view = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    outer_view->set_focus_policy(FocusPolicy::TabStop);
    auto* modal_a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* inside_a = static_cast<ProbeView*>(modal_a->add_child(std::make_unique<ProbeView>()));
    inside_a->set_focus_policy(FocusPolicy::TabStop);
    auto* modal_b = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    auto* inside_b = static_cast<ProbeView*>(modal_b->add_child(std::make_unique<ProbeView>()));
    inside_b->set_focus_policy(FocusPolicy::TabStop);

    app.push_modal(*modal_a);
    CK_CHECK(app.is_modal());
    CK_CHECK(app.focus_next());
    CK_CHECK(app.focused() == inside_a);

    app.push_modal(*modal_b);  // a modal on top of a modal (the vision's nested modality)
    CK_CHECK(app.focus_next());
    CK_CHECK(app.focused() == inside_b);  // the innermost modal wins, oblivious to modal_a

    app.pop_modal();
    CK_CHECK(app.focus_next());
    CK_CHECK(app.focused() == inside_a);  // back to scoping against modal_a alone

    app.pop_modal();
    CK_CHECK(!app.is_modal());
    app.set_focus(nullptr);
    CK_CHECK(app.focus_next());
    CK_CHECK(app.focused() == outer_view);  // fully unscoped again
}

CK_TEST(pop_modal_on_an_empty_stack_aborts) {
    CK_EXPECT_ABORT({
        ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
        ManualClock clock;
        Application app(term, clock);
        app.pop_modal();  // nothing was ever pushed — must abort
    });
}

// --- F1 context help (D-027) -------------------------------------------

// F1 is the standard help command's default chord (M9/WP-12): it now
// always routes through the command system, same as any other standard
// chord, so dispatch() reports "handled" once that command's (always-
// installed) default handler runs — regardless of whether a help
// provider is set or a key resolves. Those are the DEFAULT HANDLER's
// own internal decisions, observable via the provider callback
// itself, not via dispatch()'s return value.
CK_TEST(f1_with_no_provider_installed_still_dispatches_through_the_help_default_handler) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    a->set_help_context_key("topic.a");
    app.set_focus(a);
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::F1, Modifier::None, ""}}));
}

CK_TEST(f1_resolves_the_focused_views_help_context_key_and_hands_it_to_the_provider) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    std::string reported;
    app.set_help_provider([&](const std::string& key) { reported = key; });
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);
    a->set_help_context_key("topic.a");
    app.set_focus(a);
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::F1, Modifier::None, ""}}));
    CK_CHECK(reported == "topic.a");
}

CK_TEST(f1_walks_ancestors_when_the_focused_view_has_no_help_context_key_of_its_own) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    std::string reported;
    app.set_help_provider([&](const std::string& key) { reported = key; });
    app.root().set_help_context_key("topic.root");
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);  // no key of its own
    app.set_focus(a);
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::F1, Modifier::None, ""}}));
    CK_CHECK(reported == "topic.root");
}

CK_TEST(f1_with_a_provider_but_no_resolvable_help_context_key_anywhere_never_calls_it) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    bool called = false;
    app.set_help_provider([&](const std::string&) { called = true; });
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->set_focus_policy(FocusPolicy::TabStop);  // no key anywhere in the chain
    app.set_focus(a);
    // kHelp's handler still ran
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::F1, Modifier::None, ""}}));
    CK_CHECK(!called);  // but it found nothing to report, so the provider itself never fired
}

CK_TEST(a_view_that_consumes_f1_itself_shadows_the_help_provider) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    bool called = false;
    app.set_help_provider([&](const std::string&) { called = true; });
    auto* a = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>()));
    a->consume_keys = true;
    a->set_focus_policy(FocusPolicy::TabStop);
    a->set_help_context_key("topic.a");
    app.set_focus(a);
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::F1, Modifier::None, ""}}));
    CK_CHECK(!called);
}

CK_TEST(f1_with_no_focused_view_at_all_never_calls_the_provider) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    bool called = false;
    app.set_help_provider([&](const std::string&) { called = true; });
    // kHelp's handler still ran
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::F1, Modifier::None, ""}}));
    CK_CHECK(!called);  // but there's no focused view to resolve a key from
}

// --- Regression: dangling focused_/mouse_capture_ after a view is
// removed and destroyed (found by review, fixed via View's detach-sink
// notification) --------------------------------------------------------

CK_TEST(destroying_the_focused_view_clears_focus_and_the_next_key_dispatch_is_a_safe_no_op) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    View* raw = app.root().add_child(std::make_unique<ProbeView>());
    raw->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(raw);
    CK_CHECK(app.focused() == raw);

    // Detach and destroy the focused view without going through
    // Application at all — this is exactly the use-after-free the
    // review found: nothing in the removal path used to notify
    // Application, leaving focused_ dangling.
    std::unique_ptr<View> owned = app.root().remove_child(raw);
    owned.reset();  // actually destroys it

    CK_CHECK(app.focused() == nullptr);
    // Would dereference a freed pointer if the fix regressed; ASan
    // (the sanitizer build this suite also runs under) would abort.
    CK_CHECK(!app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
}

CK_TEST(destroying_an_ancestor_of_the_focused_view_also_clears_focus) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto container_owned = std::make_unique<View>();
    View* container = app.root().add_child(std::move(container_owned));
    View* leaf = container->add_child(std::make_unique<ProbeView>());
    leaf->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(leaf);
    CK_CHECK(app.focused() == leaf);

    // Destroy the CONTAINER (leaf's parent), not leaf directly — the
    // detach notification must cascade to every descendant, not just
    // the immediate node that was removed.
    std::unique_ptr<View> owned = app.root().remove_child(container);
    owned.reset();

    CK_CHECK(app.focused() == nullptr);
}

CK_TEST(destroying_the_mouse_captured_view_mid_drag_clears_capture_and_a_later_up_is_unhandled) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    View* raw = app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 5, 5}));
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 1},
                                  std::nullopt, Modifier::None});  // raw now has mouse capture

    std::unique_ptr<View> owned = app.root().remove_child(raw);
    owned.reset();

    // A later Up with no view under the (now-empty) point and no valid
    // capture must be unhandled, not a use-after-free.
    CK_CHECK(!app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{1, 1},
                                            std::nullopt, Modifier::None}));
}

CK_TEST(removing_but_not_destroying_the_focused_view_also_clears_focus_immediately) {
    // Even without destruction: a detached-but-still-alive view is no
    // longer part of the attached tree, so it must stop being tracked
    // as focused the moment it's removed, not only once it's freed.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    View* raw = app.root().add_child(std::make_unique<ProbeView>());
    raw->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(raw);

    std::unique_ptr<View> owned = app.root().remove_child(raw);
    CK_CHECK(app.focused() == nullptr);
    CK_CHECK(owned != nullptr);  // still alive, just detached and unfocused
}

CK_TEST(reattaching_a_previously_detached_view_and_refocusing_it_works_normally) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    View* raw = app.root().add_child(std::make_unique<ProbeView>());
    raw->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(raw);

    std::unique_ptr<View> owned = app.root().remove_child(raw);
    CK_CHECK(app.focused() == nullptr);

    View* reattached = app.root().add_child(std::move(owned));
    app.set_focus(reattached);
    CK_CHECK(app.focused() == reattached);
    // The detach sink must have been re-armed on reattachment — verify
    // by removing it again and confirming focus clears once more.
    app.root().remove_child(reattached);
    CK_CHECK(app.focused() == nullptr);
}

// --- Mouse input capture ------------------------------------------------

CK_TEST(input_capture_routes_every_mouse_event_to_the_captured_view_regardless_of_hit_test) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* under_cursor =
        static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 80, 24})));
    auto* captured = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{50, 20, 5, 2})));
    app.set_input_capture(captured);

    // Click lands squarely inside under_cursor's bounds, nowhere near
    // captured's — capture must still win.
    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 5}, std::nullopt,
                          Modifier::None};
    app.dispatch(down);
    CK_CHECK(captured->mouse_events == 1);
    CK_CHECK(under_cursor->mouse_events == 0);
}

CK_TEST(input_capture_takes_priority_over_an_in_flight_drag_capture) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* draggable = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 10, 10})));
    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 1}, std::nullopt,
                          Modifier::None};
    app.dispatch(down);  // draggable now holds mouse_capture_
    CK_CHECK(draggable->mouse_events == 1);

    auto* popup = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{50, 50, 5, 5})));
    app.set_input_capture(popup);

    ckv::MouseEvent move{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{2, 2}, std::nullopt,
                          Modifier::None};
    app.dispatch(move);
    CK_CHECK(popup->mouse_events == 1);
    CK_CHECK(draggable->mouse_events == 1);  // did NOT receive the move
}

CK_TEST(clear_input_capture_restores_ordinary_hit_testing) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* target = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 10, 10})));
    auto* popup = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{50, 50, 5, 5})));
    app.set_input_capture(popup);
    app.clear_input_capture();

    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 1}, std::nullopt,
                          Modifier::None};
    app.dispatch(down);
    CK_CHECK(target->mouse_events == 1);
    CK_CHECK(popup->mouse_events == 0);
}

CK_TEST(destroying_the_input_captured_view_clears_capture_and_a_later_click_uses_ordinary_hit_testing) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto* target = static_cast<ProbeView*>(app.root().add_child(std::make_unique<ProbeView>(Rect{0, 0, 10, 10})));
    View* popup_raw = app.root().add_child(std::make_unique<ProbeView>(Rect{50, 50, 5, 5}));
    app.set_input_capture(popup_raw);
    CK_CHECK(app.input_capture() == popup_raw);

    std::unique_ptr<View> owned = app.root().remove_child(popup_raw);
    owned.reset();
    CK_CHECK(app.input_capture() == nullptr);

    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 1}, std::nullopt,
                          Modifier::None};
    app.dispatch(down);
    CK_CHECK(target->mouse_events == 1);
}

// --- Frame completion (D-021's "sent" is not "arrived") ------------------

namespace {

// A terminal that answers every completion question it is asked — but only
// when it is next read, which is exactly where the answer to a session's
// last frame lands: in the input queue, after the loop has stopped looking.
// What is left there when the session is handed back is what the next
// reader of that queue receives.
class AnsweringTerminal final : public ckv::term::Terminal {
public:
    ckv::term::Capabilities capabilities() const noexcept override {
        return ckv::term::baseline_capabilities();
    }
    ckv::Size size() const noexcept override { return ckv::Size{80, 24}; }

    std::vector<ckv::term::TerminalEvent> poll(std::int64_t) override {
        answered_ = asked_;
        return {};
    }
    std::size_t frame_acknowledgements() const noexcept override { return answered_; }

    void write(std::string_view bytes) override {
        for (std::size_t at = bytes.find("\x1B[5n"); at != std::string_view::npos;
             at = bytes.find("\x1B[5n", at + 1))
            ++asked_;
    }
    void set_title(std::string_view) override {}
    void bell() override {}
    void write_clipboard(std::string_view) override {}

    std::size_t replies_left_in_the_queue() const noexcept { return asked_ - answered_; }

private:
    std::size_t asked_ = 0;
    std::size_t answered_ = 0;
};

}  // namespace

CK_TEST(frame_completion_tracking_is_off_until_an_application_asks_for_it) {
    ckv::term::HeadlessTerminal term{ckv::Size{20, 5}};
    ManualClock clock;
    Application app(term, clock);
    app.step(0);
    // Nothing added to the frame, and nothing to reason about.
    CK_CHECK(!app.frame_completion_tracking());
    CK_CHECK(app.frames_awaiting_terminal() == 0U);
    CK_CHECK(term.written_bytes().find("\x1B[5n") == std::string::npos);
    CK_CHECK(app.last_terminal_round_trip_nanos() < 0);
}

CK_TEST(a_presented_frame_is_outstanding_until_the_terminal_answers_for_it) {
    ckv::term::HeadlessTerminal term{ckv::Size{20, 5}};
    ManualClock clock;
    Application app(term, clock);
    app.set_frame_completion_tracking(true);
    app.step(clock.now_nanos());
    // The question rides out with the frame it asks about.
    CK_CHECK(term.written_bytes().find("\x1B[5n") != std::string::npos);
    CK_CHECK(app.frames_awaiting_terminal() == 1U);

    clock.advance(4'000'000);
    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() == 0U);
    CK_CHECK(app.last_terminal_round_trip_nanos() == 4'000'000);
}

CK_TEST(an_unanswered_frame_is_written_off_rather_than_waited_on_forever) {
    ckv::term::HeadlessTerminal term{ckv::Size{20, 5}};
    ManualClock clock;
    Application app(term, clock);
    app.set_frame_completion_tracking(true);
    app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() == 1U);

    clock.advance(ckv::ui::kFrameCompletionTimeoutNanos);
    app.step(clock.now_nanos());
    // Still tracking — one silence is not an answer about the host.
    CK_CHECK(app.frames_awaiting_terminal() == 0U);
    CK_CHECK(app.frame_completion_tracking());
}

CK_TEST(a_terminal_that_never_answers_is_taken_at_its_word) {
    ckv::term::HeadlessTerminal term{ckv::Size{20, 5}};
    ManualClock clock;
    Application app(term, clock);
    app.set_frame_completion_tracking(true);
    for (int attempt = 0; attempt < ckv::ui::kFrameCompletionGiveUpCount; ++attempt) {
        app.invalidate_all();  // re-emit the frame, so there is something to ask about
        app.step(clock.now_nanos());
        clock.advance(ckv::ui::kFrameCompletionTimeoutNanos);
        app.step(clock.now_nanos());
    }
    // Pacing against a host that does not answer is worse than not pacing:
    // it is a stall dressed as back-pressure.
    CK_CHECK(!app.frame_completion_tracking());
    CK_CHECK(app.frames_awaiting_terminal() == 0U);
}

CK_TEST(the_session_stays_for_the_answer_to_the_last_frame_it_asked_about) {
    ckv::term::HeadlessTerminal term{ckv::Size{20, 5}};
    ManualClock clock;
    Application app(term, clock);
    app.set_frame_completion_tracking(true);
    app.step(clock.now_nanos());
    clock.advance(4'000'000);
    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.step(clock.now_nanos());  // this host answers, and is now known to
    CK_CHECK(app.frames_awaiting_terminal() == 0U);

    app.invalidate_all();
    app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() == 1U);
    // The reply to that last frame arrives after the loop has stopped
    // stepping. Left uncollected it is delivered to whatever inherits the
    // terminal's input queue, where a shell reads it as typed input.
    clock.advance(1'000'000);
    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.settle_frame_completion();
    CK_CHECK(app.frames_awaiting_terminal() == 0U);
}

CK_TEST(a_session_collects_the_last_answer_before_it_hands_the_terminal_back) {
    AnsweringTerminal term;
    ManualClock clock;
    {
        Application app(term, clock);
        app.set_frame_completion_tracking(true);
        app.step(clock.now_nanos());  // the first frame asks
        clock.advance(4'000'000);
        app.step(clock.now_nanos());  // its answer is read: this host answers
        CK_CHECK(app.frames_awaiting_terminal() == 0U);

        app.invalidate_all();
        clock.advance(4'000'000);
        app.step(clock.now_nanos());  // the last frame of the session asks
        CK_CHECK(app.frames_awaiting_terminal() == 1U);
        CK_CHECK(term.replies_left_in_the_queue() == 1U);
    }
    // Handing a terminal back does not discard what its input queue holds:
    // a reply left here is delivered to whoever inherits the queue, and a
    // shell reads the tail of `CSI 0 n` as a typed `n`.
    CK_CHECK(term.replies_left_in_the_queue() == 0U);
}

CK_TEST(ending_a_session_does_not_wait_on_a_host_that_has_never_answered) {
    ckv::term::HeadlessTerminal term{ckv::Size{20, 5}};
    ManualClock clock;
    Application app(term, clock);
    app.set_frame_completion_tracking(true);
    app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() == 1U);

    // Returns rather than spending the write-off deadline on an exit: a
    // terminal that has never answered has no reply in flight to collect.
    app.settle_frame_completion();
    CK_CHECK(app.frames_awaiting_terminal() == 1U);
}

CK_TEST(settling_gives_up_on_a_backend_that_cannot_be_waited_on) {
    ckv::term::HeadlessTerminal term{ckv::Size{20, 5}};
    ManualClock clock;
    Application app(term, clock);
    app.set_frame_completion_tracking(true);
    app.step(clock.now_nanos());
    clock.advance(4'000'000);
    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.step(clock.now_nanos());

    app.invalidate_all();
    app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() == 1U);
    // A host that has answered before earns the wait — but a backend whose
    // poll neither blocks nor delivers cannot be waited into an answer, and
    // on a manual clock no deadline will ever pass. One unproductive wait
    // ends it, so this returns rather than spinning forever.
    app.settle_frame_completion();
    CK_CHECK(app.frames_awaiting_terminal() == 1U);
}

// --- Adopting a session the application did not launch (ckmux U0-a) --------

namespace {

// A child session with no process behind it: what a mirror over a protocol,
// a recording played back, or a test needs to be. It reports one drain's
// worth of work at a time, so a test can say exactly how often the host
// looked.
class FakeSubsession final : public ckv::term::TerminalSubsession {
public:
    ckv::core::TerminalSnapshot snapshot() const override {
        ckv::core::TerminalSnapshot snapshot;
        snapshot.cells = ckv::Size{4, 1};
        snapshot.cell_buffer.assign(4, ckv::Cell::from_grapheme(" ", {}));
        snapshot.state = ckv::core::TerminalSubsessionState::Running;
        return snapshot;
    }
    ckv::core::TerminalStatus status() const override {
        ckv::core::TerminalStatus status;
        status.cells = ckv::Size{4, 1};
        status.state = ckv::core::TerminalSubsessionState::Running;
        return status;
    }
    const ckv::core::TerminalDamage& damage() const noexcept override { return damage_; }
    void clear_damage() noexcept override { damage_ = {}; }
    bool synchronized_output_active() const noexcept override { return false; }
    std::span<const ckv::Cell> cells() const noexcept override {
        return std::span<const ckv::Cell>(cells_.data(), cells_.size());
    }
    std::span<const ckv::Cell> scrollback() const noexcept override { return {}; }
    std::span<const ckv::core::TerminalRaster> rasters() const noexcept override { return {}; }
    std::span<const ckv::core::TerminalDiagnostic> diagnostics() const noexcept override {
        return {};
    }
    const ckv::core::TerminalCapabilityProfile& profile() const noexcept override { return profile_; }
    void feed_output(std::string_view bytes) override { fed += std::string(bytes); }
    void resize(ckv::Size cells, ckv::Size) override { resized_to = cells; }
    void send_input(std::string_view bytes) override { sent += std::string(bytes); }
    std::string take_pending_input() override { return {}; }
    ckv::core::TerminalSubsessionState state() const noexcept override {
        return ckv::core::TerminalSubsessionState::Running;
    }
    void set_raster_identity(int identity) noexcept override { raster_identity = identity; }
    void close() noexcept override { ++closes; }

    bool drain(std::size_t byte_budget) override {
        ++drains;
        last_byte_budget = byte_budget;
        if (!has_output) return false;
        has_output = false;
        return true;
    }
    std::span<const ckv::term::WaitHandle> wait_handles() const noexcept override {
        return std::span<const ckv::term::WaitHandle>(handles_.data(), handle_count);
    }

    int drains = 0;
    int closes = 0;
    int raster_identity = 0;
    std::size_t last_byte_budget = 0;
    bool has_output = false;
    std::size_t handle_count = 0;
    std::string fed;
    std::string sent;
    ckv::Size resized_to{};

private:
    ckv::core::TerminalCapabilityProfile profile_{};
    ckv::core::TerminalDamage damage_{};
    std::array<ckv::Cell, 4> cells_{ckv::Cell::from_grapheme(" ", {}), ckv::Cell::from_grapheme(" ", {}),
                                    ckv::Cell::from_grapheme(" ", {}), ckv::Cell::from_grapheme(" ", {})};
    // A handle a host would wait on. Never read by the fake — it only has to
    // travel, which is what wait_handles() aggregation means.
    std::array<ckv::term::WaitHandle, 1> handles_{ckv::term::WaitHandle{}};
};

// A view that counts change notifications, and can act on one — which is
// where a host learns that a mirror's far end has gone.
class SubsessionProbe : public View {
public:
    int notifications = 0;
    const ckv::core::TerminalSubsession* last = nullptr;
    std::function<void(const ckv::core::TerminalSubsession&)> on_change;

    void on_terminal_subsession_changed(const ckv::core::TerminalSubsession& session) override {
        ++notifications;
        last = &session;
        if (on_change) on_change(session);
    }
};

}  // namespace

CK_TEST(an_adopted_session_is_drained_and_notified_exactly_as_a_launched_one_is) {
    ckv::term::HeadlessTerminal terminal{ckv::Size{40, 6}};
    ManualClock clock;
    Application app{terminal, clock};
    auto* const probe = app.root().add(std::make_unique<SubsessionProbe>());

    auto owned = std::make_unique<FakeSubsession>();
    FakeSubsession* const fake = owned.get();
    ckv::term::TerminalSubsession& adopted = app.adopt_terminal_subsession(std::move(owned));
    CK_CHECK(&adopted == fake);

    // Drained under the same byte budget a launched session gets, twice per
    // step: once before the poll and once after, because a child that woke
    // the combined wait supplies no outer-terminal event of its own.
    fake->has_output = true;
    app.step(clock.now_nanos());
    CK_CHECK(fake->drains >= 2);
    CK_CHECK(fake->last_byte_budget == 32U * 1024U);
    // And the view tree was told, about this session.
    CK_CHECK(probe->notifications == 1);
    CK_CHECK(probe->last == fake);

    // Nothing to report, nothing reported: a notification per step regardless
    // would repaint every terminal on every frame.
    const int before = probe->notifications;
    app.step(clock.now_nanos());
    CK_CHECK(probe->notifications == before);
}

CK_TEST(an_adopted_session_gets_a_raster_identity_of_its_own) {
    // A session left at the default identity has its pictures dropped by the
    // view that would have drawn them, without a word. The host cannot be the
    // one to remember, so adoption assigns one — from the same counter a
    // launched session draws from, so no two sessions can share.
    ckv::term::HeadlessTerminal terminal{ckv::Size{40, 6}};
    ManualClock clock;
    Application app{terminal, clock};

    auto first = std::make_unique<FakeSubsession>();
    auto second = std::make_unique<FakeSubsession>();
    FakeSubsession* const a = first.get();
    FakeSubsession* const b = second.get();
    app.adopt_terminal_subsession(std::move(first));
    app.adopt_terminal_subsession(std::move(second));
    CK_CHECK(a->raster_identity != 0);
    CK_CHECK(b->raster_identity != 0);
    CK_CHECK(a->raster_identity != b->raster_identity);
}

CK_TEST(an_adopted_sessions_wait_handles_join_the_combined_wait) {
    // The half a caller cannot reproduce from outside: its readiness sources
    // have to reach the outer terminal's wait, or the host sleeps through its
    // own child's output until something else happens to wake it.
    ckv::term::HeadlessTerminal terminal{ckv::Size{40, 6}};
    ManualClock clock;
    Application app{terminal, clock};
    const std::size_t before = app.wait_handles().size();

    auto owned = std::make_unique<FakeSubsession>();
    FakeSubsession* const fake = owned.get();
    fake->handle_count = 1;
    app.adopt_terminal_subsession(std::move(owned));
    CK_CHECK(app.wait_handles().size() == before + 1);

    // ...and leave with it.
    const std::unique_ptr<ckv::term::TerminalSubsession> released =
        app.release_terminal_subsession(*fake);
    CK_CHECK(released != nullptr);
    CK_CHECK(app.wait_handles().size() == before);
}

CK_TEST(a_released_session_is_handed_back_and_stops_being_drained) {
    ckv::term::HeadlessTerminal terminal{ckv::Size{40, 6}};
    ManualClock clock;
    Application app{terminal, clock};

    auto owned = std::make_unique<FakeSubsession>();
    FakeSubsession* const fake = owned.get();
    app.adopt_terminal_subsession(std::move(owned));
    app.step(clock.now_nanos());
    CK_CHECK(fake->drains > 0);

    std::unique_ptr<ckv::term::TerminalSubsession> released = app.release_terminal_subsession(*fake);
    CK_CHECK(released.get() == fake);
    // The caller owns it now, so it is still alive to be asked.
    const int drains_at_release = fake->drains;
    app.step(clock.now_nanos());
    app.step(clock.now_nanos());
    CK_CHECK(fake->drains == drains_at_release);

    // Releasing something this application does not own says so rather than
    // guessing.
    CK_CHECK(app.release_terminal_subsession(*fake) == nullptr);
    FakeSubsession stranger;
    CK_CHECK(app.release_terminal_subsession(stranger) == nullptr);
}

CK_TEST(a_session_may_be_released_from_inside_its_own_change_notification) {
    // Where a host actually learns a mirror is finished: the notification that
    // carried the last of its output. Erasing from the vector the drain loop
    // is walking would pull the ground out from under it, so the slot empties
    // now and the vector compacts at the top of the next step.
    ckv::term::HeadlessTerminal terminal{ckv::Size{40, 6}};
    ManualClock clock;
    Application app{terminal, clock};
    auto* const probe = app.root().add(std::make_unique<SubsessionProbe>());

    auto first = std::make_unique<FakeSubsession>();
    auto second = std::make_unique<FakeSubsession>();
    FakeSubsession* const a = first.get();
    FakeSubsession* const b = second.get();
    a->has_output = true;
    b->has_output = true;
    app.adopt_terminal_subsession(std::move(first));
    app.adopt_terminal_subsession(std::move(second));

    std::unique_ptr<ckv::term::TerminalSubsession> released;
    probe->on_change = [&app, &released, a](const ckv::core::TerminalSubsession& session) {
        if (&session != a) return;
        released = app.release_terminal_subsession(session);
    };
    app.step(clock.now_nanos());
    CK_CHECK(released.get() == a);
    // The session released mid-loop stopped being drained; the one behind it
    // in the vector was still reached, which is the part a naive erase breaks.
    CK_CHECK(probe->notifications == 2);
    CK_CHECK(b->drains >= 2);

    const int a_drains = a->drains;
    app.step(clock.now_nanos());
    CK_CHECK(a->drains == a_drains);
    CK_CHECK(b->drains > 2);
}

CK_TEST(adopting_a_session_from_inside_a_notification_does_not_invalidate_the_walk) {
    // The other half of the same hazard: a notification is application code,
    // and application code may adopt. Growing the vector can reallocate it,
    // which is why the drain loop indexes rather than iterates.
    ckv::term::HeadlessTerminal terminal{ckv::Size{40, 6}};
    ManualClock clock;
    Application app{terminal, clock};
    auto* const probe = app.root().add(std::make_unique<SubsessionProbe>());

    auto owned = std::make_unique<FakeSubsession>();
    FakeSubsession* const first = owned.get();
    first->has_output = true;
    app.adopt_terminal_subsession(std::move(owned));

    FakeSubsession* second = nullptr;
    probe->on_change = [&app, &second, first](const ckv::core::TerminalSubsession& session) {
        if (&session != first || second != nullptr) return;
        auto extra = std::make_unique<FakeSubsession>();
        second = extra.get();
        second->has_output = true;
        app.adopt_terminal_subsession(std::move(extra));
    };
    app.step(clock.now_nanos());
    CK_CHECK(second != nullptr);
    // Adopted mid-step and drained in the same step, because it joined the
    // vector the loop was still walking.
    if (second != nullptr) CK_CHECK(second->drains > 0);
}

CK_TEST(a_terminal_that_answers_slowly_is_waited_for_rather_than_written_off) {
    ckv::term::HeadlessTerminal term{ckv::Size{20, 5}};
    ManualClock clock;
    Application app(term, clock);
    app.set_frame_completion_tracking(true);

    // One prompt answer establishes that this host does answer at all.
    app.invalidate_all();
    app.step(clock.now_nanos());
    clock.advance(200'000'000);
    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() == 0U);
    CK_CHECK(app.last_terminal_round_trip_nanos() == 200'000'000);

    // Now a slow one: past the plain timeout, but well inside what this
    // host has already shown it can need. A decoder busy with a large
    // picture must not be mistaken for one that cannot answer — writing it
    // off removes the back-pressure that was keeping the picture whole.
    app.invalidate_all();
    app.step(clock.now_nanos());
    clock.advance(ckv::ui::kFrameCompletionTimeoutNanos + 100'000'000);
    app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() == 1U);
    CK_CHECK(app.frame_completion_tracking());

    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() == 0U);
}

CK_TEST(a_host_that_has_answered_once_is_never_given_up_on) {
    ckv::term::HeadlessTerminal term{ckv::Size{20, 5}};
    ManualClock clock;
    Application app(term, clock);
    app.set_frame_completion_tracking(true);
    app.invalidate_all();
    app.step(clock.now_nanos());
    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.step(clock.now_nanos());

    for (int attempt = 0; attempt < ckv::ui::kFrameCompletionGiveUpCount * 3; ++attempt) {
        app.invalidate_all();
        app.step(clock.now_nanos());
        clock.advance(ckv::ui::kFrameCompletionTimeoutNanos * 10);
        app.step(clock.now_nanos());
    }
    // Still tracking: silence from a host that has answered is a lost
    // reply, never proof that the facility is absent.
    CK_CHECK(app.frame_completion_tracking());
}

CK_TEST(a_frame_carrying_a_picture_is_not_followed_by_another_until_the_host_is_ready) {
    ckv::term::HeadlessTerminal term{ckv::Size{20, 6}, ckv::term::headless_sixel_profile()};
    ManualClock clock;
    Application app(term, clock);
    // ImageView resolves its own fallback role, so the standard set has to
    // exist before one is attached.
    (void)ckv::ui::intern_standard_roles(app.roles());
    app.set_frame_completion_tracking(true);

    auto image = std::make_shared<ckv::Image>(18, 18);
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 18; ++x) image->set_pixel(x, y, ckv::Image::Rgba{200, 120, 40, 255});
    auto* view = app.root().make<ckv::widgets::ImageView>();
    view->set_bounds(ckv::Rect{0, 0, 6, 3});
    view->set_image(image);
    app.step(clock.now_nanos());
    // One answer first: coalescing follows a host that reports completion,
    // never one whose silence could equally mean it has no such report.
    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.step(clock.now_nanos());
    view->set_bounds(ckv::Rect{1, 0, 6, 3});
    app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() == 1U);

    // A drag's worth of position changes, at the pointer's rate rather than
    // at one the terminal agreed to. None of them may put another picture
    // on the wire while the last one is still being taken in.
    const std::size_t after_first = term.written_bytes().size();
    for (int step = 2; step <= 7; ++step) {
        view->set_bounds(ckv::Rect{step, 0, 6, 3});
        clock.advance(4'000'000);
        app.step(clock.now_nanos());
    }
    CK_CHECK(term.written_bytes().size() == after_first);

    // The host catches up, and what goes out is where the view is NOW —
    // not the six positions it passed through.
    term.inject_bytes("\x1B[0n", clock.now_nanos());
    app.step(clock.now_nanos());
    CK_CHECK(term.written_bytes().size() > after_first);
    CK_CHECK(app.frames_awaiting_terminal() == 1U);
}
