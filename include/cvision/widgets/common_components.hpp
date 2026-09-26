// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/menu.hpp"

namespace ckv::widgets {

// A calendar date as three plain numbers, with no time zone and no locale. The
// struct itself enforces nothing: is_valid_date says whether it names a real
// day, and each widget that takes one states what it clamps.
struct DateValue {
    // The Gregorian year, the month 1..12 and the day of the month 1..31.
    // The default is 2026-01-01, a fixed value rather than today, because
    // ckVision reads no wall clock.
    int year = 2026;
    int month = 1;
    int day = 1;

    // Memberwise equality.
    friend bool operator==(const DateValue&, const DateValue&) = default;
    // Year, then month, then day: chronological order for valid dates.
    friend auto operator<=>(const DateValue&, const DateValue&) = default;
};

// The years a calendar here can honestly draw. Its arithmetic is Gregorian --
// the 100/400 leap rule and a weekday counted off a fixed epoch -- and that
// calendar began in October 1582, which is also the one month it cannot draw:
// the 5th to the 14th were struck out of it and never happened. So the first
// year it can show in full is 1583. Earlier dates were Julian, with different
// leap years and a different weekday for the same date, and drawing them on a
// Gregorian grid states something that was never true. The upper bound is the
// four digits a year is written in.
inline constexpr int kFirstCalendarYear = 1583;
inline constexpr int kLastCalendarYear = 9999;

// Whether `year` lies in kFirstCalendarYear..kLastCalendarYear inclusive.
constexpr bool is_drawable_year(int year) noexcept {
    return year >= kFirstCalendarYear && year <= kLastCalendarYear;
}

// The explicit, locale-free interchange form used by DatePicker and typed
// dialog results. Parsing is strict: exactly YYYY-MM-DD, a drawable Gregorian
// year, and a real day in that month.
//
// Formatting does not validate: month and day are zero-padded to two digits
// and the year is written with as many digits as it has, so only a valid date
// round-trips through parse_iso_date. Parsing returns std::nullopt for
// anything else.
std::string format_iso_date(DateValue date);
std::optional<DateValue> parse_iso_date(std::string_view text) noexcept;
// Whether `date` is a real Gregorian day in a drawable year: month 1..12 and a
// day that exists in that month, 29 February only in a leap year.
bool is_valid_date(DateValue date) noexcept;
// `date` moved by `days` calendar days, backwards when negative, across month
// and year ends. std::nullopt when `date` is not valid or the result would
// fall outside 1583-01-01..9999-12-31.
std::optional<DateValue> add_calendar_days(DateValue date, int days) noexcept;

// A time of day as three plain numbers, with no date, time zone or locale.
// The struct enforces nothing; is_valid_time says whether it is in range.
struct TimeValue {
    // The hour 0..23, the minute 0..59 and the second 0..59, always on the
    // twenty-four-hour scale whatever a widget displays. Defaults to midnight.
    int hour = 0;
    int minute = 0;
    int second = 0;

    // Memberwise equality.
    friend bool operator==(const TimeValue&, const TimeValue&) = default;
};

// A date and a time of day, read together. One reading rather than two, so a
// display showing both can never pair one day's date with the next day's time
// when it is asked just after midnight.
struct DateTimeValue {
    // The two halves of the one reading.
    DateValue date;
    TimeValue time;

    // Memberwise equality.
    friend bool operator==(const DateTimeValue&, const DateTimeValue&) = default;
};

// Locale-free interchange for typed time controls. Parsing accepts canonical
// HH:MM and HH:MM:SS forms only; formatting includes seconds when requested.
//
// is_valid_time checks hour 0..23, minute 0..59 and second 0..59; there is no
// leap second. format_iso_time zero-pads each field to two digits and does
// not validate. parse_iso_time takes exactly five or eight characters, reads a
// missing seconds field as 0, and returns std::nullopt for anything that is
// not a valid time.
bool is_valid_time(TimeValue time) noexcept;
std::string format_iso_time(TimeValue time, bool include_seconds = true);
std::optional<TimeValue> parse_iso_time(std::string_view text) noexcept;

// Whether an hour reads 0..23 or 1..12 with a meridiem word. Which one a
// reader expects is a property of where they are, not of the clock, so the
// host chooses it along with the words -- ckVision carries no locale data.
enum class HourFormat { TwentyFour, TwelveHour };

// The day a week starts on. Also a property of where the reader is: Monday
// across most of Europe, Sunday across much of the Americas and Asia.
enum class Weekday { Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday };

// A clock that repaints when the time it shows changes, and not otherwise.
//
// It ticks once a second and re-renders, but only asks for a repaint when
// the rendered text actually differs. A clock without seconds therefore
// costs one string comparison a second and one repaint a minute; the frame
// diff never sees the other fifty-nine.
//
// The time comes from an injected provider, as the calendar's today does:
// ckVision's Clock is monotonic with an implementation-defined epoch,
// deliberately not a wall clock. The provider is what makes a clock
// testable -- a test hands it a value and steps it.
class ClockView : public ui::View, public MenuBarAccessory {
public:
    // Never a focus stop: on a menu bar the bar's keyboard walk reaches it.
    // It shows nothing until it has a time provider, and starts its
    // one-second tick once attached to an Application.
    ClockView();

    // Where the time comes from. Asked at once, then on every tick; the
    // clock keeps no time of its own. An empty function blanks the display.
    void set_time_provider(std::function<TimeValue()> provider);
    // Whether the seconds are shown, as HH:MM:SS. Off by default.
    void set_show_seconds(bool show);
    bool show_seconds() const noexcept { return show_seconds_; }
    // The separator blinks: it is drawn for one tick and blank for the next,
    // so it is on for one second and off for the following one. Nothing else
    // about the display changes, so a blinking clock repaints once a second
    // and a still one does not. Off by default; switching it relights the
    // separator.
    void set_blinking_separator(bool blinking);
    bool blinking_separator() const noexcept { return blinking_separator_; }
    // TwentyFour, the default, zero-pads the hour ("09:41"). TwelveHour
    // writes 1..12 unpadded, midnight and noon as 12, followed by a space and
    // the meridiem label ("9:41 AM").
    void set_hour_format(HourFormat format);
    HourFormat hour_format() const noexcept { return hour_format_; }
    // The words for the two halves of a twelve-hour day. Empty labels
    // suppress the suffix entirely.
    void set_meridiem_labels(std::string am, std::string pm);

    // What it is displaying right now, separator and all.
    std::string text() const { return rendered_; }

    // Fires on a completed click, so a clock can open something -- a
    // calendar, say -- without knowing what. A click completes when a press
    // on the clock is released over it. Also fires when the menu bar's
    // keyboard walk activates the clock.
    std::function<void()> on_click;

    // Whether whatever this clock opened is currently open. It then draws
    // the way a menu title with its dropdown down does, using the same two
    // roles, because to a reader it IS that: a thing on the bar that is
    // showing something. Reusing the menu's roles rather than inventing a
    // highlight also means a theme dresses both alike without being asked
    // twice.
    void set_open(bool open);
    bool open() const noexcept { return open_; }

    // MenuBarAccessory: the bar walks onto this title and acts on it, so it
    // needs no separate keyboard handling of its own.
    void set_menu_highlighted(bool highlighted) override;
    void activate_from_menu_bar() override;

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;
    // Exactly the width of text() plus one cell of padding either side (at
    // least three cells), and one row. Re-published whenever the text changes
    // width, so a clock that gains seconds or a meridiem word is given the
    // room for them.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

private:
    std::string render() const;
    void refresh();
    void ensure_ticking();

    std::function<TimeValue()> time_provider_;
    bool show_seconds_ = false;
    bool blinking_separator_ = false;
    HourFormat hour_format_ = HourFormat::TwentyFour;
    std::string am_label_ = "AM";
    std::string pm_label_ = "PM";
    std::string rendered_;
    bool separator_lit_ = true;
    bool open_ = false;
    bool menu_highlighted_ = false;
    bool ticking_ = false;
    bool pressed_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId open_role_ = ui::kInvalidRole;
};

// One month as a grid of days: an optional "August 2026" title row, a row of
// weekday names, then up to six weeks, each day two digits in a three-cell
// column. One day is always the selection; the arrow keys move it by a day
// (Left/Right) or a week (Up/Down), across month ends, and a left click picks
// a day. A day outside the range or refused by the disabled predicate cannot
// be selected by either, and a move onto one does nothing.
//
// Resolves its theme roles from context() once attached: "ckv.list.normal",
// "ckv.list.selected" and "ckv.list.selected.inactive" for days and the
// selection (full highlight only while focused), "ckv.menu.dropdown.disabled"
// for a day that cannot be chosen, "ckv.calendar.today",
// "ckv.calendar.marked", and "ckv.list.disabled" for the whole calendar
// disabled. A day's style is decided in that order of precedence: not
// selectable, selected, today, marked.
class CalendarView : public ui::View {
public:
    // A Tab stop showing January 2026 with 2026-01-01 selected, English month
    // and weekday names, weeks starting on Monday, no today mark.
    CalendarView();

    // The month shown. Only year and month are used; the year is clamped into
    // kFirstCalendarYear..kLastCalendarYear and the month into 1..12. The
    // selection is not moved, so it may lie in another month.
    void set_month(DateValue first_of_month);
    // The first day of the month shown.
    DateValue month() const noexcept { return month_; }
    // Selects `selected`, its day clamped into its month, and shows its month.
    // Does not fire on_select. A date that cannot be selected leaves the
    // selection as it was, and the calendar then shows the old selection's
    // month.
    void set_selected(DateValue selected);
    DateValue selected() const noexcept { return selected_; }
    // Today, as a fixed value. std::nullopt is how a calendar is told not
    // to mark today at all -- a date picker for a birthday has no use for it.
    void set_today(std::optional<DateValue> today);
    const std::optional<DateValue>& today() const noexcept { return today_; }

    // Today, asked for afresh whenever the calendar repaints.
    //
    // A calendar left open across midnight goes on marking yesterday, which
    // is worse than marking nothing: it is confidently wrong about the one
    // fact it exists to state. The widget cannot fix that alone -- ckVision's
    // Clock is monotonic with an implementation-defined epoch, deliberately
    // not a wall clock, and reaching for the system date from a widget is
    // exactly what keeps it out of headless tests. So the host supplies the
    // date and the widget asks again each time it draws.
    //
    // Note what this does and does not buy: any repaint picks up the new
    // date, but a calendar sitting on a still screen is not repainting. A
    // host that wants the mark to turn over unattended has to invalidate it
    // when the day does.
    //
    // While a provider is set it takes precedence over set_today; an empty
    // function hands the mark back to the fixed value.
    void set_today_provider(std::function<std::optional<DateValue>()> provider);

    // A span of days to mark, clipped to whichever month is displayed.
    // Either end may be absent, meaning open in that direction. Pass two
    // absent ends to mark nothing. Ends given the wrong way round are
    // swapped.
    void set_marked_span(std::optional<DateValue> first, std::optional<DateValue> last);
    // The earliest and latest selectable days, both inclusive; an absent end
    // leaves that side open. Days outside are drawn as unselectable. The
    // current selection is kept even when it now falls outside.
    void set_range(std::optional<DateValue> minimum, std::optional<DateValue> maximum);
    // Days for which `predicate` returns true cannot be selected, on top of
    // the range. Called for every day drawn and every selection attempt; an
    // empty function disables no day.
    void set_disabled_predicate(std::function<bool(DateValue)> predicate);
    // Replaces the month names (exactly 12, January first) used in the title,
    // and the weekday names (exactly 7, Monday first, whatever the first
    // weekday) used in the header. A list of any other length leaves that set
    // as it was, so an empty list keeps it. Weekday names should be at most
    // two columns wide to fit their three-cell columns.
    void set_labels(std::vector<std::string> month_labels, std::vector<std::string> weekday_labels);
    // Draw with a caller's roles rather than the list's. A calendar inside a
    // dropdown has to look like a dropdown, not like a list that happens to
    // be floating. The three roles are for ordinary days, the selected day
    // while the calendar has focus, and a day that cannot be chosen; the
    // other roles still resolve on attach. Takes effect at the next repaint.
    void set_role_override(ui::RoleId normal, ui::RoleId selected, ui::RoleId disabled) noexcept {
        normal_role_ = normal;
        selected_role_ = selected;
        disabled_role_ = disabled;
    }
    // The "August 2026" line. Off when something above the grid already
    // says which month is shown -- a month picker and a year field, say.
    void set_show_title(bool show);
    bool show_title() const noexcept { return show_title_; }
    // The day the week starts on, which decides both the weekday header and
    // where the first of the month falls in the grid. Monday by default.
    void set_first_weekday(Weekday first);
    Weekday first_weekday() const noexcept { return first_weekday_; }
    // A "Wk" column of three cells at the left, labelling each row with the
    // ISO 8601 week number of the Monday in it. Off by default.
    void set_show_iso_week_numbers(bool show);
    bool show_iso_week_numbers() const noexcept { return show_iso_week_numbers_; }

    // Fires with the new selection when the reader changes it, by arrow key
    // or click. Not fired by set_selected, nor when a click lands on the day
    // already selected.
    std::function<void(DateValue)> on_select;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    // At least 21 cells wide, preferring 21 or 24 with week numbers, and free
    // to stretch; always eight rows high.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    void on_attached() override;

private:
    bool selectable(DateValue date) const;
    void move_selection(int days);
    void select(DateValue date, bool notify);
    std::optional<DateValue> date_at_cell(Point local) const;

    DateValue month_{2026, 1, 1};
    DateValue selected_{2026, 1, 1};
    std::optional<DateValue> today_;
    std::function<std::optional<DateValue>()> today_provider_;
    std::optional<DateValue> marked_first_;
    std::optional<DateValue> marked_last_;
    bool has_marked_span_ = false;
    std::optional<DateValue> minimum_;
    std::optional<DateValue> maximum_;
    std::function<bool(DateValue)> disabled_;
    std::vector<std::string> month_labels_;
    std::vector<std::string> weekday_labels_;
    Weekday first_weekday_ = Weekday::Monday;
    bool show_title_ = true;
    int header_row() const noexcept { return show_title_ ? 1 : 0; }
    int grid_top() const noexcept { return header_row() + 1; }
    bool show_iso_week_numbers_ = false;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId today_role_ = ui::kInvalidRole;
    ui::RoleId marked_role_ = ui::kInvalidRole;
    // The whole calendar disabled (D-076), distinct from a day that cannot
    // be chosen, which is disabled_role_.
    ui::RoleId view_disabled_role_ = ui::kInvalidRole;
    ui::RoleId selected_inactive_role_ = ui::kInvalidRole;

    std::optional<DateValue> effective_today() const;
    bool within_marked_span(DateValue date) const noexcept;
};

// A CalendarView shown as a transient popup: it closes on Escape, and on a
// click anywhere but itself. That behaviour is what separates a dropdown
// from a small window, and it does not belong in CalendarView -- the same
// calendar sits permanently in a dialog elsewhere and must not vanish when
// the reader clicks beside it.
//
// Inside a single-line frame sit one control row -- a month picker, "<<" and
// ">>" year steppers around a four-digit year field -- and the calendar below
// it, with its own title off. Tab walks month, year and days; the steppers are
// for the pointer. Enter in the year field, or Tab out of it, commits the
// typed year; a year that is empty or not drawable puts "invalid" over the
// row for three seconds, then the year shown comes back. Picking a day does
// not close the dropdown: whoever put it up decides that from on_select.
//
// Resolves "ckv.menu.dropdown.normal" for its frame on attach and dresses the
// calendar and the steppers in the dropdown menu's roles.
class CalendarDropdown : public ui::View {
public:
    // Builds the children; not a focus stop itself. The year field is empty
    // and the calendar on its default month until show_month is called.
    CalendarDropdown();

    // The dropdown's own children, owned by it and valid for its lifetime:
    // for choosing the selection, range, marks or on_select of the calendar,
    // or for inspecting the controls.
    CalendarView& calendar() noexcept { return *calendar_; }
    ComboBox& month_picker() noexcept { return *month_; }
    InputLine& year_field() noexcept { return *year_; }

    // The month names offered by the picker, in order. The same list the
    // calendar itself is given, so the two never disagree.
    void set_month_labels(std::vector<std::string> labels);

    // Puts the calendar on `month`, and the two controls with it.
    void show_month(DateValue month);

    // Both fire once, on the first dismissal -- Escape not consumed by the
    // focused control, or a press outside the dropdown -- on_closed first,
    // then on_dismiss. on_dismiss is the teardown: show_calendar_dropdown
    // sets it to release the capture and the modal scope and destroy the
    // dropdown. A caller of that function leaves on_dismiss alone and
    // observes the close through on_closed, which runs while the dropdown
    // still exists.
    std::function<void()> on_dismiss;
    std::function<void()> on_closed;

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    bool on_key(const KeyEvent& event) override;
    void on_resized() override;
    void on_attached() override;
    // A fixed size: 23 cells by 10 rows, the frame around the control row, the
    // weekday header and six weeks.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

private:
    void dismiss();
    void step_year(int delta);
    void commit_year();
    void report_invalid_year();
    void focus_slot(int slot);
    void sync_month_bounds();
    int month_width() const noexcept;
    Rect year_rect() const noexcept;
    ui::View* child_at(Point local) const noexcept;
    void set_control_row_visible(bool visible);

    CalendarView* calendar_ = nullptr;
    ComboBox* month_ = nullptr;
    InputLine* year_ = nullptr;
    Button* step_back_ = nullptr;
    Button* step_forward_ = nullptr;
    ui::View* mouse_child_ = nullptr;
    std::vector<std::string> month_labels_;
    int focus_slot_ = 0;  // 0 month, 1 year, 2 grid
    bool dismissed_ = false;
    bool showing_invalid_ = false;
    ui::RoleId frame_role_ = ui::kInvalidRole;
};

// Drops a calendar below `anchor`, right-aligned with it the way a menu is
// aligned with the title that opened it. Owned by the desktop; it takes
// input capture and hands itself back so the caller can close it.
//
// The dropdown is placed at its preferred size, clamped to stay inside the
// desktop, in a modal scope of its own so keys and Tab stay inside it, and
// the calendar takes the focus. The caller sets it up through the returned
// pointer -- show_month first, since it opens on the calendar's default month
// with an empty year field -- which stays valid until the dropdown is
// dismissed: then on_dismiss, installed here, releases the capture and the
// scope and removes it from the desktop, destroying it.
CalendarDropdown* show_calendar_dropdown(const ui::View& anchor, ui::Application& app, Desktop& desktop);

// A one-row date field showing YYYY-MM-DD, or "— no date —" when empty,
// edited a segment at a time. Left/Right choose the year, month or day
// segment (a click chooses the one under the pointer) and Up/Down or the
// wheel step it by one: the year within the drawable range, the month across
// year ends, the day across month ends, each stopping at the range's ends and
// clamping the day into a shorter month. PageUp/PageDown step the month and
// make it the active segment. Delete or Backspace clears the value when empty
// is allowed. With a calendar host a "▾" is drawn at the right edge, and Space
// or a click on the last two columns opens a CalendarDropdown.
//
// Resolves "ckv.input.normal", "ckv.input.focused", "ckv.input.invalid" and
// "ckv.input.disabled" on attach; the segment being edited is drawn reversed
// while focused.
class DatePicker : public ui::View {
public:
    // A Tab stop, empty, with empty allowed and a seed of 2026-01-01.
    DatePicker();
    // A date picker is often used for an optional fact (a due date, an end
    // date, a filter bound). Empty is therefore a first-class value rather
    // than a sentinel date. `seed` is the date the first Up/Down edit while
    // empty starts from -- that edit steps the seed's active segment -- and
    // the day the calendar marks as today; callers normally supply their
    // injected notion of today.
    //
    // set_value clamps the year into the drawable range and the month and
    // day into theirs, and a date also becomes the new seed. std::nullopt
    // becomes the seed instead when empty is not allowed. Any change clears
    // the invalid mark and fires on_change, programmatic or not.
    void set_value(std::optional<DateValue> value);
    std::optional<DateValue> value() const noexcept { return value_; }
    // Clamped like a value; does not change the value.
    void set_seed(DateValue seed);
    DateValue seed() const noexcept { return seed_; }
    // Whether the reader may clear the value. Turning it off while empty sets
    // the value to the seed, which fires on_change.
    void set_empty_allowed(bool allowed);
    bool empty_allowed() const noexcept { return empty_allowed_; }
    // A caller's verdict on the value, such as an end date before its start:
    // false draws the field in the invalid style. The picker never sets it
    // false itself, and any change of value sets it back to true.
    void set_valid(bool valid);
    bool valid() const noexcept { return valid_; }
    // Supplies the explicit popup host used by Space or the dropdown
    // affordance. Without a host, DatePicker remains a standalone segmented
    // editor and performs no hidden application or desktop lookup. The picker
    // keeps pointers to both, so both must outlive it.
    void set_calendar_host(ui::Application& app, Desktop& desktop) noexcept;
    // Opens the calendar below the field, on the value's month (the seed's
    // when empty), with the value selected and the seed marked as today. A
    // day chosen there becomes the value; the dropdown stays open until
    // dismissed. Returns false without a host, and true when a calendar is
    // open, including one that already was.
    bool open_calendar();
    // Fires with the new value after every change, from the reader or from
    // set_value and set_empty_allowed.
    std::function<void(std::optional<DateValue>)> on_change;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    void adjust_active(int delta);
    void select_field_at(int x);

    std::optional<DateValue> value_{};
    DateValue seed_{2026, 1, 1};
    int active_field_ = 0;  // 0 year, 1 month, 2 day
    bool empty_allowed_ = true;
    bool valid_ = true;
    ui::Application* calendar_app_ = nullptr;
    Desktop* calendar_desktop_ = nullptr;
    CalendarDropdown* calendar_dropdown_ = nullptr;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// A one-row time field, HH:MM[:SS], edited a field at a time. Left/Right
// choose the hour, minute or second field and Up/Down step it by one,
// wrapping within the field without carrying (59 minutes goes to 00, the hour
// stays). A left click chooses the field under the pointer and steps it up by
// one.
//
// Resolves "ckv.input.normal", "ckv.input.focused", "ckv.input.invalid" and
// "ckv.input.disabled" on attach; the field being edited is drawn reversed
// while focused.
class TimePicker : public ui::View {
public:
    // A Tab stop at 00:00:00, seconds shown, twenty-four-hour.
    TimePicker();
    // Each field is clamped into range. A change fires on_change, programmatic
    // or not.
    void set_value(TimeValue value);
    TimeValue value() const noexcept { return value_; }
    // Hiding the seconds removes that field; a selection on it moves to the
    // minutes.
    void set_show_seconds(bool show);
    // False shows the hour as 01..12 with a fixed " AM"/" PM" suffix; the
    // value itself stays on the twenty-four-hour scale.
    void set_24_hour(bool enabled);
    // A caller's verdict on the value: false draws the field in the invalid
    // style until set back to true. Unlike DatePicker, a change of value does
    // not clear it.
    void set_valid(bool valid) { valid_ = valid; invalidate(); }
    bool valid() const noexcept { return valid_; }
    bool show_seconds() const noexcept { return show_seconds_; }
    bool twenty_four_hour() const noexcept { return twenty_four_hour_; }
    // Fires with the new value after every change, from the keyboard, a click
    // or set_value.
    std::function<void(TimeValue)> on_change;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    void adjust(int delta);
    TimeValue value_;
    int field_ = 0;
    bool show_seconds_ = true;
    bool twenty_four_hour_ = true;
    bool valid_ = true;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// A one-row integer field drawn as "< 42 >". Up or Right steps the value up,
// Down or Left steps it down, and a left click on the right half of the field
// steps up, on the left half down. Where the field is too narrow the arrows
// are dropped first, and a number that still does not fit is elided, never
// cut.
//
// Resolves "ckv.input.normal", "ckv.input.focused" and "ckv.input.disabled"
// on attach.
class SpinBox : public ui::View {
public:
    // A Tab stop over 0..100, step 1, at 0.
    SpinBox();
    // The inclusive bounds; given the wrong way round they are swapped. The
    // value is clamped into the new range, which fires on_change if it moves.
    void set_range(int minimum, int maximum);
    // How far one key press or click moves the value; at least 1. Steps are
    // not aligned to multiples of it, and the last step is clamped at the
    // range's end.
    void set_step(int step);
    // Clamped into the range.
    void set_value(int value);
    int value() const noexcept { return value_; }
    // Fires with the new value after every change, from the reader or from
    // set_value and set_range.
    std::function<void(int)> on_change;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    void adjust(int delta);
    int minimum_ = 0;
    int maximum_ = 100;
    int step_ = 1;
    int value_ = 0;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// A one-row horizontal slider: a track with the part up to the value filled
// and a thumb at the value's position ("◆" while focused, "●" otherwise).
// Left/Right step the value, Home/End jump to the minimum and maximum, and a
// left click sets the value in proportion to where it lands between the
// first and last column, without snapping to the step.
//
// Resolves "ckv.list.normal" for the track, "ckv.menu.bar.active" for the
// filled part and thumb, and "ckv.list.disabled" on attach.
class Slider : public ui::View {
public:
    // A Tab stop over 0..100, step 1, at 0.
    Slider();
    // The inclusive bounds; given the wrong way round they are swapped. The
    // value is clamped into the new range, which fires on_change if it moves.
    void set_range(int minimum, int maximum);
    // How far one Left/Right press moves the value; at least 1. The last step
    // is clamped at the range's end.
    void set_step(int step);
    // Clamped into the range.
    void set_value(int value);
    int value() const noexcept { return value_; }
    // Fires with the new value after every change, from the reader or from
    // set_value and set_range.
    std::function<void(int)> on_change;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    void adjust(int delta);
    int value_from_x(int x) const;
    int minimum_ = 0;
    int maximum_ = 100;
    int step_ = 1;
    int value_ = 0;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId fill_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// A one-row search field: a "Search " prompt, the query in an input-coloured
// field with a caret at its end, and a "[x]" clear control at the right edge
// while there is a query. Typed characters (without Alt, Ctrl or Super) and
// text input append to the query; Backspace removes its last byte; Escape
// clears it. A click on the "[x]" clears it. When the query is wider than the
// field its end is shown, the part being typed. There is no caret movement:
// the query is edited at its end only.
//
// Resolves "ckv.input.normal", "ckv.input.focused", "ckv.input.disabled",
// "ckv.label.text" and "ckv.label.disabled" on attach.
class SearchBox : public ui::View {
public:
    // A Tab stop with an empty query.
    SearchBox();
    // Replaces the query; fires on_change if it differs, programmatic or not.
    void set_query(std::string query);
    const std::string& query() const noexcept { return query_; }
    // Empties the query, firing on_change if it was not empty, then fires
    // on_clear in any case. Escape and the "[x]" control come here.
    void clear();
    // Fires with the whole new query after every change: each typed
    // character, Backspace, clearing, and set_query.
    std::function<void(const std::string&)> on_change;
    // Fires after every clear(), including one of an already empty query.
    std::function<void()> on_clear;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    std::optional<CursorState> cursor_state() const override;

private:
    // The part of the query shown in a field `field_width` columns wide.
    std::string visible_query(int field_width) const;
    // Columns the "[x]" clear control occupies at the right edge. Drawing and
    // hit-testing both derive from this, which is what keeps the control the
    // reader can see and the region that answers a click the same thing.
    static constexpr int kClearControlWidth = 3;

    std::string query_;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId label_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId label_disabled_role_ = ui::kInvalidRole;
};

// A one-row strip of command buttons for the pointer. Each command is drawn as
// "[Title]" -- its registry title with the '&' mnemonic marker removed, since
// the bar never takes the keyboard -- one blank apart, in the disabled style
// while the registry reports it disabled. A left click on a button executes
// its command through the attached Application's registry, which runs only
// an enabled command with a handler. A command the registry does not know,
// or any command while the bar is detached, is drawn as "[(unknown)]".
//
// Not a focus stop. Resolves "ckv.menu.bar.normal" and
// "ckv.menu.dropdown.disabled" on attach; always one row high.
class ToolBar : public ui::View {
public:
    // The commands shown, left to right. Buttons that do not fit the width
    // are clipped or left out.
    void set_commands(std::vector<ui::CommandId> commands);
    const std::vector<ui::CommandId>& commands() const noexcept { return commands_; }

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    ui::SizeHint vertical_size_hint() const override;
    void on_attached() override;

private:
    int command_at_x(int x) const;
    std::vector<ui::CommandId> commands_;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// Browse-by-name access to the registry: the commands whose titles
// match the current query, in declaration order.
//
// What it lists is what each command says about itself —
// ui::CommandVisibility::Palette — never a property inferred from the
// id. An application decides what belongs in a browsable list by
// declaring it (or by set_visibility()), which is the only party that
// can answer the question; the framework's own standard commands and a
// MenuBar's menu accelerators declare themselves Hidden.
//
// Drawn with a two-cell inset: the query field on the third row, the results
// from the fifth, each title with its mnemonic accented and its key chord in
// parentheses, and a scroll gutter when they do not all fit. Typed characters
// (without Alt, Ctrl or Super) append to the query, Backspace removes its last
// byte, Up/Down move the highlight, and Enter executes the highlighted
// command, within the invocation contexts when they are set. The palette does
// not close itself; its host does that, and a detached palette lists nothing.
// Resolves "ckv.input.normal", "ckv.option.normal" and "ckv.option.focused"
// on attach.
class CommandPalette : public ui::View {
public:
    // A Tab stop with an empty query, preferring 40 x 8 cells.
    CommandPalette();
    // Replaces the query and moves the highlight back to the first result.
    // The match is a case-insensitive (ASCII) substring of the title as
    // displayed, mnemonic marker removed; an empty query matches every
    // command. Fires nothing.
    void set_query(std::string query);
    const std::string& query() const noexcept { return query_; }
    // Palette-visible, title-matching, currently available — in the
    // order the commands were declared.
    std::vector<ui::CommandInfo> filtered_commands() const;
    // The command contexts the palette answers for (ui::command_context_path
    // of the view the reader was on when it opened), instead of its own
    // focus path. A palette presented in a window of its own takes the focus
    // from the place whose commands it lists, exactly as a menu walk does,
    // and without this it would offer what the palette's window allows —
    // no context-bound command at all.
    void set_invocation_contexts(std::vector<std::string> contexts);
    // The command Enter would execute: the highlighted entry of
    // filtered_commands(), or std::nullopt when nothing matches.
    std::optional<ui::CommandId> highlighted_command() const;

    void draw(scene::Painter& painter) override;
    // The caret at the end of the query: the palette is typed into, and the
    // caret is what says so.
    std::optional<CursorState> cursor_state() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    bool available(ui::CommandId id) const;

    std::string query_;
    std::size_t highlighted_ = 0;
    std::optional<std::vector<std::string>> invocation_contexts_;
    ui::RoleId input_role_ = ui::kInvalidRole;
    ui::RoleId result_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
};

// A one-row path of segments, such as the folders leading to a file, drawn
// left to right with a separator between them; what does not fit the width is
// clipped. Left/Right move the focused segment, highlighted while the bar has
// focus, and Enter activates it; a left click on a segment focuses and
// activates it. Resolves "ckv.label.text", "ckv.list.selected" and
// "ckv.label.disabled" on attach.
class BreadcrumbBar : public ui::View {
public:
    // A tab stop from construction: the segments are walked and activated
    // from the keyboard.
    BreadcrumbBar();
    // The segments, first to last. Moves the focus back to the first segment.
    void set_segments(std::vector<std::string> segments);
    const std::vector<std::string>& segments() const noexcept { return segments_; }
    // Drawn verbatim between adjacent segments, "/" by default; include any
    // spaces wanted around it.
    void set_separator(std::string separator);
    // Fires with the index of the segment activated, by Enter or a click, so
    // the host can navigate to that level. Enter is not consumed while this
    // is empty.
    std::function<void(std::size_t)> on_activate;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    int segment_at_x(int x) const;
    std::vector<std::string> segments_;
    std::string separator_ = "/";
    std::size_t focused_ = 0;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// One row of a PropertyInspector, drawn as "name: value".
struct PropertyItem {
    // The label, and the value as text; the inspector edits the value in
    // place.
    std::string name;
    std::string value;
    // Whether Enter may start editing the value. A read-only row still takes
    // the cursor.
    bool editable = true;
};

// A list of name/value rows, one per row from the top, with a cursor row drawn
// as a full-width bar (muted while unfocused). Up/Down move the cursor; Enter
// on an editable row starts editing its value and Enter again ends it, as do
// Escape and moving the cursor. While editing, typed characters and text input
// append to the value and Backspace removes its last grapheme, with the caret
// at the value's end. Rows below the view's height are not drawn: there is no
// scrolling. Becomes a Tab stop when attached. Resolves "ckv.list.normal",
// "ckv.list.selected", "ckv.list.selected.inactive", "ckv.list.disabled" and
// "ckv.input.focused" (the value being edited) on attach.
class PropertyInspector : public ui::View {
public:
    // Replaces the rows and puts the cursor on the first.
    void set_items(std::vector<PropertyItem> items);
    // The rows, including every edit made so far.
    const std::vector<PropertyItem>& items() const noexcept { return items_; }
    // The index of the cursor row, or -1 when there are no rows.
    int cursor() const noexcept { return cursor_; }
    // Fires on every edit by the reader, each character typed or removed,
    // with the row's index and its whole new value. Not fired by set_items.
    std::function<void(std::size_t, std::string)> on_change;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    // The caret at the end of a value being edited.
    std::optional<CursorState> cursor_state() const override;
    bool on_text(const TextEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    std::vector<PropertyItem> items_;
    int cursor_ = 0;
    bool editing_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId selected_inactive_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId editing_role_ = ui::kInvalidRole;
};

// One step of a Wizard.
struct WizardPage {
    // Drawn on the wizard's top row while this page is current.
    std::string title;
    // Whether the reader may go on from this page, to the next or, on the
    // last, to finish. Asked afresh whenever that is decided or drawn; empty
    // means always.
    std::function<bool()> can_continue;
};

// The navigation of a multi-step flow: the current page's title on the top
// row and, on the bottom row, "< Back " when there is a page before this one
// and "Next >" or, on the last page, "Finish" -- greyed while the page's
// can_continue refuses, reversed while the wizard has focus. It draws no page
// content; the host shows what belongs to current_page(). Right or Enter
// goes next, or finishes on the last page; Left goes back; Escape fires
// on_cancel. Becomes a Tab stop when attached. Resolves
// "ckv.dialog.background", "ckv.window.title.active" and "ckv.label.disabled"
// on attach.
class Wizard : public ui::View {
public:
    // Replaces the pages and returns to the first.
    void set_pages(std::vector<WizardPage> pages);
    // The number of pages, and the zero-based index of the current one.
    std::size_t page_count() const noexcept { return pages_.size(); }
    std::size_t current_page() const noexcept { return current_page_; }
    // Whether next() would move: there is a later page and the current
    // page's can_continue allows it.
    bool can_go_next() const;
    // Whether back() would move: the current page is not the first.
    bool can_go_back() const noexcept { return current_page_ > 0; }
    // Move one page forward or back when allowed; false, with nothing
    // changed, when not. Neither fires a callback.
    bool next();
    bool back();
    // On the last page, when its can_continue allows it, fires on_finish and
    // returns true. Anywhere else, or with no pages, returns false. The page
    // stays where it is either way.
    bool finish();
    // Fires from finish(), whether it came from a key or the host.
    std::function<void()> on_finish;
    // Fires on Escape. Escape is not consumed while this is empty.
    std::function<void()> on_cancel;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    std::vector<WizardPage> pages_;
    std::size_t current_page_ = 0;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// How serious a notification is. It chooses the mark drawn before the text --
// "i", "!" or "x" -- and that mark's colour, from the "ckv.message.info.text",
// "ckv.message.warning.text" or "ckv.message.error.text" role. It does not
// change how long the notification lives.
enum class NotificationSeverity { Info, Warning, Error };

// One line posted to a NotificationCenter.
struct Notification {
    // Its severity, and the message, drawn on one row and elided with an
    // ellipsis when wider than the view.
    NotificationSeverity severity = NotificationSeverity::Info;
    std::string text;
    // Whether this one waits for the reader. A persistent notification is
    // never taken away by time — it stays until it is dismissed, by a click,
    // by Escape, or by the host. Everything else expires on its own once the
    // host has said how long a toast lives (set_auto_dismiss).
    //
    // The distinction is about whether the reader has to have SEEN it. "Config
    // reloaded" missed is nothing lost; "this session was taken over by
    // ttys011" missed leaves a reader wondering where their work went, so that
    // one waits however long it has to.
    bool persistent = false;
};

// Non-modal feedback: a short stack of lines the application posts and the
// reader does not have to answer. It is not a message box — anything needing
// a decision, or needing to be acknowledged before the application goes on,
// is a modal dialog and belongs in one.
//
// Toasts arrive and leave by themselves once a host calls set_auto_dismiss;
// without it nothing expires, which is what every consumer written before
// that got and still gets. A notification the reader must not miss says so
// with `Notification::persistent` and outlives any timer.
class NotificationCenter : public ui::View {
public:
    // Focusable by default, because a centre a reader Tabs to and dismisses
    // with Escape is what a form wants. Set in the CONSTRUCTOR rather than on
    // attach so a host can say otherwise and be obeyed: an application that
    // lays toasts over its work — where the notification is news rather than
    // something to answer — takes the focus stop away, and re-attaching must
    // not quietly put it back.
    NotificationCenter();

    // Posts a notification below the others and returns its index in
    // notifications(). The index is only good until the next removal: every
    // dismissal or expiry of an earlier line shifts the later ones up. Its
    // expiry, if any, is fixed now from the interval in force, or on
    // attachment for one posted to a detached centre. Fires on_changed.
    std::size_t add(Notification notification);
    // Removes the notification at `index`, persistent or not, and fires
    // on_changed. An index past the end is ignored.
    void dismiss(std::size_t index);
    // The notifications on show, oldest first; row i of the view draws entry
    // i, as far as the height allows. Escape dismisses the last, the newest;
    // a left click dismisses the one under it.
    const std::vector<Notification>& notifications() const noexcept { return notifications_; }

    // --- How long a toast lives ---------------------------------------
    //
    // The lifetime of a NON-persistent notification, measured on the injected
    // Clock from the moment it was added. Zero — the default — is no expiry
    // at all: nothing is taken away, which is the behaviour this widget had
    // before it could tell the time, and a host that never calls this sees no
    // change whatsoever. A negative interval is taken as zero.
    //
    // Time is read from the Application this view is attached to, so a
    // detached centre simply holds what it was given until it is attached;
    // there is nowhere to read a clock from and refusing to accept a
    // notification would be worse than showing it a moment late.
    //
    // Changing the interval re-times what is already on screen, deliberately:
    // a host that shortens its toasts means the ones in front of the reader
    // too, and leaving them on the old interval would make the setting take
    // effect at a moment nobody chose.
    void set_auto_dismiss(std::int64_t nanos);
    std::int64_t auto_dismiss_nanos() const noexcept { return auto_dismiss_nanos_; }

    // Fires whenever the set of notifications changes for any reason — one
    // posted, one dismissed by the reader, or one that expired on its own.
    // A host that sizes or places this view from `notifications().size()`
    // needs it, because expiry happens on a timer that the host never sees:
    // without it, a centre that emptied itself would leave the host holding
    // a rectangle for rows that are no longer there.
    std::function<void()> on_changed;

    void draw(scene::Painter& painter) override;
    // The line Escape would take away is marked only while focused.
    void on_focus(const FocusEvent& event) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    // Applies every expiry that is due, and arms one wake-up for the earliest
    // that is not. One timer for the whole stack rather than one per line:
    // what an expiry costs is a repaint of the view, so waking once and
    // sweeping is both cheaper and the only order in which two toasts that
    // come due together go together.
    void expire_due();
    void arm_expiry(std::int64_t deadline_nanos);
    void changed();

    std::vector<Notification> notifications_;
    // When each notification is due to leave, parallel to `notifications_`
    // and maintained with it. kNever for a persistent one, and for every one
    // added while no expiry interval was set. Kept beside the notification
    // rather than inside it so that `Notification` stays what it is — a
    // record a host writes — rather than gaining a field only this view may
    // fill in.
    static constexpr std::int64_t kNever = 0;
    std::vector<std::int64_t> deadlines_;
    std::int64_t auto_dismiss_nanos_ = 0;
    ui::Application::TimerId expiry_timer_ = 0;
    std::int64_t expiry_wake_nanos_ = 0;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId info_role_ = ui::kInvalidRole;
    ui::RoleId warning_role_ = ui::kInvalidRole;
    ui::RoleId error_role_ = ui::kInvalidRole;
};

// A one-row label with one cell of padding either side, in the "ckv.tooltip"
// role (resolved on attach). The host adds it where it may float, positions it
// with show_at and takes it away with hide; it takes no focus and no input,
// and does not time itself out.
class Tooltip : public ui::View {
public:
    // Hidden until show_at; prefers the text's width plus two by one row.
    explicit Tooltip(std::string text = {});
    // Replaces the text and the preferred size. A tooltip already shown keeps
    // its bounds until the next show_at, eliding the text to fit them.
    void set_text(std::string text);
    const std::string& text() const noexcept { return text_; }
    // Shows the tooltip with its top-left cell at `position`, in its parent's
    // coordinates, sized to the text plus padding (at least two cells). It is
    // not moved to fit inside the parent.
    void show_at(Point position);
    // Hides it; the text and bounds are kept.
    void hide();
    // Whether it is visible.
    bool shown() const noexcept { return visible(); }

    void draw(scene::Painter& painter) override;
    void on_attached() override;

private:
    std::string text_;
    ui::RoleId role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
