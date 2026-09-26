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
#include "cvision/widgets/menu.hpp"

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
    state(e, "normal", kFieldScreen, [](Stage& s) { spin_box(s); });
    state(e, "focused", kFieldScreen, [](Stage& s) { s.focus(spin_box(s)); });
    state(e, "disabled", kFieldScreen, [](Stage& s) { spin_box(s).set_enabled(false); });
    // A value wider than the field it is given.
    state(e, "clipped", kFieldScreen, [](Stage& s) { spin_box(s, 12345, Rect{1, 0, 6, 1}); });
}

// --- Slider -----------------------------------------------------------------

widgets::Slider& slider(Stage& stage, int value = 40) {
    ui::View& body = stage.dialog(kFieldDialog, "Volume");
    auto& control = stage.place(body, Rect{1, 0, 16, 1}, std::make_unique<widgets::Slider>());
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
}

// --- SearchBox --------------------------------------------------------------

constexpr Size kSearchScreen{29, 6};

widgets::SearchBox& search_box(Stage& stage, std::string query = {}, Rect bounds = Rect{0, 0, 24, 1}) {
    ui::View& body = stage.dialog(Rect{1, 1, 26, 3}, "Find");
    auto& box = stage.place(body, bounds, std::make_unique<widgets::SearchBox>());
    box.set_query(std::move(query));
    return box;
}

void add_search_box(Catalog& catalog) {
    Element& e = catalog.element("SearchBox", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    state(e, "normal", kSearchScreen, [](Stage& s) { search_box(s); });
    state(e, "focused", kSearchScreen, [](Stage& s) { s.focus(search_box(s)); });
    state(e, "query", kSearchScreen, [](Stage& s) { search_box(s, "invoice"); });
    state(e, "query-focused", kSearchScreen, [](Stage& s) { s.focus(search_box(s, "invoice")); });
    state(e, "disabled", kSearchScreen, [](Stage& s) { search_box(s).set_enabled(false); });
    state(e, "wide", kSearchScreen, [](Stage& s) { search_box(s, std::string(kWideText)); });
    state(e, "narrow", kSearchScreen, [](Stage& s) {
        s.focus(search_box(s, "quarterly invoices", Rect{0, 0, 16, 1}));
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

widgets::ToolBar& tool_bar(Stage& stage, std::string_view find_title = kFindTitle, int width = 32) {
    const std::vector<ui::CommandId> commands = declare_commands(stage, find_title);
    ui::View& body = stage.dialog(kBarDialog, "Report");
    auto& bar = stage.place(body, Rect{0, 0, width, 1}, std::make_unique<widgets::ToolBar>());
    bar.set_commands({commands[0], commands[1], commands[2]});
    return bar;
}

void add_tool_bar(Catalog& catalog) {
    Element& e = catalog.element("ToolBar", "include/cvision/widgets/common_components.hpp", Traits{.text = true});
    state(e, "normal", kBarScreen, [](Stage& s) { tool_bar(s); });
    // The command says it cannot run now, and the toolbar shows it.
    state(e, "unavailable", kBarScreen, [](Stage& s) {
        const ui::CommandId save = tool_bar(s).commands()[1];
        s.app().commands().set_enabled_predicate(save, [] { return false; });
    });
    state(e, "wide", kBarScreen, [](Stage& s) { tool_bar(s, kWideText); });
    state(e, "narrow", kBarScreen, [](Stage& s) { tool_bar(s, kFindTitle, 12); });
}

constexpr Size kPaletteScreen{37, 14};
constexpr Rect kPalette{0, 0, 32, 10};

widgets::CommandPalette& palette(Stage& stage, Rect bounds = kPalette, std::string_view find_title = kFindTitle) {
    declare_commands(stage, find_title);
    ui::View& body = stage.dialog(Rect{1, 1, 34, 12}, "Commands");
    return stage.place(body, bounds, std::make_unique<widgets::CommandPalette>());
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
    state(e, "wide", kPaletteScreen, [](Stage& s) { palette(s, kPalette, kWideText); });
    state(e, "narrow", kPaletteScreen, [](Stage& s) { palette(s, Rect{0, 0, 14, 10}); });
}

// --- BreadcrumbBar ----------------------------------------------------------

std::vector<std::string> trail() { return {"home", "klukas", "reports", "2026"}; }

widgets::BreadcrumbBar& breadcrumbs(Stage& stage, std::vector<std::string> segments = trail(), int width = 32) {
    ui::View& body = stage.dialog(kBarDialog, "Browse");
    auto& bar = stage.place(body, Rect{0, 0, width, 1}, std::make_unique<widgets::BreadcrumbBar>());
    // BreadcrumbBar declares no focus policy of its own; a host that offers
    // its keyboard walk puts it in the Tab order.
    bar.set_focus_policy(ui::FocusPolicy::TabStop);
    bar.set_segments(std::move(segments));
    return bar;
}

void add_breadcrumb_bar(Catalog& catalog) {
    Element& e = catalog.element("BreadcrumbBar", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
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
}

// --- PropertyInspector ------------------------------------------------------

widgets::PropertyInspector& inspector(Stage& stage, std::string title = "Release notes", int width = 24) {
    ui::View& body = stage.dialog(Rect{1, 1, 26, 6}, "Details");
    auto& view = stage.place(body, Rect{0, 0, width, 4}, std::make_unique<widgets::PropertyInspector>());
    view.set_items({
        widgets::PropertyItem{"Title", std::move(title), true},
        widgets::PropertyItem{"Encoding", "UTF-8", false},
        widgets::PropertyItem{"Read only", "no", true},
        widgets::PropertyItem{"Lines", "1 284", false},
    });
    return view;
}

void add_property_inspector(Catalog& catalog) {
    Element& e = catalog.element("PropertyInspector", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .control = true, .text = true});
    constexpr Size kScreen{29, 8};
    state(e, "normal", kScreen, [](Stage& s) { inspector(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(inspector(s)); });
    state(e, "cursor", kScreen, [](Stage& s) {
        s.focus(inspector(s));
        press(s, Key::Down);
        press(s, Key::Down);
    });
    // Enter opens the value for editing, and committed text lands in it.
    state(e, "editing", kScreen, [](Stage& s) {
        s.focus(inspector(s));
        press(s, Key::Enter);
        s.app().dispatch(TextEvent{" v2", false});
    });
    state(e, "disabled", kScreen, [](Stage& s) { inspector(s).set_enabled(false); });
    state(e, "wide", kScreen, [](Stage& s) { inspector(s, std::string(kWideText)); });
    state(e, "narrow", kScreen, [](Stage& s) { inspector(s, "Release notes", 12); });
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
