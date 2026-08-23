// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/combo_box.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/desktop.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::HistoryRegistry;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::ComboBox;
using ckv::widgets::ComboBoxMode;

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

}  // namespace

namespace {
// A combo box on a desktop, which is what it takes for it to have somewhere
// to drop its list.
struct ComboOnDesktop {
    ckv::term::HeadlessTerminal term{ckv::Size{40, 16}};
    ckv::ManualClock clock;
    ckv::ui::Application app{term, clock};
    ckv::widgets::Desktop* desktop = nullptr;
    ComboBox* combo = nullptr;

    ComboOnDesktop() {
        const StandardRoles roles = intern_standard_roles(app.roles());
        app.theme() = make_classic_theme(app.roles(), roles);
        desktop = static_cast<ckv::widgets::Desktop*>(
            app.root().add_child(std::make_unique<ckv::widgets::Desktop>(app.root().bounds())));
        combo = static_cast<ComboBox*>(desktop->add_child(std::make_unique<ComboBox>()));
        combo->set_bounds(Rect{2, 2, 12, 1});
        app.step(0);
    }

    void press(Key k) {
        app.dispatch(ckv::KeyEvent{KeyChord{k, Modifier::None, ""}});
        app.step(0);
    }
};
}  // namespace

CK_TEST(a_combo_drops_a_popup_list_and_takes_the_row_chosen_from_it) {
    ComboOnDesktop c;
    c.combo->set_items({"One", "Two", "Three"});
    int selected = -1;
    c.combo->on_select = [&](std::size_t index) { selected = static_cast<int>(index); };
    c.app.set_focus(c.combo);

    c.press(Key::Down);
    // A real popup on the desktop, not a list painted inside the control: the
    // row the combo occupies is still one row tall.
    CK_CHECK(c.desktop->popups().size() == 1);
    CK_CHECK(c.combo->dropdown_open());
    CK_CHECK(c.combo->bounds().height == 1);

    c.press(Key::Down);   // the list has the keys while it is up
    c.press(Key::Enter);
    CK_CHECK(c.desktop->popups().empty());
    CK_CHECK(!c.combo->dropdown_open());
    CK_CHECK(c.combo->text() == "Two");
    CK_CHECK(selected == 1);
}

CK_TEST(escape_closes_a_combos_list_without_changing_what_it_holds) {
    ComboOnDesktop c;
    c.combo->set_items({"One", "Two"});
    c.combo->set_selected_index(0);
    c.app.set_focus(c.combo);
    c.press(Key::Down);
    c.press(Key::Down);       // move within the list...
    c.press(Key::Escape);     // ...and take none of it
    CK_CHECK(c.desktop->popups().empty());
    CK_CHECK(c.combo->text() == "One");
}

CK_TEST(a_combo_with_nowhere_to_drop_a_list_still_steps_through_its_items) {
    // No desktop, so no popup. The control is still a control: the arrows
    // move the selection in place rather than doing nothing at all.
    ComboBox combo;
    combo.set_items({"One", "Two", "Three"});
    CK_CHECK(combo.on_key(key(Key::Down)));
    CK_CHECK(!combo.dropdown_open());
    CK_CHECK(combo.selected_index() == std::optional<std::size_t>{0});
    CK_CHECK(combo.on_key(key(Key::Down)));
    CK_CHECK(combo.text() == "Two");
    CK_CHECK(combo.on_key(key(Key::Up)));
    CK_CHECK(combo.text() == "One");
}

CK_TEST(editable_combo_accepts_text_and_uses_history_registry) {
    ComboBox combo(ComboBoxMode::Editable);
    HistoryRegistry history;
    history.record("recent", "old value");
    combo.set_history(&history, "recent");

    CK_CHECK(combo.on_key(key(Key::Char, Modifier::None, "A")));
    CK_CHECK(combo.on_key(key(Key::Char, Modifier::None, "b")));
    CK_CHECK(combo.text() == "Ab");
    // Enter records the value and passes the key ON: a closed combo has
    // nothing to confirm, and Enter in a form means "accept the form". While
    // it claimed the key, a dialog's default button was unreachable from the
    // keyboard for as long as any combo had focus.
    CK_CHECK(!combo.on_key(key(Key::Enter)));
    CK_CHECK(history.entries("recent")[0] == "Ab");

    CK_CHECK(combo.on_key(key(Key::Down)));
    CK_CHECK(combo.text() == "Ab");
    CK_CHECK(combo.on_key(key(Key::Down)));
    CK_CHECK(combo.text() == "old value");
    CK_CHECK(combo.on_key(key(Key::Up)));
    CK_CHECK(combo.text() == "Ab");
}

CK_TEST(editable_combo_uses_the_standard_text_editing_keymap) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    ComboBox combo(ComboBoxMode::Editable);
    combo.set_context(ckv::ui::Context{&app.theme(), &app.roles(), &app});
    combo.on_attached();
    combo.set_text("one two three");

    CK_CHECK(combo.on_key(key(Key::Left, Modifier::Ctrl)));
    CK_CHECK(combo.on_key(key(Key::Left, Modifier::Ctrl | Modifier::Shift)));
    CK_CHECK(combo.on_key(key(Key::Insert, Modifier::Ctrl)));
    CK_CHECK(app.clipboard_text() == "two ");
    CK_CHECK(combo.on_key(key(Key::Delete, Modifier::Shift)));
    CK_CHECK(combo.text() == "one three");
    CK_CHECK(combo.on_key(key(Key::Insert, Modifier::Shift)));
    CK_CHECK(combo.text() == "one two three");
}
