// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/window_switcher_bar.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"

using ckv::ManualClock;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Desktop;
using ckv::widgets::DropdownMenu;
using ckv::widgets::MenuItem;
using ckv::widgets::PagedStrip;
using ckv::widgets::StatusLine;
using ckv::widgets::Window;
using ckv::widgets::WindowSwitcherBar;
using ckv::widgets::WindowSwitcherTarget;
namespace ui = ckv::ui;

namespace {

struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    Desktop desktop{Rect{0, 0, 80, 24}};

    Fixture() { desktop.set_context(ui::Context{&theme, &registry, &app}); }

    // A bar living directly on the desktop at a width the test chooses,
    // rather than docked: docking would re-place it on every desktop resize,
    // and the narrow-width cases below are precisely about a width nobody
    // else gets to pick.
    WindowSwitcherBar* bar(int width = 80) {
        auto* view = desktop.add(std::make_unique<WindowSwitcherBar>(desktop));
        view->set_bounds(Rect{0, 0, width, 1});
        return view;
    }

    Window* open(std::string title) {
        return desktop.add_window(std::make_unique<Window>(std::move(title)));
    }
};

ckv::MouseEvent press(int x, ckv::MouseButton button = ckv::MouseButton::Left) {
    return ckv::MouseEvent{ckv::MouseAction::Down, button, Point{x, 0}, std::nullopt,
                           Modifier::None};
}

ckv::MouseEvent release(int x, ckv::MouseButton button = ckv::MouseButton::Left) {
    return ckv::MouseEvent{ckv::MouseAction::Up, button, Point{x, 0}, std::nullopt, Modifier::None};
}

// Which window is currently frontmost. Desktop::children() holds docks and
// popups too, so the answer is the last child that is a window — the same
// thing Desktop's own removal path looks for.
Window* topmost_window(const Desktop& desktop) {
    for (auto it = desktop.children().rbegin(); it != desktop.children().rend(); ++it)
        if (auto* window = dynamic_cast<Window*>(it->get())) return window;
    return nullptr;
}

// What a row draws for a window: its status glyph, a blank, then the name
// (U4-j). Composed from the widget's own table so these tests state the NAME
// they expect and the shapes stay spelled in exactly one place.
std::string entry_text(WindowSwitcherBar::Status status, std::string_view name) {
    return std::string(WindowSwitcherBar::status_glyph(status)) + " " + std::string(name);
}

// The middle column of an entry, so a click lands on it whatever its padding.
int click_column(const WindowSwitcherBar& bar, std::size_t index) {
    for (const WindowSwitcherBar::DrawnEntry& drawn : bar.drawn_entries())
        if (drawn.index == index) return drawn.x + drawn.width / 2;
    return -1;
}

}  // namespace

// --- The list -------------------------------------------------------------

CK_TEST(three_open_windows_produce_three_entries_in_the_desktops_own_order) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    Window* bravo = f.open("Bravo");
    Window* charlie = f.open("Charlie");

    CK_CHECK(bar->entries().size() == 3);
    CK_CHECK(bar->entries()[0].window == alpha);
    CK_CHECK(bar->entries()[1].window == bravo);
    CK_CHECK(bar->entries()[2].window == charlie);
    CK_CHECK(bar->entries()[0].label == "Alpha");
    CK_CHECK(bar->entries()[2].label == "Charlie");
    // The last window opened took activation with it, and exactly one entry
    // says so.
    CK_CHECK(!bar->entries()[0].active);
    CK_CHECK(!bar->entries()[1].active);
    CK_CHECK(bar->entries()[2].active);
}

CK_TEST(a_bar_built_after_the_windows_lists_them_without_waiting_for_a_change) {
    Fixture f;
    f.open("Alpha");
    f.open("Bravo");
    WindowSwitcherBar* bar = f.bar();
    CK_CHECK(bar->entries().size() == 2);
}

CK_TEST(entries_track_windows_opening_closing_being_renamed_and_changing_activation) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    Window* bravo = f.open("Bravo");
    f.open("Charlie");
    CK_CHECK(bar->entries().size() == 3);

    // Renamed. Nothing else in the tree carries this: the window repaints its
    // own frame and the bar would otherwise keep drawing the old name.
    alpha->set_title("Alpha (edited)");
    CK_CHECK(bar->entries()[0].label == "Alpha (edited)");

    // Activation moved by the standard cycling command, not by the bar.
    f.desktop.activate(alpha);
    CK_CHECK(bar->entries()[0].active);
    CK_CHECK(!bar->entries()[2].active);

    // Closed.
    f.desktop.remove_window(bravo).reset();
    CK_CHECK(bar->entries().size() == 2);
    CK_CHECK(bar->entries()[0].window == alpha);
    CK_CHECK(bar->entries()[1].label == "Charlie");

    // Opened again.
    f.open("Delta");
    CK_CHECK(bar->entries().size() == 3);
    CK_CHECK(bar->entries()[2].label == "Delta");
    CK_CHECK(bar->entries()[2].active);
}

CK_TEST(a_bar_whose_providers_are_the_hosts_own_never_reads_a_desktop) {
    // The whole point of the providers: the same row, the same layout and the
    // same input over a window model that is not a Desktop's.
    Fixture f;
    Window standalone_a{"Session 1"};
    Window standalone_b{"Session 2"};
    Window* activated = nullptr;

    auto* bar = f.desktop.add(std::make_unique<WindowSwitcherBar>());
    bar->set_bounds(Rect{0, 0, 80, 1});
    bar->set_window_source([&] { return std::vector<Window*>{&standalone_a, &standalone_b}; });
    bar->set_label_provider([](Window& window) { return "[" + window.title() + "]"; });
    bar->set_active_provider([&]() -> Window* { return &standalone_b; });
    bar->set_activate_action([&](Window& window) { activated = &window; });

    CK_CHECK(bar->entries().size() == 2);
    CK_CHECK(bar->entries()[0].label == "[Session 1]");
    CK_CHECK(bar->entries()[1].active);

    const int x = click_column(*bar, 0);
    CK_CHECK(bar->on_mouse(press(x)));
    CK_CHECK(bar->on_mouse(release(x)));
    CK_CHECK(activated == &standalone_a);
    // Nothing about the desktop moved: the host's action is the whole of what
    // clicking a row does.
    CK_CHECK(f.desktop.windows().empty());
}

// --- Clicking -------------------------------------------------------------

CK_TEST(clicking_each_entry_activates_and_raises_the_matching_window) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    Window* bravo = f.open("Bravo");
    Window* charlie = f.open("Charlie");

    for (Window* expected : {alpha, bravo, charlie, alpha}) {
        std::size_t index = 0;
        for (std::size_t i = 0; i < bar->entries().size(); ++i)
            if (bar->entries()[i].window == expected) index = i;
        const int x = click_column(*bar, index);
        CK_CHECK(x >= 0);
        Window* const before = f.desktop.active_window();
        CK_CHECK(bar->on_mouse(press(x)));
        // The press alone decides nothing — it only says which row would act.
        CK_CHECK(f.desktop.active_window() == before);
        CK_CHECK(bar->on_mouse(release(x)));
        CK_CHECK(f.desktop.active_window() == expected);
        CK_CHECK(topmost_window(f.desktop) == expected);
        CK_CHECK(bar->entries()[index].active);
    }
}

CK_TEST(a_press_released_away_from_its_entry_takes_the_click_back) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    f.open("Alpha");
    Window* charlie = f.open("Charlie");
    CK_CHECK(f.desktop.active_window() == charlie);

    const int on_alpha = click_column(*bar, 0);
    CK_CHECK(bar->on_mouse(press(on_alpha)));
    CK_CHECK(bar->on_mouse(release(70)));  // empty run past the last entry
    CK_CHECK(f.desktop.active_window() == charlie);
}

CK_TEST(a_click_on_the_bars_empty_run_is_unhandled) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    f.open("Alpha");
    CK_CHECK(!bar->on_mouse(press(70)));
}

// --- Right-click ----------------------------------------------------------

CK_TEST(right_clicking_a_row_opens_the_hosts_menu_for_that_rows_window) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    f.open("Bravo");
    Window* charlie = f.open("Charlie");
    CK_CHECK(f.desktop.active_window() == charlie);

    Window* asked_about = nullptr;
    bar->set_context_menu_provider([&](const WindowSwitcherTarget& target) {
        asked_about = target.window();
        return std::vector<MenuItem>{MenuItem::action("&Close", target.bind([](Window& window) {
            window.close();
        }))};
    });

    bool closed_alpha = false;
    bool closed_charlie = false;
    alpha->on_closed = [&] { closed_alpha = true; };
    charlie->on_closed = [&] { closed_charlie = true; };

    CK_CHECK(bar->on_mouse(press(click_column(*bar, 0), ckv::MouseButton::Right)));
    CK_CHECK(asked_about == alpha);
    CK_CHECK(f.desktop.popups().size() == 1);

    // Right-clicking a background row does NOT take the reader to it.
    CK_CHECK(f.desktop.active_window() == charlie);

    auto* menu = dynamic_cast<DropdownMenu*>(f.desktop.popups().front());
    CK_CHECK(menu != nullptr);
    CK_CHECK(menu->items().size() == 1);

    // The correctness point: command dispatch would have run against the
    // FOCUSED window, and the focused window here is Charlie. The bound item
    // acts on the window whose row was clicked.
    menu->items()[0].action()();
    CK_CHECK(closed_alpha);
    CK_CHECK(!closed_charlie);
}

CK_TEST(a_menu_item_whose_window_closed_while_the_menu_stood_open_is_a_no_op) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    f.open("Bravo");

    int ran = 0;
    bar->set_context_menu_provider([&](const WindowSwitcherTarget& target) {
        return std::vector<MenuItem>{MenuItem::action("&Act", target.bind([&](Window&) { ++ran; }))};
    });
    CK_CHECK(bar->on_mouse(press(click_column(*bar, 0), ckv::MouseButton::Right)));
    auto* menu = dynamic_cast<DropdownMenu*>(f.desktop.popups().front());
    CK_CHECK(menu != nullptr);
    const std::function<void()> chosen = menu->items()[0].action();

    // The window goes away between opening the menu and choosing from it —
    // a timer, a child dialog, or the reader's other hand.
    f.desktop.remove_window(alpha).reset();
    chosen();
    CK_CHECK(ran == 0);
}

CK_TEST(a_host_with_nothing_to_offer_for_a_window_opens_no_menu) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    f.open("Alpha");
    bar->set_context_menu_provider([](const WindowSwitcherTarget&) { return std::vector<MenuItem>{}; });
    // Unconsumed, so an application's own desktop-wide context menu still
    // gets the press.
    CK_CHECK(!bar->on_mouse(press(click_column(*bar, 0), ckv::MouseButton::Right)));
    CK_CHECK(f.desktop.popups().empty());
}

CK_TEST(a_bar_with_no_context_menu_provider_leaves_the_right_press_alone) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    f.open("Alpha");
    CK_CHECK(!bar->on_mouse(press(click_column(*bar, 0), ckv::MouseButton::Right)));
    CK_CHECK(f.desktop.popups().empty());
}

// --- Layout, paging and elision -------------------------------------------

CK_TEST(entries_take_their_natural_width_while_the_bar_is_wide_enough) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    f.open("Alpha");
    f.open("Bravo");
    f.open("Charlie");

    const std::vector<WindowSwitcherBar::DrawnEntry> drawn = bar->drawn_entries();
    CK_CHECK(drawn.size() == 3);
    // A status glyph and a blank before each label, one padding cell either
    // side of the pair, one blank cell between entries.
    using Status = WindowSwitcherBar::Status;
    CK_CHECK(drawn[0].x == 0 && drawn[0].width == 9);
    CK_CHECK(drawn[0].text == entry_text(Status::Visible, "Alpha"));
    CK_CHECK(drawn[1].x == 10 && drawn[1].width == 9);
    CK_CHECK(drawn[1].text == entry_text(Status::Visible, "Bravo"));
    // The window opened last is the one the reader is in, and says so.
    CK_CHECK(drawn[2].x == 20 && drawn[2].width == 11);
    CK_CHECK(drawn[2].text == entry_text(Status::Active, "Charlie"));
    // A bar wide enough for its windows is exactly what it was before it knew
    // how to page: one page, no index, no controls, nothing at column zero but
    // the first window.
    CK_CHECK(bar->page_count() == 1);
    CK_CHECK(bar->page_index_text().empty());
    CK_CHECK(bar->chrome().collapse_x == -1);
    CK_CHECK(bar->chrome().previous_x == -1);
    CK_CHECK(bar->chrome().next_x == -1);
    CK_CHECK(bar->chrome().index_x == -1);
    CK_CHECK(bar->chrome().items_x == 0);
}

CK_TEST(a_bar_too_narrow_for_its_windows_pages_them_rather_than_shortening_their_names) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar(20);
    f.open("Alpha");
    f.open("Bravo");
    f.open("Charlie");

    // Every window still keeps a row — dropping one would make it unreachable
    // from the very bar that exists to reach it — but the row it keeps is on a
    // page of its own rather than a squeezed share of this one.
    CK_CHECK(bar->page_count() == 3);
    CK_CHECK(bar->page() == 0);
    CK_CHECK(bar->page_index_text() == "1/3");
    CK_CHECK(bar->chrome().previous_x == 0);
    CK_CHECK(bar->chrome().index_x == 1);
    CK_CHECK(bar->chrome().items_x == 5);
    CK_CHECK(bar->chrome().next_x == 19);

    std::vector<WindowSwitcherBar::DrawnEntry> drawn = bar->drawn_entries();
    CK_CHECK(drawn.size() == 1);
    CK_CHECK(drawn[0].index == 0 && drawn[0].x == 5 && drawn[0].width == 9);
    // Whole, not "Alp…" — and the icon it carries is not what pushed it onto
    // a page of its own: the name alone would not have fitted beside another.
    CK_CHECK(drawn[0].text == entry_text(WindowSwitcherBar::Status::Visible, "Alpha"));

    CK_CHECK(bar->next_page());
    drawn = bar->drawn_entries();
    CK_CHECK(drawn.size() == 1 && drawn[0].index == 1);
    CK_CHECK(drawn[0].text == entry_text(WindowSwitcherBar::Status::Visible, "Bravo"));
    CK_CHECK(bar->page_index_text() == "2/3");
}

CK_TEST(a_window_on_another_page_has_no_columns_and_takes_no_clicks) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar(20);
    f.open("Alpha");
    f.open("Bravo");
    Window* charlie = f.open("Charlie");
    CK_CHECK(f.desktop.active_window() == charlie);

    // The entries are all still listed; only the current page has columns.
    CK_CHECK(bar->entries().size() == 3);
    CK_CHECK(click_column(*bar, 2) < 0);
    CK_CHECK(bar->entry_at(Point{5, 0}).value_or(99) == 0);
    // The paging chrome is not an entry.
    CK_CHECK(!bar->entry_at(Point{19, 0}).has_value());
    CK_CHECK(bar->on_mouse(press(19)));  // it is a control, though
    CK_CHECK(bar->page() == 1);
}

CK_TEST(a_window_whose_title_is_wider_than_the_row_is_alone_on_its_page_and_elided) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar(20);
    f.open("An extraordinarily long window title");
    f.open("Beta");

    // The one case elision survives paging: there is no other page to move a
    // single over-wide entry to.
    CK_CHECK(bar->page_count() == 2);
    const std::vector<WindowSwitcherBar::DrawnEntry> drawn = bar->drawn_entries();
    CK_CHECK(drawn.size() == 1);
    CK_CHECK(drawn[0].width == 13);
    // The glyph is at the FRONT of the text, so what the elision takes is the
    // tail of the name and what survives is the state — which is the half a
    // reader cannot recover by widening their terminal.
    CK_CHECK(drawn[0].text == entry_text(WindowSwitcherBar::Status::Visible, "An extra…"));
    CK_CHECK(bar->next_page());
    CK_CHECK(bar->drawn_entries()[0].text ==
             entry_text(WindowSwitcherBar::Status::Active, "Beta"));
}

CK_TEST(a_window_opening_or_closing_re_pages_the_bar) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar(20);
    f.open("Alpha");
    f.open("Bravo");
    f.open("Charlie");
    CK_CHECK(bar->page_count() == 3);

    Window* delta = f.open("Delta");
    CK_CHECK(bar->page_count() == 4);

    f.desktop.remove_window(delta).reset();
    CK_CHECK(bar->page_count() == 3);

    // Down to what fits: no pages left to walk, and the chrome goes with them.
    f.desktop.remove_window(f.desktop.windows().back()).reset();
    CK_CHECK(bar->page_count() == 1);
    CK_CHECK(bar->chrome().next_x == -1);
    CK_CHECK(bar->drawn_entries().size() == 2);
}

CK_TEST(closing_the_window_the_reader_is_looking_at_falls_back_a_page) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar(20);
    f.open("Alpha");
    f.open("Bravo");
    f.open("Charlie");
    Window* delta = f.open("Delta");
    CK_CHECK(bar->set_page(3));
    CK_CHECK(bar->drawn_entries()[0].index == 3);

    // In ckmux this is a terminal ending under the reader's eyes. The page
    // they were on no longer exists, and a blank row under a 4/3 index is not
    // the answer.
    f.desktop.remove_window(delta).reset();
    CK_CHECK(bar->page_count() == 3);
    CK_CHECK(bar->page() == 2);
    CK_CHECK(bar->page_index_text() == "3/3");
    CK_CHECK(bar->drawn_entries().size() == 1);
    // Active, because the desktop moved the reader on to it when Delta went.
    CK_CHECK(bar->drawn_entries()[0].text ==
             entry_text(WindowSwitcherBar::Status::Active, "Charlie"));
}

CK_TEST(closing_a_window_on_an_earlier_page_leaves_the_reader_where_they_were) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar(20);
    Window* alpha = f.open("Alpha");
    f.open("Bravo");
    f.open("Charlie");
    f.open("Delta");
    CK_CHECK(bar->set_page(2));

    f.desktop.remove_window(alpha).reset();
    CK_CHECK(bar->page_count() == 3);
    CK_CHECK(bar->page() == 2);
}

// --- The collapse toggle --------------------------------------------------

CK_TEST(the_bar_reports_a_collapse_and_hides_nothing_itself) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    f.open("Alpha");
    // No toggle until a host asks for one, so a bar that never collapses does
    // not pay a column for the possibility.
    CK_CHECK(bar->chrome().collapse_x == -1);
    CK_CHECK(bar->drawn_entries()[0].x == 0);

    bar->set_collapsible(true);
    CK_CHECK(bar->chrome().collapse_x == 0);
    CK_CHECK(bar->drawn_entries()[0].x == 2);

    int reports = 0;
    bool state = false;
    bar->on_collapse_changed = [&](bool collapsed) {
        state = collapsed;
        ++reports;
    };
    CK_CHECK(bar->on_mouse(press(0)));
    CK_CHECK(reports == 1 && state);
    CK_CHECK(bar->collapsed());
    // What collapsing MEANS is the host's — WP-35 in ckmux hides the footer
    // below this row. The bar itself hid nothing, moved nothing, and is still
    // listing the same window in the same column.
    CK_CHECK(bar->visible());
    CK_CHECK(bar->entries().size() == 1);
    CK_CHECK(bar->drawn_entries().size() == 1);
    CK_CHECK(bar->drawn_entries()[0].x == 2);
    CK_CHECK(f.desktop.windows().size() == 1);

    CK_CHECK(bar->on_mouse(press(0)));
    CK_CHECK(reports == 2 && !state);
    CK_CHECK(!bar->collapsed());
}

CK_TEST(the_bar_asks_for_exactly_one_row) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    CK_CHECK((bar->vertical_size_hint() == ui::SizeHint{1, 1, 1}));
}

// --- Docked over a status line -------------------------------------------

CK_TEST(a_bar_stacked_over_a_status_line_reserves_both_rows_from_a_maximized_window) {
    Fixture f;
    auto stack = std::make_unique<ui::Column>();
    stack->add_item(std::make_unique<WindowSwitcherBar>(f.desktop));
    stack->add_item(std::make_unique<StatusLine>());
    f.desktop.dock_bottom(std::move(stack));

    // Column's vertical hint sums its children, so the reserved height comes
    // out right with no second dock slot and no change to the dock API.
    CK_CHECK((f.desktop.content_area() == Rect{0, 0, 80, 22}));

    Window* window = f.open("Doc");
    f.app.execute_command(f.app.commands().standard().zoom);
    CK_CHECK(window->zoomed());
    CK_CHECK((window->bounds() == Rect{0, 0, 80, 22}));
}

// --- The observation seam -------------------------------------------------

CK_TEST(a_window_change_observer_hears_additions_activations_removals_and_renames) {
    Fixture f;
    std::vector<Desktop::WindowChange> heard;
    const auto id = f.desktop.subscribe_window_change(
        [&](Desktop::WindowChange change, Window&) { heard.push_back(change); });

    Window* alpha = f.open("Alpha");
    // Added is reported last, after the activation the addition itself caused.
    CK_CHECK(heard.size() == 2);
    CK_CHECK(heard[0] == Desktop::WindowChange::Activated);
    CK_CHECK(heard[1] == Desktop::WindowChange::Added);

    heard.clear();
    alpha->set_title("Renamed");
    CK_CHECK(heard.size() == 1 && heard[0] == Desktop::WindowChange::TitleChanged);

    heard.clear();
    f.desktop.remove_window(alpha).reset();
    CK_CHECK(heard.size() == 1 && heard[0] == Desktop::WindowChange::Removed);

    f.desktop.unsubscribe_window_change(id);
    heard.clear();
    f.open("Bravo");
    CK_CHECK(heard.empty());
}

CK_TEST(a_lifetime_bound_observer_stops_being_called_once_its_owner_is_gone) {
    Fixture f;
    int calls = 0;
    {
        auto owner = std::make_shared<int>(0);
        f.desktop.subscribe_window_change([&](Desktop::WindowChange, Window&) { ++calls; },
                                          std::weak_ptr<void>(owner));
        f.open("Alpha");
        CK_CHECK(calls > 0);
    }
    const int after_owner_died = calls;
    f.open("Bravo");
    CK_CHECK(calls == after_owner_died);
}

CK_TEST(a_detached_and_destroyed_bar_never_hears_from_the_desktop_again) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    f.open("Alpha");
    CK_CHECK(bar->entries().size() == 1);
    f.desktop.remove_child(bar).reset();
    // The subscription was bound to the bar's own lifetime and nothing had to
    // cancel it; the notification below must simply find nobody listening.
    f.open("Bravo");
    CK_CHECK(f.desktop.windows().size() == 2);
}

// --- Damped widths --------------------------------------------------------
//
// A window's title is not a stable string — a shell rewrites its caption at
// every prompt — and an undamped taskbar re-sizes the button and re-flows
// every button beside it each time. These pin the rule that stops that: at
// most one widening per grow delay and one narrowing per shrink delay, both
// measured from the entry's last width change.

namespace {

// The delays ckmux runs with, which are also the ones these tests reason in:
// a second before a button may get wider, half a minute before it may get
// narrower.
constexpr std::int64_t kGrow = 1'000'000'000;
constexpr std::int64_t kShrink = 30'000'000'000;

// The cells one entry's box occupies, padding included — what a reader sees
// as the length of the button, and the number damping is about.
int drawn_width(const WindowSwitcherBar& bar, std::size_t index) {
    for (const WindowSwitcherBar::DrawnEntry& drawn : bar.drawn_entries())
        if (drawn.index == index) return drawn.width;
    return -1;
}

std::string drawn_text(const WindowSwitcherBar& bar, std::size_t index) {
    for (const WindowSwitcherBar::DrawnEntry& drawn : bar.drawn_entries())
        if (drawn.index == index) return drawn.text;
    return {};
}

}  // namespace

CK_TEST(an_undamped_bar_follows_every_title_change_at_its_natural_width) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    const int narrow = drawn_width(*bar, 0);

    // No damping is the default, and it is what every existing consumer gets:
    // the box follows the name in the same frame, with no clock involved.
    alpha->set_title("Alpha, at considerably greater length");
    CK_CHECK(drawn_width(*bar, 0) > narrow);
    alpha->set_title("A");
    CK_CHECK(drawn_width(*bar, 0) < narrow);
}

CK_TEST(a_window_the_bar_has_never_measured_takes_its_width_at_once) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    bar->set_width_damping(kGrow, kShrink);

    // Opened under damping, and wide immediately: a row that has just
    // appeared has no previous width to flicker between, and growing it into
    // its own name would be the flicker damping exists to remove.
    Window* alpha = f.open("A window with a long name");
    CK_CHECK(drawn_width(*bar, 0) >= ckv::text::text_width(alpha->title()));
}

CK_TEST(a_lengthening_title_shows_at_once_but_its_button_waits_out_the_grow_delay) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    bar->set_width_damping(kGrow, kShrink);
    Window* alpha = f.open("Alpha");
    const int settled = drawn_width(*bar, 0);

    alpha->set_title("Alpha — building target 17 of 300");
    // The NAME is current; only the box is held. A bar that also held the
    // label would be showing the reader a caption that is no longer true.
    CK_CHECK(bar->entries()[0].label == "Alpha — building target 17 of 300");
    CK_CHECK(drawn_width(*bar, 0) == settled);
    // And the name it cannot fit is elided into the box it has, rather than
    // spilling over the entry beside it.
    CK_CHECK(drawn_text(*bar, 0) != bar->entries()[0].label);

    // Not yet.
    f.clock.advance(kGrow / 2);
    f.app.step(0);
    CK_CHECK(drawn_width(*bar, 0) == settled);

    // The positive half: the delay is a delay, not a refusal.
    f.clock.advance(kGrow / 2);
    f.app.step(0);
    CK_CHECK(drawn_width(*bar, 0) > settled);
    CK_CHECK(drawn_text(*bar, 0) ==
             entry_text(WindowSwitcherBar::Status::Active, bar->entries()[0].label));
}

CK_TEST(a_deferred_widening_arrives_with_no_further_title_change_to_carry_it) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    bar->set_width_damping(kGrow, kShrink);
    Window* alpha = f.open("Alpha");
    const int settled = drawn_width(*bar, 0);

    // One title change and then silence, which is the ordinary case: a
    // program renames its window once and says nothing more. Nothing else
    // re-reads the widths, so a bar that did not wake itself would leave this
    // name elided into its old box for the rest of the session.
    alpha->set_title("Alpha — a considerably longer caption");
    CK_CHECK(drawn_width(*bar, 0) == settled);
    f.clock.advance(kGrow);
    f.app.step(0);
    CK_CHECK(drawn_width(*bar, 0) > settled);
}

CK_TEST(a_shortening_title_keeps_its_button_width_for_the_longer_shrink_delay) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    bar->set_width_damping(kGrow, kShrink);
    Window* alpha = f.open("Alpha — building target 17 of 300");
    const int settled = drawn_width(*bar, 0);

    alpha->set_title("Alpha");
    CK_CHECK(drawn_width(*bar, 0) == settled);
    // Past the grow delay, and still wide: the two directions are not the
    // same wait, which is the whole point of there being two numbers. A row
    // that is too WIDE costs the reader nothing and is very often about to be
    // needed again.
    f.clock.advance(kGrow * 2);
    f.app.step(0);
    CK_CHECK(drawn_width(*bar, 0) == settled);

    f.clock.advance(kShrink);
    f.app.step(0);
    CK_CHECK(drawn_width(*bar, 0) < settled);
}

CK_TEST(a_title_rewritten_every_frame_moves_the_button_at_most_once_per_grow_delay) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    bar->set_width_damping(kGrow, kShrink);
    Window* alpha = f.open("Alpha");

    // The case this exists for: a build tool writing its progress into the
    // caption, one frame at a time. Sixty rewrites over the grow delay must
    // cost the layout one step, not sixty.
    int widths_seen = 0;
    int previous = drawn_width(*bar, 0);
    for (int frame = 1; frame <= 60; ++frame) {
        alpha->set_title("Alpha — target " + std::to_string(frame) + " of 300");
        f.clock.advance(kGrow / 60);
        f.app.step(0);
        const int now = drawn_width(*bar, 0);
        if (now != previous) ++widths_seen;
        previous = now;
    }
    CK_CHECK(widths_seen <= 1);
}

CK_TEST(a_reader_who_renames_a_window_is_not_made_to_wait_for_the_delay) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    bar->set_width_damping(kGrow, kShrink);
    Window* alpha = f.open("Alpha — building target 17 of 300");
    const int settled = drawn_width(*bar, 0);

    // Damping absorbs what a PROGRAM does to a caption. A reader who has just
    // renamed the window is not flicker, and a rename that visibly took
    // effect half a minute later reads as a command that did not work — so
    // the host says which kind of change this was.
    alpha->set_title("Alpha");
    CK_CHECK(drawn_width(*bar, 0) == settled);
    bar->settle_width(*alpha);
    CK_CHECK(drawn_width(*bar, 0) < settled);
}

CK_TEST(turning_damping_off_releases_every_width_it_was_holding) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    bar->set_width_damping(kGrow, kShrink);
    Window* alpha = f.open("Alpha — building target 17 of 300");
    const int settled = drawn_width(*bar, 0);
    alpha->set_title("Alpha");
    CK_CHECK(drawn_width(*bar, 0) == settled);

    // A setting has to take effect when the host changes it. Leaving a held
    // box in place until its old delay elapsed would make it take effect at a
    // moment nobody chose.
    bar->set_width_damping(0, 0);
    CK_CHECK(drawn_width(*bar, 0) < settled);
}

CK_TEST(a_bar_destroyed_before_its_deferred_width_comes_due_leaves_nothing_to_fire_into) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    bar->set_width_damping(kGrow, kShrink);
    Window* alpha = f.open("Alpha");
    alpha->set_title("Alpha — a considerably longer caption");

    // The wake-up is a one-shot holding a liveness token rather than
    // something a destructor cancels; under ASan this is the case that says
    // whether that is true.
    f.desktop.remove_child(bar).reset();
    f.clock.advance(kGrow * 2);
    f.app.step(0);
    CK_CHECK(f.desktop.windows().size() == 1);
}

CK_TEST(a_deferred_width_holds_still_while_a_reader_has_a_button_pressed) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    bar->set_width_damping(kGrow, kShrink);
    Window* alpha = f.open("Alpha");
    Window* bravo = f.open("Bravo, with a name of its own");
    // Bravo is behind, not in front: this test is about WHICH window a
    // release names, and a click on the reader's own current window is the
    // one that puts it away rather than raising it (U4-j).
    f.desktop.activate(alpha);
    const int alpha_width = drawn_width(*bar, 0);
    const int bravo_column = click_column(*bar, 1);

    // A press on the SECOND entry, and then a caption change on the first
    // that would widen it and push everything after it sideways. PagedStrip
    // resolves the release by index, so a box that changed width under the
    // held pointer either loses the click or spends it on the wrong window.
    bar->on_mouse(press(bravo_column));
    alpha->set_title("Alpha — a considerably longer caption");
    f.clock.advance(kGrow * 2);
    f.app.step(0);
    CK_CHECK(drawn_width(*bar, 0) == alpha_width);
    CK_CHECK(click_column(*bar, 1) == bravo_column);

    // Released on the entry it started on, which still means Bravo.
    bar->on_mouse(release(bravo_column));
    CK_CHECK(topmost_window(f.desktop) == bravo);

    // And the change that was held is not lost: the press is over, so the
    // next wake-up applies it.
    f.clock.advance(kGrow * 2);
    f.app.step(0);
    CK_CHECK(drawn_width(*bar, 0) > alpha_width);
}

// --- Status icons and taskbar clicks (U4-j) -------------------------------
//
// The reader asked for two things that turn out to be one: a row that says
// which windows are where, and a row whose buttons behave like a taskbar's.
// They are the same feature because the icon is a promise about what the
// click will do — the active window's button is the one that puts it away,
// and a button showing a window that is put away is the one that brings it
// back.

namespace {

using Status = WindowSwitcherBar::Status;

// The click a reader makes: a press and a release on the same cell. Split in
// the widget, because a press that wanders off its entry takes the click
// back; joined here, because none of these tests is about that.
void click(WindowSwitcherBar& bar, std::size_t index) {
    const int x = click_column(bar, index);
    CK_CHECK(x >= 0);
    bar.on_mouse(press(x));
    bar.on_mouse(release(x));
}

}  // namespace

CK_TEST(each_row_says_whether_its_window_is_active_behind_or_put_away) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    f.open("Bravo");
    Window* charlie = f.open("Charlie");
    CK_CHECK(f.desktop.active_window() == charlie);

    alpha->set_minimized(true);
    CK_CHECK(bar->entries()[0].status() == Status::Minimized);
    CK_CHECK(bar->entries()[1].status() == Status::Visible);
    CK_CHECK(bar->entries()[2].status() == Status::Active);

    // And the three glyphs are on the row, not merely in the model: a state
    // a listing knows and does not draw is a state the reader does not have.
    const std::vector<WindowSwitcherBar::DrawnEntry> drawn = bar->drawn_entries();
    CK_CHECK(drawn[0].text == entry_text(Status::Minimized, "Alpha"));
    CK_CHECK(drawn[1].text == entry_text(Status::Visible, "Bravo"));
    CK_CHECK(drawn[2].text == entry_text(Status::Active, "Charlie"));
    // The mark says WHERE, not which: a window in front and one behind it
    // are both on the desktop and answer with the same shape, while one that
    // has been put away answers with another. Checked against each other
    // rather than against literals, so a later table is still held to the
    // distinction rather than to two particular characters.
    CK_CHECK(WindowSwitcherBar::status_glyph(Status::Active) ==
             WindowSwitcherBar::status_glyph(Status::Visible));
    CK_CHECK(WindowSwitcherBar::status_glyph(Status::Minimized) !=
             WindowSwitcherBar::status_glyph(Status::Active));
    // And the two shapes are the window frame's own controls, so a row and
    // the window it stands for wear the same chrome. Read off a drawn Window
    // rather than named here: a frame that re-lettered its controls and left
    // this bar behind is exactly the drift this pair exists to catch.
    // Wide enough that the frame draws its minimize control at all — see
    // Window::draws_minimize_control, which gives it up below 22 columns.
    constexpr int kFrameWidth = 24;
    Surface frame(ckv::Size{kFrameWidth, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter frame_painter(frame, Rect{0, 0, kFrameWidth, 5});
    charlie->set_bounds(Rect{0, 0, kFrameWidth, 5});
    charlie->draw(frame_painter);
    CK_CHECK(frame.at(Point{3, 0}).grapheme() ==
             WindowSwitcherBar::status_glyph(Status::Active));  // the close control's square
    CK_CHECK(frame.at(Point{kFrameWidth - 7, 0}).grapheme() ==
             WindowSwitcherBar::status_glyph(Status::Minimized));  // the minimize control's line
}

CK_TEST(the_active_rows_mark_wears_the_window_control_colour_and_every_other_row_does_not) {
    // The frame's own rule, on the bar: Window::draw lights the controls of
    // the ACTIVE window and lets every other window's fall back to the frame
    // style, and that — not a third shape — is what says which window the
    // reader is in. The label beside the mark stays the row's own style in
    // both cases, so this is a mark that is coloured, not a row that is.
    Fixture f;
    // A control colour the selected row is not already filled with. The
    // classic theme pairs both with green — see PagedStrip::Item::icon_role's
    // legibility floor, which is what a theme that collides gets instead, and
    // which would leave this test passing on the fallback while believing it
    // had checked the borrow.
    f.theme.set(f.roles.window_control,
                ckv::Style{ckv::Color::rgb(255, 255, 85), ckv::Color::rgb(0, 0, 170),
                           ckv::Attr::Bold});
    WindowSwitcherBar* bar = f.bar(40);
    f.open("Alpha");
    f.open("Bravo");
    CK_CHECK(bar->entries()[1].status() == Status::Active);

    Surface s(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 40, 1});
    bar->draw(painter);

    const ckv::Color control = f.theme.resolve(f.roles.window_control).fg;
    const auto mark_at = [&](std::size_t index) {
        for (const WindowSwitcherBar::DrawnEntry& drawn : bar->drawn_entries())
            if (drawn.index == index) return drawn.x + PagedStrip::kItemPadding;
        return -1;
    };
    const int behind = mark_at(0);
    const int in_front = mark_at(1);
    CK_CHECK(behind >= 0 && in_front >= 0);
    // The mark is where the test says it is, on both rows, before any claim
    // about its colour: a colour assertion on a blank cell passes for the
    // wrong reason.
    CK_CHECK(s.at(Point{behind, 0}).grapheme() == WindowSwitcherBar::status_glyph(Status::Visible));
    CK_CHECK(s.at(Point{in_front, 0}).grapheme() == WindowSwitcherBar::status_glyph(Status::Active));

    CK_CHECK(s.at(Point{in_front, 0}).style().fg == control);
    CK_CHECK(s.at(Point{behind, 0}).style().fg != control);
    // The mark takes the control's colour, never its background: the row's
    // highlight has to run unbroken underneath it, or the mark reads as a
    // hole punched in the button rather than as a lit control on it.
    CK_CHECK(s.at(Point{in_front, 0}).style().bg == s.at(Point{in_front + 2, 0}).style().bg);
    // The name beside it is the row's own style either way.
    CK_CHECK(s.at(Point{behind + 2, 0}).style().fg == s.at(Point{behind + 3, 0}).style().fg);
    CK_CHECK(s.at(Point{behind, 0}).style().fg == s.at(Point{behind + 2, 0}).style().fg);
}

CK_TEST(every_status_glyph_is_one_cell_so_a_state_change_never_re_flows_the_row) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    f.open("Bravo");

    // The property that lets the icons ride along with damped widths (U4-m):
    // a window changing state must not change the LENGTH of its button, or
    // every minimize and restore would slide the row sideways — the exact
    // jitter the damping exists to remove.
    const int alpha_box = bar->drawn_entries()[0].width;
    for (const Status status : {Status::Active, Status::Visible, Status::Minimized})
        CK_CHECK(ckv::text::text_width(WindowSwitcherBar::status_glyph(status)) == 1);

    f.desktop.activate(alpha);
    CK_CHECK(bar->drawn_entries()[0].width == alpha_box);
    alpha->set_minimized(true);
    CK_CHECK(bar->drawn_entries()[0].width == alpha_box);
    alpha->set_minimized(false);
    CK_CHECK(bar->drawn_entries()[0].width == alpha_box);
}

CK_TEST(clicking_the_row_of_the_window_the_reader_is_in_puts_it_away) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    Window* bravo = f.open("Bravo");
    CK_CHECK(f.desktop.active_window() == bravo);

    click(*bar, 1);
    CK_CHECK(bravo->minimized());
    CK_CHECK(!bravo->visible());
    // The reader is left somewhere: a desktop whose active window has just
    // gone hands attention to whatever is still shown.
    CK_CHECK(f.desktop.active_window() == alpha);
    // Still listed — that row is now the only way back to it.
    CK_CHECK(bar->entries().size() == 2);
    CK_CHECK(bar->entries()[1].status() == Status::Minimized);
}

CK_TEST(clicking_a_window_that_is_merely_behind_brings_it_forward) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    Window* bravo = f.open("Bravo");
    CK_CHECK(f.desktop.active_window() == bravo);

    click(*bar, 0);
    CK_CHECK(f.desktop.active_window() == alpha);
    CK_CHECK(topmost_window(f.desktop) == alpha);
    CK_CHECK(!alpha->minimized());
    CK_CHECK(bar->entries()[0].status() == Status::Active);
    CK_CHECK(bar->entries()[1].status() == Status::Visible);
}

CK_TEST(clicking_a_window_that_was_put_away_brings_it_back) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    f.open("Bravo");
    alpha->set_minimized(true);
    CK_CHECK(bar->entries()[0].status() == Status::Minimized);

    click(*bar, 0);
    // Back on the desktop AND in front of the reader: naming a hidden window
    // from the one row that still lists it is asking to work in it, not
    // asking for it to exist somewhere behind everything else.
    CK_CHECK(!alpha->minimized());
    CK_CHECK(alpha->visible());
    CK_CHECK(f.desktop.active_window() == alpha);
    CK_CHECK(topmost_window(f.desktop) == alpha);
    CK_CHECK(bar->entries()[0].status() == Status::Active);
}

CK_TEST(the_three_transitions_walk_one_window_all_the_way_round) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    f.open("Bravo");

    // Behind → in front → away → back, driven only by clicks on one row.
    click(*bar, 0);
    CK_CHECK(bar->entries()[0].status() == Status::Active);
    click(*bar, 0);
    CK_CHECK(bar->entries()[0].status() == Status::Minimized);
    click(*bar, 0);
    CK_CHECK(bar->entries()[0].status() == Status::Active);
    CK_CHECK(f.desktop.active_window() == alpha);
}

CK_TEST(a_window_that_draws_no_minimize_control_is_not_put_away_by_its_row) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    f.open("Alpha");
    Window* fixed = f.open("An alert");
    fixed->set_minimizable(false);
    CK_CHECK(f.desktop.active_window() == fixed);

    // Its own frame offers no `_`; the bar must not be a second route past
    // the gate the window set.
    click(*bar, 1);
    CK_CHECK(!fixed->minimized());
    CK_CHECK(f.desktop.active_window() == fixed);
    CK_CHECK(bar->entries()[1].status() == Status::Active);
}

CK_TEST(a_right_press_on_a_row_changes_no_window_state) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    f.open("Alpha");
    Window* bravo = f.open("Bravo");
    bar->set_context_menu_provider([](const WindowSwitcherTarget& target) {
        return std::vector<MenuItem>{MenuItem::action("&Close", target.bind([](Window&) {}))};
    });

    // The active row, whose LEFT click would minimize: the two buttons must
    // not be two ways to ask for the same thing.
    CK_CHECK(bar->on_mouse(press(click_column(*bar, 1), ckv::MouseButton::Right)));
    CK_CHECK(f.desktop.popups().size() == 1);
    CK_CHECK(!bravo->minimized());
    CK_CHECK(f.desktop.active_window() == bravo);
    CK_CHECK(bar->entries()[1].status() == Status::Active);
}

CK_TEST(a_host_may_answer_put_away_and_what_putting_away_means_itself) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    f.open("Bravo");

    // A host whose windows are put away by a model of its own — a detached
    // session, a document closed but still listed — says so through the
    // provider, and the icon and the click follow that answer rather than
    // the framework's.
    bool parked = false;
    bar->set_minimized_provider([&](Window& window) { return &window == alpha && parked; });
    Window* asked_to_park = nullptr;
    bar->set_minimize_action([&](Window& window) { asked_to_park = &window; parked = true; });

    f.desktop.activate(alpha);
    CK_CHECK(bar->entries()[0].status() == Status::Active);
    click(*bar, 0);
    // The host's verb ran, and the framework's did not: the window itself is
    // untouched, which is the whole point of the seam.
    CK_CHECK(asked_to_park == alpha);
    CK_CHECK(!alpha->minimized());
    bar->refresh();
    CK_CHECK(bar->entries()[0].status() == Status::Minimized);
}

CK_TEST(a_row_that_is_told_it_is_both_active_and_put_away_reads_as_put_away) {
    Fixture f;
    WindowSwitcherBar* bar = f.bar();
    Window* alpha = f.open("Alpha");
    // A Desktop never produces this — it moves activation on as a window
    // goes — but a host's own providers can, and the row has to resolve it
    // the same way every time rather than drawing a window as the reader's
    // current one while it is nowhere on screen.
    bar->set_minimized_provider([](Window&) { return true; });
    bar->set_active_provider([alpha] { return alpha; });

    CK_CHECK(bar->entries()[0].active);
    CK_CHECK(bar->entries()[0].minimized);
    CK_CHECK(bar->entries()[0].status() == Status::Minimized);
    CK_CHECK(bar->drawn_entries()[0].text == entry_text(Status::Minimized, "Alpha"));
}
