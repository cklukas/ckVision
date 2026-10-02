// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Appearance specimens: components — the clocks, the calendar and the date
// and time controls, the steppers, and the command and navigation surfaces.
// Every date and time here is a fixed value handed to the widget; nothing
// reads a real clock.
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "appearance_matrix.hpp"
#include "cvision/widgets/big_clock.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"

namespace ckv::docgen::appearance {

namespace {

constexpr widgets::DateValue kShownDay{2026, 8, 19};

void press(Stage& stage, Key key) { stage.app().dispatch(KeyEvent{KeyChord{key, Modifier::None, ""}}); }

// Types `text` the way a terminal reports it: one character key at a time.
void type(Stage& stage, std::string_view text) {
    for (char c : text) stage.app().dispatch(KeyEvent{KeyChord{Key::Char, Modifier::None, std::string(1, c)}});
}

std::vector<std::string> month_names() {
    return {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October",
            "November", "December"};
}

// A host's table in another language: nothing a date or time control shows
// comes from anywhere else.
widgets::DateTimeLabels german_labels() {
    widgets::DateTimeLabels labels;
    labels.month_names = {"Januar", "Februar", "März",      "April",   "Mai",      "Juni",
                          "Juli",   "August",  "September", "Oktober", "November", "Dezember"};
    labels.weekday_names = {"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};
    labels.am = "vorm.";
    labels.pm = "nachm.";
    labels.no_date = "— kein Datum —";
    labels.not_a_date = "Kein Datum.";
    labels.invalid_year = "ungültig";
    return labels;
}

widgets::DateFormat dotted_format() {
    widgets::DateFormat format;
    format.order = {widgets::DateField::Day, widgets::DateField::Month, widgets::DateField::Year};
    format.separator = ".";
    return format;
}

// --- ClockView --------------------------------------------------------------

// A clock lives at the right end of a menu bar; that is where it is seen.
constexpr Size kClockScreen{26, 6};

widgets::ClockView& clock(Stage& stage, widgets::TimeValue time = {9, 41, 7}) {
    auto* bar = stage.desktop().dock_top(std::make_unique<widgets::MenuBar>(
        std::vector<widgets::MenuBarItem>{{"&File", {}}, {"&View", {}}}));
    auto* face = bar->set_trailing_view(std::make_unique<widgets::ClockView>());
    face->set_time_provider([time] { return time; });
    return *face;
}

void twelve_hour(widgets::ClockView& face, std::string label_am, std::string label_pm) {
    face.set_hour_format(widgets::HourFormat::TwelveHour);
    face.set_meridiem_labels(std::move(label_am), std::move(label_pm));
}

void add_clock_view(Catalog& catalog) {
    Element& e = catalog.element("ClockView", "include/cvision/widgets/common_components.hpp", Traits{.text = true});
    state(e, "normal", kClockScreen, [](Stage& s) { clock(s); });
    state(e, "seconds", kClockScreen, [](Stage& s) { clock(s).set_show_seconds(true); });
    state(e, "twelve-hour", kClockScreen, [](Stage& s) { twelve_hour(clock(s, {21, 41, 7}), "AM", "PM"); });
    // Showing what it opened. The keyboard walk of the bar highlights it the
    // same way, by design, so that state has no picture of its own.
    state(e, "open", kClockScreen, [](Stage& s) { clock(s).set_open(true); });
    state(e, "wide", kClockScreen, [](Stage& s) {
        twelve_hour(clock(s, {21, 41, 7}), std::string(kWideText), std::string(kWideText));
    });
    // A bar too short for its titles and the clock both.
    state(e, "narrow", Size{20, 6}, [](Stage& s) {
        widgets::ClockView& face = clock(s, {21, 41, 7});
        face.set_show_seconds(true);
        twelve_hour(face, "AM", "PM");
    });
}

// --- BigClockView -----------------------------------------------------------

widgets::BigClockView& big_clock(Stage& stage, Rect window, widgets::TimeValue time = {14, 5, 0}) {
    auto face = std::make_unique<widgets::BigClockView>();
    widgets::BigClockView& placed = *face;
    placed.set_show_seconds(false);
    placed.set_moment_provider([time] { return widgets::DateTimeValue{kShownDay, time}; });
    stage.window(window, "Clock", std::move(face), false);
    return placed;
}

// Every face here shows an afternoon time, so `pm` is the caption drawn.
void twelve_hour(widgets::BigClockView& face, std::string pm) {
    face.set_hour_format(widgets::HourFormat::TwelveHour);
    face.set_meridiem_labels("AM", std::move(pm));
}

// Room for "14:05" in block glyphs, and not for the date above it.
constexpr Size kFaceScreen{34, 10};
constexpr Rect kFace{1, 1, 31, 8};

void add_big_clock_view(Catalog& catalog) {
    Element& e = catalog.element("BigClockView", "include/cvision/widgets/big_clock.hpp",
                                 Traits{.focusable = true, .text = true});
    // A face fills its window, so holding the keyboard and being the active
    // window are one fact: unfocused, another window is the active one.
    state(e, "normal", kFaceScreen, [](Stage& s) {
        big_clock(s, kFace);
        s.document(Rect{20, 6, 12, 3}, "Notes");
    });
    state(e, "focused", kFaceScreen, [](Stage& s) { s.focus(big_clock(s, kFace)); });
    state(e, "seconds", Size{48, 10}, [](Stage& s) { big_clock(s, Rect{1, 1, 45, 8}).set_show_seconds(true); });
    state(e, "twelve-hour", Size{34, 12}, [](Stage& s) { twelve_hour(big_clock(s, Rect{1, 1, 31, 10}), "PM"); });
    // Too small for block glyphs: the same two lines, as plain text.
    state(e, "date-and-time", kFaceScreen, [](Stage& s) {
        big_clock(s, kFace).set_content(widgets::BigClockContent::DateAndTime);
    });
    state(e, "wide", Size{34, 12}, [](Stage& s) {
        twelve_hour(big_clock(s, Rect{1, 1, 31, 10}), std::string(kWideText));
    });
    state(e, "narrow", Size{20, 6}, [](Stage& s) { twelve_hour(big_clock(s, Rect{1, 1, 8, 4}), "PM"); });
}

// --- CalendarView -----------------------------------------------------------

constexpr Size kCalendarScreen{29, 13};
constexpr Rect kCalendarDialog{1, 1, 26, 11};

widgets::CalendarView& calendar(Stage& stage, Rect bounds = Rect{0, 0, 24, 8}) {
    ui::View& body = stage.dialog(kCalendarDialog, "Calendar");
    auto& view = stage.place(body, bounds, std::make_unique<widgets::CalendarView>());
    view.set_selected(kShownDay);
    return view;
}

void add_calendar_view(Catalog& catalog) {
    Element& e = catalog.element("CalendarView", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kCalendarScreen, [](Stage& s) { calendar(s); });
    state(e, "focused", kCalendarScreen, [](Stage& s) { s.focus(calendar(s)); });
    state(e, "disabled", kCalendarScreen, [](Stage& s) { calendar(s).set_enabled(false); });
    state(e, "today", kCalendarScreen, [](Stage& s) { calendar(s).set_today(widgets::DateValue{2026, 8, 9}); });
    state(e, "marked", kCalendarScreen, [](Stage& s) {
        calendar(s).set_marked_span(widgets::DateValue{2026, 8, 24}, widgets::DateValue{2026, 8, 28});
    });
    state(e, "unselectable", kCalendarScreen, [](Stage& s) {
        calendar(s).set_range(widgets::DateValue{2026, 8, 5}, widgets::DateValue{2026, 8, 26});
    });
    state(e, "iso-weeks", kCalendarScreen, [](Stage& s) { calendar(s).set_show_iso_week_numbers(true); });
    state(e, "wide", kCalendarScreen, [](Stage& s) {
        calendar(s).set_labels(std::vector<std::string>(12, std::string(kWideText)),
                               {"月", "火", "水", "木", "金", "土", "日"});
    });
    state(e, "narrow", kCalendarScreen, [](Stage& s) { calendar(s, Rect{0, 0, 12, 8}); });
}

// --- CalendarDropdown -------------------------------------------------------

// The dropdown hangs below a button, right-aligned with it.
constexpr Size kDropdownScreen{27, 14};

widgets::CalendarDropdown& dropdown(Stage& stage, std::vector<std::string> months = month_names()) {
    ui::View& body = stage.dialog(Rect{1, 0, 25, 5}, "Journal");
    auto& anchor = stage.place(body, Rect{11, 0, 10, 2}, std::make_unique<widgets::Button>("Date..."));
    widgets::CalendarDropdown& popup = *widgets::show_calendar_dropdown(anchor, stage.app(), stage.desktop());
    popup.set_month_labels(std::move(months));
    popup.show_month(kShownDay);
    popup.calendar().set_selected(kShownDay);
    return popup;
}

void add_calendar_dropdown(Catalog& catalog) {
    Element& e = catalog.element("CalendarDropdown", "include/cvision/widgets/common_components.hpp",
                                 Traits{.text = true});
    state(e, "normal", kDropdownScreen, [](Stage& s) { dropdown(s); });
    state(e, "today", kDropdownScreen, [](Stage& s) {
        dropdown(s).calendar().set_today(widgets::DateValue{2026, 8, 9});
    });
    state(e, "month-picker", kDropdownScreen, [](Stage& s) { s.focus(dropdown(s).month_picker()); });
    state(e, "year-field", kDropdownScreen, [](Stage& s) { s.focus(dropdown(s).year_field()); });
    // A year the Gregorian grid cannot draw, committed with Enter.
    state(e, "invalid-year", kDropdownScreen, [](Stage& s) {
        widgets::CalendarDropdown& popup = dropdown(s);
        s.focus(popup.year_field());
        popup.year_field().set_text("26");
        press(s, Key::Enter);
    });
    state(e, "wide", kDropdownScreen, [](Stage& s) {
        std::vector<std::string> months = month_names();
        months[7] = std::string(kWideText);
        dropdown(s, std::move(months));
    });
    // A month name longer than the picker may grow.
    state(e, "narrow", kDropdownScreen, [](Stage& s) {
        std::vector<std::string> months = month_names();
        months[7] = "Augustus Sextilis";
        dropdown(s, std::move(months));
    });
    // Every word from the host's table: the months, the weekday headings, and
    // the word over a refused year.
    state(e, "labels", kDropdownScreen, [](Stage& s) {
        widgets::CalendarDropdown& popup = dropdown(s);
        popup.set_labels(german_labels());
        popup.show_month(kShownDay);
        s.focus(popup.year_field());
        popup.year_field().set_text("26");
        press(s, Key::Enter);
    });
}

// --- DatePicker -------------------------------------------------------------

constexpr Size kFieldScreen{23, 6};
constexpr Rect kFieldDialog{1, 1, 20, 3};

widgets::DatePicker& date_picker(Stage& stage, Rect dialog = kFieldDialog) {
    ui::View& body = stage.dialog(dialog, "Due");
    auto& picker = stage.place(body, Rect{1, 0, 13, 1}, std::make_unique<widgets::DatePicker>());
    picker.set_value(kShownDay);
    return picker;
}

void add_date_picker(Catalog& catalog) {
    Element& e = catalog.element("DatePicker", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true});
    state(e, "normal", kFieldScreen, [](Stage& s) { date_picker(s); });
    state(e, "focused", kFieldScreen, [](Stage& s) { s.focus(date_picker(s)); });
    state(e, "month-field", kFieldScreen, [](Stage& s) {
        s.focus(date_picker(s));
        press(s, Key::Right);
    });
    state(e, "empty", kFieldScreen, [](Stage& s) { date_picker(s).set_value(std::nullopt); });
    state(e, "invalid", kFieldScreen, [](Stage& s) { date_picker(s).set_valid(false); });
    state(e, "disabled", kFieldScreen, [](Stage& s) { date_picker(s).set_enabled(false); });
    state(e, "dropdown", kFieldScreen, [](Stage& s) { date_picker(s).set_calendar_host(s.app(), s.desktop()); });
    state(e, "open", Size{27, 14}, [](Stage& s) {
        widgets::DatePicker& picker = date_picker(s, Rect{1, 0, 20, 3});
        picker.set_calendar_host(s.app(), s.desktop());
        picker.open_calendar();
    });
    // Day, month and year with dots, the day segment being edited.
    state(e, "dotted", kFieldScreen, [](Stage& s) {
        widgets::DatePicker& picker = date_picker(s);
        picker.set_format(dotted_format());
        s.focus(picker);
        press(s, Key::Left);
        press(s, Key::Left);
    });
    // The month by name, from the host's table.
    state(e, "month-name", kFieldScreen, [](Stage& s) {
        widgets::DatePicker& picker = date_picker(s);
        widgets::DateFormat format = dotted_format();
        format.separator = " ";
        format.month_style = widgets::MonthStyle::Name;
        format.zero_pad = false;
        picker.set_labels(german_labels());
        picker.set_format(format);
        picker.set_bounds(Rect{1, 0, 17, 1});
    });
    // Away from the keyboard, a caller's own text.
    state(e, "caller-format", kFieldScreen, [](Stage& s) {
        widgets::DateFormat format;
        format.format = [](widgets::DateValue date) { return "Wed " + std::to_string(date.day) + " Aug"; };
        date_picker(s).set_format(std::move(format));
    });
    state(e, "empty-labels", kFieldScreen, [](Stage& s) {
        widgets::DatePicker& picker = date_picker(s);
        picker.set_labels(german_labels());
        picker.set_value(std::nullopt);
        picker.set_bounds(Rect{1, 0, 17, 1});
    });
    // A date being typed, the caret after it.
    state(e, "entry", kFieldScreen, [](Stage& s) {
        s.focus(date_picker(s));
        type(s, "2026-08-2");
    });
    // A typed date that does not exist, refused and marked.
    state(e, "refused", kFieldScreen, [](Stage& s) {
        s.focus(date_picker(s));
        type(s, "2026-02-30");
        press(s, Key::Enter);
    });
}

// --- TimePicker -------------------------------------------------------------

widgets::TimePicker& time_picker(Stage& stage, widgets::TimeValue time = {9, 41, 7}) {
    ui::View& body = stage.dialog(kFieldDialog, "Alarm");
    auto& picker = stage.place(body, Rect{1, 0, 12, 1}, std::make_unique<widgets::TimePicker>());
    picker.set_value(time);
    return picker;
}

void add_time_picker(Catalog& catalog) {
    Element& e = catalog.element("TimePicker", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true});
    state(e, "normal", kFieldScreen, [](Stage& s) { time_picker(s); });
    state(e, "focused", kFieldScreen, [](Stage& s) { s.focus(time_picker(s)); });
    state(e, "no-seconds", kFieldScreen, [](Stage& s) { time_picker(s).set_show_seconds(false); });
    state(e, "twelve-hour", kFieldScreen, [](Stage& s) { time_picker(s, {21, 41, 7}).set_24_hour(false); });
    // The meridiem in the host's words.
    state(e, "meridiem-labels", kFieldScreen, [](Stage& s) {
        widgets::TimePicker& picker = time_picker(s, {21, 41, 7});
        picker.set_24_hour(false);
        picker.set_show_seconds(false);
        picker.set_meridiem_labels("vorm.", "nachm.");
    });
    state(e, "invalid", kFieldScreen, [](Stage& s) { time_picker(s).set_valid(false); });
    state(e, "disabled", kFieldScreen, [](Stage& s) { time_picker(s).set_enabled(false); });
}

// --- SpinBox ----------------------------------------------------------------

widgets::SpinBox& spin_box(Stage& stage, int value = 5, Rect bounds = Rect{1, 0, 10, 1}) {
    ui::View& body = stage.dialog(kFieldDialog, "Copies");
    auto& spin = stage.place(body, bounds, std::make_unique<widgets::SpinBox>());
    spin.set_range(0, 99999);
    spin.set_value(value);
    return spin;
}

void add_spin_box(Catalog& catalog) {
    Element& e = catalog.element("SpinBox", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true});
    for (auto presentation : {widgets::SpinBoxPresentation::Compact, widgets::SpinBoxPresentation::Separate, widgets::SpinBoxPresentation::Stacked}) {
        const std::string prefix = presentation == widgets::SpinBoxPresentation::Compact ? "compact" : presentation == widgets::SpinBoxPresentation::Separate ? "separate" : "stacked";
        for (auto variant : {"normal", "focused", "hovered", "disabled", "minimum", "maximum", "entry", "refused", "unicode", "tiny"}) {
            state(e, prefix + "-" + variant, Size{23, 6}, [presentation, variant](Stage& s) {
                const std::string_view name(variant);
                auto& body = s.dialog(Rect{1, 1, 21, 4}, "Copies");
                auto& spin = s.place(body, Rect{1, 0, 18, 2}, std::make_unique<widgets::SpinBox>());
                spin.set_presentation(presentation);
                spin.set_range(0, 100);
                spin.set_value(40);
                if (name == "focused") s.focus(spin);
                if (name == "disabled") spin.set_enabled(false);
                if (name == "minimum") spin.set_value(0);
                if (name == "maximum") spin.set_value(100);
                if (name == "tiny") spin.set_bounds(Rect{1, 0, 1, 1});
                if (name == "entry" || name == "refused" || name == "unicode") {
                    spin.set_editable(true);
                    s.focus(spin);
                    type(s, name == "unicode" ? std::string(kWideText) : name == "refused" ? "999" : "42");
                    if (name == "refused") press(s, Key::Enter);
                }
                if (name == "hovered") {
                    const auto control = spin.increment_bounds();
                    const auto abs = spin.absolute_bounds();
                    spin.on_mouse(MouseEvent{MouseAction::Move, MouseButton::None, Point{abs.x + control.x, abs.y + control.y}, std::nullopt});
                }
            });
        }
    }
    state(e, "normal", kFieldScreen, [](Stage& s) { spin_box(s); });
    state(e, "focused", kFieldScreen, [](Stage& s) { s.focus(spin_box(s)); });
    state(e, "disabled", kFieldScreen, [](Stage& s) { spin_box(s).set_enabled(false); });
    // A value wider than the field it is given.
    state(e, "clipped", kFieldScreen, [](Stage& s) { spin_box(s, 12345, Rect{1, 0, 6, 1}); });
    // A number being typed into an editable box, the caret after it.
    state(e, "entry", kFieldScreen, [](Stage& s) {
        widgets::SpinBox& spin = spin_box(s);
        spin.set_editable(true);
        s.focus(spin);
        type(s, "42");
    });
    // A number outside the range, refused and marked; left for the reader.
    state(e, "refused", kFieldScreen, [](Stage& s) {
        widgets::SpinBox& spin = spin_box(s);
        spin.set_range(0, 10);
        spin.set_editable(true);
        s.focus(spin);
        type(s, "42");
        press(s, Key::Enter);
    });
    // The same refusal once the focus has moved on: it stays marked.
    state(e, "refused-unfocused", kFieldScreen, [](Stage& s) {
        widgets::SpinBox& spin = spin_box(s);
        spin.set_range(0, 10);
        spin.set_editable(true);
        s.focus(spin);
        type(s, "x");
        press(s, Key::Enter);
        s.app().set_focus(nullptr);
    });
}

// --- Slider -----------------------------------------------------------------

// A slider with ticks is two rows high, and its dialog one row taller.
constexpr Size kTickScreen{23, 7};
constexpr Rect kTickDialog{1, 1, 20, 4};

widgets::Slider& slider(Stage& stage, int value = 40, Rect dialog = kFieldDialog, int rows = 1) {
    ui::View& body = stage.dialog(dialog, "Volume");
    auto& control = stage.place(body, Rect{1, 0, 16, rows}, std::make_unique<widgets::Slider>());
    control.set_range(0, 100);
    control.set_value(value);
    return control;
}

void add_slider(Catalog& catalog) {
    Element& e = catalog.element("Slider", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true});
    state(e, "normal", kFieldScreen, [](Stage& s) { slider(s); });
    state(e, "focused", kFieldScreen, [](Stage& s) { s.focus(slider(s)); });
    state(e, "disabled", kFieldScreen, [](Stage& s) { slider(s).set_enabled(false); });
    state(e, "minimum", kFieldScreen, [](Stage& s) { slider(s, 0); });
    state(e, "maximum", kFieldScreen, [](Stage& s) { slider(s, 100); });
    for (const auto presentation : {widgets::SliderPresentation::Line, widgets::SliderPresentation::Block, widgets::SliderPresentation::ProminentThumb}) {
        const std::string prefix = presentation == widgets::SliderPresentation::Line ? "line-value" : presentation == widgets::SliderPresentation::Block ? "block" : "prominent";
        for (const auto variant : {"normal", "focused", "disabled", "ticks", "minimum", "maximum", "tiny"}) {
            state(e, prefix + "-" + variant, kTickScreen, [presentation, variant](Stage& s) {
                auto& control = slider(s, 40, kTickDialog, 2);
                control.set_presentation(presentation);
                control.set_show_value(true);
                const std::string_view name(variant);
                if (name == "focused") s.focus(control);
                if (name == "disabled") control.set_enabled(false);
                if (name == "ticks") control.set_ticks({{0, "Low"}, {100, "High"}, {50, "Mid"}});
                if (name == "minimum") control.set_value(0);
                if (name == "maximum") control.set_value(100);
                if (name == "tiny") control.set_bounds(Rect{0, 0, 1, 1});
            });
        }
    }
    // Labelled ticks under the track.
    state(e, "ticks", kTickScreen, [](Stage& s) {
        slider(s, 40, kTickDialog, 2).set_ticks({{0, "0"}, {100, "100"}, {50, "50"}});
    });
    // More labels than room: the later ones that would collide are left out,
    // their marks kept.
    state(e, "ticks-crowded", kTickScreen, [](Stage& s) {
        widgets::Slider& control = slider(s, 70, kTickDialog, 2);
        control.set_ticks({{0, "Low"}, {100, "High"}, {25, "Quarter"}, {50, "Mid"}, {75, "3/4"}});
        s.focus(control);
    });
}

// --- SearchBox --------------------------------------------------------------

constexpr Size kSearchScreen{29, 6};

widgets::SearchBox& search_box(Stage& stage, std::string query = {}, Rect bounds = Rect{0, 0, 24, 1}) {
    ui::View& body = stage.dialog(Rect{1, 1, 26, 2 + bounds.height}, "Find");
    auto& box = stage.place(body, bounds, std::make_unique<widgets::SearchBox>());
    box.set_query(std::move(query));
    return box;
}

void add_search_box(Catalog& catalog) {
    Element& e = catalog.element("SearchBox", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    for (const auto presentation : {widgets::InputPresentation::Padded, widgets::InputPresentation::Underlined}) {
        const std::string prefix = presentation == widgets::InputPresentation::Padded ? "padded-" : "underlined-";
        for (const std::string variant : {"normal", "focused", "query", "disabled", "wide", "narrow", "status"}) {
            state(e, prefix + variant, kSearchScreen, [presentation, variant](Stage& s) {
                auto& box = search_box(s, variant == "normal" ? "" : variant == "wide" ? std::string(kWideText) : "invoice",
                                       Rect{0, 0, variant == "narrow" ? 16 : 24, widgets::input_presentation_height(presentation)});
                box.set_presentation(presentation);
                if (variant == "focused" || variant == "narrow") s.focus(box.field());
                if (variant == "disabled") box.set_enabled(false);
                if (variant == "status") box.set_status("3 of 12");
            });
        }
    }
    state(e, "normal", kSearchScreen, [](Stage& s) { search_box(s); });
    state(e, "focused", kSearchScreen, [](Stage& s) { s.focus(search_box(s).field()); });
    state(e, "query", kSearchScreen, [](Stage& s) { search_box(s, "invoice"); });
    state(e, "query-focused", kSearchScreen, [](Stage& s) { s.focus(search_box(s, "invoice").field()); });
    state(e, "disabled", kSearchScreen, [](Stage& s) { search_box(s).set_enabled(false); });
    state(e, "wide", kSearchScreen, [](Stage& s) { search_box(s, std::string(kWideText)); });
    state(e, "narrow", kSearchScreen, [](Stage& s) {
        s.focus(search_box(s, "quarterly invoices", Rect{0, 0, 16, 1}).field());
    });
    // The host's count of what the query found, right-aligned before the
    // clear control; squeezed, it is elided before the field gives way.
    state(e, "status", kSearchScreen, [](Stage& s) { search_box(s, "invoice").set_status("3 of 12"); });
    state(e, "status-focused", kSearchScreen, [](Stage& s) {
        widgets::SearchBox& box = search_box(s, "invoice");
        box.set_status("3 of 12");
        s.focus(box.field());
    });
    state(e, "status-narrow", kSearchScreen, [](Stage& s) {
        search_box(s, "invoice", Rect{0, 0, 20, 1}).set_status("3 of 12");
    });
}

// --- ToolBar and CommandPalette ---------------------------------------------

// Declares commands the way an application does; the toolbar and the palette
// read their titles, chords and availability back out of the registry.
std::vector<ui::CommandId> declare_commands(Stage& stage, std::string_view find_title) {
    ui::CommandRegistry& registry = stage.app().commands();
    const auto declare = [&registry](std::string key, std::string title, std::string chord) {
        return registry.declare(ui::CommandDescriptor{.key = std::move(key),
                                                      .title = std::move(title),
                                                      .category = "File",
                                                      .chord = std::move(chord),
                                                      .visibility = ui::CommandVisibility::Palette,
                                                      .handler = [] {}});
    };
    return {declare("doc.open", "&Open", "Ctrl+O"),
            declare("doc.save", "&Save", "Ctrl+S"),
            declare("doc.find", std::string(find_title), "Ctrl+F"),
            declare("doc.replace", "&Replace all", "Ctrl+R"),
            declare("doc.print", "&Print", ""),
            declare("win.tile", "&Tile", "")};
}

constexpr std::string_view kFindTitle = "&Find";

// The toolbar and the breadcrumb trail each stand in one dialog row.
constexpr Size kBarScreen{37, 6};
constexpr Rect kBarDialog{1, 1, 34, 3};

widgets::ToolBar& tool_bar(Stage& stage, std::string_view find_title = kFindTitle, int width = 32,
                           Size screen_dialog = Size{34, 3}) {
    const std::vector<ui::CommandId> commands = declare_commands(stage, find_title);
    ui::View& body = stage.dialog(Rect{1, 1, screen_dialog.width, screen_dialog.height}, "Report");
    auto& bar = stage.place(body, Rect{0, 0, width, 1}, std::make_unique<widgets::ToolBar>());
    bar.set_items({widgets::CommandPresentation{commands[0]}, widgets::CommandPresentation{commands[1]},
                   widgets::CommandPresentation{commands[2]}});
    return bar;
}

// A desktop's own chrome with a tool bar docked beside it: the menu bar and
// the tool bar under it at the top, or the status line and the tool bar
// above it at the bottom.
constexpr Size kDockScreen{36, 10};

widgets::ToolBar& docked_tool_bar(Stage& stage, widgets::DockEdge edge) {
    const std::vector<ui::CommandId> commands = declare_commands(stage, kFindTitle);
    stage.desktop().dock_top(std::make_unique<widgets::MenuBar>(std::vector<widgets::MenuBarItem>{
        widgets::MenuBarItem{"&File", {widgets::MenuItem::command(commands[0])}},
        widgets::MenuBarItem{"&Edit", {widgets::MenuItem::command(commands[3])}}}));
    stage.desktop()
        .dock_bottom(std::make_unique<widgets::StatusLine>())
        ->set_items({widgets::StatusLineItem{widgets::CommandPresentation{commands[1]}}});
    auto& bar = *stage.desktop().dock(std::make_unique<widgets::ToolBar>(), edge);
    bar.set_items({widgets::CommandPresentation{commands[0]}, widgets::CommandPresentation{commands[1]},
                   widgets::CommandPresentation{commands[2]}, widgets::CommandPresentation{commands[3]},
                   widgets::CommandPresentation{commands[4]}});
    stage.document(Rect{2, 3, 30, 4}, "Notes");
    return bar;
}

void add_tool_bar(Catalog& catalog) {
    Element& e = catalog.element("ToolBar", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .text = true});
    state(e, "normal", kBarScreen, [](Stage& s) { tool_bar(s); });
    // The keyboard walk stands on the first button.
    state(e, "focused", kBarScreen, [](Stage& s) { s.focus(tool_bar(s)); });
    // The command says it cannot run now, and the toolbar shows it.
    state(e, "unavailable", kBarScreen, [](Stage& s) {
        const ui::CommandId save = tool_bar(s).items()[1].command;
        s.app().commands().set_enabled_predicate(save, [] { return false; });
    });
    // A toggle command carries its state in the mark column, as a menu row
    // does: Find is on.
    state(e, "checked", kBarScreen, [](Stage& s) {
        const ui::CommandId find = tool_bar(s).items()[2].command;
        s.app().commands().set_checked_predicate(find, [] { return true; });
    });
    state(e, "chords", kBarScreen, [](Stage& s) { tool_bar(s).set_show_chords(true); });
    state(e, "disabled", kBarScreen, [](Stage& s) { tool_bar(s).set_enabled(false); });
    state(e, "wide", kBarScreen, [](Stage& s) { tool_bar(s, kWideText); });
    // Too narrow for every button: the rest go behind the overflow control.
    state(e, "narrow", kBarScreen, [](Stage& s) { tool_bar(s, kFindTitle, 12); });
    state(e, "overflow-open", Size{37, 9}, [](Stage& s) {
        widgets::ToolBar& bar = tool_bar(s, kFindTitle, 12, Size{34, 6});
        s.focus(bar);
        press(s, Key::End);
        press(s, Key::Enter);
    });
    state(e, "docked-top", kDockScreen, [](Stage& s) { docked_tool_bar(s, widgets::DockEdge::Top); });
    state(e, "docked-bottom", kDockScreen, [](Stage& s) { docked_tool_bar(s, widgets::DockEdge::Bottom); });
    // Docked at the bottom, the overflow menu opens upward.
    state(e, "docked-bottom-overflow", kDockScreen, [](Stage& s) {
        widgets::ToolBar& bar = docked_tool_bar(s, widgets::DockEdge::Bottom);
        bar.activate();
        press(s, Key::End);
        press(s, Key::Enter);
    });
    for (const auto& [prefix, presentation] : {
             std::pair{std::string("padded-"), widgets::ToolBarPresentation::Padded},
             std::pair{std::string("framed-"), widgets::ToolBarPresentation::Framed}}) {
        const auto specimen = [presentation](Stage& s, int width = 32) -> widgets::ToolBar& {
            auto& bar = tool_bar(s, kFindTitle, width, Size{34, 8});
            bar.set_presentation(presentation);
            bar.set_bounds(Rect{0, 0, width, presentation == widgets::ToolBarPresentation::Framed ? 3 : 1});
            return bar;
        };
        state(e, prefix + "normal", Size{37, 9}, [specimen](Stage& s) { specimen(s); });
        state(e, prefix + "focused", Size{37, 9}, [specimen](Stage& s) { s.focus(specimen(s)); });
        state(e, prefix + "hovered", Size{37, 9}, [specimen](Stage& s) {
            auto& bar = specimen(s); const Rect abs = bar.absolute_bounds();
            bar.on_mouse(MouseEvent{MouseAction::Move, MouseButton::None, Point{abs.x + 1, abs.y}, std::nullopt, Modifier::None});
        });
        state(e, prefix + "pressed", Size{37, 9}, [specimen](Stage& s) {
            auto& bar = specimen(s); s.focus(bar);
            KeyEvent event{KeyChord{Key::Enter, Modifier::None, ""}}; event.reports_release = true;
            bar.on_key(event);
        });
        state(e, prefix + "checked", Size{37, 9}, [specimen](Stage& s) {
            auto& bar = specimen(s); s.app().commands().set_checked_predicate(bar.items()[0].command, [] { return true; });
        });
        state(e, prefix + "disabled", Size{37, 9}, [specimen](Stage& s) { specimen(s).set_enabled(false); });
        state(e, prefix + "narrow", Size{37, 9}, [specimen](Stage& s) { specimen(s, 13); });
        state(e, prefix + "groups", Size{37, 9}, [specimen](Stage& s) {
            auto& bar = specimen(s); const auto items = bar.items();
            bar.set_groups({{items[0]}, {items[1], items[2]}});
        });
        state(e, prefix + "overflow-open", Size{37, 12}, [specimen](Stage& s) {
            auto& bar = specimen(s, 13); s.focus(bar); press(s, Key::End); press(s, Key::Enter);
        });
    }
    state(e, "hovered", kBarScreen, [](Stage& s) {
        auto& bar = tool_bar(s); const Rect abs = bar.absolute_bounds();
        bar.on_mouse(MouseEvent{MouseAction::Move, MouseButton::None, Point{abs.x + 1, abs.y}, std::nullopt, Modifier::None});
    });
    state(e, "pressed", kBarScreen, [](Stage& s) {
        auto& bar = tool_bar(s); s.focus(bar);
        KeyEvent event{KeyChord{Key::Enter, Modifier::None, ""}}; event.reports_release = true;
        bar.on_key(event);
    });
    state(e, "groups", kBarScreen, [](Stage& s) {
        auto& bar = tool_bar(s); const auto items = bar.items();
        bar.set_groups({{items[0]}, {items[1], items[2]}});
    });
}

constexpr Size kPaletteScreen{37, 14};
constexpr Rect kPalette{0, 0, 32, 10};

widgets::CommandPalette& palette(Stage& stage, Rect bounds = kPalette, std::string_view find_title = kFindTitle) {
    declare_commands(stage, find_title);
    ui::View& body = stage.dialog(Rect{1, 1, 34, 12}, "Commands");
    return stage.place(body, bounds, std::make_unique<widgets::CommandPalette>());
}

// The palette as the standard command puts it up: a framed popup over the
// document the reader was working in.
constexpr Size kPalettePopupScreen{72, 20};

void open_palette_popup(Stage& stage) {
    declare_commands(stage, kFindTitle);
    stage.document(Rect{2, 4, 50, 14}, "Notes");
    stage.app().dispatch(KeyEvent{KeyChord{Key::Char, Modifier::Ctrl | Modifier::Shift, "p"}});
}

void add_command_palette(Catalog& catalog) {
    Element& e = catalog.element("CommandPalette", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .text = true});
    state(e, "normal", kPaletteScreen, [](Stage& s) { palette(s); });
    state(e, "focused", kPaletteScreen, [](Stage& s) { s.focus(palette(s)); });
    state(e, "filtered", kPaletteScreen, [](Stage& s) {
        s.focus(palette(s));
        type(s, "re");
    });
    state(e, "highlighted", kPaletteScreen, [](Stage& s) {
        s.focus(palette(s));
        press(s, Key::Down);
        press(s, Key::Down);
    });
    // More results than rows: the list scrolls with the highlight, and a
    // scrollbar says where it is.
    state(e, "overflow", kPaletteScreen, [](Stage& s) {
        s.focus(palette(s, Rect{0, 0, 32, 7}));
        for (int i = 0; i < 4; ++i) press(s, Key::Down);
    });
    state(e, "no-match", kPaletteScreen, [](Stage& s) {
        s.focus(palette(s));
        type(s, "zzz");
    });
    // A command that applies but cannot run now is listed greyed, and the
    // highlight passes over it.
    state(e, "disabled-command", kPaletteScreen, [](Stage& s) {
        s.focus(palette(s));
        s.app().commands().set_enabled_predicate(*s.app().commands().id_for("doc.save"), [] { return false; });
        press(s, Key::Down);
    });
    state(e, "popup", kPalettePopupScreen, [](Stage& s) { open_palette_popup(s); });
    state(e, "popup-filtered", kPalettePopupScreen, [](Stage& s) {
        open_palette_popup(s);
        type(s, "re");
    });
    state(e, "wide", kPaletteScreen, [](Stage& s) { palette(s, kPalette, kWideText); });
    state(e, "narrow", kPaletteScreen, [](Stage& s) { palette(s, Rect{0, 0, 14, 10}); });
}

// --- BreadcrumbBar ----------------------------------------------------------

std::vector<std::string> trail() { return {"home", "klukas", "reports", "2026"}; }
// A path too long for the bar, so its middle is elided.
std::vector<std::string> deep_trail() { return {"home", "klukas", "projects", "ckvision", "docs", "2026"}; }

widgets::BreadcrumbBar& breadcrumbs(Stage& stage, std::vector<std::string> segments = trail(), int width = 32,
                                    Rect dialog = kBarDialog) {
    ui::View& body = stage.dialog(dialog, "Browse");
    auto& bar = stage.place(body, Rect{0, 0, width, 1}, std::make_unique<widgets::BreadcrumbBar>());
    bar.set_segments(std::move(segments));
    bar.on_activate = [](std::size_t) {};
    return bar;
}

void add_breadcrumb_bar(Catalog& catalog) {
    Element& e = catalog.element("BreadcrumbBar", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    for (auto presentation : {widgets::BreadcrumbPresentation::Padded, widgets::BreadcrumbPresentation::Connected}) {
        const std::string prefix = presentation == widgets::BreadcrumbPresentation::Padded ? "padded" : "connected";
        for (auto variant : {"normal", "focused", "current-focused", "disabled", "overflow", "tiny", "unicode"}) {
            state(e, prefix + "-" + variant, kBarScreen, [presentation, variant](Stage& s) {
                const std::string_view name(variant);
                auto& b = breadcrumbs(s, name == "unicode" ? std::vector<std::string>{"home", std::string(kWideText), "2026"} : trail(), name == "overflow" ? 18 : 32);
                b.set_presentation(presentation);
                if (name == "disabled") b.set_enabled(false);
                if (name == "tiny") b.set_bounds(Rect{0, 0, 1, 1});
                if (name == "focused" || name == "current-focused") s.focus(b);
                if (name == "current-focused") for (int i = 0; i < 8; ++i) b.on_key(KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
            });
        }
    }
    state(e, "normal", kBarScreen, [](Stage& s) { breadcrumbs(s); });
    state(e, "focused", kBarScreen, [](Stage& s) { s.focus(breadcrumbs(s)); });
    state(e, "segment", kBarScreen, [](Stage& s) {
        s.focus(breadcrumbs(s));
        press(s, Key::Right);
        press(s, Key::Right);
    });
    state(e, "separator", kBarScreen, [](Stage& s) { breadcrumbs(s).set_separator(" › "); });
    state(e, "disabled", kBarScreen, [](Stage& s) { breadcrumbs(s).set_enabled(false); });
    state(e, "wide", kBarScreen, [](Stage& s) { breadcrumbs(s, {"home", std::string(kWideText), "2026"}); });
    state(e, "narrow", kBarScreen, [](Stage& s) { breadcrumbs(s, trail(), 12); });
    // The same deep path at three widths: the first and last segments stay,
    // the ellipsis stands for fewer segments the more room there is, and at
    // the narrowest the two kept segments are elided themselves.
    state(e, "elided", kBarScreen, [](Stage& s) { breadcrumbs(s, deep_trail(), 30); });
    state(e, "elided-narrow", kBarScreen, [](Stage& s) { breadcrumbs(s, deep_trail(), 16); });
    state(e, "elided-tight", kBarScreen, [](Stage& s) { breadcrumbs(s, deep_trail(), 9); });
    state(e, "ellipsis-focused", kBarScreen, [](Stage& s) {
        s.focus(breadcrumbs(s, deep_trail(), 30));
        press(s, Key::Right);
    });
    // Enter on the ellipsis lists the segments it stands for.
    state(e, "ellipsis-open", Size{37, 10}, [](Stage& s) {
        s.focus(breadcrumbs(s, deep_trail(), 30, Rect{1, 1, 34, 7}));
        press(s, Key::Right);
        press(s, Key::Enter);
    });
}

// --- PropertyInspector ------------------------------------------------------

// One row of every kind that draws differently, so the column alignment and
// each kind's own face are seen together.
std::vector<widgets::PropertyItem> release_properties(std::string title) {
    widgets::PropertyItem encoding{"Encoding", "UTF-8", true, widgets::PropertyKind::Choice};
    encoding.choices = {"UTF-8", "Latin-1", "UTF-16"};
    widgets::PropertyItem width{"Width", "80", true, widgets::PropertyKind::Integer};
    width.minimum = 20;
    width.maximum = 200;
    return {
        widgets::PropertyItem{"Title", std::move(title), true},
        std::move(encoding),
        widgets::PropertyItem{"Read only", "false", true, widgets::PropertyKind::Bool},
        std::move(width),
        widgets::PropertyItem{"Due", "2026-09-30", true, widgets::PropertyKind::Date},
        widgets::PropertyItem{"Lines", "1 284", false},
    };
}

widgets::PropertyInspector& inspector(Stage& stage, std::string title = "Release notes", int width = 30) {
    ui::View& body = stage.dialog(Rect{1, 1, 32, 9}, "Details");
    auto& view = stage.place(body, Rect{0, 0, width, 7}, std::make_unique<widgets::PropertyInspector>());
    view.set_items(release_properties(std::move(title)));
    return view;
}

// The inspector holding the keyboard with its cursor on row `row`.
widgets::PropertyInspector& inspector_on(Stage& stage, int row) {
    widgets::PropertyInspector& view = inspector(stage);
    stage.focus(view);
    for (int i = 0; i < row; ++i) press(stage, Key::Down);
    return view;
}

void add_property_inspector(Catalog& catalog) {
    Element& e = catalog.element("PropertyInspector", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    constexpr Size kScreen{35, 11};
    for (auto presentation : {widgets::PropertyPresentation::Plain, widgets::PropertyPresentation::Divided,
                              widgets::PropertyPresentation::Sectioned}) {
        const std::string prefix = presentation == widgets::PropertyPresentation::Plain ? "banded-" :
                                   presentation == widgets::PropertyPresentation::Divided ? "divided-" : "sectioned-";
        for (const auto name : {"normal", "focused", "disabled", "editing", "refused", "choice", "date", "unicode", "narrow", "tiny"}) {
            state(e, prefix + name, Size{40, 18}, [presentation, specimen = std::string(name)](Stage& s) {
                ui::View& body = s.dialog(Rect{1, 1, 37, 16}, "Details");
                auto& view = s.place(body, Rect{0, 0, specimen == "tiny" ? 1 : specimen == "narrow" ? 12 : 35, specimen == "tiny" ? 1 : 14},
                                     std::make_unique<widgets::PropertyInspector>());
                auto items = release_properties(specimen == "unicode" ? std::string(kWideText) : "Release notes");
                for (std::size_t i = 0; i < items.size(); ++i) items[i].group = i < 3 ? "Document" : "Layout";
                view.set_items(std::move(items));
                view.set_presentation(presentation);
                view.set_banded_rows(true);
                if (specimen == "disabled") view.set_enabled(false);
                if (specimen == "focused" || specimen == "editing" || specimen == "refused" || specimen == "choice" || specimen == "date") {
                    s.focus(view);
                    const int row = specimen == "refused" ? 3 : specimen == "choice" ? 1 : specimen == "date" ? 4 : 0;
                    for (int i = 0; i < row; ++i) press(s, Key::Down);
                    if (specimen != "focused") press(s, Key::Enter);
                    if (specimen == "refused") { type(s, "7"); press(s, Key::Enter); }
                }
            });
        }
    }
    state(e, "normal", kScreen, [](Stage& s) { inspector(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(inspector(s)); });
    state(e, "cursor", kScreen, [](Stage& s) { inspector_on(s, 3); });
    // Enter opens a Text row's InputLine over the value, the text selected;
    // End keeps it, and what is typed lands after it.
    state(e, "editing", kScreen, [](Stage& s) {
        inspector_on(s, 0);
        press(s, Key::Enter);
        press(s, Key::End);
        type(s, " v2");
    });
    // A Choice row's list drops as its edit begins.
    state(e, "choice", kScreen, [](Stage& s) {
        inspector_on(s, 1);
        press(s, Key::Enter);
    });
    // A Bool row is a check box, toggled where it stands.
    state(e, "toggled", kScreen, [](Stage& s) {
        inspector_on(s, 2);
        press(s, Key::Enter);
    });
    // A Date row is edited with the DatePicker.
    state(e, "date", kScreen, [](Stage& s) {
        inspector_on(s, 4);
        press(s, Key::Enter);
    });
    // A refused value: the editor marked invalid, the reason under it, and
    // the rows after it moved down.
    state(e, "refused", kScreen, [](Stage& s) {
        inspector_on(s, 3);
        press(s, Key::Enter);
        type(s, "7");
        press(s, Key::Enter);
    });
    state(e, "disabled", kScreen, [](Stage& s) { inspector(s).set_enabled(false); });
    state(e, "wide", kScreen, [](Stage& s) { inspector(s, std::string(kWideText)); });
    state(e, "narrow", kScreen, [](Stage& s) { inspector(s, "Release notes", 14); });
}

// --- Wizard -----------------------------------------------------------------

widgets::Wizard& wizard(Stage& stage, std::string first_title = "Choose a name", int width = 26,
                        bool first_complete = true) {
    ui::View& body = stage.dialog(Rect{1, 1, 28, 7}, "Project");
    auto& view = stage.place(body, Rect{0, 0, width, 5}, std::make_unique<widgets::Wizard>());
    view.set_pages({
        widgets::WizardPage{std::move(first_title), [first_complete] { return first_complete; }},
        widgets::WizardPage{"Pick a template", [] { return true; }},
        widgets::WizardPage{"Confirm", [] { return true; }},
    });
    return view;
}

void add_wizard(Catalog& catalog) {
    Element& e = catalog.element("Wizard", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .text = true});
    constexpr Size kScreen{31, 9};
    for (auto presentation : {widgets::WizardPresentationStyle::Compact, widgets::WizardPresentationStyle::Bands,
                              widgets::WizardPresentationStyle::StepRail}) {
        const std::string prefix = presentation == widgets::WizardPresentationStyle::Compact ? "compact-" :
                                   presentation == widgets::WizardPresentationStyle::Bands ? "bands-" : "rail-";
        for (const auto name : {"normal", "focused", "button-focused", "hovered", "pressed", "blocked", "middle", "last", "disabled", "unicode", "narrow", "tiny", "content"}) {
            state(e, prefix + name, Size{56, 18}, [presentation, specimen = std::string(name)](Stage& s) {
                ui::View& body = s.dialog(Rect{1, 1, 54, 16}, "Project");
                auto& view = s.place(body, Rect{0, 0, specimen == "tiny" ? 1 : specimen == "narrow" ? 18 : 52,
                                              specimen == "tiny" ? 1 : 14}, std::make_unique<widgets::Wizard>());
                view.set_pages({{specimen == "unicode" ? std::string(kWideText) : "Choose a name", [available = specimen != "blocked"] { return available; }},
                                {"Pick a template", {}}, {"Confirm", {}}});
                view.set_presentation(presentation);
                view.on_complete = [](widgets::WizardOutcome) {};
                view.refresh_navigation();
                if (specimen == "middle" || specimen == "last") view.next();
                if (specimen == "last") view.next();
                if (specimen == "disabled") view.set_enabled(false);
                if (specimen == "focused") s.focus(view);
                if (specimen == "button-focused") s.focus(view.forward_button());
                if (specimen == "hovered") {
                    const Rect absolute = view.forward_button().absolute_bounds();
                    s.app().dispatch(MouseEvent{MouseAction::Move, MouseButton::None, Point{absolute.x, absolute.y}, std::nullopt});
                }
                if (specimen == "pressed") {
                    const Rect absolute = view.forward_button().absolute_bounds();
                    s.app().dispatch(MouseEvent{MouseAction::Down, MouseButton::Left, Point{absolute.x, absolute.y}, std::nullopt});
                }
                if (specimen == "content") {
                    auto input = std::make_unique<widgets::InputLine>();
                    input->set_text("Project name");
                    auto* field = view.set_page_content(0, std::move(input));
                    s.focus(*field);
                }
            });
        }
    }
    state(e, "normal", kScreen, [](Stage& s) { wizard(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(wizard(s)); });
    state(e, "middle", kScreen, [](Stage& s) { wizard(s).next(); });
    state(e, "last", kScreen, [](Stage& s) {
        widgets::Wizard& view = wizard(s);
        view.next();
        view.next();
    });
    // The first page is not complete yet, so it cannot be left forward.
    state(e, "blocked", kScreen, [](Stage& s) { wizard(s, "Choose a name", 26, false); });
    state(e, "wide", kScreen, [](Stage& s) { wizard(s, std::string(kWideText)); });
    state(e, "narrow", kScreen, [](Stage& s) { wizard(s, "Choose a name", 9); });
    // The current page's own content between the title and the navigation.
    state(e, "page-content", kScreen, [](Stage& s) {
        widgets::Wizard& view = wizard(s);
        auto name = std::make_unique<widgets::InputLine>();
        name->set_text("Ledger");
        view.set_page_content(0, std::move(name));
        s.focus(*view.page_content(0));
    });
    // Every word in the host's language.
    state(e, "labels", kScreen, [](Stage& s) {
        widgets::Wizard& view = wizard(s, "Anmeldung");
        widgets::WizardLabels labels;
        labels.back = "< Zurück";
        labels.next = "Weiter >";
        labels.finish = "Fertig";
        labels.cancel = "Abbruch";
        labels.step = [](std::size_t current, std::size_t count) {
            return std::to_string(current) + "/" + std::to_string(count);
        };
        view.set_labels(std::move(labels));
        view.next();
    });
    // Presented in a dialog window of its own, as present_modal_wizard does.
    state(e, "presented", Size{40, 12}, [](Stage& s) {
        auto view = std::make_unique<widgets::Wizard>();
        view->set_pages({widgets::WizardPage{"Choose a name", {}}, widgets::WizardPage{"Confirm", {}}});
        auto name = std::make_unique<widgets::InputLine>();
        name->set_text("Ledger");
        view->set_page_content(0, std::move(name));
        static_cast<void>(widgets::present_modal_wizard(std::move(view), "Set up", s.app(), s.desktop(), s.roles()));
    });
}

}  // namespace

void add_component_specimens(Catalog& catalog) {
    add_clock_view(catalog);
    add_big_clock_view(catalog);
    add_calendar_view(catalog);
    add_calendar_dropdown(catalog);
    add_date_picker(catalog);
    add_time_picker(catalog);
    add_spin_box(catalog);
    add_slider(catalog);
    add_search_box(catalog);
    add_tool_bar(catalog);
    add_command_palette(catalog);
    add_breadcrumb_bar(catalog);
    add_property_inspector(catalog);
    add_wizard(catalog);
}

}  // namespace ckv::docgen::appearance
