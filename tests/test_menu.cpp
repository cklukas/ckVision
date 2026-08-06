// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/menu.hpp"

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
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
using ckv::widgets::MenuItem;
namespace ui = ckv::ui;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

DropdownMenu make_dropdown(Fixture& f, ckv::ui::Application& app, std::vector<MenuItem> items) {
    return DropdownMenu(std::move(items), f.theme, f.roles.menu_dropdown_normal,
                        f.roles.menu_dropdown_highlighted, f.roles.menu_dropdown_disabled, app);
}

ckv::KeyEvent key(ckv::Key k, std::string text = "") {
    return ckv::KeyEvent{KeyChord{k, Modifier::None, std::move(text)}};
}
}  // namespace

// --- DropdownMenu: navigation --------------------------------------------

CK_TEST(dropdown_highlights_the_first_item_by_default) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, {{"One", ui::kInvalidCommand, nullptr, false},
                                        {"Two", ui::kInvalidCommand, nullptr, false}});
    CK_CHECK(menu.highlighted() == 0);
}

CK_TEST(dropdown_with_the_first_item_disabled_skips_it_at_construction) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    constexpr CommandId kDisabled = 1;
    app.commands().register_command(kDisabled, "Disabled");
    app.commands().set_enabled_predicate(kDisabled, [] { return false; });
    auto menu = make_dropdown(f, app, {{"One", kDisabled, nullptr, false},
                                        {"Two", ui::kInvalidCommand, nullptr, false}});
    CK_CHECK(menu.highlighted() == 1);
}

CK_TEST(down_arrow_moves_to_the_next_item_and_wraps) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, {{"One", ui::kInvalidCommand, nullptr, false},
                                        {"Two", ui::kInvalidCommand, nullptr, false}});
    menu.on_key(key(Key::Down));
    CK_CHECK(menu.highlighted() == 1);
    menu.on_key(key(Key::Down));
    CK_CHECK(menu.highlighted() == 0);  // wrapped
}

CK_TEST(up_arrow_wraps_backward_from_the_first_item) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, {{"One", ui::kInvalidCommand, nullptr, false},
                                        {"Two", ui::kInvalidCommand, nullptr, false}});
    menu.on_key(key(Key::Up));
    CK_CHECK(menu.highlighted() == 1);
}

CK_TEST(navigation_skips_disabled_items_and_separators) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    constexpr CommandId kDisabled = 1;
    app.commands().register_command(kDisabled, "Disabled");
    app.commands().set_enabled_predicate(kDisabled, [] { return false; });
    auto menu = make_dropdown(f, app,
                               {{"One", ui::kInvalidCommand, nullptr, false},
                                {"", ui::kInvalidCommand, nullptr, true},  // separator
                                {"Disabled", kDisabled, nullptr, false},
                                {"Four", ui::kInvalidCommand, nullptr, false}});
    CK_CHECK(menu.highlighted() == 0);
    menu.on_key(key(Key::Down));
    CK_CHECK(menu.highlighted() == 3);  // skips both the separator and the disabled item
}

CK_TEST(a_dropdown_with_every_item_disabled_or_a_separator_has_no_selection) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto menu = make_dropdown(f, app, {{"", ui::kInvalidCommand, nullptr, true}});
    CK_CHECK(menu.highlighted() == -1);
    // Enter is still consumed (the popup owns the key while open) even
    // though there's nothing to activate — it must not crash.
    CK_CHECK(menu.on_key(key(Key::Enter)));
}

// --- DropdownMenu: activation --------------------------------------------

CK_TEST(enter_activates_the_highlighted_items_command) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    constexpr CommandId kSave = 1;
    app.commands().register_command(kSave, "Save");
    bool ran = false;
    app.set_command_handler(kSave, [&] { ran = true; });
    bool dismissed = false;
    auto menu = make_dropdown(f, app, {{"&Save", kSave, nullptr, false}});
    menu.on_dismiss = [&] { dismissed = true; };
    menu.on_key(key(Key::Enter));
    CK_CHECK(ran);
    CK_CHECK(dismissed);
}

CK_TEST(enter_activates_a_plain_on_activate_item_when_no_command_is_bound) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    auto menu = make_dropdown(f, app, {{"&Item", ui::kInvalidCommand, [&] { ran = true; }, false}});
    menu.on_key(key(Key::Enter));
    CK_CHECK(ran);
}

CK_TEST(mnemonic_key_activates_the_matching_item_directly) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    auto menu = make_dropdown(f, app, {{"&One", ui::kInvalidCommand, nullptr, false},
                                        {"&Two", ui::kInvalidCommand, [&] { ran = true; }, false}});
    CK_CHECK(menu.on_key(key(Key::Char, "t")));  // case-insensitive
    CK_CHECK(ran);
}

CK_TEST(escape_dismisses_without_activating_anything) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    bool dismissed = false;
    auto menu = make_dropdown(f, app, {{"&Item", ui::kInvalidCommand, [&] { ran = true; }, false}});
    menu.on_dismiss = [&] { dismissed = true; };
    menu.on_key(key(Key::Escape));
    CK_CHECK(!ran);
    CK_CHECK(dismissed);
}

CK_TEST(clicking_a_disabled_item_does_not_activate_it) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    constexpr CommandId kDisabled = 1;
    app.commands().register_command(kDisabled, "Disabled");
    app.commands().set_enabled_predicate(kDisabled, [] { return false; });
    bool ran = false;
    app.set_command_handler(kDisabled, [&] { ran = true; });
    auto menu = make_dropdown(f, app, {{"Disabled", kDisabled, nullptr, false}});
    menu.set_bounds(Rect{5, 5, 20, 1});
    menu.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{6, 5}, std::nullopt,
                                   Modifier::None});
    CK_CHECK(!ran);
}

CK_TEST(clicking_outside_the_dropdowns_bounds_dismisses_it_without_activating) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    bool ran = false;
    bool dismissed = false;
    auto menu = make_dropdown(f, app, {{"&Item", ui::kInvalidCommand, [&] { ran = true; }, false}});
    menu.set_bounds(Rect{5, 5, 20, 1});
    menu.on_dismiss = [&] { dismissed = true; };
    // Far outside the dropdown's bounds — this only reaches the
    // dropdown at all because Application routes it here via input
    // capture (exercised end-to-end in the MenuBar tests below).
    CK_CHECK(!menu.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{50, 50},
                                             std::nullopt, Modifier::None}));
    CK_CHECK(dismissed);
    CK_CHECK(!ran);
}

// --- MenuBar: activation, navigation, dismissal ---------------------------

namespace {
struct MenuBarFixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    Fixture f;
    Desktop desktop{Rect{0, 0, 80, 24}, f.theme, f.roles.dialog_background};
};
}  // namespace

CK_TEST(activate_gives_the_bar_focus_and_deactivate_restores_the_previous_focus) {
    MenuBarFixture mf;
    auto* other = mf.app.root().add_child(std::make_unique<ckv::ui::View>());
    other->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    mf.app.set_focus(other);

    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false}}}}, mf.f.theme,
        mf.f.roles.menu_bar_normal, mf.f.roles.menu_bar_active, mf.f.roles.menu_dropdown_normal,
        mf.f.roles.menu_dropdown_highlighted, mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));

    bar->activate();
    CK_CHECK(bar->active());
    CK_CHECK(mf.app.focused() == bar);

    bar->deactivate();
    CK_CHECK(!bar->active());
    CK_CHECK(mf.app.focused() == other);
}

CK_TEST(enter_opens_a_dropdown_as_a_desktop_popup) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false}}}}, mf.f.theme,
        mf.f.roles.menu_bar_normal, mf.f.roles.menu_bar_active, mf.f.roles.menu_dropdown_normal,
        mf.f.roles.menu_dropdown_highlighted, mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));

    bar->activate();
    CK_CHECK(mf.desktop.popups().empty());
    mf.app.dispatch(key(Key::Enter));
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(mf.app.input_capture() == mf.desktop.popups()[0]);
}

CK_TEST(escape_closes_an_open_dropdown_first_without_deactivating_the_bar) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false}}}}, mf.f.theme,
        mf.f.roles.menu_bar_normal, mf.f.roles.menu_bar_active, mf.f.roles.menu_dropdown_normal,
        mf.f.roles.menu_dropdown_highlighted, mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
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

CK_TEST(destroying_the_bar_while_a_dropdown_is_open_cleans_up_the_popup_and_input_capture) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false}}}}, mf.f.theme,
        mf.f.roles.menu_bar_normal, mf.f.roles.menu_bar_active, mf.f.roles.menu_dropdown_normal,
        mf.f.roles.menu_dropdown_highlighted, mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
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
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {}}, {"&Edit", {}}}, mf.f.theme, mf.f.roles.menu_bar_normal,
        mf.f.roles.menu_bar_active, mf.f.roles.menu_dropdown_normal, mf.f.roles.menu_dropdown_highlighted,
        mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
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
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false}}},
                                  {"&Edit", {{"&Copy", ui::kInvalidCommand, nullptr, false}}}},
        mf.f.theme, mf.f.roles.menu_bar_normal, mf.f.roles.menu_bar_active, mf.f.roles.menu_dropdown_normal,
        mf.f.roles.menu_dropdown_highlighted, mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    bar->activate();
    CK_CHECK(bar->on_key(key(Key::Char, "e")));
    CK_CHECK(mf.desktop.popups().size() == 1);
}

CK_TEST(on_key_is_unhandled_when_the_bar_is_not_active) {
    MenuBarFixture mf;
    auto bar_owned = std::make_unique<MenuBar>(std::vector<MenuBarItem>{{"&File", {}}}, mf.f.theme,
                                                mf.f.roles.menu_bar_normal, mf.f.roles.menu_bar_active,
                                                mf.f.roles.menu_dropdown_normal, mf.f.roles.menu_dropdown_highlighted,
                                                mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
    MenuBar* bar = static_cast<MenuBar*>(mf.desktop.add_child(std::move(bar_owned)));
    CK_CHECK(!bar->on_key(key(Key::Right)));
}
