// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/menu.hpp"

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/window.hpp"

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

    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false}}}}, mf.f.theme,
        mf.f.roles.menu_bar_normal, mf.f.roles.menu_bar_active, mf.f.roles.menu_dropdown_normal,
        mf.f.roles.menu_dropdown_highlighted, mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
    MenuBar* bar = static_cast<MenuBar*>(mf.app.root().add_child(std::move(bar_owned)));
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
    auto bar_owned = std::make_unique<MenuBar>(
        std::vector<MenuBarItem>{{"&File", {{"&Reopen", ui::kInvalidCommand, nullptr, false}}}, {"&Edit", {}}},
        mf.f.theme, mf.f.roles.menu_bar_normal, mf.f.roles.menu_bar_active, mf.f.roles.menu_dropdown_normal,
        mf.f.roles.menu_dropdown_highlighted, mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
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
        std::vector<MenuBarItem>{{"&File", {{"&Open", ui::kInvalidCommand, nullptr, false}}}}, mf.f.theme,
        mf.f.roles.menu_bar_normal, mf.f.roles.menu_bar_active, mf.f.roles.menu_dropdown_normal,
        mf.f.roles.menu_dropdown_highlighted, mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
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

// --- Regression: Window::toggle_zoom bypassing clamp_size (review finding #4) ---

CK_TEST(zooming_into_an_area_smaller_than_the_windows_minimum_size_still_clamps) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    ckv::widgets::Window window("W", f.theme, f.roles.window_frame_active, f.roles.window_frame_inactive,
                                 f.roles.window_title_active, f.roles.window_title_inactive);
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
        {{"&Copy", ui::kInvalidCommand, nullptr, false}, {"&Paste", ui::kInvalidCommand, nullptr, false}},
        ckv::Point{10, 10}, mf.f.theme, mf.f.roles.menu_dropdown_normal, mf.f.roles.menu_dropdown_highlighted,
        mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
    CK_CHECK(mf.desktop.popups().size() == 1);
    CK_CHECK(mf.app.input_capture() == popup);
    CK_CHECK(popup->bounds().x == 10);
    CK_CHECK(popup->bounds().y == 10);
}

CK_TEST(show_context_menu_self_removes_and_clears_capture_on_escape) {
    MenuBarFixture mf;
    auto* popup = ckv::widgets::show_context_menu({{"&Copy", ui::kInvalidCommand, nullptr, false}},
                                                    ckv::Point{10, 10}, mf.f.theme, mf.f.roles.menu_dropdown_normal,
                                                    mf.f.roles.menu_dropdown_highlighted,
                                                    mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
    popup->on_key(key(Key::Escape));
    CK_CHECK(mf.desktop.popups().empty());
    CK_CHECK(mf.app.input_capture() == nullptr);
}

CK_TEST(show_context_menu_activating_an_item_closes_it_and_runs_the_action) {
    MenuBarFixture mf;
    bool ran = false;
    auto* popup = ckv::widgets::show_context_menu({{"&Copy", ui::kInvalidCommand, [&] { ran = true; }, false}},
                                                    ckv::Point{10, 10}, mf.f.theme, mf.f.roles.menu_dropdown_normal,
                                                    mf.f.roles.menu_dropdown_highlighted,
                                                    mf.f.roles.menu_dropdown_disabled, mf.app, mf.desktop);
    popup->on_key(key(Key::Enter));
    CK_CHECK(ran);
    CK_CHECK(mf.desktop.popups().empty());
}
