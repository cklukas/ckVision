// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/menu.hpp"

#include "cktest.hpp"
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
using ckv::widgets::MenuItemCheck;
using ckv::widgets::MenuItem;
using ckv::widgets::is_keyboard_context_menu_request;
using ckv::widgets::show_context_menu;
using ckv::widgets::show_context_menu_for_focus;
namespace ui = ckv::ui;

namespace {
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
    auto menu = make_dropdown(f, app, {{"One", ui::kInvalidCommand, nullptr, false, {}, {}},
                                        {"Two", ui::kInvalidCommand, nullptr, false, {}, {}}});
    CK_CHECK(menu->highlighted() == 0);
}

CK_TEST(dropdown_with_the_first_item_disabled_skips_it_at_construction) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    constexpr CommandId kDisabled = ui::kFirstApplicationCommandId;
    app.commands().register_command(kDisabled, "Disabled");
    app.commands().set_enabled_predicate(kDisabled, [] { return false; });
    auto menu = make_dropdown(f, app, {{"One", kDisabled, nullptr, false, {}, {}},
                                        {"Two", ui::kInvalidCommand, nullptr, false, {}, {}}});
    CK_CHECK(menu->highlighted() == 1);
}

CK_TEST(down_arrow_moves_to_the_next_item_and_wraps) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, {{"One", ui::kInvalidCommand, nullptr, false, {}, {}},
                                        {"Two", ui::kInvalidCommand, nullptr, false, {}, {}}});
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
    auto menu = make_dropdown(f, app, {{"One", ui::kInvalidCommand, nullptr, false, {}, {}},
                                        {"Two", ui::kInvalidCommand, nullptr, false, {}, {}}});
    menu->on_key(key(Key::Up));
    CK_CHECK(menu->highlighted() == 1);
}

CK_TEST(navigation_skips_disabled_items_and_separators) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    constexpr CommandId kDisabled = ui::kFirstApplicationCommandId;
    app.commands().register_command(kDisabled, "Disabled");
    app.commands().set_enabled_predicate(kDisabled, [] { return false; });
    auto menu = make_dropdown(f, app,
                               {{"One", ui::kInvalidCommand, nullptr, false, {}, {}},
                                {"", ui::kInvalidCommand, nullptr, true, {}, {}},  // separator
                                {"Disabled", kDisabled, nullptr, false, {}, {}},
                                {"Four", ui::kInvalidCommand, nullptr, false, {}, {}}});
    CK_CHECK(menu->highlighted() == 0);
    menu->on_key(key(Key::Down));
    CK_CHECK(menu->highlighted() == 3);  // skips both the separator and the disabled item
}

CK_TEST(a_dropdown_with_every_item_disabled_or_a_separator_has_no_selection) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, {{"", ui::kInvalidCommand, nullptr, true, {}, {}}});
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
    constexpr CommandId kSave = ui::kFirstApplicationCommandId;
    app.commands().register_command(kSave, "Save");
    bool ran = false;
    app.set_command_handler(kSave, [&] { ran = true; });
    bool dismissed = false;
    auto reason = ckv::widgets::MenuDismissReason::Cancelled;
    auto menu = make_dropdown(f, app, {{"&Save", kSave, nullptr, false, {}, {}}});
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
    auto menu = make_dropdown(f, app, {{"&Item", ui::kInvalidCommand, [&] { ran = true; }, false, {}, {}}});
    menu->on_key(key(Key::Enter));
    CK_CHECK(ran);
}

CK_TEST(mnemonic_key_activates_the_matching_item_directly) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    auto menu = make_dropdown(f, app, {{"&One", ui::kInvalidCommand, nullptr, false, {}, {}},
                                        {"&Two", ui::kInvalidCommand, [&] { ran = true; }, false, {}, {}}});
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
    auto menu = make_dropdown(f, app, {{"&Item", ui::kInvalidCommand, [&] { ran = true; }, false, {}, {}}});
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
    constexpr CommandId kDisabled = ui::kFirstApplicationCommandId;
    app.commands().register_command(kDisabled, "Disabled");
    app.commands().set_enabled_predicate(kDisabled, [] { return false; });
    bool ran = false;
    app.set_command_handler(kDisabled, [&] { ran = true; });
    auto menu = make_dropdown(f, app, {{"Disabled", kDisabled, nullptr, false, {}, {}}});
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
    auto menu = make_dropdown(f, app, {{"First", ui::kInvalidCommand, [&] { first = true; }, false, {}, {}},
                                        {"Second", ui::kInvalidCommand, [&] { second = true; }, false, {}, {}}});
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
    auto menu = make_dropdown(f, app, {{"Run", ui::kInvalidCommand, [&] { ran = true; }, false, {}, {}},
                                        {"", ui::kInvalidCommand, {}, true, {}, {}},
                                        {"Other", ui::kInvalidCommand, {}, false, {}, {}}});
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
    auto menu = make_dropdown(f, app, {{"&Item", ui::kInvalidCommand, [&] { ran = true; }, false, {}, {}}});
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
    constexpr CommandId kSave = ui::kFirstApplicationCommandId;
    app.commands().register_command(kSave, "&Save");
    // The label below ("Ignored") must never appear anywhere — an item
    // referencing a command carries no label text of its own.
    auto menu = make_dropdown(f, app, {{"Ignored", kSave, nullptr, false, {}, {}}});
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
    constexpr CommandId kSave = ui::kFirstApplicationCommandId;
    app.commands().register_command(kSave, "&Save");  // no default_chord
    auto menu = make_dropdown(f, app, {{"", kSave, nullptr, false, {}, {}}});
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
    constexpr CommandId kSave = ui::kFirstApplicationCommandId;
    app.commands().register_command(kSave, "&Save", "File",
                                      KeyChord{Key::Char, Modifier::Ctrl, "s"});
    auto menu = make_dropdown(f, app, {{"", kSave, nullptr, false, {}, {}}});
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
    constexpr CommandId kSave = ui::kFirstApplicationCommandId;
    app.commands().register_command(kSave, "&Save", "File",
                                      KeyChord{Key::Char, Modifier::Ctrl, "s"});
    auto menu = make_dropdown(f, app, {{"", kSave, nullptr, false, {}, {}}});
    menu->set_bounds(Rect{0, 0, 20, 1});

    app.commands().unbind_key(KeyChord{Key::Char, Modifier::Ctrl, "s"});
    app.commands().bind_key(KeyChord{Key::F5, Modifier::None, ""}, kSave);

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
    constexpr CommandId kSave = ui::kFirstApplicationCommandId;
    app.commands().register_command(kSave, "&Very Long Save Command", "File",
                                      KeyChord{Key::Char, Modifier::Ctrl, "s"});
    auto menu = make_dropdown(f, app, {{"", kSave, nullptr, false, {}, {}}});
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
    constexpr CommandId kSave = ui::kFirstApplicationCommandId;
    bool ran = false;
    app.commands().register_command(kSave, "&Save");
    app.set_command_handler(kSave, [&] { ran = true; });
    auto menu = make_dropdown(f, app, {{"&Ignored", kSave, nullptr, false, {}, {}}});

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
    MenuItem checked{"&Checked", ui::kInvalidCommand, [] {}, false, {}, {}};
    checked.check = MenuItemCheck::Checked;
    MenuItem unchecked{"&Unchecked", ui::kInvalidCommand, [] {}, false, {}, {}};
    unchecked.check = MenuItemCheck::Unchecked;
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
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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

    constexpr ui::CommandId kAbout = ui::kFirstApplicationCommandId;
    mf.app.commands().register_command(kAbout, "About");
    const ckv::ui::View* focused_when_command_ran = nullptr;
    bool ran = false;
    mf.app.set_command_handler(kAbout, [&] {
        ran = true;
        focused_when_command_ran = mf.app.focused();
    });

    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&Help", {{"&About", kAbout, nullptr, false, {}, {}}}}});
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
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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
        {"&File", {{"Open", ui::kInvalidCommand, {}, false, {}, {}}}},
        {"&Window", {{"Tile", ui::kInvalidCommand, [&] { ran = true; }, false, {}, {}}}},
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
        {"&File", {{"&Open", ui::kInvalidCommand, [&] { file_ran = true; }, false, {}, {}}}},
        {"&Window", {{"&Tile", ui::kInvalidCommand, [&] { window_ran = true; }, false, {}, {}}}},
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

CK_TEST(escape_closes_an_open_dropdown_first_without_deactivating_the_bar) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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
    constexpr CommandId kOpen = ui::kFirstApplicationCommandId;
    bool opened = false;
    mf.app.commands().register_command(kOpen, "&Open");
    mf.app.commands().set_handler(kOpen, [&] { opened = true; });

    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{ {"&File", {{"", kOpen, nullptr, false, {}, {}}}} });
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
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}}},
                                  {"&Edit", {{"&Copy", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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

    auto menu = make_dropdown(mf.f, mf.app, {{"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}});
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
    auto menu = make_dropdown(f, app, { {"&Open", ui::kInvalidCommand, nullptr, false, {}, {}},
                                       {"", ui::kInvalidCommand, nullptr, true, {}, {}},
                                       {"&Quit", ui::kInvalidCommand, nullptr, false, {}, {}} });
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

CK_TEST(a_context_menu_keeps_its_frame_inside_the_desktop_at_the_bottom_right_edge) {
    MenuBarFixture mf;
    DropdownMenu* menu = show_context_menu(
        {{"&Open", ui::kInvalidCommand, nullptr, false, {}, {}},
         {"&Quit", ui::kInvalidCommand, nullptr, false, {}, {}}},
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
    CK_CHECK(!mf.app.commands().has_handler(ui::std_commands::kMenu));
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    CK_CHECK(mf.app.commands().has_handler(ui::std_commands::kMenu));
    CK_CHECK(!bar->active());

    mf.app.commands().execute(ui::std_commands::kMenu);
    CK_CHECK(bar->active());
}

CK_TEST(f10_restarts_top_level_mnemonic_navigation_after_a_previous_menu) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {{"Open", 1}}},
                                                                          {"&Appearance", {{"Colors", 2}}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    mf.app.commands().execute(ui::std_commands::kMenu);
    CK_CHECK(bar->on_key(key(Key::Right)));

    mf.app.commands().execute(ui::std_commands::kMenu);
    CK_CHECK(bar->on_key(key(Key::Char, "a")));
    CK_CHECK(mf.desktop.popups().size() == 1);
    if (mf.desktop.popups().size() != 1) return;
    const auto* popup = dynamic_cast<const DropdownMenu*>(mf.desktop.popups().front());
    CK_CHECK(popup != nullptr);
    if (popup != nullptr && !popup->items().empty())
        CK_CHECK(popup->items().front().label == "Colors");
}

CK_TEST(a_pre_existing_kmenu_handler_is_not_overridden_by_attaching_a_menu_bar) {
    MenuBarFixture mf;
    bool custom_ran = false;
    mf.app.commands().set_handler(ui::std_commands::kMenu, [&] { custom_ran = true; });

    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));

    mf.app.commands().execute(ui::std_commands::kMenu);
    CK_CHECK(custom_ran);
    // the bar's own activate() never ran — its handler was never installed
    CK_CHECK(!bar->active());
}

CK_TEST(destroying_the_bar_clears_the_default_handler_it_installed) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    CK_CHECK(mf.app.commands().has_handler(ui::std_commands::kMenu));

    mf.desktop.remove_child(bar).reset();  // destroys the MenuBar

    CK_CHECK(!mf.app.commands().has_handler(ui::std_commands::kMenu));
    // A stale handler calling into freed memory would crash (or, under
    // ASan, report use-after-free) here rather than just returning false.
    CK_CHECK(!mf.app.commands().execute(ui::std_commands::kMenu));
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
    CK_CHECK(mf.app.commands().has_handler(ui::std_commands::kMenu));

    mf.app.commands().execute(ui::std_commands::kMenu);
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
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{
        {"&File", {{"&Reopen", ui::kInvalidCommand, nullptr, false, {}, {}}}}, {"&Edit", {}}});
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    bar->activate();
    bar->on_key(key(Key::Enter));  // opens File's dropdown
    CK_CHECK(mf.desktop.popups().size() == 1);

    auto* dropdown = static_cast<DropdownMenu*>(mf.desktop.popups()[0]);
    // Reassign on_activate on the live item to close-and-reopen — the
    // hazard the original bug required.
    const_cast<MenuItem&>(dropdown->items()[0]).on_activate = [bar] { bar->on_key(key(Key::Right)); };
    dropdown->on_key(key(Key::Enter));  // activates the item, which reopens Edit's (empty) dropdown
    CK_CHECK(true);  // reaching this line without a crash/ASan report IS the assertion
}

// --- Regression: MenuBar::open_dropdown_ desync (review finding #3) --------

CK_TEST(a_popup_removed_by_something_other_than_the_bar_still_lets_the_bar_recover_correctly) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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
    MenuItem more{"&More", ui::kInvalidCommand, nullptr, false, {}, {}};
    more.submenu = {MenuItem{"&Child", ui::kInvalidCommand, [] {}, false, {}, {}}};
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
    MenuItem more{"&More", ui::kInvalidCommand, nullptr, false, {}, {}};
    more.submenu = {MenuItem{"&Child", ui::kInvalidCommand, [&] {
                                  CK_CHECK(mf.desktop.popups().empty());
                                  CK_CHECK(mf.app.input_capture() == nullptr);
                                  ran = true;
                              }, false, {}, {}}};
    DropdownMenu* root = show_context_menu({more}, ckv::Point{2, 2}, mf.app, mf.desktop);
    CK_CHECK(root->on_key(key(Key::Right)));
    auto* child = static_cast<DropdownMenu*>(mf.desktop.popups()[1]);

    CK_CHECK(child->on_key(key(Key::Enter)));
    CK_CHECK(ran);
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
        {{"&Copy", ui::kInvalidCommand, nullptr, false, {}, {}}, {"&Paste", ui::kInvalidCommand, nullptr, false, {}, {}}},
        ckv::Point{10, 10}, mf.app, mf.desktop);
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(mf.app.input_capture() == popup);
    CK_CHECK(popup->bounds().x == 10);
    CK_CHECK(popup->bounds().y == 10);
}

CK_TEST(show_context_menu_self_removes_and_clears_capture_on_escape) {
    MenuBarFixture mf;
    auto* popup = ckv::widgets::show_context_menu({{"&Copy", ui::kInvalidCommand, nullptr, false, {}, {}}},
                                                    ckv::Point{10, 10}, mf.app, mf.desktop);
    popup->on_key(key(Key::Escape));
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(mf.app.input_capture() == nullptr);
}

CK_TEST(show_context_menu_activating_an_item_closes_it_and_runs_the_action) {
    MenuBarFixture mf;
    bool ran = false;
    auto* popup = ckv::widgets::show_context_menu({{"&Copy", ui::kInvalidCommand, [&] { ran = true; }, false, {}, {}}},
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
        {{"&Copy", ui::kInvalidCommand, nullptr, false, {}, {}}}, mf.app, mf.desktop);

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
        {{"&Copy", ui::kInvalidCommand, nullptr, false, {}, {}}}, mf.app, mf.desktop);

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
        {"&File", {{"&New", ui::kInvalidCommand, nullptr, false, {}, {}}}},
        {"&Help", {{"&About", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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
        std::vector<MenuBarItem>{{"&Help", {{"&About", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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
        {"&File", {{"&New", ui::kInvalidCommand, nullptr, false, {}, {}},
                   {"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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
        {"&File", {{"&New", ui::kInvalidCommand, nullptr, false, {}, {}},
                   {"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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
        {"&File", {{"&New", ui::kInvalidCommand, nullptr, false, {}, {}},
                   {"&Open", ui::kInvalidCommand, nullptr, false, {}, {}}}}});
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
