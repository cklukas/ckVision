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

struct DateValue {
    int year = 2026;
    int month = 1;
    int day = 1;

    friend bool operator==(const DateValue&, const DateValue&) = default;
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

constexpr bool is_drawable_year(int year) noexcept {
    return year >= kFirstCalendarYear && year <= kLastCalendarYear;
}

// The explicit, locale-free interchange form used by DatePicker and typed
// dialog results. Parsing is strict: exactly YYYY-MM-DD, a drawable Gregorian
// year, and a real day in that month.
std::string format_iso_date(DateValue date);
std::optional<DateValue> parse_iso_date(std::string_view text) noexcept;
bool is_valid_date(DateValue date) noexcept;
std::optional<DateValue> add_calendar_days(DateValue date, int days) noexcept;

struct TimeValue {
    int hour = 0;
    int minute = 0;
    int second = 0;

    friend bool operator==(const TimeValue&, const TimeValue&) = default;
};

// Locale-free interchange for typed time controls. Parsing accepts canonical
// HH:MM and HH:MM:SS forms only; formatting includes seconds when requested.
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
    ClockView();

    void set_time_provider(std::function<TimeValue()> provider);
    void set_show_seconds(bool show);
    bool show_seconds() const noexcept { return show_seconds_; }
    // The separator blinks off for half of each second. Nothing else about
    // the display changes, so a blinking clock repaints once a second and a
    // still one does not.
    void set_blinking_separator(bool blinking);
    bool blinking_separator() const noexcept { return blinking_separator_; }
    void set_hour_format(HourFormat format);
    HourFormat hour_format() const noexcept { return hour_format_; }
    // The words for the two halves of a twelve-hour day. Empty labels
    // suppress the suffix entirely.
    void set_meridiem_labels(std::string am, std::string pm);

    // What it is displaying right now, separator and all.
    std::string text() const { return rendered_; }

    // Fires on a completed click, so a clock can open something -- a
    // calendar, say -- without knowing what.
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

class CalendarView : public ui::View {
public:
    CalendarView();

    void set_month(DateValue first_of_month);
    DateValue month() const noexcept { return month_; }
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
    void set_today_provider(std::function<std::optional<DateValue>()> provider);

    // A span of days to mark, clipped to whichever month is displayed.
    // Either end may be absent, meaning open in that direction. Pass two
    // absent ends to mark nothing.
    void set_marked_span(std::optional<DateValue> first, std::optional<DateValue> last);
    void set_range(std::optional<DateValue> minimum, std::optional<DateValue> maximum);
    void set_disabled_predicate(std::function<bool(DateValue)> predicate);
    void set_labels(std::vector<std::string> month_labels, std::vector<std::string> weekday_labels);
    // The day the week starts on, which decides both the weekday header and
    // where the first of the month falls in the grid.
    // Draw with a caller's roles rather than the list's. A calendar inside a
    // dropdown has to look like a dropdown, not like a list that happens to
    // be floating.
    void set_role_override(ui::RoleId normal, ui::RoleId selected, ui::RoleId disabled) noexcept {
        normal_role_ = normal;
        selected_role_ = selected;
        disabled_role_ = disabled;
    }
    // The "August 2026" line. Off when something above the grid already
    // says which month is shown -- a month picker and a year field, say.
    void set_show_title(bool show);
    bool show_title() const noexcept { return show_title_; }
    void set_first_weekday(Weekday first);
    Weekday first_weekday() const noexcept { return first_weekday_; }
    void set_show_iso_week_numbers(bool show);
    bool show_iso_week_numbers() const noexcept { return show_iso_week_numbers_; }

    std::function<void(DateValue)> on_select;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
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
    bool has_focus_ = false;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId today_role_ = ui::kInvalidRole;
    ui::RoleId marked_role_ = ui::kInvalidRole;

    std::optional<DateValue> effective_today() const;
    bool within_marked_span(DateValue date) const noexcept;
};

// A CalendarView shown as a transient popup: it closes on Escape, and on a
// click anywhere but itself. That behaviour is what separates a dropdown
// from a small window, and it does not belong in CalendarView -- the same
// calendar sits permanently in a dialog elsewhere and must not vanish when
// the reader clicks beside it.
class CalendarDropdown : public ui::View {
public:
    CalendarDropdown();

    CalendarView& calendar() noexcept { return *calendar_; }
    ComboBox& month_picker() noexcept { return *month_; }
    InputLine& year_field() noexcept { return *year_; }

    // The month names offered by the picker, in order. The same list the
    // calendar itself is given, so the two never disagree.
    void set_month_labels(std::vector<std::string> labels);

    // Puts the calendar on `month`, and the two controls with it.
    void show_month(DateValue month);

    std::function<void()> on_dismiss;
    std::function<void()> on_closed;

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    bool on_key(const KeyEvent& event) override;
    void on_resized() override;
    void on_attached() override;
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
CalendarDropdown* show_calendar_dropdown(const ui::View& anchor, ui::Application& app, Desktop& desktop);

class DatePicker : public ui::View {
public:
    DatePicker();
    // A date picker is often used for an optional fact (a due date, an end
    // date, a filter bound). Empty is therefore a first-class value rather
    // than a sentinel date. `seed` is the deterministic value materialized
    // by the first Up/Down edit while empty; callers normally supply their
    // injected notion of today.
    void set_value(std::optional<DateValue> value);
    std::optional<DateValue> value() const noexcept { return value_; }
    void set_seed(DateValue seed);
    DateValue seed() const noexcept { return seed_; }
    void set_empty_allowed(bool allowed);
    bool empty_allowed() const noexcept { return empty_allowed_; }
    void set_valid(bool valid);
    bool valid() const noexcept { return valid_; }
    // Supplies the explicit popup host used by Space or the dropdown
    // affordance. Without a host, DatePicker remains a standalone segmented
    // editor and performs no hidden application or desktop lookup.
    void set_calendar_host(ui::Application& app, Desktop& desktop) noexcept;
    bool open_calendar();
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
    bool focused_ = false;
    ui::Application* calendar_app_ = nullptr;
    Desktop* calendar_desktop_ = nullptr;
    CalendarDropdown* calendar_dropdown_ = nullptr;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
};

class TimePicker : public ui::View {
public:
    TimePicker();
    void set_value(TimeValue value);
    TimeValue value() const noexcept { return value_; }
    void set_show_seconds(bool show);
    void set_24_hour(bool enabled);
    void set_valid(bool valid) { valid_ = valid; invalidate(); }
    bool valid() const noexcept { return valid_; }
    bool show_seconds() const noexcept { return show_seconds_; }
    bool twenty_four_hour() const noexcept { return twenty_four_hour_; }
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
    bool has_focus_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
};

class SpinBox : public ui::View {
public:
    SpinBox();
    void set_range(int minimum, int maximum);
    void set_step(int step);
    void set_value(int value);
    int value() const noexcept { return value_; }
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
    bool has_focus_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
};

class Slider : public ui::View {
public:
    Slider();
    void set_range(int minimum, int maximum);
    void set_step(int step);
    void set_value(int value);
    int value() const noexcept { return value_; }
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
    bool has_focus_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId fill_role_ = ui::kInvalidRole;
};

class SearchBox : public ui::View {
public:
    SearchBox();
    void set_query(std::string query);
    const std::string& query() const noexcept { return query_; }
    void clear();
    std::function<void(const std::string&)> on_change;
    std::function<void()> on_clear;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    std::optional<CursorState> cursor_state() const override;

private:
    // Columns the "[x]" clear control occupies at the right edge. Drawing and
    // hit-testing both derive from this, which is what keeps the control the
    // reader can see and the region that answers a click the same thing.
    static constexpr int kClearControlWidth = 3;

    std::string query_;
    bool has_focus_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId label_role_ = ui::kInvalidRole;
};

class ToolBar : public ui::View {
public:
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
class CommandPalette : public ui::View {
public:
    CommandPalette();
    void set_query(std::string query);
    const std::string& query() const noexcept { return query_; }
    // Palette-visible, title-matching, currently available — in the
    // order the commands were declared.
    std::vector<ui::CommandInfo> filtered_commands() const;
    std::optional<ui::CommandId> highlighted_command() const;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    std::string query_;
    std::size_t highlighted_ = 0;
    bool has_focus_ = false;
    ui::RoleId input_role_ = ui::kInvalidRole;
    ui::RoleId result_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
};

class BreadcrumbBar : public ui::View {
public:
    void set_segments(std::vector<std::string> segments);
    const std::vector<std::string>& segments() const noexcept { return segments_; }
    void set_separator(std::string separator);
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
    bool has_focus_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
};

struct PropertyItem {
    std::string name;
    std::string value;
    bool editable = true;
};

class PropertyInspector : public ui::View {
public:
    void set_items(std::vector<PropertyItem> items);
    const std::vector<PropertyItem>& items() const noexcept { return items_; }
    int cursor() const noexcept { return cursor_; }
    std::function<void(std::size_t, std::string)> on_change;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    std::vector<PropertyItem> items_;
    int cursor_ = 0;
    bool editing_ = false;
    bool has_focus_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
};

struct WizardPage {
    std::string title;
    std::function<bool()> can_continue;
};

class Wizard : public ui::View {
public:
    void set_pages(std::vector<WizardPage> pages);
    std::size_t page_count() const noexcept { return pages_.size(); }
    std::size_t current_page() const noexcept { return current_page_; }
    bool can_go_next() const;
    bool can_go_back() const noexcept { return current_page_ > 0; }
    bool next();
    bool back();
    bool finish();
    std::function<void()> on_finish;
    std::function<void()> on_cancel;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    void on_attached() override;

private:
    std::vector<WizardPage> pages_;
    std::size_t current_page_ = 0;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
};

enum class NotificationSeverity { Info, Warning, Error };

struct Notification {
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

    std::size_t add(Notification notification);
    void dismiss(std::size_t index);
    const std::vector<Notification>& notifications() const noexcept { return notifications_; }

    // --- How long a toast lives ---------------------------------------
    //
    // The lifetime of a NON-persistent notification, measured on the injected
    // Clock from the moment it was added. Zero — the default — is no expiry
    // at all: nothing is taken away, which is the behaviour this widget had
    // before it could tell the time, and a host that never calls this sees no
    // change whatsoever.
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
};

class Tooltip : public ui::View {
public:
    explicit Tooltip(std::string text = {});
    void set_text(std::string text);
    const std::string& text() const noexcept { return text_; }
    void show_at(Point position);
    void hide();
    bool shown() const noexcept { return visible(); }

    void draw(scene::Painter& painter) override;
    void on_attached() override;

private:
    std::string text_;
    ui::RoleId role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
