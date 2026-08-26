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
    // capture (exercised end-to-end in the MenuBar tests below).
    CK_CHECK(!menu->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                              ckv::Point{50, 50}, std::nullopt, Modifier::None}));
    CK_CHECK(dismissed);
    CK_CHECK(reason == ckv::widgets::MenuDismissReason::Cancelled);
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

// --- MenuBar: activation, navigation, dismissal ---------------------------

namespace {
struct MenuBarFixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    Fixture f;
    Desktop desktop{Rect{0, 0, 80, 24}};

    MenuBarFixture() { desktop.set_context(ui::Context{&f.theme, &f.registry, &app}); }
};
}  // namespace

CK_TEST(activate_gives_the_bar_focus_and_deactivate_restores_the_previous_focus) {
    MenuBarFixture mf;
    auto* other = mf.app.root().add_child(std::make_unique<ckv::ui::View>());
    other->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    mf.app.set_focus(other);

    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));

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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));

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
        {.key = "test.contextual-save", .title = "Save", .context = "document"});
    bool ran = false;
    mf.app.set_command_handler(save, [&] { ran = true; });
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::command(save)}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));

    bar->activate();
    CK_CHECK(mf.app.dispatch(key(Key::Down)));
    CK_CHECK(mf.app.dispatch(key(Key::Enter)));
    CK_CHECK(ran);
    CK_CHECK(mf.app.focused() == editor);
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

    mf.app.dispatch(key(Key::Escape));
    CK_CHECK(!bar->active());
    CK_CHECK(mf.app.focused() == other);  // NOT the bar — the true prior focus survived
}

CK_TEST(enter_opens_a_dropdown_as_a_desktop_popup) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));

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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    bar->activate();
    mf.app.dispatch(key(Key::Enter));  // opens the dropdown
    CK_CHECK(mf.desktop.popups().size() == 1);

    // The dropdown itself owns keyboard focus via input capture, not
    // Application's normal focus chain — Escape must route through
    // the captured popup, which the MenuBar wired to close itself.
    ui::View* popup_view = mf.desktop.popups()[0];
    static_cast<DropdownMenu*>(popup_view)->on_key(key(Key::Escape));
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(bar->active());  // the bar itself is still active — only the dropdown closed
}

CK_TEST(menu_bar_routes_item_mnemonics_to_its_open_dropdown) {
    MenuBarFixture mf;
    bool opened = false;
    const CommandId open_command = mf.app.commands().declare(
        {.key = "test.open", .title = "&Open", .handler = [&] { opened = true; }});

    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{ {"&File", {MenuItem::command(open_command)}} });
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    bar->activate();
    CK_CHECK(bar->on_key(key(Key::Enter)));
    CK_CHECK(mf.desktop.popups().size() == 1U);

    CK_CHECK(bar->on_key(key(Key::Char, "o")));
    CK_CHECK(opened);
    CK_CHECK(mf.desktop.popups().empty());
}

CK_TEST(destroying_the_bar_while_a_dropdown_is_open_cleans_up_the_popup_and_input_capture) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    bar->activate();
    CK_CHECK(bar->on_key(key(Key::Right)));
    CK_CHECK(bar->on_key(key(Key::Right)));  // wraps back to menu 0
    // No direct accessor for highlighted_ — verified indirectly via
    // which menu Enter opens next.
    bar->on_key(key(Key::Enter));
    CK_CHECK(mf.desktop.popups().size() == 1);
}

CK_TEST(mnemonic_letter_opens_the_matching_top_level_menu) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}},
                                  {"&Edit", {MenuItem::action("&Copy", {})}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    bar->activate();
    CK_CHECK(bar->on_key(key(Key::Char, "e")));
    CK_CHECK(mf.desktop.popups().size() == 1);
}

CK_TEST(on_key_is_unhandled_when_the_bar_is_not_active) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    CK_CHECK(!bar->on_key(key(Key::Right)));
}

CK_TEST(a_narrow_menu_bar_clips_visible_items_to_its_own_width) {
    MenuBarFixture mf;
    auto bar_owned =
        std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}, {"&Navigate", {}}, {"&Window", {}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    bar->set_bounds(Rect{0, 0, 6, 1});

    Surface s(ckv::Size{6, 1}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 6, 1});
    bar->draw(painter);

    CK_CHECK(row_text(s, 0).size() == 6);
    CK_CHECK(row_text(s, 0).find('.') == std::string::npos);
}

CK_TEST(menu_mnemonics_use_the_shared_hotkey_accent_on_each_surface) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    CK_CHECK(menu->on_key(key(Key::Escape)));
    CK_CHECK(mf.desktop.popups().empty());
}

// --- MenuBar: F10 default activation (M9/WP-13, D-029) ---------------------

CK_TEST(attaching_a_menu_bar_installs_itself_as_f10s_default_handler) {
    MenuBarFixture mf;
    CK_CHECK(!mf.app.commands().has_handler(standard(mf.app).menu));
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    CK_CHECK(mf.app.commands().has_handler(standard(mf.app).menu));
    CK_CHECK(!bar->active());

    mf.app.commands().execute(standard(mf.app).menu);
    CK_CHECK(bar->active());
}

CK_TEST(f10_restarts_top_level_mnemonic_navigation_after_a_previous_menu) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {MenuItem::action("Open", {})}},
         {"&Appearance", {MenuItem::action("Colors", {})}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    mf.app.commands().execute(standard(mf.app).menu);
    CK_CHECK(bar->on_key(key(Key::Right)));

    mf.app.commands().execute(standard(mf.app).menu);
    CK_CHECK(bar->on_key(key(Key::Char, "a")));
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));

    mf.app.commands().execute(standard(mf.app).menu);
    CK_CHECK(custom_ran);
    // the bar's own activate() never ran — its handler was never installed
    CK_CHECK(!bar->active());
}

CK_TEST(destroying_the_bar_clears_the_default_handler_it_installed) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    *bar_holder = bar;
    bar->activate();
    bar->on_key(key(Key::Enter));  // opens File's dropdown
    CK_CHECK(mf.desktop.popups().size() == 1);

    auto* dropdown = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);
    dropdown->on_key(key(Key::Enter));  // activates the item, which reopens Edit's (empty) dropdown
    CK_CHECK(true);  // reaching this line without a crash/ASan report IS the assertion
}

// --- Regression: MenuBar::open_dropdown_ desync (review finding #3) --------

CK_TEST(a_popup_removed_by_something_other_than_the_bar_still_lets_the_bar_recover_correctly) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {MenuItem::action("&Open", {})}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    CK_CHECK(bar->on_key(key(Key::Right)));
    CK_CHECK(mf.desktop.popups().empty());  // still no (stale) dropdown reopened
}

CK_TEST(right_arrow_opens_a_nested_submenu_and_escape_closes_one_level) {
    MenuBarFixture mf;
    const MenuItem more =
        MenuItem::submenu("&More", {MenuItem::action("&Child", [] {})});
    DropdownMenu* root = show_context_menu({more}, ckv::Point{2, 2}, mf.app, mf.desktop);

    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(mf.app.input_capture() == root);
    CK_CHECK(root->on_key(key(Key::Right)));
    CK_CHECK(mf.desktop.popups().size() == 2);

    auto* child = static_cast<DropdownMenu*>(mf.desktop.popups()[1]);
    CK_CHECK(mf.app.input_capture() == child);
    CK_CHECK(child->on_key(key(Key::Escape)));
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
    DropdownMenu* root = show_context_menu({more}, ckv::Point{2, 2}, mf.app, mf.desktop);
    CK_CHECK(root->on_key(key(Key::Right)));
    auto* child = static_cast<DropdownMenu*>(mf.desktop.popups()[1]);

    CK_CHECK(child->on_key(key(Key::Enter)));
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
    return static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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

    mf.app.dispatch(key(Key::Escape));  // the second one leaves the menu system
    CK_CHECK(!bar->active());
    CK_CHECK(mf.desktop.popups().empty());
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    popup->on_key(key(Key::Escape));
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
        {.key = "test.context-menu-save", .title = "Save", .context = "document"});
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
    popup->on_key(key(Key::Enter));
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    bar->set_bounds(Rect{0, 0, 30, 1});
    CK_CHECK(mf.app.commands().command_for_key(KeyChord{Key::Char, Modifier::Alt, "h"}).has_value());

    bar->set_menus(std::vector<MenuBarItem>{{"&File", {}}});
    // The old chord must not survive to open a menu that no longer exists.
    CK_CHECK(!mf.app.commands().command_for_key(KeyChord{Key::Char, Modifier::Alt, "h"}).has_value());
    CK_CHECK(mf.app.commands().command_for_key(KeyChord{Key::Char, Modifier::Alt, "f"}).has_value());
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    bar->set_bounds(Rect{0, 0, 30, 1});

    // No pointer is involved, so there is no other indicator of position.
    bar->activate();
    bar->on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
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
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
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
    CK_CHECK(root->on_key(key(Key::Right)));
    auto* child = static_cast<DropdownMenu*>(mf.desktop.popups()[1]);
    child->on_key(key(Key::End));
    CK_CHECK(child->highlighted() == 2);   // the submenu moved
    CK_CHECK(root->highlighted() == 0);    // and the parent did not
}

CK_TEST(the_menu_bar_ends_are_its_first_and_last_menu) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {MenuItem::action("a", {})}},
        {"&Edit", {MenuItem::action("b", {})}},
        {"&Help", {MenuItem::action("c", {})}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    bar->activate();
    CK_CHECK(bar->on_key(key(Key::End)));
    bar->on_key(key(Key::Enter));
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
