// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// End-to-end M8 regressions for seams whose mechanisms are built and
// unit-tested elsewhere but were never wired to a real terminal
// session (see plans/api-ergonomics-review-2.md, findings F1-F7). Each
// scenario here is driven entirely through Application's public event
// dispatch — the same path the POSIX/headless backends use — not by
// poking the affected widget's own methods directly, since that is
// exactly the gap that let each finding stay green under unit tests
// while broken interactively.
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

std::unique_ptr<Window> make_window(Fixture& f, std::string title = "W") {
    return std::make_unique<Window>(std::move(title), f.theme, f.roles.window_frame_active,
                                     f.roles.window_frame_inactive, f.roles.window_title_active,
                                     f.roles.window_title_inactive);
}
}  // namespace

// --- WP-1: the resize chain -------------------------------------------

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

    auto window = make_window(fx);
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

// --- WP-3: click-to-activate/raise, end to end -------------------------

CK_TEST(clicking_deep_content_in_a_background_window_raises_and_activates_it_through_dispatch) {
    Fixture fx;
    ckv::term::HeadlessTerminal term(Size{80, 24});
    ManualClock clock;
    Application app(term, clock);

    auto desktop = std::make_unique<Desktop>(app.root().bounds(), fx.theme, fx.roles.desktop_background);
    Desktop* desktop_ptr = desktop.get();
    app.root().add_child(std::move(desktop));

    auto back_window = make_window(fx, "Back");
    back_window->set_bounds(Rect{0, 0, 30, 10});
    auto back_content = std::make_unique<View>();
    auto* deep_leaf = back_content->add_child(std::make_unique<View>(Rect{2, 2, 4, 1}));
    back_window->set_content(std::move(back_content));
    Window* back = desktop_ptr->add_window(std::move(back_window));

    auto front_window = make_window(fx, "Front");
    front_window->set_bounds(Rect{40, 0, 30, 10});  // does not overlap `back`'s content point
    desktop_ptr->add_window(std::move(front_window));

    // `front` was added last, so it is active/topmost; `back` is
    // inactive and behind it — exactly the scenario a click on `back`
    // should fix.
    CK_CHECK(!back->active());

    // Click lands inside back_window's content, on deep_leaf — several
    // levels below Window itself. Window::on_mouse never runs for
    // this; only Desktop's ancestor-walk hook can raise/activate it.
    // A plain View's on_mouse is unhandled by default (returns false) —
    // click-to-activate is a side-channel notification independent of
    // that return value, so this deliberately does not assert it.
    const Rect leaf_abs = deep_leaf->absolute_bounds();
    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left,
                          ckv::Point{leaf_abs.x, leaf_abs.y}, std::nullopt, ckv::Modifier::None};
    app.dispatch(down);

    CK_CHECK(back->active());
    CK_CHECK(desktop_ptr->active_window() == back);
    // Raised to the top of z-order (children() convention: last = topmost).
    CK_CHECK(desktop_ptr->children().back().get() == static_cast<ckv::ui::View*>(back));
}

CK_TEST(clicking_the_desktops_own_background_does_not_change_which_window_is_active) {
    Fixture fx;
    ckv::term::HeadlessTerminal term(Size{80, 24});
    ManualClock clock;
    Application app(term, clock);

    auto desktop = std::make_unique<Desktop>(app.root().bounds(), fx.theme, fx.roles.desktop_background);
    Desktop* desktop_ptr = desktop.get();
    app.root().add_child(std::move(desktop));

    auto window = make_window(fx);
    window->set_bounds(Rect{0, 0, 10, 5});
    Window* w = desktop_ptr->add_window(std::move(window));
    CK_CHECK(w->active());

    // Click far outside any window, on bare desktop background.
    ckv::MouseEvent down{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{60, 20}, std::nullopt,
                          ckv::Modifier::None};
    app.dispatch(down);

    CK_CHECK(w->active());  // unchanged — no window claims a desktop-background click
}
