// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/tab_control.hpp"

#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/label.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::TabControl;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

ckv::KeyEvent key(Key k, Modifier m = Modifier::None, std::string text = {}) {
    return ckv::KeyEvent{KeyChord{k, m, std::move(text)}};
}

std::string row_text(const ckv::scene::Surface& surface, int row) {
    std::string out;
    for (int x = 0; x < surface.size().width; ++x) out += surface.at(ckv::Point{x, row}).grapheme();
    return out;
}
}  // namespace

CK_TEST(tab_control_owns_pages_and_shows_only_the_active_page) {
    TabControl tabs;
    auto* first = tabs.add_tab("&One", std::make_unique<ckv::ui::View>());
    auto* second = tabs.add_tab("&Two", std::make_unique<ckv::ui::View>());

    CK_CHECK(tabs.tab_count() == 2);
    CK_CHECK(tabs.active_page() == first);
    CK_CHECK(first->visible());
    CK_CHECK(!second->visible());

    tabs.set_active_index(1);
    CK_CHECK(tabs.active_page() == second);
    CK_CHECK(!first->visible());
    CK_CHECK(second->visible());
}

CK_TEST(tab_control_keyboard_cycles_and_alt_mnemonics_activate_tabs) {
    TabControl tabs;
    tabs.add_tab("&One", std::make_unique<ckv::ui::View>());
    tabs.add_tab("&Two", std::make_unique<ckv::ui::View>());
    tabs.add_tab("T&hree", std::make_unique<ckv::ui::View>());

    CK_CHECK(tabs.on_key(key(Key::Right)));
    CK_CHECK(tabs.active_index() == 1);
    CK_CHECK(tabs.on_key(key(Key::Left)));
    CK_CHECK(tabs.active_index() == 0);
    CK_CHECK(tabs.on_key(key(Key::Char, Modifier::Alt, "h")));
    CK_CHECK(tabs.active_index() == 2);
}

CK_TEST(tab_control_resizes_active_page_below_the_tab_row) {
    TabControl tabs;
    auto* first = tabs.add_tab("&One", std::make_unique<ckv::ui::View>());
    tabs.set_bounds(Rect{0, 0, 40, 8});

    CK_CHECK(first->bounds() == (Rect{0, 1, 40, 7}));
}

CK_TEST(tab_control_draws_tab_labels) {
    Fixture f;
    TabControl tabs;
    tabs.set_context(f.ctx());
    tabs.set_bounds(Rect{0, 0, 30, 4});
    tabs.add_tab("&One", std::make_unique<ckv::ui::View>());
    tabs.add_tab("&Two", std::make_unique<ckv::ui::View>());

    ckv::scene::Surface surface(ckv::Size{30, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 30, 4});
    tabs.draw(painter);

    CK_CHECK(row_text(surface, 0).find("One") != std::string::npos);
    CK_CHECK(row_text(surface, 0).find("Two") != std::string::npos);
}

CK_TEST(a_tab_mnemonic_takes_the_hotkey_accent_and_a_disabled_strip_shows_none) {
    Fixture f;
    TabControl tabs;
    tabs.set_context(f.ctx());
    tabs.set_bounds(Rect{0, 0, 30, 4});
    tabs.add_tab("&One", std::make_unique<ckv::ui::View>());
    tabs.add_tab("T&wo", std::make_unique<ckv::ui::View>());

    ckv::scene::Surface surface(ckv::Size{30, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 30, 4});
    tabs.draw(painter);
    const std::string row = row_text(surface, 0);
    CK_CHECK(row.find('&') == std::string::npos);
    CK_CHECK(row.substr(0, 11) == " One   Two ");

    // The active tab's "O" and the other tab's "w": the accent's foreground
    // over whichever tab surface it sits on.
    const ckv::Style hotkey = f.theme.resolve(f.roles.hotkey);
    const ckv::Style active = f.theme.resolve(f.roles.menu_bar_active);
    const ckv::Style normal = f.theme.resolve(f.roles.menu_bar_normal);
    CK_CHECK(surface.at(ckv::Point{1, 0}).style().fg == hotkey.fg);
    CK_CHECK(surface.at(ckv::Point{1, 0}).style().bg == active.bg);
    CK_CHECK(surface.at(ckv::Point{2, 0}).style() == active);
    CK_CHECK(surface.at(ckv::Point{8, 0}).grapheme() == "w");
    CK_CHECK(surface.at(ckv::Point{8, 0}).style().fg == hotkey.fg);
    CK_CHECK(surface.at(ckv::Point{8, 0}).style().bg == normal.bg);
    CK_CHECK(surface.at(ckv::Point{7, 0}).style() == normal);

    tabs.set_enabled(false);
    tabs.draw(painter);
    CK_CHECK(surface.at(ckv::Point{1, 0}).style() == surface.at(ckv::Point{2, 0}).style());
    CK_CHECK(surface.at(ckv::Point{8, 0}).style() == surface.at(ckv::Point{7, 0}).style());
    CK_CHECK(surface.at(ckv::Point{1, 0}).style().fg != hotkey.fg);
}

CK_TEST(tab_and_shift_tab_are_left_for_focus_traversal) {
    // A24: Tab and Shift+Tab move the focus through the form; the strip is
    // stepped with its own keys, so a focused TabControl can be left.
    TabControl tabs;
    tabs.add_tab("&One", std::make_unique<ckv::ui::View>());
    tabs.add_tab("&Two", std::make_unique<ckv::ui::View>());
    CK_CHECK(!tabs.on_key(key(Key::Tab)));
    CK_CHECK(!tabs.on_key(key(Key::Tab, Modifier::Shift)));
    CK_CHECK(tabs.active_index() == 0);
    CK_CHECK(tabs.on_key(key(Key::Right)));
    CK_CHECK(tabs.active_index() == 1);
}

CK_TEST(a_scripted_tab_control_switches_pages_by_arrows_mnemonics_and_clicks) {
    // Application-level script: keys and clicks reach the control through
    // Application::dispatch, and each step() shows the page they chose.
    ckv::term::HeadlessTerminal term(ckv::Size{40, 8});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* tabs = app.root().add(std::make_unique<TabControl>());
    tabs->set_bounds(Rect{0, 0, 40, 6});
    tabs->add_tab("&General", std::make_unique<ckv::widgets::Label>("General page"));
    tabs->add_tab("&Display", std::make_unique<ckv::widgets::Label>("Display page"));
    tabs->add_tab("&Advanced", std::make_unique<ckv::widgets::Label>("Advanced page"));
    app.set_focus(tabs);
    app.step(0);
    const auto shown = [&](const std::string& text) {
        return row_text(app.composed_surface(), 1).find(text) != std::string::npos;
    };
    CK_CHECK(shown("General page"));

    CK_CHECK(app.dispatch(key(Key::Right)));
    app.step(0);
    CK_CHECK(tabs->active_index() == 1U);
    CK_CHECK(shown("Display page"));
    CK_CHECK(!shown("General page"));

    CK_CHECK(app.dispatch(key(Key::Left)));
    CK_CHECK(app.dispatch(key(Key::Left)));  // wraps from the first to the last
    app.step(0);
    CK_CHECK(tabs->active_index() == 2U);
    CK_CHECK(shown("Advanced page"));

    CK_CHECK(app.dispatch(key(Key::Char, Modifier::Alt, "d")));
    app.step(0);
    CK_CHECK(tabs->active_index() == 1U);

    // "General" takes columns 0..9 of the strip ("General" plus three).
    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{3, 0},
                                          std::nullopt, Modifier::None}));
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{3, 0}, std::nullopt,
                                 Modifier::None});
    app.step(0);
    CK_CHECK(tabs->active_index() == 0U);
    CK_CHECK(shown("General page"));
}
