// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/desktop.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <set>

#include "cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Desktop;
using ckv::widgets::Window;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

std::unique_ptr<Window> make_window(Fixture& f, std::string title = "W") {
    return std::make_unique<Window>(std::move(title), f.theme, f.roles.window_frame_active,
                                     f.roles.window_frame_inactive, f.roles.window_title_active,
                                     f.roles.window_title_inactive);
}
}  // namespace

// --- Adding / removing / activation ------------------------------------

CK_TEST(adding_a_window_activates_it) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    CK_CHECK(desktop.active_window() == w);
    CK_CHECK(w->active());
}

CK_TEST(adding_a_second_window_activates_it_and_deactivates_the_first) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    CK_CHECK(desktop.active_window() == b);
    CK_CHECK(!a->active());
    CK_CHECK(b->active());
}

CK_TEST(a_newly_added_window_becomes_the_topmost_child) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    CK_CHECK(desktop.children().back().get() == b);
    (void)a;
}

CK_TEST(remove_window_for_a_window_not_owned_by_this_desktop_returns_null) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window stray("stray", f.theme, f.roles.window_frame_active, f.roles.window_frame_inactive,
                 f.roles.window_title_active, f.roles.window_title_inactive);
    CK_CHECK(desktop.remove_window(&stray) == nullptr);
}

CK_TEST(removing_the_active_window_activates_the_new_topmost_remaining_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    CK_CHECK(desktop.active_window() == b);
    desktop.remove_window(b);
    CK_CHECK(desktop.active_window() == a);
    CK_CHECK(a->active());
}

CK_TEST(removing_the_last_window_leaves_no_active_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    desktop.remove_window(a);
    CK_CHECK(desktop.active_window() == nullptr);
    CK_CHECK(desktop.windows().empty());
}

CK_TEST(removing_an_inactive_window_does_not_disturb_the_active_one) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));  // b is active
    desktop.remove_window(a);
    CK_CHECK(desktop.active_window() == b);
    CK_CHECK(b->active());
}

CK_TEST(activate_on_an_already_active_window_is_a_no_op) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    int calls = 0;
    a->set_dirty_rect_sink([&](Rect) { ++calls; });
    desktop.activate(a);  // already active
    CK_CHECK(calls == 0);
}

CK_TEST(activating_an_older_window_raises_it_to_the_front_and_reactivates_it) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    desktop.activate(a);
    CK_CHECK(desktop.active_window() == a);
    CK_CHECK(a->active());
    CK_CHECK(!b->active());
    CK_CHECK(desktop.children().back().get() == a);
}

CK_TEST(activate_on_a_window_not_owned_by_this_desktop_aborts) {
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        Fixture f;
        Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
        desktop.add_window(make_window(f));
        Window stray("stray", f.theme, f.roles.window_frame_active, f.roles.window_frame_inactive,
                     f.roles.window_title_active, f.roles.window_title_inactive);
        desktop.activate(&stray);  // must abort: not owned by this desktop
        ::_exit(1);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
}

// --- Cycling ---------------------------------------------------------

CK_TEST(activate_next_and_previous_are_no_ops_with_zero_or_one_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.activate_next();  // zero windows
    CK_CHECK(desktop.active_window() == nullptr);
    Window* a = desktop.add_window(make_window(f));
    desktop.activate_next();  // one window
    CK_CHECK(desktop.active_window() == a);
}

CK_TEST(activate_next_visits_every_window_exactly_once_before_repeating) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    Window* c = desktop.add_window(make_window(f));  // active
    CK_CHECK(desktop.active_window() == c);
    desktop.activate_next();
    CK_CHECK(desktop.active_window() == a);
    desktop.activate_next();
    CK_CHECK(desktop.active_window() == b);
    desktop.activate_next();
    CK_CHECK(desktop.active_window() == c);  // wrapped all the way around
}

CK_TEST(activate_previous_is_the_exact_reverse_of_activate_next) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    Window* c = desktop.add_window(make_window(f));
    desktop.activate(a);           // a is now active, at insertion index 0
    desktop.activate_previous();   // steps to index (0-1+3)%3 = 2 = c
    CK_CHECK(desktop.active_window() == c);
    desktop.activate_next();       // steps back to index 0 = a, undoing the previous step
    CK_CHECK(desktop.active_window() == a);
}

CK_TEST(cycling_still_visits_every_window_after_an_out_of_order_activation) {
    // Regression guard: a z-order-relative "next" definition would get
    // stuck cycling only the top two windows once activation itself
    // perturbs z-order — this exercises exactly that scenario.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    Window* c = desktop.add_window(make_window(f));
    desktop.activate(a);  // out-of-order activation, perturbs z-order
    std::set<Window*> seen;
    for (int i = 0; i < 3; ++i) {
        seen.insert(desktop.active_window());
        desktop.activate_next();
    }
    CK_CHECK(seen.size() == 3);
    CK_CHECK(seen.count(a) == 1 && seen.count(b) == 1 && seen.count(c) == 1);
}

CK_TEST(select_by_number_activates_the_nth_window_in_insertion_order) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    desktop.select_by_number(1);
    CK_CHECK(desktop.active_window() == a);
}

CK_TEST(select_by_number_out_of_range_is_a_harmless_no_op) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    desktop.select_by_number(0);   // below range
    CK_CHECK(desktop.active_window() == a);
    desktop.select_by_number(99);  // above range
    CK_CHECK(desktop.active_window() == a);
}

// --- Tile / cascade ------------------------------------------------------

CK_TEST(tile_divides_the_desktop_width_evenly_among_windows_with_no_overlap_or_gap) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 90, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    Window* c = desktop.add_window(make_window(f));
    desktop.tile();
    CK_CHECK(a->bounds() == (Rect{0, 0, 30, 24}));
    CK_CHECK(b->bounds() == (Rect{30, 0, 30, 24}));
    CK_CHECK(c->bounds() == (Rect{60, 0, 30, 24}));
}

CK_TEST(tile_with_a_width_not_evenly_divisible_gives_the_remainder_to_the_last_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 10, 24}, f.theme, f.roles.dialog_background);  // 10 / 3 = 3 remainder 1
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    Window* c = desktop.add_window(make_window(f));
    desktop.tile();
    CK_CHECK(a->bounds().width == 3);
    CK_CHECK(b->bounds().width == 3);
    CK_CHECK(c->bounds().width == 4);  // 3 + the leftover 1 column
    CK_CHECK(a->bounds().width + b->bounds().width + c->bounds().width == 10);
}

CK_TEST(tile_with_zero_windows_does_not_crash) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.tile();
    CK_CHECK(true);
}

CK_TEST(cascade_makes_the_last_window_topmost) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    desktop.cascade();
    CK_CHECK(desktop.children().back().get() == b);
}

CK_TEST(cascade_keeps_every_window_within_the_desktop_bounds) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    for (int i = 0; i < 5; ++i) desktop.add_window(make_window(f));
    desktop.cascade();
    for (Window* w : desktop.windows()) {
        CK_CHECK(w->bounds().x >= 0 && w->bounds().x + w->bounds().width <= 40);
        CK_CHECK(w->bounds().y >= 0 && w->bounds().y + w->bounds().height <= 20);
    }
}

// --- Rendering -----------------------------------------------------------

CK_TEST(draw_fills_the_background_without_crashing) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 10, 5}, f.theme, f.roles.dialog_background);
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    desktop.draw(painter);
    CK_CHECK(true);
}
