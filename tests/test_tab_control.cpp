// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/tab_control.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

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
