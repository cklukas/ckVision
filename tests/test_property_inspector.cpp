// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// PropertyInspector's typed cell editors: every kind is edited in place with
// the editor it calls for, checked on commit, and refused with a reason shown
// under the row.
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/popup_list.hpp"
#include "cvision/widgets/window.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::widgets::PropertyInspector;
using ckv::widgets::PropertyItem;
using ckv::widgets::PropertyKind;

namespace {

// An inspector in a window on a desktop, holding the keyboard: a desktop so a
// Choice row's list and a Date row's calendar have somewhere to drop.
struct InspectorFixture {
    ckv::term::HeadlessTerminal term{ckv::Size{50, 14}};
    ckv::ManualClock clock;
    ckv::ui::Application app{term, clock};
    ckv::widgets::Desktop* desktop = nullptr;
    PropertyInspector* inspector = nullptr;
    std::vector<std::pair<std::size_t, std::string>> changes;

    explicit InspectorFixture(std::vector<PropertyItem> items) {
        const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 50, 14}));
        auto window = std::make_unique<ckv::widgets::Window>("Properties");
        window->set_bounds(ckv::Rect{1, 1, 40, 11});
        auto* placed = desktop->add_window(std::move(window));
        inspector = placed->content_pane().add(std::make_unique<PropertyInspector>());
        inspector->set_bounds(ckv::Rect{0, 0, 36, 8});
        inspector->set_items(std::move(items));
        inspector->on_change = [this](std::size_t index, std::string value) {
            changes.emplace_back(index, std::move(value));
        };
        app.set_focus(inspector);
        app.step(0);
    }

    bool press(Key k, Modifier modifiers = Modifier::None, std::string text = {}) {
        const bool handled = app.dispatch(ckv::KeyEvent{KeyChord{k, modifiers, std::move(text)}});
        app.step(0);
        return handled;
    }
    void type(std::string_view text) {
        for (const char c : text) press(Key::Char, Modifier::None, std::string(1, c));
    }
    void erase(int count) {
        for (int i = 0; i < count; ++i) press(Key::Backspace);
    }
    void click(ckv::Point cell) {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.step(0);
    }
    // Row `y` of the inspector as drawn, from its left edge.
    std::string row(int y) const {
        const ckv::Rect at = inspector->absolute_bounds();
        std::string out;
        for (int x = at.x; x < at.right(); ++x) out += app.composed_surface().at(ckv::Point{x, at.y + y}).grapheme();
        return out;
    }
    ckv::Point value_cell(int y, int offset = 0) const {
        const ckv::Rect at = inspector->absolute_bounds();
        return ckv::Point{at.x + value_column() + offset, at.y + y};
    }
    // Where the values start: the widest name, then the two-cell gutter.
    int value_column() const {
        int widest = 0;
        for (const PropertyItem& item : inspector->items()) widest = std::max(widest, static_cast<int>(item.name.size()));
        return widest + 2;
    }
};

PropertyItem item(std::string name, std::string value, PropertyKind kind) {
    PropertyItem row{std::move(name), std::move(value), true};
    row.kind = kind;
    return row;
}

}  // namespace

CK_TEST(the_values_stand_in_an_aligned_second_column) {
    InspectorFixture f({{"Title", "Notes", true}, {"Encoding", "UTF-8", false},
                        item("Wrap", "true", PropertyKind::Bool)});
    CK_CHECK(f.row(0).starts_with("Title     Notes"));
    CK_CHECK(f.row(1).starts_with("Encoding  UTF-8"));
    CK_CHECK(f.row(2).starts_with("Wrap      [X]"));
}

CK_TEST(a_bool_property_toggles_in_place_by_enter_space_and_a_click) {
    InspectorFixture f({item("Wrap", "false", PropertyKind::Bool), {"Name", "a", true}});
    CK_CHECK(f.row(0).starts_with("Wrap  [ ]"));
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(!f.inspector->editing());  // nothing to type, so no editor
    CK_CHECK(f.inspector->items()[0].value == "true");
    CK_CHECK(f.row(0).starts_with("Wrap  [X]"));
    CK_CHECK(f.press(Key::Char, Modifier::None, " "));
    CK_CHECK(f.inspector->items()[0].value == "false");
    f.click(f.value_cell(0, 1));
    CK_CHECK(f.inspector->items()[0].value == "true");
    CK_CHECK((f.changes == std::vector<std::pair<std::size_t, std::string>>{{0U, "true"}, {0U, "false"}, {0U, "true"}}));
}

CK_TEST(a_choice_property_drops_its_list_and_commits_the_row_chosen) {
    PropertyItem wrap = item("Wrap", "Word", PropertyKind::Choice);
    wrap.choices = {"None", "Word", "Character"};
    InspectorFixture f({wrap});
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->editing());
    // The list is up at once, on the current choice; Down and Enter choose
    // the next, which commits the edit.
    bool list_open = false;
    for (ckv::ui::View* popup : f.desktop->popups())
        list_open = list_open || dynamic_cast<ckv::widgets::PopupList*>(popup) != nullptr;
    CK_CHECK(list_open);
    CK_CHECK(f.press(Key::Down));
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(!f.inspector->editing());
    CK_CHECK(f.inspector->items()[0].value == "Character");
    CK_CHECK(f.app.focused() == f.inspector);
    CK_CHECK(f.row(0).starts_with("Wrap  Character"));
    CK_CHECK((f.changes == std::vector<std::pair<std::size_t, std::string>>{{0U, "Character"}}));
}

CK_TEST(an_integer_property_refuses_what_is_not_a_whole_number_in_range_and_says_why) {
    PropertyItem width = item("Width", "8", PropertyKind::Integer);
    width.minimum = 1;
    width.maximum = 80;
    InspectorFixture f({width, {"After", "next", true}});
    CK_CHECK(f.press(Key::Enter));
    // The editor opens with the value selected, as a field reached from the
    // keyboard does: typing replaces it.
    f.type("x");
    CK_CHECK(f.press(Key::Enter));
    // Refused: still editing, the reason on the row under the value, and the
    // rows after it moved down to make room.
    CK_CHECK(f.inspector->editing());
    CK_CHECK(f.inspector->validation_message() == "Must be a whole number");
    CK_CHECK(f.row(1).find("Must be a whole number") != std::string::npos);
    CK_CHECK(f.row(2).starts_with("After"));
    CK_CHECK(f.inspector->items()[0].value == "8");
    // Changing the value takes the reason away again.
    f.erase(1);
    CK_CHECK(f.inspector->validation_message().empty());
    CK_CHECK(f.row(1).starts_with("After"));

    f.type("90");
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->validation_message() == "Must be at most 80");
    f.erase(2);
    f.type("0");
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->validation_message() == "Must be at least 1");
    f.erase(1);
    f.type("007");
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(!f.inspector->editing());
    CK_CHECK(f.inspector->items()[0].value == "7");  // committed in its plain spelling
    CK_CHECK((f.changes == std::vector<std::pair<std::size_t, std::string>>{{0U, "7"}}));
}

CK_TEST(a_real_property_commits_its_shortest_spelling_and_refuses_what_is_not_a_number) {
    PropertyItem scale = item("Scale", "1", PropertyKind::Real);
    scale.maximum = 4.5;
    InspectorFixture f({scale});
    CK_CHECK(f.press(Key::Enter));
    f.type(".");
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->validation_message() == "Must be a number");
    f.erase(1);
    f.type("1.50");
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->items()[0].value == "1.5");

    CK_CHECK(f.press(Key::Enter));
    f.type("4.75");
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->validation_message() == "Must be at most 4.5");
    // The reasons are the application's to word.
    ckv::widgets::PropertyInspectorMessages german;
    german.at_most = "Höchstens";
    f.inspector->set_messages(german);
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->validation_message() == "Höchstens 4.5");
    CK_CHECK(f.press(Key::Escape));
    CK_CHECK(f.inspector->items()[0].value == "1.5");
}

CK_TEST(a_date_property_is_edited_with_the_date_picker) {
    InspectorFixture f({item("Due", "2026-09-30", PropertyKind::Date)});
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->editing());
    CK_CHECK(dynamic_cast<ckv::widgets::DatePicker*>(f.app.focused()) != nullptr);
    // The picker's own keys: Right to the month, Up a month on; Enter
    // commits.
    CK_CHECK(f.press(Key::Right));  // year -> month
    CK_CHECK(f.press(Key::Up));
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->items()[0].value == "2026-10-30");
    CK_CHECK(f.row(0).starts_with("Due  2026-10-30"));
}

CK_TEST(new_items_close_the_calendar_a_date_edit_had_dropped) {
    // set_items ends the edit, since the value it began on is gone. The
    // calendar the picker dropped goes with it: left open, it would keep its
    // modal scope over a picker nobody can see, and a day chosen there would
    // edit a value that no longer exists.
    InspectorFixture f({item("Due", "2026-09-30", PropertyKind::Date)});
    CK_CHECK(f.press(Key::Enter));
    auto* picker = dynamic_cast<ckv::widgets::DatePicker*>(f.app.focused());
    CK_CHECK(picker != nullptr);
    if (picker == nullptr) return;
    CK_CHECK(f.press(Key::Char, Modifier::None, " "));  // Space drops the calendar
    CK_CHECK(picker->calendar_open());
    CK_CHECK(f.desktop->popups().size() == 1U);
    CK_CHECK(f.app.is_modal());

    // The host refreshes its model while the calendar is up.
    f.app.post([&f] { f.inspector->set_items({item("Start", "2026-01-01", PropertyKind::Date)}); });
    f.app.step(0);
    CK_CHECK(!f.inspector->editing());
    CK_CHECK(!picker->calendar_open());
    CK_CHECK(!picker->visible());
    CK_CHECK(f.desktop->popups().empty());
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.app.focused() == f.inspector);
    CK_CHECK(f.row(0).starts_with("Start  2026-01-01"));
    // The keyboard is the inspector's: Enter begins an edit of the new row.
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.app.focused() == picker);
    CK_CHECK(picker->value() == (ckv::widgets::DateValue{2026, 1, 1}));
    CK_CHECK(f.changes.empty());
}

CK_TEST(a_time_property_is_edited_with_the_time_picker_and_keeps_its_seconds_or_their_absence) {
    InspectorFixture f({item("Start", "09:30", PropertyKind::Time), item("Alarm", "06:00:15", PropertyKind::Time)});
    CK_CHECK(f.press(Key::Enter));
    auto* picker = dynamic_cast<ckv::widgets::TimePicker*>(f.app.focused());
    CK_CHECK(picker != nullptr);
    if (picker == nullptr) return;
    CK_CHECK(!picker->show_seconds());
    CK_CHECK(f.press(Key::Up));  // the hour
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->items()[0].value == "10:30");

    CK_CHECK(f.press(Key::Down));
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(picker->show_seconds());
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->items()[1].value == "06:00:15");
    CK_CHECK(f.changes.size() == 1U);  // an unchanged commit reports nothing
}

CK_TEST(escape_cancels_an_edit_and_tab_or_an_arrow_commits_it_on_the_way_out) {
    InspectorFixture f({{"Name", "Ada", true}, {"Role", "dev", true}});
    CK_CHECK(f.press(Key::Enter));
    f.type("m");
    CK_CHECK(f.press(Key::Escape));
    CK_CHECK(!f.inspector->editing());
    CK_CHECK(f.inspector->items()[0].value == "Ada");
    CK_CHECK(f.app.focused() == f.inspector);

    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.press(Key::End));  // keep the text, and type after it
    f.type("m");
    CK_CHECK(f.press(Key::Down));  // commits, then moves on
    CK_CHECK(f.inspector->items()[0].value == "Adam");
    CK_CHECK(f.inspector->cursor() == 1);

    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.press(Key::End));
    f.type("s");
    f.press(Key::Tab);
    CK_CHECK(f.inspector->items()[1].value == "devs");
    CK_CHECK(!f.inspector->editing());
}

CK_TEST(an_items_own_check_refuses_with_its_own_reason_and_a_refused_toggle_changes_nothing) {
    PropertyItem name{"Name", "Ada", true};
    name.validate = [](const std::string& text) -> std::optional<std::string> {
        if (text.empty()) return std::string("A name is required");
        return std::nullopt;
    };
    PropertyItem locked = item("Locked", "true", PropertyKind::Bool);
    locked.validate = [](const std::string& text) -> std::optional<std::string> {
        if (text == "false") return std::string("Unlock it from the owner's account");
        return std::nullopt;
    };
    InspectorFixture f({name, locked, {"After", "x", true}});
    CK_CHECK(f.press(Key::Enter));
    f.erase(3);
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->validation_message() == "A name is required");
    CK_CHECK(f.row(1).find("A name is required") != std::string::npos);
    CK_CHECK(f.press(Key::Escape));

    CK_CHECK(f.press(Key::Down));
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(f.inspector->items()[1].value == "true");
    CK_CHECK(f.row(2).find("Unlock it from the owner") != std::string::npos);
    CK_CHECK(f.row(3).starts_with("After"));
    CK_CHECK(f.changes.empty());
    // Whatever the reader does next takes the reason away.
    CK_CHECK(f.press(Key::Down));
    CK_CHECK(f.inspector->validation_message().empty());
    CK_CHECK(f.row(2).starts_with("After"));
}

CK_TEST(a_click_on_a_value_edits_it_and_a_click_on_another_row_commits_first) {
    InspectorFixture f({{"Name", "Ada", true}, {"Role", "dev", true}});
    f.click(f.value_cell(1, 1));
    CK_CHECK(f.inspector->cursor() == 1);
    CK_CHECK(f.inspector->editing());
    CK_CHECK(f.press(Key::End));
    f.type("s");
    const ckv::Rect at = f.inspector->absolute_bounds();
    f.click(ckv::Point{at.x + 1, at.y});  // Name's label: moves, does not edit
    CK_CHECK(f.inspector->items()[1].value == "devs");
    CK_CHECK(f.inspector->cursor() == 0);
    CK_CHECK(!f.inspector->editing());
}
