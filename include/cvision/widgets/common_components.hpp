// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/command_presentation.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/mnemonic.hpp"

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

// Every word a calendar, a date or time field, and the standard date and
// time dialogs put on screen, in the language the host writes them in.
// ckVision reads no locale, so these names come from a table the host
// supplies; a default-constructed table is the built-in English one.
struct DateTimeLabels {
    // The twelve month names, January first: a calendar's title, a month
    // picker's entries, and a date written with its month by name.
    std::vector<std::string> month_names{"January", "February", "March",     "April",   "May",      "June",
                                         "July",    "August",   "September", "October", "November", "December"};
    // The seven weekday headings, Monday first whatever day a week starts
    // on. Each should be at most two columns wide, to fit a calendar's
    // three-cell day column.
    std::vector<std::string> weekday_names{"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
    // The words for the two halves of a twelve-hour day. An empty word
    // leaves the suffix off.
    std::string am = "AM";
    std::string pm = "PM";
    // What an empty date field shows.
    std::string no_date = "— no date —";
    // The reason a date field gives for a typed entry it cannot read as a
    // date in its format.
    std::string not_a_date = "Not a date.";
    // What a calendar dropdown puts over its year row, for a moment, when
    // the year typed there is one it cannot draw.
    std::string invalid_year = "invalid";
};

// The built-in English table: one immutable instance, valid for the life of
// the process, and the default argument wherever a table is optional.
const DateTimeLabels& english_date_time_labels() noexcept;

// The three parts of a written date, for naming the order they come in.
enum class DateField { Year, Month, Day };

// How a date writes its month: as a number, or by name from the month names
// of a DateTimeLabels table.
enum class MonthStyle { Number, Name };

// How a date is written and read: the formatting and parsing policy of a
// DatePicker and of a Date field in a descriptor dialog. The default is the
// ISO form, YYYY-MM-DD, exactly format_iso_date and parse_iso_date.
//
// The options describe a layout of three parts: `order` names each field
// once, `separator` stands between them, the year is written in its four
// digits, and the month and day are numbers -- two digits each while
// `zero_pad` holds, or as many as they have otherwise -- or, with
// MonthStyle::Name, the month is its name. Reading follows the same layout:
// exactly three parts between two separators, a four-digit year, a month
// and day of the width the options write (or its name, compared without
// regard to ASCII case), naming a real day in a drawable year.
//
// A caller whose dates the options cannot describe supplies `format` and
// `parse` instead. Either one, when set, replaces the options' own text for
// what it does; the options still decide the segments a DatePicker edits
// with the arrow keys, since arbitrary text has no segments a widget could
// find.
struct DateFormat {
    // The order the three fields are written in; each field exactly once.
    std::array<DateField, 3> order{DateField::Year, DateField::Month, DateField::Day};
    // Written between the fields; reading needs it to be non-empty.
    std::string separator = "-";
    // Whether the month is a number or a name.
    MonthStyle month_style = MonthStyle::Number;
    // Whether a numeric month and the day are written with two digits.
    bool zero_pad = true;
    // A caller's own formatting and parsing. Parsing returns std::nullopt for
    // text that is not a date; what it does return should be a valid date.
    std::function<std::string(DateValue)> format{};
    std::function<std::optional<DateValue>(std::string_view)> parse{};
};

// `date` written as `format` says, month names from `labels`. Like
// format_iso_date, the options' own formatting does not validate.
std::string format_date(DateValue date, const DateFormat& format,
                        const DateTimeLabels& labels = english_date_time_labels());
// `text` read as `format` says, or std::nullopt when it is not a valid date
// written that way. A caller's `parse` answers whatever it answers, except
// that a date it returns which is not valid is refused here.
std::optional<DateValue> parse_date(std::string_view text, const DateFormat& format,
                                    const DateTimeLabels& labels = english_date_time_labels());

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
    // other roles still resolve on attach. A change repaints.
    void set_role_override(ui::RoleId normal, ui::RoleId selected, ui::RoleId disabled) noexcept {
        if (normal_role_ == normal && selected_role_ == selected && disabled_role_ == disabled) return;
        normal_role_ = normal;
        selected_role_ = selected;
        disabled_role_ = disabled;
        invalidate();
    }
    // The "August 2026" line. Off when something above the grid already
    // says which month is shown -- a month picker and a year field, say.
    // A change repaints and reports a size-hint change.
    void set_show_title(bool show);
    bool show_title() const noexcept { return show_title_; }
    // The day the week starts on, which decides both the weekday header and
    // where the first of the month falls in the grid. Monday by default.
    void set_first_weekday(Weekday first);
    Weekday first_weekday() const noexcept { return first_weekday_; }
    // A "Wk" column of three cells at the left, labelling each row with the
    // ISO 8601 week number of the Monday in it. Off by default. A change
    // repaints and reports a size-hint change.
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
    // At least as wide as the columns drawn -- 20 cells, or 23 with week
    // numbers -- preferring one blank more, and free to stretch; exactly as high as what is drawn: the title row when shown,
    // the weekday heading and six week rows -- eight rows, or seven without
    // the title.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    // The rows it draws, whatever the width: a month grid does not wrap.
    int height_for_width(int width) const override;
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
    // The cells the "Wk" column takes at the left, 0 while it is hidden.
    int week_column_width() const noexcept { return show_iso_week_numbers_ ? 3 : 0; }
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
    // Every word the dropdown shows, from one table: the month names (as
    // set_month_labels), the calendar's weekday headings, and the word put
    // over the year row when a typed year is refused. A month or weekday list
    // of the wrong length leaves that set as it was.
    void set_labels(const DateTimeLabels& labels);

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
    // Closes it as a dismissal, exactly as Escape does -- for the control
    // that opened it, when something else has ended the interaction. Nothing
    // is chosen. The dropdown may be destroyed before this returns.
    void request_dismiss() { dismiss(); }

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
    std::string invalid_year_label_ = english_date_time_labels().invalid_year;
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

// A one-row date field, written as its DateFormat says (YYYY-MM-DD by
// default), or showing its labels' no-date text when empty, and edited a
// segment at a time. Left/Right choose the previous or next segment in the
// format's order (a click chooses the one under the pointer) and Up/Down or
// the wheel step it by one: the year within the drawable range, the month
// across year ends, the day across month ends, each stopping at the range's
// ends and clamping the day into a shorter month. PageUp/PageDown step the
// month and make it the active segment. Delete or Backspace clears the value
// when empty is allowed. With a calendar host a "▾" is drawn at the right
// edge, and Space or a click on the last two columns opens a
// CalendarDropdown.
//
// A date can also be typed. Any printable character other than a space
// opens an entry holding just that character, drawn in place of the value
// with the caret at its end; further characters and text input append to it,
// Backspace takes back its last grapheme, and Delete, which would otherwise
// clear the value, does nothing. Enter, or the focus leaving the field,
// commits it: text the format reads as a date becomes the value, and
// an empty entry empties the value when empty is allowed. Anything else is
// refused -- never guessed at or clamped: the entry stays as typed, the field
// is drawn invalid, validation_message() gives the reason and on_invalid
// fires with it. Escape abandons the entry and shows the value again. An
// arrow, PageUp/PageDown, the wheel or a click commits the entry first and
// does nothing more when it is refused.
//
// While the field does not have the focus it shows the format's `format`
// callback's text when there is one; while it has the focus it shows the
// options' layout, whose segments the arrows edit.
//
// Resolves "ckv.input.normal", "ckv.input.focused", "ckv.input.invalid" and
// "ckv.input.disabled" on attach; the segment being edited is drawn reversed
// while focused.
class DatePicker : public ui::View {
public:
    // A Tab stop, empty, with empty allowed, a seed of 2026-01-01, the ISO
    // format and the English labels.
    DatePicker();
    // A date picker is often used for an optional fact (a due date, an end
    // date, a filter bound). Empty is therefore a first-class value rather
    // than a sentinel date. `seed` is the date the first Up/Down edit while
    // empty starts from -- that edit steps the seed's active segment -- and
    // the day the calendar marks as today; callers normally supply their
    // injected notion of today.
    //
    // set_value clamps the year into the drawable range and the month and
    // day into theirs; it never changes the seed. std::nullopt becomes the
    // seed instead when empty is not allowed. It abandons an entry being
    // typed. Any change clears the invalid mark and fires on_change,
    // programmatic or not.
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
    // false itself, and any change of value sets it back to true. valid()
    // answers whether the field is drawn valid: the verdict holds and no
    // refused entry stands.
    void set_valid(bool valid);
    bool valid() const noexcept { return valid_ && refusal_.empty(); }
    // How dates are written, shown and read (see DateFormat). Asserts that
    // `order` names each field once. The active segment is kept by field,
    // so the arrows go on editing the same part of the date.
    void set_format(DateFormat format);
    const DateFormat& format() const noexcept { return format_; }
    // The words the field and its calendar show: the month names (for a
    // format that writes them and for the calendar), the weekday headings,
    // the no-date text and the reason given for a refused entry.
    void set_labels(DateTimeLabels labels);
    const DateTimeLabels& labels() const noexcept { return labels_; }
    // Whether a typed entry is open, and its text so far (empty when not).
    bool editing() const noexcept { return entry_.has_value(); }
    std::string_view entry() const noexcept { return entry_ ? std::string_view(*entry_) : std::string_view{}; }
    // Commits the entry being typed, as Enter does, and returns whether it
    // was taken; true when none was open.
    bool commit_entry();
    // Abandons the entry being typed, as Escape does, clearing a refusal.
    void cancel_entry();
    // Why the entry being typed was refused, or empty while no refusal
    // stands. Editing the entry again clears it until the next commit.
    const std::string& validation_message() const noexcept { return refusal_; }
    // Supplies the explicit popup host used by Space or the dropdown
    // affordance. Without a host, DatePicker remains a standalone segmented
    // editor and performs no hidden application or desktop lookup. The picker
    // keeps pointers to both, so both must outlive it.
    void set_calendar_host(ui::Application& app, Desktop& desktop) noexcept;
    // Opens the calendar below the field, on the value's month (the seed's
    // when empty), with the value selected, the seed marked as today, and the
    // picker's labels. A day chosen there becomes the value; the dropdown
    // stays open until dismissed. Returns false without a host, and true
    // when a calendar is open, including one that already was.
    bool open_calendar();
    // Dismisses an open calendar without choosing: the value stays as it
    // is. What an owner calls when it ends the edit the calendar belongs to,
    // so the calendar never outlives the field it edits. A no-op when none is
    // open.
    void close_calendar();
    bool calendar_open() const noexcept { return calendar_dropdown_ != nullptr; }
    // Fires with the new value after every change, from the reader or from
    // set_value and set_empty_allowed.
    std::function<void(std::optional<DateValue>)> on_change;
    // Fires with the reason each time a typed entry is refused.
    std::function<void(const std::string&)> on_invalid;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    // The caret at the end of an entry being typed, while focused.
    std::optional<CursorState> cursor_state() const override;

private:
    void adjust_active(int delta);
    void select_field_at(int x);
    void edit_entry(std::string entry);

    std::optional<DateValue> value_{};
    DateValue seed_{2026, 1, 1};
    DateField active_field_ = DateField::Year;
    bool empty_allowed_ = true;
    bool valid_ = true;
    DateFormat format_{};
    DateTimeLabels labels_{};
    std::optional<std::string> entry_{};
    std::string refusal_;
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
    // False shows the hour as 01..12 followed by a space and the meridiem
    // word; the value itself stays on the twenty-four-hour scale.
    void set_24_hour(bool enabled);
    // The words for the two halves of a twelve-hour day, "AM" and "PM" by
    // default -- the host's, since ckVision carries no locale data. An empty
    // word leaves the suffix off.
    void set_meridiem_labels(std::string am, std::string pm);
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
    std::string am_label_ = english_date_time_labels().am;
    std::string pm_label_ = english_date_time_labels().pm;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// Why a SpinBox refused a typed entry: text that is not a whole number, or a
// number outside the box's range.
enum class SpinBoxRefusal { NotANumber, OutOfRange };

// A one-row integer field drawn as "< 42 >". Up or Right steps the value up,
// Down or Left steps it down, a left click on the right half of the field
// steps up, on the left half down, and the wheel steps up when turned away
// from the reader and down when turned toward. Where the field is too narrow
// the arrows are dropped first, and a number that still does not fit is
// elided, never cut.
//
// An editable box (set_editable) also takes a typed number. A printable
// character opens an entry holding it, drawn in place of the value with the
// caret at its end; further characters and text input append to it, and
// Backspace takes back its last grapheme -- or, with no entry open, opens one
// holding the value less its last digit. Enter, or the focus leaving the
// box, commits the entry: an optional sign and digits naming a number within
// the range become the value. Anything else is refused, and never clamped:
// the entry stays as typed, the box is drawn invalid, validation_message()
// gives the reason and on_invalid fires with it, as a dialog's veto names
// what is wrong. Editing the entry clears the mark until the next commit;
// Escape abandons the entry and shows the value again. An arrow, a click or
// the wheel commits the entry first and steps from what it committed, and
// does nothing more when the entry is refused.
//
// Resolves "ckv.input.normal", "ckv.input.focused", "ckv.input.invalid" and
// "ckv.input.disabled" on attach.
class SpinBox : public ui::View {
public:
    // A Tab stop over 0..100, step 1, at 0, not editable.
    SpinBox();
    // The inclusive bounds; given the wrong way round they are swapped. The
    // value is clamped into the new range, which fires on_change if it moves.
    // An entry being typed is kept, and judged against the new range when it
    // is committed.
    void set_range(int minimum, int maximum);
    int minimum() const noexcept { return minimum_; }
    int maximum() const noexcept { return maximum_; }
    // How far one key press or click moves the value; at least 1. Steps are
    // not aligned to multiples of it, and the last step is clamped at the
    // range's end.
    void set_step(int step);
    // The owner's value, clamped into the range: an owner setting a value is
    // not a reader typing one, so nothing is refused. Abandons an entry
    // being typed.
    void set_value(int value);
    int value() const noexcept { return value_; }
    // Whether the reader may type a number. Off by default; turning it off
    // abandons an entry being typed.
    void set_editable(bool editable);
    bool editable() const noexcept { return editable_; }
    // Whether a typed entry is open, and its text so far (empty when not).
    bool editing() const noexcept { return entry_.has_value(); }
    std::string_view entry() const noexcept { return entry_ ? std::string_view(*entry_) : std::string_view{}; }
    // Commits the entry being typed, as Enter does, and returns whether it
    // was taken; true when none was open.
    bool commit_entry();
    // Abandons the entry being typed, as Escape does, clearing a refusal.
    void cancel_entry();
    // False while a refused entry stands, and validation_message() then
    // says why; true, with an empty message, otherwise.
    bool valid() const noexcept { return refusal_.empty(); }
    const std::string& validation_message() const noexcept { return refusal_; }
    // The reasons given for a refused entry, from the kind of refusal and
    // the range in force. The default is English: "Not a whole number." and
    // "Enter a number from <minimum> to <maximum>.", the numbers written in
    // plain ASCII digits. An empty function restores it.
    void set_refusal_text(std::function<std::string(SpinBoxRefusal, int minimum, int maximum)> text);
    // Fires with the new value after every change, from the reader or from
    // set_value and set_range.
    std::function<void(int)> on_change;
    // Fires with the reason each time a typed entry is refused.
    std::function<void(const std::string&)> on_invalid;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    // The caret at the end of an entry being typed, while focused.
    std::optional<CursorState> cursor_state() const override;

private:
    void adjust(int delta);
    void assign(int value);
    void edit_entry(std::string entry);
    // What draw() shows in a field `width` columns wide, and where in it the
    // entry's text ends: the one layout the drawing and the caret share.
    struct Shown {
        std::string text;
        int text_end = 0;
    };
    Shown shown(int width) const;
    int minimum_ = 0;
    int maximum_ = 100;
    int step_ = 1;
    int value_ = 0;
    bool editable_ = false;
    std::optional<std::string> entry_{};
    std::string refusal_;
    std::function<std::string(SpinBoxRefusal, int, int)> refusal_text_;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// A labelled mark on a Slider's scale at a value the host chooses.
struct SliderTick {
    // Where the mark stands, in the slider's own units, and the text under
    // it. A value outside the range is not drawn.
    int value = 0;
    std::string label;
};

// A horizontal slider: a track with the part up to the value filled and a
// thumb at the value's position ("◆" while focused, "●" otherwise).
// Left/Right step the value, Home/End jump to the minimum and maximum, and a
// left click sets the value in proportion to where it lands between the
// first and last column, without snapping to the step.
//
// With ticks (set_ticks) the slider is two rows high: each tick in the range
// marks its column on the track ("┬", "┯" on the filled part; the thumb
// stands over a mark it shares a column with), and its label is drawn on the
// row below, centred on that column and moved inward as far as it must to
// stay inside the slider. Labels never overlap and always keep one blank
// between them: they are placed in the order the ticks were given, and a
// label that would collide with one already placed is left out, while its
// mark stays. So a host lists first the labels that must show -- the two
// ends, typically -- and the choice of the rest is the same on every run.
// A label wider than the slider is elided to its width.
//
// Resolves "ckv.list.normal" for the track and the labels,
// "ckv.menu.bar.active" for the filled part and thumb, and
// "ckv.list.disabled" on attach.
class Slider : public ui::View {
public:
    // A Tab stop over 0..100, step 1, at 0, with no ticks.
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
    // The labelled ticks, in priority order (see the class comment). An empty
    // list takes them away and the slider back to one row; a change repaints
    // and reports a size-hint change.
    void set_ticks(std::vector<SliderTick> ticks);
    const std::vector<SliderTick>& ticks() const noexcept { return ticks_; }
    // Fires with the new value after every change, from the reader or from
    // set_value and set_range.
    std::function<void(int)> on_change;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    // Two rows, exactly, with ticks; without them, the size asked for.
    ui::SizeHint vertical_size_hint() const override;

private:
    void adjust(int delta);
    // The value a press at column `x` sets, and the column the thumb is drawn
    // in for the current value: the one proportional map, both ways.
    int value_from_x(int x) const;
    int x_from_value(int value) const;
    int minimum_ = 0;
    int maximum_ = 100;
    int step_ = 1;
    int value_ = 0;
    std::vector<SliderTick> ticks_;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId fill_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// A one-row search field: a "Search " prompt, the query field, the host's
// status text, and a "[x]" clear control at the right edge while there is a
// query. The query field is an InputLine (the widget catalog: "InputLine plus
// clear/status affordance") and it is the box's focus stop: it edits the
// query with everything a one-line field offers -- caret movement,
// Shift-extended and mouse selection, the clipboard and undo keys, and the
// shared history -- and shows its caret and selection as any input line does.
// When the query is wider than the field it scrolls to keep the caret in view.
//
// The box itself is the field's container and takes no focus of its own. The
// keys the field leaves unhandled reach it: Escape clears a query, and with
// nothing to clear is left unhandled for the enclosing dialog or popup; Enter
// records the query in the history and is left unhandled for whatever else
// answers it. A press on the "[x]" clears the query; a press elsewhere on the
// box (the prompt or the status) puts the keyboard in the field.
//
// The status is what the search found, in the host's words -- "3 of 12",
// "no match" -- drawn right-aligned between the field and the clear control,
// one blank before it, in the label colours. The box only shows it: the host
// counts, and sets it again after each on_change. The field keeps at least
// kMinimumQueryColumns; a status that would take more is elided, and one
// with fewer than two columns left is not drawn.
// With a history key, Up and Down recall earlier queries (set_history_key).
//
// Resolves "ckv.label.text" and "ckv.label.disabled" on attach for the
// prompt, the status and the clear control; the field resolves the input
// roles as every InputLine does.
class SearchBox : public ui::View {
public:
    // An empty query in a field that is a Tab stop.
    SearchBox();
    // Replaces the query, caret at its end; fires on_change if it differs,
    // programmatic or not.
    void set_query(std::string query);
    // The query as the field holds it.
    std::string query() const { return field_->text(); }
    // Empties the query, firing on_change if it was not empty, then fires
    // on_clear in any case. Escape and the "[x]" control come here.
    void clear();
    // Fires with the whole new query after every change: each typed
    // character, an erase, a cut, paste or undo, a recalled history entry,
    // clearing, and set_query.
    std::function<void(const std::string&)> on_change;
    // Fires after every clear(), including one of an already empty query.
    std::function<void()> on_clear;
    // The status text drawn at the right, empty (the default) for none.
    // Drawn verbatim; setting it repaints and fires nothing.
    void set_status(std::string status);
    const std::string& status() const noexcept { return status_; }
    // The columns the query field keeps however long the status is.
    static constexpr int kMinimumQueryColumns = 6;
    // Names the history list the field shares (InputLine::set_history_key):
    // the list under `key` in the Application::history() of the Application
    // the box is attached to, which every input line, combo box, search box
    // and dialog field naming the same key shares too. While a key is set and
    // the box is attached, Up recalls older queries and Down newer ones, back
    // to the query that was there before recall began, each recall firing
    // on_change as typing does; and Enter records the query, leaving the key
    // unhandled for whatever else answers it. Empty (the default) turns
    // history off.
    void set_history_key(std::string key) { field_->set_history_key(std::move(key)); }
    const std::string& history_key() const noexcept { return field_->history_key(); }
    // Records the query as the newest entry of the history list. An empty
    // query is not recorded, and nothing is without a key or an Application.
    void commit_to_history();

    // The query field: the box's focus stop, and what an owner focuses
    // (Application::set_focus(&box.field())), gives another focus policy, or
    // hands keys to when the typing happens elsewhere -- a list whose
    // type-ahead filters it, say. Edits made through it report on_change like
    // any other. Its bounds belong to the box, which lays it out.
    InputLine& field() noexcept { return *field_; }
    const InputLine& field() const noexcept { return *field_; }

    void draw(scene::Painter& painter) override;
    // Escape and Enter as they bubble up from the field (see above).
    bool on_key(const KeyEvent& event) override;
    // Presses outside the field: the clear control, and the rest of the box.
    bool on_mouse(const MouseEvent& event) override;
    void on_resized() override;
    void on_attached() override;

private:
    // Where the parts fall across the box's width, left to right: the
    // prompt, the query field, the status (with its leading blank) and the
    // clear control. Drawing, the field's placement and hit-testing all read
    // this one layout, which keeps what the reader sees and what answers a
    // click the same thing.
    struct Layout {
        int prompt_width = 0;
        int field_x = 0;
        int field_width = 0;
        // The status's columns, its leading blank included; they end where
        // the clear control begins.
        int status_width = 0;
        int clear_x = 0;
        int clear_width = 0;
    };
    Layout layout() const;
    // Puts the field where layout() says it goes. The layout changes with
    // the width, the status and whether there is a query to clear.
    void place_field();
    // What every change of query does: the field is placed again (the clear
    // control comes and goes with the query) and on_change hears the query.
    void query_changed();
    // Columns the "[x]" clear control occupies at the right edge. Drawing and
    // hit-testing both derive from this, which is what keeps the control the
    // reader can see and the region that answers a click the same thing.
    static constexpr int kClearControlWidth = 3;

    InputLine* field_ = nullptr;
    std::string status_;
    ui::RoleId label_role_ = ui::kInvalidRole;
    ui::RoleId label_disabled_role_ = ui::kInvalidRole;
};

// A one-row strip of command buttons: the tool bar. Its items are
// CommandPresentations -- the value a menu row (MenuItem::command) and a
// status-line item present a command with -- so everything a button says
// comes from where the menus and the status line get it: the label is the
// presentation's or the registered title, with its '&' mnemonic accented; the
// chord is the presentation's or the registry's (shown with
// set_show_chords); a button is drawn and acts as available exactly when its
// command is (CommandRegistry::is_available, for the focus the reader is
// working in); and a toggle command (CommandRegistry::set_checked_predicate)
// carries its state in a mark column, "[x Wrap]" on and "[  Wrap]" off, as a
// menu row marks it. A button whose command the registry does not know, or
// any button while the bar is detached, is drawn "[(unknown)]" and inert.
//
// Overflow. Buttons are laid out left to right, one blank apart, in order.
// When they do not all fit, an overflow control "[»]" takes the right edge
// and every button from the first that does not fit before it goes into its
// menu: a DropdownMenu of the same presentations, hanging below the bar (or
// above it, for a bar docked at the bottom), with their chords and marks.
// The menu needs a Desktop above the bar.
//
// Keyboard. The bar is a Tab stop, and activate() hands it the keyboard from
// wherever the reader is -- the way F10 reaches a menu bar -- for a bar
// docked on a Desktop, which no Tab walk reaches. While it has the keyboard,
// Left and Right walk the buttons and the overflow control, wrapping, Home
// and End jump to the ends, and Enter or Space activates the one walked to:
// an available button runs its command, an unavailable one does nothing, and
// the overflow control opens its menu. A button's mnemonic letter, alone or
// with Alt, runs it directly, overflowed or not. Escape ends an activate()
// walk. Choosing a button, or opening the overflow menu, ends a walk too: the
// focus goes back where it was, and the command runs for that focus, as a
// menu's command does.
//
// Pointer. A left press on a button or the overflow control shows it pressed
// and acts when released over it, like any button; dragged off, it is taken
// back. A press on a bar that did not have the keyboard is a walk of its own:
// the bar holds the focus only while the button is down, then hands it back
// before the command runs, so clicking a docked tool bar never takes the
// keyboard away from the document it acts on.
//
// Docking. A tool bar docks to either edge of a Desktop (Desktop::dock, or
// ApplicationShellOptions::tool_bar), beside the menu bar or the status line
// already there. Resolves "ckv.menu.bar.normal", "ckv.menu.bar.active" (the
// walked and the pressed button), "ckv.menu.dropdown.disabled" and
// "ckv.hotkey" on attach; always one row high.
class ToolBar : public ui::View {
public:
    // An empty bar; a Tab stop.
    ToolBar();

    // The buttons, left to right. Moves the keyboard back to the first.
    void set_items(std::vector<CommandPresentation> items);
    const std::vector<CommandPresentation>& items() const noexcept { return items_; }
    // Whether each face also states its chord, "[Save Ctrl+S]". Off by
    // default: a tool bar is the compact surface, and the menus and the
    // overflow menu state chords already.
    void set_show_chords(bool show);
    bool show_chords() const noexcept { return show_chords_; }

    // The items shown on the bar, and those in the overflow menu, at the
    // current width; indices into items().
    std::vector<std::size_t> shown_items() const;
    std::vector<std::size_t> overflow_items() const;
    // The item the keyboard is on, or std::nullopt while it is on the
    // overflow control -- including when the item it was on has overflowed.
    std::optional<std::size_t> focused_item() const;

    // Gives the bar the keyboard, remembering the focus it came from and that
    // focus's command contexts, and puts the walk on the first button. The bar
    // must be attached under an Application. On a bar that already has the
    // keyboard it only moves the walk back to the first button.
    void activate();
    // Ends a walk activate() began: the focus goes back where it came from.
    void deactivate();

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Every button on it runs something.
    std::optional<PointerShape> pointer_shape_at(Point) const override { return PointerShape::Pointer; }
    // Losing the keyboard ends a walk however it happened, forgetting the
    // focus it would have handed back.
    void on_focus(const FocusEvent& event) override;
    // At least the overflow control, preferring every button, free to grow;
    // exactly one row.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    void on_attached() override;

private:
    // One thing on the bar: a button, or the overflow control, which has no
    // item.
    struct Slot {
        std::optional<std::size_t> item;
        int x = 0;
        int width = 0;
    };
    struct Layout {
        std::vector<Slot> slots;
        std::vector<std::size_t> overflow;
    };
    // A walk: the focus to hand back when it ends, and that focus's command
    // contexts, which every button is judged and run against meanwhile.
    struct Walk {
        ui::Application::FocusBookmark focus;
        std::vector<std::string> contexts;
    };
    Layout layout() const;
    // The item's face text, its brackets and mark column included, and its
    // label as parsed for the mnemonic accent.
    std::string face(std::size_t item) const;
    MnemonicText label(std::size_t item) const;
    std::string chord(std::size_t item) const;
    bool available(std::size_t item) const;
    std::size_t focused_slot(const Layout& bar) const;
    void focus_slot(const Layout& bar, std::size_t slot);
    int slot_at_x(const Layout& bar, int x) const;
    void begin_walk();
    // Ends the walk, handing the focus back; returns the contexts it held,
    // or std::nullopt when there was none.
    std::optional<std::vector<std::string>> end_walk();
    // Enter, Space, a completed click or a mnemonic: runs the item's command
    // or opens the overflow menu. Returns whether anything was done.
    bool activate_slot(const Layout& bar, std::size_t slot);
    bool run_item(std::size_t item);
    void open_overflow(const Layout& bar);

    std::vector<CommandPresentation> items_;
    bool show_chords_ = false;
    std::size_t focused_ = 0;
    // The keyboard is on the overflow control rather than on focused_.
    bool overflow_focused_ = false;
    std::optional<Walk> walk_;
    // The slot a pointer press went down on, and whether the pointer is
    // still over it.
    std::optional<std::size_t> pressed_slot_;
    bool pressed_visible_ = false;
    // The press in flight began the walk, borrowing the keyboard, and so
    // ends it.
    bool press_owns_walk_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId active_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId hotkey_role_ = ui::kInvalidRole;
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
// parentheses, and a scroll gutter when they do not all fit. A command that
// applies where the palette answers but is disabled is listed in the
// disabled style, and the highlight passes over it as a menu's does. Typed
// characters (without Alt, Ctrl or Super) append to the query, Backspace
// removes its last grapheme, Up/Down and the wheel move the highlight, and
// Enter or a left click on an enabled row executes that command, within the
// invocation contexts when they are set. Everything is read from the
// registry as it is drawn, so enablement, chords, retraction and rebinding
// show at once. A detached palette lists nothing.
//
// Embedded in a surface of its own, the palette does not close itself: its
// host does that. show_command_palette presents it as a popup instead, which
// is what on_dismiss is for. Resolves "ckv.input.normal",
// "ckv.option.normal", "ckv.option.focused" and "ckv.option.disabled" on
// attach.
class CommandPalette : public ui::View {
public:
    // A Tab stop with an empty query, preferring 40 x 8 cells.
    CommandPalette();
    // Replaces the query and moves the highlight back to the first enabled
    // result. The match is a case-insensitive (ASCII) substring of the title
    // as displayed, mnemonic marker removed; an empty query matches every
    // command. Fires nothing.
    void set_query(std::string query);
    const std::string& query() const noexcept { return query_; }
    // Palette-visible, title-matching and in scope where the palette answers
    // (ui::CommandRegistry::in_scope), enabled or not — in the order the
    // commands were declared.
    std::vector<ui::CommandInfo> filtered_commands() const;
    // The command contexts the palette answers for (ui::command_context_path
    // of the view the reader was on when it opened), instead of its own
    // focus path. A palette presented in a window of its own takes the focus
    // from the place whose commands it lists, exactly as a menu walk does,
    // and without this it would offer what the palette's window allows —
    // no context-bound command at all.
    void set_invocation_contexts(std::vector<std::string> contexts);
    // The command Enter would execute: the highlighted entry of
    // filtered_commands(), or std::nullopt when no enabled command matches.
    std::optional<ui::CommandId> highlighted_command() const;
    // Whether a single-line frame is drawn in the outermost ring of cells,
    // inside the inset, and the palette casts the standard shadow: how it
    // stands as a popup over other content. Off by default.
    void set_framed(bool framed);
    bool framed() const noexcept { return framed_; }
    // When set, the palette dismisses itself as a popup does, calling this
    // once: on Escape, on a left press outside it (which reaches it while it
    // holds the input capture), and before it executes the command Enter or a
    // click chose — so the command runs where the reader was rather than in
    // the palette. show_command_palette sets it to tear the popup down; the
    // palette may be destroyed by the call. Unset, Escape is left to the host.
    std::function<void()> on_dismiss;

    void draw(scene::Painter& painter) override;
    // The caret at the end of the query: the palette is typed into, and the
    // caret is what says so.
    std::optional<CursorState> cursor_state() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    // True while framed.
    bool casts_shadow() const noexcept override { return framed_; }

private:
    // The contexts the palette answers for: the invocation contexts when set,
    // otherwise its own place.
    std::vector<std::string> answering_contexts() const;
    bool enabled_command(ui::CommandId id) const;
    // The row the highlight is on once disabled rows are passed over: the
    // chosen row if enabled, else the next enabled one, else the previous;
    // std::nullopt when no row is enabled.
    std::optional<std::size_t> resolved_highlight(const std::vector<ui::CommandInfo>& commands) const;
    // The first result row shown, so the highlighted row is in view.
    std::size_t first_visible(std::size_t highlighted) const;
    void move_highlight(int delta);
    // Executes `id` (dismissing first when on_dismiss is set) and returns
    // whether its handler ran.
    bool run(ui::CommandId id);
    void dismiss();

    std::string query_;
    std::size_t highlighted_ = 0;
    std::optional<std::vector<std::string>> invocation_contexts_;
    bool framed_ = false;
    bool dismissed_ = false;
    ui::RoleId input_role_ = ui::kInvalidRole;
    ui::RoleId result_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// Opens a framed CommandPalette as a popup over `desktop`, the default handler
// of the standard command_palette command (Ctrl+Shift+P). It is centred near
// the top of the desktop's content area, up to 56 x 14 cells, and lists what
// the view focused when it opened allows (set_invocation_contexts with that
// view's context path). It is modal, holds the input capture and takes the
// focus; Escape, a press outside it, or running a command dismisses it, which
// releases the capture and the modal scope, gives the focus back to the view
// that had it, and destroys the palette. Returns the palette, valid until
// then.
CommandPalette* show_command_palette(ui::Application& app, Desktop& desktop);

// A one-row path of segments, such as the folders leading to a file, drawn
// left to right with a separator between them. Left/Right move the focused
// stop, highlighted while the bar has focus, and Enter activates it; a left
// click on a segment focuses and activates it. Resolves "ckv.label.text",
// "ckv.list.selected" and "ckv.label.disabled" on attach.
//
// Elision. When the whole path does not fit, the first and the last segment
// stay, the segments nearest the last are kept as the room allows, and the
// run between them is drawn as one "…" stop. The ellipsis is walked onto
// like a segment; Enter on it, or a click, opens a menu of the segments it
// stands for, and choosing one there activates it as Enter on a shown
// segment would (the menu needs a Desktop above the bar). Too narrow for even
// the first segment, the ellipsis and the last, the two segments are elided
// themselves, the last keeping the larger share of the room.
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
    // Fires with the index of the segment activated, by Enter, a click or the
    // ellipsis's menu, so the host can navigate to that level. Enter is not
    // consumed, and the ellipsis opens no menu, while this is empty.
    std::function<void(std::size_t)> on_activate;

    // The segments the ellipsis stands for at the current width, first to
    // last; empty while the whole path fits.
    std::vector<std::size_t> hidden_segments() const;
    // The segment the keyboard is on, or std::nullopt while it is on the
    // ellipsis -- including when the segment it was on has been elided into
    // it by a narrower width.
    std::optional<std::size_t> focused_segment() const;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    // One thing drawn on the bar: a segment (possibly elided to fit) or the
    // ellipsis, which has no segment.
    struct Stop {
        std::optional<std::size_t> segment;
        int x = 0;
        std::string text;
    };
    // The stops at the current width, left to right, and the indices the
    // ellipsis stands for (empty when there is none).
    struct Layout {
        std::vector<Stop> stops;
        std::vector<std::size_t> hidden;
    };
    Layout layout() const;
    // Which stop carries the focus in `bar`.
    std::size_t focused_stop(const Layout& bar) const;
    void focus_stop(const Layout& bar, std::size_t stop);
    int stop_at_x(const Layout& bar, int x) const;
    // Enter or a click on the stop: activates a segment, or opens the
    // ellipsis's menu. Returns whether it acted.
    bool activate_stop(const Layout& bar, std::size_t stop);
    void activate_segment(std::size_t index);
    std::vector<std::string> segments_;
    std::string separator_ = "/";
    std::size_t focused_ = 0;
    // The keyboard is on the ellipsis rather than on focused_.
    bool ellipsis_focused_ = false;
    // A pointer press went down on the ellipsis; its release opens the menu.
    bool ellipsis_pressed_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// What a PropertyItem holds. The kind decides how the value is shown, which
// editor changes it, and what that editor accepts. Every kind's value is
// text in one canonical spelling, so a host reads any of them the same way
// and parses the ones it needs (parse_iso_date, parse_iso_time, and so on).
enum class PropertyKind {
    // Free text, edited in an InputLine; only the item's `validate` can refuse
    // it.
    Text,
    // "true" or "false", shown as a check box, "[X]" or "[ ]", and toggled in
    // place by Enter, Space or a click on it: there is nothing to type.
    Bool,
    // One of the item's `choices`, picked from a pick-only ComboBox whose list
    // drops as the edit begins.
    Choice,
    // A whole number, typed in an InputLine. Refused unless all of it reads as
    // one and it lies within `minimum`..`maximum`; committed in its plain
    // decimal spelling ("007" becomes "7").
    Integer,
    // A decimal number, likewise, in the "C" locale's spelling whatever the
    // host's; committed in its shortest exact spelling ("1.50" becomes "1.5").
    // Infinities and NaN are refused.
    Real,
    // A calendar date, YYYY-MM-DD, edited with a DatePicker; a value that does
    // not parse starts the picker on 2026-01-01.
    Date,
    // A time of day, HH:MM or HH:MM:SS, edited with a TimePicker that shows
    // the seconds exactly when the value has them.
    Time,
};

// One row of a PropertyInspector: a name, a value, and what kind of value it
// is.
struct PropertyItem {
    // The label, and the value as text; the inspector edits the value in
    // place.
    std::string name;
    std::string value;
    // Whether the value may be edited. A read-only row still takes the cursor.
    bool editable = true;
    // Every member from here down has an initializer, so a row written
    // positionally as {name, value, editable} is a Text row.
    PropertyKind kind = PropertyKind::Text;
    // Choice only: what there is to choose from, in the order the list shows.
    std::vector<std::string> choices{};
    // Integer and Real only: the inclusive bounds, where a bound is real.
    std::optional<double> minimum{};
    std::optional<double> maximum{};
    // A check of the caller's, run on commit after the kind's own with the
    // canonical text: std::nullopt accepts, and anything else is the reason
    // the edit is refused, shown under the row.
    std::function<std::optional<std::string>(const std::string&)> validate{};
};

// The reasons a PropertyInspector gives when it refuses a number itself; an
// item's `validate` supplies its own. English by default, replaced whole with
// PropertyInspector::set_messages. `at_least` and `at_most` are followed by a
// space and the bound.
struct PropertyInspectorMessages {
    // An Integer row whose text is not a whole number.
    std::string whole_number = "Must be a whole number";
    // A Real row whose text is not a finite decimal number.
    std::string number = "Must be a number";
    // A number below `minimum`, and one above `maximum`.
    std::string at_least = "Must be at least";
    std::string at_most = "Must be at most";
};

// A two-column list of properties: the names in a column as wide as the
// widest (up to half the view), the values aligned after a two-cell gutter,
// one row each from the top, with the cursor row drawn as a full-width bar
// (muted while neither the inspector nor its editor holds the keyboard).
// Rows below the view's height are not drawn: there is no scrolling.
//
// Up/Down move the cursor. Enter or F2 on an editable row, or a click on its
// value, edits it in place with the editor its kind calls for — an
// InputLine, a pick-only ComboBox, a DatePicker or a TimePicker, laid over
// the value column and given the keyboard — except that a Bool row toggles
// at once. While editing, the keys go to the editor first; what it leaves
// comes here: Enter commits, as does Tab or Up/Down on its way elsewhere, and
// Escape cancels. Choosing from a Choice row's list commits too. A commit
// checks the value (the kind's rule, then the item's `validate`); a refused
// value keeps the edit open, marks the editor invalid, and shows the reason
// on a row of its own under the edited one until the value is changed again.
// A refused toggle changes nothing and shows its reason the same way.
// Needs an Application to hand the keyboard to an editor: detached, only Bool
// rows can be changed.
//
// A Tab stop. Resolves "ckv.list.normal", "ckv.list.selected",
// "ckv.list.selected.inactive", "ckv.list.disabled" and "ckv.input.invalid"
// (the reason) on attach; the editors resolve their own.
class PropertyInspector : public ui::View {
public:
    // A Tab stop with no rows, its editors built and hidden.
    PropertyInspector();
    // Replaces the rows, puts the cursor on the first and cancels any edit.
    void set_items(std::vector<PropertyItem> items);
    // The rows, with every committed edit.
    const std::vector<PropertyItem>& items() const noexcept { return items_; }
    // The index of the cursor row, or -1 when there are no rows.
    int cursor() const noexcept { return cursor_; }
    // Replaces the reasons the inspector gives of its own accord.
    void set_messages(PropertyInspectorMessages messages);

    // Whether an editor is open on the cursor row.
    bool editing() const noexcept { return editor_ != nullptr; }
    // Why the last commit or Bool toggle was refused; empty when it was not.
    // Cleared by any change to the value being edited, and by whatever the
    // reader does next with the inspector.
    const std::string& validation_message() const noexcept { return reason_; }
    // Opens the cursor row's editor, or toggles a Bool row (subject to its
    // `validate`). Returns false for a read-only row, with no rows, while
    // already editing, or — for every kind but Bool — without an Application.
    bool begin_edit();
    // Checks the edited value and, when it passes, stores it, closes the
    // editor, gives the keyboard back to the inspector and fires on_change if
    // the value changed. Returns whether it passed; false too when nothing is
    // being edited.
    bool commit_edit();
    // Closes the editor without storing anything.
    void cancel_edit();
    // Fires once per committed change of a value — an edit that passed its
    // check and left the value different, or a Bool toggle — with the row's
    // index and its new canonical text. Not fired by set_items.
    std::function<void(std::size_t, std::string)> on_change;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    void on_resized() override;

private:
    // The value column's first cell and the screen row of item `index`,
    // which a shown reason pushes down for every row after the edited one.
    int value_x() const;
    int row_of(std::size_t index) const noexcept;
    // The item drawn on view row `y`, or -1 for the reason row and past the
    // last item.
    int item_at_row(int y) const noexcept;
    // The edited value as canonical text, before any check.
    std::string editor_text() const;
    // The kind's own check, then the item's; the reason, or std::nullopt.
    // `text` is rewritten to its canonical spelling when it passes.
    std::optional<std::string> check(const PropertyItem& item, std::string& text) const;
    void toggle(std::size_t index);
    void place_editor();
    void close_editor();
    void refuse(std::string reason);
    void clear_reason();
    // Commits an open edit (true when there was none), then moves the cursor.
    bool move_cursor_to(int index);

    std::vector<PropertyItem> items_;
    int cursor_ = -1;
    PropertyInspectorMessages messages_;
    // The editors, built once and shown one at a time. They are never
    // destroyed mid-edit, so an editor's own callback may end the edit it is
    // part of.
    InputLine* text_editor_ = nullptr;
    ComboBox* choice_editor_ = nullptr;
    DatePicker* date_editor_ = nullptr;
    TimePicker* time_editor_ = nullptr;
    // The editor open on the cursor row, or nullptr.
    ui::View* editor_ = nullptr;
    // Why the last commit or toggle was refused, shown under item
    // `reason_index_`; empty when nothing is refused.
    std::string reason_;
    int reason_index_ = -1;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId selected_inactive_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId reason_role_ = ui::kInvalidRole;
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

// How a wizard ended: the reader finished it on its last page, or cancelled
// it -- by Escape, the Cancel control, or, for a presented wizard, its
// window's close control or the application quitting.
enum class WizardOutcome { Finished, Cancelled };

// The words a Wizard draws, in the language the host writes them in. The
// default is English.
struct WizardLabels {
    // The navigation controls on the bottom row.
    std::string back = "< Back";
    std::string next = "Next >";
    std::string finish = "Finish";
    std::string cancel = "Cancel";
    // The step indicator for page `current` (counted from 1) of `count`,
    // "Step 2 of 4" by default. An empty function, or empty text, shows none.
    std::function<std::string(std::size_t current, std::size_t count)> step =
        [](std::size_t current, std::size_t count) {
            return "Step " + std::to_string(current) + " of " + std::to_string(count);
        };
};

// A multi-step flow: the current page's title on the top row with the step
// indicator ("Step 2 of 4") at its right end, the current page's content
// below it, and on the bottom row the navigation -- "< Back" while there is
// a page before this one, then "Next >" or, on the last page, "Finish", and
// "Cancel" at the right end. The forward action is greyed while the page's
// can_continue refuses, and reversed while the wizard itself has focus.
//
// Right or Enter goes next, or finishes on the last page; Left goes back;
// Escape cancels. Keys a page's own controls leave unused reach the wizard
// the same way, so Enter in a page's last field moves on. A left click on a
// navigation control does what its key does. Finishing and cancelling end
// the flow through on_complete, typed by WizardOutcome; present_modal_wizard turns
// that into a typed, non-blocking DialogPresentation.
//
// Each page may have content of its own (set_page_content), which the wizard
// owns and lays out between its top and bottom rows, showing only the
// current page's. When the page changes while the focus is inside the page
// being left, the focus moves to the first control of the page arriving, or
// to the wizard when it has none. A page without content leaves those rows
// for the host to draw over.
//
// Becomes a Tab stop when attached. Resolves "ckv.dialog.background",
// "ckv.window.title.active" and "ckv.label.disabled" on attach.
class Wizard : public ui::View {
public:
    // Replaces the pages and returns to the first. Content set for the old
    // pages is destroyed.
    void set_pages(std::vector<WizardPage> pages);
    // The number of pages, and the zero-based index of the current one.
    std::size_t page_count() const noexcept { return pages_.size(); }
    std::size_t current_page() const noexcept { return current_page_; }
    // Makes `content` the view shown under the title while page `page` is
    // current, destroying any content that page had, and returns it. Asserts
    // that `page` exists.
    ui::View* set_page_content(std::size_t page, std::unique_ptr<ui::View> content);
    // Page `page`'s content, or nullptr when it has none or does not exist.
    ui::View* page_content(std::size_t page) const noexcept;
    // Replaces the words the wizard draws.
    void set_labels(WizardLabels labels);
    const WizardLabels& labels() const noexcept { return labels_; }
    // Whether next() would move: there is a later page and the current
    // page's can_continue allows it.
    bool can_go_next() const;
    // Whether back() would move: the current page is not the first.
    bool can_go_back() const noexcept { return current_page_ > 0; }
    // Move one page forward or back when allowed; false, with nothing
    // changed, when not. Neither fires a callback.
    bool next();
    bool back();
    // On the last page, when its can_continue allows it, fires on_complete
    // with WizardOutcome::Finished and returns true. Anywhere else, or with
    // no pages, returns false. The page stays where it is either way.
    bool finish();
    // Fires on_complete with WizardOutcome::Cancelled and returns true;
    // returns false, doing nothing, while on_complete is empty.
    bool cancel();
    // The end of the flow, finished or cancelled; see WizardOutcome. Escape
    // is not consumed while this is empty. present_modal_wizard sets it: a caller
    // of that function leaves it alone and awaits the presentation.
    std::function<void(WizardOutcome)> on_complete;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    void on_resized() override;
    // Wide enough for the widest page content, the navigation row and the
    // widest title with its step indicator; tall enough for the tallest
    // content between the two rows.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

private:
    // The navigation controls' columns on the bottom row, one layout for
    // drawing and for hit-testing.
    struct NavigationLayout {
        int back_x = 0;
        int action_x = 0;
        int action_width = 0;
        int cancel_x = 0;
    };
    NavigationLayout navigation_layout() const;
    bool last_page() const noexcept { return pages_.empty() || current_page_ + 1 >= pages_.size(); }
    bool page_allows_leaving() const;
    void show_page(std::size_t page);

    std::vector<WizardPage> pages_;
    std::vector<ui::View*> contents_;
    std::size_t current_page_ = 0;
    WizardLabels labels_{};
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

// The caller's handle on a presented wizard, completing once with how it
// ended after its window has detached.
using WizardPresentation = DialogPresentation<WizardOutcome>;

// Presents `wizard` modally in a dialog window titled `title`, without
// starting a nested loop (D-038). The window is sized to the wizard's own
// size hints, clamped to the desktop, and opens with the focus on the first
// control of the current page, or on the wizard when the page has none.
// Finishing records WizardOutcome::Finished and closes the window; Escape,
// Cancel, the close control, an external detach and a host quit all complete
// with WizardOutcome::Cancelled. This function sets the wizard's
// on_complete; the caller reads each page's own controls once the
// presentation completes with Finished, and must keep what the page
// callbacks capture alive until then.
[[nodiscard]] WizardPresentation present_modal_wizard(std::unique_ptr<Wizard> wizard, std::string title,
                                                      ui::Application& app, Desktop& desktop,
                                                      const ui::StandardRoles& roles);

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
// role (resolved on attach): the popup a TooltipController shows, and a
// plain floating note a host may position itself. It takes no focus. While
// it is not held it takes no input either; held, it answers keys and
// pointer presses the way an open menu does (see set_held).
class Tooltip : public ui::View {
public:
    // Hidden until shown; prefers the text's width plus two by one row.
    explicit Tooltip(std::string text = {});
    // Replaces the text and the preferred size. A tooltip already shown keeps
    // its bounds until it is shown again, eliding the text to fit them.
    void set_text(std::string text);
    const std::string& text() const noexcept { return text_; }
    // Shows the tooltip with its top-left cell at `position`, in its parent's
    // coordinates, sized to the text plus padding (at least two cells). It is
    // not moved to fit inside the parent.
    void show_at(Point position);
    // Shows the tooltip beside `anchor`, kept inside `area`; both rectangles
    // are in its parent's coordinates. The placement is a fixed rule, so the
    // same geometry always puts it in the same place: on the row below the
    // anchor when that row is inside the area, else on the row above it when
    // that one is, else over the anchor's bottom row, kept inside the area.
    // Its left edge starts under the anchor's and moves left as far as it
    // must to end inside the area, and never past the area's left edge. It
    // is as wide as its text and padding, and never wider than the area,
    // eliding the text to fit.
    void show_near(Rect anchor, Rect area);
    // Hides it; the text and bounds are kept.
    void hide();
    // Whether it is visible.
    bool shown() const noexcept { return visible(); }
    // Whether it answers input the way an open menu does. Held, every key
    // press, text input and pointer press -- inside it or outside -- fire
    // on_dismiss and are consumed, except a press outside it, which fires
    // on_dismiss and is left unhandled, as a menu leaves one. A host that
    // holds a tooltip first routes input to it, with a modal scope for the
    // keys and input capture for the pointer; TooltipController does this
    // for the tooltip key.
    void set_held(bool held) noexcept { held_ = held; }
    bool held() const noexcept { return held_; }
    // Fires on a dismissing key or press while held. The host hides or
    // removes the tooltip from here; the tooltip does nothing else itself.
    std::function<void()> on_dismiss;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    int shown_width() const noexcept;
    void dismiss();
    std::string text_;
    bool held_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
};

// Shows explanatory tooltips for an application's views. A host tells it
// the tip of each view it wants explained (set_tip); the controller then
// shows the tip of the view the reader turns to, as a Tooltip popup on the
// desktop, placed with Tooltip::show_near.
//
// It shows a tip in three ways. When the pointer comes to rest over a view,
// or the keyboard focus arrives on one, and that view or the nearest of its
// ancestors has a tip, the tip shows once the delay (set_delay) has passed
// on the Application's injected clock, unless the reader has moved on
// meanwhile: under the pointer's cell for the pointer, under the view for
// the focus. Such a tip is passive -- it takes no input, and goes when the
// pointer leaves the view it explains (for the tooltip itself, which the
// reader may move onto to read), when the focus leaves the view it explains,
// or at the next key press, pointer press, wheel turn or text input.
//
// And the tooltip command (CommandRegistry::standard().tooltip, Ctrl+F1 by
// default) shows the focused view's tip at once, under the view. That tip
// is held: it keeps a modal scope and the pointer the way an open menu does,
// so Escape -- or any other key -- or a press anywhere dismisses it, and
// nothing else reaches the application until it has gone. The command is
// available inside a modal dialog as well.
//
// The controller learns of the focus, the pointer and the input through
// Application::add_attention_observer, and keeps only lifetime-checked
// references to the views it explains. It installs the tooltip command's
// handler when nothing has claimed it yet, and withdraws it again on
// destruction if it was the one that installed it.
class TooltipController {
public:
    // Watches `app` and shows its tips over `desktop`. Both must outlive the
    // controller.
    TooltipController(ui::Application& app, Desktop& desktop);
    // Takes any tip down, cancels a pending one and stops watching.
    ~TooltipController();
    // Not copyable: the observers and the command handler it installs name
    // this controller.
    TooltipController(const TooltipController&) = delete;
    TooltipController& operator=(const TooltipController&) = delete;

    // The tip for `view`, and for everything inside it that has none of its
    // own; empty text removes the view's tip. The controller does not keep
    // the view alive and forgets tips whose views are gone.
    void set_tip(const ui::View& view, std::string text);
    // The tip that `view` shows: its own, or the nearest ancestor's; empty
    // when neither it nor any ancestor has one.
    std::string tip_for(const ui::View& view) const;
    // How long the pointer rests on a view, or the focus stays on one,
    // before its tip shows, in nanoseconds of the injected clock. 500 ms by
    // default; a value below one nanosecond is taken as one.
    void set_delay(std::int64_t nanos);
    std::int64_t delay_nanos() const noexcept { return delay_nanos_; }

    // Shows the focused view's tip at once, held, as the tooltip command
    // does. Returns false, showing nothing, when nothing is focused or the
    // focused view has no tip.
    bool show_for_focus();
    // Takes down the tip on show, if any, and forgets a pending one.
    void dismiss();
    // The tooltip on show, or nullptr when none is. Owned by the desktop;
    // valid until the tip goes.
    const Tooltip* tooltip() const noexcept;

private:
    // What asked for the tip on show or pending: the pointer resting, the
    // focus arriving, or the tooltip command.
    enum class Source { Pointer, Focus, Command };
    struct Tip {
        const ui::View* view = nullptr;
        std::weak_ptr<void> liveness;
        std::string text;
    };
    const Tip* tip_entry_for(const ui::View& view) const;
    void observe(ui::Application::AttentionChange change);
    void arm(Source source, const ui::View& target);
    void cancel_pending();
    void show(Source source, const Tip& tip, Rect anchor);
    void take_down();

    ui::Application& app_;
    Desktop& desktop_;
    std::weak_ptr<void> desktop_liveness_;
    std::vector<Tip> tips_;
    std::int64_t delay_nanos_ = 500'000'000;
    ui::Application::AttentionObserverId observer_ = 0;
    ui::Application::TimerId pending_timer_ = 0;
    Source pending_source_ = Source::Pointer;
    const ui::View* pending_target_ = nullptr;
    std::weak_ptr<void> pending_liveness_;
    Tooltip* tooltip_ = nullptr;
    std::weak_ptr<void> tooltip_liveness_;
    Source shown_source_ = Source::Pointer;
    const ui::View* shown_owner_ = nullptr;
    ui::Application::ModalScopeId held_scope_ = 0;
    bool installed_command_handler_ = false;
};

}  // namespace ckv::widgets
