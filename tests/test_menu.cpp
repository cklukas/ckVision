// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/menu.hpp"

#include "cvision/core/text.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/window.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::Application;
using ckv::ui::CommandId;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Desktop;
using ckv::widgets::DropdownMenu;
using ckv::widgets::MenuBar;
using ckv::widgets::MenuBarItem;
using ckv::widgets::MenuMark;
using ckv::widgets::MenuItem;
using ckv::widgets::is_keyboard_context_menu_request;
using ckv::widgets::show_context_menu;
using ckv::widgets::show_context_menu_for_focus;
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
};

std::unique_ptr<DropdownMenu> make_dropdown(Fixture& f, ckv::ui::Application& app,
                                             std::vector<MenuItem> items) {
    auto menu = std::make_unique<DropdownMenu>(std::move(items));
    menu->set_context(ui::Context{&f.theme, &f.registry, &app});
    return menu;
}

ckv::KeyEvent key(ckv::Key k, std::string text = "") {
    return ckv::KeyEvent{KeyChord{k, Modifier::None, std::move(text)}};
}

ckv::KeyEvent key(ckv::Key k, Modifier modifiers) {
    return ckv::KeyEvent{KeyChord{k, modifiers, ""}};
}

std::string row_text(const Surface& s, int y) {
    std::string out;
    for (int x = 0; x < s.size().width; ++x) out += s.at(ckv::Point{x, y}).grapheme();
    return out;
}
}  // namespace

// --- DropdownMenu: navigation --------------------------------------------

CK_TEST(dropdown_highlights_the_first_item_by_default) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, {MenuItem::action("One", {}),
                                        MenuItem::action("Two", {})});
    CK_CHECK(menu->highlighted() == 0);
}

CK_TEST(a_dropdown_opens_on_its_first_row_even_when_that_row_is_unavailable) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId disabled_command = app.commands().declare(
        {.key = "test.disabled", .title = "Disabled"});
    app.commands().set_enabled_predicate(disabled_command, [] { return false; });
    auto menu = make_dropdown(f, app, {MenuItem::command(disabled_command),
                                        MenuItem::action("Two", {})});
    // A grey verb is reachable: a reader who cannot stand on it cannot be
    // told why it is grey, and that is the question a grey verb provokes.
    CK_CHECK(menu->highlighted() == 0);
    CK_CHECK(!menu->highlight().enabled);
}

CK_TEST(down_arrow_moves_to_the_next_item_and_wraps) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, {MenuItem::action("One", {}),
                                        MenuItem::action("Two", {})});
    menu->on_key(key(Key::Down));
    CK_CHECK(menu->highlighted() == 1);
    menu->on_key(key(Key::Down));
    CK_CHECK(menu->highlighted() == 0);  // wrapped
}

CK_TEST(up_arrow_wraps_backward_from_the_first_item) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, {MenuItem::action("One", {}),
                                        MenuItem::action("Two", {})});
    menu->on_key(key(Key::Up));
    CK_CHECK(menu->highlighted() == 1);
}

CK_TEST(navigation_skips_separators_but_stops_on_unavailable_rows) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId disabled_command = app.commands().declare(
        {.key = "test.disabled", .title = "Disabled"});
    app.commands().set_enabled_predicate(disabled_command, [] { return false; });
    auto menu = make_dropdown(f, app,
                               {MenuItem::action("One", {}),
                                MenuItem::separator(),  // separator
                                MenuItem::command(disabled_command),
                                MenuItem::action("Four", {})});
    CK_CHECK(menu->highlighted() == 0);
    menu->on_key(key(Key::Down));
    // Over the separator — which is scenery — and onto the unavailable
    // row, which is a place the reader may stand and read about.
    CK_CHECK(menu->highlighted() == 2);
    CK_CHECK(!menu->highlight().enabled);
    menu->on_key(key(Key::Down));
    CK_CHECK(menu->highlighted() == 3);
}

CK_TEST(a_dropdown_with_every_item_disabled_or_a_separator_has_no_selection) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, {MenuItem::separator()});
    CK_CHECK(menu->highlighted() == -1);
    // Enter is still consumed (the popup owns the key while open) even
    // though there's nothing to activate — it must not crash.
    CK_CHECK(menu->on_key(key(Key::Enter)));
}

// --- DropdownMenu: activation --------------------------------------------

CK_TEST(enter_activates_the_highlighted_items_command) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId save_command = app.commands().declare(
        {.key = "test.save", .title = "Save"});
    bool ran = false;
    app.set_command_handler(save_command, [&] { ran = true; });
    bool dismissed = false;
    auto reason = ckv::widgets::MenuDismissReason::Cancelled;
    auto menu = make_dropdown(f, app, {MenuItem::command(save_command)});
    menu->on_dismiss = [&](ckv::widgets::MenuDismissReason r) {
        dismissed = true;
        reason = r;
    };
    menu->on_key(key(Key::Enter));
    CK_CHECK(ran);
    CK_CHECK(dismissed);
    CK_CHECK(reason == ckv::widgets::MenuDismissReason::ItemChosen);
}

CK_TEST(enter_activates_a_plain_on_activate_item_when_no_command_is_bound) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    auto menu = make_dropdown(f, app, {MenuItem::action("&Item", [&] { ran = true; })});
    menu->on_key(key(Key::Enter));
    CK_CHECK(ran);
}

CK_TEST(mnemonic_key_activates_the_matching_item_directly) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    auto menu = make_dropdown(f, app, {MenuItem::action("&One", {}),
                                        MenuItem::action("&Two", [&] { ran = true; })});
    CK_CHECK(menu->on_key(key(Key::Char, "t")));  // case-insensitive
    CK_CHECK(ran);
}

CK_TEST(escape_dismisses_without_activating_anything) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    bool dismissed = false;
    auto reason = ckv::widgets::MenuDismissReason::ItemChosen;
    auto menu = make_dropdown(f, app, {MenuItem::action("&Item", [&] { ran = true; })});
    menu->on_dismiss = [&](ckv::widgets::MenuDismissReason r) {
        dismissed = true;
        reason = r;
    };
    menu->on_key(key(Key::Escape));
    CK_CHECK(!ran);
    CK_CHECK(dismissed);
    CK_CHECK(reason == ckv::widgets::MenuDismissReason::Cancelled);
}

CK_TEST(clicking_a_disabled_item_does_not_activate_it) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId disabled_command = app.commands().declare(
        {.key = "test.disabled", .title = "Disabled"});
    app.commands().set_enabled_predicate(disabled_command, [] { return false; });
    bool ran = false;
    app.set_command_handler(disabled_command, [&] { ran = true; });
    auto menu = make_dropdown(f, app, {MenuItem::command(disabled_command)});
    menu->set_bounds(Rect{5, 5, 20, 1});
    menu->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{6, 5},
                                    std::nullopt, Modifier::None});
    CK_CHECK(!ran);
}

CK_TEST(mouse_activation_is_deferred_until_release_and_tracks_dragged_item) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool first = false;
    bool second = false;
    auto menu = make_dropdown(f, app, {MenuItem::action("First", [&] { first = true; }),
                                        MenuItem::action("Second", [&] { second = true; })});
    menu->set_bounds(Rect{5, 5, 14, 2});
    menu->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{6, 5},
                                   std::nullopt, Modifier::None});
    CK_CHECK(!first && !second);
    menu->on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{6, 6},
                                   std::nullopt, Modifier::None});
    CK_CHECK(menu->highlighted() == 1);
    menu->on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{6, 6},
                                   std::nullopt, Modifier::None});
    CK_CHECK(!first && second);
}

CK_TEST(mouse_release_on_a_divider_cancels_the_pending_activation) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    auto menu = make_dropdown(f, app, {MenuItem::action("Run", [&] { ran = true; }),
                                        MenuItem::separator(),
                                        MenuItem::action("Other", {})});
    menu->set_bounds(Rect{5, 5, 14, 3});
    menu->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{6, 5},
                                   std::nullopt, Modifier::None});
    menu->on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{6, 6},
                                   std::nullopt, Modifier::None});
    menu->on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{6, 6},
                                   std::nullopt, Modifier::None});
    CK_CHECK(!ran);
}

CK_TEST(clicking_outside_the_dropdowns_bounds_dismisses_it_without_activating) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    bool dismissed = false;
    auto menu = make_dropdown(f, app, {MenuItem::action("&Item", [&] { ran = true; })});
    menu->set_bounds(Rect{5, 5, 20, 1});
    auto reason = ckv::widgets::MenuDismissReason::ItemChosen;
    menu->on_dismiss = [&](ckv::widgets::MenuDismissReason r) {
        dismissed = true;
        reason = r;
    };
    // Far outside the dropdown's bounds — this only reaches the
    // dropdown at all because Application routes it here via input
    // capture (exercised end-to-end in the MenuBar tests below). The
    // dismissal consumes the press: it is the light dismiss, not a click
    // on whatever lies beneath.
    CK_CHECK(menu->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                             ckv::Point{50, 50}, std::nullopt, Modifier::None}));
    CK_CHECK(dismissed);
    CK_CHECK(reason == ckv::widgets::MenuDismissReason::Outside);
    CK_CHECK(!ran);
}

// --- DropdownMenu: registry-rendered title and chord (M9/WP-11) -----------

CK_TEST(an_item_referencing_a_command_renders_the_registered_title_not_its_own_label) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId save_command = app.commands().declare(
        {.key = "test.save", .title = "&Save"});
    // The label below ("Ignored") must never appear anywhere — an item
    // referencing a command carries no label text of its own.
    auto menu = make_dropdown(f, app, {MenuItem::command(save_command)});
    menu->set_bounds(Rect{0, 0, 20, 1});

    Surface s(ckv::Size{20, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 1});
    menu->draw(painter);

    const std::string row = row_text(s, 0);
    CK_CHECK(row.find("Save") != std::string::npos);
    CK_CHECK(row.find("Ignored") == std::string::npos);
}

CK_TEST(an_item_referencing_a_command_with_no_chord_bound_shows_no_chord_hint) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId save_command = app.commands().declare(
        {.key = "test.save", .title = "&Save"});  // no default_chord
    auto menu = make_dropdown(f, app, {MenuItem::command(save_command)});
    menu->set_bounds(Rect{0, 0, 20, 1});

    Surface s(ckv::Size{20, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 1});
    menu->draw(painter);

    // "Save" at column 1 (a 1-column left margin), nothing else on the
    // row — no stray chord text past it.
    CK_CHECK(row_text(s, 0) == " Save" + std::string(15, ' '));
}

CK_TEST(an_item_referencing_a_command_with_a_bound_chord_shows_it_right_aligned) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId save_command = app.commands().declare(
        {.key = "test.save", .title = "&Save", .category = "File", .chord = "Ctrl+S"});
    auto menu = make_dropdown(f, app, {MenuItem::command(save_command)});
    menu->set_bounds(Rect{0, 0, 20, 1});

    Surface s(ckv::Size{20, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 1});
    menu->draw(painter);

    const std::string row = row_text(s, 0);
    CK_CHECK(row.find("Save") != std::string::npos);
    // format(Ctrl+S) upper-cases the single-letter Char chord for display.
    const std::string hint = "Ctrl+S";
    const auto hint_pos = row.find(hint);
    CK_CHECK(hint_pos != std::string::npos);
    // Right-aligned: the hint's last character sits one cell in from
    // the dropdown's own right edge (column 19 of a 20-wide surface).
    CK_CHECK(hint_pos + hint.size() == 19);
}

CK_TEST(a_runtime_rebind_changes_the_rendered_chord_hint_without_touching_the_item) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId save_command = app.commands().declare(
        {.key = "test.save", .title = "&Save", .category = "File", .chord = "Ctrl+S"});
    auto menu = make_dropdown(f, app, {MenuItem::command(save_command)});
    menu->set_bounds(Rect{0, 0, 20, 1});

    app.commands().unbind_key(KeyChord{Key::Char, Modifier::Ctrl, "s"});
    app.commands().bind_key(KeyChord{Key::F5, Modifier::None, ""}, save_command);

    Surface s(ckv::Size{20, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 1});
    menu->draw(painter);  // same MenuItem, no re-declaration — just a re-render

    const std::string row = row_text(s, 0);
    CK_CHECK(row.find("F5") != std::string::npos);
    CK_CHECK(row.find("Ctrl+S") == std::string::npos);
}

CK_TEST(a_narrow_dropdown_clips_label_and_chord_columns_without_leaving_garbage) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId save_command = app.commands().declare(
        {.key = "test.save", .title = "&Very Long Save Command", .category = "File", .chord = "Ctrl+S"});
    auto menu = make_dropdown(f, app, {MenuItem::command(save_command)});
    menu->set_bounds(Rect{0, 0, 5, 1});

    Surface s(ckv::Size{5, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 5, 1});
    menu->draw(painter);

    CK_CHECK(row_text(s, 0).size() == 5);
    CK_CHECK(row_text(s, 0).find('.') == std::string::npos);
}

CK_TEST(mnemonic_navigation_uses_the_registered_title_not_the_items_own_label) {
    // The item's own label carries a DIFFERENT letter ('&Ignored') —
    // if mnemonic lookup used it instead of the registered title
    // ('&Save'), 'i' would activate this item and 's' would not.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    const CommandId save_command = app.commands().declare({.key = "test.save", .title = "&Save"});
    app.set_command_handler(save_command, [&] { ran = true; });
    auto menu = make_dropdown(f, app, {MenuItem::command(save_command)});

    CK_CHECK(!menu->on_key(key(Key::Char, "i")));
    CK_CHECK(!ran);
    CK_CHECK(menu->on_key(key(Key::Char, "s")));
    CK_CHECK(ran);
}

CK_TEST(checkable_items_render_a_stable_check_column) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const MenuItem checked = MenuItem::action("&Checked", [] {}).with_mark(MenuMark::Checked);
    const MenuItem unchecked =
        MenuItem::action("&Unchecked", [] {}).with_mark(MenuMark::Unchecked);
    auto menu = make_dropdown(f, app, {checked, unchecked});
    menu->set_bounds(Rect{0, 0, 16, 2});

    Surface s(ckv::Size{16, 2}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 16, 2});
    menu->draw(painter);

    CK_CHECK(row_text(s, 0).find(" x Checked") == 0);
    CK_CHECK(row_text(s, 1).find("   Unchecked") == 0);
}

// A toggle command's state lives in the registry, so a Command row shows it
// with no mark of its own -- the same answer a tool bar presenting the
// command reads -- and follows it as it changes.
CK_TEST(a_command_row_takes_its_check_mark_from_the_registrys_toggle_state) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool wrapping = true;
    const CommandId wrap = app.commands().declare({.key = "test.wrap", .title = "&Wrap"});
    const CommandId save = app.commands().declare({.key = "test.save", .title = "&Save"});
    app.commands().set_checked_predicate(wrap, [&] { return wrapping; });
    auto menu = make_dropdown(f, app, {MenuItem::command(wrap), MenuItem::command(save)});
    menu->set_bounds(Rect{0, 0, 16, 2});

    Surface s(ckv::Size{16, 2}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 16, 2});
    menu->draw(painter);
    CK_CHECK(row_text(s, 0).find(" x Wrap") == 0);
    // An ordinary command beside it keeps the column, blank.
    CK_CHECK(row_text(s, 1).find("   Save") == 0);

    wrapping = false;
    menu->draw(painter);
    CK_CHECK(row_text(s, 0).find("   Wrap") == 0);

    // Withdrawn from being a toggle, it needs no column at all.
    app.commands().set_checked_predicate(wrap, {});
    CK_CHECK(!app.commands().checked(wrap).has_value());
    menu->draw(painter);
    CK_CHECK(row_text(s, 0).find(" Wrap") == 0);
}

CK_TEST(a_menu_mark_provider_reflects_current_state_without_rebuilding_the_menu) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool selected = false;
    const MenuItem choice = MenuItem::action("&Scheme", [] {})
        .with_mark_provider([&selected] {
            return selected ? MenuMark::RadioOn : MenuMark::RadioOff;
        });
    auto menu = make_dropdown(f, app, {choice});
    menu->set_bounds(Rect{0, 0, 16, 1});
    Surface surface(ckv::Size{16, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 16, 1});
    menu->draw(painter);
    CK_CHECK(row_text(surface, 0).find("   Scheme") == 0);
    selected = true;
    menu->draw(painter);
    CK_CHECK(row_text(surface, 0).find(" \u2022 Scheme") == 0);
}

// --- MenuBar: activation, navigation, dismissal ---------------------------

namespace {
struct MenuBarFixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    Fixture f;
    Desktop desktop{Rect{0, 0, 80, 24}};

    MenuBarFixture() { desktop.set_context(ui::Context{&f.theme, &f.registry, &app}); }

    // A bar across the desktop's top row, as dock_top lays one out: a bar
    // with no width shows no titles, and would hide every one of them behind
    // its overflow title.
    MenuBar* add_bar(std::unique_ptr<MenuBar> bar) {
        auto* raw = static_cast<MenuBar*>(desktop.add_child(std::move(bar)));
        raw->set_bounds(Rect{0, 0, desktop.bounds().width, 1});
        return raw;
    }
};
}  // namespace

CK_TEST(activate_gives_the_bar_focus_and_deactivate_restores_the_previous_focus) {
    MenuBarFixture mf;
    auto* other = mf.app.root().add_child(std::make_unique<ckv::ui::View>());
    other->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    mf.app.set_focus(other);

    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));

    bar->activate();
    CK_CHECK(bar->active());
    CK_CHECK(mf.app.focused() == bar);

    bar->deactivate();
    CK_CHECK(!bar->active());
    CK_CHECK(mf.app.focused() == other);
}

// Choosing an item ends the menu interaction, not just the popup. The bar
// must be back out of the way — and focus back where the reader left it —
// BEFORE the command runs, because a command that opens a dialog captures
// whatever holds focus at that moment and restores it on close. A bar still
// focused here is what makes a dialog hand focus back to the menu, which
// then reappears highlighted over a window the reader was working in.
CK_TEST(choosing_an_item_hands_focus_back_before_the_command_runs) {
    MenuBarFixture mf;
    auto* other = mf.app.root().add_child(std::make_unique<ckv::ui::View>());
    other->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    mf.app.set_focus(other);

    const ui::CommandId about_command = mf.app.commands().declare(
        {.key = "test.about", .title = "About"});
    const ckv::ui::View* focused_when_command_ran = nullptr;
    bool ran = false;
    mf.app.set_command_handler(about_command, [&] {
        ran = true;
        focused_when_command_ran = mf.app.focused();
    });

    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&Help", {MenuItem::command(about_command)}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));

    bar->activate();
    mf.app.dispatch(key(Key::Down));   // open the dropdown
    mf.app.dispatch(key(Key::Enter));  // choose "About"

    CK_CHECK(ran);
    // The command saw the reader's focus, not the menu that launched it.
    CK_CHECK(focused_when_command_ran == other);
    CK_CHECK(!bar->active());
    CK_CHECK(mf.app.focused() == other);
}

CK_TEST(a_menu_bar_keeps_the_invoking_views_command_context_while_open) {
    MenuBarFixture mf;
    auto* editor = mf.app.root().add_child(std::make_unique<ckv::ui::View>());
    editor->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    editor->set_command_context("document");
    mf.app.set_focus(editor);

    const ui::CommandId save = mf.app.commands().declare(
        {.key = "test.contextual-save", .title = "Save", .scope = {.contexts = {"document"}}});
    bool ran = false;
    mf.app.set_command_handler(save, [&] { ran = true; });
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::command(save)}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));

    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::Down)));
    CK_CHECK(mf.app.dispatch(key(Key::Enter)));
    CK_CHECK(ran);
    CK_CHECK(mf.app.focused() == editor);
}

CK_TEST(a_menu_opened_from_the_bare_desktop_offers_what_is_usable_outside_contexts) {
    MenuBarFixture mf;
    const ui::CommandId open = mf.app.commands().declare(
        {.key = "test.contextual-open",
         .title = "Open",
         .scope = {.contexts = {"document"}, .outside_contexts = true}});
    const ui::CommandId save = mf.app.commands().declare(
        {.key = "test.contextual-save", .title = "Save", .scope = {.contexts = {"document"}}});
    int opened = 0;
    int saved = 0;
    mf.app.set_command_handler(open, [&] { ++opened; });
    mf.app.set_command_handler(save, [&] { ++saved; });
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::command(open), MenuItem::command(save)}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));

    mf.app.set_focus(nullptr);
    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::Down)));
    CK_CHECK(mf.app.dispatch(key(Key::Enter)));
    CK_CHECK(opened == 1);

    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::Down)));
    CK_CHECK(mf.app.dispatch(key(Key::Down)));
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(saved == 0);
}

// Regression (M8/WP-2): Application::dispatch's new click-to-focus must
// not corrupt MenuBar's own previously_focused_ bookkeeping. A mouse
// click on the bar (not a direct activate() call) drives MenuBar's
// on_mouse -> activate() through the SAME dispatch call that also runs
// click-to-focus; if click-to-focus ran BEFORE delivery it would
// already have set focused_ to the bar itself by the time activate()
// reads app.focused() to remember "what was focused before", and
// Escape would then restore focus to the bar instead of `other`.
CK_TEST(clicking_the_bar_to_open_it_still_restores_the_true_prior_focus_on_escape) {
    MenuBarFixture mf;
    auto* other = mf.app.root().add_child(std::make_unique<ckv::ui::View>());
    other->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    mf.app.set_focus(other);

    // The click below is dispatched through Application's real mouse
    // hit-test (topmost_view_at), which only searches root()'s own
    // subtree — unlike the other MenuBar tests in this file, which
    // drive the bar directly and never need root() reachability, this
    // one needs a Desktop that is BOTH under root() (for the hit-test
    // to find the bar at all) AND the bar's immediate parent (for
    // MenuBar's own parent-walk to resolve it). mf.desktop itself
    // stays off of root() (matching every other test here), so this
    // uses its own throwaway Desktop instead of reaching for it.
    auto desktop_owned =
        std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(mf.app.root().add_child(std::move(desktop_owned)));

    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}});
    MenuBar* bar = static_cast<MenuBar*>(desktop->add_child(std::move(bar_owned)));
    bar->set_bounds(Rect{0, 0, 80, 1});

    mf.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 0},
                                     std::nullopt, Modifier::None});
    CK_CHECK(bar->active());
    CK_CHECK(mf.app.focused() == bar);

    // Esc closes one level: the dropdown the click opened, with the walk
    // still on its title. The second Esc leaves the bar.
    mf.app.dispatch(key(Key::Escape));
    CK_CHECK(bar->active());
    CK_CHECK(desktop->popups().empty());
    mf.app.dispatch(key(Key::Escape));
    CK_CHECK(!bar->active());
    CK_CHECK(mf.app.focused() == other);  // NOT the bar — the true prior focus survived
}

CK_TEST(enter_opens_a_dropdown_as_a_desktop_popup) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));

    bar->activate();
    CK_CHECK(mf.desktop.popups().empty());
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(mf.app.input_capture() == mf.desktop.popups()[0]);
}

CK_TEST(active_menu_bar_highlight_includes_one_cell_of_visual_padding) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->set_bounds(Rect{0, 0, 20, 1});
    bar->activate();
    Surface surface(ckv::Size{20, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 20, 1});
    bar->draw(painter);
    // The 2-cell leading margin (matched to the classic layout so a
    // dropped popup can hang one cell left of its title) puts the padded
    // highlight at columns 1..7.
    CK_CHECK(surface.at(ckv::Point{1, 0}).style() != surface.at(ckv::Point{2, 0}).style());
    CK_CHECK(surface.at(ckv::Point{7, 0}).style() != surface.at(ckv::Point{6, 0}).style());
}

CK_TEST(mouse_drag_from_a_menu_bar_item_switches_dropdown_before_release) {
    MenuBarFixture mf;
    bool ran = false;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {MenuItem::action("Open", {})}},
        {"&Window", {MenuItem::action("Tile", [&] { ran = true; })}},
    });
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->set_bounds(Rect{0, 0, 40, 1});
    bar->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 0},
                                  std::nullopt, Modifier::None});
    CK_CHECK(mf.desktop.popups().size() == 1U);
    auto* first = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);
    first->on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{10, 0},
                                    std::nullopt, Modifier::None});
    CK_CHECK(mf.desktop.popups().size() == 1U);
    auto* second = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);
    second->on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{10, 2},
                                     std::nullopt, Modifier::None});
    second->on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{10, 2},
                                     std::nullopt, Modifier::None});
    CK_CHECK(ran);
}

CK_TEST(captured_menu_bar_pointer_gesture_keeps_dropdown_open_until_release_over_an_item) {
    MenuBarFixture mf;
    bool file_ran = false;
    bool window_ran = false;
    auto desktop_owned = std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* desktop = static_cast<Desktop*>(mf.app.root().add_child(std::move(desktop_owned)));
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {MenuItem::action("&Open", [&] { file_ran = true; })}},
        {"&Window", {MenuItem::action("&Tile", [&] { window_ran = true; })}},
    });
    MenuBar* bar = static_cast<MenuBar*>(desktop->add_child(std::move(bar_owned)));
    bar->set_bounds(Rect{0, 0, 40, 1});

    // One ordinary click opens File; its release remains part of the menu
    // gesture and must not dismiss the captured dropdown.
    mf.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 0},
                                     std::nullopt, Modifier::None});
    mf.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{2, 0},
                                     std::nullopt, Modifier::None});
    CK_CHECK(desktop->popups().size() == 1U);
    CK_CHECK(!file_ran && !window_ran);

    // The capture also makes a fresh press on Window switch dropdowns before
    // release, rather than treating the bar as an outside click.
    mf.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{10, 0},
                                     std::nullopt, Modifier::None});
    mf.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{10, 0},
                                     std::nullopt, Modifier::None});
    CK_CHECK(desktop->popups().size() == 1U);
    CK_CHECK(!file_ran && !window_ran);

    // A later press-drag-release over the item, not the initially pressed
    // top-level label, is the only action that executes the command.
    mf.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{10, 0},
                                     std::nullopt, Modifier::None});
    mf.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{10, 2},
                                     std::nullopt, Modifier::None});
    mf.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{10, 2},
                                     std::nullopt, Modifier::None});
    CK_CHECK(!file_ran && window_ran);
    CK_CHECK(desktop->popups().empty());
}

namespace {
// A trailing view that counts what the bar does to it -- what a clock at the
// right end of the bar is, reduced to what these tests need to see.
class TrailingProbe : public ckv::ui::View, public ckv::widgets::MenuBarAccessory {
public:
    void set_menu_highlighted(bool highlighted) override { highlighted_ = highlighted; }
    void activate_from_menu_bar() override { ++activations; }
    bool highlighted() const noexcept { return highlighted_; }
    ckv::ui::SizeHint horizontal_size_hint() const override {
        return ckv::ui::SizeHint{width_, width_, width_};
    }
    ckv::ui::SizeHint vertical_size_hint() const override { return ckv::ui::SizeHint{1, 1, 1}; }
    // What a clock does when it is given seconds: it becomes a wider thing
    // than it was, and it says so.
    void set_width(int width) {
        width_ = width;
        size_hint_changed();
    }
    int activations = 0;

private:
    bool highlighted_ = false;
    int width_ = 5;
};

MenuBar* bar_with_trailing_probe(MenuBarFixture& mf, TrailingProbe** probe) {
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {MenuItem::action("&Open", {})}},
        {"&Help", {MenuItem::action("&About", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->set_bounds(Rect{0, 0, 80, 1});
    *probe = bar->set_trailing_view(std::make_unique<TrailingProbe>());
    return bar;
}
}  // namespace

CK_TEST(a_trailing_view_that_changes_width_is_placed_again_at_the_right_end) {
    // The bar measures its trailing view when it is put in and when the bar is
    // resized. Neither happens when the view itself becomes wider — a clock
    // switched to seconds — so without this the bar kept the old width and the
    // last digits were clipped off the end of the row.
    MenuBarFixture mf;
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);
    CK_CHECK(probe->bounds().width == 5);
    CK_CHECK(probe->bounds().right() == bar->bounds().width);

    probe->set_width(8);
    CK_CHECK(probe->bounds().width == 8);
    CK_CHECK(probe->bounds().right() == bar->bounds().width);

    probe->set_width(5);  // and back again, without leaving a gap at the edge
    CK_CHECK(probe->bounds().width == 5);
    CK_CHECK(probe->bounds().right() == bar->bounds().width);
}

CK_TEST(taking_the_trailing_title_away_takes_the_walk_off_its_slot) {
    // A trailing title can be removed while the bar is being walked — an
    // application whose clock the reader has just switched off. Its slot goes
    // with it, and a highlight left pointing one past the last menu made the
    // next Enter ask for a dropdown that does not exist.
    MenuBarFixture mf;
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);
    bar->activate();
    mf.app.dispatch(key(Key::Left));  // wraps left, onto the trailing title
    CK_CHECK(probe->highlighted());

    bar->set_trailing_view(std::unique_ptr<TrailingProbe>{});
    CK_CHECK(bar->trailing_view() == nullptr);
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(mf.desktop.popups().size() == 1);  // the last menu, not an abort
}

CK_TEST(a_replacement_trailing_title_inherits_the_walk_that_was_on_it) {
    MenuBarFixture mf;
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);
    bar->activate();
    mf.app.dispatch(key(Key::Left));
    CK_CHECK(probe->highlighted());

    TrailingProbe* const replacement = bar->set_trailing_view(std::make_unique<TrailingProbe>());
    CK_CHECK(replacement->highlighted());  // to the reader, the same title is still there
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(replacement->activations == 1);
    CK_CHECK(mf.desktop.popups().empty());
}

CK_TEST(walking_onto_the_trailing_title_closes_the_menu_that_was_open) {
    MenuBarFixture mf;
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);
    bar->activate();
    mf.app.dispatch(key(Key::Enter));  // opens File
    CK_CHECK(mf.desktop.popups().size() == 1);

    mf.app.dispatch(key(Key::Right));  // File -> Help, carrying the open menu
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(!probe->highlighted());

    mf.app.dispatch(key(Key::Right));  // Help -> the trailing title
    // One slot is highlighted and what is open belongs to it: a menu left
    // open here would keep the keys, and Enter would choose its item rather
    // than the title the reader can see is selected.
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(probe->highlighted());

    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(probe->activations == 1);
}

CK_TEST(stepping_back_off_the_trailing_title_shows_the_menu_again) {
    MenuBarFixture mf;
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);
    bar->activate();
    mf.app.dispatch(key(Key::Enter));  // opens File
    mf.app.dispatch(key(Key::Right));
    mf.app.dispatch(key(Key::Right));  // onto the trailing title: suspended
    CK_CHECK(mf.desktop.popups().empty());

    mf.app.dispatch(key(Key::Left));  // back onto Help
    // Suspended, not cancelled: this is still the walk the reader started
    // with a menu open.
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(!probe->highlighted());
}

CK_TEST(a_menu_opened_by_mnemonic_takes_the_highlight_off_the_trailing_title) {
    MenuBarFixture mf;
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);
    bar->activate();
    mf.app.dispatch(key(Key::Right));
    mf.app.dispatch(key(Key::Right));  // onto the trailing title
    CK_CHECK(probe->highlighted());

    mf.app.dispatch(key(Key::Char, "f"));  // File, by its mnemonic
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(!probe->highlighted());  // never two of them lit at once
}

CK_TEST(escape_closes_an_open_dropdown_first_without_deactivating_the_bar) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->activate();
    mf.app.dispatch(key(Key::Enter));  // opens the dropdown
    CK_CHECK(mf.desktop.popups().size() == 1);

    // Through the Application's own routing: the key reaches the bar, which
    // holds the focus while its dropdown holds the pointer capture.
    CK_CHECK(mf.app.dispatch(key(Key::Escape)));
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(mf.app.input_capture() == nullptr);
    CK_CHECK(bar->active());  // the bar itself is still active — only the dropdown closed
    CK_CHECK(mf.app.focused() == bar);

    // The same one level when the popup is told directly, as a light
    // dismissal tells it.
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(mf.desktop.popups().size() == 1);
    static_cast<DropdownMenu*>(mf.desktop.popups()[0])->on_key(key(Key::Escape));
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(bar->active());
}

CK_TEST(menu_bar_routes_item_mnemonics_to_its_open_dropdown) {
    MenuBarFixture mf;
    bool opened = false;
    const CommandId open_command = mf.app.commands().declare(
        {.key = "test.open", .title = "&Open", .handler = [&] { opened = true; }});

    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{ {"&File", {MenuItem::command(open_command)}} });
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::Enter)));
    CK_CHECK(mf.desktop.popups().size() == 1U);

    CK_CHECK(mf.app.dispatch(key(Key::Char, "o")));
    CK_CHECK(opened);
    CK_CHECK(mf.desktop.popups().empty());
}

CK_TEST(destroying_the_bar_while_a_dropdown_is_open_cleans_up_the_popup_and_input_capture) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->activate();
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(mf.desktop.popups().size() == 1);

    auto owned = mf.desktop.remove_child(bar);
    owned.reset();  // destroys the MenuBar — ~MenuBar must close the still-open dropdown

    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(mf.app.input_capture() == nullptr);
}

CK_TEST(left_and_right_move_the_highlighted_menu_and_wrap) {
    MenuBarFixture mf;
    auto bar_owned =
        std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}, {"&Edit", {}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::Right)));
    CK_CHECK(mf.app.dispatch(key(Key::Right)));  // wraps back to menu 0
    // No direct accessor for highlighted_ — verified indirectly via
    // which menu Enter opens next.
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(mf.desktop.popups().size() == 1);
}

CK_TEST(mnemonic_letter_opens_the_matching_top_level_menu) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}},
                                  {"&Edit", {MenuItem::action("&Copy", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::Char, "e")));
    CK_CHECK(mf.desktop.popups().size() == 1);
}

CK_TEST(on_key_is_unhandled_when_the_bar_is_not_active) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    CK_CHECK(!bar->on_key(key(Key::Right)));
}

namespace {
// Four titles that need 30 columns; a 22-column bar draws two of them.
std::vector<MenuBarItem> four_titles(std::vector<std::string>* log = nullptr) {
    const auto row = [log](const char* label, const char* what) {
        return MenuItem::action(label, [log, what] {
            if (log != nullptr) log->push_back(what);
        });
    };
    return {{"&File", {row("&New", "new"), row("&Open", "open")}},
            {"&Navigate", {row("&Back", "back"), row("&Forward", "forward")}},
            {"&Window", {row("&Tile", "tile"), row("&Cascade", "cascade")}},
            {"&Help", {row("&About", "about")}}};
}
}  // namespace

CK_TEST(a_narrow_menu_bar_draws_whole_titles_and_ends_them_with_the_overflow_title) {
    MenuBarFixture mf;
    MenuBar* bar = mf.add_bar(std::make_unique<MenuBar>(four_titles()));
    bar->set_bounds(Rect{0, 0, 22, 1});
    CK_CHECK(bar->overflowing());
    CK_CHECK(bar->visible_menu_count() == 2U);

    Surface s(ckv::Size{22, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 22, 1});
    bar->draw(painter);
    // No title is cut short: Window and Help are behind the overflow title,
    // not clipped into view.
    CK_CHECK(row_text(s, 0) == "  File  Navigate  \u00BB   ");
}

CK_TEST(a_bar_with_room_for_every_title_draws_no_overflow_title) {
    MenuBarFixture mf;
    MenuBar* bar = mf.add_bar(std::make_unique<MenuBar>(four_titles()));
    bar->set_bounds(Rect{0, 0, 30, 1});
    CK_CHECK(!bar->overflowing());
    CK_CHECK(bar->visible_menu_count() == 4U);
    Surface s(ckv::Size{30, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 30, 1});
    bar->draw(painter);
    CK_CHECK(row_text(s, 0) == "  File  Navigate  Window  Help");

    // One column less and the last title would be clipped, so it goes behind
    // the overflow title, which takes its place.
    bar->set_bounds(Rect{0, 0, 29, 1});
    CK_CHECK(bar->visible_menu_count() == 3U);
    Surface narrower(ckv::Size{29, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter narrower_painter(narrower, Rect{0, 0, 29, 1});
    bar->draw(narrower_painter);
    CK_CHECK(row_text(narrower, 0) == "  File  Navigate  Window  \u00BB  ");
}

CK_TEST(a_bar_with_no_room_for_any_title_is_all_overflow) {
    MenuBarFixture mf;
    MenuBar* bar = mf.add_bar(std::make_unique<MenuBar>(four_titles()));
    bar->set_bounds(Rect{0, 0, 5, 1});
    CK_CHECK(bar->visible_menu_count() == 0U);
    Surface s(ckv::Size{5, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 5, 1});
    bar->draw(painter);
    CK_CHECK(row_text(s, 0) == "  \u00BB  ");

    // F10 stands on the overflow title, and Enter lists every menu.
    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::Enter)));
    CK_CHECK(mf.desktop.popups().size() == 1U);
    CK_CHECK(static_cast<DropdownMenu*>(mf.desktop.popups()[0])->items().size() == 4U);
}

CK_TEST(the_overflow_list_holds_each_hidden_menu_as_a_submenu) {
    MenuBarFixture mf;
    MenuBar* bar = mf.add_bar(std::make_unique<MenuBar>(four_titles()));
    bar->set_bounds(Rect{0, 0, 22, 1});
    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::End)));  // the overflow title is the last stop
    CK_CHECK(mf.app.dispatch(key(Key::Down)));
    CK_CHECK(mf.desktop.popups().size() == 1U);
    const auto* list = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);
    CK_CHECK(list->items().size() == 2U);
    CK_CHECK(list->items()[0].has_children());
    CK_CHECK(list->items()[0].label() == "&Window");
    CK_CHECK(list->items()[1].label() == "&Help");
    CK_CHECK(list->items()[0].children().size() == 2U);
    CK_CHECK(list->highlighted() == 0);  // on the first hidden menu, not in it
}

CK_TEST(the_walk_steps_over_the_overflow_title_as_one_stop_and_wraps) {
    MenuBarFixture mf;
    MenuBar* bar = mf.add_bar(std::make_unique<MenuBar>(four_titles()));
    bar->set_bounds(Rect{0, 0, 22, 1});
    bar->activate();
    const auto opened = [&mf]() -> std::string {
        mf.app.dispatch(key(Key::Down));
        const auto* menu = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);
        std::string first = menu->items()[0].label();
        mf.app.dispatch(key(Key::Escape));  // one level: back to the title
        return first;
    };
    mf.app.dispatch(key(Key::Right));
    mf.app.dispatch(key(Key::Right));
    CK_CHECK(opened() == "&Window");  // the overflow list
    mf.app.dispatch(key(Key::Right));  // past the overflow title, round to File
    CK_CHECK(opened() == "&New");
    mf.app.dispatch(key(Key::Left));  // and back onto it
    CK_CHECK(opened() == "&Window");
    CK_CHECK(bar->active());
}

CK_TEST(a_hidden_menus_mnemonic_opens_it_inside_the_overflow_list) {
    MenuBarFixture mf;
    MenuBar* bar = mf.add_bar(std::make_unique<MenuBar>(four_titles()));
    bar->set_bounds(Rect{0, 0, 22, 1});

    // The Alt accelerator, from outside the menu system.
    CK_CHECK(mf.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "h"}}));
    CK_CHECK(bar->active());
    CK_CHECK(mf.desktop.popups().size() == 2U);
    auto* list = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);
    auto* help = static_cast<DropdownMenu*>(mf.desktop.popups()[1]);
    CK_CHECK(list->highlighted() == 1);
    CK_CHECK(help->items().size() == 1U);
    CK_CHECK(help->highlighted() == 0);
    CK_CHECK(mf.app.input_capture() == help);

    // Esc backs out one level at a time: Help, the list, the bar.
    mf.app.dispatch(key(Key::Escape));
    CK_CHECK(mf.desktop.popups().size() == 1U);
    mf.app.dispatch(key(Key::Escape));
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(bar->active());
    mf.app.dispatch(key(Key::Escape));
    CK_CHECK(!bar->active());

    // The letter alone while the bar is walked does the same.
    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::Char, "w")));
    CK_CHECK(mf.desktop.popups().size() == 2U);
    CK_CHECK(static_cast<DropdownMenu*>(mf.desktop.popups()[1])->items()[0].label() == "&Tile");
}

CK_TEST(choosing_from_a_hidden_menu_ends_the_walk_and_runs_the_row) {
    MenuBarFixture mf;
    std::vector<std::string> log;
    MenuBar* bar = mf.add_bar(std::make_unique<MenuBar>(four_titles(&log)));
    bar->set_bounds(Rect{0, 0, 22, 1});
    bar->activate();
    mf.app.dispatch(key(Key::End));
    mf.app.dispatch(key(Key::Down));
    mf.app.dispatch(key(Key::Right));  // into Window
    mf.app.dispatch(key(Key::Down));
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK((log == std::vector<std::string>{"cascade"}));
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(!bar->active());
}

CK_TEST(a_press_on_the_overflow_title_opens_the_list) {
    MenuBarFixture mf;
    MenuBar* bar = mf.add_bar(std::make_unique<MenuBar>(four_titles()));
    bar->set_bounds(Rect{0, 0, 22, 1});
    CK_CHECK(bar->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{18, 0},
                                           std::nullopt, Modifier::None}));
    CK_CHECK(bar->active());
    CK_CHECK(mf.desktop.popups().size() == 1U);
    auto* list = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);
    CK_CHECK(list->items()[0].label() == "&Window");
    CK_CHECK(list->highlighted() == -1);  // a pointer-opened menu waits for the pointer

    // Dragging along the bar onto a drawn title swaps the list for its menu.
    CK_CHECK(list->on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{10, 0},
                                            std::nullopt, Modifier::None}));
    CK_CHECK(mf.desktop.popups().size() == 1U);
    CK_CHECK(static_cast<DropdownMenu*>(mf.desktop.popups()[0])->items()[0].label() == "&Back");
}

CK_TEST(a_resize_moves_an_open_menu_between_its_title_and_the_overflow_list) {
    MenuBarFixture mf;
    MenuBar* bar = mf.add_bar(std::make_unique<MenuBar>(four_titles()));
    bar->set_bounds(Rect{0, 0, 22, 1});
    CK_CHECK(mf.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "w"}}));
    CK_CHECK(mf.desktop.popups().size() == 2U);

    // Room for Window's own title: the reader is in Window, under it now.
    bar->set_bounds(Rect{0, 0, 80, 1});
    CK_CHECK(!bar->overflowing());
    CK_CHECK(mf.desktop.popups().size() == 1U);
    const auto* window = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);
    CK_CHECK(window->items()[0].label() == "&Tile");
    CK_CHECK(window->bounds().x == 17);  // hanging from "Window" at column 18
    CK_CHECK(mf.app.input_capture() == window);

    // And back: Window is hidden again, and still open, inside the list.
    bar->set_bounds(Rect{0, 0, 22, 1});
    CK_CHECK(mf.desktop.popups().size() == 2U);
    CK_CHECK(static_cast<DropdownMenu*>(mf.desktop.popups()[1])->items()[0].label() == "&Tile");
    CK_CHECK(bar->active());
}

CK_TEST(the_trailing_title_follows_the_overflow_title_on_the_walk) {
    MenuBarFixture mf;
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);
    // File fits; Help does not, so the overflow title stands at column 8 and
    // leaves the probe no room.
    bar->set_bounds(Rect{0, 0, 10, 1});
    CK_CHECK(bar->visible_menu_count() == 1U);
    CK_CHECK(probe->bounds().width == 0);
    bar->activate();
    mf.app.dispatch(key(Key::Right));  // the overflow title
    CK_CHECK(!probe->highlighted());
    mf.app.dispatch(key(Key::Right));  // the trailing title
    CK_CHECK(probe->highlighted());
    mf.app.dispatch(key(Key::Right));  // round to File
    CK_CHECK(!probe->highlighted());
    mf.app.dispatch(key(Key::Left));
    CK_CHECK(probe->highlighted());

    // Wide enough for both titles and the probe, the overflow title is gone.
    bar->set_bounds(Rect{0, 0, 18, 1});
    CK_CHECK(!bar->overflowing());
    CK_CHECK(probe->bounds().width == 5);
}

CK_TEST(menu_mnemonics_use_the_shared_hotkey_accent_on_each_surface) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->set_bounds(Rect{0, 0, 12, 1});
    Surface bar_surface(ckv::Size{12, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter bar_painter(bar_surface, Rect{0, 0, 12, 1});
    bar->draw(bar_painter);
    CK_CHECK(bar_surface.at(ckv::Point{2, 0}).grapheme() == "F");
    CK_CHECK(bar_surface.at(ckv::Point{2, 0}).style().fg == mf.f.theme.resolve(mf.f.roles.hotkey).fg);
    CK_CHECK(bar_surface.at(ckv::Point{2, 0}).style().bg == mf.f.theme.resolve(mf.f.roles.menu_bar_normal).bg);

    auto menu = make_dropdown(mf.f, mf.app, {MenuItem::action("&Open", {})});
    menu->set_bounds(Rect{0, 0, 12, 1});
    Surface menu_surface(ckv::Size{12, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter menu_painter(menu_surface, Rect{0, 0, 12, 1});
    menu->draw(menu_painter);
    CK_CHECK(menu_surface.at(ckv::Point{1, 0}).grapheme() == "O");
    CK_CHECK(menu_surface.at(ckv::Point{1, 0}).style().fg == mf.f.theme.resolve(mf.f.roles.hotkey).fg);
    CK_CHECK(menu_surface.at(ckv::Point{1, 0}).style().bg ==
             mf.f.theme.resolve(mf.f.roles.menu_dropdown_highlighted).bg);
}

CK_TEST(a_popup_dropdown_has_an_opaque_classic_frame_and_padded_item_interior) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, { MenuItem::action("&Open", {}),
                                       MenuItem::separator(),
                                       MenuItem::action("&Quit", {}) });
    const ui::SizeHint width = menu->horizontal_size_hint();
    const ui::SizeHint height = menu->vertical_size_hint();
    menu->set_bounds(Rect{0, 0, width.preferred, height.preferred});

    Surface surface(ckv::Size{width.preferred, height.preferred},
                    ckv::Cell::from_grapheme("P", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, width.preferred, height.preferred});
    menu->draw(painter);

    CK_CHECK(surface.at(ckv::Point{0, 0}).grapheme() == "┌");
    CK_CHECK(surface.at(ckv::Point{width.preferred - 1, height.preferred - 1}).grapheme() == "┘");
    CK_CHECK(surface.at(ckv::Point{1, 1}).grapheme() == " ");
    CK_CHECK(surface.at(ckv::Point{2, 1}).grapheme() == "O");
    CK_CHECK(surface.at(ckv::Point{2, 1}).style().bg ==
             f.theme.resolve(f.roles.menu_dropdown_highlighted).bg);
    // The separator row runs into the side frames, so those cells are the
    // junction the rule makes with the border, not a plain vertical.
    CK_CHECK(surface.at(ckv::Point{0, 2}).grapheme() == "├");
    CK_CHECK(surface.at(ckv::Point{width.preferred - 1, 2}).grapheme() == "┤");
}

CK_TEST(a_submenu_marker_and_a_chord_hint_end_at_the_same_column) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const MenuItem opens_submenu = MenuItem::submenu("&New", {MenuItem::action("&Child", [] {})});
    auto menu = make_dropdown(
        f, app, {opens_submenu, MenuItem::command(ckv::widgets::CommandPresentation{standard(app).quit})});
    const int width = menu->horizontal_size_hint().preferred;
    const int height = menu->vertical_size_hint().preferred;
    menu->set_bounds(Rect{0, 0, width, height});

    Surface surface(ckv::Size{width, height}, ckv::Cell::from_grapheme("P", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, width, height});
    menu->draw(painter);

    // One column of padding inside the frame on both sides, so the last
    // column any item may write to is the same one for every item.
    const int content_right = width - 3;
    // The pointer the convention uses, rather than an ASCII '>' that reads
    // as text somebody typed into the label.
    CK_CHECK(surface.at(ckv::Point{content_right, 1}).grapheme() == "►");
    // Alt+X, the standard quit chord, ends in that very column: a menu
    // carrying both reads as one right-hand column, not two ragged ones.
    CK_CHECK(surface.at(ckv::Point{content_right, 2}).grapheme() == "X");
    CK_CHECK(surface.at(ckv::Point{content_right + 1, 1}).grapheme() == " ");
    CK_CHECK(surface.at(ckv::Point{content_right + 1, 2}).grapheme() == " ");
    // And no item is charged for a column it does not use: the widest item
    // is exactly what the menu is wide enough for.
    CK_CHECK(width == 1 + 1 + ckv::text::text_width("Quit") + 2 + ckv::text::text_width("Alt+X") + 1 + 1);
}

CK_TEST(a_context_menu_keeps_its_frame_inside_the_desktop_at_the_bottom_right_edge) {
    MenuBarFixture mf;
    DropdownMenu* menu = show_context_menu(
        {MenuItem::action("&Open", {}),
         MenuItem::action("&Quit", {})},
        ckv::Point{79, 23}, mf.app, mf.desktop);

    const Rect bounds = menu->bounds();
    CK_CHECK(bounds.x >= 0);
    CK_CHECK(bounds.y >= 0);
    CK_CHECK(bounds.right() <= mf.desktop.bounds().width);
    CK_CHECK(bounds.bottom() <= mf.desktop.bounds().height);
    CK_CHECK(mf.app.dispatch(key(Key::Escape)));
    CK_CHECK(mf.desktop.popups().empty());
}

// A menu hanging from a control opens below it while it fits there, and
// above it when the control sits too near the bottom -- a bar docked at the
// bottom of the desktop drops its menus upward.
CK_TEST(an_anchored_menu_opens_below_its_anchor_or_above_it_near_the_bottom) {
    MenuBarFixture mf;
    const std::vector<MenuItem> items{MenuItem::action("&Open", {}), MenuItem::action("&Quit", {})};

    DropdownMenu* below = ckv::widgets::show_anchored_menu(items, Rect{10, 3, 5, 1}, mf.app, mf.desktop);
    CK_CHECK((below->bounds().x == 10));
    CK_CHECK((below->bounds().y == 4));
    CK_CHECK(mf.app.dispatch(key(Key::Escape)));
    CK_CHECK(mf.desktop.popups().empty());

    DropdownMenu* above = ckv::widgets::show_anchored_menu(items, Rect{10, 22, 5, 1}, mf.app, mf.desktop);
    // Two rows and the frame, ending on the row above the anchor.
    CK_CHECK((above->bounds().y == 22 - 4));
    CK_CHECK((above->bounds().bottom() == 22));
    CK_CHECK(mf.app.dispatch(key(Key::Escape)));
    CK_CHECK(mf.desktop.popups().empty());
}

// --- MenuBar: F10 default activation (M9/WP-13, D-029) ---------------------

CK_TEST(attaching_a_menu_bar_installs_itself_as_f10s_default_handler) {
    MenuBarFixture mf;
    CK_CHECK(!mf.app.commands().has_handler(standard(mf.app).menu));
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    CK_CHECK(mf.app.commands().has_handler(standard(mf.app).menu));
    CK_CHECK(!bar->active());

    mf.app.commands().execute(standard(mf.app).menu);
    CK_CHECK(bar->active());
}

CK_TEST(f10_restarts_top_level_mnemonic_navigation_after_a_previous_menu) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {MenuItem::action("Open", {})}},
         {"&Appearance", {MenuItem::action("Colors", {})}}});
    CK_CHECK(mf.add_bar(std::move(bar_owned)) != nullptr);
    CK_CHECK(mf.app.dispatch(key(Key::F10)));
    CK_CHECK(mf.app.dispatch(key(Key::Right)));

    CK_CHECK(mf.app.dispatch(key(Key::F10)));
    CK_CHECK(mf.app.dispatch(key(Key::Char, "a")));
    CK_CHECK(mf.desktop.popups().size() == 1);
    if (mf.desktop.popups().size() != 1) return;
    const auto* popup = dynamic_cast<const DropdownMenu*>(mf.desktop.popups().front());
    CK_CHECK(popup != nullptr);
    if (popup != nullptr && !popup->items().empty())
        CK_CHECK(popup->items().front().label() == "Colors");
}

CK_TEST(a_pre_existing_kmenu_handler_is_not_overridden_by_attaching_a_menu_bar) {
    MenuBarFixture mf;
    bool custom_ran = false;
    mf.app.commands().set_handler(standard(mf.app).menu, [&] { custom_ran = true; });

    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));

    mf.app.commands().execute(standard(mf.app).menu);
    CK_CHECK(custom_ran);
    // the bar's own activate() never ran — its handler was never installed
    CK_CHECK(!bar->active());
}

CK_TEST(destroying_the_bar_clears_the_default_handler_it_installed) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    CK_CHECK(mf.app.commands().has_handler(standard(mf.app).menu));

    mf.desktop.remove_child(bar).reset();  // destroys the MenuBar

    CK_CHECK(!mf.app.commands().has_handler(standard(mf.app).menu));
    // A stale handler calling into freed memory would crash (or, under
    // ASan, report use-after-free) here rather than just returning false.
    CK_CHECK(!mf.app.commands().execute(standard(mf.app).menu));
}

CK_TEST(a_second_menu_bar_that_finds_kmenu_already_claimed_does_not_clear_it_on_destruction) {
    MenuBarFixture mf;
    auto bar1_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar1 = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar1_owned)));

    auto bar2_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&Edit", {}}});
    MenuBar* bar2 = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar2_owned)));

    // bar2 attached second: kMenu was already claimed by bar1, so bar2
    // never installed its own handler — destroying it must not clear
    // the one bar1 is relying on.
    mf.desktop.remove_child(bar2).reset();
    CK_CHECK(mf.app.commands().has_handler(standard(mf.app).menu));

    mf.app.commands().execute(standard(mf.app).menu);
    CK_CHECK(bar1->active());  // still routes to bar1, the true owner
}

// --- Regression: reentrancy during activation (review finding #1) ---------

CK_TEST(an_items_on_activate_that_reopens_another_menu_does_not_crash_the_activating_dropdown) {
    // DropdownMenu::activate() used to run the item's callback THEN
    // touch `this->dismiss()` afterward. If the callback closes THIS
    // dropdown (e.g. by opening a different menu, which MenuBar
    // implements by closing whatever is currently open first), that
    // trailing dismiss() ran on freed memory. Reproduces exactly that:
    // the item's on_activate calls back into MenuBar to open a SECOND
    // menu, which must close (and destroy) the dropdown this activation
    // started from.
    MenuBarFixture mf;
    // The item's own action reopens another menu, destroying the dropdown
    // it is being activated from — the hazard the original bug required.
    // The action is part of the item, so it is written when the menu is
    // built; the bar it needs does not exist until just after, which the
    // holder bridges.
    auto bar_holder = std::make_shared<MenuBar*>(nullptr);
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {MenuItem::action("&Reopen",
                                    [bar_holder] {
                                        if (*bar_holder != nullptr)
                                            (*bar_holder)->on_key(key(Key::Right));
                                    })}},
        {"&Edit", {}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    *bar_holder = bar;
    bar->activate();
    mf.app.dispatch(key(Key::Enter));  // opens File's dropdown
    CK_CHECK(mf.desktop.popups().size() == 1);

    mf.app.dispatch(key(Key::Enter));  // activates the item, which reopens Edit's (empty) dropdown
    CK_CHECK(true);  // reaching this line without a crash/ASan report IS the assertion
}

// --- Regression: MenuBar::open_dropdown_ desync (review finding #3) --------

CK_TEST(a_popup_removed_by_something_other_than_the_bar_still_lets_the_bar_recover_correctly) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->activate();
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(mf.desktop.popups().size() == 1);

    // Bypass MenuBar entirely: remove the popup directly, as a caller
    // unaware of MenuBar's internal bookkeeping might.
    ui::View* popup = mf.desktop.popups()[0];
    mf.desktop.remove_popup(popup).reset();  // destroys it -> ~DropdownMenu fires on_dismiss

    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(mf.app.input_capture() == nullptr);  // MenuBar's close_dropdown() ran and cleared it

    // The bar must now believe no dropdown is open — Right must move
    // the highlight without trying to reopen a dropdown for a menu
    // that no longer exists (there's only one menu, so this also
    // implicitly checks no crash from operating on the stale pointer).
    CK_CHECK(mf.app.dispatch(key(Key::Right)));
    CK_CHECK(mf.desktop.popups().empty());  // still no (stale) dropdown reopened
}

CK_TEST(right_arrow_opens_a_nested_submenu_and_escape_closes_one_level) {
    MenuBarFixture mf;
    const MenuItem more =
        MenuItem::submenu("&More", {MenuItem::action("&Child", [] {})});
    DropdownMenu* root = show_context_menu({more}, ckv::Point{2, 2}, mf.app, mf.desktop);

    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(mf.app.input_capture() == root);
    CK_CHECK(mf.app.dispatch(key(Key::Right)));
    CK_CHECK(mf.desktop.popups().size() == 2);

    // The keyboard focus stays on the root; the key still reaches the
    // submenu, and Escape there closes that one level only.
    auto* child = static_cast<DropdownMenu*>(mf.desktop.popups()[1]);
    CK_CHECK(mf.app.input_capture() == child);
    CK_CHECK(mf.app.focused() == root);
    CK_CHECK(mf.app.dispatch(key(Key::Escape)));
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(mf.desktop.popups()[0] == root);
    CK_CHECK(mf.app.input_capture() == root);
}

CK_TEST(activating_a_nested_leaf_closes_the_whole_menu_chain_before_running_it) {
    MenuBarFixture mf;
    bool ran = false;
    const MenuItem more = MenuItem::submenu("&More", {MenuItem::action("&Child", [&] {
                                  CK_CHECK(mf.desktop.popups().empty());
                                  CK_CHECK(mf.app.input_capture() == nullptr);
                                  ran = true;
                              })});
    CK_CHECK(show_context_menu({more}, ckv::Point{2, 2}, mf.app, mf.desktop) != nullptr);
    CK_CHECK(mf.app.dispatch(key(Key::Right)));
    CK_CHECK(mf.desktop.popups().size() == 2U);

    CK_CHECK(mf.app.dispatch(key(Key::Enter)));
    CK_CHECK(ran);
}

// --- A submenu below a menu bar is a keyboard destination -----------------

namespace {
// The shape a document application's File menu has: an entry that opens a
// submenu, a plain entry beside it, and a second top-level menu — enough to
// tell "step deeper" from "step along" and to prove each still happens where
// the other does not.
MenuBar* bar_with_a_submenu(MenuBarFixture& mf, bool& chose_text) {
    const MenuItem new_item = MenuItem::submenu("&New", {MenuItem::action("&Sheet", [] {}),
        MenuItem::action("&Text", [&chose_text] { chose_text = true; })});
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {new_item, MenuItem::action("&Close", [] {})}},
        {"&Window", {MenuItem::action("&Tile", [] {})}},
    });
    return mf.add_bar(std::move(bar_owned));
}

// Opens File and steps into New's submenu, the way a reader does: F10-
// equivalent activation, Enter to drop the menu, Right to go deeper.
MenuBar* open_the_submenu(MenuBarFixture& mf, bool& chose_text) {
    MenuBar* bar = bar_with_a_submenu(mf, chose_text);
    bar->activate();
    mf.app.dispatch(key(Key::Enter));
    mf.app.dispatch(key(Key::Right));
    return bar;
}
}  // namespace

CK_TEST(right_enters_the_submenu_and_the_arrows_then_move_inside_it) {
    MenuBarFixture mf;
    bool chose_text = false;
    MenuBar* bar = bar_with_a_submenu(mf, chose_text);

    bar->activate();
    mf.app.dispatch(key(Key::Enter));  // File drops, standing on "New"
    CK_CHECK(mf.desktop.popups().size() == 1);

    mf.app.dispatch(key(Key::Right));
    CK_CHECK(mf.desktop.popups().size() == 2);
    auto* root = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);
    auto* submenu = static_cast<DropdownMenu*>(mf.desktop.popups()[1]);
    CK_CHECK(mf.app.input_capture() == submenu);
    CK_CHECK(submenu->highlighted() == 0);

    // The arrow belongs to the menu the reader is looking at. Before this,
    // it went to the parent, which closed the submenu on the way past.
    mf.app.dispatch(key(Key::Down));
    CK_CHECK(mf.desktop.popups().size() == 2);
    CK_CHECK(submenu->highlighted() == 1);
    CK_CHECK(root->highlighted() == 0);  // "New" still marks the way in

    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(chose_text);
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(!bar->active());
}

CK_TEST(enter_opens_a_submenu_the_same_way_right_does) {
    MenuBarFixture mf;
    bool chose_text = false;
    MenuBar* bar = bar_with_a_submenu(mf, chose_text);
    bar->activate();
    mf.app.dispatch(key(Key::Enter));  // File drops, standing on "New"

    mf.app.dispatch(key(Key::Enter));  // choosing an entry that IS a submenu
    CK_CHECK(mf.desktop.popups().size() == 2);
    CK_CHECK(static_cast<DropdownMenu*>(mf.desktop.popups()[1])->highlighted() == 0);
}

CK_TEST(a_submenu_mnemonic_reaches_the_submenus_own_items) {
    MenuBarFixture mf;
    bool chose_text = false;
    open_the_submenu(mf, chose_text);

    mf.app.dispatch(key(Key::Char, "t"));  // "&Text", inside the submenu
    CK_CHECK(chose_text);
    CK_CHECK(mf.desktop.popups().empty());
}

CK_TEST(left_inside_a_submenu_returns_to_the_item_that_opened_it) {
    MenuBarFixture mf;
    bool chose_text = false;
    open_the_submenu(mf, chose_text);
    auto* root = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);

    mf.app.dispatch(key(Key::Left));

    CK_CHECK(mf.desktop.popups().size() == 1);  // one level, not the whole menu
    CK_CHECK(mf.desktop.popups()[0] == root);
    CK_CHECK(mf.app.input_capture() == root);
    CK_CHECK(root->highlighted() == 0);  // back on "New", which is where it came from

    // And the bar has Left back now that there is no submenu to leave: the
    // next one steps to the previous top-level menu, wrapping onto Window.
    mf.app.dispatch(key(Key::Left));
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(static_cast<DropdownMenu*>(mf.desktop.popups()[0])->items()[0].label() == "&Tile");
}

CK_TEST(escape_inside_a_submenu_closes_only_the_submenu) {
    MenuBarFixture mf;
    bool chose_text = false;
    MenuBar* bar = open_the_submenu(mf, chose_text);
    auto* root = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);

    mf.app.dispatch(key(Key::Escape));
    CK_CHECK(bar->active());
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(mf.desktop.popups()[0] == root);
    CK_CHECK(mf.app.input_capture() == root);

    // The second closes the dropdown and leaves the walk on its title; only
    // the third leaves the menu system.
    mf.app.dispatch(key(Key::Escape));
    CK_CHECK(bar->active());
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(mf.app.input_capture() == nullptr);
    mf.app.dispatch(key(Key::Escape));
    CK_CHECK(!bar->active());
}

CK_TEST(right_where_there_is_nowhere_deeper_still_walks_on_to_the_next_menu) {
    MenuBarFixture mf;
    bool chose_text = false;
    MenuBar* bar = bar_with_a_submenu(mf, chose_text);
    bar->activate();
    mf.app.dispatch(key(Key::Enter));
    mf.app.dispatch(key(Key::Down));  // "Close": an entry with no submenu

    mf.app.dispatch(key(Key::Right));
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(static_cast<DropdownMenu*>(mf.desktop.popups()[0])->items()[0].label() == "&Tile");
}

CK_TEST(right_on_a_submenu_leaf_leaves_the_whole_chain_for_the_next_menu) {
    MenuBarFixture mf;
    bool chose_text = false;
    open_the_submenu(mf, chose_text);  // standing on "Sheet", which opens nothing

    mf.app.dispatch(key(Key::Right));
    CK_CHECK(mf.desktop.popups().size() == 1);  // the submenu went with its parent
    CK_CHECK(static_cast<DropdownMenu*>(mf.desktop.popups()[0])->items()[0].label() == "&Tile");
    CK_CHECK(!chose_text);
}

CK_TEST(the_highlight_a_bar_reports_follows_the_reader_into_a_submenu) {
    // Declared before the fixture so it is destroyed after it: tearing the
    // bar down closes its drop-down, which reports one last highlight, and
    // an observer capturing something already off the stack is read there.
    std::vector<CommandId> reported;
    MenuBarFixture mf;
    const MenuItem new_item = MenuItem::submenu("&New", {MenuItem::command(ckv::widgets::CommandPresentation{standard(mf.app).quit})});
    auto bar_owned =
        std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {new_item}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->on_highlight_changed = [&reported](const ckv::widgets::MenuHighlight& highlight) {
        reported.push_back(highlight.command);
    };

    bar->activate();
    mf.app.dispatch(key(Key::Enter));  // "New" runs nothing; it only opens
    CK_CHECK(!reported.empty());
    CK_CHECK(reported.back() == ui::kInvalidCommand);

    mf.app.dispatch(key(Key::Right));
    CK_CHECK(reported.back() == standard(mf.app).quit);  // what the reader is now on

    mf.app.dispatch(key(Key::Left));
    CK_CHECK(reported.back() == ui::kInvalidCommand);  // and back on the parent entry
}

// --- ...and a pointer destination, through the whole chain ----------------

namespace {
// The pointer tests go through Application::dispatch rather than calling
// on_mouse directly, because what they are about IS the dispatch: input
// capture moves to a submenu the instant it opens, and the release that ends
// the press which opened it arrives afterwards. That needs a Desktop both
// under app.root() — so the hit-test and capture can reach it — and the bar's
// own parent, for MenuBar's parent-walk.
struct PointerMenuFixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    Fixture f;
    Desktop* desktop = nullptr;
    MenuBar* bar = nullptr;
    bool chose_text = false;

    PointerMenuFixture() {
        app.root().set_context(ui::Context{&f.theme, &f.registry, &app});
        desktop = static_cast<Desktop*>(
            app.root().add_child(std::make_unique<Desktop>(Rect{0, 0, 80, 24})));
        auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
            {"&File", {MenuItem::submenu("&New", {MenuItem::action("&Sheet", [] {}),
                                                   MenuItem::action("&Text", [this] { chose_text = true; })}),
                        MenuItem::action("&Close", [] {})}},
            {"&Window", {MenuItem::action("&Tile", [] {})}},
        });
        bar = static_cast<MenuBar*>(desktop->add_child(std::move(bar_owned)));
        bar->set_bounds(Rect{0, 0, 80, 1});
    }

    DropdownMenu* popup(std::size_t index) const {
        return static_cast<DropdownMenu*>(desktop->popups()[index]);
    }
    std::size_t popups() const noexcept { return desktop->popups().size(); }

    void send(ckv::MouseAction action, ckv::Point cell) {
        app.dispatch(ckv::MouseEvent{action, ckv::MouseButton::Left, cell, std::nullopt, Modifier::None});
    }
    // A press and release on the File title, which is how a reader opens it.
    void click_the_file_title() {
        send(ckv::MouseAction::Down, ckv::Point{3, 0});
        send(ckv::MouseAction::Up, ckv::Point{3, 0});
    }
    // A cell on one row's label: inside the frame, clear of both borders.
    static ckv::Point row(const DropdownMenu& menu, int index) {
        const Rect b = menu.absolute_bounds();
        return ckv::Point{b.x + 2, b.y + 1 + index};
    }
};
}  // namespace

CK_TEST(the_release_that_ends_the_press_which_opened_a_submenu_leaves_it_open) {
    PointerMenuFixture pf;
    pf.click_the_file_title();
    CK_CHECK(pf.popups() == 1);
    const ckv::Point on_new = PointerMenuFixture::row(*pf.popup(0), 0);

    pf.send(ckv::MouseAction::Down, on_new);
    CK_CHECK(pf.popups() == 2);  // the press opens it

    // ...and the release does not take it straight back again. The submenu
    // holds the capture by now, and this release is over a point it does not
    // contain — read as a click outside itself it dismissed the menu in the
    // same breath as opening it.
    pf.send(ckv::MouseAction::Up, on_new);
    CK_CHECK(pf.popups() == 2);
}

CK_TEST(a_submenu_row_can_be_clicked_once_the_submenu_stays_up) {
    PointerMenuFixture pf;
    pf.click_the_file_title();
    const ckv::Point on_new = PointerMenuFixture::row(*pf.popup(0), 0);
    pf.send(ckv::MouseAction::Down, on_new);
    pf.send(ckv::MouseAction::Up, on_new);

    const ckv::Point on_text = PointerMenuFixture::row(*pf.popup(1), 1);
    pf.send(ckv::MouseAction::Move, on_text);
    CK_CHECK(pf.popup(1)->highlight().command == ui::kInvalidCommand);  // an action row
    pf.send(ckv::MouseAction::Down, on_text);
    pf.send(ckv::MouseAction::Up, on_text);

    CK_CHECK(pf.chose_text);
    CK_CHECK(pf.popups() == 0);
    CK_CHECK(!pf.bar->active());
}

CK_TEST(a_press_on_the_parent_row_can_be_dragged_into_the_submenu_and_released_there) {
    PointerMenuFixture pf;
    pf.click_the_file_title();
    const ckv::Point on_new = PointerMenuFixture::row(*pf.popup(0), 0);

    pf.send(ckv::MouseAction::Down, on_new);  // one gesture, begun on the parent...
    const ckv::Point on_text = PointerMenuFixture::row(*pf.popup(1), 1);
    pf.send(ckv::MouseAction::Move, on_text);
    pf.send(ckv::MouseAction::Up, on_text);   // ...and ended in the child

    CK_CHECK(pf.chose_text);
    CK_CHECK(pf.popups() == 0);
}

CK_TEST(the_pointer_moving_back_onto_a_row_without_children_closes_the_submenu) {
    PointerMenuFixture pf;
    pf.click_the_file_title();
    DropdownMenu* root = pf.popup(0);
    pf.send(ckv::MouseAction::Down, PointerMenuFixture::row(*root, 0));
    pf.send(ckv::MouseAction::Up, PointerMenuFixture::row(*root, 0));
    CK_CHECK(pf.popups() == 2);

    // The submenu holds the capture, so this move only reaches the parent
    // because the chain hands it to the menu the pointer is actually over.
    pf.send(ckv::MouseAction::Move, PointerMenuFixture::row(*root, 1));
    CK_CHECK(pf.popups() == 1);
    CK_CHECK(pf.desktop->popups()[0] == root);
    CK_CHECK(pf.app.input_capture() == root);
    CK_CHECK(root->highlighted() == 1);  // standing on "Close"
}

CK_TEST(a_press_outside_every_menu_of_the_chain_closes_all_of_them_at_once) {
    PointerMenuFixture pf;
    pf.click_the_file_title();
    pf.send(ckv::MouseAction::Down, PointerMenuFixture::row(*pf.popup(0), 0));
    pf.send(ckv::MouseAction::Up, PointerMenuFixture::row(*pf.popup(0), 0));
    CK_CHECK(pf.popups() == 2);

    pf.send(ckv::MouseAction::Down, ckv::Point{70, 20});
    CK_CHECK(pf.popups() == 0);  // not one level, and not needing a second click
}

namespace {
// A focusable view that counts the presses it is handed.
class PressProbe final : public ckv::ui::View {
public:
    PressProbe() { set_focus_policy(ckv::ui::FocusPolicy::TabStop); }
    bool on_mouse(const ckv::MouseEvent& event) override {
        if (event.action == ckv::MouseAction::Down) ++presses;
        return true;
    }
    int presses = 0;
};

// An application as a reader meets it: a desktop filling the terminal, a
// docked menu bar, and a window holding the focused field and, well clear of
// every menu, a view that counts presses.
struct LightDismissFixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    StandardRoles roles = intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;
    MenuBar* bar = nullptr;
    ckv::widgets::InputLine* field = nullptr;
    PressProbe* probe = nullptr;
    int opened = 0;

    LightDismissFixture() {
        app.theme() = make_classic_theme(app.roles(), roles);
        desktop = static_cast<Desktop*>(app.root().add_child(std::make_unique<Desktop>(Rect{0, 0, 80, 24})));
        bar = desktop->dock_top(std::make_unique<MenuBar>(std::vector<MenuBarItem>{
            {"&File", {MenuItem::submenu("&New", {MenuItem::action("&Text", [] {})}),
                       MenuItem::action("&Open", [this] { ++opened; })}}}));
        auto* window = desktop->add_window(std::make_unique<ckv::widgets::Window>("Document"));
        window->set_bounds(Rect{30, 5, 40, 12});
        field = window->add(std::make_unique<ckv::widgets::InputLine>());
        field->set_bounds(Rect{2, 2, 20, 1});
        probe = window->add(std::make_unique<PressProbe>());
        probe->set_bounds(Rect{2, 6, 20, 3});
        app.set_focus(field);
        app.step(0);
    }

    void mouse(ckv::MouseAction action, ckv::Point cell) {
        term.inject_event(ckv::MouseEvent{action, ckv::MouseButton::Left, cell, std::nullopt, Modifier::None});
        app.step(0);
    }
    void press_key(Key k) {
        term.inject_event(ckv::KeyEvent{KeyChord{k, Modifier::None, ""}});
        app.step(0);
    }
    ckv::Point probe_cell() const { return ckv::Point{probe->absolute_bounds().x + 3, probe->absolute_bounds().y + 1}; }
};
}  // namespace

CK_TEST(a_press_outside_an_open_menu_ends_the_menu_system_and_is_consumed) {
    // The light dismiss (the architecture §5): a press outside every menu of
    // the chain closes them all, deactivates the bar and puts the focus back
    // where the menu found it -- and it is only a dismissal: the view under
    // the pointer never hears the press.
    LightDismissFixture lf;
    lf.mouse(ckv::MouseAction::Down, ckv::Point{3, 0});  // the File title
    lf.mouse(ckv::MouseAction::Up, ckv::Point{3, 0});
    CK_CHECK(lf.desktop->popups().size() == 1U);
    CK_CHECK(lf.bar->active());

    lf.mouse(ckv::MouseAction::Down, lf.probe_cell());
    lf.mouse(ckv::MouseAction::Up, lf.probe_cell());
    CK_CHECK(lf.desktop->popups().empty());
    CK_CHECK(!lf.bar->active());
    CK_CHECK(lf.app.focused() == lf.field);
    CK_CHECK(lf.app.input_capture() == nullptr);
    CK_CHECK(lf.probe->presses == 0);
    CK_CHECK(lf.opened == 0);

    // With the menu gone, the same press is an ordinary click again.
    lf.mouse(ckv::MouseAction::Down, lf.probe_cell());
    lf.mouse(ckv::MouseAction::Up, lf.probe_cell());
    CK_CHECK(lf.probe->presses == 1);
}

CK_TEST(a_press_outside_a_keyboard_opened_submenu_chain_restores_the_focus_it_found) {
    LightDismissFixture lf;
    lf.press_key(Key::F10);
    lf.press_key(Key::Enter);  // File drops
    lf.press_key(Key::Right);  // into New's submenu
    CK_CHECK(lf.desktop->popups().size() == 2U);
    CK_CHECK(lf.app.focused() == lf.bar);

    lf.mouse(ckv::MouseAction::Down, ckv::Point{75, 20});  // bare desktop
    lf.mouse(ckv::MouseAction::Up, ckv::Point{75, 20});
    CK_CHECK(lf.desktop->popups().empty());
    CK_CHECK(!lf.bar->active());
    CK_CHECK(lf.app.focused() == lf.field);
    // The keyboard is the field's again: a key typed now lands in it.
    lf.term.inject_event(ckv::TextEvent{"x", false});
    lf.app.step(0);
    CK_CHECK(lf.field->text() == "x");
}

// --- Regression: Window::toggle_zoom bypassing clamp_size (review finding #4) ---

CK_TEST(zooming_into_an_area_smaller_than_the_windows_minimum_size_still_clamps) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    ckv::widgets::Window window("W");
    window.set_min_size(ckv::Size{20, 10});
    window.set_bounds(Rect{0, 0, 25, 12});
    window.toggle_zoom(Rect{0, 0, 5, 5});  // far smaller than the configured minimum
    CK_CHECK(window.bounds().width == 20);
    CK_CHECK(window.bounds().height == 10);
}

// --- show_context_menu ---------------------------------------------------

CK_TEST(show_context_menu_opens_a_popup_with_input_capture_at_the_given_position) {
    MenuBarFixture mf;
    auto* popup = ckv::widgets::show_context_menu(
        {MenuItem::action("&Copy", {}), MenuItem::action("&Paste", {})},
        ckv::Point{10, 10}, mf.app, mf.desktop);
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(mf.app.input_capture() == popup);
    CK_CHECK(popup->bounds().x == 10);
    CK_CHECK(popup->bounds().y == 10);
}

CK_TEST(show_context_menu_self_removes_and_clears_capture_on_escape) {
    MenuBarFixture mf;
    auto* popup = ckv::widgets::show_context_menu({MenuItem::action("&Copy", {})},
                                                    ckv::Point{10, 10}, mf.app, mf.desktop);
    CK_CHECK(mf.app.input_capture() == popup);
    CK_CHECK(mf.app.dispatch(key(Key::Escape)));
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(mf.app.input_capture() == nullptr);
}

CK_TEST(a_context_menu_receives_dispatched_keys_and_restores_focus) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.theme() = make_classic_theme(app.roles(), intern_standard_roles(app.roles()));
    auto desktop = std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* host = desktop.get();
    app.root().add(std::move(desktop));
    auto origin = std::make_unique<ckv::ui::View>(Rect{2, 2, 10, 2}, ckv::ui::FocusPolicy::TabStop);
    ckv::ui::View* previous = host->add(std::move(origin));
    app.set_focus(previous);

    DropdownMenu* popup = show_context_menu({MenuItem::action("&Copy", {})},
                                            ckv::Point{10, 10}, app, *host);
    CK_CHECK(app.focused() == popup);
    CK_CHECK(app.dispatch(key(Key::Escape)));
    CK_CHECK(host->popups().empty());
    CK_CHECK(app.input_capture() == nullptr);
    CK_CHECK(app.focused() == previous);
}

CK_TEST(application_teardown_safely_closes_an_open_context_menu) {
    struct TeardownProbe final : ckv::ui::View {
        explicit TeardownProbe(bool& destroyed)
            : View(Rect{2, 2, 10, 2}, ckv::ui::FocusPolicy::TabStop), destroyed_(destroyed) {}
        ~TeardownProbe() override { destroyed_ = true; }
        bool& destroyed_;
    };

    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    bool origin_destroyed = false;
    {
        Application app(term, clock);
        app.theme() = make_classic_theme(app.roles(), intern_standard_roles(app.roles()));
        auto desktop = std::make_unique<Desktop>(Rect{0, 0, 80, 24});
        Desktop* host = desktop.get();
        app.root().add(std::move(desktop));
        auto origin = std::make_unique<TeardownProbe>(origin_destroyed);
        ckv::ui::View* previous = host->add(std::move(origin));
        app.set_focus(previous);

        DropdownMenu* popup = show_context_menu(
            {MenuItem::action("&Copy", {})}, ckv::Point{10, 10}, app, *host);
        CK_CHECK(app.focused() == popup);
        CK_CHECK(host->popups().size() == 1);
        // Leave the menu open. Application owns the root teardown, so the
        // popup's ordinary dismissal and focus restoration must be safe even
        // while its Desktop subtree is being destroyed.
    }
    CK_CHECK(origin_destroyed);
}

CK_TEST(a_context_menu_keeps_the_invoking_views_command_context_while_open) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    app.theme() = make_classic_theme(app.roles(), intern_standard_roles(app.roles()));
    auto desktop = std::make_unique<Desktop>(Rect{0, 0, 80, 24});
    Desktop* host = desktop.get();
    app.root().add(std::move(desktop));
    auto origin = std::make_unique<ckv::ui::View>(Rect{2, 2, 10, 2}, ckv::ui::FocusPolicy::TabStop);
    origin->set_command_context("document");
    ckv::ui::View* previous = host->add(std::move(origin));
    app.set_focus(previous);
    const ui::CommandId save = app.commands().declare(
        {.key = "test.context-menu-save", .title = "Save", .scope = {.contexts = {"document"}}});
    bool ran = false;
    app.set_command_handler(save, [&] { ran = true; });

    DropdownMenu* popup = show_context_menu(
        {MenuItem::command(save)}, ckv::Point{10, 10}, app, *host);
    CK_CHECK(popup->highlight().enabled);
    CK_CHECK(app.dispatch(key(Key::Enter)));
    CK_CHECK(ran);
    CK_CHECK(app.focused() == previous);
}

CK_TEST(show_context_menu_activating_an_item_closes_it_and_runs_the_action) {
    MenuBarFixture mf;
    bool ran = false;
    auto* popup = ckv::widgets::show_context_menu({MenuItem::action("&Copy", [&] { ran = true; })},
                                                    ckv::Point{10, 10}, mf.app, mf.desktop);
    CK_CHECK(mf.app.input_capture() == popup);
    CK_CHECK(mf.app.dispatch(key(Key::Enter)));
    CK_CHECK(ran);
    CK_CHECK(mf.desktop.popups().empty());
}

CK_TEST(shift_f10_is_the_portable_keyboard_context_menu_request) {
    CK_CHECK(is_keyboard_context_menu_request(key(Key::F10, Modifier::Shift)));
    CK_CHECK(!is_keyboard_context_menu_request(key(Key::F10)));
    CK_CHECK(!is_keyboard_context_menu_request(key(Key::F9, Modifier::Shift)));
    CK_CHECK(!is_keyboard_context_menu_request(ckv::KeyEvent{KeyChord{Key::F10, Modifier::Shift, ""},
                                                             ckv::KeyAction::Release}));
}

CK_TEST(show_context_menu_for_focus_opens_at_the_focused_views_cell) {
    MenuBarFixture mf;
    auto* focused = mf.desktop.add(std::make_unique<ckv::ui::View>());
    focused->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    focused->set_bounds(Rect{12, 7, 8, 2});
    mf.app.set_focus(focused);

    DropdownMenu* popup = show_context_menu_for_focus(
        {MenuItem::action("&Copy", {})}, mf.app, mf.desktop);

    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(mf.app.input_capture() == popup);
    CK_CHECK(popup->bounds().x == 12);
    CK_CHECK(popup->bounds().y == 7);
}

CK_TEST(show_context_menu_for_focus_falls_back_to_the_desktop_origin_when_focus_is_elsewhere) {
    MenuBarFixture mf;
    auto* outside = mf.app.root().add(std::make_unique<ckv::ui::View>());
    outside->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    mf.app.set_focus(outside);

    DropdownMenu* popup = show_context_menu_for_focus(
        {MenuItem::action("&Copy", {})}, mf.app, mf.desktop);

    CK_CHECK(popup->bounds().x == 0);
    CK_CHECK(popup->bounds().y == 0);
}

// --- Alt+<mnemonic> menu accelerators ---------------------------------
//
// Opening a menu by its mnemonic has to work from wherever the reader
// happens to be, not only once the bar already holds focus — that is what
// makes it an accelerator rather than a second navigation key.

CK_TEST(alt_mnemonic_opens_its_menu_without_the_bar_being_active_first) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {MenuItem::action("&New", {})}},
        {"&Help", {MenuItem::action("&About", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->set_bounds(Rect{0, 0, 30, 1});

    // Nothing has focused the bar; the chord alone must reach it.
    CK_CHECK(mf.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "h"}}));
    CK_CHECK(mf.desktop.popups().size() == 1u);
    const auto* dropdown = dynamic_cast<const DropdownMenu*>(mf.desktop.popups().front());
    CK_CHECK(dropdown != nullptr);
    if (dropdown == nullptr) return;
    CK_CHECK(dropdown->items().size() == 1u);
}

CK_TEST(menu_accelerators_are_withdrawn_when_the_menus_are_replaced) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&Help", {MenuItem::action("&About", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->set_bounds(Rect{0, 0, 30, 1});
    CK_CHECK(mf.app.commands().command_for_key(KeyChord{Key::Char, Modifier::Alt, "h"}).has_value());

    bar->set_menus(std::vector<MenuBarItem>{{"&File", {}}});
    // The old chord must not survive to open a menu that no longer exists.
    CK_CHECK(!mf.app.commands().command_for_key(KeyChord{Key::Char, Modifier::Alt, "h"}).has_value());
    CK_CHECK(mf.app.commands().command_for_key(KeyChord{Key::Char, Modifier::Alt, "f"}).has_value());
}

CK_TEST(rebuilding_the_same_menu_titles_keeps_the_accelerators_and_leaves_the_registry_alone) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}, {"&Help", {}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->set_bounds(Rect{0, 0, 30, 1});
    const auto help = mf.app.commands().command_for_key(KeyChord{Key::Char, Modifier::Alt, "h"});
    CK_CHECK(help.has_value());
    const std::uint64_t revision = mf.app.commands().revision();

    // New items under the same titles: the accelerators are what they were,
    // so nothing a surface shows has changed and nothing need repaint for it.
    bar->set_menus({{"&File", {MenuItem::action("&Open", {}), MenuItem::action("&Close", {})}}, {"&Help", {}}});
    CK_CHECK(mf.app.commands().revision() == revision);
    CK_CHECK(mf.app.commands().command_for_key(KeyChord{Key::Char, Modifier::Alt, "h"}) == help);
    CK_CHECK(mf.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "f"}}));
    const auto* dropdown = dynamic_cast<const DropdownMenu*>(mf.desktop.popups().front());
    CK_CHECK(dropdown != nullptr && dropdown->items().size() == 2u);
}

// --- Press-drag menu opening ------------------------------------------
//
// Pressing on a menu title opens it with nothing selected: the pointer is
// the indicator, and highlighting an item the reader has not pointed at
// would claim a choice they never made. The selection appears when the
// press ends, so the keyboard has a definite place to carry on from.

CK_TEST(a_pointer_opened_menu_starts_with_nothing_selected) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {MenuItem::action("&New", {}),
                   MenuItem::action("&Open", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->set_bounds(Rect{0, 0, 30, 1});

    bar->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 0},
                                  std::nullopt, Modifier::None});
    auto* dropdown = dynamic_cast<DropdownMenu*>(mf.desktop.popups().front());
    CK_CHECK(dropdown != nullptr);
    if (dropdown == nullptr) return;
    CK_CHECK(dropdown->highlighted() == -1);  // the press alone selects nothing

    // Releasing over the title, having pointed at no item, settles on the
    // first one — that is the state the arrow keys move from.
    bar->on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{2, 0},
                                  std::nullopt, Modifier::None});
    CK_CHECK(dropdown->highlighted() == 0);
}

CK_TEST(a_keyboard_opened_menu_selects_its_first_item_at_once) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {MenuItem::action("&New", {}),
                   MenuItem::action("&Open", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->set_bounds(Rect{0, 0, 30, 1});

    // No pointer is involved, so there is no other indicator of position.
    bar->activate();
    mf.app.dispatch(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
    auto* dropdown = dynamic_cast<DropdownMenu*>(mf.desktop.popups().front());
    CK_CHECK(dropdown != nullptr);
    if (dropdown == nullptr) return;
    CK_CHECK(dropdown->highlighted() == 0);
}

CK_TEST(dragging_from_the_title_onto_an_item_highlights_it_before_release) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {MenuItem::action("&New", {}),
                   MenuItem::action("&Open", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->set_bounds(Rect{0, 0, 30, 1});

    bar->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 0},
                                  std::nullopt, Modifier::None});
    auto* dropdown = dynamic_cast<DropdownMenu*>(mf.desktop.popups().front());
    CK_CHECK(dropdown != nullptr);
    if (dropdown == nullptr) return;
    const Rect popup = dropdown->absolute_bounds();
    // Onto the first item: the pointer is now the indicator.
    dropdown->on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left,
                                       ckv::Point{popup.x + 1, popup.y + 1}, std::nullopt,
                                       Modifier::None});
    CK_CHECK(dropdown->highlighted() == 0);
    // Releasing there acts on the item the pointer chose, not on the one
    // the menu happened to open with.
    dropdown->on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left,
                                       ckv::Point{popup.x + 1, popup.y + 1}, std::nullopt,
                                       Modifier::None});
    CK_CHECK(mf.desktop.popups().empty());
}

CK_TEST(a_menu_item_can_state_the_chord_its_application_actually_uses) {
    // The registry's chord is one KeyChord; an application may reach the same
    // command by a sequence the keymap cannot hold. The menu then has to
    // advertise the application's spelling, or it teaches a key that does
    // nothing where the reader is standing.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId save_command = app.commands().declare(
        {.key = "test.save", .title = "&Save", .category = "File", .chord = "Ctrl+S"});
    auto menu = make_dropdown(
        f, app,
        {ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{save_command, "&Save", "^B s"})});
    menu->set_bounds(Rect{0, 0, 20, 1});

    Surface s(ckv::Size{20, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 1});
    menu->draw(painter);

    const std::string row = row_text(s, 0);
    const auto hint_pos = row.find("^B s");
    CK_CHECK(hint_pos != std::string::npos);
    CK_CHECK(hint_pos + 4 == 19);  // still right-aligned
    CK_CHECK(row.find("Ctrl+S") == std::string::npos);
    // Presentation only: the registry binding is untouched and still fires.
    CK_CHECK(app.commands().command_for_key(KeyChord{Key::Char, Modifier::Ctrl, "s"}) == save_command);
}

// --- Home and End: the ends of the menu the reader is in ------------------

CK_TEST(home_and_end_reach_the_first_and_last_choosable_row) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app,
                              {MenuItem::separator(), MenuItem::action("&One", {}),
                               MenuItem::action("&Two", {}), MenuItem::action("T&hree", {}),
                               MenuItem::separator()});
    // Past the separators at both ends: End lands on a row that can be
    // chosen, not on the divider that happens to be last.
    menu->on_key(key(Key::End));
    CK_CHECK(menu->highlighted() == 3);
    menu->on_key(key(Key::Home));
    CK_CHECK(menu->highlighted() == 1);
}

CK_TEST(home_and_end_act_on_the_submenu_the_reader_is_standing_in) {
    MenuBarFixture mf;
    const MenuItem more = MenuItem::submenu(
        "&More", {MenuItem::action("&A", {}), MenuItem::action("&B", {}), MenuItem::action("&C", {})});
    DropdownMenu* root = show_context_menu({more}, ckv::Point{2, 2}, mf.app, mf.desktop);
    CK_CHECK(mf.app.dispatch(key(Key::Right)));
    auto* child = static_cast<DropdownMenu*>(mf.desktop.popups()[1]);
    CK_CHECK(mf.app.dispatch(key(Key::End)));
    CK_CHECK(child->highlighted() == 2);   // the submenu moved
    CK_CHECK(root->highlighted() == 0);    // and the parent did not
}

CK_TEST(the_menu_bar_ends_are_its_first_and_last_menu) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {MenuItem::action("a", {})}},
        {"&Edit", {MenuItem::action("b", {})}},
        {"&Help", {MenuItem::action("c", {})}}});
    MenuBar* bar = mf.add_bar(std::move(bar_owned));
    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::End)));
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(static_cast<DropdownMenu*>(mf.desktop.popups()[0])->items()[0].label() == "c");
}

// --- The pointer reaches submenus too -------------------------------------

CK_TEST(hovering_a_row_with_children_opens_them_and_leaving_closes_them) {
    MenuBarFixture mf;
    const MenuItem more = MenuItem::submenu("&More", {MenuItem::action("&Child", [] {})});
    DropdownMenu* root =
        show_context_menu({more, MenuItem::action("&Plain", {})}, ckv::Point{2, 2}, mf.app, mf.desktop);
    const ckv::Rect at = root->absolute_bounds();
    const auto move_to = [&](int row) {
        return ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left,
                               ckv::Point{at.x + 2, at.y + 1 + row}, std::nullopt,
                               ckv::Modifier::None};
    };
    root->on_mouse(move_to(1));  // the plain row: nothing open
    CK_CHECK(mf.desktop.popups().size() == 1);
    root->on_mouse(move_to(0));  // onto the parent row
    CK_CHECK(mf.desktop.popups().size() == 2);  // the submenu followed the pointer
    root->on_mouse(move_to(1));
    CK_CHECK(mf.desktop.popups().size() == 1);  // and left with it
}

// --- What a menu says about the row under the highlight -------------------

CK_TEST(the_highlight_carries_the_help_topic_and_the_reason_a_row_is_grey) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(
        f, app,
        {MenuItem::action("&Fill form", {})
             .with_help("pdf.forms")
             .with_disabled_reason("this document has no form fields")
             .with_enabled(false),
         MenuItem::action("&Plain", {})});
    ckv::widgets::MenuHighlight seen;
    menu->on_highlight_changed = [&seen](const ckv::widgets::MenuHighlight& h) { seen = h; };

    // The menu already opens on row 0, so read the highlight directly for
    // what it carries, and drive a real move to prove it is reported.
    CK_CHECK(menu->highlight().help_context == "pdf.forms");
    menu->on_key(key(Key::Down));
    menu->on_key(key(Key::Up));
    CK_CHECK(seen.help_context == "pdf.forms");
    CK_CHECK(seen.disabled_reason == "this document has no form fields");
    CK_CHECK(!seen.enabled);
    CK_CHECK(!seen.none);
}

CK_TEST(a_command_row_cannot_be_told_it_is_available_when_its_command_is_not) {
    // Enablement has one source per kind, so a menu cannot disagree with
    // the palette about whether a verb can be used.
    CK_EXPECT_ABORT({ (void)MenuItem::command(ui::kInvalidCommand).with_enabled(false); });
}

CK_TEST(activating_the_trailing_title_from_the_keyboard_ends_the_walk_first) {
    // Enter on the trailing title hands off to whatever the title opens -- a
    // calendar, say -- and that hand-off ends the walk the way choosing an item
    // does. Done in the other order, the calendar's modal scope saved a focused
    // BAR and gave it back when the calendar closed: a bar standing highlighted
    // on its first menu that nobody had asked for.
    MenuBarFixture mf;
    auto* other = mf.app.root().add_child(std::make_unique<ckv::ui::View>());
    other->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    mf.app.set_focus(other);
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);

    bar->activate();
    mf.app.dispatch(key(Key::Left));  // onto the trailing title
    CK_CHECK(probe->highlighted());
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(probe->activations == 1);
    CK_CHECK(!bar->active());
    CK_CHECK(!probe->highlighted());
    CK_CHECK(mf.app.focused() == other);
}

CK_TEST(a_pointer_on_the_trailing_title_ends_the_walk_before_the_title_reacts) {
    MenuBarFixture mf;
    auto* other = mf.app.root().add_child(std::make_unique<ckv::ui::View>());
    other->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    mf.app.set_focus(other);
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);

    bar->activate();
    CK_CHECK(bar->active());
    // What the Application tells every ancestor of a pointer-down target
    // before the target itself hears of it (the architecture §5, pointer
    // delivery): the bar sees the press on its trailing title coming.
    bar->on_descendant_mouse_down(*probe);
    CK_CHECK(!bar->active());
    CK_CHECK(!probe->highlighted());
    CK_CHECK(mf.app.focused() == other);
}

CK_TEST(losing_focus_to_another_view_ends_the_walk_and_its_trailing_highlight) {
    // Focus can leave the bar without the bar's own doing -- a view opened
    // from the trailing title takes it. Whatever the walk had lit, the bar's
    // title or the trailing one, goes out with it, and the bookmark goes too:
    // a deactivate() that came later must not carry focus back to a view the
    // reader left long ago.
    MenuBarFixture mf;
    auto* other = mf.app.root().add_child(std::make_unique<ckv::ui::View>());
    other->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    auto* taker = mf.app.root().add_child(std::make_unique<ckv::ui::View>());
    taker->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    mf.app.set_focus(other);
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);

    bar->activate();
    mf.app.dispatch(key(Key::Left));
    CK_CHECK(probe->highlighted());
    mf.app.set_focus(taker);
    CK_CHECK(!bar->active());
    CK_CHECK(!probe->highlighted());
    bar->deactivate();  // nothing to end; must not move focus anywhere
    CK_CHECK(mf.app.focused() == taker);
}

// --- Room: the trailing view, and popups wider than their desktop -----------

CK_TEST(a_trailing_view_is_given_no_width_where_it_would_cover_a_menu_title) {
    // A clock laid over a menu title hides the only way into that menu, so
    // the titles come first and the trailing view takes what is left.
    MenuBarFixture mf;
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);
    // "Help" ends at column 11; after one cell of gap the probe's five
    // columns start at 13, so the bar must be 18 wide to hold it.
    bar->set_bounds(Rect{0, 0, 18, 1});
    CK_CHECK(probe->bounds().width == 5);
    CK_CHECK(probe->bounds().right() == 18);
    bar->set_bounds(Rect{0, 0, 17, 1});
    CK_CHECK(probe->bounds().width == 0);
}

CK_TEST(replacing_the_menus_lays_the_trailing_view_out_again) {
    MenuBarFixture mf;
    TrailingProbe* probe = nullptr;
    MenuBar* bar = bar_with_trailing_probe(mf, &probe);
    bar->set_bounds(Rect{0, 0, 20, 1});
    CK_CHECK(probe->bounds().width == 5);

    bar->set_menus({{"&File", {MenuItem::action("&Open", {})}},
                    {"&Help", {MenuItem::action("&About", {})}},
                    {"&Options", {MenuItem::action("&Colors", {})}}});
    CK_CHECK(probe->bounds().width == 0);

    bar->set_menus({{"&File", {MenuItem::action("&Open", {})}}});
    CK_CHECK(probe->bounds().width == 5);
    CK_CHECK(probe->bounds().right() == 20);
}

CK_TEST(an_unavailable_row_under_the_highlight_keeps_the_bar_in_the_disabled_foreground) {
    // The reader walking a menu has to see where they are, even on a row
    // that will not act.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId unavailable = app.commands().declare({.key = "test.unavailable", .title = "Unavailable"});
    app.commands().set_enabled_predicate(unavailable, [] { return false; });
    auto menu = make_dropdown(f, app, {MenuItem::command(unavailable), MenuItem::action("Two", {})});
    const int width = menu->horizontal_size_hint().preferred;
    const int height = menu->vertical_size_hint().preferred;
    menu->set_bounds(Rect{0, 0, width, height});
    CK_CHECK(menu->highlighted() == 0);

    Surface surface(ckv::Size{width, height}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, width, height});
    menu->draw(painter);
    const ckv::Style highlighted = f.theme.resolve(f.roles.menu_dropdown_highlighted);
    const ckv::Style disabled = f.theme.resolve(f.roles.menu_dropdown_disabled);
    CK_CHECK(surface.at(ckv::Point{2, 1}).grapheme() == "U");
    for (const ckv::Point cell : {ckv::Point{1, 1}, ckv::Point{2, 1}, ckv::Point{width - 2, 1}}) {
        CK_CHECK(surface.at(cell).style().bg == highlighted.bg);
        CK_CHECK(surface.at(cell).style().fg == disabled.fg);
    }
    CK_CHECK(surface.at(ckv::Point{2, 2}).style() == f.theme.resolve(f.roles.menu_dropdown_normal));
}

CK_TEST(a_disabled_row_draws_its_mnemonic_like_the_rest_of_its_label) {
    // D-076: a disabled control shows no mnemonic accent. The letter a row
    // would answer to is drawn in the row's own disabled style, whether or
    // not the highlight stands on it, while an enabled row keeps its accent.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId unavailable = app.commands().declare({.key = "test.unavailable", .title = "&Unavailable"});
    app.commands().set_enabled_predicate(unavailable, [] { return false; });
    auto menu = make_dropdown(
        f, app, {MenuItem::action("&Open", {}), MenuItem::command(unavailable), MenuItem::action("&Close", {})});
    const int width = menu->horizontal_size_hint().preferred;
    const int height = menu->vertical_size_hint().preferred;
    menu->set_bounds(Rect{0, 0, width, height});
    const ckv::Style disabled = f.theme.resolve(f.roles.menu_dropdown_disabled);
    const ckv::Style hotkey = f.theme.resolve(f.roles.hotkey);
    CK_CHECK(hotkey.fg != disabled.fg);

    const auto render = [&] {
        Surface surface(ckv::Size{width, height}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
        Painter painter(surface, Rect{0, 0, width, height});
        menu->draw(painter);
        return surface;
    };

    // Resting: the disabled row's mnemonic cell is styled exactly like the
    // cell after it, in the disabled style.
    const Surface resting = render();
    CK_CHECK(resting.at(ckv::Point{2, 2}).grapheme() == "U");
    CK_CHECK(resting.at(ckv::Point{2, 2}).style() == disabled);
    CK_CHECK(resting.at(ckv::Point{2, 2}).style() == resting.at(ckv::Point{3, 2}).style());
    // The enabled row below it keeps its accent.
    CK_CHECK(resting.at(ckv::Point{2, 3}).grapheme() == "C");
    CK_CHECK(resting.at(ckv::Point{2, 3}).style().fg == hotkey.fg);

    // Under the highlight: still no accent; the bar is in the disabled foreground.
    CK_CHECK(menu->on_key(key(Key::Down)));
    CK_CHECK(menu->highlighted() == 1);
    const Surface lit = render();
    CK_CHECK(lit.at(ckv::Point{2, 2}).style() == lit.at(ckv::Point{3, 2}).style());
    CK_CHECK(lit.at(ckv::Point{2, 2}).style().fg == disabled.fg);
}

CK_TEST(a_popup_wider_than_its_desktop_is_narrowed_to_it_and_elides_its_labels) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    Desktop desktop{Rect{0, 0, 12, 10}};
    desktop.set_context(ui::Context{&f.theme, &f.registry, &app});
    DropdownMenu* menu = show_context_menu(
        {MenuItem::action("&Preferences and settings", {}), MenuItem::action("&Quit", {})}, ckv::Point{0, 0}, app,
        desktop);
    CK_CHECK(menu->horizontal_size_hint().preferred > 12);
    const Rect bounds = menu->bounds();
    CK_CHECK(bounds.x == 0);
    CK_CHECK(bounds.width == 12);

    Surface surface(ckv::Size{bounds.width, bounds.height}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, bounds.width, bounds.height});
    menu->draw(painter);
    // Both sides of the frame stand, and the long label says it was shortened.
    CK_CHECK(row_text(surface, 0) == "┌──────────┐");
    CK_CHECK(row_text(surface, 1) == "│ Prefere… │");
    CK_CHECK(row_text(surface, 2) == "│ Quit     │");
    CK_CHECK(menu->on_key(key(Key::Escape)));
    CK_CHECK(desktop.popups().empty());
}

// --- End-to-end scripts: the whole menu system through public input ------
//
// Two Application-level scripts drive a menu bar, its dropdowns, a nested
// submenu, a checkable and a disabled row and a context menu -- one by
// keyboard alone, one by mouse alone. ApplicationShell mounts a real Desktop
// in the root and docks the bar, and every event takes the path a terminal's
// input takes: HeadlessTerminal::inject_event, then Application::step.

namespace {

struct MenuScript {
    ckv::term::HeadlessTerminal term{ckv::Size{60, 16}};
    ManualClock clock;
    Application app{term, clock};
    StandardRoles roles = intern_standard_roles(app.roles());
    std::vector<std::string> log;
    bool wrap = false;
    ckv::widgets::ApplicationShell shell;
    ckv::widgets::Window* window = nullptr;
    ckv::widgets::InputLine* field = nullptr;

    MenuScript() : shell(app, options()) {
        window = shell.desktop().add_window(std::make_unique<ckv::widgets::Window>("Draft"));
        window->set_bounds(Rect{30, 6, 28, 6});
        field = window->content_pane().add(std::make_unique<ckv::widgets::InputLine>());
        field->set_bounds(Rect{1, 1, 20, 1});
        app.set_focus(field);
        app.step(0);
    }

    ckv::widgets::ApplicationShellOptions options() {
        const CommandId closing = app.commands().declare({.key = "test.close", .title = "&Close"});
        app.commands().set_enabled_predicate(closing, [] { return false; });
        return ckv::widgets::ApplicationShellOptions{
            .theme = make_classic_theme(app.roles(), roles),
            .menus = {
                {"&File",
                 {MenuItem::submenu("&New", {MenuItem::action("&Sheet", [this] { log.push_back("sheet"); }),
                                             MenuItem::action("&Text", [this] { log.push_back("text"); })}),
                  MenuItem::separator(), MenuItem::command(closing),
                  MenuItem::action("&Wrap", [this] { wrap = !wrap; }).with_mark_provider([this] {
                      return wrap ? MenuMark::Checked : MenuMark::Unchecked;
                  })}},
                {"&Edit", {MenuItem::action("&Undo", [this] { log.push_back("undo"); })}},
            }};
    }

    std::vector<MenuItem> context_items() {
        return {MenuItem::submenu("&More", {MenuItem::action("&Deep", [this] { log.push_back("deep"); })}),
                MenuItem::action("&Copy", [this] { log.push_back("copy"); })};
    }

    void inject(ckv::term::TerminalEvent event) {
        term.inject_event(std::move(event));
        app.step(0);
    }
    void press(Key k, Modifier modifiers = Modifier::None, std::string text = {}) {
        inject(ckv::KeyEvent{KeyChord{k, modifiers, std::move(text)}});
    }
    void pointer(ckv::MouseAction action, ckv::Point at, ckv::MouseButton button = ckv::MouseButton::Left) {
        inject(ckv::MouseEvent{action, button, at, std::nullopt, Modifier::None});
    }

    std::size_t popups() { return shell.desktop().popups().size(); }
    bool bar_active() { return shell.menu_bar()->active(); }

    // Where `label` is drawn on the presented frame, searched row by row.
    std::optional<ckv::Point> find(std::string_view label) const {
        const Surface& frame = app.composed_surface();
        for (int y = 0; y < frame.size().height; ++y)
            for (int x = 0; x + static_cast<int>(label.size()) <= frame.size().width; ++x) {
                bool match = true;
                for (std::size_t i = 0; i < label.size() && match; ++i)
                    match = frame.at(ckv::Point{x + static_cast<int>(i), y}).grapheme() == label.substr(i, 1);
                if (match) return ckv::Point{x, y};
            }
        return std::nullopt;
    }
    std::string row_of(std::string_view label) const {
        const auto at = find(label);
        return at ? row_text(app.composed_surface(), at->y) : std::string{};
    }
};

}  // namespace

CK_TEST(the_whole_menu_system_is_operable_by_keyboard_alone) {
    MenuScript s;
    // The application's own keyboard context-menu command, as the header
    // prescribes: Shift+F10 opens a menu at the focused view.
    const CommandId context = s.app.commands().declare(
        {.key = "test.context", .title = "Context menu", .chord = "Shift+F10", .handler = [&s] {
             show_context_menu_for_focus(s.context_items(), s.app, s.shell.desktop());
         }});
    (void)context;
    const CommandId save = s.app.commands().declare(
        {.key = "test.save", .title = "Save", .chord = "Ctrl+S", .handler = [&s] { s.log.push_back("save"); }});
    (void)save;
    s.inject(ckv::TextEvent{"draft"});

    // F10 enters the bar, Down drops File on its first row, Right opens the
    // nested submenu, and Enter on its second row runs it and ends the walk.
    s.press(Key::F10);
    CK_CHECK(s.bar_active());
    s.press(Key::Down);
    CK_CHECK(s.popups() == 1U);
    CK_CHECK(s.find("Sheet") == std::nullopt);
    s.press(Key::Right);
    CK_CHECK(s.popups() == 2U);
    CK_CHECK(s.find("Sheet").has_value());
    s.press(Key::Escape);  // one level: the submenu alone
    CK_CHECK(s.popups() == 1U);
    CK_CHECK(s.bar_active());
    s.press(Key::Right);
    s.press(Key::Down);
    s.press(Key::Enter);
    CK_CHECK((s.log == std::vector<std::string>{"text"}));
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(!s.bar_active());
    // Activation never took the reader's unsaved field away.
    CK_CHECK(s.app.focused() == s.field);
    CK_CHECK(s.field->text() == "draft");

    // A disabled row is shown and can be stood on, but Enter there does
    // nothing; the checkable row toggles and says so the next time.
    s.press(Key::F10);
    s.press(Key::Down);
    s.press(Key::Down);
    CK_CHECK(s.find("Close").has_value());
    s.press(Key::Enter);
    CK_CHECK(s.popups() == 1U);
    CK_CHECK(s.log.size() == 1U);
    s.press(Key::Down);
    const std::string unchecked = s.row_of("Wrap");
    s.press(Key::Enter);
    CK_CHECK(s.wrap);
    CK_CHECK(s.popups() == 0U);
    s.press(Key::F10);
    s.press(Key::Down);
    CK_CHECK(s.row_of("Wrap") != unchecked);

    // Escape from a top-level dropdown closes it and leaves the walk on the
    // bar; a second Escape leaves the menu system, and the reader is back
    // where they were.
    s.press(Key::Escape);
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(s.bar_active());
    s.press(Key::Escape);
    CK_CHECK(!s.bar_active());
    CK_CHECK(s.app.focused() == s.field);
    // Navigation over the bar wraps; Enter drops the menu reached; a row's
    // mnemonic runs it.
    s.press(Key::F10);
    s.press(Key::Right);
    s.press(Key::Right);
    s.press(Key::Left);
    s.press(Key::Enter);
    CK_CHECK(s.find("Undo").has_value());
    s.press(Key::Char, Modifier::None, "u");
    CK_CHECK((s.log == std::vector<std::string>{"text", "undo"}));
    CK_CHECK(!s.bar_active());

    // Alt and a title's mnemonic open that menu directly; Escape closes it a
    // level at a time and leaves the reader where they were.
    s.press(Key::Char, Modifier::Alt, "e");
    CK_CHECK(s.popups() == 1U);
    CK_CHECK(s.find("Undo").has_value());
    s.press(Key::Escape);
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(s.bar_active());
    s.press(Key::Escape);
    CK_CHECK(!s.bar_active());
    CK_CHECK(s.app.focused() == s.field);

    // An accelerator fires without opening any menu.
    s.press(Key::Char, Modifier::Ctrl, "s");
    CK_CHECK(s.log.back() == "save");
    CK_CHECK(s.popups() == 0U);

    // Shift+F10: a context menu with its own submenu, closed a level at a
    // time, then a row run from it.
    s.press(Key::F10, Modifier::Shift);
    CK_CHECK(s.popups() == 1U);
    s.press(Key::Right);
    CK_CHECK(s.popups() == 2U);
    CK_CHECK(s.find("Deep").has_value());
    s.press(Key::Down);  // the submenu's own row, not its parent's
    CK_CHECK(s.popups() == 2U);
    s.press(Key::Escape);
    CK_CHECK(s.popups() == 1U);
    s.press(Key::Down);
    s.press(Key::Enter);
    CK_CHECK(s.log.back() == "copy");
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(s.app.focused() == s.field);
    CK_CHECK(s.field->text() == "draft");
}

namespace {
// The part of a window an application answers a right-click on. What the
// click MEANS is the host's; the library supplies the menu.
class ContextArea final : public ckv::ui::View {
public:
    explicit ContextArea(MenuScript& script) : script_(script) {}
    bool on_mouse(const ckv::MouseEvent& event) override {
        if (event.action != ckv::MouseAction::Down || event.button != ckv::MouseButton::Right) return false;
        show_context_menu(script_.context_items(), event.cell, script_.app, script_.shell.desktop());
        return true;
    }

private:
    MenuScript& script_;
};
}  // namespace

CK_TEST(the_whole_menu_system_is_operable_by_mouse_alone) {
    MenuScript s;
    auto* area = s.window->content_pane().add(std::make_unique<ContextArea>(s));
    area->set_bounds(Rect{1, 2, 20, 2});
    s.app.step(0);
    const auto file = s.find("File");
    const auto edit = s.find("Edit");
    CK_CHECK(file.has_value() && edit.has_value());
    if (!file || !edit) return;

    // A press on a title drops its menu; the release over the title leaves
    // it open. The pointer moving onto another title switches menus.
    s.pointer(ckv::MouseAction::Down, *file);
    CK_CHECK(s.popups() == 1U);
    s.pointer(ckv::MouseAction::Up, *file);
    CK_CHECK(s.popups() == 1U);
    s.pointer(ckv::MouseAction::Move, *edit, ckv::MouseButton::None);
    CK_CHECK(s.find("Undo").has_value());
    s.pointer(ckv::MouseAction::Move, *file, ckv::MouseButton::None);
    CK_CHECK(s.find("Undo") == std::nullopt);

    // Hovering the row with children opens them; a click in the submenu runs
    // the row and closes the whole chain.
    const auto fresh = s.find("New");
    CK_CHECK(fresh.has_value());
    if (!fresh) return;
    s.pointer(ckv::MouseAction::Move, ckv::Point{fresh->x + 1, fresh->y}, ckv::MouseButton::None);
    CK_CHECK(s.popups() == 2U);
    const auto text = s.find("Text");
    CK_CHECK(text.has_value());
    if (!text) return;
    s.pointer(ckv::MouseAction::Move, *text, ckv::MouseButton::None);
    s.pointer(ckv::MouseAction::Down, *text);
    s.pointer(ckv::MouseAction::Up, *text);
    CK_CHECK((s.log == std::vector<std::string>{"text"}));
    CK_CHECK(s.popups() == 0U);

    // Press on the title, drag down onto the checkable row, release there.
    s.pointer(ckv::MouseAction::Down, *file);
    const auto wrap = s.find("Wrap");
    CK_CHECK(wrap.has_value());
    if (!wrap) return;
    s.pointer(ckv::MouseAction::Move, *wrap);
    s.pointer(ckv::MouseAction::Up, *wrap);
    CK_CHECK(s.wrap);
    CK_CHECK(s.popups() == 0U);

    // A disabled row takes the click and does nothing; a press outside every
    // menu of the chain closes them all.
    s.pointer(ckv::MouseAction::Down, *file);
    s.pointer(ckv::MouseAction::Up, *file);
    const auto closing = s.find("Close");
    CK_CHECK(closing.has_value());
    if (!closing) return;
    s.pointer(ckv::MouseAction::Down, *closing);
    s.pointer(ckv::MouseAction::Up, *closing);
    CK_CHECK(s.log.size() == 1U);
    s.pointer(ckv::MouseAction::Down, ckv::Point{5, 14});
    s.pointer(ckv::MouseAction::Up, ckv::Point{5, 14});
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(s.log.size() == 1U);

    // A right-click opens the host's context menu at the pointer; a click on
    // one of its rows runs it.
    const ckv::Rect at = area->absolute_bounds();
    s.pointer(ckv::MouseAction::Down, ckv::Point{at.x + 2, at.y}, ckv::MouseButton::Right);
    s.pointer(ckv::MouseAction::Up, ckv::Point{at.x + 2, at.y}, ckv::MouseButton::Right);
    CK_CHECK(s.popups() == 1U);
    const auto copy = s.find("Copy");
    CK_CHECK(copy.has_value());
    if (!copy) return;
    s.pointer(ckv::MouseAction::Move, *copy, ckv::MouseButton::None);
    s.pointer(ckv::MouseAction::Down, *copy);
    s.pointer(ckv::MouseAction::Up, *copy);
    CK_CHECK(s.log.back() == "copy");
    CK_CHECK(s.popups() == 0U);
}

namespace {
// The focused view of a window that owns a context menu and answers the
// keyboard's request for it -- the Menu key or Shift+F10 -- at the focus, as
// a right click answers at the pointer.
class ContextPane final : public ckv::ui::View {
public:
    explicit ContextPane(std::function<void()> open_menu)
        : View(Rect{}, ckv::ui::FocusPolicy::TabStop), open_menu_(std::move(open_menu)) {}
    bool on_key(const ckv::KeyEvent& event) override {
        if (!is_keyboard_context_menu_request(event)) return false;
        open_menu_();
        return true;
    }

private:
    std::function<void()> open_menu_;
};

// An application at a width chosen by the test: File, Navigate, Window and
// Help on the bar, a status line that says why a highlighted row is grey --
// the application's own wiring of MenuBar::on_highlight_changed, as D-083
// has it -- and a window whose pane owns a context menu.
struct ShellScript {
    ckv::term::HeadlessTerminal term;
    ManualClock clock;
    Application app{term, clock};
    StandardRoles roles = intern_standard_roles(app.roles());
    std::vector<std::string> log;
    ckv::widgets::ApplicationShell shell;
    ContextPane* pane = nullptr;

    explicit ShellScript(ckv::Size size) : term(size), shell(app, options()) {
        shell.menu_bar()->on_highlight_changed = [this](const ckv::widgets::MenuHighlight& highlight) {
            shell.status_line()->set_transient_hint(!highlight.none && !highlight.enabled ? highlight.disabled_reason
                                                                                          : std::string{});
        };
        auto* window = shell.desktop().add_window(std::make_unique<ckv::widgets::Window>("Draft"));
        window->set_bounds(Rect{1, 3, 14, 4});
        auto owned = std::make_unique<ContextPane>([this] {
            show_context_menu_for_focus({MenuItem::action("&Copy", [this] { log.push_back("copy"); })}, app,
                                        shell.desktop());
        });
        pane = owned.get();
        window->set_content(std::move(owned));
        app.set_focus(pane);
        app.step(0);
    }

    ckv::widgets::ApplicationShellOptions options() {
        const auto row = [this](const char* label, const char* what) {
            return MenuItem::action(label, [this, what] { log.push_back(what); });
        };
        return ckv::widgets::ApplicationShellOptions{
            .theme = make_classic_theme(app.roles(), roles),
            .menus = {{"&File",
                       {row("&New", "new"),
                        row("&Print", "print").with_enabled(false).with_disabled_reason("No printer"),
                        row("&Quit", "quit")}},
                      {"&Navigate", {row("&Back", "back"), row("&Forward", "forward")}},
                      {"&Window", {row("&Tile", "tile"), row("&Cascade", "cascade")}},
                      {"&Help", {row("&About", "about")}}},
            .always_dock_status_line = true};
    }

    void settle() { app.step(0); }
    void press(Key k, Modifier modifiers = Modifier::None, std::string text = {}) {
        term.inject_event(ckv::KeyEvent{KeyChord{k, modifiers, std::move(text)}});
        settle();
    }
    void bytes(std::string_view raw) {
        term.inject_bytes(raw, 0);
        settle();
    }
    void pointer(ckv::MouseAction action, ckv::Point at, ckv::MouseButton button = ckv::MouseButton::Left) {
        term.inject_event(ckv::MouseEvent{action, button, at, std::nullopt, Modifier::None});
        settle();
    }
    void click(ckv::Point at) {
        pointer(ckv::MouseAction::Down, at);
        pointer(ckv::MouseAction::Up, at);
    }

    std::size_t popups() { return shell.desktop().popups().size(); }
    bool bar_active() { return shell.menu_bar()->active(); }
    std::string row(int y) const { return row_text(app.composed_surface(), y); }
    std::optional<ckv::Point> find(std::string_view label) const {
        const Surface& frame = app.composed_surface();
        for (int y = 0; y < frame.size().height; ++y) {
            const std::string text = row_text(frame, y);
            if (const std::size_t at = text.find(label); at != std::string::npos)
                return ckv::Point{ckv::text::text_width(std::string_view(text).substr(0, at)), y};
        }
        return std::nullopt;
    }
};
}  // namespace

CK_TEST(a_narrow_bar_is_operable_through_its_overflow_title_by_keyboard_alone) {
    ShellScript s(ckv::Size{22, 12});
    CK_CHECK(s.row(0) == "  File  Navigate  »   ");

    // F10 and the arrows walk onto the overflow title; Down lists the menus
    // that did not fit; Right enters one; Enter runs its row and ends the walk.
    s.press(Key::F10);
    s.press(Key::Right);
    s.press(Key::Right);
    s.press(Key::Down);
    CK_CHECK(s.popups() == 1U);
    CK_CHECK(s.find("Window").has_value());
    CK_CHECK(s.find("Help").has_value());
    s.press(Key::Right);
    CK_CHECK(s.popups() == 2U);
    s.press(Key::Enter);
    CK_CHECK((s.log == std::vector<std::string>{"tile"}));
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(!s.bar_active());
    CK_CHECK(s.app.focused() == s.pane);

    // A hidden menu's mnemonic reaches it; Esc closes one level at a time.
    s.press(Key::Char, Modifier::Alt, "h");
    CK_CHECK(s.popups() == 2U);
    CK_CHECK(s.find("About").has_value());
    s.press(Key::Escape);
    CK_CHECK(s.popups() == 1U);
    s.press(Key::Escape);
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(s.bar_active());
    s.press(Key::Char, Modifier::None, "h");  // the bare letter while walking
    CK_CHECK(s.popups() == 2U);
    s.press(Key::Enter);
    CK_CHECK(s.log.back() == "about");
    CK_CHECK(s.app.focused() == s.pane);
}

CK_TEST(a_narrow_bar_is_operable_through_its_overflow_title_by_mouse_alone) {
    ShellScript s(ckv::Size{22, 12});
    const auto overflow = s.find("»");
    CK_CHECK(overflow.has_value());
    if (!overflow) return;
    CK_CHECK(*overflow == (ckv::Point{18, 0}));
    s.click(*overflow);
    CK_CHECK(s.popups() == 1U);
    const auto window = s.find("Window");
    CK_CHECK(window.has_value());
    if (!window) return;
    // Hovering the hidden menu's row opens it; a click in it runs the row.
    s.pointer(ckv::MouseAction::Move, *window, ckv::MouseButton::None);
    CK_CHECK(s.popups() == 2U);
    const auto cascade = s.find("Cascade");
    CK_CHECK(cascade.has_value());
    if (!cascade) return;
    s.pointer(ckv::MouseAction::Move, *cascade, ckv::MouseButton::None);
    s.click(*cascade);
    CK_CHECK((s.log == std::vector<std::string>{"cascade"}));
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(!s.bar_active());
}

CK_TEST(escape_closes_one_menu_level_at_a_time_through_dispatched_keys) {
    ShellScript s(ckv::Size{60, 12});
    s.press(Key::F10);
    s.press(Key::Down);
    CK_CHECK(s.popups() == 1U);
    // One Esc: the dropdown closes, the walk stays on File, and the next
    // Down drops File again rather than anything the reader left.
    s.press(Key::Escape);
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(s.bar_active());
    CK_CHECK(s.app.input_capture() == nullptr);
    s.press(Key::Right);  // the walk moves without dragging a menu with it
    CK_CHECK(s.popups() == 0U);
    s.press(Key::Left);
    s.press(Key::Down);
    CK_CHECK(s.find("New").has_value());
    // Two more: the dropdown, then the bar, and the focus is back.
    s.press(Key::Escape);
    s.press(Key::Escape);
    CK_CHECK(!s.bar_active());
    CK_CHECK(s.app.focused() == s.pane);
}

CK_TEST(arrows_stop_on_a_disabled_row_and_the_status_line_says_why) {
    // D-083: a reader who cannot land on a grey verb cannot be told why it is
    // grey. The arrows stand on it, Enter there does nothing, and the status
    // line -- wired to the bar's highlight -- gives the reason.
    ShellScript s(ckv::Size{60, 12});
    ckv::widgets::StatusLine& status = *s.shell.status_line();
    s.press(Key::F10);
    s.press(Key::Down);
    CK_CHECK(status.current_hint().empty());  // New is available
    s.press(Key::Down);
    CK_CHECK(status.current_hint() == "No printer");
    CK_CHECK(s.row(11).find("No printer") != std::string::npos);
    s.press(Key::Enter);
    CK_CHECK(s.log.empty());
    CK_CHECK(s.popups() == 1U);
    s.press(Key::Down);
    CK_CHECK(status.current_hint().empty());
    s.press(Key::Up);  // back up onto it from below
    CK_CHECK(status.current_hint() == "No printer");
    s.press(Key::Escape);  // the reason leaves with the menu
    CK_CHECK(status.current_hint().empty());
}

CK_TEST(the_menu_key_opens_the_focused_views_context_menu_as_shift_f10_does) {
    ShellScript s(ckv::Size{60, 12});
    const Rect focus = s.pane->absolute_bounds();

    // Shift+F10, from its legacy bytes.
    s.bytes("\x1b[21;2~");
    CK_CHECK(s.popups() == 1U);
    const Rect shift_f10 = s.shell.desktop().popups()[0]->absolute_bounds();
    CK_CHECK(shift_f10.x == focus.x);
    CK_CHECK(shift_f10.y == focus.y);
    s.press(Key::Escape);
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(s.app.focused() == s.pane);

    // The Menu key as the xterm lineage and rxvt-unicode send it: the same
    // menu, at the same place.
    s.bytes("\x1b[29~");
    CK_CHECK(s.popups() == 1U);
    CK_CHECK(s.shell.desktop().popups()[0]->absolute_bounds() == shift_f10);
    s.press(Key::Enter);
    CK_CHECK((s.log == std::vector<std::string>{"copy"}));
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(s.app.focused() == s.pane);

    // And as kitty sends it, press and release: one menu, not two.
    ckv::term::Capabilities kitty = s.term.capabilities();
    kitty.keyboard_protocol = ckv::term::KeyboardProtocol::Kitty;
    kitty.kitty_keyboard_flags = ckv::term::kKittyRequestedFlags;
    s.term.set_capabilities(kitty);
    s.bytes("\x1b[57363u\x1b[57363;1:3u");
    CK_CHECK(s.popups() == 1U);
    CK_CHECK(s.shell.desktop().popups()[0]->absolute_bounds() == shift_f10);
    s.press(Key::Escape);
    CK_CHECK(s.popups() == 0U);
    CK_CHECK(s.app.focused() == s.pane);
}

CK_TEST(the_keyboard_context_menu_request_is_the_menu_key_or_shift_f10) {
    CK_CHECK(is_keyboard_context_menu_request(key(Key::Menu)));
    CK_CHECK(!is_keyboard_context_menu_request(key(Key::Menu, Modifier::Ctrl)));
    CK_CHECK(!is_keyboard_context_menu_request(
        ckv::KeyEvent{KeyChord{Key::Menu, Modifier::None, ""}, ckv::KeyAction::Release}));
    CK_CHECK(!is_keyboard_context_menu_request(
        ckv::KeyEvent{KeyChord{Key::Menu, Modifier::None, ""}, ckv::KeyAction::Repeat}));
}

CK_TEST(a_terminal_resize_moves_an_open_hidden_menu_under_its_own_title) {
    ShellScript s(ckv::Size{22, 12});
    s.press(Key::Char, Modifier::Alt, "w");
    CK_CHECK(s.popups() == 2U);  // the overflow list, and Window inside it

    // The terminal grows: Window has a title of its own again, and the
    // reader is still in Window, now hanging from it.
    s.term.resize(ckv::Size{60, 12});
    s.settle();
    CK_CHECK(s.row(0).find("Window") != std::string::npos);
    CK_CHECK(s.row(0).find("»") == std::string::npos);
    CK_CHECK(s.popups() == 1U);
    const auto tile = s.find("Tile");
    CK_CHECK(tile.has_value());
    if (tile) CK_CHECK(tile->x == 19);  // one cell right of Window's first letter at 18
    s.press(Key::Down);
    s.press(Key::Enter);
    CK_CHECK((s.log == std::vector<std::string>{"cascade"}));
    CK_CHECK(s.app.focused() == s.pane);
}
