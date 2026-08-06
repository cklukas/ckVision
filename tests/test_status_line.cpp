// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/status_line.hpp"

#include "cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::ManualClock;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::Application;
using ckv::ui::CommandId;
using ckv::ui::FocusPolicy;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::ui::View;
using ckv::widgets::StatusLine;
using ckv::widgets::StatusLineItem;
namespace ui = ckv::ui;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

std::string row_text(const Surface& s, int y) {
    std::string out;
    for (int x = 0; x < s.size().width; ++x) out += s.at(ckv::Point{x, y}).grapheme();
    return out;
}
}  // namespace

// --- Hint text ------------------------------------------------------------

CK_TEST(no_hint_provider_reports_an_empty_current_hint) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    CK_CHECK(status.current_hint().empty());
}

CK_TEST(no_focused_view_reports_an_empty_hint_even_with_a_provider_installed) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    status.set_hint_provider([](const std::string&) { return "should not appear"; });
    CK_CHECK(status.current_hint().empty());
}

CK_TEST(focused_view_with_no_help_context_key_reports_an_empty_hint) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    status.set_hint_provider([](const std::string&) { return "should not appear"; });
    auto* view = app.root().add_child(std::make_unique<View>());
    view->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(view);
    CK_CHECK(status.current_hint().empty());
}

CK_TEST(hint_resolves_through_the_focused_views_help_context_key) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    status.set_hint_provider([](const std::string& key) { return "Hint for " + key; });
    auto* view = app.root().add_child(std::make_unique<View>());
    view->set_focus_policy(FocusPolicy::TabStop);
    view->set_help_context_key("topic.a");
    app.set_focus(view);
    CK_CHECK(status.current_hint() == "Hint for topic.a");
}

CK_TEST(hint_updates_when_focus_moves_to_a_different_view) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    status.set_hint_provider([](const std::string& key) { return key; });
    auto* a = app.root().add_child(std::make_unique<View>());
    a->set_focus_policy(FocusPolicy::TabStop);
    a->set_help_context_key("a");
    auto* b = app.root().add_child(std::make_unique<View>());
    b->set_focus_policy(FocusPolicy::TabStop);
    b->set_help_context_key("b");

    app.set_focus(a);
    CK_CHECK(status.current_hint() == "a");
    app.set_focus(b);
    CK_CHECK(status.current_hint() == "b");
}

// --- Items -----------------------------------------------------------

CK_TEST(clicking_an_items_label_fires_its_command) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    status.set_bounds(Rect{0, 0, 40, 1});
    constexpr CommandId kHelp = 1;
    app.commands().register_command(kHelp, "Help");
    bool ran = false;
    app.set_command_handler(kHelp, [&] { ran = true; });
    status.set_items({StatusLineItem{"Help", kHelp}});

    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 0},
                                              std::nullopt, Modifier::None}));
    CK_CHECK(ran);
}

CK_TEST(clicking_outside_any_item_is_unhandled) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    status.set_bounds(Rect{0, 0, 40, 1});
    status.set_items({StatusLineItem{"Help", ui::kInvalidCommand}});
    CK_CHECK(!status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{30, 0},
                                               std::nullopt, Modifier::None}));
}

CK_TEST(a_disabled_commands_item_still_fires_execute_command_which_itself_declines) {
    // StatusLine doesn't special-case enablement — execute_command
    // already refuses a disabled command, so clicking a disabled
    // item's label is a harmless no-op rather than StatusLine needing
    // its own enablement check.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    status.set_bounds(Rect{0, 0, 40, 1});
    constexpr CommandId kHelp = 1;
    app.commands().register_command(kHelp, "Help");
    app.commands().set_enabled_predicate(kHelp, [] { return false; });
    bool ran = false;
    app.set_command_handler(kHelp, [&] { ran = true; });
    status.set_items({StatusLineItem{"Help", kHelp}});

    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 0},
                                              std::nullopt, Modifier::None}));
    CK_CHECK(!ran);
}

CK_TEST(second_item_is_positioned_after_the_first_with_a_separator_between) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    status.set_bounds(Rect{0, 0, 40, 1});
    constexpr CommandId kFirst = 1;
    constexpr CommandId kSecond = 2;
    app.commands().register_command(kFirst, "First");
    app.commands().register_command(kSecond, "Second");
    bool second_ran = false;
    app.set_command_handler(kSecond, [&] { second_ran = true; });
    status.set_items({StatusLineItem{"AB", kFirst}, StatusLineItem{"Second", kSecond}});

    // "AB" occupies columns 0-1; " | " occupies 2-4; "Second" starts at 5.
    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{6, 0},
                                              std::nullopt, Modifier::None}));
    CK_CHECK(second_ran);
}

CK_TEST(draw_renders_the_hint_and_items_without_crashing) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    status.set_bounds(Rect{0, 0, 40, 1});
    status.set_hint_provider([](const std::string& key) { return key; });
    auto* view = app.root().add_child(std::make_unique<View>());
    view->set_focus_policy(FocusPolicy::TabStop);
    view->set_help_context_key("Ready");
    app.set_focus(view);
    status.set_items({StatusLineItem{"Help", ui::kInvalidCommand}});

    Surface s(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 40, 1});
    status.draw(painter);
    CK_CHECK(row_text(s, 0).substr(0, 5) == "Ready");
}

CK_TEST(draw_stops_placing_items_that_would_start_past_the_available_width_without_crashing) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status(f.theme, f.roles.dialog_background, app);
    status.set_bounds(Rect{0, 0, 5, 1});  // very narrow
    status.set_items({StatusLineItem{"This label is far too long to fit", ui::kInvalidCommand}});

    Surface s(ckv::Size{5, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 5, 1});
    status.draw(painter);  // must not crash even though the item overflows the bounds
    CK_CHECK(true);
}
