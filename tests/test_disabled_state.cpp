// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// D-076: a disabled view, and everything inside a disabled view, receives no
// input from the Application and draws its disabled face.
#include <memory>
#include <string>

#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/option_group.hpp"

using ckv::Cell;
using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::MouseAction;
using ckv::MouseButton;
using ckv::MouseEvent;
using ckv::Point;
using ckv::Rect;
using ckv::Size;
using ckv::Style;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::Application;
using ckv::ui::FocusPolicy;
using ckv::ui::View;
using ckv::widgets::Button;
using ckv::widgets::CheckGroup;
using ckv::widgets::InputLine;
using ckv::widgets::Label;
using ckv::widgets::ListView;

namespace {

// A plain container that records what reaches it.
class Pane : public View {
public:
    explicit Pane(Rect bounds) : View(bounds) { set_fills_root(false); }
    int mouse_events = 0;
    int key_events = 0;
    int text_events = 0;
    bool on_mouse(const MouseEvent&) override {
        ++mouse_events;
        return true;
    }
    bool on_key(const ckv::KeyEvent&) override {
        ++key_events;
        return false;
    }
    bool on_text(const ckv::TextEvent&) override {
        ++text_events;
        return false;
    }
};

class Field : public Pane {
public:
    explicit Field(Rect bounds) : Pane(bounds) { set_focus_policy(FocusPolicy::TabStop); }
    std::optional<ckv::CursorState> cursor_state() const override {
        const Rect absolute = absolute_bounds();
        return ckv::CursorState{true, Point{absolute.x, absolute.y}};
    }
};

struct AppFixture {
    ckv::term::HeadlessTerminal terminal{Size{40, 10}};
    ManualClock clock;
    Application app{terminal, clock};
};

MouseEvent press(Point cell) { return MouseEvent{MouseAction::Down, MouseButton::Left, cell, std::nullopt}; }
MouseEvent release(Point cell) { return MouseEvent{MouseAction::Up, MouseButton::Left, cell, std::nullopt}; }
ckv::KeyEvent key(Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }

struct DrawFixture {
    ckv::ui::RoleRegistry registry;
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, nullptr}; }
    Style style(ckv::ui::RoleId role) const { return theme.resolve(role); }
};

Surface make_surface(int w, int h) { return Surface(Size{w, h}, Cell::from_grapheme(" ", Style{})); }

}  // namespace

// --- Input routing -----------------------------------------------------

CK_TEST(a_press_on_a_disabled_button_never_fires_it_and_reaches_its_enabled_container) {
    AppFixture f;
    auto* pane = static_cast<Pane*>(f.app.root().add_child(std::make_unique<Pane>(Rect{0, 0, 20, 5})));
    auto* button = static_cast<Button*>(pane->add_child(std::make_unique<Button>("OK")));
    button->set_bounds(Rect{2, 1, 10, 2});
    int fired = 0;
    button->on_press = [&fired] { ++fired; };
    button->set_enabled(false);

    f.app.dispatch(press(Point{4, 1}));
    f.app.dispatch(release(Point{4, 1}));
    CK_CHECK(fired == 0);
    CK_CHECK(pane->mouse_events == 2);  // the container hears the press instead

    button->set_enabled(true);
    f.app.dispatch(press(Point{4, 1}));
    f.app.dispatch(release(Point{4, 1}));
    CK_CHECK(fired == 1);
}

CK_TEST(disabling_a_container_disables_every_descendant_for_focus_keys_and_text) {
    AppFixture f;
    auto* outer = static_cast<Pane*>(f.app.root().add_child(std::make_unique<Pane>(Rect{0, 0, 20, 5})));
    auto* inner = static_cast<Pane*>(outer->add_child(std::make_unique<Pane>(Rect{0, 0, 20, 5})));
    auto* field = static_cast<Field*>(inner->add_child(std::make_unique<Field>(Rect{0, 0, 5, 1})));
    f.app.set_focus(field);
    CK_CHECK(f.app.focused() == field);

    inner->set_enabled(false);
    CK_CHECK(!field->enabled_in_tree());
    CK_CHECK(field->enabled());  // its own flag is untouched
    CK_CHECK(!field->focusable());

    f.app.dispatch(key(Key::Down));
    f.app.dispatch(ckv::TextEvent{"x"});
    CK_CHECK(field->key_events == 0);
    CK_CHECK(field->text_events == 0);
    CK_CHECK(inner->key_events == 0);
    // The enabled container outside the disabled subtree still hears the
    // unhandled key, exactly as it would have from an enabled child.
    CK_CHECK(outer->key_events == 1);
    CK_CHECK(outer->text_events == 1);

    inner->set_enabled(true);
    CK_CHECK(field->focusable());
    f.app.dispatch(key(Key::Down));
    CK_CHECK(field->key_events == 1);
}

CK_TEST(a_disabled_subtree_is_skipped_by_focus_traversal) {
    AppFixture f;
    auto* group = static_cast<Pane*>(f.app.root().add_child(std::make_unique<Pane>(Rect{0, 0, 20, 5})));
    auto* first = static_cast<Field*>(group->add_child(std::make_unique<Field>(Rect{0, 0, 5, 1})));
    auto* second = static_cast<Field*>(f.app.root().add_child(std::make_unique<Field>(Rect{0, 6, 5, 1})));
    group->set_enabled(false);
    CK_CHECK(f.app.focus_next());
    CK_CHECK(f.app.focused() == second);
    CK_CHECK(f.app.focused() != first);
}

CK_TEST(a_press_captured_before_the_button_was_disabled_does_not_fire_on_release) {
    AppFixture f;
    auto* pane = static_cast<Pane*>(f.app.root().add_child(std::make_unique<Pane>(Rect{0, 0, 20, 5})));
    auto* button = static_cast<Button*>(pane->add_child(std::make_unique<Button>("OK")));
    button->set_bounds(Rect{2, 1, 10, 2});
    int fired = 0;
    button->on_press = [&fired] { ++fired; };

    f.app.dispatch(press(Point{4, 1}));  // the button now holds the pointer
    button->set_enabled(false);
    f.app.dispatch(release(Point{4, 1}));
    CK_CHECK(fired == 0);
}

CK_TEST(a_focused_view_that_becomes_disabled_places_no_cursor) {
    AppFixture f;
    auto* field = static_cast<Field*>(f.app.root().add_child(std::make_unique<Field>(Rect{3, 2, 5, 1})));
    f.app.set_focus(field);
    f.app.step(0);
    CK_CHECK(f.app.current_cursor().visible);
    field->set_enabled(false);
    f.app.step(0);
    CK_CHECK(!f.app.current_cursor().visible);
}

CK_TEST(a_disabled_button_ignores_its_mnemonic) {
    Button button("&Save");
    int fired = 0;
    button.on_press = [&fired] { ++fired; };
    button.set_enabled(false);
    CK_CHECK(!button.activate_mnemonic("s"));
    CK_CHECK(fired == 0);
    button.set_enabled(true);
    CK_CHECK(button.activate_mnemonic("s"));
    CK_CHECK(fired == 1);
}

// --- Drawing -------------------------------------------------------------

CK_TEST(a_disabled_button_wears_the_disabled_face_without_a_mnemonic_accent) {
    DrawFixture f;
    Surface s = make_surface(12, 2);
    Button button("&Save");
    button.set_context(f.ctx());
    button.on_attached();
    button.set_bounds(Rect{0, 0, 10, 2});
    button.set_enabled(false);
    Painter painter(s, Rect{0, 0, 12, 2});
    button.draw(painter);
    const Style disabled = f.style(f.roles.button_disabled);
    for (int x = 1; x < 9; ++x) CK_CHECK(s.at(Point{x, 0}).style() == disabled);  // "S" included
    CK_CHECK(s.at(Point{9, 0}).grapheme() == "▄");  // still a raised button
}

CK_TEST(a_disabled_field_shows_its_text_without_caret_selection_or_invalid_mark) {
    DrawFixture f;
    Surface s = make_surface(8, 1);
    InputLine field;
    field.set_context(f.ctx());
    field.on_attached();
    field.set_bounds(Rect{0, 0, 8, 1});
    field.set_text("abc");
    field.set_enabled(false);
    Painter painter(s, Rect{0, 0, 8, 1});
    field.draw(painter);
    const Style disabled = f.style(f.roles.input_disabled);
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "a");
    for (int x = 0; x < 8; ++x) CK_CHECK(s.at(Point{x, 0}).style() == disabled);
}

CK_TEST(a_disabled_check_group_keeps_its_marks_on_the_disabled_face) {
    DrawFixture f;
    Surface s = make_surface(12, 2);
    CheckGroup group({"&One", "Two"});
    group.set_context(f.ctx());
    group.on_attached();
    group.set_bounds(Rect{0, 0, 12, 2});
    group.set_checked(0, true);
    group.set_enabled(false);
    Painter painter(s, Rect{0, 0, 12, 2});
    group.draw(painter);
    CK_CHECK(s.at(Point{1, 0}).grapheme() == "X");
    const Style disabled = f.style(f.roles.option_disabled);
    for (int x = 0; x < 12; ++x) CK_CHECK(s.at(Point{x, 0}).style() == disabled);
}

CK_TEST(a_disabled_list_keeps_its_cursor_row_on_the_muted_selection) {
    DrawFixture f;
    Surface s = make_surface(8, 2);
    ListView list;
    list.set_context(f.ctx());
    list.on_attached();
    list.set_bounds(Rect{0, 0, 8, 2});
    list.set_items({"one", "two"});
    list.set_enabled(false);
    Painter painter(s, Rect{0, 0, 8, 2});
    list.draw(painter);
    const Style disabled = f.style(f.roles.list_disabled);
    const Style inactive = f.style(f.roles.list_selected_inactive);
    CK_CHECK(s.at(Point{0, 0}).style() == (Style{disabled.fg, inactive.bg, inactive.attrs | disabled.attrs}));
    CK_CHECK(s.at(Point{0, 1}).style() == disabled);
}

CK_TEST(a_disabled_label_keeps_its_surface_and_drops_its_mnemonic_accent) {
    DrawFixture f;
    Surface s = make_surface(6, 1);
    Label label("&Name");
    label.set_context(f.ctx());
    label.on_attached();
    label.set_bounds(Rect{0, 0, 6, 1});
    label.set_enabled(false);
    Painter painter(s, Rect{0, 0, 6, 1});
    label.draw(painter);
    const Style text = f.style(f.roles.label_text);
    const Style disabled = f.style(f.roles.label_disabled);
    for (int x = 0; x < 4; ++x) CK_CHECK(s.at(Point{x, 0}).style() == (Style{disabled.fg, text.bg, text.attrs | disabled.attrs}));
}
