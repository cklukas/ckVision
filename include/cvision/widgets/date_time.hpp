// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <array>
#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/ui_strings.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/menu.hpp"

namespace ckv::widgets {

// A calendar date as three plain numbers, with no time zone and no locale. The
// struct itself enforces nothing: is_valid_date says whether it names a real
// day, and each widget that takes one states what it clamps.
struct CVISION_API DateValue {
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
CVISION_API std::string format_iso_date(DateValue date);
CVISION_API std::optional<DateValue> parse_iso_date(std::string_view text) noexcept;
// Whether `date` is a real Gregorian day in a drawable year: month 1..12 and a
// day that exists in that month, 29 February only in a leap year.
CVISION_API bool is_valid_date(DateValue date) noexcept;
// `date` moved by `days` calendar days, backwards when negative, across month
// and year ends. std::nullopt when `date` is not valid or the result would
// fall outside 1583-01-01..9999-12-31.
CVISION_API std::optional<DateValue> add_calendar_days(DateValue date, int days) noexcept;

// A time of day as three plain numbers, with no date, time zone or locale.
// The struct enforces nothing; is_valid_time says whether it is in range.
struct CVISION_API TimeValue {
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
struct CVISION_API DateTimeValue {
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
CVISION_API bool is_valid_time(TimeValue time) noexcept;
CVISION_API std::string format_iso_time(TimeValue time, bool include_seconds = true);
CVISION_API std::optional<TimeValue> parse_iso_time(std::string_view text) noexcept;

// The three parts of a written date, for naming the order they come in.
enum class DateField { Year, Month, Day };

// How a date writes its month: as a number, or by name from the month names
// of a UiStrings table.
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
struct CVISION_API DateFormat {
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
CVISION_API std::string format_date(DateValue date, const DateFormat& format,
                        const ui::UiStrings& strings = ui::english_ui_strings());
// `text` read as `format` says, or std::nullopt when it is not a valid date
// written that way. A caller's `parse` answers whatever it answers, except
// that a date it returns which is not valid is refused here.
CVISION_API std::optional<DateValue> parse_date(std::string_view text, const DateFormat& format,
                                    const ui::UiStrings& strings = ui::english_ui_strings());

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
class CVISION_API ClockView : public ui::View, public MenuBarAccessory {
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
class CVISION_API CalendarView : public ui::View {
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
    // Does not fire on_change. A date that cannot be selected leaves the
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
    std::function<void(DateValue)> on_change;

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
// not close the dropdown: whoever put it up decides that from on_change.
//
// Resolves "ckv.menu.dropdown.normal" for its frame on attach and dresses the
// calendar and the steppers in the dropdown menu's roles.
class CVISION_API CalendarDropdown : public ui::View {
public:
    // Builds the children; not a focus stop itself. The year field is empty
    // and the calendar on its default month until show_month is called.
    CalendarDropdown();

    // The dropdown's own children, owned by it and valid for its lifetime:
    // for choosing the selection, range, marks or on_change of the calendar,
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
    // Replaces the dropdown's month/weekday/year strings from `strings`.
    void set_labels(const ui::UiStrings& strings);

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
    std::string invalid_year_label_ = ui::english_ui_strings().invalid_year;
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
CVISION_API CalendarDropdown* show_calendar_dropdown(const ui::View& anchor, ui::Application& app, Desktop& desktop);

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
class CVISION_API DatePicker : public ui::View {
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
    // Replaces the picker's date/calendar strings from `strings`.
    void set_labels(ui::UiStrings strings);
    const ui::UiStrings& labels() const noexcept { return labels_; }
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
    ui::UiStrings labels_{};
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
class CVISION_API TimePicker : public ui::View {
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
    std::string am_label_ = ui::english_ui_strings().am;
    std::string pm_label_ = ui::english_ui_strings().pm;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
