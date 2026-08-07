// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// End-to-end M8/WP-1 regression: a real terminal resize, dispatched
// through Application exactly as the POSIX/headless backends deliver
// it, must reach a Desktop attached under root() and repin its docked
// chrome / reclamp its windows — not just Desktop::on_resized() called
// directly (test_desktop.cpp covers that mechanism in isolation
// already). Before this landed, ResizeEvent stopped at
// Application::root(); nothing propagated further, so this exact
// scenario went stale interactively while every unit-level piece
// stayed green (see plans/api-ergonomics-review-2.md, finding F1).
#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

using ckv::Rect;
using ckv::Size;
using ckv::ManualClock;
using ckv::ResizeEvent;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::ui::View;
using ckv::widgets::Desktop;
using ckv::widgets::Window;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};
}  // namespace

CK_TEST(an_application_level_resize_event_repins_docked_desktop_chrome_and_reclamps_windows) {
    Fixture fx;
    ckv::term::HeadlessTerminal term(Size{80, 24});
    ManualClock clock;
    Application app(term, clock);

    auto desktop = std::make_unique<Desktop>(app.root().bounds(), fx.theme, fx.roles.desktop_background);
    Desktop* desktop_ptr = desktop.get();
    app.root().add_child(std::move(desktop));

    View* top = desktop_ptr->dock_top(std::make_unique<View>());
    View* bottom = desktop_ptr->dock_bottom(std::make_unique<View>());

    auto window = std::make_unique<Window>("W", fx.theme, fx.roles.window_frame_active,
                                            fx.roles.window_frame_inactive, fx.roles.window_title_active,
                                            fx.roles.window_title_inactive);
    window->set_bounds(Rect{60, 15, 15, 6});  // reachable in the original 80x24 desktop
    Window* window_ptr = desktop_ptr->add_window(std::move(window));

    // Shrink, exactly as a POSIX SIGWINCH-driven ResizeEvent would.
    CK_CHECK(app.dispatch(ResizeEvent{Size{40, 10}}));

    CK_CHECK(desktop_ptr->bounds() == (Rect{0, 0, 40, 10}));
    CK_CHECK(top->bounds() == (Rect{0, 0, 40, 1}));
    CK_CHECK(bottom->bounds() == (Rect{0, 9, 40, 1}));
    // The window must remain fully inside the new content area — never
    // left stranded outside the shrunk terminal.
    const Rect content_after_shrink = desktop_ptr->content_area();
    const Rect win_after_shrink = window_ptr->bounds();
    CK_CHECK(win_after_shrink.x >= content_after_shrink.x);
    CK_CHECK(win_after_shrink.y >= content_after_shrink.y);
    CK_CHECK(win_after_shrink.x + win_after_shrink.width <= content_after_shrink.x + content_after_shrink.width);
    CK_CHECK(win_after_shrink.y + win_after_shrink.height <= content_after_shrink.y + content_after_shrink.height);

    // Grow past the original size — chrome must track the NEW edges,
    // not stay pinned to the old (or shrunk) ones.
    CK_CHECK(app.dispatch(ResizeEvent{Size{120, 50}}));

    CK_CHECK(desktop_ptr->bounds() == (Rect{0, 0, 120, 50}));
    CK_CHECK(top->bounds() == (Rect{0, 0, 120, 1}));
    CK_CHECK(bottom->bounds() == (Rect{0, 49, 120, 1}));
}

CK_TEST(a_view_that_opts_out_of_fills_root_is_left_alone_by_a_terminal_resize) {
    ckv::term::HeadlessTerminal term(Size{80, 24});
    ManualClock clock;
    Application app(term, clock);

    auto* fixed = app.root().add_child(std::make_unique<View>(Rect{5, 5, 10, 3}));
    fixed->set_fills_root(false);

    CK_CHECK(app.dispatch(ResizeEvent{Size{200, 60}}));

    CK_CHECK(fixed->bounds() == (Rect{5, 5, 10, 3}));  // untouched
    CK_CHECK(app.root().bounds() == (Rect{0, 0, 200, 60}));  // root itself still resized
}

CK_TEST(a_default_root_child_fills_root_on_construction_default_and_on_resize) {
    ckv::term::HeadlessTerminal term(Size{80, 24});
    ManualClock clock;
    Application app(term, clock);

    auto* child = app.root().add_child(std::make_unique<View>());
    CK_CHECK(child->fills_root());  // default is fill (F1's escape hatch defaults to on)

    CK_CHECK(app.dispatch(ResizeEvent{Size{30, 12}}));
    CK_CHECK(child->bounds() == (Rect{0, 0, 30, 12}));
}

CK_TEST(on_bounds_changed_fires_with_the_new_bounds_only_when_bounds_actually_change) {
    View v(Rect{1, 1, 5, 5});
    int fire_count = 0;
    Rect last_seen{};
    v.on_bounds_changed = [&](Rect b) {
        ++fire_count;
        last_seen = b;
    };

    v.set_bounds(Rect{2, 2, 8, 8});
    CK_CHECK(fire_count == 1);
    CK_CHECK(last_seen == (Rect{2, 2, 8, 8}));

    v.set_bounds(Rect{2, 2, 8, 8});  // identical — set_bounds no-ops, hook must not refire
    CK_CHECK(fire_count == 1);
}
