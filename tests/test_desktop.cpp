// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/desktop.hpp"

#include <functional>
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

class ResizeSelfRemovingWindow final : public Window {
public:
    ResizeSelfRemovingWindow(Desktop& desktop, std::string title)
        : Window(std::move(title)), desktop_(desktop) {}

    void on_resized() override {
        Window::on_resized();
        if (!armed_) return;
        std::unique_ptr<Window> detached = desktop_.remove_window(this);
        CK_CHECK(detached.get() == this);
        detached.reset();
    }

    void arm() noexcept { armed_ = true; }

private:
    Desktop& desktop_;
    bool armed_ = false;
};

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
    CK_EXPECT_ABORT({
        Fixture f;
        Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
        desktop.add_window(make_window(f));
        Window stray("stray");
        desktop.activate(&stray);  // must abort: not owned by this desktop
    });
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

CK_TEST(snapshot_restore_recovers_window_geometry_zoom_active_state_and_z_order) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    a->set_bounds(Rect{1, 1, 20, 8});
    b->set_bounds(Rect{4, 3, 22, 9});
    c->set_bounds(Rect{7, 5, 24, 10});
    b->toggle_zoom(desktop.content_area());
    desktop.activate(a);
    const Desktop::Snapshot snapshot = desktop.snapshot();
    ckv::ui::View* popup = desktop.add_popup(std::make_unique<ckv::ui::View>());

    a->set_bounds(Rect{30, 10, 15, 6});
    b->toggle_zoom(desktop.content_area());
    c->set_bounds(Rect{35, 12, 16, 7});
    desktop.activate(c);

    desktop.restore(snapshot);

    CK_CHECK(a->bounds() == (Rect{1, 1, 20, 8}));
    CK_CHECK(b->zoomed());
    CK_CHECK(b->bounds() == desktop.content_area());
    CK_CHECK(c->bounds() == (Rect{7, 5, 24, 10}));
    CK_CHECK(desktop.active_window() == a);
    CK_CHECK(a->active());
    CK_CHECK(!c->active());
    CK_CHECK(desktop.children()[0].get() == b);
    CK_CHECK(desktop.children()[1].get() == c);
    CK_CHECK(desktop.children()[2].get() == a);
    CK_CHECK(desktop.children().back().get() == popup);
}

CK_TEST(snapshot_restore_ignores_windows_that_have_left_the_desktop) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    const Desktop::Snapshot snapshot = desktop.snapshot();

    auto detached = desktop.remove_window(b);
    CK_CHECK(detached.get() == b);
    a->set_bounds(Rect{9, 9, 20, 6});
    desktop.restore(snapshot);

    CK_CHECK(desktop.windows().size() == 1);
    CK_CHECK(desktop.windows()[0] == a);
    CK_CHECK(a->bounds() == snapshot.windows[0].bounds);
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

CK_TEST(tile_and_cascade_survive_a_window_that_destroys_itself_from_on_resized) {
    Fixture f;
    const auto exercise = [&f](bool cascade) {
        Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
        Window* survivor = desktop.add_window(make_window(f, "Survivor"));
        auto self_removing = std::make_unique<ResizeSelfRemovingWindow>(desktop, "Self removing");
        ResizeSelfRemovingWindow* self_removing_ptr = self_removing.get();
        desktop.add_window(std::move(self_removing));
        self_removing_ptr->arm();

        if (cascade)
            desktop.cascade();
        else
            desktop.tile();

        CK_CHECK(desktop.windows().size() == 1);
        CK_CHECK(desktop.windows().front() == survivor);
    };
    exercise(false);
    exercise(true);
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

CK_TEST(desktop_resize_survives_a_window_that_destroys_itself_from_on_resized) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    auto self_removing = std::make_unique<ResizeSelfRemovingWindow>(desktop, "Self removing");
    ResizeSelfRemovingWindow* self_removing_ptr = self_removing.get();
    Window* survivor = desktop.add_window(make_window(f, "Survivor"));
    desktop.add_window(std::move(self_removing));
    self_removing_ptr->arm();

    desktop.set_bounds(Rect{0, 0, 100, 40});

    CK_CHECK(desktop.windows().size() == 1);
    CK_CHECK(desktop.windows().front() == survivor);
    CK_CHECK(survivor->bounds().width <= 100);
    CK_CHECK(survivor->bounds().height <= 40);
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

CK_TEST(overlapping_window_shadows_dim_the_desktop_exactly_once) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    desktop.set_context(f.ctx());
    Window* first = desktop.add_window(make_window(f, "First"));
    first->set_bounds(Rect{2, 2, 5, 3});
    Window* second = desktop.add_window(make_window(f, "Second"));
    second->set_bounds(Rect{3, 2, 5, 3});

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);

    const ckv::Style background_style = f.theme.resolve(f.roles.dialog_background);
    CK_CHECK(surface.at(ckv::Point{8, 3}).style() ==
             ckv::scene::default_dim(background_style));
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

CK_TEST(a_non_shadow_casting_view_leaves_its_footprint_undimmed) {
    // A plain View opts out of View::casts_shadow(). It remains a safe
    // Desktop child and must not acquire a shadow merely by participating in
    // the z-ordered paint pass.
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

CK_TEST(a_foreground_window_frame_does_not_merge_with_a_background_window_frame) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    desktop.set_context(f.ctx());

    Window* background = desktop.add_window(make_window(f, "Background"));
    background->set_bounds(Rect{2, 2, 14, 4});  // bottom edge: y=5, x=2..15

    Window* foreground = desktop.add_window(make_window(f, "Front"));
    foreground->set_bounds(Rect{12, 3, 12, 7});  // active double frame, painted last

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);

    // The foreground's left edge crosses the background's bottom edge
    // at (12,5). Z-order means the later double vertical line replaces
    // the earlier single horizontal line; the two windows are unrelated
    // drawing contexts, so this must never become a junction ("╬").
    CK_CHECK(surface.at(ckv::Point{12, 5}).grapheme() == "║");
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

CK_TEST(removing_a_docked_view_clears_the_desktops_content_area_observer) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    auto dock = std::make_unique<ckv::ui::View>();
    dock->set_preferred_size(ckv::Size{80, 2});
    ckv::ui::View* const dock_ptr = desktop.dock_top(std::move(dock));
    CK_CHECK(desktop.content_area() == (Rect{0, 2, 80, 22}));

    std::unique_ptr<ckv::ui::View> detached = desktop.remove_child(dock_ptr);
    CK_CHECK(detached.get() == dock_ptr);
    detached.reset();
    CK_CHECK(desktop.top_dock() == nullptr);
    CK_CHECK(desktop.content_area() == (Rect{0, 0, 80, 24}));
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

CK_TEST(a_zoomed_window_refills_the_content_area_after_the_desktop_grows) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background);
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 5, 20, 10});
    w->toggle_zoom(desktop.content_area());
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

CK_TEST(a_direct_window_attachment_uses_the_same_desktop_management_path_as_add_window) {
    // View's generic public insertion is dynamically dispatched.  No
    // direct Window child may bypass Desktop's ownership/bookkeeping.
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    desktop.set_context(f.ctx());
    ui::View& generic_parent = desktop;
    Window* w = static_cast<Window*>(generic_parent.add_child(make_window(f)));
    CK_CHECK(desktop.windows().size() == 1);
    CK_CHECK(desktop.windows().front() == w);
    CK_CHECK(desktop.active_window() == w);
    CK_CHECK(w->active());

    auto owned = generic_parent.remove_child(w);
    CK_CHECK(owned != nullptr);
    CK_CHECK(desktop.children().empty());
    CK_CHECK(desktop.windows().empty());
    CK_CHECK(desktop.active_window() == nullptr);
}

CK_TEST(a_window_retained_after_detaching_and_destroying_its_desktop_cannot_reach_the_former_desktop) {
    // ASan/UBSan turns this interaction into a regression proof for the
    // old captured-Desktop callback: the retained Window still exposes
    // its normal mouse API, but must fall back to its own safe bounds.
    Fixture f;
    auto desktop = std::make_unique<Desktop>(Rect{0, 0, 40, 20}, f.theme, f.roles.dialog_background);
    Window* raw = desktop->add_window(make_window(f));
    raw->set_bounds(Rect{5, 5, 20, 10});
    std::unique_ptr<Window> retained = desktop->remove_window(raw);
    desktop.reset();

    CK_CHECK(retained->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                                ckv::Point{23, 5}, std::nullopt, ckv::Modifier::None}));
    CK_CHECK(retained->zoomed());
    CK_CHECK(retained->bounds() == (Rect{5, 5, 20, 10}));
}

CK_TEST(removing_a_modal_window_through_the_generic_desktop_api_clears_modal_routing_state) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<Desktop>(app.root().bounds(), app.theme(), roles.desktop_background);
    Desktop* desktop = static_cast<Desktop*>(app.root().add_child(std::move(desktop_owned)));
    Window* modal = desktop->add_window(std::make_unique<Window>("Modal"));
    app.push_modal(*modal);
    CK_CHECK(app.is_modal());

    std::unique_ptr<ui::View> detached = desktop->remove_child(modal);
    CK_CHECK(detached.get() == modal);
    CK_CHECK(!app.is_modal());
}

// --- kClose/kQuit default handlers + exec_modal (M9/WP-15, D-021) ---------
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
    // painted (exec_modal's pump calls step(), which paints) must
    // resolve real roles through it, exactly like every example's own
    // `app_.theme() = make_classic_theme(app_.roles(), roles_);`.
    AttachedFixture() {
        app.theme() = make_classic_theme(app.roles(), roles);
        auto owned = std::make_unique<Desktop>(Rect{0, 0, 80, 24}, app.theme(),
                                                roles.dialog_background);
        desktop = static_cast<Desktop*>(app.root().add_child(std::move(owned)));
    }
};

class FocusLossProbe final : public ui::View {
public:
    FocusLossProbe() { set_focus_policy(ui::FocusPolicy::TabStop); }

    void on_focus(const ckv::FocusEvent& event) override {
        if (!event.gained && on_focus_lost) on_focus_lost();
    }

    std::function<void()> on_focus_lost;
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

CK_TEST(nonblocking_presentation_returns_null_when_focus_loss_removes_its_new_window) {
    AttachedFixture af;
    auto focus = std::make_unique<FocusLossProbe>();
    FocusLossProbe* focus_ptr = focus.get();
    af.app.root().add_child(std::move(focus));
    af.app.set_focus(focus_ptr);

    auto modeless = make_window(af.f, "modeless");
    Window* modeless_ptr = modeless.get();
    focus_ptr->on_focus_lost = [&] {
        std::unique_ptr<Window> detached = af.desktop->remove_window(modeless_ptr);
        CK_CHECK(detached.get() == modeless_ptr);
        detached.reset();
    };
    CK_CHECK(af.desktop->present_modeless(WindowHandle{std::move(modeless), nullptr}, af.app) == nullptr);
    CK_CHECK(af.desktop->windows().empty());

    af.app.set_focus(focus_ptr);
    auto modal = make_window(af.f, "modal");
    Window* modal_ptr = modal.get();
    focus_ptr->on_focus_lost = [&] {
        std::unique_ptr<Window> detached = af.desktop->remove_window(modal_ptr);
        CK_CHECK(detached.get() == modal_ptr);
        detached.reset();
    };
    CK_CHECK(af.desktop->present_modal(WindowHandle{std::move(modal), nullptr}, af.app) == nullptr);
    CK_CHECK(af.desktop->windows().empty());
    CK_CHECK(!af.app.is_modal());
}

CK_TEST(terminal_mouse_zoom_and_restore_track_the_desktop_after_a_resize) {
    // This is deliberately an end-to-end interaction: HeadlessTerminal
    // input enters Application::step(), hit testing selects the Window,
    // and only then does its private Desktop-bound zoom target run.
    AttachedFixture af;
    auto owned = make_window(af.f, "Zoom");
    owned->set_bounds(Rect{10, 5, 20, 10});
    Window* window = af.desktop->add_window(std::move(owned));
    const Rect restored = window->bounds();

    af.app.step(0);  // establish the initial composed frame
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{28, 5},
                                         std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    CK_CHECK(window->zoomed());
    CK_CHECK(window->bounds() == af.desktop->content_area());

    af.term.resize(ckv::Size{100, 30});
    CK_CHECK(af.app.step(0));
    CK_CHECK(window->zoomed());
    CK_CHECK(window->bounds() == af.desktop->content_area());

    const Rect zoomed = window->absolute_bounds();
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                         ckv::Point{zoomed.x + window->bounds().width - 2, zoomed.y},
                                         std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    CK_CHECK(!window->zoomed());
    CK_CHECK(window->bounds() == restored);
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

CK_TEST(kquit_default_remains_safe_when_each_close_detaches_its_window) {
    AttachedFixture af;
    Window* first = af.desktop->add_window(make_window(af.f));
    Window* second = af.desktop->add_window(make_window(af.f));
    int closed_count = 0;
    first->on_closed = [&] {
        ++closed_count;
        CK_CHECK(af.desktop->remove_window(first) != nullptr);
    };
    second->on_closed = [&] {
        ++closed_count;
        CK_CHECK(af.desktop->remove_window(second) != nullptr);
    };

    af.app.commands().execute(ui::std_commands::kQuit);
    CK_CHECK(closed_count == 2);
    CK_CHECK(af.desktop->windows().empty());
    CK_CHECK(af.app.quit_requested());
}

CK_TEST(close_request_may_destroy_its_window_without_a_post_callback_use_after_free) {
    AttachedFixture af;
    Window* window = af.desktop->add_window(make_window(af.f, "self-destroying"));
    bool on_closed_called = false;
    window->close_request = [&] {
        std::unique_ptr<Window> detached = af.desktop->remove_window(window);
        CK_CHECK(detached.get() == window);
        detached.reset();
        return true;
    };
    window->on_closed = [&] { on_closed_called = true; };

    // The raw observer ends at the member-call boundary. Under ASan this
    // proves close() does not read either callback member after close_request
    // has destroyed the instance.
    CK_CHECK(window->close());
    CK_CHECK(!on_closed_called);
    CK_CHECK(af.desktop->windows().empty());
}

CK_TEST(kquit_default_skips_destroyed_starters_and_never_sweeps_a_reentrant_replacement) {
    AttachedFixture af;
    Window* first = af.desktop->add_window(make_window(af.f, "first"));
    Window* middle = af.desktop->add_window(make_window(af.f, "middle"));
    Window* top = af.desktop->add_window(make_window(af.f, "top"));
    Window* replacement = nullptr;
    int first_closed = 0;
    int top_closed = 0;
    int replacement_close_requests = 0;

    first->on_closed = [&] {
        ++first_closed;
        CK_CHECK(af.desktop->remove_window(first) != nullptr);
    };
    top->on_closed = [&] {
        ++top_closed;
        // This is the difficult re-entrant case: invalidate an unvisited
        // snapshot entry, force the live vector to change shape, and add a
        // new window while close() is still on the stack.
        CK_CHECK(af.desktop->remove_window(middle) != nullptr);
        replacement = af.desktop->add_window(make_window(af.f, "replacement"));
        replacement->close_request = [&] {
            ++replacement_close_requests;
            return true;
        };
        CK_CHECK(af.desktop->remove_window(top) != nullptr);
    };

    CK_CHECK(af.app.execute_command(ui::std_commands::kQuit));
    CK_CHECK(top_closed == 1);
    CK_CHECK(first_closed == 1);
    CK_CHECK(replacement_close_requests == 0);
    CK_CHECK(af.desktop->windows().size() == 1);
    CK_CHECK(af.desktop->windows().front() == replacement);
    CK_CHECK(af.app.quit_requested());
}

CK_TEST(kquit_default_ignores_a_recursive_quit_from_a_close_callback) {
    AttachedFixture af;
    Window* first = af.desktop->add_window(make_window(af.f, "first"));
    Window* veto = af.desktop->add_window(make_window(af.f, "veto"));
    Window* top = af.desktop->add_window(make_window(af.f, "top"));
    int top_close_calls = 0;
    int veto_calls = 0;
    int first_close_calls = 0;

    // The front-most close callback invokes the same default command again.
    // Without the sweep guard that recursively re-enters top->close() before
    // the outer sweep can reach the next window's veto.
    top->on_closed = [&] {
        ++top_close_calls;
        CK_CHECK(af.app.execute_command(ui::std_commands::kQuit));
    };
    veto->close_request = [&] {
        ++veto_calls;
        return false;
    };
    first->on_closed = [&] { ++first_close_calls; };

    CK_CHECK(af.app.execute_command(ui::std_commands::kQuit));
    CK_CHECK(top_close_calls == 1);
    CK_CHECK(veto_calls == 1);
    CK_CHECK(first_close_calls == 0);
    CK_CHECK(!af.app.quit_requested());
}

CK_TEST(kquit_default_survives_a_close_callback_that_destroys_its_desktop) {
    AttachedFixture af;
    Window* top = af.desktop->add_window(make_window(af.f, "top"));
    top->on_closed = [&] {
        std::unique_ptr<ui::View> detached = af.app.root().remove_child(af.desktop);
        CK_CHECK(detached.get() == af.desktop);
        detached.reset();
    };

    CK_CHECK(af.app.execute_command(ui::std_commands::kQuit));
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

CK_TEST(exec_modal_blocks_until_the_window_closes) {
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
    af.desktop->exec_modal(af.app, WindowHandle{std::move(modal), nullptr});

    CK_CHECK(still_modal_mid_pump);           // scoped for the duration of the pump...
    CK_CHECK(!af.app.is_modal());             // ...and unscoped again once it returns
    CK_CHECK(af.desktop->windows().empty());  // the modal window is gone
}

CK_TEST(exec_modal_runs_posted_work_during_the_pump) {
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

    af.desktop->exec_modal(af.app, WindowHandle{std::move(modal), nullptr});
    CK_CHECK(unrelated_work_ran);
}

CK_TEST(exec_modal_is_not_disrupted_by_an_unrelated_windows_close_during_the_pump) {
    // The "close during modal" re-entrancy case: closing some OTHER
    // window while exec_modal's pump is running for THIS one must not
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

    af.desktop->exec_modal(af.app, WindowHandle{std::move(modal), nullptr});
    CK_CHECK(modal_still_open_after_other_closed);
    CK_CHECK(af.desktop->windows().empty());  // `other` and the modal both ended up closed
}

CK_TEST(exec_modal_survives_a_posted_callback_that_destroys_its_desktop) {
    AttachedFixture af;
    auto modal = make_window(af.f, "Original");
    bool desktop_destroyed = false;
    af.app.post([&] {
        std::unique_ptr<ui::View> detached = af.app.root().remove_child(af.desktop);
        CK_CHECK(detached.get() == af.desktop);
        detached.reset();
        desktop_destroyed = true;
    });

    // ASan exercises the completion predicate after the posted callback. The
    // call must return through Application's normal scope cleanup without
    // reading the destroyed Desktop's window vector.
    af.desktop->exec_modal(af.app, WindowHandle{std::move(modal), nullptr});

    CK_CHECK(desktop_destroyed);
    CK_CHECK(!af.app.is_modal());
}

CK_TEST(exec_modal_host_quit_force_detaches_the_open_window_without_a_close_veto) {
    // A host shutdown request can arrive while an exec_* call is blocked.
    // It must end the pump and cannot leave an ordinary window attached but
    // silently modeless. This is intentionally not a user close request, so
    // a content veto must never run.
    AttachedFixture af;
    auto modal = make_window(af.f);
    Window* modal_ptr = modal.get();
    modal->on_closed = [modal_ptr, &af] { schedule_self_detach(*modal_ptr, af.app); };
    int close_veto_calls = 0;
    modal->close_request = [&] {
        ++close_veto_calls;
        return false;
    };

    af.app.post([&] { af.app.request_quit(); });  // never actually dismisses the modal

    af.desktop->exec_modal(af.app, WindowHandle{std::move(modal), nullptr});

    CK_CHECK(af.app.quit_requested());
    CK_CHECK(close_veto_calls == 0);
    CK_CHECK(!af.app.is_modal());
    CK_CHECK(af.desktop->windows().empty());
}

CK_TEST(exec_modal_host_quit_never_pops_a_reentrant_replacement_modal) {
    AttachedFixture af;
    auto modal = make_window(af.f, "Original");
    Window* replacement = nullptr;
    modal->on_detached = [&] {
        auto next = make_window(af.f, "Replacement");
        replacement = af.desktop->present_modal(WindowHandle{std::move(next), nullptr}, af.app);
    };
    af.app.post([&] { af.app.request_quit(); });

    af.desktop->exec_modal(af.app, WindowHandle{std::move(modal), nullptr});

    // Removing the original scope invokes its detachment callback. That
    // callback is allowed to establish a newer scope; exec_modal's cleanup
    // must target its retained identity rather than popping the replacement.
    CK_CHECK(replacement != nullptr);
    CK_CHECK(af.desktop->windows().size() == 1);
    CK_CHECK(af.desktop->windows().front() == replacement);
    CK_CHECK(af.app.is_modal());

    std::unique_ptr<Window> detached = af.desktop->remove_window(replacement);
    CK_CHECK(detached.get() == replacement);
    CK_CHECK(!af.app.is_modal());
}
