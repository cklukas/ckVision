// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/desktop.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <set>

#include "cktest.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::ManualClock;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::Application;
using ckv::ui::Context;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Desktop;
using ckv::widgets::schedule_self_detach;
using ckv::widgets::Window;
using ckv::widgets::WindowHandle;
namespace ui = ckv::ui;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    Context ctx() { return Context{&theme, &registry, nullptr}; }
};

std::unique_ptr<Window> make_window(Fixture&, std::string title = "W") {
    return std::make_unique<Window>(std::move(title));
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

// --- Click-to-activate/raise (on_descendant_mouse_down, M8 WP-3) -------

CK_TEST(on_descendant_mouse_down_activates_and_raises_the_owning_window_for_a_deep_descendant) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* back = desktop.add_window(make_window(f, "Back"));
    auto content = std::make_unique<ckv::ui::View>();
    ckv::ui::View* deep_leaf = content->add_child(std::make_unique<ckv::ui::View>());
    back->set_content(std::move(content));
    Window* front = desktop.add_window(make_window(f, "Front"));  // now active/topmost

    CK_CHECK(!back->active());
    CK_CHECK(desktop.children().back().get() == static_cast<ckv::ui::View*>(front));

    desktop.on_descendant_mouse_down(*deep_leaf);  // several levels below Window itself

    CK_CHECK(back->active());
    CK_CHECK(!front->active());
    CK_CHECK(desktop.children().back().get() == static_cast<ckv::ui::View*>(back));
}

CK_TEST(on_descendant_mouse_down_activates_the_window_itself_when_target_is_the_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* back = desktop.add_window(make_window(f, "Back"));
    desktop.add_window(make_window(f, "Front"));

    CK_CHECK(!back->active());
    desktop.on_descendant_mouse_down(*back);  // e.g. a click on the window's own frame chrome
    CK_CHECK(back->active());
}

CK_TEST(on_descendant_mouse_down_is_a_no_op_for_a_view_outside_any_owned_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));

    ckv::ui::View stray;  // detached: not inside the desktop's tree at all
    desktop.on_descendant_mouse_down(stray);
    CK_CHECK(w->active());  // unchanged, no crash walking a detached view's parent chain
}

CK_TEST(remove_window_for_a_window_not_owned_by_this_desktop_returns_null) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window stray("stray");
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
        Window stray("stray");
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
    desktop.set_context(f.ctx());
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
    desktop.set_context(f.ctx());
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
    desktop.set_context(f.ctx());
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
    desktop.set_context(f.ctx());
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
    desktop.set_context(f.ctx());
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

// --- Size-hint-change propagation (M9/WP-16, E10) -----------------------

namespace {
class MutableHeightView : public ckv::ui::View {
public:
    explicit MutableHeightView(int height) : height_(height) {}
    void set_height(int height) {
        height_ = height;
        size_hint_changed();
    }
    ckv::ui::SizeHint vertical_size_hint() const override {
        return ckv::ui::SizeHint{0, height_, ckv::ui::kUnboundedExtent};
    }

private:
    int height_;
};
}  // namespace

CK_TEST(a_docked_views_own_height_change_repositions_it_without_a_desktop_resize) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24}, f.theme, f.roles.dialog_background);
    auto* status = desktop.dock_bottom(std::make_unique<MutableHeightView>(1));
    CK_CHECK(status->bounds() == (Rect{0, 23, 60, 1}));

    status->set_height(2);  // e.g. a status line that now wraps to two rows

    CK_CHECK(status->bounds() == (Rect{0, 22, 60, 2}));
}

CK_TEST(a_docked_views_own_height_change_re_clamps_windows_against_the_shrunk_content_area) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 20, 15, 4});  // bottom edge at row 24, the very last row
    auto* status = desktop.dock_bottom(std::make_unique<MutableHeightView>(1));
    CK_CHECK(w->bounds().y + w->bounds().height <= 23);  // already clamped by the initial dock

    status->set_height(2);  // now occupies rows 22-23, shrinking content_area() further

    CK_CHECK(w->bounds().y + w->bounds().height <= 22);
}

// --- Desktop growth tracking (DesktopGrowPolicy, M8 WP-4) ---------------

CK_TEST(add_window_installs_a_zoom_area_provider_that_returns_content_area) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!!w->zoom_area_provider);
    CK_CHECK(w->zoom_area_provider() == desktop.content_area());
}

CK_TEST(a_zoomed_window_refills_the_content_area_after_the_desktop_grows) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 5, 20, 10});
    w->toggle_zoom(w->zoom_area_provider());
    CK_CHECK(w->bounds() == desktop.content_area());

    desktop.set_bounds(Rect{0, 0, 120, 50});

    CK_CHECK(w->zoomed());
    CK_CHECK(w->bounds() == desktop.content_area());  // tracked the growth, not stale at the old size
}

CK_TEST(a_keep_filling_window_tracks_desktop_growth_without_being_zoomed) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_grow_policy(ckv::widgets::DesktopGrowPolicy::KeepFilling);

    desktop.set_bounds(Rect{0, 0, 120, 50});

    CK_CHECK(!w->zoomed());
    CK_CHECK(w->bounds() == desktop.content_area());
}

CK_TEST(an_anchor_edges_window_keeps_its_margin_to_the_right_and_bottom_edges_as_the_desktop_grows) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_grow_policy(ckv::widgets::DesktopGrowPolicy::AnchorEdges);
    w->set_bounds(Rect{5, 5, 20, 10});  // right margin 80-25=55, bottom margin 24-15=9

    desktop.set_bounds(Rect{0, 0, 120, 40});  // +40 width, +16 height

    CK_CHECK(w->bounds().x == 5);  // top-left origin untouched
    CK_CHECK(w->bounds().y == 5);
    CK_CHECK(w->bounds().width == 60);   // 20 + 40: right margin (55) preserved
    CK_CHECK(w->bounds().height == 26);  // 10 + 16: bottom margin (9) preserved
}

CK_TEST(a_none_policy_window_is_only_clamped_on_shrink_never_grown) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 5, 20, 10});  // default grow policy: None

    desktop.set_bounds(Rect{0, 0, 200, 60});  // grow substantially

    CK_CHECK(w->bounds() == (Rect{5, 5, 20, 10}));  // unchanged — still fits, so reposition_within is a no-op too
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
    desktop.set_context(f.ctx());
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
    desktop.set_context(f.ctx());
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

// --- detach_child (the polymorphic entry point for schedule_self_detach) --

CK_TEST(detach_child_on_a_window_added_via_add_window_also_removes_it_from_windows) {
    // Regression: widgets::schedule_self_detach used to call plain
    // View::remove_child, which only ever touches the child list —
    // for a window added via add_window() (tracked separately in
    // Desktop::windows_), that silently left a DANGLING pointer in
    // windows_ forever once the Window itself was destroyed.
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    desktop.set_context(f.ctx());
    Window* w = desktop.add_window(make_window(f));
    CK_CHECK(desktop.windows().size() == 1);

    auto owned = desktop.detach_child(w);
    CK_CHECK(owned != nullptr);
    CK_CHECK(desktop.windows().empty());
}

CK_TEST(detach_child_on_a_popup_added_via_add_popup_also_removes_it_from_popups) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    desktop.set_context(f.ctx());
    ckv::ui::View* popup = desktop.add_popup(std::make_unique<ckv::ui::View>());
    CK_CHECK(desktop.popups().size() == 1);

    auto owned = desktop.detach_child(popup);
    CK_CHECK(owned != nullptr);
    CK_CHECK(desktop.popups().empty());
}

CK_TEST(detach_child_on_a_plain_child_falls_back_to_ordinary_removal) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    desktop.set_context(f.ctx());
    ckv::ui::View* plain = desktop.add_child(std::make_unique<ckv::ui::View>());
    CK_CHECK(desktop.children().size() == 1);

    auto owned = desktop.detach_child(plain);
    CK_CHECK(owned != nullptr);
    CK_CHECK(desktop.children().empty());
}

CK_TEST(detach_child_on_a_window_added_via_plain_add_child_does_not_touch_windows) {
    // A window attached via add_child() directly (the convention the
    // library's own dialog factories use for throwaway, non-cycling
    // windows — see the M6 review's message_box.cpp fix) was never in
    // windows_ to begin with; detach_child must still remove it via
    // the plain-child path without crashing on the "not found" case.
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    desktop.set_context(f.ctx());
    Window* w = static_cast<Window*>(desktop.add_child(make_window(f)));
    CK_CHECK(desktop.windows().empty());

    auto owned = desktop.detach_child(w);
    CK_CHECK(owned != nullptr);
    CK_CHECK(desktop.children().empty());
}

// --- kClose/kQuit default handlers + exec_window (M9/WP-15, D-021) --------
//
// Unlike every test above, these need a REAL Application with the
// Desktop actually attached via add_child (not a standalone Desktop
// with set_context() called by hand) — on_attached() only fires
// through the real attach path, and that's what installs these
// defaults, exactly like MenuBar's own kMenu default (see
// test_menu.cpp's "F10 default activation" section, the same pattern).

namespace {
struct AttachedFixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    Fixture f;  // unused for styling here — only to match make_window()'s signature
    StandardRoles roles = intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;

    // Styles `app`'s OWN theme/roles (not `f`'s standalone ones) — real
    // attachment propagates THIS context to every descendant (see
    // View::add_child), so a Window attached here that later gets
    // painted (exec_window's pump calls step(), which paints) must
    // resolve real roles through it, exactly like every example's own
    // `app_.theme() = make_classic_theme(app_.roles(), roles_);`.
    AttachedFixture() {
        app.theme() = make_classic_theme(app.roles(), roles);
        auto owned = std::make_unique<Desktop>(Rect{0, 0, 80, 24}, app.theme(),
                                                roles.dialog_background);
        desktop = static_cast<Desktop*>(app.root().add_child(std::move(owned)));
    }
};
}  // namespace

CK_TEST(attaching_a_desktop_installs_itself_as_every_window_management_commands_default) {
    AttachedFixture af;
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kClose));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kQuit));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kZoom));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kNextWindow));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kPreviousWindow));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kTile));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kCascade));
}

CK_TEST(a_pre_existing_kclose_handler_is_not_overridden_by_attaching_a_desktop) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    bool custom_ran = false;
    app.commands().set_handler(ui::std_commands::kClose, [&] { custom_ran = true; });

    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto owned = std::make_unique<Desktop>(Rect{0, 0, 80, 24}, app.theme(),
                                            roles.dialog_background);
    app.root().add_child(std::move(owned));

    app.commands().execute(ui::std_commands::kClose);
    CK_CHECK(custom_ran);
}

CK_TEST(a_pre_existing_kquit_handler_is_not_overridden_by_attaching_a_desktop) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    bool custom_ran = false;
    app.commands().set_handler(ui::std_commands::kQuit, [&] { custom_ran = true; });

    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto owned = std::make_unique<Desktop>(Rect{0, 0, 80, 24}, app.theme(),
                                            roles.dialog_background);
    app.root().add_child(std::move(owned));

    app.commands().execute(ui::std_commands::kQuit);
    CK_CHECK(custom_ran);
    CK_CHECK(!app.quit_requested());  // the default sweep never ran, so it never requested quit
}

CK_TEST(a_pre_existing_ktile_handler_is_not_overridden_by_attaching_a_desktop) {
    // The guard is the SAME install_default_handler() call for all seven
    // commands — one representative non-kClose/kQuit case is enough to
    // prove it generalizes, without repeating this five more times.
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    bool custom_ran = false;
    app.commands().set_handler(ui::std_commands::kTile, [&] { custom_ran = true; });

    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto owned =
        std::make_unique<Desktop>(Rect{0, 0, 80, 24}, app.theme(), roles.dialog_background);
    app.root().add_child(std::move(owned));

    app.commands().execute(ui::std_commands::kTile);
    CK_CHECK(custom_ran);
}

CK_TEST(destroying_the_desktop_clears_only_the_default_handlers_it_installed) {
    AttachedFixture af;
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kClose));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kQuit));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kZoom));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kNextWindow));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kPreviousWindow));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kTile));
    CK_CHECK(af.app.commands().has_handler(ui::std_commands::kCascade));

    af.app.root().detach_child(af.desktop).reset();  // destroys the Desktop

    CK_CHECK(!af.app.commands().has_handler(ui::std_commands::kClose));
    CK_CHECK(!af.app.commands().has_handler(ui::std_commands::kQuit));
    CK_CHECK(!af.app.commands().has_handler(ui::std_commands::kZoom));
    CK_CHECK(!af.app.commands().has_handler(ui::std_commands::kNextWindow));
    CK_CHECK(!af.app.commands().has_handler(ui::std_commands::kPreviousWindow));
    CK_CHECK(!af.app.commands().has_handler(ui::std_commands::kTile));
    CK_CHECK(!af.app.commands().has_handler(ui::std_commands::kCascade));
}

CK_TEST(kclose_default_closes_the_active_window) {
    AttachedFixture af;
    Window* w = af.desktop->add_window(make_window(af.f));
    bool closed = false;
    w->on_closed = [&] { closed = true; };

    af.app.commands().execute(ui::std_commands::kClose);
    CK_CHECK(closed);
}

CK_TEST(kclose_default_is_a_no_op_with_no_active_window) {
    AttachedFixture af;
    CK_CHECK(af.app.commands().execute(ui::std_commands::kClose));  // the handler ran, harmlessly
}

CK_TEST(kquit_default_sweeps_every_window_and_requests_quit_when_none_veto) {
    AttachedFixture af;
    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    int closed_count = 0;
    a->on_closed = [&] { ++closed_count; };
    b->on_closed = [&] { ++closed_count; };

    af.app.commands().execute(ui::std_commands::kQuit);
    CK_CHECK(closed_count == 2);
    CK_CHECK(af.app.quit_requested());
}

CK_TEST(kquit_default_stops_at_the_first_veto_and_does_not_request_quit) {
    AttachedFixture af;
    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    int closed_count = 0;
    // b is topmost (added last) — the sweep goes front-to-back (topmost
    // first), so b is asked before a; vetoing it must stop the sweep
    // before a is ever asked, not just before quit is requested.
    b->close_request = [] { return false; };
    a->on_closed = [&] { ++closed_count; };
    b->on_closed = [&] { ++closed_count; };

    af.app.commands().execute(ui::std_commands::kQuit);
    CK_CHECK(closed_count == 0);
    CK_CHECK(!af.app.quit_requested());
}

// --- kZoom/kNextWindow/kPreviousWindow/kTile/kCascade default handlers
// (M10/WP-13 completion, D-029) — thin wiring proofs only: the underlying
// Window::toggle_zoom/Desktop::activate_next/activate_previous/tile/
// cascade are each already covered directly and in depth above; these
// just confirm the command actually reaches the right call. ------------

CK_TEST(kzoom_default_toggles_the_active_windows_zoom) {
    AttachedFixture af;
    Window* w = af.desktop->add_window(make_window(af.f));
    CK_CHECK(!w->zoomed());

    af.app.commands().execute(ui::std_commands::kZoom);
    CK_CHECK(w->zoomed());

    af.app.commands().execute(ui::std_commands::kZoom);
    CK_CHECK(!w->zoomed());
}

CK_TEST(kzoom_default_is_a_no_op_with_no_active_window) {
    AttachedFixture af;
    CK_CHECK(af.app.commands().execute(ui::std_commands::kZoom));  // the handler ran, harmlessly
}

CK_TEST(knextwindow_default_activates_the_next_window_in_cycling_order) {
    AttachedFixture af;
    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    CK_CHECK(af.desktop->active_window() == b);  // b, added last, activates on add

    af.app.commands().execute(ui::std_commands::kNextWindow);

    CK_CHECK(af.desktop->active_window() == a);  // wraps around from the last window to the first
}

CK_TEST(kpreviouswindow_default_activates_the_previous_window_in_cycling_order) {
    // Three windows, not two — with exactly two, "next" and "previous"
    // of the active one land on the same window, which wouldn't
    // distinguish this command's own wiring from kNextWindow's.
    AttachedFixture af;
    af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    Window* c = af.desktop->add_window(make_window(af.f));
    CK_CHECK(af.desktop->active_window() == c);

    af.app.commands().execute(ui::std_commands::kPreviousWindow);

    CK_CHECK(af.desktop->active_window() == b);
}

CK_TEST(ktile_default_arranges_every_window_into_the_content_area) {
    AttachedFixture af;
    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    a->set_bounds(Rect{0, 0, 80, 24});  // both windows start fully overlapping
    b->set_bounds(Rect{0, 0, 80, 24});

    af.app.commands().execute(ui::std_commands::kTile);

    // tile() gave each its own slice — no longer overlapping.
    CK_CHECK(a->bounds().x != b->bounds().x);
}

CK_TEST(kcascade_default_arranges_every_window_offset_diagonally) {
    AttachedFixture af;
    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    a->set_bounds(Rect{0, 0, 30, 15});
    b->set_bounds(Rect{0, 0, 30, 15});

    af.app.commands().execute(ui::std_commands::kCascade);

    CK_CHECK(a->bounds().x != b->bounds().x || a->bounds().y != b->bounds().y);
}

CK_TEST(exec_window_blocks_until_the_window_closes) {
    AttachedFixture af;
    auto modal = make_window(af.f);
    Window* modal_ptr = modal.get();
    modal->on_closed = [modal_ptr, &af] { schedule_self_detach(*modal_ptr, af.app); };

    bool still_modal_mid_pump = false;
    af.app.post([&] {
        still_modal_mid_pump = af.app.is_modal();
        modal_ptr->close();
    });

    CK_CHECK(af.desktop->windows().empty());
    af.desktop->exec_window(af.app, WindowHandle{std::move(modal), nullptr});

    CK_CHECK(still_modal_mid_pump);           // scoped for the duration of the pump...
    CK_CHECK(!af.app.is_modal());             // ...and unscoped again once it returns
    CK_CHECK(af.desktop->windows().empty());  // the modal window is gone
}

CK_TEST(exec_window_runs_posted_work_during_the_pump) {
    // The "post during modal" re-entrancy case (D-021, WP-15's own
    // acceptance criterion): posted work follows the ordinary
    // single-loop rules regardless of modality, since step() itself has
    // no notion of modal state at all — only dispatch() does.
    AttachedFixture af;
    auto modal = make_window(af.f);
    Window* modal_ptr = modal.get();
    modal->on_closed = [modal_ptr, &af] { schedule_self_detach(*modal_ptr, af.app); };

    bool unrelated_work_ran = false;
    af.app.post([&] { unrelated_work_ran = true; });  // ahead of the dismiss, same drain batch
    af.app.post([&] { modal_ptr->close(); });

    af.desktop->exec_window(af.app, WindowHandle{std::move(modal), nullptr});
    CK_CHECK(unrelated_work_ran);
}

CK_TEST(exec_window_is_not_disrupted_by_an_unrelated_windows_close_during_the_pump) {
    // The "close during modal" re-entrancy case: closing some OTHER
    // window while exec_window's pump is running for THIS one must not
    // end the pump early — it watches this specific window, not
    // "windows() changed."
    AttachedFixture af;
    Window* other = af.desktop->add_window(make_window(af.f));
    other->on_closed = [other, &af] { schedule_self_detach(*other, af.app); };

    auto modal = make_window(af.f);
    Window* modal_ptr = modal.get();
    modal->on_closed = [modal_ptr, &af] { schedule_self_detach(*modal_ptr, af.app); };

    bool modal_still_open_after_other_closed = false;
    af.app.post([&] { other->close(); });
    af.app.post([&] {
        modal_still_open_after_other_closed = af.app.is_modal();
        modal_ptr->close();
    });

    af.desktop->exec_window(af.app, WindowHandle{std::move(modal), nullptr});
    CK_CHECK(modal_still_open_after_other_closed);
    CK_CHECK(af.desktop->windows().empty());  // `other` and the modal both ended up closed
}

CK_TEST(exec_window_returns_promptly_if_quit_is_requested_before_the_window_closes) {
    // Without this, a SIGINT delivered while an exec_* call is blocked
    // (run()'s own SIGINT handler just sets the atomic flag, M9/WP-14)
    // would leave the process stuck forever inside the pump — nothing
    // would ever satisfy "the window closed," and step()/wake() alone
    // have no way to interrupt a poll() that's waiting on terminal
    // input which will never arrive. run_until()'s own
    // quit_requested() check is what prevents this hang.
    AttachedFixture af;
    auto modal = make_window(af.f);
    Window* modal_ptr = modal.get();
    modal->on_closed = [modal_ptr, &af] { schedule_self_detach(*modal_ptr, af.app); };

    af.app.post([&] { af.app.request_quit(); });  // never actually dismisses the modal

    af.desktop->exec_window(af.app, WindowHandle{std::move(modal), nullptr});

    CK_CHECK(af.app.quit_requested());
    CK_CHECK(!af.app.is_modal());  // pop_modal still ran even though the window never closed
    CK_CHECK(af.desktop->windows().size() == 1);  // the modal is still attached — never dismissed
}
