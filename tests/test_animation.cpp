// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// U4-k: what "animation" means in this toolkit, and the minimize flight as
// its first consumer.
//
// Every case here exists to pin one of the constraints the package is
// defined by — a text user interface is not a game loop, so an effect must
// never block the loop, must be interruptible, must be skippable, and must
// degrade honestly on a host that cannot deliver frames. The last one is why
// so many of these tests advance a ManualClock in uneven jumps rather than
// evenly: a run that only behaves when every frame arrives on time is a run
// that is wrong on the machine it matters on.
//
// The single load-bearing invariant, which most of these check from one
// angle or another: **the end state does not come from the frames.** It is
// applied before the animation is asked for, so an effect that is disabled,
// cut short, or never ticked cannot leave the application anywhere a reader
// did not ask to be.
#include "cvision/ui/animation.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

using ckv::ManualClock;
using ckv::Rect;
using ckv::ui::Animation;
using ckv::ui::Application;
using ckv::ui::Context;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Desktop;
using ckv::widgets::Window;
namespace ui = ckv::ui;

namespace {

constexpr std::int64_t kFrame = Animation::kDefaultFrameIntervalNanos;

struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    Desktop desktop{Rect{0, 0, 80, 24}};

    Fixture() {
        desktop.set_context(Context{&theme, &registry, &app});
        // These tests are about the FLIGHT, and every one of them installs a
        // host's own minimize target — which is what a host that lists its
        // windows itself does (D-064). Left on the default the desktop would
        // also park a stub for each window put away, and `decoration` below
        // reads the popup list to answer "is an effect on screen".
        desktop.set_minimized_window_placement(
            ckv::widgets::MinimizedWindowPlacement::HostListed);
    }

    // One turn of the loop at the current clock. Timers due now run.
    void step() { app.step(0); }

    // Advance and run, the way a host that is keeping up would.
    void advance(std::int64_t nanos) {
        clock.advance(nanos);
        app.step(0);
    }

    Window* open(std::string title, Rect where) {
        Window* const window = desktop.add_window(std::make_unique<Window>(std::move(title)));
        window->set_bounds(where);
        return window;
    }
};

// The flight decoration, if one is up: the desktop's only popup while an
// effect is running. Read as "is a decoration on screen", never as a handle
// to drive — it is deliberately not a type any host can name.
ui::View* decoration(const Desktop& desktop) {
    return desktop.popups().empty() ? nullptr : desktop.popups().front();
}

}  // namespace

// --- The mechanism --------------------------------------------------------

CK_TEST(an_animation_reports_progress_from_the_clock_rather_than_counting_frames) {
    // The constraint that makes a slow host honest. A run advanced in one
    // late jump is as far along as one advanced in three punctual ones —
    // otherwise a terminal that drops frames turns every effect into
    // slow motion, and the effect outlives the thing it was describing.
    Fixture punctual;
    std::vector<double> even;
    Animation a;
    a.start(punctual.app, 300'000'000, [&](double p) { even.push_back(p); }, [] {}, kFrame);
    for (int i = 0; i < 3; ++i) punctual.advance(100'000'000);

    Fixture late;
    std::vector<double> jumped;
    Animation b;
    b.start(late.app, 300'000'000, [&](double p) { jumped.push_back(p); }, [] {}, kFrame);
    late.advance(200'000'000);

    CK_CHECK(!even.empty());
    CK_CHECK(!jumped.empty());
    // Same wall time, different frame counts, same place in the run.
    CK_CHECK(even.size() > jumped.size());
    CK_CHECK(jumped.back() > 0.6);
    CK_CHECK(jumped.back() < 0.7);
}

CK_TEST(a_run_that_completes_finishes_exactly_once) {
    Fixture f;
    int frames = 0;
    int finished = 0;
    Animation animation;
    animation.start(f.app, 100'000'000, [&](double) { ++frames; }, [&] { ++finished; }, kFrame);
    CK_CHECK(animation.running());

    for (int i = 0; i < 12; ++i) f.advance(kFrame);
    CK_CHECK(frames > 0);
    CK_CHECK(finished == 1);
    CK_CHECK(!animation.running());
    // And it stays once: the timer is gone, not merely inert.
    for (int i = 0; i < 12; ++i) f.advance(kFrame);
    CK_CHECK(finished == 1);
}

CK_TEST(a_run_cut_short_finishes_exactly_once_and_draws_no_more_frames) {
    Fixture f;
    int frames = 0;
    int finished = 0;
    Animation animation;
    animation.start(f.app, 1'000'000'000, [&](double) { ++frames; }, [&] { ++finished; }, kFrame);
    f.advance(kFrame * 2);
    const int drawn = frames;
    CK_CHECK(drawn > 0);

    animation.finish();
    CK_CHECK(finished == 1);
    CK_CHECK(!animation.running());
    f.advance(kFrame * 10);
    CK_CHECK(frames == drawn);
    CK_CHECK(finished == 1);
}

CK_TEST(a_run_with_no_duration_is_over_before_it_starts) {
    // What "animations off" costs a call site: nothing. A zero duration is
    // answered here rather than by a branch every effect has to remember, and
    // the terminal callback still runs — so a decoration is still torn down
    // by the same line that tears it down after a full flight.
    Fixture f;
    int frames = 0;
    int finished = 0;
    Animation animation;
    animation.start(f.app, 0, [&](double) { ++frames; }, [&] { ++finished; }, kFrame);
    CK_CHECK(finished == 1);
    CK_CHECK(frames == 0);
    CK_CHECK(!animation.running());
    f.advance(kFrame * 10);
    CK_CHECK(frames == 0);
    CK_CHECK(finished == 1);
}

CK_TEST(starting_a_second_run_finishes_the_first_so_its_decoration_still_goes) {
    Fixture f;
    int first_finished = 0;
    int second_finished = 0;
    Animation animation;
    animation.start(f.app, 1'000'000'000, [](double) {}, [&] { ++first_finished; }, kFrame);
    f.advance(kFrame);
    CK_CHECK(first_finished == 0);

    animation.start(f.app, 100'000'000, [](double) {}, [&] { ++second_finished; }, kFrame);
    // The first run's terminal callback ran when the second displaced it: two
    // effects overlapping is the caller's business, two decorations left on
    // screen is nobody's.
    CK_CHECK(first_finished == 1);
    CK_CHECK(second_finished == 0);

    for (int i = 0; i < 8; ++i) f.advance(kFrame);
    CK_CHECK(first_finished == 1);
    CK_CHECK(second_finished == 1);
}

CK_TEST(a_run_finished_from_inside_its_own_frame_is_safe) {
    // An effect that decides mid-flight that it is done. The timer may still
    // be holding the callback when that happens, which is what the liveness
    // guard in the callback is for; under ASan this is the case that says so.
    Fixture f;
    int finished = 0;
    Animation animation;
    animation.start(
        f.app, 1'000'000'000, [&](double) { animation.finish(); }, [&] { ++finished; }, kFrame);
    f.advance(kFrame);
    CK_CHECK(finished == 1);
    CK_CHECK(!animation.running());
    f.advance(kFrame * 5);
    CK_CHECK(finished == 1);
}

CK_TEST(a_destroyed_animation_is_silent_and_leaves_nothing_to_fire_into) {
    // Destruction does NOT run the terminal callback: an owner being
    // destroyed is already tearing down. What it must do is leave nothing for
    // the Application — which outlives it — to call. ASan is the judge here;
    // unsanitized this passes either way.
    Fixture f;
    int finished = 0;
    {
        Animation animation;
        animation.start(f.app, 1'000'000'000, [](double) {}, [&] { ++finished; }, kFrame);
        f.advance(kFrame);
    }
    f.advance(kFrame * 20);
    CK_CHECK(finished == 0);
}

// --- The minimize flight --------------------------------------------------

CK_TEST(a_desktop_with_no_target_provider_animates_nothing) {
    // The quiet default. An application that has not said where its hidden
    // windows go gets no effect rather than a guess — and, more to the point,
    // every ckVision application that existed before this package is
    // unchanged by it.
    Fixture f;
    Window* const window = f.open("W", Rect{4, 2, 30, 10});
    window->set_minimized(true);
    CK_CHECK(window->minimized());
    CK_CHECK(decoration(f.desktop) == nullptr);
    f.advance(kFrame * 10);
    CK_CHECK(decoration(f.desktop) == nullptr);
}

CK_TEST(a_minimized_window_flies_to_the_row_the_host_named_and_the_flight_then_goes) {
    Fixture f;
    Rect target{2, 23, 12, 1};
    f.desktop.set_minimize_target_provider([&](Window&) { return std::optional<Rect>(target); });
    Window* const window = f.open("W", Rect{20, 4, 40, 12});

    window->set_minimized(true);
    // The end state is already reached, before a single frame has been drawn.
    CK_CHECK(window->minimized());
    CK_CHECK(!window->visible());

    ui::View* const flight = decoration(f.desktop);
    CK_CHECK(flight != nullptr);
    if (flight == nullptr) return;
    // Parenthesised: a braced initialiser inside a macro argument is read as
    // several arguments.
    const Rect source{20, 4, 40, 12};
    CK_CHECK(flight->bounds() == source);

    // Part way: between the window and the row, and heading the right way.
    f.advance(kFrame * 3);
    const Rect midway = flight->bounds();
    CK_CHECK(midway.y > 4);
    CK_CHECK(midway.y < 23);
    CK_CHECK(midway.width < 40);
    CK_CHECK(midway.width > 12);

    // And it lands and clears itself up.
    f.advance(Desktop::kDefaultMinimizeAnimationNanos);
    CK_CHECK(decoration(f.desktop) == nullptr);
    CK_CHECK(window->minimized());
}

CK_TEST(restoring_flies_the_other_way) {
    Fixture f;
    const Rect target{2, 23, 12, 1};
    f.desktop.set_minimize_target_provider([&](Window&) { return std::optional<Rect>(target); });
    Window* const window = f.open("W", Rect{20, 4, 40, 12});
    window->set_minimized(true);
    f.advance(Desktop::kDefaultMinimizeAnimationNanos * 2);
    CK_CHECK(decoration(f.desktop) == nullptr);

    window->set_minimized(false);
    ui::View* const flight = decoration(f.desktop);
    CK_CHECK(flight != nullptr);
    if (flight == nullptr) return;
    // Out of the row this time, not into it.
    CK_CHECK(flight->bounds() == target);
    f.advance(kFrame * 3);
    CK_CHECK(flight->bounds().height > 1);
    f.advance(Desktop::kDefaultMinimizeAnimationNanos);
    CK_CHECK(decoration(f.desktop) == nullptr);
    CK_CHECK(!window->minimized());
    CK_CHECK(window->visible());
}

CK_TEST(the_end_state_is_identical_with_the_animation_on_and_off) {
    // The package's first acceptance item, asked as a comparison rather than
    // as an assertion about one run: whatever the animated desktop ends up
    // as, the unanimated one ends up as too.
    const auto settle = [](bool animate) {
        auto f = std::make_unique<Fixture>();
        const Rect target{2, 23, 12, 1};
        f->desktop.set_minimize_target_provider(
            [target](Window&) { return std::optional<Rect>(target); });
        if (!animate) f->desktop.set_minimize_animation_duration(0);
        Window* const first = f->open("A", Rect{2, 2, 30, 8});
        Window* const second = f->open("B", Rect{34, 2, 30, 8});
        first->set_minimized(true);
        f->advance(Desktop::kDefaultMinimizeAnimationNanos * 2);
        second->set_minimized(true);
        f->advance(Desktop::kDefaultMinimizeAnimationNanos * 2);
        first->set_minimized(false);
        f->advance(Desktop::kDefaultMinimizeAnimationNanos * 2);
        return f;
    };

    const auto animated = settle(true);
    const auto plain = settle(false);
    CK_CHECK(animated->desktop.windows().size() == plain->desktop.windows().size());
    for (std::size_t i = 0; i < animated->desktop.windows().size(); ++i) {
        const Window* const a = animated->desktop.windows()[i];
        const Window* const b = plain->desktop.windows()[i];
        CK_CHECK(a->minimized() == b->minimized());
        CK_CHECK(a->visible() == b->visible());
        CK_CHECK(a->bounds() == b->bounds());
        CK_CHECK(a->title() == b->title());
    }
    CK_CHECK((animated->desktop.active_window() == nullptr) ==
             (plain->desktop.active_window() == nullptr));
    // Neither is left with a decoration on it.
    CK_CHECK(decoration(animated->desktop) == nullptr);
    CK_CHECK(decoration(plain->desktop) == nullptr);
}

CK_TEST(a_host_that_never_gets_a_timer_tick_still_ends_with_the_window_hidden) {
    // The third acceptance item, and the reason the end state is applied
    // before the effect is asked for rather than by its last frame. Not one
    // step is taken here: no timer ever runs, so no frame is ever drawn.
    Fixture f;
    f.desktop.set_minimize_target_provider(
        [](Window&) { return std::optional<Rect>(Rect{2, 23, 12, 1}); });
    Window* const window = f.open("W", Rect{20, 4, 40, 12});

    window->set_minimized(true);
    CK_CHECK(window->minimized());
    CK_CHECK(!window->visible());
    CK_CHECK(f.desktop.active_window() != window);
    // A decoration is up, because a flight started; nothing downstream cares.
    window->set_minimized(false);
    CK_CHECK(!window->minimized());
    CK_CHECK(window->visible());
}

CK_TEST(interrupting_a_flight_leaves_a_coherent_desktop) {
    // The second acceptance item. "Resolves immediately to the end state" is
    // trivially true here — the end state arrived before the first frame —
    // so what interruption actually has to guarantee is that the decoration
    // goes and nothing of it is left behind.
    Fixture f;
    f.desktop.set_minimize_target_provider(
        [](Window&) { return std::optional<Rect>(Rect{2, 23, 12, 1}); });
    Window* const window = f.open("W", Rect{20, 4, 40, 12});
    window->set_minimized(true);
    f.advance(kFrame);
    CK_CHECK(decoration(f.desktop) != nullptr);

    f.desktop.finish_minimize_animation();
    CK_CHECK(decoration(f.desktop) == nullptr);
    CK_CHECK(window->minimized());
    CK_CHECK(!window->visible());
    // And the loop is quiet afterwards: no timer still running, nothing to
    // draw, no second teardown.
    f.advance(kFrame * 20);
    CK_CHECK(decoration(f.desktop) == nullptr);
    CK_CHECK(window->minimized());
}

CK_TEST(a_second_minimize_during_a_flight_leaves_exactly_one_decoration) {
    // Two windows put away in quick succession — the ordinary impatient case,
    // and the one that leaks a decoration if a run does not displace its
    // predecessor properly.
    Fixture f;
    f.desktop.set_minimize_target_provider(
        [](Window&) { return std::optional<Rect>(Rect{2, 23, 12, 1}); });
    Window* const first = f.open("A", Rect{2, 2, 30, 8});
    Window* const second = f.open("B", Rect{34, 2, 30, 8});

    first->set_minimized(true);
    f.advance(kFrame);
    second->set_minimized(true);
    CK_CHECK(f.desktop.popups().size() == 1U);

    f.advance(Desktop::kDefaultMinimizeAnimationNanos * 2);
    CK_CHECK(f.desktop.popups().empty());
    CK_CHECK(first->minimized());
    CK_CHECK(second->minimized());
}

CK_TEST(a_flight_never_takes_a_click_meant_for_the_desktop_underneath_it) {
    // The decoration is drawn over a desktop that is ALREADY in its end
    // state, so a click it swallowed would be one stolen from a window the
    // reader can see, on the way to a state that had already arrived.
    Fixture f;
    f.desktop.set_minimize_target_provider(
        [](Window&) { return std::optional<Rect>(Rect{2, 23, 12, 1}); });
    Window* const window = f.open("W", Rect{20, 4, 40, 12});
    window->set_minimized(true);
    f.advance(kFrame);
    ui::View* const flight = decoration(f.desktop);
    CK_CHECK(flight != nullptr);
    if (flight == nullptr) return;

    const Rect at = flight->bounds();
    const ckv::MouseEvent press{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                ckv::Point{at.x + at.width / 2, at.y}, std::nullopt,
                                ckv::Modifier::None};
    CK_CHECK(!flight->on_mouse(press));
    CK_CHECK(!flight->focusable());
    CK_CHECK(!flight->pointer_shape_at(ckv::Point{0, 0}).has_value());
}

CK_TEST(animations_off_does_not_even_ask_the_host_where_the_window_goes) {
    // "Off" has to cost nothing, and nothing is a stronger claim than "no
    // decoration is left behind". A zero-duration run through ui::Animation
    // would finish immediately and tear its own popup down in the same call,
    // so the desktop would LOOK identical while still paying an add_popup and
    // a remove_popup — each of which recomposes the compositor's layers — on
    // every single minimize.
    //
    // So the observable is the provider itself: with the effect off, the host
    // is never even asked. Written this way because the weaker version of
    // this test passed with the disable branch deleted, which made it a test
    // of ui::Animation's zero-duration path rather than of the switch.
    Fixture f;
    int asked = 0;
    f.desktop.set_minimize_target_provider([&](Window&) {
        ++asked;
        return std::optional<Rect>(Rect{2, 23, 12, 1});
    });
    f.desktop.set_minimize_animation_duration(0);
    Window* const window = f.open("W", Rect{20, 4, 40, 12});

    window->set_minimized(true);
    CK_CHECK(asked == 0);
    CK_CHECK(f.desktop.popups().empty());
    CK_CHECK(window->minimized());
    window->set_minimized(false);
    CK_CHECK(asked == 0);
    CK_CHECK(f.desktop.popups().empty());
    CK_CHECK(!window->minimized());

    // The positive partner: the same desktop with the effect on does ask,
    // so this is a test of the switch rather than of a provider nobody calls.
    f.desktop.set_minimize_animation_duration(Desktop::kDefaultMinimizeAnimationNanos);
    window->set_minimized(true);
    CK_CHECK(asked == 1);
    CK_CHECK(f.desktop.popups().size() == 1U);
}

CK_TEST(a_host_that_names_nowhere_to_fly_to_gets_no_flight) {
    // A provider that answers nullopt is the ordinary answer for a window
    // with no row — one the taskbar does not list, or a bar that is not on
    // screen. Not an error, and not a flight to the origin.
    Fixture f;
    f.desktop.set_minimize_target_provider([](Window&) { return std::optional<Rect>(); });
    Window* const window = f.open("W", Rect{20, 4, 40, 12});
    window->set_minimized(true);
    CK_CHECK(window->minimized());
    CK_CHECK(decoration(f.desktop) == nullptr);
}

CK_TEST(replacing_the_target_provider_mid_flight_ends_the_flight) {
    // Whatever is in the air was flying to an answer this desktop no longer
    // gives. Ended rather than redirected — and, either way, not left over a
    // desktop whose host has just changed what its chrome is.
    Fixture f;
    f.desktop.set_minimize_target_provider(
        [](Window&) { return std::optional<Rect>(Rect{2, 23, 12, 1}); });
    Window* const window = f.open("W", Rect{20, 4, 40, 12});
    window->set_minimized(true);
    f.advance(kFrame);
    CK_CHECK(decoration(f.desktop) != nullptr);

    f.desktop.set_minimize_target_provider(nullptr);
    CK_CHECK(decoration(f.desktop) == nullptr);
    CK_CHECK(window->minimized());
}

CK_TEST(an_application_whose_root_owns_a_desktop_with_a_flight_in_the_air_can_be_destroyed) {
    // Regression for a heap-use-after-free in ~Application. Members go in
    // reverse order of declaration, and Application's timer table was
    // declared AFTER root_ — so the view tree was torn down into a timer
    // table that had already been freed: a Desktop owns an Animation whose
    // destructor calls cancel_timer(). This fixture's own Desktop is a
    // separate member and cannot show it; the Desktop here lives UNDER
    // root_, the way every real host's does, so its Animation dies during
    // ~Application. The claim is the sanitizer's, and a LINUX sanitizer's at
    // that: libstdc++'s ~vector leaves its pointers behind, so the dead timer
    // table is walked as freed memory (ASan: heap-use-after-free in
    // cancel_timer; glibc without ASan: "double free detected in tcache" at
    // the end of a ckmux e2e suite), while libc++'s ~vector nulls them and
    // the same walk sees an empty range — macOS cannot witness this defect
    // at all, with or without ASan. Red on the Linux ASan lane before the
    // declaration moved, green after.
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    {
        Application app{term, clock};
        StandardRoles roles = intern_standard_roles(app.roles());
        app.theme() = make_classic_theme(app.roles(), roles);
        Desktop* const desktop = static_cast<Desktop*>(
            app.root().add_child(std::make_unique<Desktop>(app.root().bounds())));
        desktop->set_minimized_window_placement(
            ckv::widgets::MinimizedWindowPlacement::HostListed);
        desktop->set_minimize_target_provider(
            [](Window&) { return std::optional<Rect>(Rect{2, 23, 12, 1}); });
        Window* const window = desktop->add_window(std::make_unique<Window>("W"));
        window->set_bounds(Rect{20, 4, 40, 12});
        window->set_minimized(true);  // arms the flight's timer
        app.step(0);
        CK_CHECK(window->minimized());
        // The precondition, asserted: a flight IS in the air, so the Animation
        // holds a live timer and its destructor will reach cancel_timer().
        // Without this line the test could pass by never arming anything.
        CK_CHECK(decoration(*desktop) != nullptr);
    }  // ~Application: root_, the desktop, and its Animation's cancel_timer()
    CK_CHECK(true);
}
