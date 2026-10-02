// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/date_time_dialog.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/combo_box.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;
using namespace ckv::widgets;

namespace {

// An application with a desktop covering its root, as every presented dialog
// needs, driven the way a terminal drives one.
struct Script {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;

    Script() {
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        desktop = app.root().add(std::make_unique<Desktop>(app.root().bounds()));
        app.step(0);
    }

    void press(Key key, Modifier modifiers = Modifier::None) {
        app.dispatch(ckv::KeyEvent{KeyChord{key, modifiers, ""}});
        app.step(0);
    }
    void type(std::string_view text) {
        for (const char c : text) {
            app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, std::string(1, c)}});
            app.step(0);
        }
    }
    void click(ckv::Point cell) {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.step(0);
    }

    std::string screen() const {
        std::string out;
        const ckv::scene::Surface& surface = app.composed_surface();
        for (int y = 0; y < surface.size().height; ++y) {
            for (int x = 0; x < surface.size().width; ++x) out += surface.at(ckv::Point{x, y}).grapheme();
            out += '\n';
        }
        return out;
    }
    bool shows(std::string_view text) const { return screen().find(text) != std::string::npos; }
    // Whether the reason a SpinBox gives for 1583..9999 is on screen: a
    // sentence the dialog wraps onto its two reason rows.
    bool shows_year_reason() const { return shows("Enter a number from") && shows("1583 to 9999."); }
};

// The first view of type T in `view`'s subtree.
template <class T>
T* find_in(ckv::ui::View& view) {
    if (auto* found = dynamic_cast<T*>(&view)) return found;
    for (const auto& child : view.children())
        if (T* found = find_in<T>(*child)) return found;
    return nullptr;
}

}  // namespace

CK_TEST(a_date_dialog_opens_on_its_day_and_accepts_the_day_the_arrows_reach) {
    Script s;
    DateDialogOptions options;
    options.initial = DateValue{2026, 8, 19};
    DateDialogPresentation presentation = present_modal_date_dialog(s.app, *s.desktop, s.roles, options);
    std::optional<DateDialogResult> answer;
    presentation.set_completion_handler([&](DateDialogResult result) { answer = result; });
    s.app.step(0);
    CK_CHECK(s.shows("Select Date"));
    CK_CHECK(s.shows("August"));
    CK_CHECK(s.shows("2026"));
    CK_CHECK(!s.shows("< 2026 >"));
    CK_CHECK(dynamic_cast<CalendarView*>(s.app.focused()) != nullptr);

    s.press(Key::Right);  // the 20th
    s.press(Key::Down);   // a week on: the 27th
    s.press(Key::Enter);
    CK_CHECK((answer == DateDialogResult{true, DateValue{2026, 8, 27}}));
    CK_CHECK(!s.app.is_modal());
}

CK_TEST(escape_dismisses_a_date_dialog_without_an_answer) {
    Script s;
    DateDialogPresentation presentation = present_modal_date_dialog(s.app, *s.desktop, s.roles, DateDialogOptions{});
    std::optional<DateDialogResult> answer;
    presentation.set_completion_handler([&](DateDialogResult result) { answer = result; });
    s.app.step(0);
    s.press(Key::Right);
    s.press(Key::Escape);
    s.app.step(0);
    CK_CHECK((answer == DateDialogResult{}));
    CK_CHECK(!answer->accepted);
}

CK_TEST(a_typed_year_moves_the_calendar_and_a_refused_one_vetoes_the_accept_with_its_reason) {
    Script s;
    DateDialogOptions options;
    options.initial = DateValue{2024, 2, 29};
    DateDialogPresentation presentation = present_modal_date_dialog(s.app, *s.desktop, s.roles, options);
    std::optional<DateDialogResult> answer;
    presentation.set_completion_handler([&](DateDialogResult result) { answer = result; });
    s.app.step(0);
    ckv::ui::View* const calendar = s.app.focused();
    // Tab walks the month, the year and the days; from the days it wraps to
    // the month and then reaches the year.
    s.press(Key::Tab);  // OK
    s.press(Key::Tab);  // Cancel
    s.press(Key::Tab);  // the month
    s.press(Key::Tab);  // the year
    auto* year = dynamic_cast<SpinBox*>(s.app.focused());
    CK_CHECK(year != nullptr);
    if (year == nullptr) return;
    CK_CHECK(year->editable());

    // 2025 has no 29 February: the day is kept as far as the month allows.
    s.type("2025");
    s.press(Key::Enter);
    CK_CHECK(year->value() == 2025);
    auto* view = dynamic_cast<CalendarView*>(calendar);
    CK_CHECK(view != nullptr && view->selected() == (DateValue{2025, 2, 28}));

    // A year the calendar cannot draw is refused, never clamped, and OK is
    // vetoed while it stands: the field keeps the focus and says why.
    s.type("1200");
    s.press(Key::Enter);
    CK_CHECK(!year->valid());
    CK_CHECK(year->value() == 2025);
    CK_CHECK(s.shows_year_reason());
    auto* ok = find_in<Button>(*s.desktop->windows().back());
    CK_CHECK(ok != nullptr);
    if (ok == nullptr) return;
    const ckv::Rect button = ok->absolute_bounds();
    s.click(ckv::Point{button.x + 1, button.y});
    s.app.step(0);
    CK_CHECK(!answer);
    CK_CHECK(s.app.focused() == year);
    CK_CHECK(s.shows_year_reason());

    // Corrected, the answer goes through with the day the calendar holds.
    for (int i = 0; i < 4; ++i) s.press(Key::Backspace);
    s.type("2028");
    s.press(Key::Enter);
    CK_CHECK(!s.shows_year_reason());
    CK_CHECK(view != nullptr && view->selected() == (DateValue{2028, 2, 28}));
    s.click(ckv::Point{button.x + 1, button.y});
    s.app.step(0);
    CK_CHECK((answer == DateDialogResult{true, DateValue{2028, 2, 28}}));
}

CK_TEST(picking_a_month_in_the_date_dialog_moves_the_selection_there) {
    Script s;
    DateDialogOptions options;
    options.initial = DateValue{2026, 1, 31};
    DateDialogPresentation presentation = present_modal_date_dialog(s.app, *s.desktop, s.roles, options);
    s.app.step(0);
    auto* month = find_in<ComboBox>(*s.desktop->windows().back());
    CK_CHECK(month != nullptr);
    if (month == nullptr) return;
    s.app.set_focus(month);
    s.press(Key::Down);   // opens the month list
    s.press(Key::Down);   // February
    s.press(Key::Enter);  // chosen
    auto* calendar = find_in<CalendarView>(*s.desktop->windows().back());
    CK_CHECK(calendar != nullptr && calendar->selected() == (DateValue{2026, 2, 28}));
    CK_CHECK(month->text() == "February");
}

CK_TEST(a_date_dialog_speaks_the_language_of_its_tables_and_reads_no_locale) {
    Script s;
    DateDialogOptions options;
    options.initial = DateValue{2026, 3, 4};
    options.labels.month_names = {"Januar", "Februar", "März",      "April",   "Mai",      "Juni",
                                  "Juli",   "August",  "September", "Oktober", "November", "Dezember"};
    options.labels.weekday_names = {"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};
    StandardStrings strings;
    strings.select_date_title = "Datum wählen";
    strings.ok = "OK";
    strings.cancel = "Abbrechen";
    DateDialogPresentation presentation = present_modal_date_dialog(s.app, *s.desktop, s.roles, options, strings);
    s.app.step(0);
    CK_CHECK(s.shows("Datum wählen"));
    CK_CHECK(s.shows("März"));
    CK_CHECK(s.shows("Mo Di Mi Do Fr Sa So"));
    CK_CHECK(s.shows("Abbrechen"));
    CK_CHECK(!s.shows("March"));
}

CK_TEST(a_date_dialog_does_not_let_the_reader_leave_its_range) {
    Script s;
    DateDialogOptions options;
    options.initial = DateValue{2026, 8, 30};
    options.maximum = DateValue{2026, 8, 31};
    options.today = DateValue{2026, 8, 19};
    DateDialogPresentation presentation = present_modal_date_dialog(s.app, *s.desktop, s.roles, options);
    std::optional<DateDialogResult> answer;
    presentation.set_completion_handler([&](DateDialogResult result) { answer = result; });
    s.app.step(0);
    s.press(Key::Right);
    s.press(Key::Right);  // 1 September is past the range
    s.press(Key::Enter);
    s.app.step(0);
    CK_CHECK((answer == DateDialogResult{true, DateValue{2026, 8, 31}}));
}

CK_TEST(a_time_dialog_sets_its_time_with_the_arrows_and_answers_it_on_enter) {
    Script s;
    TimeDialogOptions options;
    options.initial = TimeValue{9, 41, 7};
    TimeDialogPresentation presentation = present_modal_time_dialog(s.app, *s.desktop, s.roles, options);
    std::optional<TimeDialogResult> answer;
    presentation.set_completion_handler([&](TimeDialogResult result) { answer = result; });
    s.app.step(0);
    CK_CHECK(s.shows("Select Time"));
    CK_CHECK(s.shows("09:41:07"));
    CK_CHECK(dynamic_cast<TimePicker*>(s.app.focused()) != nullptr);
    s.press(Key::Up);     // the hour
    s.press(Key::Right);  // then the minutes
    s.press(Key::Down);
    s.press(Key::Enter);
    s.app.step(0);
    CK_CHECK((answer == TimeDialogResult{true, TimeValue{10, 40, 7}}));
}

CK_TEST(a_time_dialog_on_the_twelve_hour_face_uses_its_tables_meridiem_and_hides_no_seconds_it_answers) {
    Script s;
    TimeDialogOptions options;
    options.initial = TimeValue{21, 5, 30};
    options.show_seconds = false;
    options.hour_format = HourFormat::TwelveHour;
    options.labels.am = "vorm.";
    options.labels.pm = "nachm.";
    TimeDialogPresentation presentation = present_modal_time_dialog(s.app, *s.desktop, s.roles, options);
    std::optional<TimeDialogResult> answer;
    presentation.set_completion_handler([&](TimeDialogResult result) { answer = result; });
    s.app.step(0);
    CK_CHECK(s.shows("09:05 nachm."));
    CK_CHECK(!s.shows("PM"));
    s.press(Key::Enter);
    s.app.step(0);
    // The seconds the reader was never shown are not part of the answer.
    CK_CHECK((answer == TimeDialogResult{true, TimeValue{21, 5, 0}}));
}

CK_TEST(cancel_and_a_host_quit_complete_the_time_dialog_unanswered) {
    Script s;
    TimeDialogPresentation presentation = present_modal_time_dialog(s.app, *s.desktop, s.roles, TimeDialogOptions{});
    std::optional<TimeDialogResult> answer;
    presentation.set_completion_handler([&](TimeDialogResult result) { answer = result; });
    s.app.step(0);
    std::vector<Button*> buttons;
    const auto collect = [&](auto&& self, ckv::ui::View& view) -> void {
        if (auto* button = dynamic_cast<Button*>(&view)) buttons.push_back(button);
        for (const auto& child : view.children()) self(self, *child);
    };
    collect(collect, *s.desktop->windows().back());
    CK_CHECK(buttons.size() == 2U);
    if (buttons.size() != 2U) return;
    const ckv::Rect cancel = buttons[1]->absolute_bounds();
    s.click(ckv::Point{cancel.x + 1, cancel.y});
    s.app.step(0);
    CK_CHECK((answer == TimeDialogResult{}));

    // A dialog still up when its window is taken away answers the same.
    TimeDialogPresentation second = present_modal_time_dialog(s.app, *s.desktop, s.roles, TimeDialogOptions{});
    s.app.step(0);
    std::unique_ptr<ckv::ui::View> removed = s.desktop->remove_child(s.desktop->windows().back());
    CK_CHECK(second.completed());
    CK_CHECK((second.result() == TimeDialogResult{}));
}
