// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/desktop.hpp"

#include <functional>
#include <set>

#include "cvision/testing/cktest.hpp"
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
    Context ctx() { return Context{&theme, &registry, nullptr}; }
};

std::unique_ptr<Window> make_window(Fixture&, std::string title = "W") {
    return std::make_unique<Window>(std::move(title));
}

// Every cell of `area` covered by exactly one window: no gap the desktop
// shows through, no cell claimed twice. Counted cell by cell on purpose —
// re-deriving it from the same rect arithmetic filled_tile_fractions()
// uses would only prove that function agrees with itself.
bool covers_exactly_once(const Desktop& desktop, Rect area) {
    std::vector<int> hits(static_cast<std::size_t>(area.width) * static_cast<std::size_t>(area.height), 0);
    for (Window* window : desktop.windows()) {
        const Rect b = window->bounds();
        if (b.empty()) return false;
        if (b.x < area.x || b.x + b.width > area.x + area.width) return false;
        if (b.y < area.y || b.y + b.height > area.y + area.height) return false;
        for (int y = b.y; y < b.y + b.height; ++y)
            for (int x = b.x; x < b.x + b.width; ++x)
                ++hits[static_cast<std::size_t>((y - area.y) * area.width + (x - area.x))];
    }
    for (int hit : hits)
        if (hit != 1) return false;
    return true;
}

bool about_equal(double a, double b) { return a - b < 1e-9 && b - a < 1e-9; }

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
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* w = desktop.add_window(make_window(f));
    CK_CHECK(desktop.active_window() == w);
    CK_CHECK(w->active());
}

CK_TEST(adding_a_second_window_activates_it_and_deactivates_the_first) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    CK_CHECK(desktop.active_window() == b);
    CK_CHECK(!a->active());
    CK_CHECK(b->active());
}

CK_TEST(a_newly_added_window_becomes_the_topmost_child) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    CK_CHECK(desktop.children().back().get() == b);
    (void)a;
}

// --- Click-to-activate/raise (on_descendant_mouse_down, M8 WP-3) -------

CK_TEST(on_descendant_mouse_down_activates_and_raises_the_owning_window_for_a_deep_descendant) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* back = desktop.add_window(make_window(f, "Back"));
    desktop.add_window(make_window(f, "Front"));

    CK_CHECK(!back->active());
    desktop.on_descendant_mouse_down(*back);  // e.g. a click on the window's own frame chrome
    CK_CHECK(back->active());
}

CK_TEST(on_descendant_mouse_down_is_a_no_op_for_a_view_outside_any_owned_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* w = desktop.add_window(make_window(f));

    ckv::ui::View stray;  // detached: not inside the desktop's tree at all
    desktop.on_descendant_mouse_down(stray);
    CK_CHECK(w->active());  // unchanged, no crash walking a detached view's parent chain
}

CK_TEST(remove_window_for_a_window_not_owned_by_this_desktop_returns_null) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window stray("stray");
    CK_CHECK(desktop.remove_window(&stray) == nullptr);
}

CK_TEST(removing_the_active_window_activates_the_new_topmost_remaining_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    CK_CHECK(desktop.active_window() == b);
    desktop.remove_window(b);
    CK_CHECK(desktop.active_window() == a);
    CK_CHECK(a->active());
}

CK_TEST(removing_the_last_window_leaves_no_active_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    desktop.remove_window(a);
    CK_CHECK(desktop.active_window() == nullptr);
    CK_CHECK(desktop.windows().empty());
}

CK_TEST(removing_an_inactive_window_does_not_disturb_the_active_one) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));  // b is active
    desktop.remove_window(a);
    CK_CHECK(desktop.active_window() == b);
    CK_CHECK(b->active());
}

CK_TEST(activate_on_an_already_active_window_is_a_no_op) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    int calls = 0;
    a->set_dirty_rect_sink([&](Rect) { ++calls; });
    desktop.activate(a);  // already active
    CK_CHECK(calls == 0);
}

CK_TEST(activating_an_older_window_raises_it_to_the_front_and_reactivates_it) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
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
        Desktop desktop(Rect{0, 0, 80, 24});
        desktop.add_window(make_window(f));
        Window stray("stray");
        desktop.activate(&stray);  // must abort: not owned by this desktop
    });
}

// --- Cycling ---------------------------------------------------------

CK_TEST(activate_next_and_previous_are_no_ops_with_zero_or_one_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.activate_next();  // zero windows
    CK_CHECK(desktop.active_window() == nullptr);
    Window* a = desktop.add_window(make_window(f));
    desktop.activate_next();  // one window
    CK_CHECK(desktop.active_window() == a);
}

CK_TEST(activate_next_visits_every_window_exactly_once_before_repeating) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    desktop.select_by_number(1);
    CK_CHECK(desktop.active_window() == a);
}

CK_TEST(select_by_number_out_of_range_is_a_harmless_no_op) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    desktop.select_by_number(0);   // below range
    CK_CHECK(desktop.active_window() == a);
    desktop.select_by_number(99);  // above range
    CK_CHECK(desktop.active_window() == a);
}

CK_TEST(snapshot_restore_recovers_window_geometry_zoom_active_state_and_z_order) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 90, 24});
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
    Desktop desktop(Rect{0, 0, 10, 24});  // 10 / 3 = 3 remainder 1
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
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.tile();
    CK_CHECK(true);
}

CK_TEST(tile_and_cascade_survive_a_window_that_destroys_itself_from_on_resized) {
    Fixture f;
    const auto exercise = [&f](bool cascade) {
        Desktop desktop(Rect{0, 0, 80, 24});
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

// --- Tile Horizontally / Tile Vertically (U4-b) ---------------------------
//
// The two words are used inconsistently across desktops, so each test names
// the arrangement rather than the word: horizontal bands are full-WIDTH and
// stack top to bottom; vertical bands are full-HEIGHT and stand side by side.

CK_TEST(tile_vertically_stacks_full_width_bands_top_to_bottom) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    desktop.tile_vertically();
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 12}));
    CK_CHECK(b->bounds() == (Rect{0, 12, 80, 12}));

    Window* c = desktop.add_window(make_window(f));
    desktop.tile_vertically();
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 8}));
    CK_CHECK(b->bounds() == (Rect{0, 8, 80, 8}));
    CK_CHECK(c->bounds() == (Rect{0, 16, 80, 8}));

    Window* d = desktop.add_window(make_window(f));
    desktop.tile_vertically();
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 6}));
    CK_CHECK(b->bounds() == (Rect{0, 6, 80, 6}));
    CK_CHECK(c->bounds() == (Rect{0, 12, 80, 6}));
    CK_CHECK(d->bounds() == (Rect{0, 18, 80, 6}));
}

CK_TEST(tile_horizontally_stands_full_height_bands_side_by_side) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    desktop.tile_horizontally();
    CK_CHECK(a->bounds() == (Rect{0, 0, 40, 24}));
    CK_CHECK(b->bounds() == (Rect{40, 0, 40, 24}));

    Window* c = desktop.add_window(make_window(f));
    desktop.tile_horizontally();
    CK_CHECK(a->bounds() == (Rect{0, 0, 26, 24}));
    CK_CHECK(b->bounds() == (Rect{26, 0, 26, 24}));
    CK_CHECK(c->bounds() == (Rect{52, 0, 28, 24}));  // last band absorbs the leftover 2

    Window* d = desktop.add_window(make_window(f));
    desktop.tile_horizontally();
    CK_CHECK(a->bounds() == (Rect{0, 0, 20, 24}));
    CK_CHECK(b->bounds() == (Rect{20, 0, 20, 24}));
    CK_CHECK(c->bounds() == (Rect{40, 0, 20, 24}));
    CK_CHECK(d->bounds() == (Rect{60, 0, 20, 24}));
}

CK_TEST(tile_horizontally_is_the_arrangement_tile_has_always_produced) {
    // kTile is a standard command applications already bind, and the axis
    // commands are additive: tile() must keep placing windows exactly where
    // it always has, which is what tile_horizontally() names — the axis is
    // what the windows are laid out ALONG, so a row of them side by side is
    // the horizontal one.
    Fixture f;
    Desktop tiled(Rect{0, 0, 90, 24});
    Desktop sideways(Rect{0, 0, 90, 24});
    for (int i = 0; i < 3; ++i) {
        tiled.add_window(make_window(f));
        sideways.add_window(make_window(f));
    }
    tiled.tile();
    sideways.tile_horizontally();
    for (std::size_t i = 0; i < 3; ++i)
        CK_CHECK(tiled.windows()[i]->bounds() == sideways.windows()[i]->bounds());
    CK_CHECK(tiled.windows()[0]->bounds() == (Rect{0, 0, 30, 24}));
}

CK_TEST(tile_grid_lays_a_near_square_grid_with_a_full_width_last_row) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    desktop.tile_grid();  // n=2: 2 columns, 1 row
    CK_CHECK(a->bounds() == (Rect{0, 0, 40, 24}));
    CK_CHECK(b->bounds() == (Rect{40, 0, 40, 24}));

    Window* c = desktop.add_window(make_window(f));
    desktop.tile_grid();  // n=3: 2 columns, 2 rows — the last row holds one
    CK_CHECK(a->bounds() == (Rect{0, 0, 40, 12}));
    CK_CHECK(b->bounds() == (Rect{40, 0, 40, 12}));
    // Stretched across the whole width rather than stopping at the column
    // grid: a half-empty bottom row would be desktop showing through, and
    // the arrangement would stop counting as a filled tiling.
    CK_CHECK(c->bounds() == (Rect{0, 12, 80, 12}));

    Window* d = desktop.add_window(make_window(f));
    desktop.tile_grid();  // n=4: the 2x2 everyone means by "grid"
    CK_CHECK(a->bounds() == (Rect{0, 0, 40, 12}));
    CK_CHECK(b->bounds() == (Rect{40, 0, 40, 12}));
    CK_CHECK(c->bounds() == (Rect{0, 12, 40, 12}));
    CK_CHECK(d->bounds() == (Rect{40, 12, 40, 12}));
}

CK_TEST(tile_grid_gives_five_windows_three_columns_and_a_two_window_last_row) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 90, 24});
    for (int i = 0; i < 5; ++i) desktop.add_window(make_window(f));
    desktop.tile_grid();  // ceil(sqrt(5)) = 3 columns, 2 rows
    const auto& w = desktop.windows();
    CK_CHECK(w[0]->bounds() == (Rect{0, 0, 30, 12}));
    CK_CHECK(w[1]->bounds() == (Rect{30, 0, 30, 12}));
    CK_CHECK(w[2]->bounds() == (Rect{60, 0, 30, 12}));
    CK_CHECK(w[3]->bounds() == (Rect{0, 12, 45, 12}));
    CK_CHECK(w[4]->bounds() == (Rect{45, 12, 45, 12}));
}

CK_TEST(tile_grid_with_one_window_fills_the_content_area) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* only = desktop.add_window(make_window(f));
    desktop.tile_grid();
    CK_CHECK(only->bounds() == desktop.content_area());
}

CK_TEST(every_tiling_fills_the_content_area_exactly_for_two_to_five_windows) {
    Fixture f;
    // A height and a width that divide unevenly by 3, 4 and 5, so every
    // remainder rule is exercised rather than dodged.
    Desktop desktop(Rect{0, 0, 79, 26});
    for (int n = 1; n <= 5; ++n) {
        desktop.add_window(make_window(f));
        if (n < 2) continue;
        desktop.tile_horizontally();
        CK_CHECK(covers_exactly_once(desktop, desktop.content_area()));
        desktop.tile_vertically();
        CK_CHECK(covers_exactly_once(desktop, desktop.content_area()));
        desktop.tile_grid();
        CK_CHECK(covers_exactly_once(desktop, desktop.content_area()));
    }
}

CK_TEST(the_last_band_absorbs_the_remainder_rather_than_leaving_a_gap_row) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 10});  // 10 / 3 = 3 remainder 1
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    Window* c = desktop.add_window(make_window(f));
    desktop.tile_vertically();
    CK_CHECK(a->bounds().height == 3);
    CK_CHECK(b->bounds().height == 3);
    CK_CHECK(c->bounds().height == 4);  // 3 + the leftover row
    CK_CHECK(c->bounds().y + c->bounds().height == 10);
}

CK_TEST(every_tiling_stays_inside_the_content_area_under_docked_chrome) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24});
    desktop.dock_top(std::make_unique<ckv::ui::View>());
    desktop.dock_bottom(std::make_unique<ckv::ui::View>());
    desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    CK_CHECK(desktop.content_area() == (Rect{0, 1, 60, 22}));

    for (int which = 0; which < 3; ++which) {
        if (which == 0) desktop.tile_horizontally();
        if (which == 1) desktop.tile_vertically();
        if (which == 2) desktop.tile_grid();
        CK_CHECK(covers_exactly_once(desktop, desktop.content_area()));
        CK_CHECK(desktop.windows().front()->bounds().y == 1);  // never under the menu bar
    }
}

CK_TEST(every_tiling_survives_a_window_that_destroys_itself_from_on_resized) {
    Fixture f;
    const auto exercise = [&f](int which) {
        Desktop desktop(Rect{0, 0, 80, 24});
        Window* survivor = desktop.add_window(make_window(f, "Survivor"));
        auto self_removing = std::make_unique<ResizeSelfRemovingWindow>(desktop, "Self removing");
        ResizeSelfRemovingWindow* self_removing_ptr = self_removing.get();
        desktop.add_window(std::move(self_removing));
        self_removing_ptr->arm();

        if (which == 0) desktop.tile_horizontally();
        if (which == 1) desktop.tile_vertically();
        if (which == 2) desktop.tile_grid();

        CK_CHECK(desktop.windows().size() == 1);
        CK_CHECK(desktop.windows().front() == survivor);
    };
    exercise(0);
    exercise(1);
    exercise(2);
}

CK_TEST(every_tiling_is_a_no_op_with_zero_windows) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.tile_horizontally();
    desktop.tile_vertically();
    desktop.tile_grid();
    CK_CHECK(desktop.windows().empty());
}

// --- filled_tile_fractions (U4-b) ----------------------------------------

CK_TEST(filled_tile_fractions_reports_each_band_when_stacked) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    Window* c = desktop.add_window(make_window(f));
    Window* d = desktop.add_window(make_window(f));
    desktop.tile_vertically();

    const auto fractions = desktop.filled_tile_fractions();
    CK_CHECK(fractions.size() == 4);
    // Insertion order, so a caller can line the fractions up against
    // windows() without matching pointers itself.
    CK_CHECK(fractions[0].window == a);
    CK_CHECK(fractions[1].window == b);
    CK_CHECK(fractions[2].window == c);
    CK_CHECK(fractions[3].window == d);
    for (std::size_t i = 0; i < fractions.size(); ++i) {
        CK_CHECK(about_equal(fractions[i].x, 0.0));
        CK_CHECK(about_equal(fractions[i].width, 1.0));
        CK_CHECK(about_equal(fractions[i].y, 0.25 * static_cast<double>(i)));
        CK_CHECK(about_equal(fractions[i].height, 0.25));
    }
}

CK_TEST(filled_tile_fractions_reports_each_band_when_side_by_side) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    desktop.tile_horizontally();

    const auto fractions = desktop.filled_tile_fractions();
    CK_CHECK(fractions.size() == 2);
    CK_CHECK(about_equal(fractions[0].x, 0.0));
    CK_CHECK(about_equal(fractions[0].width, 0.5));
    CK_CHECK(about_equal(fractions[1].x, 0.5));
    CK_CHECK(about_equal(fractions[1].width, 0.5));
    for (const auto& fraction : fractions) {
        CK_CHECK(about_equal(fraction.y, 0.0));
        CK_CHECK(about_equal(fraction.height, 1.0));
    }
}

CK_TEST(filled_tile_fractions_reports_a_grid_including_its_ragged_last_row) {
    // n=3 is the case an exact-cover check is most likely to fail on: the
    // last row holds one window and has to span the whole width for the
    // arrangement to still be filled.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    for (int i = 0; i < 3; ++i) desktop.add_window(make_window(f));
    desktop.tile_grid();

    const auto fractions = desktop.filled_tile_fractions();
    CK_CHECK(fractions.size() == 3);
    CK_CHECK(about_equal(fractions[0].x, 0.0) && about_equal(fractions[0].y, 0.0));
    CK_CHECK(about_equal(fractions[0].width, 0.5) && about_equal(fractions[0].height, 0.5));
    CK_CHECK(about_equal(fractions[1].x, 0.5) && about_equal(fractions[1].y, 0.0));
    CK_CHECK(about_equal(fractions[1].width, 0.5) && about_equal(fractions[1].height, 0.5));
    CK_CHECK(about_equal(fractions[2].x, 0.0) && about_equal(fractions[2].y, 0.5));
    CK_CHECK(about_equal(fractions[2].width, 1.0) && about_equal(fractions[2].height, 0.5));
}

CK_TEST(filled_tile_fractions_reports_a_five_window_grid_with_two_row_widths) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 90, 24});
    for (int i = 0; i < 5; ++i) desktop.add_window(make_window(f));
    desktop.tile_grid();

    const auto fractions = desktop.filled_tile_fractions();
    CK_CHECK(fractions.size() == 5);
    for (std::size_t i = 0; i < 3; ++i) {
        CK_CHECK(about_equal(fractions[i].x, static_cast<double>(i) / 3.0));
        CK_CHECK(about_equal(fractions[i].width, 1.0 / 3.0));
        CK_CHECK(about_equal(fractions[i].y, 0.0));
        CK_CHECK(about_equal(fractions[i].height, 0.5));
    }
    for (std::size_t i = 3; i < 5; ++i) {
        CK_CHECK(about_equal(fractions[i].x, 0.5 * static_cast<double>(i - 3)));
        CK_CHECK(about_equal(fractions[i].width, 0.5));
        CK_CHECK(about_equal(fractions[i].y, 0.5));
        CK_CHECK(about_equal(fractions[i].height, 0.5));
    }
}

CK_TEST(filled_tile_fractions_measures_the_content_area_not_the_whole_desktop) {
    // The fractions a restore lays back down must be of the rect the tiling
    // actually fills; measuring the full bounds would push every window a
    // menu bar's height too far down on a differently sized desktop.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 25});
    desktop.dock_top(std::make_unique<ckv::ui::View>());
    desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    desktop.tile_vertically();
    CK_CHECK(desktop.content_area() == (Rect{0, 1, 80, 24}));

    const auto fractions = desktop.filled_tile_fractions();
    CK_CHECK(fractions.size() == 2);
    CK_CHECK(about_equal(fractions[0].y, 0.0));  // the top band, not 1/25 of the way down
    CK_CHECK(about_equal(fractions[0].height, 0.5));
    CK_CHECK(about_equal(fractions[1].y, 0.5));
}

CK_TEST(filled_tile_fractions_is_empty_once_a_window_is_dragged_off_the_grid) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    desktop.tile_vertically();
    CK_CHECK(!desktop.filled_tile_fractions().empty());

    a->set_bounds(Rect{1, 0, 40, 24});  // dragged one cell right
    CK_CHECK(desktop.filled_tile_fractions().empty());
}

CK_TEST(filled_tile_fractions_is_empty_for_a_one_cell_gap_with_no_overlap_at_all) {
    // The sum condition on its own: nothing overlaps and everything is
    // inside, but a column of desktop shows down the middle.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    desktop.tile_vertically();
    a->set_bounds(Rect{0, 0, 39, 24});
    CK_CHECK(desktop.filled_tile_fractions().empty());
}

CK_TEST(filled_tile_fractions_is_empty_for_windows_that_cover_by_overlapping) {
    // Two maximized windows leave no desktop showing, but they are not a
    // tiling: there is no proportional arrangement here to restore.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    a->set_bounds(desktop.content_area());
    b->set_bounds(desktop.content_area());
    CK_CHECK(desktop.filled_tile_fractions().empty());
}

CK_TEST(filled_tile_fractions_is_empty_with_no_windows_and_after_a_cascade) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    CK_CHECK(desktop.filled_tile_fractions().empty());
    desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    desktop.cascade();
    CK_CHECK(desktop.filled_tile_fractions().empty());
}

CK_TEST(filled_tile_fractions_detects_a_tiling_nobody_used_a_tile_command_for) {
    // Detection is geometric, so a reader who arranged the same grid by hand
    // — or a host that laid it out itself — is reported exactly like one who
    // pressed the command. A flag set by the commands could not do this.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* top_left = desktop.add_window(make_window(f));
    Window* top_right = desktop.add_window(make_window(f));
    Window* bottom = desktop.add_window(make_window(f));
    top_left->set_bounds(Rect{0, 0, 40, 12});
    top_right->set_bounds(Rect{40, 0, 40, 12});
    bottom->set_bounds(Rect{0, 12, 80, 12});

    const auto fractions = desktop.filled_tile_fractions();
    CK_CHECK(fractions.size() == 3);
    CK_CHECK(about_equal(fractions[1].x, 0.5));
    CK_CHECK(about_equal(fractions[2].height, 0.5));
}

CK_TEST(a_fixed_size_dialog_over_a_filled_tiling_is_not_one_of_its_windows) {
    // An About box floats above the arrangement; it has no band, and putting
    // it in the fractions would both break the coverage test and hand a
    // restore a window that never belonged to the layout.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    desktop.tile_vertically();
    Window* dialog = desktop.add_window(make_window(f, "About"));
    dialog->set_resizable(false);
    dialog->set_bounds(Rect{20, 6, 30, 8});

    const auto fractions = desktop.filled_tile_fractions();
    CK_CHECK(fractions.size() == 2);
    for (const auto& fraction : fractions) CK_CHECK(fraction.window != dialog);
}

// --- Maximize-follows-on-open (U4-c) --------------------------------------

CK_TEST(maximize_follows_active_is_off_until_a_host_asks_for_it) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    CK_CHECK(!desktop.maximize_follows_active());

    Window* first = desktop.add_window(make_window(f));
    first->toggle_zoom(desktop.content_area());
    CK_CHECK(first->maximized());

    auto owned = make_window(f, "Second");
    owned->set_bounds(Rect{5, 5, 20, 6});
    Window* second = desktop.add_window(std::move(owned));

    // Every ckVision application written before this existed is one that
    // never asked for it, and none of them may see their placement change.
    CK_CHECK(second->bounds() == (Rect{5, 5, 20, 6}));
    CK_CHECK(!second->zoomed());
}

CK_TEST(an_enabled_desktop_opens_a_new_window_maximized_beside_a_maximized_one) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.set_maximize_follows_active(true);
    Window* first = desktop.add_window(make_window(f));
    first->toggle_zoom(desktop.content_area());

    auto owned = make_window(f, "Second");
    owned->set_bounds(Rect{5, 5, 20, 6});
    Window* second = desktop.add_window(std::move(owned));

    CK_CHECK(second->zoomed());
    CK_CHECK(second->bounds() == desktop.content_area());
    // Zoom, not a second notion of "maximized": the frame's own zoom
    // control restores it to the geometry it was built with.
    second->toggle_zoom(desktop.content_area());
    CK_CHECK(second->bounds() == (Rect{5, 5, 20, 6}));
}

CK_TEST(an_enabled_desktop_leaves_a_new_window_alone_when_the_active_one_is_not_maximized) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.set_maximize_follows_active(true);
    Window* first = desktop.add_window(make_window(f));
    first->set_bounds(Rect{2, 2, 30, 10});

    auto owned = make_window(f, "Second");
    owned->set_bounds(Rect{5, 5, 20, 6});
    Window* second = desktop.add_window(std::move(owned));

    CK_CHECK(!second->zoomed());
    CK_CHECK(second->bounds() == (Rect{5, 5, 20, 6}));
}

CK_TEST(an_enabled_desktops_very_first_window_has_nothing_to_follow) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.set_maximize_follows_active(true);
    auto owned = make_window(f, "First");
    owned->set_bounds(Rect{5, 5, 20, 6});
    Window* first = desktop.add_window(std::move(owned));
    CK_CHECK(!first->zoomed());
    CK_CHECK(first->bounds() == (Rect{5, 5, 20, 6}));
}

CK_TEST(a_permanently_filling_active_window_counts_as_maximized_for_the_follow) {
    // KeepFilling IS a maximized presentation — Window::maximized() says so —
    // and a reader looking at a full-screen window does not know or care
    // which of the two mechanisms is holding it there.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.set_maximize_follows_active(true);
    Window* first = desktop.add_window(make_window(f));
    first->set_grow_policy(ckv::widgets::DesktopGrowPolicy::KeepFilling);
    first->fill(desktop.content_area());

    Window* second = desktop.add_window(make_window(f, "Second"));
    CK_CHECK(second->zoomed());
    CK_CHECK(second->bounds() == desktop.content_area());
}

CK_TEST(a_window_opened_maximized_without_bounds_still_has_somewhere_to_restore_to) {
    // A window zoomed straight from its default empty rect would record 0x0
    // as its restored bounds, and the reader's first click on the zoom
    // control would make it vanish instead of shrinking it.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.set_context(f.ctx());
    desktop.set_maximize_follows_active(true);
    Window* first = desktop.add_window(make_window(f));
    first->toggle_zoom(desktop.content_area());

    Window* second = desktop.add_window(make_window(f, "Second"));  // no bounds set
    CK_CHECK(second->zoomed());
    CK_CHECK(second->bounds() == desktop.content_area());

    second->toggle_zoom(desktop.content_area());
    CK_CHECK(second->bounds().width > 0);
    CK_CHECK(second->bounds().height > 0);
}

CK_TEST(cascade_makes_the_last_window_topmost) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));
    desktop.cascade();
    CK_CHECK(desktop.children().back().get() == b);
}

CK_TEST(cascade_keeps_every_window_within_the_desktop_bounds) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
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
    Desktop desktop(Rect{0, 0, 10, 5});
    desktop.set_context(f.ctx());
    Surface s(ckv::Size{10, 5}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 5});
    desktop.draw(painter);
    CK_CHECK(true);
}

CK_TEST(desktop_resize_survives_a_window_that_destroys_itself_from_on_resized) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.add_window(make_window(f));
    desktop.add_window(make_window(f));
    ckv::ui::View* popup = desktop.add_popup(std::make_unique<ckv::ui::View>());
    CK_CHECK(desktop.children().back().get() == popup);
}

CK_TEST(activating_a_window_while_a_popup_is_open_does_not_bury_the_popup) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
    ckv::ui::View stray;
    CK_CHECK(desktop.remove_popup(&stray) == nullptr);
}

CK_TEST(removing_a_popup_returns_ownership_and_it_no_longer_appears_in_popups) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    ckv::ui::View* popup = desktop.add_popup(std::make_unique<ckv::ui::View>());
    auto owned = desktop.remove_popup(popup);
    CK_CHECK(owned.get() == popup);
    CK_CHECK(desktop.popups().empty());
}

CK_TEST(removing_the_active_window_while_a_popup_is_open_still_activates_the_next_window_not_the_popup) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f));
    Window* b = desktop.add_window(make_window(f));  // b is active
    desktop.add_popup(std::make_unique<ckv::ui::View>());

    desktop.remove_window(b);
    CK_CHECK(desktop.active_window() == a);  // not a crash from mis-casting the popup as a Window
}

CK_TEST(multiple_popups_stack_in_the_order_they_were_added) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.add_window(make_window(f));
    ckv::ui::View* first = desktop.add_popup(std::make_unique<ckv::ui::View>());
    ckv::ui::View* second = desktop.add_popup(std::make_unique<ckv::ui::View>());
    CK_CHECK(desktop.children().back().get() == second);
    (void)first;
}

// --- Window shadow compositing ------------------------------------------

CK_TEST(a_shadow_casting_windows_footprint_is_dimmed_on_the_desktop) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
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
    const ckv::Style background_style = f.theme.resolve(f.roles.desktop_background);
    for (const Rect& fp : footprints) {
        const ckv::Point sample{fp.x, fp.y};
        CK_CHECK(!(surface.at(sample).style() == background_style));
        CK_CHECK(surface.at(sample).style() == ckv::scene::default_dim(background_style));
    }
}

CK_TEST(overlapping_window_shadows_dim_the_desktop_exactly_once) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
    desktop.set_context(f.ctx());
    Window* first = desktop.add_window(make_window(f, "First"));
    first->set_bounds(Rect{2, 2, 5, 3});
    Window* second = desktop.add_window(make_window(f, "Second"));
    second->set_bounds(Rect{3, 2, 5, 3});

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);

    const ckv::Style background_style = f.theme.resolve(f.roles.desktop_background);
    CK_CHECK(surface.at(ckv::Point{8, 3}).style() ==
             ckv::scene::default_dim(background_style));
}

CK_TEST(cells_outside_any_shadow_footprint_are_not_dimmed) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
    desktop.set_context(f.ctx());
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 5, 10, 4});

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);

    const ckv::Style background_style = f.theme.resolve(f.roles.desktop_background);
    // Far corner of the desktop, well outside the window's shadow reach.
    CK_CHECK(surface.at(ckv::Point{0, 0}).style() == background_style);
}

CK_TEST(a_non_shadow_casting_view_leaves_its_footprint_undimmed) {
    // A plain View opts out of View::casts_shadow(). It remains a safe
    // Desktop child and must not acquire a shadow merely by participating in
    // the z-ordered paint pass.
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
    desktop.set_context(f.ctx());
    auto plain = std::make_unique<ckv::ui::View>(Rect{5, 5, 10, 4});
    desktop.add_child(std::move(plain));

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);  // must not crash on a non-Window child

    const ckv::Style background_style = f.theme.resolve(f.roles.desktop_background);
    CK_CHECK(surface.at(ckv::Point{17, 6}).style() == background_style);  // where a shadow WOULD be
}

CK_TEST(a_higher_window_painted_afterward_is_not_dimmed_by_a_lower_windows_shadow) {
    // The whole point of interleaving shadow compositing with the
    // z-order walk rather than a separate post-pass: window B, drawn
    // AFTER window A's shadow is applied, must show ITS OWN content —
    // undimmed — even where A's shadow footprint overlaps B.
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
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
    const ckv::Style background_style = f.theme.resolve(f.roles.desktop_background);
    CK_CHECK(!(surface.at(ckv::Point{12, 3}).style() == ckv::scene::default_dim(background_style)));
}

CK_TEST(a_foreground_window_frame_does_not_merge_with_a_background_window_frame) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
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
    Desktop desktop(Rect{0, 0, 60, 24});
    auto* bar = desktop.dock_top(std::make_unique<ckv::ui::View>());
    CK_CHECK(bar->bounds() == (Rect{0, 0, 60, 1}));
    CK_CHECK(desktop.top_dock() == bar);
}

CK_TEST(dock_bottom_positions_the_view_at_the_last_row_full_width) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24});
    auto* status = desktop.dock_bottom(std::make_unique<ckv::ui::View>());
    CK_CHECK(status->bounds() == (Rect{0, 23, 60, 1}));
    CK_CHECK(desktop.bottom_dock() == status);
}

CK_TEST(content_area_excludes_both_docks) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24});
    desktop.dock_top(std::make_unique<ckv::ui::View>());
    desktop.dock_bottom(std::make_unique<ckv::ui::View>());
    CK_CHECK(desktop.content_area() == (Rect{0, 1, 60, 22}));
}

CK_TEST(content_area_is_the_full_bounds_when_nothing_is_docked) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24});
    CK_CHECK(desktop.content_area() == (Rect{0, 0, 60, 24}));
}

CK_TEST(removing_a_docked_view_clears_the_desktops_content_area_observer) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 60, 24});
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
    Desktop desktop(Rect{0, 0, 60, 24});
    auto* status = desktop.dock_bottom(std::make_unique<MutableHeightView>(1));
    CK_CHECK(status->bounds() == (Rect{0, 23, 60, 1}));

    status->set_height(2);  // e.g. a status line that now wraps to two rows

    CK_CHECK(status->bounds() == (Rect{0, 22, 60, 2}));
}

CK_TEST(a_docked_views_own_height_change_re_clamps_windows_against_the_shrunk_content_area) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* w = desktop.add_window(make_window(f));
    w->set_grow_policy(ckv::widgets::DesktopGrowPolicy::KeepFilling);

    desktop.set_bounds(Rect{0, 0, 120, 50});

    CK_CHECK(!w->zoomed());
    CK_CHECK(w->maximized());
    CK_CHECK(w->bounds() == desktop.content_area());
}

CK_TEST(an_anchor_edges_window_keeps_its_margin_to_the_right_and_bottom_edges_as_the_desktop_grows) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
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
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 5, 20, 10});  // default grow policy: None

    desktop.set_bounds(Rect{0, 0, 200, 60});  // grow substantially

    CK_CHECK(w->bounds() == (Rect{5, 5, 20, 10}));  // unchanged — still fits, so reposition_within is a no-op too
}

CK_TEST(shrinking_the_desktop_pulls_a_window_back_into_the_new_content_area) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{60, 15, 15, 6});  // reachable in the original 80x24 desktop
    desktop.set_bounds(Rect{0, 0, 40, 24});
    CK_CHECK(w->bounds().x + w->bounds().width <= 40);
}

CK_TEST(docking_a_status_line_shrinks_the_content_area_and_clamps_windows_out_of_it) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(Rect{5, 20, 15, 4});  // bottom edge at row 24, the very last row
    desktop.dock_bottom(std::make_unique<ckv::ui::View>());  // now occupies row 23
    CK_CHECK(w->bounds().y + w->bounds().height <= 23);
}

CK_TEST(tile_places_windows_within_the_content_area_not_under_the_docked_chrome) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 60, 24});
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
    Desktop desktop(Rect{0, 0, 60, 24});
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
    Desktop desktop(Rect{0, 0, 40, 20});
    desktop.set_context(f.ctx());
    ckv::ui::View* status = desktop.dock_bottom(std::make_unique<ckv::ui::View>());
    Window* w = desktop.add_window(make_window(f));
    w->set_bounds(desktop.content_area());  // "maximized"
    CK_CHECK(w->bounds().y + w->bounds().height == status->bounds().y);  // flush against the footer

    Surface surface(ckv::Size{40, 20});
    Painter painter(surface, Rect{0, 0, 40, 20});
    desktop.draw(painter);
    desktop.paint_children(painter);

    const ckv::Style background_style = f.theme.resolve(f.roles.desktop_background);
    for (int x = 0; x < 40; ++x)
        CK_CHECK(surface.at(ckv::Point{x, status->bounds().y}).style() == background_style);
}

CK_TEST(a_window_maximized_to_the_content_area_still_shadows_normally_above_the_footer) {
    // The fix must not over-clip: shadow rows that fall WITHIN
    // content_area() (not on the footer itself) still dim normally.
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
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
    const ckv::Style background_style = f.theme.resolve(f.roles.desktop_background);
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
    Desktop desktop(Rect{0, 0, 40, 20});
    desktop.set_context(f.ctx());
    Window* w = desktop.add_window(make_window(f));
    CK_CHECK(desktop.windows().size() == 1);

    auto owned = desktop.detach_child(w);
    CK_CHECK(owned != nullptr);
    CK_CHECK(desktop.windows().empty());
}

CK_TEST(detach_child_on_a_popup_added_via_add_popup_also_removes_it_from_popups) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
    desktop.set_context(f.ctx());
    ckv::ui::View* popup = desktop.add_popup(std::make_unique<ckv::ui::View>());
    CK_CHECK(desktop.popups().size() == 1);

    auto owned = desktop.detach_child(popup);
    CK_CHECK(owned != nullptr);
    CK_CHECK(desktop.popups().empty());
}

CK_TEST(detach_child_on_a_plain_child_falls_back_to_ordinary_removal) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
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
    Desktop desktop(Rect{0, 0, 40, 20});
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
    auto desktop = std::make_unique<Desktop>(Rect{0, 0, 40, 20});
    Window* raw = desktop->add_window(make_window(f));
    raw->set_bounds(Rect{5, 5, 20, 10});
    std::unique_ptr<Window> retained = desktop->remove_window(raw);
    desktop.reset();

    // width-4 is the maximize/restore control's glyph column: absolute 21
    // for this 20-wide window at x=5.
    // The control acts on release, so the click has to be completed.
    CK_CHECK(retained->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                                ckv::Point{21, 5}, std::nullopt, ckv::Modifier::None}));
    CK_CHECK(retained->on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left,
                                                ckv::Point{21, 5}, std::nullopt, ckv::Modifier::None}));
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
        std::make_unique<Desktop>(app.root().bounds());
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
        auto owned = std::make_unique<Desktop>(Rect{0, 0, 80, 24});
        desktop = static_cast<Desktop*>(app.root().add_child(std::move(owned)));
    }
};

// Owns a cursor at its own top-left, which is the whole point: a cursor is
// reported in ABSOLUTE screen cells, so panning the view that owns it is what
// moves the cursor off the screen.
class CursorProbe final : public ui::View {
public:
    CursorProbe() { set_focus_policy(ui::FocusPolicy::TabStop); }

    std::optional<ckv::CursorState> cursor_state() const override {
        const Rect here = absolute_bounds();
        return ckv::CursorState{true, ckv::Point{here.x, here.y}, ckv::CursorShape::Block, true};
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
    CK_CHECK(af.app.commands().has_handler(standard(af.app).close));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).quit));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).zoom));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).next_window));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).previous_window));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).tile));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).tile_horizontally));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).tile_vertically));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).tile_grid));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).cascade));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).window_list));
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
    // The maximize/restore control spans local columns width-5..width-3;
    // width-4 is its glyph. For a 20-wide window at x=10 that is absolute
    // column 26 — pressing outside that span would start a title drag
    // instead of toggling zoom.
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{26, 5},
                                         std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{26, 5},
                                         std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    CK_CHECK(window->zoomed());
    CK_CHECK(window->bounds() == af.desktop->content_area());

    af.term.resize(ckv::Size{100, 30});
    CK_CHECK(af.app.step(0));
    CK_CHECK(window->zoomed());
    CK_CHECK(window->bounds() == af.desktop->content_area());

    const Rect zoomed = window->absolute_bounds();
    const ckv::Point on_control{zoomed.x + window->bounds().width - 4, zoomed.y};
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, on_control,
                                         std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, on_control,
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
    app.commands().set_handler(standard(app).close, [&] { custom_ran = true; });

    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto owned = std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    app.root().add_child(std::move(owned));

    app.commands().execute(standard(app).close);
    CK_CHECK(custom_ran);
}

CK_TEST(a_pre_existing_kquit_handler_is_not_overridden_by_attaching_a_desktop) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    bool custom_ran = false;
    app.commands().set_handler(standard(app).quit, [&] { custom_ran = true; });

    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto owned = std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    app.root().add_child(std::move(owned));

    app.commands().execute(standard(app).quit);
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
    app.commands().set_handler(standard(app).tile, [&] { custom_ran = true; });

    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto owned =
        std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    app.root().add_child(std::move(owned));

    app.commands().execute(standard(app).tile);
    CK_CHECK(custom_ran);
}

CK_TEST(destroying_the_desktop_clears_only_the_default_handlers_it_installed) {
    AttachedFixture af;
    CK_CHECK(af.app.commands().has_handler(standard(af.app).close));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).quit));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).zoom));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).next_window));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).previous_window));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).tile));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).cascade));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).window_list));

    af.app.root().detach_child(af.desktop).reset();  // destroys the Desktop

    CK_CHECK(!af.app.commands().has_handler(standard(af.app).close));
    CK_CHECK(!af.app.commands().has_handler(standard(af.app).quit));
    CK_CHECK(!af.app.commands().has_handler(standard(af.app).zoom));
    CK_CHECK(!af.app.commands().has_handler(standard(af.app).next_window));
    CK_CHECK(!af.app.commands().has_handler(standard(af.app).previous_window));
    CK_CHECK(!af.app.commands().has_handler(standard(af.app).tile));
    CK_CHECK(!af.app.commands().has_handler(standard(af.app).cascade));
    CK_CHECK(!af.app.commands().has_handler(standard(af.app).window_list));
}

CK_TEST(kclose_default_closes_the_active_window) {
    AttachedFixture af;
    Window* w = af.desktop->add_window(make_window(af.f));
    bool closed = false;
    w->on_closed = [&] { closed = true; };

    af.app.commands().execute(standard(af.app).close);
    CK_CHECK(closed);
}

CK_TEST(kclose_default_is_a_no_op_with_no_active_window) {
    AttachedFixture af;
    CK_CHECK(af.app.commands().execute(standard(af.app).close));  // the handler ran, harmlessly
}

CK_TEST(kquit_default_sweeps_every_window_and_requests_quit_when_none_veto) {
    AttachedFixture af;
    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    int closed_count = 0;
    a->on_closed = [&] { ++closed_count; };
    b->on_closed = [&] { ++closed_count; };

    af.app.commands().execute(standard(af.app).quit);
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

    af.app.commands().execute(standard(af.app).quit);
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

    af.app.commands().execute(standard(af.app).quit);
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

    CK_CHECK(af.app.execute_command(standard(af.app).quit));
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
        CK_CHECK(af.app.execute_command(standard(af.app).quit));
    };
    veto->close_request = [&] {
        ++veto_calls;
        return false;
    };
    first->on_closed = [&] { ++first_close_calls; };

    CK_CHECK(af.app.execute_command(standard(af.app).quit));
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

    CK_CHECK(af.app.execute_command(standard(af.app).quit));
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

    af.app.commands().execute(standard(af.app).zoom);
    CK_CHECK(w->zoomed());

    af.app.commands().execute(standard(af.app).zoom);
    CK_CHECK(!w->zoomed());
}

CK_TEST(kzoom_default_is_a_no_op_with_no_active_window) {
    AttachedFixture af;
    CK_CHECK(af.app.commands().execute(standard(af.app).zoom));  // the handler ran, harmlessly
}

CK_TEST(kminimize_default_puts_the_active_window_away) {
    AttachedFixture af;
    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    CK_CHECK(af.desktop->active_window() == b);

    af.app.commands().execute(standard(af.app).minimize);
    CK_CHECK(b->minimized());
    CK_CHECK(!b->visible());
    // Still listed, and the reader is handed the window that is still shown.
    CK_CHECK(af.desktop->windows().size() == 2);
    CK_CHECK(af.desktop->active_window() == a);
}

CK_TEST(kminimize_default_leaves_a_window_that_cannot_be_minimized_alone) {
    AttachedFixture af;
    Window* w = af.desktop->add_window(make_window(af.f));
    w->set_minimizable(false);
    // The command is the menu's and the keyboard's route to the `_` control;
    // where the control is not drawn, neither route may hide the window —
    // an alert or a modal that could be parked leaves an application
    // answering nothing.
    CK_CHECK(af.app.commands().execute(standard(af.app).minimize));
    CK_CHECK(!w->minimized());
    CK_CHECK(af.desktop->active_window() == w);
}

CK_TEST(kminimize_default_is_a_no_op_with_no_active_window) {
    AttachedFixture af;
    CK_CHECK(af.app.commands().execute(standard(af.app).minimize));  // ran, harmlessly
}

CK_TEST(knextwindow_default_activates_the_next_window_in_cycling_order) {
    AttachedFixture af;
    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    CK_CHECK(af.desktop->active_window() == b);  // b, added last, activates on add

    af.app.commands().execute(standard(af.app).next_window);

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

    af.app.commands().execute(standard(af.app).previous_window);

    CK_CHECK(af.desktop->active_window() == b);
}

CK_TEST(ktile_default_arranges_every_window_into_the_content_area) {
    AttachedFixture af;
    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    a->set_bounds(Rect{0, 0, 80, 24});  // both windows start fully overlapping
    b->set_bounds(Rect{0, 0, 80, 24});

    af.app.commands().execute(standard(af.app).tile);

    // tile() gave each its own slice — no longer overlapping.
    CK_CHECK(a->bounds().x != b->bounds().x);
}

CK_TEST(the_three_named_tiling_commands_are_ordinary_desktop_defaults) {
    AttachedFixture af;
    CK_CHECK(af.app.commands().has_handler(standard(af.app).tile_horizontally));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).tile_vertically));
    CK_CHECK(af.app.commands().has_handler(standard(af.app).tile_grid));

    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    Window* c = af.desktop->add_window(make_window(af.f));
    a->set_bounds(Rect{0, 0, 80, 24});  // all three start fully overlapping
    b->set_bounds(Rect{0, 0, 80, 24});
    c->set_bounds(Rect{0, 0, 80, 24});

    // Horizontally = laid out along the horizontal axis, so a row of
    // full-height bands; vertically = stacked down the desktop.
    CK_CHECK(af.app.commands().execute(standard(af.app).tile_horizontally));
    CK_CHECK(a->bounds() == (Rect{0, 0, 26, 24}));
    CK_CHECK(c->bounds() == (Rect{52, 0, 28, 24}));

    CK_CHECK(af.app.commands().execute(standard(af.app).tile_vertically));
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 8}));
    CK_CHECK(c->bounds() == (Rect{0, 16, 80, 8}));

    CK_CHECK(af.app.commands().execute(standard(af.app).tile_grid));
    CK_CHECK(a->bounds() == (Rect{0, 0, 40, 12}));
    CK_CHECK(c->bounds() == (Rect{0, 12, 80, 12}));
}

CK_TEST(a_pre_existing_tile_horizontally_handler_is_not_overridden_by_attaching_a_desktop) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    bool custom_ran = false;
    app.commands().set_handler(standard(app).tile_horizontally, [&] { custom_ran = true; });

    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto owned = std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(app.root().add_child(std::move(owned)));
    Fixture f;
    Window* only = desktop->add_window(make_window(f));
    only->set_bounds(Rect{3, 3, 20, 6});

    app.commands().execute(standard(app).tile_horizontally);
    CK_CHECK(custom_ran);
    CK_CHECK(only->bounds() == (Rect{3, 3, 20, 6}));  // the default never ran
}

CK_TEST(a_presented_dialog_does_not_open_maximized_just_because_the_document_behind_it_is) {
    // Maximize-follows-on-open is about the windows an application opens for
    // the reader to work in. A message box has already been sized to what it
    // has to say, and blowing it up to the whole desktop because the document
    // behind it happened to be maximized is not the policy a host enabled.
    AttachedFixture af;
    af.desktop->set_maximize_follows_active(true);
    Window* document = af.desktop->add_window(make_window(af.f, "Document"));
    document->toggle_zoom(af.desktop->content_area());
    CK_CHECK(document->maximized());

    auto dialog = make_window(af.f, "Message");
    dialog->set_bounds(Rect{10, 5, 30, 8});
    Window* presented =
        af.desktop->present_modeless(WindowHandle{std::move(dialog), nullptr}, af.app);
    CK_CHECK(presented != nullptr);
    CK_CHECK(!presented->zoomed());
    CK_CHECK(presented->bounds() == (Rect{10, 5, 30, 8}));
}

CK_TEST(kcascade_default_arranges_every_window_offset_diagonally) {
    AttachedFixture af;
    Window* a = af.desktop->add_window(make_window(af.f));
    Window* b = af.desktop->add_window(make_window(af.f));
    a->set_bounds(Rect{0, 0, 30, 15});
    b->set_bounds(Rect{0, 0, 30, 15});

    af.app.commands().execute(standard(af.app).cascade);

    CK_CHECK(a->bounds().x != b->bounds().x || a->bounds().y != b->bounds().y);
}

// --- kWindowList default handler ---------------------------------------
//
// The dialog itself is covered in depth by test_window_list_dialog.cpp;
// these prove the command reaches it, which is the part that was missing.
// An unhandled command is still "available", so a menu bound to it looked
// live and did nothing — the failure this wiring exists to end.

CK_TEST(kwindowlist_default_puts_up_a_list_of_this_desktops_windows) {
    AttachedFixture af;
    af.desktop->add_window(make_window(af.f));
    af.desktop->add_window(make_window(af.f));
    const std::size_t before = af.desktop->windows().size();

    CK_CHECK(af.app.commands().execute(standard(af.app).window_list));

    // The list is itself a Window on this Desktop, and it took focus.
    CK_CHECK(af.desktop->windows().size() == before + 1);
    CK_CHECK(af.desktop->active_window() == af.desktop->windows().back());
}

CK_TEST(kwindowlist_default_is_a_no_op_while_one_is_already_open) {
    AttachedFixture af;
    af.desktop->add_window(make_window(af.f));
    af.app.commands().execute(standard(af.app).window_list);
    const std::size_t with_list = af.desktop->windows().size();

    af.app.commands().execute(standard(af.app).window_list);

    CK_CHECK(af.desktop->windows().size() == with_list);  // not a second list
}

CK_TEST(kwindowlist_default_can_be_reopened_after_its_list_closes) {
    AttachedFixture af;
    af.desktop->add_window(make_window(af.f));
    af.app.commands().execute(standard(af.app).window_list);
    Window* const list = af.desktop->windows().back();
    const std::size_t with_list = af.desktop->windows().size();

    list->close();
    af.app.step(af.clock.now_nanos());  // the dialog self-detaches on a post
    CK_CHECK(af.desktop->windows().size() == with_list - 1);

    // The guard cleared with the dialog, rather than latching it shut.
    af.app.commands().execute(standard(af.app).window_list);
    CK_CHECK(af.desktop->windows().size() == with_list);
}

CK_TEST(destroying_a_desktop_with_its_window_list_open_is_safe) {
    // The completion handler outlives the Desktop that presented the dialog:
    // destroying the Desktop detaches the list, which completes the
    // presentation. It must not reach back into the Desktop to say so.
    AttachedFixture af;
    af.desktop->add_window(make_window(af.f));
    af.app.commands().execute(standard(af.app).window_list);

    af.app.root().detach_child(af.desktop).reset();  // destroys it, list and all
    af.app.step(af.clock.now_nanos());
}

CK_TEST(a_pre_existing_kwindowlist_handler_is_not_overridden_by_attaching_a_desktop) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    bool custom_ran = false;
    app.commands().set_handler(standard(app).window_list, [&] { custom_ran = true; });

    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto owned = std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(app.root().add_child(std::move(owned)));

    app.commands().execute(standard(app).window_list);
    CK_CHECK(custom_ran);
    CK_CHECK(desktop->windows().empty());  // the default never presented anything
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

// --- Shadows and tiling ----------------------------------------------------

CK_TEST(tiled_windows_leaving_no_desktop_between_them_stop_casting_shadows) {
    // A shadow says "this floats above that". Tiled windows leave nothing to
    // float over, so the shadow one casts on its neighbour is a dark smudge
    // along a shared edge that eats a row and a column of the window beneath.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.set_context(f.ctx());
    Window* left = desktop.add_window(make_window(f, "L"));
    Window* right = desktop.add_window(make_window(f, "R"));
    left->set_resizable(true);
    right->set_resizable(true);
    const Rect content = desktop.content_area();
    left->set_bounds(Rect{content.x, content.y, content.width / 2, content.height});
    right->set_bounds(Rect{content.x + content.width / 2, content.y,
                           content.width - content.width / 2, content.height});

    CK_CHECK(!desktop.child_casts_shadow(*left));
    CK_CHECK(!desktop.child_casts_shadow(*right));
}

CK_TEST(one_visible_cell_of_desktop_is_enough_to_bring_the_shadows_back) {
    // The windows are still arranged OVER a desktop, so the depth a shadow
    // describes is real. One cell settles it either way.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.set_context(f.ctx());
    Window* left = desktop.add_window(make_window(f, "L"));
    Window* right = desktop.add_window(make_window(f, "R"));
    left->set_resizable(true);
    right->set_resizable(true);
    const Rect content = desktop.content_area();
    left->set_bounds(Rect{content.x, content.y, content.width / 2, content.height});
    right->set_bounds(Rect{content.x + content.width / 2, content.y,
                           content.width - content.width / 2, content.height - 1});

    CK_CHECK(desktop.child_casts_shadow(*left));
    CK_CHECK(desktop.child_casts_shadow(*right));
}

CK_TEST(shadows_go_the_moment_a_tiling_command_fills_the_desktop_and_return_the_moment_one_moves) {
    // The visual half of the same detection the layout query reports: while
    // the arrangement is filled there is no desktop between the windows for
    // a shadow to fall on, and one drag brings both back at once.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.set_context(f.ctx());
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));

    for (int which = 0; which < 3; ++which) {
        if (which == 0) desktop.tile_horizontally();
        if (which == 1) desktop.tile_vertically();
        if (which == 2) desktop.tile_grid();
        CK_CHECK(!desktop.filled_tile_fractions().empty());
        CK_CHECK(!desktop.child_casts_shadow(*a));
        CK_CHECK(!desktop.child_casts_shadow(*b));
        CK_CHECK(!desktop.child_casts_shadow(*c));

        const Rect moved = b->bounds();
        b->set_bounds(Rect{moved.x + 1, moved.y + 1, moved.width, moved.height});
        CK_CHECK(desktop.filled_tile_fractions().empty());
        CK_CHECK(desktop.child_casts_shadow(*a));
        CK_CHECK(desktop.child_casts_shadow(*b));
        CK_CHECK(desktop.child_casts_shadow(*c));
    }
}

CK_TEST(a_fixed_size_window_keeps_its_shadow_over_a_full_tiling) {
    // An About or alert is above the arrangement whatever the arrangement
    // covers, and the shadow is what says so.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.set_context(f.ctx());
    Window* filler = desktop.add_window(make_window(f, "F"));
    filler->set_resizable(true);
    filler->set_bounds(desktop.content_area());
    Window* dialog = desktop.add_window(make_window(f, "About"));
    dialog->set_resizable(false);
    dialog->set_bounds(Rect{10, 5, 30, 8});

    CK_CHECK(!desktop.child_casts_shadow(*filler));
    CK_CHECK(desktop.child_casts_shadow(*dialog));
}

// --- Desktop content: an arrangement rather than floating windows -----

CK_TEST(desktop_content_fills_the_content_area_immediately) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
    desktop.set_context(f.ctx());
    auto* content = desktop.set_content(std::make_unique<ckv::ui::View>());
    CK_CHECK(content != nullptr);
    CK_CHECK(content->bounds() == (Rect{0, 0, 40, 20}));
    CK_CHECK(desktop.content() == content);
}

CK_TEST(desktop_content_keeps_filling_across_a_resize_and_under_chrome) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
    desktop.set_context(f.ctx());
    auto* content = desktop.set_content(std::make_unique<ckv::ui::View>());
    // Chrome takes its rows off the top and bottom; content gets the rest,
    // which is the whole point of not making an application compute this.
    auto chrome = std::make_unique<ckv::ui::View>();
    chrome->set_preferred_size(ckv::Size{0, 1});
    desktop.dock_bottom(std::move(chrome));
    CK_CHECK(content->bounds() == desktop.content_area());
    desktop.set_bounds(Rect{0, 0, 60, 30});
    CK_CHECK(content->bounds() == desktop.content_area());
    CK_CHECK(content->bounds().width == 60);
}

CK_TEST(setting_content_again_hands_back_the_previous_arrangement) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
    desktop.set_context(f.ctx());
    ckv::ui::View* const first = desktop.set_content(std::make_unique<ckv::ui::View>());
    ckv::ui::View* const second = desktop.set_content(std::make_unique<ckv::ui::View>());
    CK_CHECK(first != second);
    CK_CHECK(desktop.content() == second);
    // The old arrangement is gone from the tree, not stacked behind the new
    // one: exactly one content child remains.
    int content_like = 0;
    for (const auto& child : desktop.children())
        if (child.get() == first || child.get() == second) ++content_like;
    CK_CHECK(content_like == 1);
}

CK_TEST(a_window_floats_above_the_desktop_content) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 40, 20});
    desktop.set_context(f.ctx());
    ckv::ui::View* const content = desktop.set_content(std::make_unique<ckv::ui::View>());
    Window* const window = desktop.add_window(make_window(f));
    // Later children paint on top, so content must sit before the window.
    std::size_t content_at = 0;
    std::size_t window_at = 0;
    for (std::size_t i = 0; i < desktop.children().size(); ++i) {
        if (desktop.children()[i].get() == content) content_at = i;
        if (desktop.children()[i].get() == window) window_at = i;
    }
    CK_CHECK(content_at < window_at);
}

// --- How wide a window without bounds of its own opens ---------------------

namespace {

// Content that is a fixed block: a picture, a table, anything whose height
// is what it is whatever width it is given.
class FixedBlock final : public ui::View {
public:
    FixedBlock(int width, int height) : width_(width), height_(height) {}
    ui::SizeHint horizontal_size_hint() const override {
        return ui::SizeHint{width_, width_, ui::kUnboundedExtent};
    }
    ui::SizeHint vertical_size_hint() const override {
        return ui::SizeHint{height_, height_, ui::kUnboundedExtent};
    }
    int height_for_width(int) const override { return height_; }

private:
    int width_;
    int height_;
};

// Content that trades width for height, the way wrapped text does: 200
// cells poured into however many columns it is given.
class Reflowing final : public ui::View {
public:
    explicit Reflowing(int cells) : cells_(cells) {}
    ui::SizeHint horizontal_size_hint() const override {
        return ui::SizeHint{1, 20, ui::kUnboundedExtent};
    }
    ui::SizeHint vertical_size_hint() const override {
        return ui::SizeHint{1, 1, ui::kUnboundedExtent};
    }
    int height_for_width(int width) const override {
        return width <= 0 ? cells_ : (cells_ + width - 1) / width;
    }

private:
    int cells_;
};

// A desktop of a stated size, with an application under it, since presenting
// is what places a window that has no bounds of its own.
struct PresentingFixture {
    explicit PresentingFixture(ckv::Size size) : term(size) {
        app.theme() = make_classic_theme(app.roles(), roles);
        desktop = static_cast<Desktop*>(app.root().add_child(
            std::make_unique<Desktop>(Rect{0, 0, size.width, size.height})));
    }

    ckv::term::HeadlessTerminal term;
    ManualClock clock;
    Application app{term, clock};
    Fixture f;
    StandardRoles roles = intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;

    Window* present(std::unique_ptr<ui::View> content) {
        auto window = make_window(f);
        window->set_content(std::move(content));
        return desktop->present_modeless(WindowHandle{std::move(window), nullptr}, app);
    }
};

}  // namespace

CK_TEST(a_presented_window_opens_at_the_width_its_content_asks_for) {
    PresentingFixture fixture(ckv::Size{80, 24});
    Window* placed = fixture.present(std::make_unique<FixedBlock>(30, 5));
    CK_CHECK(placed != nullptr);
    if (placed == nullptr) return;
    CK_CHECK(placed->bounds().width == 30 + 2);  // its content, and the frame
    CK_CHECK(placed->bounds().x > 0);            // centred in what is left
}

CK_TEST(a_window_too_tall_for_the_desktop_widens_until_its_content_fits) {
    // Reflowing content answers a shorter height at a wider width, so there
    // is a width at which the window fits the desktop; it opens at the
    // narrowest such width, not at the widest available.
    PresentingFixture fixture(ckv::Size{80, 10});
    Window* placed = fixture.present(std::make_unique<Reflowing>(200));
    CK_CHECK(placed != nullptr);
    if (placed == nullptr) return;
    CK_CHECK(placed->bounds().height <= 10);
    CK_CHECK(placed->bounds().width > 20 + 2);  // wider than the columns it asked for
    CK_CHECK(placed->bounds().width < 80);      // and not simply the whole desktop
    CK_CHECK(placed->height_for_width(placed->bounds().width) <= 10);
    CK_CHECK(placed->height_for_width(placed->bounds().width - 1) > 10);
}

CK_TEST(a_window_that_widening_cannot_shorten_opens_at_its_own_width_anyway) {
    // The width a window asks for is not a bid to be talked out of: content
    // that is as tall at eighty columns as at thirty gains nothing from the
    // extra fifty, so it does not get them. It overflows at the width it
    // wanted, and the overflow is the desktop's to clamp.
    PresentingFixture fixture(ckv::Size{80, 10});
    Window* placed = fixture.present(std::make_unique<FixedBlock>(30, 20));
    CK_CHECK(placed != nullptr);
    if (placed == nullptr) return;
    CK_CHECK(placed->bounds().width == 30 + 2);
    CK_CHECK(placed->bounds().height == 10);
}

// --- Minimize (U4-i) ------------------------------------------------------
//
// A hidden window is still one of this desktop's windows — that is how a
// reader gets it back — while everything that arranges or cycles windows
// steps over it. Every check below is on the numbers: which rectangle each
// remaining window got, and how many of them the area was divided by.

namespace {

// Where `window` sits in z-order among the desktop's children: 0 is the
// bottom of the stack. Minimizing must not move a window in it, which is a
// claim about a number, not about a rectangle.
int z_index(const Desktop& desktop, const Window* window) {
    int index = 0;
    for (const auto& child : desktop.children()) {
        if (child.get() == static_cast<const ui::View*>(window)) return index;
        ++index;
    }
    return -1;
}

}  // namespace

CK_TEST(minimizing_a_window_hides_it_and_leaves_it_listed) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    a->set_minimized(true);
    CK_CHECK(a->minimized());
    CK_CHECK(!a->visible());
    CK_CHECK(b->visible());
    // Still listed, still in insertion order: a switcher bar has something
    // to draw, and something to click.
    CK_CHECK(desktop.windows().size() == 2);
    CK_CHECK(desktop.windows()[0] == a);
    CK_CHECK(desktop.windows()[1] == b);
}

CK_TEST(a_minimized_window_gets_no_band_and_the_rest_divide_the_desktop_exactly) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 90, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    b->set_bounds(Rect{5, 5, 20, 8});
    b->set_minimized(true);
    desktop.tile();
    // Two windows, not three: 90 / 2, not 90 / 3 with a 30-column gap where
    // the hidden window's band would have been.
    CK_CHECK(a->bounds() == (Rect{0, 0, 45, 24}));
    CK_CHECK(c->bounds() == (Rect{45, 0, 45, 24}));
    CK_CHECK(a->bounds().width + c->bounds().width == 90);
    CK_CHECK(b->bounds() == (Rect{5, 5, 20, 8}));  // untouched by the arrangement
}

CK_TEST(tile_vertically_divides_by_the_shown_windows_only) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    c->set_minimized(true);
    desktop.tile_vertically();
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 12}));
    CK_CHECK(b->bounds() == (Rect{0, 12, 80, 12}));
    CK_CHECK(a->bounds().height + b->bounds().height == 24);
}

CK_TEST(tile_horizontally_divides_by_the_shown_windows_only) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    a->set_minimized(true);
    desktop.tile_horizontally();
    CK_CHECK(b->bounds() == (Rect{0, 0, 40, 24}));
    CK_CHECK(c->bounds() == (Rect{40, 0, 40, 24}));
}

CK_TEST(tile_grid_takes_its_shape_from_the_shown_windows) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    Window* d = desktop.add_window(make_window(f, "D"));
    Window* e = desktop.add_window(make_window(f, "E"));
    c->set_bounds(Rect{1, 1, 12, 6});
    c->set_minimized(true);
    desktop.tile_grid();
    // Four shown windows: two columns and two full rows, not five windows'
    // three columns with a hole in the last row.
    CK_CHECK(a->bounds() == (Rect{0, 0, 40, 12}));
    CK_CHECK(b->bounds() == (Rect{40, 0, 40, 12}));
    CK_CHECK(d->bounds() == (Rect{0, 12, 40, 12}));
    CK_CHECK(e->bounds() == (Rect{40, 12, 40, 12}));
    CK_CHECK(c->bounds() == (Rect{1, 1, 12, 6}));
}

CK_TEST(cascade_gives_a_minimized_window_no_step_in_the_diagonal) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    b->set_bounds(Rect{7, 3, 20, 8});
    b->set_minimized(true);
    desktop.cascade();
    // The second SHOWN window takes the second step, so the run of title
    // bars has no gap in it: two cells right and one down per step.
    CK_CHECK(a->bounds() == (Rect{0, 0, 53, 16}));
    CK_CHECK(c->bounds() == (Rect{2, 1, 53, 16}));
    CK_CHECK(b->bounds() == (Rect{7, 3, 20, 8}));
}

CK_TEST(the_window_cycle_steps_over_a_minimized_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    Window* d = desktop.add_window(make_window(f, "D"));
    c->set_minimized(true);
    desktop.activate(a);
    desktop.activate_next();
    CK_CHECK(desktop.active_window() == b);
    desktop.activate_next();  // over c
    CK_CHECK(desktop.active_window() == d);
    CK_CHECK(c->minimized());  // stepping over is not restoring
    desktop.activate_next();
    CK_CHECK(desktop.active_window() == a);
    desktop.activate_previous();  // over c again, the other way
    CK_CHECK(desktop.active_window() == d);
    desktop.activate_previous();
    CK_CHECK(desktop.active_window() == b);
}

CK_TEST(cycling_where_every_other_window_is_minimized_leaves_activation_alone) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    b->set_minimized(true);
    c->set_minimized(true);
    CK_CHECK(desktop.active_window() == a);
    desktop.activate_next();
    CK_CHECK(desktop.active_window() == a);
    desktop.activate_previous();
    CK_CHECK(desktop.active_window() == a);
    CK_CHECK(b->minimized() && c->minimized());
}

CK_TEST(filled_tile_fractions_does_not_count_a_minimized_window) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    c->set_bounds(Rect{3, 3, 20, 8});
    c->set_minimized(true);
    desktop.tile();

    const auto fractions = desktop.filled_tile_fractions();
    CK_CHECK(fractions.size() == 2);  // not three, and not empty either
    CK_CHECK(fractions[0].window == a);
    CK_CHECK(fractions[1].window == b);
    CK_CHECK(about_equal(fractions[0].x, 0.0));
    CK_CHECK(about_equal(fractions[0].width, 0.5));
    CK_CHECK(about_equal(fractions[1].x, 0.5));
    CK_CHECK(about_equal(fractions[1].width, 0.5));

    // And minimizing one of a filled pair uncovers half the desktop, which
    // is a gap like any other: the arrangement stops being a tiling.
    b->set_minimized(true);
    CK_CHECK(desktop.filled_tile_fractions().empty());
}

CK_TEST(minimizing_the_active_window_hands_activation_to_the_topmost_shown_one) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    CK_CHECK(desktop.active_window() == c);
    c->set_minimized(true);
    CK_CHECK(desktop.active_window() == b);
    CK_CHECK(b->active());
    CK_CHECK(!c->active());
    CK_CHECK(c->minimized());
    // The successor rose above it, which is activation's own doing and not
    // a stack slot the hiding forgot: b was raised, c stayed where it was.
    CK_CHECK(z_index(desktop, a) == 0);
    CK_CHECK(z_index(desktop, c) == 1);
    CK_CHECK(z_index(desktop, b) == 2);
    // Restoring leaves it there rather than bringing it forward: coming back
    // is not the same request as being asked for.
    c->set_minimized(false);
    CK_CHECK(z_index(desktop, c) == 1);
    CK_CHECK(desktop.active_window() == b);
}

CK_TEST(closing_the_active_window_skips_a_minimized_successor) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    b->set_minimized(true);
    CK_CHECK(desktop.active_window() == c);
    desktop.remove_window(c);
    // b is topmost of what remains and hidden; the successor is a.
    CK_CHECK(desktop.active_window() == a);
    CK_CHECK(a->active());
    CK_CHECK(b->minimized());
}

CK_TEST(a_desktop_with_every_window_minimized_is_still_a_working_desktop) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    a->set_bounds(Rect{1, 1, 20, 8});
    b->set_bounds(Rect{30, 2, 20, 8});
    b->set_minimized(true);
    a->set_minimized(true);

    // No activation pointing at something hidden — none at all, which is
    // the honest answer when there is nothing on the desktop to point at.
    CK_CHECK(desktop.active_window() == nullptr);
    CK_CHECK(!a->active());
    CK_CHECK(!b->active());
    CK_CHECK(desktop.windows().size() == 2);

    desktop.tile();
    desktop.tile_grid();
    desktop.cascade();
    desktop.activate_next();
    desktop.activate_previous();
    CK_CHECK(a->bounds() == (Rect{1, 1, 20, 8}));
    CK_CHECK(b->bounds() == (Rect{30, 2, 20, 8}));
    CK_CHECK(desktop.filled_tile_fractions().empty());
    CK_CHECK(desktop.active_window() == nullptr);
}

CK_TEST(naming_a_minimized_window_by_number_brings_it_back_and_activates_it) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    a->set_minimized(true);
    CK_CHECK(desktop.active_window() == b);
    // The numbering counts every listed window, hidden ones included, so
    // Alt+1 still means the entry a reader is looking at.
    desktop.select_by_number(1);
    CK_CHECK(!a->minimized());
    CK_CHECK(a->visible());
    CK_CHECK(desktop.active_window() == a);
    CK_CHECK(z_index(desktop, a) == 1);  // activated, so raised
}

CK_TEST(restoring_a_window_returns_its_bounds_z_order_and_activation_unchanged) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    b->set_bounds(Rect{12, 6, 30, 9});
    CK_CHECK(z_index(desktop, b) == 1);

    b->set_minimized(true);  // an inactive window: nothing else moves
    CK_CHECK(desktop.active_window() == c);
    CK_CHECK(z_index(desktop, b) == 1);
    b->set_minimized(false);
    CK_CHECK(b->bounds() == (Rect{12, 6, 30, 9}));
    CK_CHECK(z_index(desktop, b) == 1);
    CK_CHECK(z_index(desktop, a) == 0);
    CK_CHECK(z_index(desktop, c) == 2);
    CK_CHECK(desktop.active_window() == c);  // restoring is not activating
}

CK_TEST(a_window_minimized_while_maximized_comes_back_maximized) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    a->set_bounds(Rect{10, 4, 30, 10});
    a->toggle_zoom(desktop.content_area());
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 24}));

    a->set_minimized(true);
    a->set_minimized(false);
    CK_CHECK(a->zoomed());
    CK_CHECK(a->maximized());
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 24}));
    // And the geometry underneath it survived too: un-zooming still finds
    // the rectangle the window had before it was ever maximized.
    a->toggle_zoom(desktop.content_area());
    CK_CHECK(a->bounds() == (Rect{10, 4, 30, 10}));
}

CK_TEST(a_hidden_maximized_window_keeps_filling_a_desktop_that_resized_while_it_was_away) {
    // Hiding withholds a window's presentation, not its owner's sizing
    // policy: a frozen rectangle is how a window comes back off the edge of
    // a desktop that shrank while it was parked.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    b->set_bounds(Rect{60, 18, 18, 6});
    a->toggle_zoom(desktop.content_area());
    a->set_minimized(true);
    b->set_minimized(true);

    desktop.set_bounds(Rect{0, 0, 50, 16});
    a->set_minimized(false);
    b->set_minimized(false);
    CK_CHECK(a->bounds() == (Rect{0, 0, 50, 16}));   // still filling, at the new size
    CK_CHECK(b->bounds().x + b->bounds().width <= 50);
    CK_CHECK(b->bounds().y + b->bounds().height <= 16);
}

CK_TEST(the_desktop_reports_minimizing_and_restoring_to_whoever_lists_windows) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    std::vector<Desktop::WindowChange> heard;
    std::vector<Window*> subjects;
    desktop.subscribe_window_change([&](Desktop::WindowChange change, Window& window) {
        heard.push_back(change);
        subjects.push_back(&window);
    });

    a->set_minimized(true);  // inactive: nothing else changes
    CK_CHECK(heard.size() == 1);
    CK_CHECK(heard[0] == Desktop::WindowChange::Minimized);
    CK_CHECK(subjects[0] == a);

    heard.clear();
    subjects.clear();
    a->set_minimized(false);
    CK_CHECK(heard.size() == 1);
    CK_CHECK(heard[0] == Desktop::WindowChange::Restored);
    CK_CHECK(subjects[0] == a);

    // The active one: the successor's Activated arrives first, so an
    // observer reading active_window() from the Minimized reads the answer.
    heard.clear();
    subjects.clear();
    b->set_minimized(true);
    CK_CHECK(heard.size() == 2);
    CK_CHECK(heard[0] == Desktop::WindowChange::Activated);
    CK_CHECK(subjects[0] == a);
    CK_CHECK(heard[1] == Desktop::WindowChange::Minimized);
    CK_CHECK(subjects[1] == b);

    // Asked for by name: back on the desktop first, in front second.
    heard.clear();
    subjects.clear();
    desktop.activate(b);
    CK_CHECK(heard.size() == 2);
    CK_CHECK(heard[0] == Desktop::WindowChange::Restored);
    CK_CHECK(heard[1] == Desktop::WindowChange::Activated);
    CK_CHECK(subjects[0] == b);
    CK_CHECK(subjects[1] == b);
}

CK_TEST(snapshot_and_restore_carry_the_minimized_state) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    a->set_bounds(Rect{2, 2, 24, 9});
    b->set_bounds(Rect{30, 5, 24, 9});
    b->set_minimized(true);
    const Desktop::Snapshot parked = desktop.snapshot();

    b->set_minimized(false);
    b->set_bounds(Rect{40, 10, 20, 6});
    desktop.restore(parked);
    CK_CHECK(b->minimized());
    CK_CHECK(!b->visible());
    CK_CHECK(b->bounds() == (Rect{30, 5, 24, 9}));
    CK_CHECK(a->bounds() == (Rect{2, 2, 24, 9}));
    CK_CHECK(desktop.active_window() == a);
}

CK_TEST(a_window_added_already_minimized_neither_shows_nor_takes_activation) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    auto parked = make_window(f, "B");
    parked->set_minimized(true);
    Window* b = desktop.add_window(std::move(parked));
    CK_CHECK(desktop.active_window() == a);
    CK_CHECK(a->active());
    CK_CHECK(b->minimized());
    CK_CHECK(desktop.windows().size() == 2);  // listed all the same
}

CK_TEST(a_modal_window_has_no_minimize_control_to_hide_itself_with) {
    AttachedFixture af;
    Window* plain = af.desktop->add_window(make_window(af.f, "Plain"));
    CK_CHECK(plain->minimizable());  // an ordinary window keeps it

    auto owned = make_window(af.f, "Modal");
    owned->set_bounds(Rect{10, 4, 40, 10});
    Window* modal = owned.get();
    af.desktop->present_modal(WindowHandle{std::move(owned), nullptr}, af.app);
    // Hiding the one window that is accepting input would leave an
    // application answering nothing, with the way back on a bar the modal
    // scope will not let the reader click.
    CK_CHECK(!modal->minimizable());
    af.desktop->remove_window(modal);
}

CK_TEST(clicking_the_minimize_control_hides_the_window_it_is_on) {
    // End to end: HeadlessTerminal input enters Application::step(), hit
    // testing selects the Window, and the control acts on release.
    AttachedFixture af;
    auto owned = make_window(af.f, "Park");
    owned->set_bounds(Rect{10, 5, 24, 10});
    Window* window = af.desktop->add_window(std::move(owned));
    af.app.step(0);

    // The minimize control spans local columns width-8..width-6, its glyph
    // at width-7. For a 24-wide window at x=10 that is absolute column 27 —
    // three cells left of the maximize control's own glyph at 30.
    const ckv::Point on_control{27, 5};
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, on_control,
                                         std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    CK_CHECK(!window->minimized());  // a press is a question; the release answers it
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, on_control,
                                         std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    CK_CHECK(window->minimized());
    CK_CHECK(!window->visible());
    CK_CHECK(window->bounds() == (Rect{10, 5, 24, 10}));  // nothing moved
    CK_CHECK(af.desktop->windows().size() == 1);
}

CK_TEST(a_window_too_narrow_for_a_third_control_does_not_answer_where_it_would_be) {
    // Twenty-one columns is one short of leaving the window four cells of
    // its own name beside three controls, so the control is neither drawn
    // nor hit-tested there: pressing where it would have been is title bar,
    // and such a window keeps exactly the frame it had before.
    AttachedFixture af;
    auto owned = make_window(af.f, "Narrow");
    owned->set_bounds(Rect{10, 5, 21, 10});
    Window* window = af.desktop->add_window(std::move(owned));
    af.app.step(0);

    const ckv::Point where_it_would_be{10 + 14, 5};  // width-7 for a 21-wide window
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                         where_it_would_be, std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left,
                                         where_it_would_be, std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    CK_CHECK(!window->minimized());
    CK_CHECK(window->bounds() == (Rect{10, 5, 21, 10}));

    // One column wider, and the control is there.
    window->set_bounds(Rect{10, 5, 22, 10});
    // Pumped, not asserted: step() reports whether an event, timer or posted
    // work ran, and a bare set_bounds is none of those however much it
    // repaints.
    af.app.step(0);
    const ckv::Point on_control{10 + 15, 5};  // width-7 for a 22-wide window
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, on_control,
                                         std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    af.term.inject_event(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, on_control,
                                         std::nullopt, ckv::Modifier::None});
    CK_CHECK(af.app.step(0));
    CK_CHECK(window->minimized());
}

CK_TEST(the_minimize_control_appears_exactly_where_it_still_leaves_the_window_its_name) {
    // The width gate and the title budget are ONE number, checked here
    // without a picture: the control appears at the first width whose
    // budget still leaves four columns of title, so there is no band where
    // the frame says "room for a third control" while the caption says
    // "no room for me at all". Twenty-one columns is one short of that, and
    // such a window keeps precisely the frame it had before this existed.
    Fixture f;
    const auto title_row = [&f](std::string title, int width, bool resizable, bool minimizable) {
        auto window = std::make_unique<Window>(std::move(title));
        window->set_context(f.ctx());
        window->set_resizable(resizable);
        window->set_minimizable(minimizable);
        window->set_bounds(Rect{0, 0, width, 5});
        Surface surface(ckv::Size{width, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
        Painter painter(surface, Rect{0, 0, width, 5});
        window->draw(painter);
        std::vector<std::string> row;
        row.reserve(static_cast<std::size_t>(width));
        for (int x = 0; x < width; ++x) row.emplace_back(surface.at(ckv::Point{x, 0}).grapheme());
        return row;
    };
    const auto carries_a_minimize_control = [](const std::vector<std::string>& row) {
        for (const std::string& cell : row)
            if (cell == "_") return true;
        return false;
    };

    // 21 columns: no third control, the maximize control where it has always
    // been (width-5..width-3), and the old title budget — 21 - 10 = 11
    // columns, so "Back" centres at (21 - 4) / 2 = 8.
    const std::vector<std::string> narrow = title_row("Back", 21, true, true);
    CK_CHECK(!carries_a_minimize_control(narrow));
    CK_CHECK(narrow[16] == "[");
    CK_CHECK(narrow[17] == "\xE2\x86\x91");  // U+2191 UPWARDS ARROW
    CK_CHECK(narrow[18] == "]");
    CK_CHECK(narrow[7] == " ");
    CK_CHECK(narrow[8] == "B");
    CK_CHECK(narrow[11] == "k");
    CK_CHECK(narrow[12] == " ");

    // 22 columns: the control at width-8..width-6, abutting the maximize
    // control, and the title still four columns wide — 22 - 2 * 9 — centred
    // at (22 - 4) / 2 = 9.
    const std::vector<std::string> gate = title_row("Back", 22, true, true);
    CK_CHECK(gate[14] == "[");
    CK_CHECK(gate[15] == "_");
    CK_CHECK(gate[16] == "]");
    CK_CHECK(gate[17] == "[");
    CK_CHECK(gate[18] == "\xE2\x86\x91");
    CK_CHECK(gate[19] == "]");
    CK_CHECK(gate[8] == " ");
    CK_CHECK(gate[9] == "B");
    CK_CHECK(gate[12] == "k");
    CK_CHECK(gate[13] == " ");

    // A title that fills its whole budget stops one cell short of the
    // control, padding included: nine cells a side, not eight, is what
    // keeps that trailing space off the opening bracket.
    const std::vector<std::string> elided = title_row("Workspace Manager", 22, true, true);
    CK_CHECK(elided[9] == "W");
    CK_CHECK(elided[13] == " ");
    CK_CHECK(elided[14] == "[");
    CK_CHECK(elided[15] == "_");
    CK_CHECK(elided[16] == "]");

    // A wide window keeps more of its name than the gate leaves: 40 - 18.
    const std::vector<std::string> wide = title_row("Back", 40, true, true);
    CK_CHECK(wide[32] == "[");
    CK_CHECK(wide[33] == "_");
    CK_CHECK(wide[34] == "]");
    CK_CHECK(wide[17] == " ");
    CK_CHECK(wide[18] == "B");
    CK_CHECK(wide[21] == "k");

    // Neither a fixed-size window nor one told not to offer it draws the
    // control at any width — and turning it off restores the two-control
    // budget, so the title comes back to (40 - 4) / 2 = 18 either way.
    const std::vector<std::string> fixed = title_row("Back", 40, false, true);
    CK_CHECK(!carries_a_minimize_control(fixed));
    const std::vector<std::string> opted_out = title_row("Back", 40, true, false);
    CK_CHECK(!carries_a_minimize_control(opted_out));
    CK_CHECK(opted_out[35] == "[");
    CK_CHECK(opted_out[36] == "\xE2\x86\x91");
    CK_CHECK(opted_out[37] == "]");
    CK_CHECK(opted_out[18] == "B");
}

// --- A stated arrangement across a resize (U4-n) --------------------------
//
// Every number below is a cell count on a stated desktop size, because the
// whole claim is arithmetic: the proportions of an arrangement are preserved
// exactly, a round trip returns the very rectangles it started from, and the
// one case that cannot be honoured is declined rather than approximated.

namespace {

// A reader's own move or resize, driven the way a reader drives one. The
// keyboard move/resize modes bracket themselves with the same gesture a mouse
// drag does, and that bracket is what tells a Desktop the bounds change came
// from the reader and not from a host's own layout code. Tests that want the
// host's kind of change call set_bounds directly, as everything above does.
void reader_moves(Window& window, ckv::Key direction, int times) {
    window.enter_move_mode();
    for (int i = 0; i < times; ++i)
        window.on_key(ckv::KeyEvent{ckv::KeyChord{direction, ckv::Modifier::None, ""}});
    window.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
}

void reader_resizes(Window& window, ckv::Key direction, int times) {
    window.enter_resize_mode();
    for (int i = 0; i < times; ++i)
        window.on_key(ckv::KeyEvent{ckv::KeyChord{direction, ckv::Modifier::None, ""}});
    window.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
}

}  // namespace

CK_TEST(a_stacked_tiling_survives_a_one_row_resize_round_trip) {
    // The defect this mechanism exists for, in its smallest form. One row off
    // the terminal used to pull B up WITHOUT shortening it — leaving A and B
    // both claiming row 11 — and growing back never repaired that, because an
    // independent per-window clamp can only ever shrink.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 12}));
    CK_CHECK(b->bounds() == (Rect{0, 12, 80, 12}));

    desktop.set_bounds(Rect{0, 0, 80, 23});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 12}));   // the seam stays where it is
    CK_CHECK(b->bounds() == (Rect{0, 12, 80, 11}));  // and the row comes off B
    CK_CHECK(covers_exactly_once(desktop, desktop.content_area()));
    CK_CHECK(desktop.filled_tile_fractions().size() == 2);

    desktop.set_bounds(Rect{0, 0, 80, 24});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 12}));  // exactly the rectangles it began with
    CK_CHECK(b->bounds() == (Rect{0, 12, 80, 12}));
    CK_CHECK(desktop.filled_tile_fractions().size() == 2);
}

CK_TEST(a_side_by_side_tiling_survives_a_width_round_trip) {
    // 80 / 3 = 26 with the last band absorbing the remainder, so the seams sit
    // at 26 and 52. At 60 columns those map to 20 and 39 — the middle band is
    // a column narrower than its neighbours, which is what rounding to nearest
    // costs and what returning to 80 gives straight back.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    desktop.tile_horizontally();
    CK_CHECK(a->bounds() == (Rect{0, 0, 26, 24}));
    CK_CHECK(b->bounds() == (Rect{26, 0, 26, 24}));
    CK_CHECK(c->bounds() == (Rect{52, 0, 28, 24}));

    desktop.set_bounds(Rect{0, 0, 60, 24});
    CK_CHECK(a->bounds() == (Rect{0, 0, 20, 24}));
    CK_CHECK(b->bounds() == (Rect{20, 0, 19, 24}));
    CK_CHECK(c->bounds() == (Rect{39, 0, 21, 24}));
    CK_CHECK(covers_exactly_once(desktop, desktop.content_area()));

    desktop.set_bounds(Rect{0, 0, 80, 24});
    CK_CHECK(a->bounds() == (Rect{0, 0, 26, 24}));
    CK_CHECK(b->bounds() == (Rect{26, 0, 26, 24}));
    CK_CHECK(c->bounds() == (Rect{52, 0, 28, 24}));
}

CK_TEST(a_grid_survives_a_resize_round_trip_on_both_axes) {
    // The ragged last row is the interesting cell: it spans the whole width,
    // so the mapping has to give it back the whole width and not two thirds of
    // it plus a rounding error.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    desktop.tile_grid();
    CK_CHECK(a->bounds() == (Rect{0, 0, 40, 12}));
    CK_CHECK(b->bounds() == (Rect{40, 0, 40, 12}));
    CK_CHECK(c->bounds() == (Rect{0, 12, 80, 12}));

    desktop.set_bounds(Rect{0, 0, 60, 18});
    CK_CHECK(a->bounds() == (Rect{0, 0, 30, 9}));
    CK_CHECK(b->bounds() == (Rect{30, 0, 30, 9}));
    CK_CHECK(c->bounds() == (Rect{0, 9, 60, 9}));
    CK_CHECK(covers_exactly_once(desktop, desktop.content_area()));

    desktop.set_bounds(Rect{0, 0, 80, 24});
    CK_CHECK(a->bounds() == (Rect{0, 0, 40, 12}));
    CK_CHECK(b->bounds() == (Rect{40, 0, 40, 12}));
    CK_CHECK(c->bounds() == (Rect{0, 12, 80, 12}));
}

CK_TEST(a_tiling_is_re_divided_by_its_proportions_when_the_desktop_grows) {
    // The positive partner of every "is not snapped back" test below: with no
    // remembered arrangement a None-policy window is never grown at all, so
    // these two windows would still be 12 rows tall in a 48-row desktop with
    // 24 rows of bare desktop under them. 24 and 24 is a number only the
    // arrangement can produce.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 24}));
    CK_CHECK(b->bounds() == (Rect{0, 24, 80, 24}));
    CK_CHECK(desktop.filled_tile_fractions().size() == 2);
}

CK_TEST(repeated_resize_round_trips_do_not_walk_a_seam) {
    // Rounding to nearest is not an involution: re-deriving the arrangement
    // from each intermediate size would move a seam a row per trip and never
    // move it back. Mapping from the ORIGINAL every time is what makes three
    // round trips indistinguishable from none.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    desktop.tile_vertically();
    CK_CHECK(b->bounds() == (Rect{0, 8, 80, 8}));

    for (int trip = 0; trip < 3; ++trip) {
        desktop.set_bounds(Rect{0, 0, 80, 23});
        desktop.set_bounds(Rect{0, 0, 80, 24});
    }
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 8}));
    CK_CHECK(b->bounds() == (Rect{0, 8, 80, 8}));
    CK_CHECK(c->bounds() == (Rect{0, 16, 80, 8}));

    // And the shrunk shape is the same one every trip produced, not a drifted
    // neighbour of it: 24 -> 23 takes the row off the middle band.
    desktop.set_bounds(Rect{0, 0, 80, 23});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 8}));
    CK_CHECK(b->bounds() == (Rect{0, 8, 80, 7}));
    CK_CHECK(c->bounds() == (Rect{0, 15, 80, 8}));
}

CK_TEST(a_readers_own_move_is_never_snapped_back_into_the_arrangement) {
    // Validation is against where this Desktop last PUT each window, not
    // against the cell a tile command once stated: a reader who moves a window
    // has said it is not in the arrangement any more, and a resize that put it
    // back would be overruling them.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();

    reader_moves(*a, ckv::Key::Down, 1);
    CK_CHECK(a->bounds() == (Rect{0, 1, 80, 12}));
    CK_CHECK(desktop.filled_tile_fractions().empty());

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(a->bounds() == (Rect{0, 1, 80, 12}));   // left exactly where the reader left it
    CK_CHECK(b->bounds() == (Rect{0, 12, 80, 12}));  // and its neighbour is not re-divided either
    CK_CHECK(desktop.filled_tile_fractions().empty());
}

CK_TEST(a_hand_made_tiling_is_remembered_when_the_readers_gesture_ends) {
    // Detection is geometric everywhere else in this class, and so is this: a
    // reader who builds the arrangement by hand gets exactly what a reader who
    // pressed Tile gets. Three gestures — grow A, move B down, shorten B — of
    // which only the last leaves a partition.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();

    reader_resizes(*a, ckv::Key::Down, 2);
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 14}));  // now overlapping B
    reader_moves(*b, ckv::Key::Down, 2);
    CK_CHECK(b->bounds() == (Rect{0, 14, 80, 12}));  // now off the bottom
    reader_resizes(*b, ckv::Key::Up, 2);
    CK_CHECK(b->bounds() == (Rect{0, 14, 80, 10}));  // a 14/10 split, by hand
    CK_CHECK(desktop.filled_tile_fractions().size() == 2);

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 28}));   // 14/24 of 48
    CK_CHECK(b->bounds() == (Rect{0, 28, 80, 20}));  // 10/24 of 48
    desktop.set_bounds(Rect{0, 0, 80, 24});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 14}));
    CK_CHECK(b->bounds() == (Rect{0, 14, 80, 10}));
}

CK_TEST(the_same_tiling_assembled_by_a_host_is_not_remembered) {
    // The partner of the test above, and the reason it is not vacuous: the
    // identical geometry, arrived at by a host's own set_bounds rather than by
    // a reader's gesture, is left to the ordinary clamp. A host that wants its
    // layout maintained says so — with a grow policy, or by laying it out
    // again — and one that merely placed two windows is not signed up to have
    // them resized underneath it.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    a->set_bounds(Rect{0, 0, 80, 14});
    b->set_bounds(Rect{0, 14, 80, 10});
    CK_CHECK(desktop.filled_tile_fractions().size() == 2);  // it IS a tiling, and is reported as one

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 14}));
    CK_CHECK(b->bounds() == (Rect{0, 14, 80, 10}));
    CK_CHECK(desktop.filled_tile_fractions().empty());
}

CK_TEST(a_dialog_opening_over_a_tiling_does_not_take_the_arrangement_with_it) {
    // A newcomer holds none of the arrangement's cells, and the windows that
    // do still hold exactly what they were given. Forgetting the arrangement
    // on every addition would cost a reader their tiling for opening a
    // message box — including the two dialogs a Desktop presents itself.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();
    Window* dialog = desktop.add_window(make_window(f, "About"));
    dialog->set_resizable(false);
    dialog->set_bounds(Rect{20, 6, 30, 8});
    CK_CHECK(desktop.filled_tile_fractions().size() == 2);  // a fixed-size window is above it

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 24}));
    CK_CHECK(b->bounds() == (Rect{0, 24, 80, 24}));
    const std::unique_ptr<Window> closed = desktop.remove_window(dialog);
    CK_CHECK(closed.get() == dialog);
    CK_CHECK(desktop.filled_tile_fractions().size() == 2);
}

CK_TEST(a_resizable_window_over_a_tiling_suspends_the_verdict_but_not_the_arrangement) {
    // The verdict is about what is on the desktop now; the arrangement is
    // about what was arranged. A window covering part of the grid answers the
    // first and not the second — so the shadows come back while it is open,
    // the bands underneath are still re-divided by a resize, and the verdict
    // returns the moment it closes.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();
    Window* c = desktop.add_window(make_window(f, "C"));
    c->set_bounds(Rect{10, 4, 20, 6});
    CK_CHECK(desktop.filled_tile_fractions().empty());

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 24}));
    CK_CHECK(b->bounds() == (Rect{0, 24, 80, 24}));
    CK_CHECK(c->bounds() == (Rect{10, 4, 20, 6}));  // itself only clamped, never re-divided
    const std::unique_ptr<Window> closed = desktop.remove_window(c);
    CK_CHECK(closed.get() == c);
    CK_CHECK(desktop.filled_tile_fractions().size() == 2);
}

CK_TEST(a_window_leaving_the_desktop_forgets_the_arrangement) {
    // Which is also what keeps a remembered cell from ever naming a window
    // that no longer exists.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    Window* c = desktop.add_window(make_window(f, "C"));
    desktop.tile_vertically();
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 8}));
    const std::unique_ptr<Window> departed = desktop.remove_window(c);
    CK_CHECK(departed.get() == c);

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 8}));  // not re-divided between the two survivors
    CK_CHECK(b->bounds() == (Rect{0, 8, 80, 8}));
    CK_CHECK(desktop.filled_tile_fractions().empty());
}

CK_TEST(an_arrangement_holds_down_to_one_row_a_band_and_is_lost_below_it) {
    // The floor, stated as a fixture: two stacked bands need two rows, not
    // four. Window's default minimum is 10x4, and the arrangement deliberately
    // writes cells straight through that — a band is the arrangement's answer,
    // and letting one window's minimum push it back over its neighbour would
    // reopen the very overlap this exists to prevent.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();

    desktop.set_bounds(Rect{0, 0, 80, 2});  // exactly n rows for n bands
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 1}));
    CK_CHECK(b->bounds() == (Rect{0, 1, 80, 1}));
    CK_CHECK(covers_exactly_once(desktop, desktop.content_area()));
    desktop.set_bounds(Rect{0, 0, 80, 24});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 12}));  // and it comes all the way back
    CK_CHECK(b->bounds() == (Rect{0, 12, 80, 12}));

    // One row further and the second band would be nothing at all. Nothing is
    // re-laid, the arrangement is forgotten, and the ordinary clamp — which
    // does honour the minimum, and does overlap — takes over for good.
    desktop.set_bounds(Rect{0, 0, 80, 1});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 4}));
    CK_CHECK(b->bounds() == (Rect{0, 0, 80, 4}));
    desktop.set_bounds(Rect{0, 0, 80, 24});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 4}));  // honestly lost, not silently restored
    CK_CHECK(b->bounds() == (Rect{0, 0, 80, 4}));
    CK_CHECK(desktop.filled_tile_fractions().empty());
}

CK_TEST(a_minimized_window_comes_back_into_the_cell_the_arrangement_grew_for_it) {
    // Membership is the arrangement's question, not the desktop's: a window
    // that is merely hidden left the screen, not the tiling, and it is coming
    // back into the cell it left — at whatever size that cell is by then.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();
    b->set_minimized(true);

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 24}));
    CK_CHECK(b->bounds() == (Rect{0, 24, 80, 24}));  // reflowed while parked
    CK_CHECK(desktop.filled_tile_fractions().empty());  // it is not on the desktop to be counted

    b->set_minimized(false);
    CK_CHECK(b->bounds() == (Rect{0, 24, 80, 24}));
    CK_CHECK(desktop.filled_tile_fractions().size() == 2);  // and the tiling is whole again
}

CK_TEST(a_keep_filling_window_keeps_its_own_authority_over_a_band) {
    // A window with a grow policy is already sized by something on every
    // resize. Two authorities writing one window's bounds is a bug waiting for
    // a resize, so the policy keeps it and the arrangement is not remembered
    // at all — not even for the windows around it.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    b->set_grow_policy(ckv::widgets::DesktopGrowPolicy::KeepFilling);
    desktop.tile_vertically();
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 12}));
    CK_CHECK(b->bounds() == (Rect{0, 12, 80, 12}));

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(b->bounds() == (Rect{0, 0, 80, 48}));  // its policy, unchanged
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 12}));  // and no band was re-divided
}

CK_TEST(a_zoomed_window_in_a_tiling_still_tracks_the_content_area) {
    // The same veto, for the same reason, through zoom instead of a policy:
    // refresh_zoom_area must keep running for a zoomed window, and it does.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    b->toggle_zoom(desktop.content_area());
    desktop.tile_vertically();

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(b->zoomed());
    CK_CHECK(b->bounds() == desktop.content_area());
    CK_CHECK(b->bounds() == (Rect{0, 0, 80, 48}));
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 12}));
}

CK_TEST(one_window_told_to_tile_fills_the_desktop_but_one_merely_placed_does_not) {
    // A single window covering the area is a one-cell partition, so the two
    // halves of this test are geometrically identical and differ only in
    // whether an arrangement was ever stated. That difference is the whole
    // policy: DesktopGrowPolicy::None still means "never grown" for a floating
    // window, and stops meaning it only for a window in a stated arrangement.
    Fixture f;
    Desktop placed_only(Rect{0, 0, 80, 24});
    Window* floating = placed_only.add_window(make_window(f, "A"));
    floating->set_bounds(placed_only.content_area());
    placed_only.set_bounds(Rect{0, 0, 120, 40});
    CK_CHECK(floating->bounds() == (Rect{0, 0, 80, 24}));

    Fixture g;
    Desktop tiled(Rect{0, 0, 80, 24});
    Window* stated = tiled.add_window(make_window(g, "A"));
    tiled.tile();
    CK_CHECK(stated->bounds() == (Rect{0, 0, 80, 24}));
    tiled.set_bounds(Rect{0, 0, 120, 40});
    CK_CHECK(stated->bounds() == (Rect{0, 0, 120, 40}));
}

CK_TEST(a_cascade_supersedes_the_tiling_it_replaced) {
    // Every command that lays windows out ends by stating what it made, this
    // one included — so the tiling before it is gone at once rather than
    // lingering until the next resize notices.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();
    desktop.cascade();
    CK_CHECK(a->bounds() == (Rect{0, 0, 53, 16}));
    CK_CHECK(b->bounds() == (Rect{2, 1, 53, 16}));

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(a->bounds() == (Rect{0, 0, 53, 16}));  // no band re-appears under them
    CK_CHECK(b->bounds() == (Rect{2, 1, 53, 16}));
}

CK_TEST(an_arrangement_under_docked_chrome_is_re_divided_within_the_content_area) {
    // The rows a menu bar holds are not the arrangement's to divide, in either
    // direction: the cells are mapped from the content area they were stated
    // in, into the content area there is now.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 25});
    desktop.dock_top(std::make_unique<ckv::ui::View>());
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();
    CK_CHECK(desktop.content_area() == (Rect{0, 1, 80, 24}));
    CK_CHECK(a->bounds() == (Rect{0, 1, 80, 12}));
    CK_CHECK(b->bounds() == (Rect{0, 13, 80, 12}));

    desktop.set_bounds(Rect{0, 0, 80, 49});
    CK_CHECK(desktop.content_area() == (Rect{0, 1, 80, 48}));
    CK_CHECK(a->bounds() == (Rect{0, 1, 80, 24}));
    CK_CHECK(b->bounds() == (Rect{0, 25, 80, 24}));
    CK_CHECK(covers_exactly_once(desktop, desktop.content_area()));
}

CK_TEST(a_restored_snapshot_states_an_arrangement_the_next_resize_re_divides) {
    // A snapshot replays absolute rectangles; what is remembered is the
    // arrangement those rectangles turn out to make on THIS desktop.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();
    const Desktop::Snapshot snapshot = desktop.snapshot();
    desktop.cascade();
    CK_CHECK(desktop.filled_tile_fractions().empty());

    desktop.restore(snapshot);
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 12}));
    CK_CHECK(b->bounds() == (Rect{0, 12, 80, 12}));

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(a->bounds() == (Rect{0, 0, 80, 24}));
    CK_CHECK(b->bounds() == (Rect{0, 24, 80, 24}));
}

CK_TEST(the_arrangement_reflow_survives_a_window_that_destroys_itself_from_on_resized) {
    // Writing a cell enters the window's own on_resized, which is
    // application-extensible and may detach and destroy any window — including
    // one this pass has not reached yet.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    Window* survivor = desktop.add_window(make_window(f, "Survivor"));
    auto self_removing = std::make_unique<ResizeSelfRemovingWindow>(desktop, "Self removing");
    ResizeSelfRemovingWindow* self_removing_ptr = self_removing.get();
    desktop.add_window(std::move(self_removing));
    desktop.tile_vertically();
    self_removing_ptr->arm();

    desktop.set_bounds(Rect{0, 0, 80, 48});
    CK_CHECK(desktop.windows().size() == 1);
    CK_CHECK(desktop.windows().front() == survivor);
    CK_CHECK(survivor->bounds() == (Rect{0, 0, 80, 24}));  // its own cell, written before the removal

    // And the removal took the arrangement with it, so the next resize is an
    // ordinary clamp.
    desktop.set_bounds(Rect{0, 0, 80, 20});
    CK_CHECK(survivor->bounds() == (Rect{0, 0, 80, 20}));
}

CK_TEST(shadows_stay_off_across_a_resize_that_keeps_the_tiling) {
    // The reason this matters beyond geometry: the shadow rule reads the same
    // filled-tiling verdict, so an arrangement lost to a resize used to put
    // every shadow back — a dark smudge along every seam of a grid the reader
    // never touched.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    desktop.set_context(f.ctx());
    Window* a = desktop.add_window(make_window(f, "A"));
    Window* b = desktop.add_window(make_window(f, "B"));
    desktop.tile_vertically();
    CK_CHECK(!desktop.child_casts_shadow(*a));
    CK_CHECK(!desktop.child_casts_shadow(*b));

    desktop.set_bounds(Rect{0, 0, 80, 23});
    CK_CHECK(desktop.filled_tile_fractions().size() == 2);
    CK_CHECK(!desktop.child_casts_shadow(*a));
    CK_CHECK(!desktop.child_casts_shadow(*b));
}

// --- A world larger than the view of it (U7-a) -----------------------------
//
// A Desktop is its own viewport until a host says otherwise. An EXTENT makes
// the world bigger than the hole it is seen through; a PAN says which part of
// it the hole is over. The distinction these cases exist to pin is that the
// two vocabularies never mix: an arrangement is a statement about the world
// and does not change because a reader looked somewhere else, while drawing
// and hit-testing follow the view and must agree with each other.

CK_TEST(a_desktop_with_no_extent_is_its_own_world) {
    // The regression bar for the whole feature, stated once: a viewport equal
    // to the extent is the desktop every existing consumer already has.
    AttachedFixture af;
    Window* w = af.desktop->add_window(make_window(af.f));

    CK_CHECK((af.desktop->extent() == ckv::Size{0, 0}));  // "no extent of my own"
    CK_CHECK(af.desktop->content_area().width == af.desktop->bounds().width);
    CK_CHECK((af.desktop->pan() == ckv::Point{0, 0}));
    CK_CHECK((w->paint_offset() == ckv::Point{0, 0}));
    CK_CHECK(w->absolute_bounds().x == w->bounds().x);
}

CK_TEST(an_extent_makes_the_content_area_the_worlds_rather_than_the_views) {
    AttachedFixture af;
    const int view_width = af.desktop->bounds().width;
    af.desktop->set_extent(ckv::Size{200, 60});

    // Everything that ARRANGES windows reads this, so it has to answer in
    // world units: a maximized window fills the world, not the hole.
    CK_CHECK(af.desktop->content_area().width == 200);
    CK_CHECK(af.desktop->content_area().width != view_width);
    CK_CHECK(af.desktop->bounds().width == view_width);  // the view did not move
}

CK_TEST(panning_moves_where_a_window_is_drawn_and_not_where_it_is) {
    // The invariant ckmux depends on: a window's rect is session state shared
    // between readers, and a reader scrolling must not restate it.
    AttachedFixture af;
    af.desktop->set_extent(ckv::Size{200, 60});
    Window* w = af.desktop->add_window(make_window(af.f));
    w->set_bounds(ckv::Rect{100, 20, 30, 8});
    const ckv::Rect stated = w->bounds();

    af.desktop->set_pan(ckv::Point{40, 5});
    CK_CHECK(w->bounds() == stated);                       // the arrangement is untouched
    CK_CHECK((w->paint_offset() == ckv::Point{-40, -5}));  // it is DRAWN elsewhere
    // And hit-testing follows the drawing, or a click lands on whatever would
    // have been there if nobody had scrolled.
    CK_CHECK(w->absolute_bounds().x == stated.x - 40);
    CK_CHECK(w->absolute_bounds().y == stated.y - 5);

    af.desktop->set_pan(ckv::Point{0, 0});
    CK_CHECK(w->bounds() == stated);
    CK_CHECK(w->absolute_bounds().x == stated.x);
}

CK_TEST(a_pan_cannot_look_past_the_edge_of_the_world) {
    AttachedFixture af;
    af.desktop->set_extent(ckv::Size{200, 60});

    af.desktop->set_pan(ckv::Point{10'000, 10'000});
    const ckv::Point panned = af.desktop->pan();
    CK_CHECK(panned.x == 200 - af.desktop->bounds().width);
    CK_CHECK(panned.x >= 0 && panned.y >= 0);

    af.desktop->set_pan(ckv::Point{-50, -50});
    CK_CHECK((af.desktop->pan() == ckv::Point{0, 0}));
}

CK_TEST(a_world_that_shrinks_under_the_view_pulls_the_pan_back_with_it) {
    AttachedFixture af;
    af.desktop->set_extent(ckv::Size{200, 60});
    af.desktop->set_pan(ckv::Point{100, 20});
    CK_CHECK(af.desktop->pan().x == 100);

    // The session desktop got smaller. The view cannot go on looking at a
    // region that no longer exists.
    af.desktop->set_extent(ckv::Size{100, 30});
    // Held against what the VIEW can show, which is the desktop's own bounds —
    // comparing the world against itself, as an earlier draft of this case
    // did, asserts that a 30-row world seen through a 24-row hole may not be
    // panned at all, and it may be panned by six.
    CK_CHECK(af.desktop->pan().x <= std::max(0, 100 - af.desktop->bounds().width));
    CK_CHECK(af.desktop->pan().y <= std::max(0, 30 - af.desktop->bounds().height));
}

CK_TEST(a_window_opened_while_panned_is_drawn_where_the_pan_says) {
    AttachedFixture af;
    af.desktop->set_extent(ckv::Size{200, 60});
    af.desktop->set_pan(ckv::Point{40, 5});

    Window* late = af.desktop->add_window(make_window(af.f));
    CK_CHECK((late->paint_offset() == ckv::Point{-40, -5}));
}

CK_TEST(docked_chrome_does_not_pan) {
    // A menu bar that scrolled off the top of the screen would not be a menu
    // bar. The docks belong to the view; the windows belong to the world.
    AttachedFixture af;
    auto* bar = af.desktop->dock_top(std::make_unique<ckv::ui::View>());
    af.desktop->set_extent(ckv::Size{200, 60});
    af.desktop->set_pan(ckv::Point{40, 5});

    CK_CHECK((bar->paint_offset() == ckv::Point{0, 0}));
    CK_CHECK(bar->absolute_bounds().y == bar->bounds().y);
    CK_CHECK(bar->bounds().width == af.desktop->bounds().width);  // the view's width
}

CK_TEST(a_cursor_panned_off_the_screen_is_withdrawn_rather_than_addressed) {
    // The case U7-a's own tests never posed, because every one of them panned
    // TOWARDS a window: what becomes of a window the pan LEAVES BEHIND, when
    // that window holds the focus and therefore the cursor. Found consuming
    // the feature (WP-43), where a reader with a world larger than their
    // screen pans past their own terminal in two keystrokes.
    //
    // A cursor is reported in absolute screen cells, so a pan takes it
    // negative, and `emit_cursor_move` will say `ESC[-3;-27H` — a row and a
    // column that do not exist. A strict host refuses the frame outright.
    //
    // The whole assertion is that a frame completes: painting is not a query,
    // so there is nothing to read back, and what there is to check is that
    // nothing addressed a cell the screen does not have. Under HeadlessTerminal
    // the VirtualDisplay IS that check — it parses what the presenter emits and
    // refuses anything it cannot address.
    AttachedFixture af;
    af.desktop->set_extent(ckv::Size{200, 60});
    Window* left_behind = af.desktop->add_window(make_window(af.f));
    left_behind->set_bounds(Rect{2, 4, 40, 10});
    auto* probe = left_behind->add_child(std::make_unique<CursorProbe>());
    probe->set_bounds(Rect{1, 1, 4, 2});
    af.app.set_focus(probe);
    af.app.step(0);

    af.desktop->set_pan(ckv::Point{60, 20});
    // Wholly outside the view: its right edge lands well left of column zero,
    // so not one of its cells — and not its cursor — is on screen.
    CK_CHECK((left_behind->paint_offset() == ckv::Point{-60, -20}));
    CK_CHECK(left_behind->absolute_bounds().x + left_behind->bounds().width <= 0);
    af.app.step(0);

    // And back: the window that was left behind is where it was, and its
    // cursor is addressable again — which is what says the frame above
    // withdrew the cursor rather than corrupting anything.
    af.desktop->set_pan(ckv::Point{0, 0});
    af.app.step(0);
    CK_CHECK((left_behind->absolute_bounds() == Rect{2, 4, 40, 10}));
    CK_CHECK((probe->cursor_state()->position == ckv::Point{3, 5}));
}

CK_TEST(a_minimized_window_takes_its_cursor_off_the_screen_with_it) {
    // The other half of the question the pan test above asks: a window can
    // leave the screen without moving at all. Minimizing it hides the frame
    // and everything in it, but nothing takes the keyboard away — with no
    // other window to hand it to there is nowhere for it to go — so the view
    // inside it still holds the focus and still reports a cursor, at the
    // absolute cell it occupied while it was visible.
    //
    // Reported by a reader as "the cursor does not disappear when I minimize
    // a window; it sits on the background as a bright cell", which is exactly
    // what a terminal draws when it is told to put the cursor in a cell that
    // now shows the desktop.
    AttachedFixture af;
    Window* window = af.desktop->add_window(make_window(af.f));
    window->set_bounds(Rect{2, 4, 40, 10});
    auto* probe = window->add_child(std::make_unique<CursorProbe>());
    probe->set_bounds(Rect{1, 1, 4, 2});
    af.app.set_focus(probe);
    af.app.step(0);
    CK_CHECK(af.app.current_cursor().visible);
    CK_CHECK((af.app.current_cursor().position == ckv::Point{3, 5}));

    window->set_minimized(true);
    af.app.step(0);
    CK_CHECK(!af.app.current_cursor().visible);
    // And the reason it is gone is the frame's, not the widget's: the focus
    // never moved and the view still offers the same cursor it always did.
    // Checked so a later change that fixed this by taking the focus away
    // instead cannot pass this test while leaving the frame rule unwritten.
    CK_CHECK(af.app.focused() == probe);
    CK_CHECK(probe->cursor_state().has_value());

    // Restored, the cursor comes back to the cell it left.
    window->set_minimized(false);
    af.app.step(0);
    CK_CHECK(af.app.current_cursor().visible);
    CK_CHECK((af.app.current_cursor().position == ckv::Point{3, 5}));
}

CK_TEST(pan_to_show_moves_the_least_it_can_and_nothing_if_it_need_not) {
    AttachedFixture af;
    af.desktop->set_extent(ckv::Size{200, 60});
    const int view_width = af.desktop->bounds().width;

    // Already visible: a reader who focuses what they can see does not want
    // their view thrown anywhere.
    af.desktop->pan_to_show(ckv::Rect{2, 2, 10, 4});
    CK_CHECK((af.desktop->pan() == ckv::Point{0, 0}));

    // Off to the right: brought in by exactly as much as it takes.
    af.desktop->pan_to_show(ckv::Rect{150, 2, 20, 4});
    CK_CHECK(af.desktop->pan().x == 170 - view_width);

    // And back the other way.
    af.desktop->pan_to_show(ckv::Rect{0, 2, 10, 4});
    CK_CHECK(af.desktop->pan().x == 0);
}
