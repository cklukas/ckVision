// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/combo_box.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"

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

std::string row_text(const ckv::scene::Surface& surface, int row) {
    std::string out;
    for (int x = 0; x < surface.size().width; ++x) out += surface.at(ckv::Point{x, row}).grapheme();
    return out;
}
}  // namespace

CK_TEST(pick_only_combo_opens_moves_and_accepts_a_selection) {
    ComboBox combo;
    combo.set_items({"One", "Two", "Three"});
    int selected = -1;
    combo.on_select = [&](std::size_t index) { selected = static_cast<int>(index); };

    CK_CHECK(combo.on_key(key(Key::Down)));
    CK_CHECK(combo.dropdown_open());
    CK_CHECK(combo.selected_index() == 0);
    CK_CHECK(combo.on_key(key(Key::Down)));
    CK_CHECK(combo.selected_index() == 1);
    CK_CHECK(combo.on_key(key(Key::Enter)));
    CK_CHECK(!combo.dropdown_open());
    CK_CHECK(combo.text() == "Two");
    CK_CHECK(selected == 1);
}

CK_TEST(editable_combo_accepts_text_and_uses_history_registry) {
    ComboBox combo(ComboBoxMode::Editable);
    HistoryRegistry history;
    history.record("recent", "old value");
    combo.set_history(&history, "recent");

    CK_CHECK(combo.on_key(key(Key::Char, Modifier::None, "A")));
    CK_CHECK(combo.on_key(key(Key::Char, Modifier::None, "b")));
    CK_CHECK(combo.text() == "Ab");
    CK_CHECK(combo.on_key(key(Key::Enter)));
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

CK_TEST(open_combo_draws_dropdown_rows_inside_its_bounds) {
    Fixture f;
    ComboBox combo;
    combo.set_context(f.ctx());
    combo.set_bounds(Rect{0, 0, 10, 4});
    combo.set_items({"One", "Two"});
    combo.open_dropdown();

    ckv::scene::Surface surface(ckv::Size{10, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 10, 4});
    combo.draw(painter);

    CK_CHECK(row_text(surface, 1).substr(0, 3) == "One");
    CK_CHECK(row_text(surface, 2).substr(0, 3) == "Two");
}


CK_TEST(an_open_list_asks_for_the_height_it_needs) {
    ComboBox combo;
    combo.set_items({"one", "two", "three"});
    CK_CHECK(combo.vertical_size_hint().preferred == 1);
    combo.open_dropdown();
    // The list is drawn inside the control, so opening it changes the height
    // the control needs -- a container hears that and lays it out taller.
    CK_CHECK(combo.vertical_size_hint().preferred == 4);
    combo.close_dropdown();
    CK_CHECK(combo.vertical_size_hint().preferred == 1);
}

CK_TEST(a_list_given_less_room_than_it_needs_scrolls_to_the_selection) {
    Fixture fixture;
    ComboBox combo;
    combo.set_items({"one", "two", "three", "four", "five"});
    combo.set_context(fixture.ctx());
    combo.on_attached();
    combo.set_bounds(Rect{0, 0, 12, 3});  // room for the row and two items
    combo.open_dropdown();

    auto paint = [&](ckv::scene::Surface& surface) {
        ckv::scene::Painter painter(surface, Rect{0, 0, 12, 3});
        combo.draw(painter);
    };
    ckv::scene::Surface surface(ckv::Size{12, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    paint(surface);
    CK_CHECK(row_text(surface, 1).find("one") == 0);

    for (int i = 0; i < 4; ++i) combo.on_key(key(Key::Down));  // walk to the last item
    ckv::scene::Surface scrolled(ckv::Size{12, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    paint(scrolled);
    // The tail of the list is reachable: what the eye sees is the selection.
    CK_CHECK(row_text(scrolled, 2).find("five") == 0);
    // ...and the mouse agrees with the eye, rather than with index 0.
    combo.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 2},
                                   std::nullopt, Modifier::None});
    CK_CHECK(combo.text() == "five");
}
