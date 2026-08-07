// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/desktop.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <set>

#include "cktest.hpp"
#include "cvision/scene/compositor.hpp"
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

// --- Popups ----------------------------------------------------------

CK_TEST(a_popup_is_added_above_every_window_regardless_of_add_order) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    ckv::ui::View* popup = desktop.add_popup(std::make_unique<ckv::ui::View>());
    CK_CHECK(desktop.children().back().get() == popup);
}

CK_TEST(activating_a_window_while_a_popup_is_open_does_not_bury_the_popup) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    ckv::ui::View* popup = desktop.add_popup(std::make_unique<ckv::ui::View>());
    CK_CHECK(desktop.children().back().get() == popup);

    desktop.activate(a);  // a moves to front, ahead of b
    CK_CHECK(desktop.children().back().get() == popup);  // popup must still be topmost
    (void)b;
}

CK_TEST(remove_popup_for_a_view_not_currently_a_popup_returns_null) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    ckv::ui::View stray;
    CK_CHECK(desktop.remove_popup(&stray) == nullptr);
}

CK_TEST(removing_a_popup_returns_ownership_and_it_no_longer_appears_in_popups) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    ckv::ui::View* popup = desktop.add_popup(std::make_unique<ckv::ui::View>());
    auto owned = desktop.remove_popup(popup);
    CK_CHECK(owned.get() == popup);
    CK_CHECK(desktop.popups().empty());
}

CK_TEST(removing_the_active_window_while_a_popup_is_open_still_activates_the_next_window_not_the_popup) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));  // b is active
    desktop.add_popup(std::make_unique<ckv::ui::View>());

    desktop.remove_window(b);
    CK_CHECK(desktop.active_window() == a);  // not a crash from mis-casting the popup as a Window
}

CK_TEST(multiple_popups_stack_in_the_order_they_were_added) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    desktop.add_window(make_window(f));
    ckv::ui::View* first = desktop.add_popup(std::make_unique<ckv::ui::View>());
    ckv::ui::View* second = desktop.add_popup(std::make_unique<ckv::ui::View>());
    CK_CHECK(desktop.children().back().get() == second);
    (void)first;
}

// --- Window shadow compositing ------------------------------------------

CK_TEST(a_shadow_casting_windows_footprint_is_dimmed_on_the_desktop) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 5, 10, 4});
    CK_CHECK(w->casts_shadow());

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);

    const auto footprints = ckv::scene::shadow_footprint(w->bounds(), ckv::scene::ShadowSpec{});
    CK_CHECK(!footprints.empty());
    const ckv::Style background_style = f.theme.resolve(f.roles.dialog_background);
    for (const Rect& fp : footprints) {
        const ckv::Point sample{fp.x, fp.y};
        CK_CHECK(!(surface.at(sample).style() == background_style));
        CK_CHECK(surface.at(sample).style() == ckv::scene::default_dim(background_style));
    }
}

CK_TEST(cells_outside_any_shadow_footprint_are_not_dimmed) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 5, 10, 4});

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);

    const ckv::Style background_style = f.theme.resolve(f.roles.dialog_background);
    // Far corner of the desktop, well outside the window's shadow reach.
    CK_CHECK(surface.at(ckv::Point{0, 0}).style() == background_style);
}

CK_TEST(a_window_that_does_not_cast_a_shadow_leaves_its_footprint_undimmed) {
    // Every baseline Window currently declares casts_shadow() == true
    // unconditionally — this test documents the OTHER branch (the
    // dynamic_cast-fails / non-Window-child path in
    // Desktop::paint_children) by using a plain View standing in where
    // a window would be, proving non-Window children never crash or
    // get spuriously shadowed.
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    auto plain = std::make_unique<ckv::ui::View>(Rect{5, 5, 10, 4});
    desktop.add_child(std::move(plain));

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);  // must not crash on a non-Window child

    const ckv::Style background_style = f.theme.resolve(f.roles.dialog_background);
    CK_CHECK(surface.at(ckv::Point{17, 6}).style() == background_style);  // where a shadow WOULD be
}

CK_TEST(a_higher_window_painted_afterward_is_not_dimmed_by_a_lower_windows_shadow) {
    // The whole point of interleaving shadow compositing with the
    // z-order walk rather than a separate post-pass: window B, drawn
    // AFTER window A's shadow is applied, must show ITS OWN content —
    // undimmed — even where A's shadow footprint overlaps B.
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f));
    a->set_bounds(Rect{2, 2, 10, 4});  // shadow footprint includes column 12-13, rows 3-6

    Window* b = desktop.add_window(make_window(f));  // added after a: on top in z-order
    b->set_bounds(Rect{12, 3, 10, 4});               // sits exactly over a's shadow region

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);

    // Inside b's frame (its own border style), not the dimmed
    // dialog_background shadow style.
    const ckv::Style background_style = f.theme.resolve(f.roles.dialog_background);
    CK_CHECK(!(surface.at(ckv::Point{12, 3}).style() == ckv::scene::default_dim(background_style)));
}

// --- Docked chrome (dock_top/dock_bottom) and resize reflow ---------------

CK_TEST(dock_top_positions_the_view_at_row_zero_full_width) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24}, f.theme, f.roles.dialog_background);
    auto* bar = desktop.dock_top(std::make_unique<ckv::ui::View>());
    CK_CHECK(bar->bounds() == (Rect{0, 0, 60, 1}));
    CK_CHECK(desktop.top_dock() == bar);
}

CK_TEST(dock_bottom_positions_the_view_at_the_last_row_full_width) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24}, f.theme, f.roles.dialog_background);
    auto* status = desktop.dock_bottom(std::make_unique<ckv::ui::View>());
    CK_CHECK(status->bounds() == (Rect{0, 23, 60, 1}));
    CK_CHECK(desktop.bottom_dock() == status);
}

CK_TEST(content_area_excludes_both_docks) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24}, f.theme, f.roles.dialog_background);
    desktop.dock_top(std::make_unique<ckv::ui::View>());
    desktop.dock_bottom(std::make_unique<ckv::ui::View>());
    CK_CHECK(desktop.content_area() == (Rect{0, 1, 60, 22}));
}

CK_TEST(content_area_is_the_full_bounds_when_nothing_is_docked) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24}, f.theme, f.roles.dialog_background);
    CK_CHECK(desktop.content_area() == (Rect{0, 0, 60, 24}));
}

CK_TEST(resizing_the_desktop_repositions_both_docks_to_the_new_edges) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24}, f.theme, f.roles.dialog_background);
    auto* bar = desktop.dock_top(std::make_unique<ckv::ui::View>());
    auto* status = desktop.dock_bottom(std::make_unique<ckv::ui::View>());
    desktop.set_bounds(Rect{0, 0, 100, 40});
    CK_CHECK(bar->bounds() == (Rect{0, 0, 100, 1}));
    CK_CHECK(status->bounds() == (Rect{0, 39, 100, 1}));
}

CK_TEST(shrinking_the_desktop_pulls_a_window_back_into_the_new_content_area) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{60, 15, 15, 6});  // reachable in the original 80x24 desktop
    desktop.set_bounds(Rect{0, 0, 40, 24});
    CK_CHECK(w->bounds().x + w->bounds().width <= 40);
}

CK_TEST(docking_a_status_line_shrinks_the_content_area_and_clamps_windows_out_of_it) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 20, 15, 4});  // bottom edge at row 24, the very last row
    desktop.dock_bottom(std::make_unique<ckv::ui::View>());  // now occupies row 23
    CK_CHECK(w->bounds().y + w->bounds().height <= 23);
}

CK_TEST(tile_places_windows_within_the_content_area_not_under_the_docked_chrome) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24}, f.theme, f.roles.dialog_background);
    desktop.dock_top(std::make_unique<ckv::ui::View>());
    desktop.dock_bottom(std::make_unique<ckv::ui::View>());
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    desktop.tile();
    CK_CHECK(a->bounds().y == 1);
    CK_CHECK(b->bounds().y == 1);
    CK_CHECK(a->bounds().height == 22);
}

CK_TEST(cascade_places_windows_within_the_content_area_not_under_the_docked_chrome) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24}, f.theme, f.roles.dialog_background);
    desktop.dock_top(std::make_unique<ckv::ui::View>());
    Window* w = desktop.add_window(make_window(f));
    desktop.cascade();
    CK_CHECK(w->bounds().y >= 1);
}

CK_TEST(a_window_maximized_to_the_content_area_does_not_shadow_the_docked_status_line) {
    // Regression: shadow_footprint's bottom strip falls exactly on the
    // row just below a window's bottom edge — a window maximized to
    // content_area() (Window::toggle_zoom is handed that exact rect by
    // every real caller) has its bottom edge flush against the docked
    // status line, so that strip used to land ON the status line and
    // dim it. Shadows must clip to content_area(), not the full desktop.
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    ckv::ui::View* status = desktop.dock_bottom(std::make_unique<ckv::ui::View>());
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(desktop.content_area());  // "maximized"
    CK_CHECK(w->bounds().y + w->bounds().height == status->bounds().y);  // flush against the footer

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);

    const ckv::Style background_style = f.theme.resolve(f.roles.dialog_background);
    for (int x = 0; x < 40; ++x)
        CK_CHECK(surface.at(ckv::Point{x, status->bounds().y}).style() == background_style);
}

CK_TEST(a_window_maximized_to_the_content_area_still_shadows_normally_above_the_footer) {
    // The fix must not over-clip: shadow rows that fall WITHIN
    // content_area() (not on the footer itself) still dim normally.
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    desktop.dock_bottom(std::make_unique<ckv::ui::View>());
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 5, 10, 4});  // ordinary, unmaximized — well clear of the footer

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);

    const auto footprints = ckv::scene::shadow_footprint(w->bounds(), ckv::scene::ShadowSpec{});
    CK_CHECK(!footprints.empty());
    const ckv::Style background_style = f.theme.resolve(f.roles.dialog_background);
    for (const Rect& fp : footprints)
        CK_CHECK(surface.at(ckv::Point{fp.x, fp.y}).style() == ckv::scene::default_dim(background_style));
}
