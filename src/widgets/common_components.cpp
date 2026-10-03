// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/common_components.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>

#include "cvision/core/ascii.hpp"
#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {
namespace {

bool is_press(const KeyEvent& event) noexcept { return event.action == KeyAction::Press; }

// A month is drawn on six week rows: enough for a 31-day month that begins
// on the last day of a week. Each row is seven days, each day two digits and
// a one-cell gap.
constexpr int kWeekRows = 6;
constexpr int kDayColumns = 7;
constexpr int kDayColumnWidth = 3;

// `value` moved `delta` steps of `step`, held to [minimum, maximum]. Worked in
// 64 bits, where a step or its sum with the value cannot overflow for any
// pair of ints, and clamped before it is narrowed back.
int stepped(int value, int delta, int step, int minimum, int maximum) noexcept {
    const std::int64_t moved = std::int64_t{value} + std::int64_t{delta} * step;
    return static_cast<int>(std::clamp<std::int64_t>(moved, minimum, maximum));
}

int days_in_month(int year, int month) noexcept {
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return leap ? 29 : 28;
    }
    return kDays[std::clamp(month, 1, 12) - 1];
}

int days_before_month(int year, int month) noexcept {
    int days = 0;
    for (int m = 1; m < month; ++m) days += days_in_month(year, m);
    return days;
}

int serial(DateValue date) noexcept {
    // Leap days of the years BEFORE this one: this year's own leap day is
    // counted by days_before_month once February is behind it. Counting it
    // from January 1 put every date of a leap year one weekday late.
    const int y = date.year;
    const int prior = y - 1;
    int days = 365 * y + prior / 4 - prior / 100 + prior / 400;
    days += days_before_month(date.year, date.month);
    days += date.day - 1;
    return days;
}

DateValue from_serial(int value) noexcept {
    int year = value / 366;
    while (serial(DateValue{year + 1, 1, 1}) <= value) ++year;
    while (serial(DateValue{year, 1, 1}) > value) --year;
    int month = 1;
    while (month < 12 && serial(DateValue{year, month + 1, 1}) <= value) ++month;
    const int day = value - serial(DateValue{year, month, 1}) + 1;
    return DateValue{year, month, day};
}

DateValue clamp_day(DateValue date) noexcept {
    date.month = std::clamp(date.month, 1, 12);
    date.day = std::clamp(date.day, 1, days_in_month(date.year, date.month));
    return date;
}

int weekday_index(DateValue date, Weekday first) noexcept;

int weekday_monday_zero(DateValue date) noexcept {
    // 0000-03-01 style weekday math is unnecessary here; the absolute offset
    // only needs to be stable. 1970-01-01 was Thursday, hence +3 for Monday=0.
    const int days = serial(date) - serial(DateValue{1970, 1, 1});
    return ((days + 3) % 7 + 7) % 7;
}

// ISO 8601 week number: a week belongs to the year that holds its Thursday,
// and counts from the week holding that year's first Thursday.
int iso_week(DateValue date) noexcept {
    const DateValue thursday = from_serial(serial(date) - weekday_monday_zero(date) + 3);
    return (days_before_month(thursday.year, thursday.month) + thursday.day - 1) / 7 + 1;
}

// The column a date falls in, counting from whichever day the week starts.
int weekday_index(DateValue date, Weekday first) noexcept {
    const int monday_zero = weekday_monday_zero(date);
    const int offset = static_cast<int>(first);
    return ((monday_zero - offset) % 7 + 7) % 7;
}

std::string two_digit(int value) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02d", value);
    return buffer;
}

// A time picker's face: the fields, and on the twelve-hour scale a space and
// the meridiem word, unless that word is empty.
std::string time_text(TimeValue time, bool seconds, bool twenty_four_hour, std::string_view am,
                      std::string_view pm) {
    int hour = std::clamp(time.hour, 0, 23);
    std::string suffix;
    if (!twenty_four_hour) {
        const std::string_view word = hour < 12 ? am : pm;
        if (!word.empty()) suffix = " " + std::string(word);
        hour %= 12;
        if (hour == 0) hour = 12;
    }
    std::string result = two_digit(hour) + ":" + two_digit(std::clamp(time.minute, 0, 59));
    if (seconds) result += ":" + two_digit(std::clamp(time.second, 0, 59));
    return result + suffix;
}

bool contains_ci(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            if (ascii_lower(haystack[i + j]) != ascii_lower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

// A ToolBar entry naming a command no registry has declared — or one
// drawn before the bar is attached to an Application at all — is an
// application defect. Draw a placeholder the reader can report rather
// than an empty button that looks deliberate. There is nothing more
// informative to show: a CommandId is a handle its own registry
// assigned, so an id that registry does not know has no title, no key,
// and no meaning anywhere else.
std::string command_title(ui::Application* app, ui::CommandId id) {
    if (app == nullptr) return "(unknown)";
    if (const ui::CommandInfo* info = app->commands().find(id); info != nullptr) return info->title;
    return "(unknown)";
}

// The Desktop a view's popups open on: the nearest one above it.
Desktop* enclosing_desktop(ui::View& view) {
    for (ui::View* ancestor = view.parent(); ancestor != nullptr; ancestor = ancestor->parent())
        if (auto* desktop = dynamic_cast<Desktop*>(ancestor)) return desktop;
    return nullptr;
}

// The end of `text` that fits in `columns`, whole clusters only. A search
// field types at its end, so when the query outgrows the field the end is the
// part the reader is working on.
std::string tail_to_width(std::string_view text, int columns) {
    const std::vector<std::string_view> graphemes = text::split_graphemes(text);
    int used = 0;
    std::size_t first = graphemes.size();
    while (first > 0) {
        const int width = text::grapheme_width(graphemes[first - 1]);
        if (used + width > columns) break;
        used += width;
        --first;
    }
    std::string tail;
    for (std::size_t i = first; i < graphemes.size(); ++i) tail.append(graphemes[i]);
    return tail;
}

// `text` without its last grapheme cluster. Backspace takes back what one
// keystroke or one pasted character made, never a byte of it: a byte would
// leave half a UTF-8 scalar behind.
std::string without_last_grapheme(std::string_view text) {
    const std::vector<std::string_view> graphemes = text::split_graphemes(text);
    if (graphemes.empty()) return {};
    return std::string(text.substr(0, text.size() - graphemes.back().size()));
}

}  // namespace

std::string format_iso_date(DateValue date) {
    return std::to_string(date.year) + "-" + two_digit(date.month) + "-" + two_digit(date.day);
}

bool is_valid_date(DateValue date) noexcept {
    return is_drawable_year(date.year) && date.month >= 1 && date.month <= 12 && date.day >= 1 &&
           date.day <= days_in_month(date.year, date.month);
}

std::optional<DateValue> parse_iso_date(std::string_view text) noexcept {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') return std::nullopt;
    const auto digit = [text](std::size_t index) -> std::optional<int> {
        const char value = text[index];
        if (value < '0' || value > '9') return std::nullopt;
        return value - '0';
    };
    for (const std::size_t index : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{3},
                                    std::size_t{5}, std::size_t{6}, std::size_t{8}, std::size_t{9}})
        if (!digit(index)) return std::nullopt;
    const int year = *digit(0) * 1000 + *digit(1) * 100 + *digit(2) * 10 + *digit(3);
    const int month = *digit(5) * 10 + *digit(6);
    const int day = *digit(8) * 10 + *digit(9);
    const DateValue date{year, month, day};
    return is_valid_date(date) ? std::optional<DateValue>{date} : std::nullopt;
}

bool is_valid_time(TimeValue time) noexcept {
    return time.hour >= 0 && time.hour <= 23 && time.minute >= 0 && time.minute <= 59 &&
           time.second >= 0 && time.second <= 59;
}

std::string format_iso_time(TimeValue time, bool include_seconds) {
    return two_digit(time.hour) + ":" + two_digit(time.minute) +
           (include_seconds ? ":" + two_digit(time.second) : std::string{});
}

std::optional<TimeValue> parse_iso_time(std::string_view text) noexcept {
    if ((text.size() != 5 && text.size() != 8) || text[2] != ':' || (text.size() == 8 && text[5] != ':'))
        return std::nullopt;
    const auto pair = [text](std::size_t index) -> std::optional<int> {
        if (text[index] < '0' || text[index] > '9' || text[index + 1] < '0' || text[index + 1] > '9')
            return std::nullopt;
        return (text[index] - '0') * 10 + (text[index + 1] - '0');
    };
    const auto hour = pair(0);
    const auto minute = pair(3);
    const auto second = text.size() == 8 ? pair(6) : std::optional<int>{0};
    if (!hour || !minute || !second) return std::nullopt;
    const TimeValue value{*hour, *minute, *second};
    return is_valid_time(value) ? std::optional<TimeValue>{value} : std::nullopt;
}

std::optional<DateValue> add_calendar_days(DateValue date, int days) noexcept {
    if (!is_valid_date(date)) return std::nullopt;
    const int first = serial(DateValue{kFirstCalendarYear, 1, 1});
    const int last = serial(DateValue{kLastCalendarYear, 12, 31});
    const long long shifted = static_cast<long long>(serial(date)) + days;
    if (shifted < first || shifted > last) return std::nullopt;
    return from_serial(static_cast<int>(shifted));
}

const DateTimeLabels& english_date_time_labels() noexcept {
    static const DateTimeLabels labels{};
    return labels;
}

namespace {

// Where each field of a written date lies: its first column and its width,
// indexed by DateField.
struct DateSpan {
    int x = 0;
    int width = 0;
};
struct DateLayout {
    std::string text;
    std::array<DateSpan, 3> spans{};
};

std::size_t field_index(DateField field) noexcept { return static_cast<std::size_t>(field); }

// The month's name from `labels`, or the English one when the table does not
// hold twelve: a list of the wrong length is a mistake the widget can
// survive, and a name is still more use to the reader than nothing.
const std::string& month_name(int month, const DateTimeLabels& labels) {
    const std::vector<std::string>& names =
        labels.month_names.size() == 12 ? labels.month_names : english_date_time_labels().month_names;
    return names[static_cast<std::size_t>(std::clamp(month, 1, 12) - 1)];
}

std::string field_text(DateValue date, DateField field, const DateFormat& format, const DateTimeLabels& labels) {
    switch (field) {
        case DateField::Year: return std::to_string(date.year);
        case DateField::Month:
            if (format.month_style == MonthStyle::Name) return month_name(date.month, labels);
            return format.zero_pad ? two_digit(date.month) : std::to_string(date.month);
        case DateField::Day: return format.zero_pad ? two_digit(date.day) : std::to_string(date.day);
    }
    return {};
}

// The options' own layout of `date`: the text, and where each field is in
// it. Formatting and a DatePicker's segments both come from here, so the
// segment an arrow edits is always the one drawn reversed.
DateLayout layout_date(DateValue date, const DateFormat& format, const DateTimeLabels& labels) {
    DateLayout layout;
    int x = 0;
    for (std::size_t position = 0; position < format.order.size(); ++position) {
        if (position > 0) {
            layout.text += format.separator;
            x += text::text_width(format.separator);
        }
        const std::string part = field_text(date, format.order[position], format, labels);
        const int width = text::text_width(part);
        layout.spans[field_index(format.order[position])] = DateSpan{x, width};
        layout.text += part;
        x += width;
    }
    return layout;
}

// Whether `order` names each of the three fields once.
bool is_field_order(const std::array<DateField, 3>& order) noexcept {
    std::array<bool, 3> seen{};
    for (const DateField field : order) {
        const std::size_t index = field_index(field);
        if (index >= seen.size() || seen[index]) return false;
        seen[index] = true;
    }
    return true;
}

// `text` read as a run of ASCII digits of an accepted length, or nothing.
std::optional<int> read_digits(std::string_view text, std::size_t shortest, std::size_t longest) noexcept {
    if (text.size() < shortest || text.size() > longest) return std::nullopt;
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + (c - '0');
    }
    return value;
}

bool equal_ignoring_ascii_case(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
    return true;
}

std::string_view trim_spaces(std::string_view text) noexcept {
    while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
    return text;
}

}  // namespace

std::string format_date(DateValue date, const DateFormat& format, const DateTimeLabels& labels) {
    if (format.format) return format.format(date);
    return layout_date(date, format, labels).text;
}

std::optional<DateValue> parse_date(std::string_view text, const DateFormat& format, const DateTimeLabels& labels) {
    if (format.parse) {
        const std::optional<DateValue> parsed = format.parse(text);
        return parsed && is_valid_date(*parsed) ? parsed : std::nullopt;
    }
    if (format.separator.empty() || !is_field_order(format.order)) return std::nullopt;
    text = trim_spaces(text);
    const std::string_view separator = format.separator;
    const std::size_t first = text.find(separator);
    if (first == std::string_view::npos) return std::nullopt;
    const std::size_t second = text.find(separator, first + separator.size());
    if (second == std::string_view::npos) return std::nullopt;
    const std::array<std::string_view, 3> parts{text.substr(0, first),
                                                text.substr(first + separator.size(),
                                                            second - first - separator.size()),
                                                text.substr(second + separator.size())};
    DateValue date{0, 0, 0};
    const std::size_t shortest = format.zero_pad ? 2 : 1;
    for (std::size_t position = 0; position < parts.size(); ++position) {
        const std::string_view part = parts[position];
        std::optional<int> value;
        switch (format.order[position]) {
            case DateField::Year:
                value = read_digits(part, 4, 4);
                if (value) date.year = *value;
                break;
            case DateField::Month:
                if (format.month_style == MonthStyle::Name) {
                    for (int month = 1; month <= 12 && !value; ++month)
                        if (equal_ignoring_ascii_case(part, month_name(month, labels))) value = month;
                } else {
                    value = read_digits(part, shortest, 2);
                }
                if (value) date.month = *value;
                break;
            case DateField::Day:
                value = read_digits(part, shortest, 2);
                if (value) date.day = *value;
                break;
        }
        if (!value) return std::nullopt;
    }
    return is_valid_date(date) ? std::optional<DateValue>{date} : std::nullopt;
}

CalendarView::CalendarView() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{24, 9});
    month_labels_ = {"January", "February", "March", "April", "May", "June",
                     "July", "August", "September", "October", "November", "December"};
    weekday_labels_ = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
}

void CalendarView::set_month(DateValue first_of_month) {
    // Held inside the years this calendar can state truthfully, rather than
    // drawing a Gregorian grid over dates that were never Gregorian.
    first_of_month.year = std::clamp(first_of_month.year, kFirstCalendarYear, kLastCalendarYear);
    first_of_month = clamp_day(first_of_month);
    first_of_month.day = 1;
    month_ = first_of_month;
    invalidate();
}

void CalendarView::set_selected(DateValue selected) {
    select(clamp_day(selected), false);
    set_month(DateValue{selected_.year, selected_.month, 1});
}

void CalendarView::set_today(std::optional<DateValue> today) {
    today_ = today ? std::optional<DateValue>{clamp_day(*today)} : std::nullopt;
    invalidate();
}

void CalendarView::set_range(std::optional<DateValue> minimum, std::optional<DateValue> maximum) {
    minimum_ = minimum;
    maximum_ = maximum;
    invalidate();
}

void CalendarView::set_disabled_predicate(std::function<bool(DateValue)> predicate) {
    disabled_ = std::move(predicate);
    invalidate();
}

void CalendarView::set_labels(std::vector<std::string> month_labels, std::vector<std::string> weekday_labels) {
    if (month_labels.size() == 12) month_labels_ = std::move(month_labels);
    if (weekday_labels.size() == 7) weekday_labels_ = std::move(weekday_labels);
    invalidate();
}

void CalendarView::set_show_title(bool show) {
    if (show_title_ == show) return;
    show_title_ = show;
    invalidate();
    size_hint_changed();  // one row more or fewer
}

void CalendarView::set_first_weekday(Weekday first) {
    if (first_weekday_ == first) return;
    first_weekday_ = first;
    invalidate();
}

void CalendarView::set_show_iso_week_numbers(bool show) {
    if (show_iso_week_numbers_ == show) return;
    show_iso_week_numbers_ = show;
    invalidate();
    size_hint_changed();  // the "Wk" column widens the preferred width
}

bool CalendarView::selectable(DateValue date) const {
    if (minimum_ && date < *minimum_) return false;
    if (maximum_ && *maximum_ < date) return false;
    if (disabled_ && disabled_(date)) return false;
    return true;
}

void CalendarView::select(DateValue date, bool notify) {
    if (!selectable(date)) return;
    if (selected_ == date) return;
    selected_ = date;
    if (selected_.month != month_.month || selected_.year != month_.year)
        month_ = DateValue{selected_.year, selected_.month, 1};
    invalidate();
    if (notify && on_select) on_select(selected_);
}

void CalendarView::move_selection(int days) { select(from_serial(serial(selected_) + days), true); }

std::optional<DateValue> CalendarView::date_at_cell(Point local) const {
    const int x_offset = week_column_width();
    if (local.x < x_offset) return std::nullopt;
    // The six week rows start where draw() puts them, below the title only
    // when there is one.
    const int column = (local.x - x_offset) / kDayColumnWidth;
    const int row = local.y - grid_top();
    if (row < 0 || row >= kWeekRows || column >= kDayColumns) return std::nullopt;
    const int first_weekday = weekday_index(month_, first_weekday_);
    const int day = row * 7 + column - first_weekday + 1;
    if (day < 1 || day > days_in_month(month_.year, month_.month)) return std::nullopt;
    return DateValue{month_.year, month_.month, day};
}

void CalendarView::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    // Disabled (D-076): every cell keeps its surface -- so today, the marked
    // span, and the chosen day still show where they are -- with the disabled
    // foreground, and the chosen day on the muted selection.
    const bool enabled = enabled_in_tree();
    const Style inert = theme.resolve(view_disabled_role_);
    const auto shown = [&](Style style) { return enabled ? style : accent_style(style, inert); };
    const Style normal = shown(theme.resolve(normal_role_));
    // The chosen day wears the full highlight only while the calendar holds
    // the keyboard, as a list's selection does; elsewhere, and disabled, it
    // keeps the muted form so its place still shows.
    const Style selected = shown(theme.resolve(enabled && has_focus() ? selected_role_ : selected_inactive_role_));
    const Style disabled = shown(theme.resolve(disabled_role_));
    const Style today_style = shown(theme.resolve(today_role_));
    const Style marked_style = shown(theme.resolve(marked_role_));
    // Asked once per frame, not once per day drawn: a provider reading a
    // system clock should be called a fixed, small number of times, and every
    // cell in one frame has to agree about what day it is.
    const std::optional<DateValue> today = effective_today();
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));
    if (show_title_) {
        const std::string title =
            month_labels_[std::clamp(month_.month, 1, 12) - 1] + " " + std::to_string(month_.year);
        painter.draw_text(Point{0, 0}, text::clip_to_width(title, bounds().width), normal);
    }
    const int x_offset = week_column_width();
    // The weekday row names the columns rather than being one of them, so it
    // is set apart from the days it heads.
    Style header = normal;
    header.attrs = header.attrs | Attr::Bold;
    if (show_iso_week_numbers_) {
        painter.draw_text(Point{0, header_row()}, "Wk", header);
        // Each row is labelled with the ISO week of its Monday: every row of
        // seven days holds exactly one, and it is the week six of those
        // days belong to whichever day the calendar starts on.
        const int lead = weekday_index(month_, first_weekday_);
        const int rows = (lead + days_in_month(month_.year, month_.month) + 6) / 7;
        const int first_cell = serial(DateValue{month_.year, month_.month, 1}) - lead;
        for (int row = 0; row < rows; ++row) {
            const DateValue row_start = from_serial(first_cell + row * 7);
            const DateValue monday = from_serial(first_cell + row * 7 + (7 - weekday_monday_zero(row_start)) % 7);
            const int week = iso_week(monday);
            painter.draw_text(Point{0, grid_top() + row}, (week < 10 ? " " : "") + std::to_string(week), header);
        }
    }
    // The weekday names start at whichever day the week starts on.
    const int first_offset = static_cast<int>(first_weekday_);
    for (int i = 0; i < 7; ++i)
        painter.draw_text(Point{x_offset + i * kDayColumnWidth, header_row()}, weekday_labels_[(first_offset + i) % 7], header);
    for (int day = 1; day <= days_in_month(month_.year, month_.month); ++day) {
        const DateValue date{month_.year, month_.month, day};
        const int index = weekday_index(month_, first_weekday_) + day - 1;
        const int row = index / 7;
        const int column = index % 7;
        // Disabled first: a day that cannot be chosen says so before
        // anything else. Then the reader's own choice, then today, then the
        // marked span -- each a stronger claim on the cell than the next.
        const Style style = !selectable(date)          ? disabled
                            : date == selected_        ? selected
                            : (today && date == *today) ? today_style
                            : within_marked_span(date)  ? marked_style
                                                        : normal;
        painter.draw_text(Point{x_offset + column * kDayColumnWidth, grid_top() + row}, (day < 10 ? " " : "") + std::to_string(day), style);
    }
}

bool CalendarView::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    if (event.chord.key == Key::Left) {
        move_selection(-1);
        return true;
    }
    if (event.chord.key == Key::Right) {
        move_selection(1);
        return true;
    }
    if (event.chord.key == Key::Up) {
        move_selection(-7);
        return true;
    }
    if (event.chord.key == Key::Down) {
        move_selection(7);
        return true;
    }
    return false;
}

bool CalendarView::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    if (auto date = date_at_cell(Point{event.cell.x - absolute_bounds().x, event.cell.y - absolute_bounds().y})) {
        select(*date, true);
        return true;
    }
    return false;
}

void CalendarView::on_focus(const FocusEvent&) { invalidate(); }

ui::SizeHint CalendarView::horizontal_size_hint() const {
    // What draw() uses: the "Wk" column when shown, then seven day columns
    // of three cells, the last without its trailing gap. The preferred width
    // keeps that one blank after the last column.
    const int drawn = week_column_width() + kDayColumns * kDayColumnWidth - 1;
    return ui::SizeHint{drawn, drawn + 1, ui::kUnboundedExtent};
}
ui::SizeHint CalendarView::vertical_size_hint() const {
    // What draw() uses: the title when shown, the weekday heading, and the
    // six week rows the longest month can need.
    const int rows = grid_top() + kWeekRows;
    return ui::SizeHint{rows, rows, rows};
}
int CalendarView::height_for_width(int) const {
    // Asked by a container measuring its children; without this it answered
    // with the preferred size set at construction, which knows nothing of
    // the title being hidden.
    return vertical_size_hint().preferred;
}

ClockView::ClockView() {
    set_focus_policy(ui::FocusPolicy::None);
    set_preferred_size(Size{8, 1});
}

void ClockView::set_time_provider(std::function<TimeValue()> provider) {
    time_provider_ = std::move(provider);
    refresh();
    ensure_ticking();
}

void ClockView::set_show_seconds(bool show) {
    if (show_seconds_ == show) return;
    show_seconds_ = show;
    refresh();
}

void ClockView::set_blinking_separator(bool blinking) {
    if (blinking_separator_ == blinking) return;
    blinking_separator_ = blinking;
    separator_lit_ = true;
    refresh();
}

void ClockView::set_hour_format(HourFormat format) {
    if (hour_format_ == format) return;
    hour_format_ = format;
    refresh();
}

void ClockView::set_meridiem_labels(std::string am, std::string pm) {
    am_label_ = std::move(am);
    pm_label_ = std::move(pm);
    refresh();
}

std::string ClockView::render() const {
    if (!time_provider_) return {};
    const TimeValue now = time_provider_();
    int hour = now.hour;
    std::string suffix;
    if (hour_format_ == HourFormat::TwelveHour) {
        const std::string& label = hour < 12 ? am_label_ : pm_label_;
        hour = hour % 12;
        if (hour == 0) hour = 12;  // midnight and noon read as twelve, not zero
        if (!label.empty()) suffix = " " + label;
    }
    // Blinking hides the separator, never the digits: a clock whose numbers
    // flicker is unreadable, and the separator carries no information.
    const std::string separator = (blinking_separator_ && !separator_lit_) ? " " : ":";
    std::string text = (hour_format_ == HourFormat::TwelveHour ? std::to_string(hour) : two_digit(hour)) +
                       separator + two_digit(now.minute);
    if (show_seconds_) text += separator + two_digit(now.second);
    return text + suffix;
}

void ClockView::refresh() {
    // The whole point: re-render every tick, repaint only on a difference.
    // Without this a clock is a full repaint a second forever, seconds shown
    // or not.
    std::string next = render();
    if (next == rendered_) return;
    const int previous_width = text::text_width(rendered_);
    rendered_ = std::move(next);
    // A clock that is now a different WIDTH has to be placed again and not
    // merely repainted: seconds switched on, a meridiem word arriving, a
    // twelve-hour clock passing from 9 to 10. Without this its container keeps
    // the width it measured once, and the digits that no longer fit are
    // clipped -- which is how a clock showing seconds read 09:41:0.
    if (text::text_width(rendered_) != previous_width) size_hint_changed();
    invalidate();
}

void ClockView::ensure_ticking() {
    if (ticking_ || context().app == nullptr) return;
    ticking_ = true;
    ui::Application* const app = context().app;
    const std::weak_ptr<void> liveness = lifetime_token();
    auto id = std::make_shared<ui::Application::TimerId>(0);
    // One second, whether or not seconds are shown: the cost of a tick is a
    // string comparison, and a minute-resolution clock that ticked once a
    // minute would show each minute up to a second late.
    *id = app->start_timer(1'000'000'000, /*repeating=*/true, [this, liveness, id, app] {
        // The view is gone and nothing will cancel this but the callback
        // itself; a repeating timer holding a dead pointer would fire
        // forever.
        if (liveness.expired()) {
            app->cancel_timer(*id);
            return;
        }
        if (blinking_separator_) separator_lit_ = !separator_lit_;
        refresh();
    });
}

void ClockView::set_open(bool open) {
    if (open_ == open) return;
    open_ = open;
    invalidate();
}

void ClockView::set_menu_highlighted(bool highlighted) {
    if (menu_highlighted_ == highlighted) return;
    menu_highlighted_ = highlighted;
    invalidate();
}

void ClockView::activate_from_menu_bar() {
    if (on_click) on_click();
}

void ClockView::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.menu.bar.normal");
    if (open_role_ == ui::kInvalidRole) open_role_ = context().roles->find("ckv.menu.bar.active");
    refresh();
    ensure_ticking();
}

void ClockView::draw(scene::Painter& painter) {
    // Held by the keyboard walk or showing something: either way it is the
    // title the reader is on, and a bar title looks the same in both.
    const Style style = context().theme->resolve((open_ || menu_highlighted_) ? open_role_ : role_);
    // The padding is part of the highlight, as a menu title's is: without it
    // the open state reads as coloured text rather than as a pressed item.
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", style));
    painter.draw_text(Point{1, 0}, text::clip_to_width(rendered_, std::max(0, bounds().width - 1)), style);
}

bool ClockView::on_mouse(const MouseEvent& event) {
    if (event.action == MouseAction::Down) {
        pressed_ = true;
        return true;
    }
    if (event.action == MouseAction::Up) {
        const bool was_pressed = pressed_;
        pressed_ = false;
        const Rect abs = absolute_bounds();
        const bool inside = event.cell.x >= abs.x && event.cell.x < abs.right() &&
                            event.cell.y >= abs.y && event.cell.y < abs.bottom();
        if (was_pressed && inside && on_click) on_click();
        return was_pressed;
    }
    return false;
}

ui::SizeHint ClockView::horizontal_size_hint() const {
    // Measured from what is actually shown, so a clock that gains seconds or
    // a meridiem word is given room for them rather than clipped.
    // One cell of padding either side, so the open highlight has the same
    // shape a menu title's does.
    const int width = std::max(1, text::text_width(rendered_)) + 2;
    return ui::SizeHint{width, width, width};
}

ui::SizeHint ClockView::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

namespace {
// The calendar grid is seven columns of three cells, less the trailing gap.
constexpr int kGridWidth = kDayColumns * kDayColumnWidth - 1;
// One column of padding to the left of the content, inside the frame.
constexpr int kContentPad = 1;
constexpr int kContentLeft = 1 + kContentPad;  // frame, then the padding
// Month, year and its steppers share one row: the whole point of a dropdown
// under a clock is that it is small, and three of the ten rows would
// otherwise be spent on chrome.
constexpr int kControlRow = 1;
constexpr int kGridRow = 2;
constexpr int kStepWidth = 2;  // "<<" and ">>"
constexpr int kGap = 2;        // between the month and the stepper beside it
constexpr int kYearWidth = 4;  // the four digits a year is written in
constexpr std::int64_t kInvalidHoldNanos = 3'000'000'000;
}  // namespace

CalendarDropdown::CalendarDropdown() {
    set_focus_policy(ui::FocusPolicy::None);
    // Added in the order a reader works through them, which is the order Tab
    // walks: the month, the year, then the days themselves.
    auto month = std::make_unique<ComboBox>(ComboBoxMode::PickOnly);
    month->on_select = [this](std::size_t index) {
        calendar_->set_month(DateValue{calendar_->month().year, static_cast<int>(index) + 1, 1});
    };
    month_ = static_cast<ComboBox*>(add_child(std::move(month)));
    auto back = std::make_unique<Button>("<<");
    back->set_flat(true);  // a stepper in a shared row has no room for a shadow
    back->set_focus_policy(ui::FocusPolicy::None);  // Tab walks month, year, days
    back->on_press = [this] { step_year(-1); };
    step_back_ = static_cast<Button*>(add_child(std::move(back)));
    auto forward = std::make_unique<Button>(">>");
    forward->set_flat(true);
    forward->set_focus_policy(ui::FocusPolicy::None);
    forward->on_press = [this] { step_year(1); };
    step_forward_ = static_cast<Button*>(add_child(std::move(forward)));
    auto year = std::make_unique<InputLine>();
    // Digits only: anything else is not part of a year, and refusing it as
    // it is typed beats accepting it and complaining afterwards.
    year->set_grapheme_filter([](std::string_view g) { return g.size() == 1 && g[0] >= '0' && g[0] <= '9'; });
    year_ = static_cast<InputLine*>(add_child(std::move(year)));
    auto calendar = std::make_unique<CalendarView>();
    calendar->set_show_title(false);  // the controls above say which month
    calendar_ = static_cast<CalendarView*>(add_child(std::move(calendar)));
}

void CalendarDropdown::on_attached() {
    if (frame_role_ == ui::kInvalidRole) frame_role_ = context().roles->find("ckv.menu.dropdown.normal");
    // A calendar inside a dropdown has to look like a dropdown, not like a
    // list that happens to be floating.
    const ui::RoleId highlighted = context().roles->find("ckv.menu.dropdown.highlighted");
    calendar_->set_role_override(frame_role_, highlighted, context().roles->find("ckv.menu.dropdown.disabled"));
    // The steppers are part of this popup, not dialog buttons that landed in
    // it: they wear its colours at rest and its highlight when held, which is
    // the same statement a menu item or a status-line item makes.
    for (Button* step : {step_back_, step_forward_}) {
        step->set_role_override(frame_role_, frame_role_, frame_role_, frame_role_);
        step->set_pressed_role_override(highlighted);
    }
    if (month_labels_.empty()) set_month_labels(english_date_time_labels().month_names);
}

void CalendarDropdown::set_month_labels(std::vector<std::string> labels) {
    month_labels_ = std::move(labels);
    month_->set_items(month_labels_);
    calendar_->set_labels(month_labels_, {});
}

void CalendarDropdown::set_labels(const DateTimeLabels& labels) {
    if (labels.month_names.size() == 12) set_month_labels(labels.month_names);
    calendar_->set_labels({}, labels.weekday_names);
    invalid_year_label_ = labels.invalid_year;
    invalidate();
}

void CalendarDropdown::show_month(DateValue month) {
    calendar_->set_month(DateValue{month.year, month.month, 1});
    // Selected, not merely listed: opening on August with nothing chosen
    // would make the reader pick the month they are already looking at.
    month_->set_selected_index(static_cast<std::size_t>(std::clamp(month.month, 1, 12) - 1));
    year_->set_text(std::to_string(month.year));
    showing_invalid_ = false;
    set_control_row_visible(true);
    invalidate();
}

int CalendarDropdown::month_width() const noexcept {
    // As wide as the longest month it offers and no wider: the picker is
    // read, not filled in, so space past the longest name is space the year
    // beside it could have had.
    int widest = 0;
    for (const std::string& label : month_labels_) widest = std::max(widest, text::text_width(label));
    // One cell for the arrow; the blank that keeps the picker off the stepper
    // beside it belongs to the row, not to the picker.
    const int wanted = widest + 1;
    const int rest = kGap + kStepWidth + kYearWidth + kStepWidth;
    return std::clamp(wanted, 4, kGridWidth - rest);
}

Rect CalendarDropdown::year_rect() const noexcept {
    return Rect{kContentLeft + month_width() + kGap + kStepWidth, kControlRow, kYearWidth, 1};
}

void CalendarDropdown::set_control_row_visible(bool visible) {
    year_->set_visible(visible);
    step_back_->set_visible(visible);
    step_forward_->set_visible(visible);
}

void CalendarDropdown::sync_month_bounds() {
    month_->set_bounds(Rect{kContentLeft, kControlRow, month_width(), 1});
}

void CalendarDropdown::on_resized() {
    sync_month_bounds();
    const Rect year = year_rect();
    step_back_->set_bounds(Rect{year.x - kStepWidth, kControlRow, kStepWidth, 1});
    year_->set_bounds(year);
    step_forward_->set_bounds(Rect{year.right(), kControlRow, kStepWidth, 1});
    calendar_->set_bounds(Rect{kContentLeft, kGridRow, kGridWidth, std::max(0, bounds().height - kGridRow - 1)});
}

ui::SizeHint CalendarDropdown::horizontal_size_hint() const {
    const int width = kContentLeft + kGridWidth + 1;  // frame, pad, grid, frame
    return ui::SizeHint{width, width, width};
}

ui::SizeHint CalendarDropdown::vertical_size_hint() const {
    const int height = kGridRow + 1 + 6 + 1;  // chrome, weekday row, six weeks, frame
    return ui::SizeHint{height, height, height};
}

void CalendarDropdown::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(frame_role_);
    const Rect all{0, 0, bounds().width, bounds().height};
    painter.fill(all, Cell::from_grapheme(" ", style));
    // Framed like a dropdown menu, in a dropdown menu's colours, because
    // that is what it is.
    painter.draw_box(all, scene::LineStyle::Single, style);
    if (!showing_invalid_) return;
    // The complaint takes the whole stepper-and-year span, which is the one
    // place on this row with room for a word. The controls it covers are
    // hidden meanwhile, so nothing prints through it.
    const Rect span{step_back_->bounds().x, kControlRow,
                    step_forward_->bounds().right() - step_back_->bounds().x, 1};
    painter.fill(span, Cell::from_grapheme(" ", style));
    const std::string word = text::elide_to_width(invalid_year_label_, span.width);
    painter.draw_text(Point{span.x + std::max(0, (span.width - text::text_width(word)) / 2), kControlRow}, word,
                      style);
}

void CalendarDropdown::dismiss() {
    if (dismissed_) return;
    dismissed_ = true;
    const std::function<void()> notify = on_closed;
    const std::function<void()> callback = on_dismiss;
    if (notify) notify();
    if (callback) callback();
}

void CalendarDropdown::step_year(int delta) {
    const DateValue shown = calendar_->month();
    show_month(DateValue{std::clamp(shown.year + delta, kFirstCalendarYear, kLastCalendarYear), shown.month, 1});
}

void CalendarDropdown::report_invalid_year() {
    showing_invalid_ = true;
    // The row says what happened, and while it does the controls it covers
    // step aside -- left up, the field would print its rejected text through
    // the word and the steppers would sit on top of it.
    set_control_row_visible(false);
    invalidate();
    if (context().app == nullptr) return;
    const std::weak_ptr<void> liveness = lifetime_token();
    // Held long enough to read, then the year that is actually shown comes
    // back -- leaving the complaint there would be a second thing to clear.
    context().app->start_timer(kInvalidHoldNanos, /*repeating=*/false, [this, liveness] {
        if (liveness.expired()) return;
        if (!showing_invalid_) return;
        showing_invalid_ = false;
        set_control_row_visible(true);
        year_->set_text(std::to_string(calendar_->month().year));
        invalidate();
    });
}

void CalendarDropdown::commit_year() {
    if (showing_invalid_) return;
    const std::string text = year_->text();
    int year = 0;
    // The field only admits digits, so what is left to reject is a year this
    // calendar cannot draw: nothing typed, or a year outside the Gregorian
    // range. "26" is the case that matters -- it parses, and it would print a
    // confident grid for a year that had a different calendar entirely.
    const bool parsed = !text.empty() && text.size() <= 4 &&
                        std::from_chars(text.data(), text.data() + text.size(), year).ec == std::errc{};
    if (!parsed || !is_drawable_year(year)) {
        report_invalid_year();
        return;
    }
    show_month(DateValue{year, calendar_->month().month, 1});
}

void CalendarDropdown::focus_slot(int slot) {
    focus_slot_ = ((slot % 3) + 3) % 3;
    if (focus_slot_ != 0 && month_->dropdown_open()) month_->close_dropdown();
    if (context().app == nullptr) return;
    ui::View* const target = focus_slot_ == 0   ? static_cast<ui::View*>(month_)
                             : focus_slot_ == 1 ? static_cast<ui::View*>(year_)
                                                : static_cast<ui::View*>(calendar_);
    context().app->set_focus(target);
    invalidate();
}

ui::View* CalendarDropdown::child_at(Point local) const noexcept {
    for (ui::View* child : {static_cast<ui::View*>(month_), static_cast<ui::View*>(step_back_),
                            static_cast<ui::View*>(year_), static_cast<ui::View*>(step_forward_),
                            static_cast<ui::View*>(calendar_)})
        if (child->visible() && child->bounds().contains(local)) return child;
    return nullptr;
}

bool CalendarDropdown::on_mouse(const MouseEvent& event) {
    // A grab lives only while the button is down, so a fresh press means the
    // release was never delivered here: the child took the mouse with it when
    // it opened a popup of its own. Left latched, that grab swallowed every
    // later event -- clicks outside stopped dismissing this popup, and the
    // application behind it stopped responding at all.
    if (event.action == MouseAction::Down) mouse_child_ = nullptr;
    // Otherwise a press that began on a child stays with it until the button
    // comes up, wherever the pointer goes meanwhile: that is what lets a
    // button show itself disarmed when the pointer slides off it, and how the
    // press is taken back rather than fired.
    if (mouse_child_ != nullptr) {
        ui::View* const child = mouse_child_;
        if (event.action == MouseAction::Up) mouse_child_ = nullptr;
        return child->on_mouse(event);
    }
    const Rect abs = absolute_bounds();
    const bool inside = event.cell.x >= abs.x && event.cell.x < abs.right() && event.cell.y >= abs.y &&
                        event.cell.y < abs.bottom();
    if (!inside) {
        if (event.action == MouseAction::Down) dismiss();
        return false;
    }
    ui::View* const child = child_at(Point{event.cell.x - abs.x, event.cell.y - abs.y});
    if (child == nullptr) return true;  // the frame, or the row's spare cells
    if (event.action == MouseAction::Down) {
        if (child == month_) focus_slot(0);
        else if (child == year_) focus_slot(1);
        else if (child == calendar_) focus_slot(2);
        mouse_child_ = child;
        // The popup holds the mouse so a press beside it can dismiss it,
        // which means every event inside arrives here and is handed on by
        // hand. If handing it on gave the mouse to something else -- a
        // picker's own list -- the grab is not ours to keep.
        const bool handled = child->on_mouse(event);
        if (context().app != nullptr && context().app->input_capture() != this) mouse_child_ = nullptr;
        return handled;
    }
    return child->on_mouse(event);
}

bool CalendarDropdown::on_key(const KeyEvent& event) {
    // Keys reach the focused control first and only then this popup, so what
    // is left here is what belongs to the popup as a whole.
    if (event.action != KeyAction::Press) return false;
    const ui::View* const focused = context().app != nullptr ? context().app->focused() : nullptr;
    if (event.chord.key == Key::Tab) {
        // Leaving the year field is a commit: the reader typed a year and
        // moved on, which is the same statement as pressing Enter on it.
        if (focused == year_) commit_year();
        return false;  // traversal itself is the application's to run
    }
    if (event.chord.key == Key::Escape) {
        dismiss();
        return true;
    }
    if (event.chord.key == Key::Enter && focused == year_) {
        commit_year();
        return true;
    }
    return false;
}

CalendarDropdown* show_calendar_dropdown(const ui::View& anchor, ui::Application& app, Desktop& desktop) {
    const Rect desktop_abs = desktop.absolute_bounds();
    const Rect anchor_abs = anchor.absolute_bounds();
    auto* raw = desktop.add_popup(std::make_unique<CalendarDropdown>());
    const ui::SizeHint w = raw->horizontal_size_hint();
    const ui::SizeHint h = raw->vertical_size_hint();
    // Hung below the anchor with their RIGHT edges aligned, the way a
    // submenu hangs from the right end of a bar. A clock sits at the right
    // end, so aligning the left edges would send the calendar off the screen
    // and the clamp would then park it against the edge, visibly unrelated
    // to the thing it came from.
    const int x = std::clamp(anchor_abs.right() - w.preferred - desktop_abs.x, 0,
                             std::max(0, desktop.bounds().width - w.preferred));
    const int y = std::clamp(anchor_abs.bottom() - desktop_abs.y, 0,
                             std::max(0, desktop.bounds().height - h.preferred));
    raw->set_bounds(Rect{x, y, w.preferred, h.preferred});
    // Scoped like any other popup: keys and Tab traversal stay inside it, so
    // the month picker and the year field are reachable from the keyboard and
    // the menu bar behind it is not.
    const ui::Application::ModalScopeId scope = app.push_modal(*raw);
    raw->on_dismiss = [&app, &desktop, raw, scope] {
        if (app.input_capture() == raw) app.clear_input_capture();
        app.pop_modal(scope);
        desktop.remove_popup(raw);  // discards ownership -> destroys this view
    };
    app.set_input_capture(raw);
    // The days are what the reader came for; Tab from there reaches the month
    // picker and the year, and comes back.
    app.set_focus(&raw->calendar());
    return raw;
}

void CalendarView::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.menu.dropdown.disabled");
    if (today_role_ == ui::kInvalidRole) today_role_ = context().roles->find("ckv.calendar.today");
    if (marked_role_ == ui::kInvalidRole) marked_role_ = context().roles->find("ckv.calendar.marked");
    if (view_disabled_role_ == ui::kInvalidRole) view_disabled_role_ = context().roles->find("ckv.list.disabled");
    if (selected_inactive_role_ == ui::kInvalidRole)
        selected_inactive_role_ = context().roles->find("ckv.list.selected.inactive");
}

void CalendarView::set_today_provider(std::function<std::optional<DateValue>()> provider) {
    today_provider_ = std::move(provider);
    invalidate();
}

void CalendarView::set_marked_span(std::optional<DateValue> first, std::optional<DateValue> last) {
    // An inverted span marks nothing rather than everything: a caller that
    // hands over its ends the wrong way round has made a mistake, and the
    // helpful reading of a mistake is the quiet one.
    if (first && last && *last < *first) std::swap(first, last);
    marked_first_ = first;
    marked_last_ = last;
    has_marked_span_ = first.has_value() || last.has_value();
    invalidate();
}

std::optional<DateValue> CalendarView::effective_today() const {
    // The provider wins when there is one: it is the answer that can still
    // change, and a fixed today set once is the thing it exists to replace.
    if (today_provider_) return today_provider_();
    return today_;
}

bool CalendarView::within_marked_span(DateValue date) const noexcept {
    if (!has_marked_span_) return false;
    if (marked_first_ && date < *marked_first_) return false;
    if (marked_last_ && *marked_last_ < date) return false;
    return true;
}

namespace {

// The text a key event types, or nothing: a printable character pressed
// without Alt, Ctrl or Super. Terminals report ordinary typing as Key::Char
// and only an IME or a paste as a TextEvent, so a control that takes typing
// reads both; Alt, Ctrl and Super characters stay chords so that commands
// still see them, and Shift is part of producing the text.
std::optional<std::string_view> typed_text(const KeyEvent& event) noexcept {
    if (event.chord.key != Key::Char || event.chord.text.empty()) return std::nullopt;
    if (has_modifier(event.chord.modifiers, Modifier::Alt) || has_modifier(event.chord.modifiers, Modifier::Ctrl) ||
        has_modifier(event.chord.modifiers, Modifier::Super))
        return std::nullopt;
    return std::string_view(event.chord.text);
}

// Where the caret after an entry of `entry` sits in a field `width` columns
// wide starting at `x`: just past its last column, and never past the field.
std::optional<CursorState> entry_caret(const ui::View& view, int x, int text_end, int width) {
    if (width <= 0) return std::nullopt;
    const Rect absolute = view.absolute_bounds();
    return CursorState{true, Point{absolute.x + x + std::clamp(text_end, 0, width - 1), absolute.y},
                       CursorShape::Bar, false};
}

}  // namespace

DatePicker::DatePicker() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{14, 1});
}

void DatePicker::set_value(std::optional<DateValue> value) {
    // The owner's value replaces whatever the reader was typing: an entry
    // belongs to the value it was typed over.
    if (entry_ || !refusal_.empty()) {
        entry_.reset();
        refusal_.clear();
        invalidate();
    }
    // The seed is the caller's today and stays so: a value is the date the
    // reader chose, and making it the seed made "today" follow every choice.
    if (value) {
        value->year = std::clamp(value->year, kFirstCalendarYear, kLastCalendarYear);
        *value = clamp_day(*value);
    } else if (!empty_allowed_) {
        value = seed_;
    }
    if (value_ == value) return;
    value_ = value;
    valid_ = true;
    invalidate();
    if (on_change) on_change(value_);
}

void DatePicker::set_seed(DateValue seed) {
    seed.year = std::clamp(seed.year, kFirstCalendarYear, kLastCalendarYear);
    seed_ = clamp_day(seed);
}

void DatePicker::set_empty_allowed(bool allowed) {
    if (empty_allowed_ == allowed) return;
    empty_allowed_ = allowed;
    if (!empty_allowed_ && !value_) set_value(seed_);
}

void DatePicker::set_valid(bool valid) {
    if (valid_ == valid) return;
    valid_ = valid;
    invalidate();
}

void DatePicker::set_format(DateFormat format) {
    CKV_ASSERT(is_field_order(format.order));
    format_ = std::move(format);
    invalidate();
}

void DatePicker::set_labels(DateTimeLabels labels) {
    labels_ = std::move(labels);
    invalidate();
}

void DatePicker::edit_entry(std::string entry) {
    // An edit supersedes the verdict on what was there before it, as it
    // does in an InputLine: the reader is correcting it.
    entry_ = std::move(entry);
    refusal_.clear();
    invalidate();
}

bool DatePicker::commit_entry() {
    if (!entry_) return true;
    const std::string_view typed = trim_spaces(*entry_);
    std::optional<DateValue> parsed;
    const bool taken = typed.empty() ? empty_allowed_ : (parsed = parse_date(typed, format_, labels_)).has_value();
    if (!taken) {
        // Refused, and left exactly as typed: guessing at a date the reader
        // did not write, or clamping one they did, would put a value in the
        // form that nobody chose.
        refusal_ = labels_.not_a_date;
        invalidate();
        if (on_invalid) {
            const std::string reason = refusal_;
            on_invalid(reason);
        }
        return false;
    }
    entry_.reset();
    refusal_.clear();
    invalidate();
    set_value(parsed);
    return true;
}

void DatePicker::cancel_entry() {
    if (!entry_ && refusal_.empty()) return;
    entry_.reset();
    refusal_.clear();
    invalidate();
}

void DatePicker::set_calendar_host(ui::Application& app, Desktop& desktop) noexcept {
    calendar_app_ = &app;
    calendar_desktop_ = &desktop;
}

bool DatePicker::open_calendar() {
    if (calendar_app_ == nullptr || calendar_desktop_ == nullptr) return false;
    if (calendar_dropdown_ != nullptr) return true;
    CalendarDropdown* const dropdown =
        show_calendar_dropdown(*this, *calendar_app_, *calendar_desktop_);
    calendar_dropdown_ = dropdown;
    const DateValue selected = value_.value_or(seed_);
    dropdown->set_labels(labels_);
    dropdown->show_month(selected);
    dropdown->calendar().set_selected(selected);
    dropdown->calendar().set_today(seed_);
    const std::weak_ptr<void> liveness = lifetime_token();
    dropdown->calendar().on_select = [this, liveness](DateValue date) {
        if (!liveness.expired()) set_value(date);
    };
    dropdown->on_closed = [this, liveness] {
        if (!liveness.expired()) calendar_dropdown_ = nullptr;
    };
    return true;
}

void DatePicker::close_calendar() {
    if (calendar_dropdown_ == nullptr) return;
    CalendarDropdown* const dropdown = calendar_dropdown_;
    calendar_dropdown_ = nullptr;
    dropdown->request_dismiss();  // closing is not choosing
    invalidate();
}

void DatePicker::draw(scene::Painter& painter) {
    const bool enabled = enabled_in_tree();
    const ui::RoleId role = !enabled     ? disabled_role_
                            : !valid()    ? invalid_role_
                            : has_focus() ? focused_role_
                                          : normal_role_;
    const Style style = context().theme->resolve(role);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));
    const bool has_dropdown = calendar_app_ != nullptr && calendar_desktop_ != nullptr && bounds().width >= 2;
    const int value_width = std::max(0, bounds().width - (has_dropdown ? 2 : 0));
    if (has_dropdown) painter.draw_text(Point{bounds().width - 1, 0}, "▾", style);
    if (entry_ && enabled) {
        // The end of the entry, where the reader is typing, when it has
        // outgrown the field; the caret takes the column after it.
        const std::string shown = text::text_width(*entry_) < value_width
                                      ? *entry_
                                      : tail_to_width(*entry_, std::max(0, value_width - 1));
        painter.draw_text(Point{0, 0}, shown, style);
        return;
    }
    if (!value_) {
        painter.draw_text(Point{0, 0}, text::clip_to_width(labels_.no_date, value_width), style);
        return;
    }
    // Away from the keyboard the field shows the caller's own text when it
    // has one; with the keyboard, the layout whose segments the arrows edit.
    const bool editing_face = enabled && has_focus();
    if (!editing_face && format_.format) {
        painter.draw_text(Point{0, 0}, text::clip_to_width(format_.format(*value_), value_width), style);
        return;
    }
    const DateLayout layout = layout_date(*value_, format_, labels_);
    painter.draw_text(Point{0, 0}, text::clip_to_width(layout.text, value_width), style);
    if (!editing_face) return;
    const DateSpan span = layout.spans[field_index(active_field_)];
    if (span.x >= value_width) return;
    Style active = style;
    active.attrs |= Attr::Reverse;
    const int shown_width = std::min(span.width, value_width - span.x);
    painter.draw_text(Point{span.x, 0},
                      text::clip_to_width(field_text(*value_, active_field_, format_, labels_), shown_width), active);
}

std::optional<CursorState> DatePicker::cursor_state() const {
    if (!entry_ || !has_focus() || !enabled_in_tree()) return std::nullopt;
    const bool has_dropdown = calendar_app_ != nullptr && calendar_desktop_ != nullptr && bounds().width >= 2;
    const int value_width = std::max(0, bounds().width - (has_dropdown ? 2 : 0));
    return entry_caret(*this, 0, text::text_width(*entry_), value_width);
}

bool DatePicker::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    if (entry_) {
        switch (event.chord.key) {
            case Key::Enter: commit_entry(); return true;
            case Key::Escape: cancel_entry(); return true;
            case Key::Backspace: edit_entry(without_last_grapheme(*entry_)); return true;
            case Key::Delete: return true;
            case Key::Char:
                if (const std::optional<std::string_view> text = typed_text(event)) {
                    edit_entry(*entry_ + std::string(*text));
                    return true;
                }
                return false;
            case Key::Left:
            case Key::Right:
            case Key::Up:
            case Key::Down:
            case Key::PageUp:
            case Key::PageDown:
                // The segment keys edit the value, so the entry becomes the
                // value first -- or, refused, stays for the reader to fix.
                if (!commit_entry()) return true;
                break;
            default: return false;
        }
    }
    // Left and Right walk the segments in the order they are written.
    const auto position = static_cast<std::size_t>(
        std::find(format_.order.begin(), format_.order.end(), active_field_) - format_.order.begin());
    switch (event.chord.key) {
        case Key::Left:
            active_field_ = format_.order[position > 0 ? position - 1 : 0];
            invalidate();
            return true;
        case Key::Right:
            active_field_ = format_.order[std::min(position + 1, format_.order.size() - 1)];
            invalidate();
            return true;
        case Key::Up: adjust_active(1); return true;
        case Key::Down: adjust_active(-1); return true;
        case Key::PageUp:
            active_field_ = DateField::Month;
            adjust_active(1);
            return true;
        case Key::PageDown:
            active_field_ = DateField::Month;
            adjust_active(-1);
            return true;
        case Key::Delete:
        case Key::Backspace:
            if (empty_allowed_) set_value(std::nullopt);
            return true;
        case Key::Char:
            if (event.chord.text == " ") return open_calendar();
            if (const std::optional<std::string_view> text = typed_text(event)) {
                edit_entry(std::string(*text));
                return true;
            }
            return false;
        default: return false;
    }
}

bool DatePicker::on_text(const TextEvent& event) {
    edit_entry(entry_.value_or(std::string{}) + event.text);
    return true;
}

bool DatePicker::on_mouse(const MouseEvent& event) {
    const bool press = event.action == MouseAction::Down && event.button == MouseButton::Left;
    const bool wheel = event.action == MouseAction::Wheel &&
                       (event.button == MouseButton::WheelUp || event.button == MouseButton::WheelDown);
    if (!press && !wheel) return false;
    // A click or a turn of the wheel edits the value, as the arrows do.
    if (entry_ && !commit_entry()) return true;
    if (press) {
        const int local_x = event.cell.x - absolute_bounds().x;
        if (calendar_app_ != nullptr && calendar_desktop_ != nullptr && local_x >= bounds().width - 2) {
            return open_calendar();
        }
        select_field_at(local_x);
        invalidate();
        return true;
    }
    adjust_active(event.button == MouseButton::WheelUp ? 1 : -1);
    return true;
}

void DatePicker::on_focus(const FocusEvent& event) {
    // Leaving the field is a commit, the same statement as Enter: the reader
    // typed a date and moved on. A refused entry stays, marked, for them to
    // come back to.
    if (!event.gained && entry_) commit_entry();
    invalidate();
}

void DatePicker::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (invalid_role_ == ui::kInvalidRole) invalid_role_ = context().roles->find("ckv.input.invalid");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.input.disabled");
}

void DatePicker::adjust_active(int delta) {
    DateValue adjusted = value_.value_or(seed_);
    if (active_field_ == DateField::Year) {
        adjusted.year = std::clamp(adjusted.year + delta, kFirstCalendarYear, kLastCalendarYear);
        adjusted = clamp_day(adjusted);
    } else if (active_field_ == DateField::Month) {
        const int month_index = adjusted.year * 12 + adjusted.month - 1;
        const int first = kFirstCalendarYear * 12;
        const int last = kLastCalendarYear * 12 + 11;
        const int changed = std::clamp(month_index + delta, first, last);
        adjusted.year = changed / 12;
        adjusted.month = changed % 12 + 1;
        adjusted = clamp_day(adjusted);
    } else {
        adjusted = add_calendar_days(adjusted, delta).value_or(adjusted);
    }
    set_value(adjusted);
}

void DatePicker::select_field_at(int x) {
    // The segment the pointer is on, or the nearest one before it: the
    // separator after a segment belongs to it. Laid out from the value, or
    // from the seed while empty, since that is what the first edit steps.
    const DateLayout layout = layout_date(value_.value_or(seed_), format_, labels_);
    active_field_ = format_.order.front();
    for (const DateField field : format_.order)
        if (x >= layout.spans[field_index(field)].x) active_field_ = field;
}

TimePicker::TimePicker() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{12, 1});
}

void TimePicker::set_value(TimeValue value) {
    value.hour = std::clamp(value.hour, 0, 23);
    value.minute = std::clamp(value.minute, 0, 59);
    value.second = std::clamp(value.second, 0, 59);
    if (value_ == value) return;
    value_ = value;
    invalidate();
    if (on_change) on_change(value_);
}

void TimePicker::set_show_seconds(bool show) {
    show_seconds_ = show;
    if (!show) field_ = std::min(field_, 1);  // the seconds field is gone
    invalidate();
}

void TimePicker::set_24_hour(bool enabled) {
    twenty_four_hour_ = enabled;
    invalidate();
}

void TimePicker::set_meridiem_labels(std::string am, std::string pm) {
    am_label_ = std::move(am);
    pm_label_ = std::move(pm);
    invalidate();
}

void TimePicker::adjust(int delta) {
    TimeValue next = value_;
    if (field_ == 0) next.hour += delta;
    if (field_ == 1) next.minute += delta;
    if (field_ == 2) next.second += delta;
    if (next.hour < 0) next.hour = 23;
    if (next.hour > 23) next.hour = 0;
    if (next.minute < 0) next.minute = 59;
    if (next.minute > 59) next.minute = 0;
    if (next.second < 0) next.second = 59;
    if (next.second > 59) next.second = 0;
    set_value(next);
}

void TimePicker::draw(scene::Painter& painter) {
    const bool enabled = enabled_in_tree();
    const Style style = context().theme->resolve(!enabled     ? disabled_role_
                                                 : !valid_     ? invalid_role_
                                                 : has_focus() ? focused_role_
                                                               : role_);
    const int width = bounds().width;
    painter.fill(Rect{0, 0, width, 1}, Cell::from_grapheme(" ", style));
    const std::string rendered = time_text(value_, show_seconds_, twenty_four_hour_, am_label_, pm_label_);
    painter.draw_text(Point{0, 0}, text::clip_to_width(rendered, width), style);
    // The field the arrows change, as DatePicker marks its own: without it
    // the reader cannot tell whether Up will move the hour or the minute.
    if (!enabled || !has_focus()) return;
    const int start = field_ * 3;  // "hh:mm:ss" -- two digits and a separator per field
    if (start >= width) return;
    Style active = style;
    active.attrs |= Attr::Reverse;
    painter.draw_text(Point{start, 0},
                      text::clip_to_width(rendered.substr(static_cast<std::size_t>(start), 2), std::min(2, width - start)),
                      active);
}

bool TimePicker::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    if (event.chord.key == Key::Up) {
        adjust(1);
        return true;
    }
    if (event.chord.key == Key::Down) {
        adjust(-1);
        return true;
    }
    if (event.chord.key == Key::Left) {
        field_ = std::max(0, field_ - 1);
        invalidate();
        return true;
    }
    if (event.chord.key == Key::Right) {
        field_ = std::min(show_seconds_ ? 2 : 1, field_ + 1);
        invalidate();
        return true;
    }
    return false;
}

bool TimePicker::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    field_ = std::clamp((event.cell.x - absolute_bounds().x) / 3, 0, show_seconds_ ? 2 : 1);
    adjust(1);
    return true;
}

void TimePicker::on_focus(const FocusEvent&) { invalidate(); }
void TimePicker::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (invalid_role_ == ui::kInvalidRole) invalid_role_ = context().roles->find("ckv.input.invalid");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.input.disabled");
}

namespace {

// The English reasons a SpinBox gives when nothing else was asked for. The
// bounds are written with std::to_string, which formats an int in plain
// ASCII digits whatever the process locale.
std::string english_refusal_text(SpinBoxRefusal refusal, int minimum, int maximum) {
    if (refusal == SpinBoxRefusal::NotANumber) return "Not a whole number.";
    return "Enter a number from " + std::to_string(minimum) + " to " + std::to_string(maximum) + ".";
}

}  // namespace

SpinBox::SpinBox() : refusal_text_(english_refusal_text) {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{10, 1});
}
void SpinBox::set_range(int minimum, int maximum) {
    minimum_ = std::min(minimum, maximum);
    maximum_ = std::max(minimum, maximum);
    invalidate();  // Stepper availability can change without the value moving.
    assign(value_);
}
void SpinBox::set_step(int step) { step_ = std::max(1, step); }
void SpinBox::set_value(int value) {
    cancel_entry();
    assign(value);
}
void SpinBox::assign(int value) {
    value = std::clamp(value, minimum_, maximum_);
    if (value_ == value) return;
    value_ = value;
    invalidate();
    if (on_change) on_change(value_);
}
void SpinBox::set_editable(bool editable) {
    if (editable_ == editable) return;
    editable_ = editable;
    invalidate();
    if (!editable_) cancel_entry();
}
void SpinBox::set_refusal_text(std::function<std::string(SpinBoxRefusal, int, int)> text) {
    refusal_text_ = text ? std::move(text) : english_refusal_text;
}
void SpinBox::edit_entry(std::string entry) {
    // An edit supersedes the verdict on what was there before it: the reader
    // is correcting it, and the next commit judges it again.
    entry_ = std::move(entry);
    refusal_.clear();
    invalidate();
}
bool SpinBox::commit_entry() {
    if (!entry_) return true;
    std::string_view text = trim_spaces(*entry_);
    // An optional sign, then digits and nothing else: "12 or so" is not 12,
    // and "+-3" is not -3.
    bool signed_plus = false;
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
        signed_plus = true;
    }
    int parsed = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    std::optional<SpinBoxRefusal> refusal;
    if (text.empty() || (signed_plus && text.front() == '-') || error == std::errc::invalid_argument ||
        end != text.data() + text.size())
        refusal = SpinBoxRefusal::NotANumber;
    else if (error == std::errc::result_out_of_range || parsed < minimum_ || parsed > maximum_)
        refusal = SpinBoxRefusal::OutOfRange;
    if (refusal) {
        // Refused, and never clamped: a number the reader did not type would
        // be a value nobody chose. The entry stays as typed for them to fix.
        refusal_ = refusal_text_(*refusal, minimum_, maximum_);
        invalidate();
        if (on_invalid) {
            const std::string reason = refusal_;
            on_invalid(reason);
        }
        return false;
    }
    entry_.reset();
    refusal_.clear();
    invalidate();
    assign(parsed);
    return true;
}
void SpinBox::cancel_entry() {
    if (!entry_ && refusal_.empty()) return;
    entry_.reset();
    refusal_.clear();
    invalidate();
}
void SpinBox::adjust(int delta) {
    // A step moves the value, so a typed entry becomes the value first -- and
    // one that is refused stays, and nothing moves.
    if (!commit_entry()) return;
    assign(stepped(value_, delta, step_, minimum_, maximum_));
}
void SpinBox::set_presentation(SpinBoxPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    hover_position_.reset();
    size_hint_changed();
    invalidate();
}
ui::SizeHint SpinBox::vertical_size_hint() const {
    const int height = presentation_ == SpinBoxPresentation::Stacked ? 2 : 1;
    return {height, height, height};
}
Rect SpinBox::field_bounds() const noexcept {
    const int width = std::max(0, bounds().width);
    const int reserve = presentation_ == SpinBoxPresentation::Compact ? 2 : presentation_ == SpinBoxPresentation::Separate ? 7 : 3;
    return {0, 0, width == 0 ? 0 : std::max(1, width - reserve), std::min(std::max(0, bounds().height), presentation_ == SpinBoxPresentation::Stacked ? 2 : 1)};
}
Rect SpinBox::decrement_bounds() const noexcept {
    const int start = field_bounds().width;
    const Rect wanted = presentation_ == SpinBoxPresentation::Compact ? Rect{start, 0, 1, 1} : presentation_ == SpinBoxPresentation::Separate ? Rect{start + 1, 0, 3, 1} : Rect{start, 1, 3, 1};
    return wanted.intersected(Rect{0, 0, bounds().width, bounds().height});
}
Rect SpinBox::increment_bounds() const noexcept {
    const int start = field_bounds().width;
    const Rect wanted = presentation_ == SpinBoxPresentation::Compact ? Rect{start + 1, 0, 1, 1} : presentation_ == SpinBoxPresentation::Separate ? Rect{start + 4, 0, 3, 1} : Rect{start, 0, 3, 1};
    return wanted.intersected(Rect{0, 0, bounds().width, bounds().height});
}
SpinBox::Shown SpinBox::shown_entry(int width) const {
    if (!entry_ || width <= 0) return {};
    const std::string_view entry(*entry_);
    int remaining = text::text_width(entry);
    const int available = std::max(0, width - 1);
    std::size_t first = 0;
    while (first < entry.size() && remaining > available) {
        const auto end = text::grapheme_end(entry, first);
        remaining -= text::grapheme_width(entry.substr(first, end - first));
        first = end;
    }
    return {entry.substr(first), remaining};
}
bool SpinBox::can_step(int delta) const noexcept {
    return enabled_in_tree() && valid() && (entry_.has_value() || (delta > 0 ? value_ < maximum_ : value_ > minimum_));
}
std::optional<PointerShape> SpinBox::pointer_shape_at(Point local) const {
    if (decrement_bounds().contains(local)) return can_step(-1) ? PointerShape::Pointer : PointerShape::NotAllowed;
    if (increment_bounds().contains(local)) return can_step(1) ? PointerShape::Pointer : PointerShape::NotAllowed;
    if (field_bounds().contains(local)) return !enabled_in_tree() ? PointerShape::NotAllowed : editable_ ? PointerShape::Text : PointerShape::Default;
    return std::nullopt;
}
void SpinBox::on_hover_changed(bool hovered) {
    if (!hovered) hover_position_.reset();
    invalidate();
}
void SpinBox::draw(scene::Painter& target) {
    if (bounds().width <= 0 || bounds().height <= 0) return;
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    const bool enabled = enabled_in_tree();
    const Style style = context().theme->resolve(!enabled ? disabled_role_ : !valid() ? invalid_role_ : has_focus() ? focused_role_ : role_);
    const Rect field = field_bounds();
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", context().theme->resolve(surface_role_)));
    painter.fill(field, Cell::from_grapheme(" ", style));
    Style text_style = style;
    if (enabled && has_focus()) text_style.attrs |= Attr::Underline;
    if (entry_) painter.draw_text(Point{field.x, field.y}, shown_entry(field.width).text, text_style);
    else {
        std::array<char, 32> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value_);
        const std::string_view number(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
        const bool elided = static_cast<int>(number.size()) > field.width;
        const auto prefix = text::clip_to_width_view(number, elided ? field.width - 1 : field.width);
        painter.draw_text(Point{field.x, field.y}, prefix, text_style);
        if (elided) painter.draw_text(Point{field.x + static_cast<int>(prefix.size()), field.y}, "…", text_style);
    }
    const Style accessory = context().theme->resolve(accessory_role_);
    const auto control = [&](Rect rect, std::string_view glyph, int delta) {
        if (rect.empty()) return;
        auto clipped = painter.clipped(rect);
        Style shown = can_step(delta) ? accessory : accent_style(accessory, context().theme->resolve(disabled_role_));
        if (can_step(delta) && hover_position_ && rect.contains(*hover_position_)) shown = accent_style(shown, context().theme->resolve(accessory_hovered_role_));
        clipped.fill(rect, Cell::from_grapheme(" ", shown));
        clipped.draw_text(Point{rect.x + rect.width / 2, rect.y}, glyph, shown);
    };
    control(decrement_bounds(), presentation_ == SpinBoxPresentation::Stacked ? "▼" : "−", -1);
    control(increment_bounds(), presentation_ == SpinBoxPresentation::Stacked ? "▲" : "+", 1);
}
std::optional<CursorState> SpinBox::cursor_state() const {
    if (!entry_ || !has_focus() || !enabled_in_tree()) return std::nullopt;
    return entry_caret(*this, 0, shown_entry(field_bounds().width).text_end, field_bounds().width);
}
bool SpinBox::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event)) return false;
    const Key key = event.chord.key;
    if (entry_) {
        if (key == Key::Enter) {
            commit_entry();
            return true;
        }
        if (key == Key::Escape) {
            cancel_entry();
            return true;
        }
        if (key == Key::Backspace) {
            edit_entry(without_last_grapheme(*entry_));
            return true;
        }
    }
    if (editable_) {
        if (const std::optional<std::string_view> text = typed_text(event)) {
            edit_entry(entry_.value_or(std::string{}) + std::string(*text));
            return true;
        }
        if (key == Key::Backspace) {
            edit_entry(without_last_grapheme(std::to_string(value_)));
            return true;
        }
    }
    if (key == Key::Up || key == Key::Right) { adjust(1); return true; }
    if (key == Key::Down || key == Key::Left) { adjust(-1); return true; }
    return false;
}
bool SpinBox::on_text(const TextEvent& event) {
    if (!enabled_in_tree() || !editable_) return false;
    edit_entry(entry_.value_or(std::string{}) + event.text);
    return true;
}
bool SpinBox::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || !absolute_bounds().contains(event.cell)) return false;
    if (event.action == MouseAction::Move) {
        const Rect abs = absolute_bounds();
        hover_position_ = Point{event.cell.x - abs.x, event.cell.y - abs.y};
        invalidate();
        return false;
    }
    // The wheel steps the value the way the date picker's segments step:
    // away from the reader is up.
    if (event.action == MouseAction::Wheel && event.button == MouseButton::WheelUp) {
        adjust(1);
        return true;
    }
    if (event.action == MouseAction::Wheel && event.button == MouseButton::WheelDown) {
        adjust(-1);
        return true;
    }
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    if (increment_bounds().contains(local)) { adjust(1); return true; }
    if (decrement_bounds().contains(local)) { adjust(-1); return true; }
    return false;
}
void SpinBox::on_focus(const FocusEvent& event) {
    // Leaving the box commits what was typed, as Enter does; a refused entry
    // stays, marked, for the reader to come back to.
    if (!event.gained && entry_) commit_entry();
    invalidate();
}
void SpinBox::on_attached() {
    accessory_role_ = context().roles->find("ckv.input.accessory");
    accessory_hovered_role_ = context().roles->find("ckv.input.accessory.hovered");
    surface_role_ = context().roles->find("ckv.label.text");
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (invalid_role_ == ui::kInvalidRole) invalid_role_ = context().roles->find("ckv.input.invalid");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.input.disabled");
}

Slider::Slider() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{20, 1});
}
void Slider::set_presentation(SliderPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    invalidate();
}
void Slider::set_show_value(bool show) {
    if (show_value_ == show) return;
    show_value_ = show;
    invalidate();
}
int Slider::track_width() const noexcept {
    const int width = std::max(0, bounds().width);
    if (!show_value_) return width;
    std::array<char, 32> buffer{};
    const auto digits = [&](int value) { return static_cast<int>(std::to_chars(buffer.data(), buffer.data() + buffer.size(), value).ptr - buffer.data()); };
    const int reserved = std::max(digits(minimum_), digits(maximum_)) + 1;
    return width >= reserved + 2 ? width - reserved : width;
}
Rect Slider::track_bounds() const noexcept { return Rect{0, 0, track_width(), std::min(1, std::max(0, bounds().height))}; }
void Slider::set_range(int minimum, int maximum) { minimum_ = std::min(minimum, maximum); maximum_ = std::max(minimum, maximum); set_value(value_); invalidate(); }
void Slider::set_step(int step) { step_ = std::max(1, step); }
void Slider::set_value(int value) {
    value = std::clamp(value, minimum_, maximum_);
    if (value_ == value) return;
    value_ = value;
    invalidate();
    if (on_change) on_change(value_);
}
void Slider::set_ticks(std::vector<SliderTick> ticks) {
    const bool rows_change = ticks_.empty() != ticks.empty();
    ticks_ = std::move(ticks);
    placed_labels_.reserve(ticks_.size());
    invalidate();
    if (rows_change) size_hint_changed();  // the label row comes or goes
}
ui::SizeHint Slider::vertical_size_hint() const {
    if (ticks_.empty()) return ui::View::vertical_size_hint();
    return ui::SizeHint{2, 2, 2};
}
void Slider::adjust(int delta) { set_value(stepped(value_, delta, step_, minimum_, maximum_)); }
int Slider::value_from_x(int x) const {
    if (track_width() <= 1 || maximum_ == minimum_) return minimum_;
    // In 64 bits: the span of an int range needs 32, and times a column 63.
    const std::int64_t span = std::int64_t{maximum_} - minimum_;
    const std::int64_t offset = span * std::clamp(x, 0, track_width() - 1) / (track_width() - 1);
    return static_cast<int>(minimum_ + offset);
}
int Slider::x_from_value(int value) const {
    const int last = std::max(0, track_width() - 1);
    if (maximum_ == minimum_) return 0;
    const std::int64_t span = std::int64_t{maximum_} - minimum_;
    return static_cast<int>((std::int64_t{value} - minimum_) * last / span);
}
void Slider::draw(scene::Painter& target) {
    if (bounds().width <= 0 || bounds().height <= 0) return;
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    // Disabled (D-076): the track and its filled part keep their surfaces,
    // so the value still shows, in the disabled foreground.
    const bool enabled = enabled_in_tree();
    const Style inert = context().theme->resolve(disabled_role_);
    const auto shown = [&](Style style) { return enabled ? style : accent_style(style, inert); };
    const Style track = shown(context().theme->resolve(role_));
    const Style fill = shown(context().theme->resolve(fill_role_));
    const int width = track_width();
    painter.fill(Rect{0, 0, width, 1}, Cell::from_grapheme(presentation_ == SliderPresentation::Block ? "░" : "─", track));
    const int pos = x_from_value(value_);
    painter.fill(Rect{0, 0, pos, 1}, Cell::from_grapheme(presentation_ == SliderPresentation::Block ? "█" : "━", fill));
    const auto in_range = [this](const SliderTick& tick) { return tick.value >= minimum_ && tick.value <= maximum_; };
    // Each tick marks its column on the track, in the weight of the part it
    // falls on; the thumb, drawn last, stands over a mark in its own column.
    for (const SliderTick& tick : ticks_) {
        if (!in_range(tick)) continue;
        const int x = x_from_value(tick.value);
        painter.draw_text(Point{x, 0}, x < pos ? "┯" : "┬", x < pos ? fill : track);
    }
    if (presentation_ == SliderPresentation::ProminentThumb) {
        painter.fill(Rect{std::max(0, pos - 1), 0, std::min(width, pos + 2) - std::max(0, pos - 1), 1}, Cell::from_grapheme(" ", fill));
    }
    painter.draw_text(Point{pos, 0}, enabled && has_focus() ? "◆" : "●", fill);
    if (show_value_ && width < bounds().width) {
        std::array<char, 32> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value_);
        const std::string_view label(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
        painter.fill(Rect{width, 0, bounds().width - width, 1}, Cell::from_grapheme(" ", track));
        painter.draw_text(Point{bounds().width - static_cast<int>(label.size()), 0}, label, track);
    }
    if (ticks_.empty() || bounds().height < 2) return;
    // The labels, in the order given: each centred on its mark, moved inward
    // to stay inside the slider, and left out when it would touch one that
    // is already down -- the rule that makes the same slider label the same
    // ticks every time.
    painter.fill(Rect{0, 1, bounds().width, 1}, Cell::from_grapheme(" ", track));
    placed_labels_.clear();  // Retained [left, right) ranges, prepared by set_ticks.
    for (const SliderTick& tick : ticks_) {
        if (!in_range(tick)) continue;
        const bool elided = text::text_width(tick.label) > width;
        const std::string_view label = text::clip_to_width_view(tick.label, elided ? width - 1 : width);
        const int prefix_width = text::text_width(label);
        const int label_width = prefix_width + (elided ? 1 : 0);
        if (label_width == 0) continue;
        const int left = std::clamp(x_from_value(tick.value) - label_width / 2, 0, std::max(0, width - label_width));
        const int right = left + label_width;
        const bool collides = std::any_of(placed_labels_.begin(), placed_labels_.end(), [left, right](const auto& other) {
            return left < other.second + 1 && other.first < right + 1;
        });
        if (collides) continue;
        placed_labels_.emplace_back(left, right);
        painter.draw_text(Point{left, 1}, label, track);
        if (elided) painter.draw_text(Point{left + prefix_width, 1}, "…", track);
    }
}
bool Slider::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event)) return false;
    if (event.chord.key == Key::Left) { adjust(-1); return true; }
    if (event.chord.key == Key::Right) { adjust(1); return true; }
    if (event.chord.key == Key::Home) { set_value(minimum_); return true; }
    if (event.chord.key == Key::End) { set_value(maximum_); return true; }
    return false;
}
bool Slider::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    if (!track_bounds().contains(Point{event.cell.x - abs.x, event.cell.y - abs.y})) return false;
    set_value(value_from_x(event.cell.x - absolute_bounds().x));
    return true;
}
void Slider::on_focus(const FocusEvent&) { invalidate(); }
void Slider::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.list.normal");
    if (fill_role_ == ui::kInvalidRole) fill_role_ = context().roles->find("ckv.menu.bar.active");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.list.disabled");
}

namespace {
constexpr std::string_view kSearchPrompt = "Search ";
}  // namespace

SearchBox::SearchBox() {
    set_preferred_size(Size{20, 1});
    // The query is typed into a real InputLine, the box's one focus stop, so
    // it edits exactly as every other one-line field does.
    field_ = make<InputLine>();
    field_->on_edited = [this] { query_changed(); };
}
void SearchBox::set_presentation(InputPresentation presentation) {
    if (field_->presentation() == presentation) return;
    field_->set_presentation(presentation);
    hover_position_.reset();
    place_field();
    size_hint_changed();
    invalidate();
}

ui::SizeHint SearchBox::horizontal_size_hint() const {
    const int padding = presentation() == InputPresentation::Flat ? 0 : 2;
    const int preferred = 20 + padding;
    return ui::SizeHint{10 + padding, preferred, ui::kUnboundedExtent};
}

ui::SizeHint SearchBox::vertical_size_hint() const {
    return field_->vertical_size_hint();
}

void SearchBox::set_query(std::string query) {
    if (field_->text() == query) return;
    field_->set_text(std::move(query));
    query_changed();
}
void SearchBox::query_changed() {
    place_field();
    invalidate();
    if (on_change) on_change(query());
}
void SearchBox::commit_to_history() {
    if (field_->empty()) return;
    field_->commit_to_history();
}
void SearchBox::clear() {
    set_query({});
    if (on_clear) on_clear();
}
void SearchBox::set_status(std::string status) {
    if (status_ == status) return;
    status_ = std::move(status);
    place_field();
    invalidate();
}

SearchBox::Layout SearchBox::layout() const {
    Layout parts;
    const int width = std::max(0, bounds().width);
    parts.prompt_width = std::min(text::text_width(kSearchPrompt), width);
    // The clear control only exists while there is something to clear, and
    // only at the right edge, where it is drawn.
    parts.clear_width = field_->empty() ? 0 : std::min(kClearControlWidth, width - parts.prompt_width);
    parts.clear_x = width - parts.clear_width;
    parts.field_x = parts.prompt_width;
    const int between = std::max(0, parts.clear_x - parts.field_x);
    // The status takes what the field can spare, the field keeping its
    // minimum; a status squeezed below two columns (its blank and one cell)
    // says nothing and is left out.
    if (!status_.empty()) {
        const int wanted = text::text_width(status_) + 1;
        const int spare = std::max(0, between - (kMinimumQueryColumns + (presentation() == InputPresentation::Flat ? 0 : 2)));
        parts.status_width = std::min(wanted, spare);
        if (parts.status_width < 2) parts.status_width = 0;
    }
    parts.field_width = between - parts.status_width;
    return parts;
}

void SearchBox::place_field() {
    const Layout parts = layout();
    field_->set_bounds(Rect{parts.field_x, 0, parts.field_width,
                           std::min(input_presentation_height(presentation()), std::max(0, bounds().height))});
    shown_status_ = parts.status_width > 0 ? text::elide_to_width(status_, parts.status_width - 1) : std::string{};
    status_x_ = parts.clear_x - text::text_width(shown_status_);
}

void SearchBox::on_resized() { hover_position_.reset(); place_field(); }

void SearchBox::draw(scene::Painter& painter) {
    // A search box has to look like something you can type into: the field
    // draws itself, in the input surface's own colours, and the box draws the
    // prompt, the status and the clear control around it in the label's.
    const bool enabled = enabled_in_tree();
    const Style label = enabled ? context().theme->resolve(label_role_)
                                : accent_style(context().theme->resolve(label_role_),
                                               context().theme->resolve(label_disabled_role_));
    const int width = bounds().width;
    if (width <= 0 || bounds().height <= 0) return;
    auto clipped = painter.clipped(Rect{0, 0, width, std::min(bounds().height, input_presentation_height(presentation()))});
    clipped.fill(Rect{0, 0, width, std::min(bounds().height, input_presentation_height(presentation()))}, Cell::from_grapheme(" ", label));

    const Layout parts = layout();
    clipped.draw_text(Point{0, 0}, text::clip_to_width_view(kSearchPrompt, parts.prompt_width), label);
    // Right-aligned against the clear control, so a count that changes width
    // as the reader types does not make the clear control jump.
    if (!shown_status_.empty()) clipped.draw_text(Point{status_x_, 0}, shown_status_, label);
    if (parts.clear_width > 0) {
        const bool hovered = hover_position_ && hover_position_->y == 0 &&
            hover_position_->x >= parts.clear_x && hover_position_->x < parts.clear_x + parts.clear_width;
        const Style accessory = enabled ? context().theme->resolve(hovered ? accessory_hovered_role_ : accessory_role_) : label;
        clipped.fill(Rect{parts.clear_x, 0, parts.clear_width, 1}, Cell::from_grapheme(" ", accessory));
        clipped.draw_text(Point{parts.clear_x + (parts.clear_width - 1) / 2, 0}, "x", accessory);
    }
}

bool SearchBox::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    // Escape clears a query; with nothing to clear it is left for whatever
    // encloses the box -- a dialog's cancel, a popup's dismissal.
    if (event.chord.key == Key::Escape) {
        if (field_->empty()) return false;
        clear();
        return true;
    }
    // Enter is the reader saying "this is the query": it goes into the
    // history, and the key goes on to whatever else answers it -- a dialog's
    // default button, or nothing in a viewer that filters live.
    if (event.chord.key == Key::Enter) commit_to_history();
    return false;
}
bool SearchBox::on_mouse(const MouseEvent& event) {
    const Rect absolute = absolute_bounds();
    const Point local{event.cell.x - absolute.x, event.cell.y - absolute.y};
    if (event.action == MouseAction::Move) { hover_position_ = local; invalidate(); return false; }
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left ||
        !Rect{0, 0, bounds().width, std::min(1, bounds().height)}.contains(local)) return false;
    // The clear control only exists while there is something to clear, and
    // only where it is drawn — the right edge.
    const Layout parts = layout();
    if (parts.clear_width > 0 && local.x >= parts.clear_x && local.x < parts.clear_x + parts.clear_width) {
        clear();
        return true;
    }
    // A press anywhere else on the box -- its prompt, its status -- is a
    // request to type in it. The field is the box's focus stop, so the
    // Application's click-to-focus, which looks for a focusable ancestor of
    // what was pressed, would not find it from here.
    if (field_->focusable() && context().app != nullptr) context().app->set_focus(field_);
    return true;
}
std::optional<PointerShape> SearchBox::pointer_shape_at(Point local) const {
    if (!Rect{0, 0, bounds().width, std::min(1, bounds().height)}.contains(local)) return std::nullopt;
    if (!enabled_in_tree()) return PointerShape::NotAllowed;
    const Layout parts = layout();
    return parts.clear_width > 0 && local.x >= parts.clear_x ? PointerShape::Pointer : PointerShape::Text;
}

void SearchBox::on_hover_changed(bool hovered) {
    if (!hovered) hover_position_.reset();
    invalidate();
}

void SearchBox::on_attached() {
    accessory_role_ = context().roles->find("ckv.input.accessory");
    accessory_hovered_role_ = context().roles->find("ckv.input.accessory.hovered");
    if (label_role_ == ui::kInvalidRole) label_role_ = context().roles->find("ckv.label.text");
    if (label_disabled_role_ == ui::kInvalidRole)
        label_disabled_role_ = context().roles->find("ckv.label.disabled");
    place_field();
}

namespace {
// The control whose menu holds the buttons that do not fit.
// A toggle's mark column, as a menu row draws it: the mark and a blank.
constexpr std::string_view kCheckedMark = "x ";
constexpr std::string_view kUncheckedMark = "  ";
}  // namespace

ToolBar::ToolBar() { set_focus_policy(ui::FocusPolicy::TabStop); }

void ToolBar::set_items(std::vector<CommandPresentation> items) {
    cancel_press();
    group_starts_.clear();
    hover_position_.reset();
    items_ = std::move(items);
    preparation_dirty_ = true;
    focused_ = 0;
    overflow_focused_ = false;
    pressed_slot_.reset();
    invalidate();
    size_hint_changed();
}

void ToolBar::set_groups(std::vector<std::vector<CommandPresentation>> groups) {
    std::vector<CommandPresentation> items;
    std::vector<std::size_t> starts;
    for (auto& group : groups) {
        if (group.empty()) continue;
        if (!items.empty()) starts.push_back(items.size());
        for (auto& item : group) items.push_back(std::move(item));
    }
    set_items(std::move(items));
    group_starts_ = std::move(starts);
    size_hint_changed();
    invalidate();
}

void ToolBar::set_presentation(ToolBarPresentation presentation) {
    if (presentation_ == presentation) return;
    cancel_press();
    hover_position_.reset();
    presentation_ = presentation;
    preparation_dirty_ = true;
    size_hint_changed();
    invalidate();
}

int ToolBar::presentation_height() const { return presentation_ == ToolBarPresentation::Framed ? 3 : 1; }
int ToolBar::caption_inset() const { return presentation_ == ToolBarPresentation::Compact ? 1 : 2; }
int ToolBar::control_width() const { return presentation_ == ToolBarPresentation::Compact ? 3 : 5; }

void ToolBar::cancel_press() {
    pressed_slot_.reset();
    pressed_item_.reset();
    pressed_visible_ = false;
    armed_key_.reset();
    const bool owned = std::exchange(press_owns_walk_, false);
    if (owned) end_walk();
    invalidate();
}

void ToolBar::on_resized() { cancel_press(); hover_position_.reset(); }
void ToolBar::on_detaching() { cancel_press(); walk_.reset(); hover_position_.reset(); }
void ToolBar::on_hover_changed(bool now_hovered) {
    if (!now_hovered) hover_position_.reset();
    invalidate();
}

void ToolBar::set_show_chords(bool show) {
    if (show_chords_ == show) return;
    cancel_press();
    hover_position_.reset();
    show_chords_ = show;
    preparation_dirty_ = true;
    invalidate();
    size_hint_changed();
}

void ToolBar::prepare() const {
    const auto* app = context().app;
    const std::uint64_t revision = app ? app->commands().revision() : 0;
    bool rebuild = preparation_dirty_ || prepared_revision_ != revision || prepared_app_ != app;
    if (!rebuild) {
        for (std::size_t i = 0; i < items_.size(); ++i) {
            const bool toggle = app && app->commands().checked(items_[i].command).has_value();
            if (toggle != prepared_[i].toggle) { rebuild = true; break; }
        }
    }
    if (!rebuild) return;
    prepared_.resize(items_.size());
    layout_.slots.reserve(items_.size() + 1);
    layout_.overflow.reserve(items_.size());
    layout_.separators.reserve(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        auto& prepared = prepared_[i];
        prepared.label = parse_mnemonic(item.label.empty() ? command_title(context().app, item.command) : item.label);
        prepared.toggle = app && app->commands().checked(item.command).has_value();
        std::string shortcut = item.chord;
        if (show_chords_ && shortcut.empty() && app) shortcut = app->commands().chord_text(item.command);
        const auto make_face = [&](bool checked) {
            std::string text(static_cast<std::size_t>(caption_inset()), ' ');
            if (prepared.toggle) text += checked ? kCheckedMark : kUncheckedMark;
            text += prepared.label.display;
            if (show_chords_ && !shortcut.empty()) { text += " "; text += shortcut; }
            text.append(static_cast<std::size_t>(caption_inset()), ' ');
            return text;
        };
        prepared.unchecked_face = make_face(false);
        prepared.checked_face = make_face(true);
        prepared.width = text::text_width(prepared.unchecked_face);
    }
    prepared_revision_ = revision;
    prepared_app_ = app;
    preparation_dirty_ = false;
}

const MnemonicText& ToolBar::label(std::size_t item) const {
    return prepared_[item].label;
}

std::string_view ToolBar::face(std::size_t item) const {
    const bool checked = context().app && context().app->commands().checked(items_[item].command).value_or(false);
    return checked ? prepared_[item].checked_face : prepared_[item].unchecked_face;
}

bool ToolBar::available(std::size_t item) const {
    ui::Application* const app = context().app;
    if (app == nullptr || app->commands().find(items_[item].command) == nullptr) return false;
    // Judged for the place the reader is working in: the focus a walk came
    // from while there is one, and wherever the focus is otherwise.
    if (walk_) return app->commands().is_available(items_[item].command, walk_->contexts);
    return app->command_available(items_[item].command);
}

const ToolBar::Layout& ToolBar::layout() const {
    prepare();
    Layout& bar = layout_;
    bar.slots.clear();
    bar.overflow.clear();
    bar.separators.clear();
    const int width = std::max(0, bounds().width);
    if (width == 0 || bounds().height <= 0) return bar;
    int whole = 0;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (i > 0) whole += std::binary_search(group_starts_.begin(), group_starts_.end(), i) ? 3 : 1;
        whole += prepared_[i].width;
    }
    const bool overflows = whole > width;
    const int control = control_width();
    const int limit = overflows ? width - control - 1 : width;
    int x = 0;
    std::size_t index = 0;
    for (; index < items_.size(); ++index) {
        const bool group = std::binary_search(group_starts_.begin(), group_starts_.end(), index);
        const int gap = index == 0 ? 0 : (group ? 3 : 1);
        if (x + gap + prepared_[index].width > limit) break;
        if (group) bar.separators.push_back(x + 1);
        x += gap;
        bar.slots.push_back(Slot{index, x, prepared_[index].width});
        x += prepared_[index].width;
    }
    for (std::size_t i = index; i < items_.size(); ++i) bar.overflow.push_back(i);
    if (overflows) bar.slots.push_back(Slot{std::nullopt, std::max(0, width - control), control});
    return bar;
}

std::vector<std::size_t> ToolBar::shown_items() const {
    std::vector<std::size_t> shown;
    for (const Slot& slot : layout().slots)
        if (slot.item) shown.push_back(*slot.item);
    return shown;
}

std::vector<std::size_t> ToolBar::overflow_items() const { return layout().overflow; }

std::size_t ToolBar::focused_slot(const Layout& bar) const {
    std::optional<std::size_t> control;
    for (std::size_t i = 0; i < bar.slots.size(); ++i) {
        if (!bar.slots[i].item) control = i;
        else if (!overflow_focused_ && *bar.slots[i].item == focused_) return i;
    }
    // On the overflow control, or on a button that has overflowed into it.
    if (control) return *control;
    // On a control a wider bar no longer draws: the first button it held.
    for (std::size_t i = 0; i < bar.slots.size(); ++i)
        if (bar.slots[i].item && *bar.slots[i].item >= focused_) return i;
    return 0;
}

std::optional<std::size_t> ToolBar::focused_item() const {
    const Layout& bar = layout();
    if (bar.slots.empty()) return std::nullopt;
    return bar.slots[focused_slot(bar)].item;
}

void ToolBar::focus_slot(const Layout& bar, std::size_t slot) {
    if (const std::optional<std::size_t> item = bar.slots[slot].item) {
        focused_ = *item;
        overflow_focused_ = false;
    } else {
        overflow_focused_ = true;
    }
    invalidate();
}

int ToolBar::slot_at(const Layout& bar, Point local) const {
    if (!Rect{0, 0, bounds().width, std::min(bounds().height, presentation_height())}.contains(local)) return -1;
    for (std::size_t i = 0; i < bar.slots.size(); ++i)
        if (local.x >= bar.slots[i].x && local.x < bar.slots[i].x + bar.slots[i].width) return static_cast<int>(i);
    return -1;
}

bool ToolBar::slot_available(const Slot& slot) const {
    if (!enabled_in_tree()) return false;
    if (slot.item) return available(*slot.item);
    for (const ui::View* view = parent(); view != nullptr; view = view->parent())
        if (dynamic_cast<const Desktop*>(view) != nullptr) return true;
    return false;
}

std::optional<PointerShape> ToolBar::pointer_shape_at(Point local) const {
    const Layout& bar = layout();
    const int hit = slot_at(bar, local);
    if (hit < 0 || !slot_available(bar.slots[static_cast<std::size_t>(hit)])) return std::nullopt;
    return PointerShape::Pointer;
}

void ToolBar::begin_walk() {
    ui::Application* const app = context().app;
    walk_ = Walk{app->save_focus(), ui::command_context_path(app->focused())};
    app->set_focus(this);
}

std::optional<std::vector<std::string>> ToolBar::end_walk() {
    if (!walk_) return std::nullopt;
    Walk walk = std::move(*walk_);
    walk_.reset();
    invalidate();
    // After forgetting it: handing the focus back is a focus change this bar
    // hears, and a walk still held would be ended twice.
    context().app->restore_focus(walk.focus);
    return std::move(walk.contexts);
}

void ToolBar::activate() {
    cancel_press();
    ui::Application* const app = context().app;
    CKV_ASSERT(app != nullptr);
    focused_ = 0;
    overflow_focused_ = false;
    invalidate();
    if (has_focus()) return;
    begin_walk();
}

void ToolBar::deactivate() { end_walk(); }

bool ToolBar::run_item(std::size_t item) {
    if (!available(item)) return false;
    const ui::CommandId command = items_[item].command;
    ui::Application* const app = context().app;
    // The reader has chosen, and is done with the bar: the focus goes back
    // first, so whatever the command does sees where the reader works.
    if (std::optional<std::vector<std::string>> contexts = end_walk())
        return app->commands().execute(command, *contexts);
    return app->execute_command(command);
}

void ToolBar::open_overflow(const Layout& bar) {
    if (bar.overflow.empty() || context().app == nullptr) return;
    Desktop* const desktop = enclosing_desktop(*this);
    if (desktop == nullptr) return;
    ui::Application& app = *context().app;
    std::vector<MenuItem> items;
    for (const std::size_t item : bar.overflow) {
        if (!items.empty() && std::binary_search(group_starts_.begin(), group_starts_.end(), item))
            items.push_back(MenuItem::separator());
        items.push_back(MenuItem::command(items_[item]));
    }
    // The walk ends before the menu opens, so the menu takes the focus from
    // where the reader works and judges and runs its commands for it.
    end_walk();
    const Rect abs = absolute_bounds();
    const Slot& control = bar.slots.back();
    show_anchored_menu(std::move(items), Rect{abs.x + control.x, abs.y, std::min(control.width, abs.width), std::min(abs.height, presentation_height())}, app, *desktop);
}

bool ToolBar::activate_slot(const Layout& bar, std::size_t slot) {
    if (slot >= bar.slots.size() || !slot_available(bar.slots[slot])) return false;
    if (const std::optional<std::size_t> item = bar.slots[slot].item) return run_item(*item);
    open_overflow(bar);
    return true;
}

void ToolBar::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    const int height = std::min(bounds().height, presentation_height());
    if (bounds().width <= 0 || height <= 0) return;
    auto clipped = painter.clipped(Rect{0, 0, bounds().width, height});
    const Style normal = theme.resolve(role_);
    clipped.fill(Rect{0, 0, bounds().width, height}, Cell::from_grapheme(" ", normal));
    const Layout& bar = layout();
    const int hovered_slot = hover_position_ ? slot_at(bar, *hover_position_) : -1;
    for (const int x : bar.separators)
        clipped.draw_text(Point{x, presentation_height() / 2}, "│", theme.resolve(separator_role_));
    const std::optional<std::size_t> walked = enabled_in_tree() && has_focus() && !bar.slots.empty()
        ? std::optional<std::size_t>{focused_slot(bar)} : std::nullopt;
    for (std::size_t i = 0; i < bar.slots.size(); ++i) {
        const Slot& slot = bar.slots[i];
        const bool acts = slot_available(slot);
        const bool pressed = acts && pressed_slot_ == i && pressed_visible_ && pressed_item_ == slot.item;
        const bool focused = acts && walked == i;
        const bool checked = slot.item && context().app != nullptr &&
            context().app->commands().checked(items_[*slot.item].command).value_or(false);
        const ui::RoleId role = !acts ? disabled_role_ : pressed ? pressed_role_ : focused ? focused_role_ :
            hovered_slot == static_cast<int>(i) ? hovered_role_ : checked ? checked_role_ : role_;
        const Style style = theme.resolve(role);
        const int y = presentation_height() / 2;
        std::string_view whole = slot.item ? face(*slot.item) : (caption_inset() == 1 ? " » " : "  »  ");
        if (presentation_ == ToolBarPresentation::Framed) {
            clipped.fill(Rect{slot.x, 0, slot.width, 3}, Cell::from_grapheme(" ", style));
            clipped.draw_box(Rect{slot.x, 0, slot.width, 3}, scene::LineStyle::Single, style);
            clipped.draw_text(Point{slot.x + 1, y}, whole.substr(1, whole.size() - 2), style);
        } else {
            clipped.draw_text(Point{slot.x, y}, whole, style);
        }
        if (!slot.item) continue;
        const MnemonicText& parsed = label(*slot.item);
        const bool toggle = context().app != nullptr &&
            context().app->commands().checked(items_[*slot.item].command).has_value();
        const int label_x = slot.x + caption_inset() + (toggle ? text::text_width(kCheckedMark) : 0);
        Style caption = style;
        if (focused) caption.attrs |= Attr::Underline;
        const Style accent = acts ? accent_style(caption, theme.resolve(hotkey_role_)) : caption;
        draw_mnemonic(clipped, Point{label_x, y}, parsed, std::max(0, slot.x + slot.width - caption_inset() - label_x), caption, accent);
    }
}

bool ToolBar::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || (!is_press(event) && event.action != KeyAction::Repeat)) return false;
    const KeyChord& chord = event.chord;
    if (armed_key_) {
        if (chord.key == Key::Escape) {
            cancel_press();
            end_walk();
            return true;
        }
        if (chord == *armed_key_) return true;
        cancel_press();
    }
    if (chord.key == Key::Escape && is_press(event) && walk_) {
        end_walk();
        return true;
    }
    const Layout& bar = layout();
    if (bar.slots.empty()) return false;
    const std::size_t focus = focused_slot(bar);
    const std::size_t count = bar.slots.size();
    if (chord.key == Key::Left) { focus_slot(bar, (focus + count - 1) % count); return true; }
    if (chord.key == Key::Right) { focus_slot(bar, (focus + 1) % count); return true; }
    if (chord.key == Key::Home) { focus_slot(bar, 0); return true; }
    if (chord.key == Key::End) { focus_slot(bar, count - 1); return true; }
    if (!is_press(event)) return false;
    if (chord.key == Key::Enter || (chord.key == Key::Char && chord.text == " " && chord.modifiers == Modifier::None)) {
        if (event.reports_release && slot_available(bar.slots[focus])) {
            armed_key_ = chord;
            pressed_slot_ = focus;
            pressed_item_ = bar.slots[focus].item;
            pressed_visible_ = true;
            invalidate();
        } else activate_slot(bar, focus);
        return true;
    }
    // A mnemonic letter runs its button, on the bar or in the overflow menu,
    // typed alone or with Alt as a menu's is.
    if (chord.key == Key::Char && !chord.text.empty() && !has_modifier(chord.modifiers, Modifier::Ctrl) &&
        !has_modifier(chord.modifiers, Modifier::Super)) {
        for (std::size_t i = 0; i < items_.size(); ++i) {
            const MnemonicText& parsed = label(i);
            if (!parsed.mnemonic.empty() && ascii_iequals(parsed.mnemonic, chord.text)) {
                focused_ = i;
                overflow_focused_ = false;
                invalidate();
                run_item(i);
                return true;
            }
        }
    }
    return false;
}

bool ToolBar::on_key_release(const KeyEvent& event) {
    if (!armed_key_ || event.chord != *armed_key_) return false;
    const Layout& bar = layout();
    const auto slot = pressed_slot_;
    const auto item = pressed_item_;
    cancel_press();
    if (slot && *slot < bar.slots.size() && bar.slots[*slot].item == item) activate_slot(bar, *slot);
    return true;
}

bool ToolBar::on_mouse(const MouseEvent& event) {
    const Layout& bar = layout();
    const Rect abs = absolute_bounds();
    const int hit = slot_at(bar, Point{event.cell.x - abs.x, event.cell.y - abs.y});
    if (event.action == MouseAction::Move) {
        const int previous = hover_position_ ? slot_at(bar, *hover_position_) : -1;
        hover_position_ = Point{event.cell.x - abs.x, event.cell.y - abs.y};
        if (previous != hit) invalidate();
    }
    if (event.action == MouseAction::Down) {
        if (event.button != MouseButton::Left || hit < 0 || context().app == nullptr ||
            !slot_available(bar.slots[static_cast<std::size_t>(hit)])) return false;
        cancel_press();
        // A press on a bar that did not have the keyboard borrows it for as
        // long as the button is down, so it can hand it back.
        press_owns_walk_ = !has_focus();
        if (press_owns_walk_) begin_walk();
        const auto slot = static_cast<std::size_t>(hit);
        focus_slot(bar, slot);
        pressed_slot_ = slot;
        pressed_item_ = bar.slots[slot].item;
        pressed_visible_ = true;
        return true;
    }
    if (!pressed_slot_) return false;
    if (armed_key_) return false;
    const bool over = hit >= 0 && static_cast<std::size_t>(hit) == *pressed_slot_ &&
        bar.slots[static_cast<std::size_t>(hit)].item == pressed_item_;
    if (event.action == MouseAction::Move) {
        if (over != pressed_visible_) {
            pressed_visible_ = over;
            invalidate();
        }
        return true;
    }
    if (event.action == MouseAction::Up) {
        const std::size_t slot = *pressed_slot_;
        const bool own_release = event.button == MouseButton::Left || event.button == MouseButton::None;
        pressed_slot_.reset();
        pressed_item_.reset();
        pressed_visible_ = false;
        invalidate();
        // Taken back, or refused: a borrowed keyboard goes home with nothing
        // done, and a walk the reader began stays where it was.
        const bool owned = std::exchange(press_owns_walk_, false);
        if ((!over || !own_release || !activate_slot(bar, slot)) && owned) end_walk();
        return true;
    }
    return false;
}

void ToolBar::on_focus(const FocusEvent& event) {
    // However the keyboard left, the walk is over, and the focus it would
    // have handed back is not this bar's to hand back any more.
    if (!event.gained) {
        pressed_slot_.reset(); pressed_item_.reset(); pressed_visible_ = false; armed_key_.reset();
        press_owns_walk_ = false; walk_.reset();
    }
    invalidate();
}

ui::SizeHint ToolBar::horizontal_size_hint() const {
    prepare();
    int whole = 0;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (i > 0) whole += std::binary_search(group_starts_.begin(), group_starts_.end(), i) ? 3 : 1;
        whole += prepared_[i].width;
    }
    const int control = control_width();
    return ui::SizeHint{std::min(control, std::max(0, whole)), whole, ui::kUnboundedExtent};
}

ui::SizeHint ToolBar::vertical_size_hint() const {
    const int height = presentation_height();
    return ui::SizeHint{height, height, height};
}

void ToolBar::on_attached() {
    preparation_dirty_ = true;
    role_ = context().roles->find("ckv.toolbar.normal");
    focused_role_ = context().roles->find("ckv.toolbar.focused");
    hovered_role_ = context().roles->find("ckv.toolbar.hovered");
    pressed_role_ = context().roles->find("ckv.toolbar.pressed");
    checked_role_ = context().roles->find("ckv.toolbar.checked");
    disabled_role_ = context().roles->find("ckv.toolbar.disabled");
    separator_role_ = context().roles->find("ckv.toolbar.separator");
    hotkey_role_ = context().roles->find("ckv.toolbar.mnemonic");
}

CommandPalette::CommandPalette() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{40, 8});
}
void CommandPalette::set_query(std::string query) { query_ = std::move(query); highlighted_ = 0; invalidate(); }
void CommandPalette::set_invocation_contexts(std::vector<std::string> contexts) {
    invocation_contexts_ = std::move(contexts);
    highlighted_ = 0;
    invalidate();
}
void CommandPalette::set_framed(bool framed) {
    if (framed_ == framed) return;
    framed_ = framed;
    invalidate();
}
std::vector<std::string> CommandPalette::answering_contexts() const {
    if (invocation_contexts_) return *invocation_contexts_;
    return ui::command_context_path(this);
}
bool CommandPalette::enabled_command(ui::CommandId id) const {
    return context().app != nullptr && context().app->commands().is_enabled(id);
}
std::vector<ui::CommandInfo> CommandPalette::filtered_commands() const {
    if (context().app == nullptr) return {};
    const ui::CommandRegistry& registry = context().app->commands();
    const std::vector<std::string> contexts = answering_contexts();
    std::vector<ui::CommandInfo> out;
    for (const ui::CommandInfo& info : registry.all())
        // The reader types what the row shows, so the match is against the
        // title as displayed: "Save as" finds "Save &as...". A command that
        // does not apply here is left out; one that applies but cannot run
        // now is listed, disabled, because "it exists, just not now" is
        // something the reader can act on and "it is not there" is not.
        if (info.visibility == ui::CommandVisibility::Palette &&
            contains_ci(parse_mnemonic(info.title).display, query_) && registry.in_scope(info.id, contexts))
            out.push_back(info);
    return out;
}
std::optional<std::size_t> CommandPalette::resolved_highlight(const std::vector<ui::CommandInfo>& commands) const {
    if (commands.empty()) return std::nullopt;
    const std::size_t chosen = std::min(highlighted_, commands.size() - 1);
    for (std::size_t index = chosen; index < commands.size(); ++index)
        if (enabled_command(commands[index].id)) return index;
    for (std::size_t index = chosen; index-- > 0;)
        if (enabled_command(commands[index].id)) return index;
    return std::nullopt;
}
std::optional<ui::CommandId> CommandPalette::highlighted_command() const {
    const auto commands = filtered_commands();
    const std::optional<std::size_t> row = resolved_highlight(commands);
    if (!row) return std::nullopt;
    return commands[*row].id;
}
void CommandPalette::move_highlight(int delta) {
    const auto commands = filtered_commands();
    const std::optional<std::size_t> from = resolved_highlight(commands);
    if (!from) return;
    // Past the disabled rows, as a menu's highlight goes, and no further than
    // the last enabled row in either direction.
    std::size_t index = *from;
    while (true) {
        if (delta > 0 ? index + 1 >= commands.size() : index == 0) return;
        index = delta > 0 ? index + 1 : index - 1;
        if (!enabled_command(commands[index].id)) continue;
        highlighted_ = index;
        invalidate();
        return;
    }
}
namespace {
// The palette's layout: a two-cell inset on every side — the outer ring for a
// popup's frame, the inner one blank — and a blank row between the search
// field and its results (see CommandPalette::draw).
constexpr int kPaletteInset = 2;
constexpr int kPaletteSearchRow = 2;
constexpr int kPaletteFirstResultRow = 4;
}  // namespace

std::size_t CommandPalette::first_visible(std::size_t highlighted) const {
    // The result rows stop one row short of the bottom when framed, the
    // frame's own row.
    const int visible_rows = std::max(0, bounds().height - kPaletteFirstResultRow - (framed_ ? 1 : 0));
    if (visible_rows == 0 || highlighted < static_cast<std::size_t>(visible_rows)) return 0;
    return highlighted - static_cast<std::size_t>(visible_rows) + 1;
}

std::optional<CursorState> CommandPalette::cursor_state() const {
    if (!has_focus()) return std::nullopt;
    const int field_width = std::max(0, bounds().width - 2 * kPaletteInset);
    if (field_width < 2) return std::nullopt;
    const int caret = std::min(text::text_width(query_), field_width - 2);
    const Rect absolute = absolute_bounds();
    return CursorState{true, Point{absolute.x + kPaletteInset + 1 + caret, absolute.y + kPaletteSearchRow},
                       CursorShape::Bar, false};
}

void CommandPalette::draw(scene::Painter& painter) {
    const Style input = context().theme->resolve(input_role_);
    const Style normal = context().theme->resolve(result_role_);
    const Style selected = context().theme->resolve(selected_role_);
    const Style disabled = context().theme->resolve(disabled_role_);
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));
    // A popup has an edge. Drawn in the outer ring of the inset, so the
    // search field and the results sit exactly where they do unframed.
    if (framed_) painter.draw_box(Rect{0, 0, bounds().width, bounds().height}, scene::LineStyle::Single, normal);

    // The palette deliberately reserves a two-cell inset on every side: the
    // search field reads as an edit control, while its results read as a
    // separate, scrollable choice surface.  This is also what keeps a
    // constrained palette legible when it sits above document content.
    constexpr int kInset = kPaletteInset;
    constexpr int kSearchRow = kPaletteSearchRow;
    constexpr int kFirstResultRow = kPaletteFirstResultRow;
    const int result_width = std::max(0, bounds().width - 2 * kInset);
    painter.fill(Rect{kInset, kSearchRow, result_width, 1}, Cell::from_grapheme(" ", input));
    painter.draw_text(Point{kInset + 1, kSearchRow},
                      text::clip_to_width(query_, std::max(0, result_width - 1)), input);

    const auto commands = filtered_commands();
    const std::optional<std::size_t> highlighted = resolved_highlight(commands);
    const int visible_rows = std::max(0, bounds().height - kFirstResultRow - (framed_ ? 1 : 0));
    const std::size_t first = first_visible(highlighted.value_or(0));
    const bool needs_scrollbar = commands.size() > static_cast<std::size_t>(visible_rows);
    const int text_columns = std::max(0, result_width - (needs_scrollbar ? 1 : 0));
    const Style hotkey = context().theme->resolve(context().roles->find("ckv.hotkey"));
    const Style mnemonic = accent_style(normal, hotkey);
    const Style selected_mnemonic = accent_style(selected, hotkey);
    for (std::size_t index = first;
         index < commands.size() && index - first < static_cast<std::size_t>(visible_rows); ++index) {
        const bool enabled = enabled_command(commands[index].id);
        const bool is_highlighted = highlighted == index;
        const Style style = !enabled ? disabled : is_highlighted ? selected : normal;
        const int row = kFirstResultRow + static_cast<int>(index - first);
        painter.fill(Rect{kInset, row, result_width, 1}, Cell::from_grapheme(" ", style));
        // A palette that hides the binding teaches nobody the keyboard: a
        // command reached here once is meant to be reached by its chord
        // next time, so the chord travels with the title.
        const auto parsed = parse_mnemonic(commands[index].title);
        const int title_columns = std::max(0, text_columns - 1);
        // A disabled row's mnemonic is not accented: it names a key that
        // would do nothing.
        draw_mnemonic(painter, Point{kInset + 1, row}, parsed, title_columns, style,
                      !enabled ? style : is_highlighted ? selected_mnemonic : mnemonic);
        const std::string chord = context().app->commands().chord_text(commands[index].id);
        if (chord.empty()) continue;
        const std::string annotation = "  (" + chord + ")";
        const int title_width = text::text_width(parsed.display);
        const int annotation_columns = title_columns - title_width;
        if (annotation_columns <= 0) continue;
        painter.draw_text(Point{kInset + 1 + title_width, row},
                          text::clip_to_width(annotation, annotation_columns), style);
    }
    if (needs_scrollbar && visible_rows > 0) {
        const int column = kInset + result_width - 1;
        const Style track = context().theme->resolve(context().roles->find("ckv.scrollbar.track"));
        const Style thumb = context().theme->resolve(context().roles->find("ckv.scrollbar.thumb"));
        // Same vocabulary as Scrollbar: arrows, a medium-shade page area
        // and a single-cell indicator, so a palette's gutter is not a
        // second, different-looking kind of scrollbar.
        painter.fill(Rect{column, kFirstResultRow, 1, visible_rows}, Cell::from_grapheme("▒", track));
        painter.draw_text(Point{column, kFirstResultRow}, "▲", track);
        painter.draw_text(Point{column, kFirstResultRow + visible_rows - 1}, "▼", track);
        const int travel = std::max(0, visible_rows - 3);
        const std::size_t maximum_first = commands.size() - static_cast<std::size_t>(visible_rows);
        const int thumb_row = kFirstResultRow + 1 +
                              (maximum_first == 0 ? 0 : static_cast<int>(first * static_cast<std::size_t>(travel) /
                                                                            maximum_first));
        painter.draw_text(Point{column, thumb_row}, "■", thumb);
    }
}
void CommandPalette::dismiss() {
    if (dismissed_ || !on_dismiss) return;
    dismissed_ = true;
    // Copied first: the call may destroy this palette, and the function with
    // it.
    const std::function<void()> callback = on_dismiss;
    callback();
}
bool CommandPalette::run(ui::CommandId id) {
    ui::Application* const app = context().app;
    if (app == nullptr) return false;
    // Everything the command needs is taken off the palette before a popup
    // tears itself down, so the command runs after the focus has gone back
    // to where the reader was — a command that acts on "the active window" or
    // "the focused field" finds theirs, not the palette.
    const std::optional<std::vector<std::string>> contexts = invocation_contexts_;
    dismiss();
    if (contexts) return app->commands().execute(id, *contexts);
    return app->execute_command(id);
}
bool CommandPalette::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    // A typed character reaches the query the way a terminal reports it:
    // as a Key::Char event. Alt, Ctrl and Super chords stay the registry's.
    if (event.chord.key == Key::Char && !event.chord.text.empty() &&
        !has_modifier(event.chord.modifiers, Modifier::Alt) && !has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
        !has_modifier(event.chord.modifiers, Modifier::Super))
        return on_text(TextEvent{event.chord.text, false});
    switch (event.chord.key) {
        case Key::Down: move_highlight(1); return true;
        case Key::Up: move_highlight(-1); return true;
        case Key::Backspace:
            if (query_.empty()) return false;
            set_query(without_last_grapheme(query_));
            return true;
        case Key::Enter: {
            // A popup owns Enter whatever happens: passing it on would reach
            // whatever is behind the palette the reader is looking at. Read
            // first, since running the command may destroy the popup.
            const bool popup = static_cast<bool>(on_dismiss);
            const std::optional<ui::CommandId> command = highlighted_command();
            const bool ran = command && run(*command);
            return ran || popup;
        }
        case Key::Escape:
            if (!on_dismiss) return false;
            dismiss();
            return true;
        default: return false;
    }
}
bool CommandPalette::on_text(const TextEvent& event) { set_query(query_ + event.text); return true; }
bool CommandPalette::on_mouse(const MouseEvent& event) {
    const Rect abs = absolute_bounds();
    if (!abs.contains(event.cell)) {
        // Only a popup holding the input capture sees presses outside it:
        // the light dismissal a menu has. Read first: dismissing may destroy
        // the palette.
        const bool popup = static_cast<bool>(on_dismiss);
        if (event.action == MouseAction::Down) dismiss();
        return popup;
    }
    if (event.action == MouseAction::Wheel) {
        if (event.button == MouseButton::WheelUp) move_highlight(-1);
        if (event.button == MouseButton::WheelDown) move_highlight(1);
        return true;
    }
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return true;
    const auto commands = filtered_commands();
    const int row = event.cell.y - abs.y - kPaletteFirstResultRow;
    const int column = event.cell.x - abs.x;
    const int visible_rows = std::max(0, bounds().height - kPaletteFirstResultRow - (framed_ ? 1 : 0));
    if (row < 0 || row >= visible_rows || column < kPaletteInset || column >= bounds().width - kPaletteInset)
        return true;
    const std::size_t index = first_visible(resolved_highlight(commands).value_or(0)) + static_cast<std::size_t>(row);
    if (index >= commands.size() || !enabled_command(commands[index].id)) return true;
    highlighted_ = index;
    invalidate();
    run(commands[index].id);
    return true;
}
void CommandPalette::on_focus(const FocusEvent&) { invalidate(); }
void CommandPalette::on_attached() {
    if (input_role_ == ui::kInvalidRole) input_role_ = context().roles->find("ckv.input.normal");
    if (result_role_ == ui::kInvalidRole) result_role_ = context().roles->find("ckv.option.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.option.focused");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.option.disabled");
}

CommandPalette* show_command_palette(ui::Application& app, Desktop& desktop) {
    // Read before the palette takes the focus: it lists what the reader's
    // own place allows, and gives the focus back there when it goes.
    ui::View* const invoker = app.focused();
    const detail::DialogFocusRestore focus_restore{invoker};
    auto palette = std::make_unique<CommandPalette>();
    palette->set_framed(true);
    palette->set_invocation_contexts(ui::command_context_path(invoker));
    CommandPalette* const raw = desktop.add_popup(std::move(palette));
    // Centred across the content, near its top: where the eye already is
    // after reaching for a chord, and clear of the text being worked on in
    // the middle of the screen.
    const Rect area = desktop.content_area();
    const int width = std::min(56, area.width);
    const int height = std::min(14, area.height);
    raw->set_bounds(Rect{area.x + (area.width - width) / 2, area.y + std::min(1, area.height - height), width,
                         height});
    const ui::Application::ModalScopeId scope = app.push_modal(*raw);
    raw->on_dismiss = [&app, &desktop, raw, scope, focus_restore] {
        if (app.input_capture() == raw) app.clear_input_capture();
        app.pop_modal(scope);
        focus_restore.restore(app);
        desktop.remove_popup(raw);  // discards ownership -> destroys the palette
    };
    app.set_input_capture(raw);
    app.set_focus(raw);
    return raw;
}

void BreadcrumbBar::set_segments(std::vector<std::string> segments) {
    ellipsis_pressed_ = false;
    cached_layout_.reset();
    segments_ = std::move(segments);
    focused_ = 0;
    ellipsis_focused_ = false;
    invalidate();
}
void BreadcrumbBar::set_separator(std::string separator) {
    ellipsis_pressed_ = false;
    cached_layout_.reset();
    separator_ = std::move(separator);
    invalidate();
}
void BreadcrumbBar::set_presentation(BreadcrumbPresentation presentation) {
    if (presentation_ == presentation) return;
    ellipsis_pressed_ = false;
    presentation_ = presentation;
    cached_layout_.reset();
    invalidate();
}
std::string_view BreadcrumbBar::separator_text() const noexcept {
    return presentation_ == BreadcrumbPresentation::Connected ? std::string_view("›") : std::string_view(separator_);
}
const BreadcrumbBar::Layout& BreadcrumbBar::layout() const {
    if (!cached_layout_) cached_layout_ = build_layout();
    return *cached_layout_;
}
std::optional<PointerShape> BreadcrumbBar::pointer_shape_at(Point local) const {
    if (local.y != 0 || local.x < 0 || local.x >= bounds().width || bounds().height <= 0) return std::nullopt;
    if (stop_at_x(layout(), local.x) < 0) return std::nullopt;
    return enabled_in_tree() ? PointerShape::Pointer : PointerShape::NotAllowed;
}

namespace {
// The stop that stands for the segments elision hides.
constexpr std::string_view kBreadcrumbEllipsis = "…";

// `text` in at most `columns` cells: whole when it fits, elided when a cell
// is left for the mark, and nothing at all when there is no room.
std::string fitted(const std::string& text, int columns) {
    if (columns <= 0) return {};
    return text::elide_to_width(text, columns);
}
}  // namespace

BreadcrumbBar::Layout BreadcrumbBar::build_layout() const {
    Layout bar;
    const std::size_t count = segments_.size();
    if (count == 0) return bar;
    const int width = std::max(0, bounds().width);
    const int separator = text::text_width(separator_text());
    const int padding = presentation_ == BreadcrumbPresentation::Plain ? 0 : 2;
    const auto segment_width = [this, padding](std::size_t index) { return text::text_width(segments_[index]) + padding; };
    const auto place = [&](std::vector<std::pair<std::optional<std::size_t>, std::string>> parts) {
        int x = 0;
        for (auto& [segment, text] : parts) {
            if (padding) text = " " + text + " ";
            bar.stops.push_back(Stop{segment, x, text});
            x += text::text_width(text) + separator;
        }
    };

    int whole = separator * static_cast<int>(count - 1);
    for (std::size_t i = 0; i < count; ++i) whole += segment_width(i);
    if (whole <= width) {
        std::vector<std::pair<std::optional<std::size_t>, std::string>> parts;
        for (std::size_t i = 0; i < count; ++i) parts.emplace_back(i, segments_[i]);
        place(std::move(parts));
        return bar;
    }

    const std::size_t last = count - 1;
    const int ellipsis = count > 2 ? text::text_width(kBreadcrumbEllipsis) + padding + separator : 0;
    // Keep the longest run of segments before the last that still fits
    // beside the first segment and the ellipsis: those nearest the last are
    // the ones the reader is most likely to go back to.
    if (count > 2) {
        int tail = segment_width(last);
        std::size_t kept_from = last;
        while (kept_from > 1) {
            const int wider = tail + segment_width(kept_from - 1) + separator;
            if (segment_width(0) + separator + ellipsis + wider > width) break;
            tail = wider;
            --kept_from;
        }
        if (segment_width(0) + separator + ellipsis + tail <= width) {
            std::vector<std::pair<std::optional<std::size_t>, std::string>> parts;
            parts.emplace_back(std::size_t{0}, segments_[0]);
            parts.emplace_back(std::nullopt, std::string(kBreadcrumbEllipsis));
            for (std::size_t i = kept_from; i < count; ++i) parts.emplace_back(i, segments_[i]);
            for (std::size_t i = 1; i < kept_from; ++i) bar.hidden.push_back(i);
            place(std::move(parts));
            return bar;
        }
    }

    // Not even the first segment, the ellipsis and the last fit: the two
    // segments share what room there is, the last -- where the reader is --
    // keeping the larger part of it.
    const int room = std::max(0, width - separator - ellipsis);
    const int last_room = count == 1 ? width
                                     : std::min(segment_width(last), std::max(room - segment_width(0), (room + 1) / 2));
    const int first_room = count == 1 ? 0 : std::min(segment_width(0), room - last_room);
    std::vector<std::pair<std::optional<std::size_t>, std::string>> parts;
    if (count > 1) parts.emplace_back(std::size_t{0}, fitted(segments_[0], std::max(0, first_room - padding)));
    if (count > 2) {
        parts.emplace_back(std::nullopt, std::string(kBreadcrumbEllipsis));
        for (std::size_t i = 1; i < last; ++i) bar.hidden.push_back(i);
    }
    parts.emplace_back(last, fitted(segments_[last], std::max(0, last_room - padding)));
    place(std::move(parts));
    return bar;
}

std::vector<std::size_t> BreadcrumbBar::hidden_segments() const { return layout().hidden; }

std::size_t BreadcrumbBar::focused_stop(const Layout& bar) const {
    std::optional<std::size_t> ellipsis;
    for (std::size_t i = 0; i < bar.stops.size(); ++i) {
        if (!bar.stops[i].segment) ellipsis = i;
        else if (!ellipsis_focused_ && *bar.stops[i].segment == focused_) return i;
    }
    // On the ellipsis, or on a segment elided into it.
    if (ellipsis) return *ellipsis;
    // On an ellipsis a wider bar no longer draws: the first segment it stood
    // for, which is the second stop now.
    return std::min<std::size_t>(ellipsis_focused_ ? 1 : 0, bar.stops.empty() ? 0 : bar.stops.size() - 1);
}

std::optional<std::size_t> BreadcrumbBar::focused_segment() const {
    const Layout& bar = layout();
    if (bar.stops.empty()) return std::nullopt;
    return bar.stops[focused_stop(bar)].segment;
}

void BreadcrumbBar::focus_stop(const Layout& bar, std::size_t stop) {
    if (const std::optional<std::size_t> segment = bar.stops[stop].segment) {
        focused_ = *segment;
        ellipsis_focused_ = false;
    } else {
        ellipsis_focused_ = true;
    }
    invalidate();
}

int BreadcrumbBar::stop_at_x(const Layout& bar, int x) const {
    for (std::size_t i = 0; i < bar.stops.size(); ++i) {
        const int start = bar.stops[i].x;
        if (x >= start && x < start + text::text_width(bar.stops[i].text)) return static_cast<int>(i);
    }
    return -1;
}

void BreadcrumbBar::activate_segment(std::size_t index) {
    focused_ = index;
    ellipsis_focused_ = false;
    invalidate();
    if (on_activate) on_activate(index);
}

bool BreadcrumbBar::activate_stop(const Layout& bar, std::size_t stop) {
    if (!on_activate) return false;
    if (const std::optional<std::size_t> segment = bar.stops[stop].segment) {
        activate_segment(*segment);
        return true;
    }
    // The ellipsis: the segments it stands for, as a menu hanging from it.
    Desktop* const desktop = enclosing_desktop(*this);
    if (desktop == nullptr || context().app == nullptr) return false;
    ellipsis_focused_ = true;
    invalidate();
    std::vector<MenuItem> items;
    const std::weak_ptr<void> liveness = lifetime_token();
    for (const std::size_t index : bar.hidden) {
        // A segment is plain text, so an '&' in it is a literal one.
        std::string label;
        for (const char c : segments_[index]) label += c == '&' ? std::string("&&") : std::string(1, c);
        items.push_back(MenuItem::action(std::move(label), [this, liveness, index] {
            if (!liveness.expired()) activate_segment(index);
        }));
    }
    const Rect abs = absolute_bounds();
    show_anchored_menu(std::move(items),
                       Rect{abs.x + bar.stops[stop].x, abs.y, text::text_width(bar.stops[stop].text), 1},
                       *context().app, *desktop);
    return true;
}

void BreadcrumbBar::draw(scene::Painter& target) {
    if (bounds().width <= 0 || bounds().height <= 0) return;
    auto painter = target.clipped(Rect{0, 0, bounds().width, 1});
    const bool enabled = enabled_in_tree();
    const Style normal = enabled ? context().theme->resolve(role_)
                                 : accent_style(context().theme->resolve(role_), context().theme->resolve(disabled_role_));
    const Style focused = context().theme->resolve(focused_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", normal));
    const Layout& bar = layout();
    if (bar.stops.empty()) return;
    const std::size_t focus = focused_stop(bar);
    for (std::size_t i = 0; i < bar.stops.size(); ++i) {
        const Stop& stop = bar.stops[i];
        if (stop.x >= bounds().width) break;
        Style style = enabled && has_focus() && i == focus ? focused : normal;
        if (enabled && stop.segment && *stop.segment + 1 == segments_.size()) style.attrs |= Attr::Bold;
        if (enabled && has_focus() && i == focus) style.attrs |= Attr::Underline;
        painter.draw_text(Point{stop.x, 0}, text::clip_to_width_view(stop.text, bounds().width - stop.x), style);
        if (i + 1 < bar.stops.size())
            painter.draw_text(Point{stop.x + text::text_width(stop.text), 0}, separator_text(), normal);
    }
}
bool BreadcrumbBar::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event) || segments_.empty()) return false;
    const Layout& bar = layout();
    const std::size_t focus = focused_stop(bar);
    if (event.chord.key == Key::Left && focus > 0) { focus_stop(bar, focus - 1); return true; }
    if (event.chord.key == Key::Right && focus + 1 < bar.stops.size()) { focus_stop(bar, focus + 1); return true; }
    if (event.chord.key == Key::Enter) return activate_stop(bar, focus);
    return false;
}
bool BreadcrumbBar::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree()) { ellipsis_pressed_ = false; return false; }
    const Rect abs = absolute_bounds();
    if (event.cell.y != abs.y || event.cell.x < abs.x || event.cell.x >= abs.x + abs.width || abs.height <= 0) {
        if (event.action == MouseAction::Up) ellipsis_pressed_ = false;
        return false;
    }
    if (event.button != MouseButton::Left && event.button != MouseButton::None) return false;
    const Layout& bar = layout();
    const int stop = stop_at_x(bar, event.cell.x - absolute_bounds().x);
    if (event.action == MouseAction::Down) {
        if (event.button != MouseButton::Left || stop < 0) return false;
        const auto index = static_cast<std::size_t>(stop);
        focus_stop(bar, index);
        // A segment acts at once. The ellipsis opens its menu when the click
        // completes: a menu put up on the way down would take the release
        // that follows as a click outside it and close again.
        if (bar.stops[index].segment) activate_stop(bar, index);
        else ellipsis_pressed_ = true;
        return true;
    }
    if (event.action == MouseAction::Up && ellipsis_pressed_) {
        ellipsis_pressed_ = false;
        if (stop >= 0 && !bar.stops[static_cast<std::size_t>(stop)].segment)
            activate_stop(bar, static_cast<std::size_t>(stop));
        return true;
    }
    return false;
}
void BreadcrumbBar::on_focus(const FocusEvent& event) { if (!event.gained) ellipsis_pressed_ = false; invalidate(); }
BreadcrumbBar::BreadcrumbBar() { set_focus_policy(ui::FocusPolicy::TabStop); }

void BreadcrumbBar::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.label.text");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.list.selected");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.label.disabled");
}

namespace {
// The gap between the name column and the value column.
constexpr int kPropertyGutter = 2;

bool property_true(std::string_view value) noexcept { return value == "true"; }

// A bound as the reader typed numbers: whole for an Integer row, the shortest
// exact spelling for a Real one. Locale-free either way.
std::string bound_text(double bound, PropertyKind kind) {
    char buffer[64];
    const auto result = kind == PropertyKind::Integer
                            ? std::to_chars(std::begin(buffer), std::end(buffer), static_cast<long long>(bound))
                            : std::to_chars(std::begin(buffer), std::end(buffer), bound);
    return result.ec == std::errc{} ? std::string(buffer, result.ptr) : std::string{};
}
}  // namespace

PropertyInspector::PropertyInspector() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    // Built once and hidden, one shown at a time over the value being edited.
    // An editor's own callback (a choice picked from its list) may end the
    // edit, which must not destroy the editor while it is still running.
    text_editor_ = make<InputLine>();
    choice_editor_ = make<ComboBox>(ComboBoxMode::PickOnly);
    date_editor_ = make<DatePicker>();
    time_editor_ = make<TimePicker>();
    for (ui::View* editor : {static_cast<ui::View*>(text_editor_), static_cast<ui::View*>(choice_editor_),
                             static_cast<ui::View*>(date_editor_), static_cast<ui::View*>(time_editor_)})
        editor->set_visible(false);
    date_editor_->set_empty_allowed(false);
    // A refused value stays refused only until it is changed: the reason is
    // about the value the reader tried, not the one they are fixing it into.
    text_editor_->on_edited = [this] { clear_reason(); };
    choice_editor_->on_text_changed = [this](const std::string&) { clear_reason(); };
    date_editor_->on_change = [this](std::optional<DateValue>) { clear_reason(); };
    time_editor_->on_change = [this](TimeValue) { clear_reason(); };
    // A choice from the list is the whole of a Choice edit.
    choice_editor_->on_select = [this](std::size_t) { commit_edit(); };
}
void PropertyInspector::set_items(std::vector<PropertyItem> items) {
    // An edit belongs to the value it began on, and that value is gone.
    cancel_edit();
    clear_reason();
    items_ = std::move(items);
    cursor_ = items_.empty() ? -1 : 0;
    rows_.resize(items_.size());
    rebuild_rows();
    invalidate();
}
void PropertyInspector::set_messages(PropertyInspectorMessages messages) {
    messages_ = std::move(messages);
}
int PropertyInspector::value_x() const {
    int widest = 0;
    for (const PropertyItem& item : items_) widest = std::max(widest, text::text_width(item.name));
    const int names = std::min(widest, std::max(0, (bounds().width - kPropertyGutter) / 2));
    return names + kPropertyGutter;
}
void PropertyInspector::set_presentation(PropertyPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    rebuild_rows();
    place_editor();
    invalidate();
}
void PropertyInspector::set_banded_rows(bool banded) {
    if (banded_rows_ == banded) return;
    banded_rows_ = banded;
    invalidate();
}
bool PropertyInspector::begins_group(std::size_t index) const noexcept {
    return presentation_ == PropertyPresentation::Sectioned && !items_[index].group.empty() &&
           (index == 0 || items_[index].group != items_[index - 1].group);
}
void PropertyInspector::rebuild_rows() {
    int row = 0;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (begins_group(i)) row += i == 0 ? 1 : 2;
        rows_[i] = row++;
        if (!reason_.empty() && static_cast<int>(i) == reason_index_) ++row;
    }
    content_rows_ = row;
    size_hint_changed();
}
ui::SizeHint PropertyInspector::vertical_size_hint() const {
    return {0, std::max(content_rows_, preferred_size().height), ui::kUnboundedExtent};
}
int PropertyInspector::row_of(std::size_t index) const noexcept { return rows_[index]; }
int PropertyInspector::item_at_row(int y) const noexcept {
    const auto found = std::lower_bound(rows_.begin(), rows_.end(), y);
    return found != rows_.end() && *found == y ? static_cast<int>(found - rows_.begin()) : -1;
}
Rect PropertyInspector::item_bounds(std::size_t index) const noexcept {
    if (index >= items_.size()) return {};
    return Rect{0, row_of(index), std::max(0, bounds().width), 1}.intersected(Rect{0, 0, bounds().width, bounds().height});
}
Rect PropertyInspector::value_bounds(std::size_t index) const {
    const Rect row = item_bounds(index);
    const int x = value_x();
    return Rect{x, row.y, std::max(0, bounds().width - x), 1}.intersected(row);
}
std::optional<PointerShape> PropertyInspector::pointer_shape_at(Point local) const {
    if (!Rect{0, 0, bounds().width, bounds().height}.contains(local)) return std::nullopt;
    const int index = item_at_row(local.y);
    if (index < 0) return std::nullopt;
    if (!enabled_in_tree()) return PointerShape::NotAllowed;
    if (local.x < value_x()) return PointerShape::Default;
    if (!items_[static_cast<std::size_t>(index)].editable) return PointerShape::Default;
    return items_[static_cast<std::size_t>(index)].kind == PropertyKind::Text ||
                   items_[static_cast<std::size_t>(index)].kind == PropertyKind::Integer ||
                   items_[static_cast<std::size_t>(index)].kind == PropertyKind::Real
               ? PointerShape::Text : PointerShape::Pointer;
}
void PropertyInspector::draw(scene::Painter& target) {
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    const bool enabled = enabled_in_tree();
    const Style inert = context().theme->resolve(disabled_role_);
    const Style normal = enabled ? context().theme->resolve(role_) : inert;
    const bool active = has_focus() || (editor_ != nullptr && editor_->has_focus());
    const Style selected = enabled && active
                               ? context().theme->resolve(selected_role_)
                               : accent_style(context().theme->resolve(selected_inactive_role_), normal);
    const int width = bounds().width;
    const int value_column = value_x();
    const int names = std::max(0, value_column - kPropertyGutter);
    painter.fill(Rect{0, 0, width, bounds().height}, Cell::from_grapheme(" ", normal));
    if (presentation_ == PropertyPresentation::Divided)
        painter.vline(Point{names, 0}, bounds().height, scene::LineStyle::Single,
                      accent_style(normal, context().theme->resolve(divider_role_)));
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const int y = row_of(i);
        if (begins_group(i) && y - 1 < bounds().height) {
            Style heading = enabled ? accent_style(normal, context().theme->resolve(heading_role_)) : inert;
            heading.attrs |= Attr::Bold;
            painter.draw_text(Point{0, y - 1}, text::clip_to_width_view(items_[i].group, width), heading);
        }
        if (y >= bounds().height) break;
        const bool cursor_row = static_cast<int>(i) == cursor_;
        const Style style = cursor_row ? selected : enabled && banded_rows_ && i % 2
            ? context().theme->resolve(banded_role_) : normal;
        painter.fill(Rect{0, y, width, 1}, Cell::from_grapheme(" ", style));
        painter.draw_text(Point{0, y}, text::clip_to_width_view(items_[i].name, names), style);
        if (presentation_ == PropertyPresentation::Divided)
            painter.vline(Point{names, y}, 1, scene::LineStyle::Single,
                          accent_style(style, context().theme->resolve(divider_role_)));
        if (value_column >= width || (cursor_row && editing())) continue;
        const std::string_view shown = items_[i].kind == PropertyKind::Bool
            ? (property_true(items_[i].value) ? "[X]" : "[ ]") : std::string_view(items_[i].value);
        painter.draw_text(Point{value_column, y}, text::clip_to_width_view(shown, width - value_column), style);
    }
    if (!reason_.empty() && reason_index_ >= 0 && value_column < width) {
        const Style reason = context().theme->resolve(reason_role_);
        const int y = row_of(static_cast<std::size_t>(reason_index_)) + 1;
        painter.fill(Rect{value_column, y, width - value_column, 1}, Cell::from_grapheme(" ", reason));
        painter.draw_text(Point{value_column, y}, text::clip_to_width_view(reason_, width - value_column), reason);
    }
}
void PropertyInspector::place_editor() {
    if (editor_ == nullptr) return;
    editor_->set_bounds(value_bounds(static_cast<std::size_t>(cursor_)));
}
void PropertyInspector::on_resized() { place_editor(); }
bool PropertyInspector::begin_edit() {
    if (!enabled_in_tree() || editing() || cursor_ < 0 || cursor_ >= static_cast<int>(items_.size())) return false;
    const PropertyItem& item = items_[static_cast<std::size_t>(cursor_)];
    if (!item.editable) return false;
    if (item.kind == PropertyKind::Bool) {
        toggle(static_cast<std::size_t>(cursor_));
        return true;
    }
    ui::Application* const app = context().app;
    if (app == nullptr) return false;
    switch (item.kind) {
        case PropertyKind::Text:
        case PropertyKind::Integer:
        case PropertyKind::Real:
            text_editor_->set_text(item.value);
            text_editor_->set_valid(true);
            editor_ = text_editor_;
            break;
        case PropertyKind::Choice: {
            choice_editor_->set_items(item.choices);
            const auto chosen = std::find(item.choices.begin(), item.choices.end(), item.value);
            choice_editor_->set_selected_index(
                chosen == item.choices.end()
                    ? std::nullopt
                    : std::optional<std::size_t>(static_cast<std::size_t>(chosen - item.choices.begin())));
            editor_ = choice_editor_;
            break;
        }
        case PropertyKind::Date:
            date_editor_->set_value(parse_iso_date(item.value).value_or(DateValue{}));
            date_editor_->set_valid(true);
            // Space and the "▾" drop the calendar, where there is a desktop
            // to drop it on.
            for (ui::View* p = parent(); p != nullptr; p = p->parent())
                if (auto* desktop = dynamic_cast<Desktop*>(p)) {
                    date_editor_->set_calendar_host(*app, *desktop);
                    break;
                }
            editor_ = date_editor_;
            break;
        case PropertyKind::Time: {
            const std::optional<TimeValue> time = parse_iso_time(item.value);
            time_editor_->set_show_seconds(item.value.size() > 5);
            time_editor_->set_value(time.value_or(TimeValue{}));
            time_editor_->set_valid(true);
            editor_ = time_editor_;
            break;
        }
        case PropertyKind::Bool: break;
    }
    clear_reason();
    place_editor();
    editor_->set_visible(true);
    app->set_focus(editor_);
    invalidate();
    // A Choice edit is a choice: the list is what the reader asked for.
    if (editor_ == choice_editor_) choice_editor_->open_dropdown();
    return true;
}
std::string PropertyInspector::editor_text() const {
    if (editor_ == text_editor_) return text_editor_->text();
    if (editor_ == choice_editor_) return choice_editor_->text();
    if (editor_ == date_editor_) return date_editor_->value() ? format_iso_date(*date_editor_->value()) : std::string{};
    if (editor_ == time_editor_) return format_iso_time(time_editor_->value(), time_editor_->show_seconds());
    return {};
}
std::optional<std::string> PropertyInspector::check(const PropertyItem& item, std::string& text) const {
    const auto out_of_range = [&](double value) -> std::optional<std::string> {
        if (item.minimum && value < *item.minimum) return messages_.at_least + " " + bound_text(*item.minimum, item.kind);
        if (item.maximum && value > *item.maximum) return messages_.at_most + " " + bound_text(*item.maximum, item.kind);
        return std::nullopt;
    };
    const char* const first = text.data();
    const char* const last = text.data() + text.size();
    if (item.kind == PropertyKind::Integer) {
        long long value = 0;
        const auto parsed = std::from_chars(first, last, value);
        if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != last) return messages_.whole_number;
        if (auto reason = out_of_range(static_cast<double>(value))) return reason;
        // Exact for every whole number, where going through a double would
        // not be past 2^53.
        char buffer[32];
        const auto written = std::to_chars(std::begin(buffer), std::end(buffer), value);
        text.assign(buffer, written.ptr);
    } else if (item.kind == PropertyKind::Real) {
        double value = 0;
        const auto parsed = std::from_chars(first, last, value);
        if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != last || !std::isfinite(value))
            return messages_.number;
        if (auto reason = out_of_range(value)) return reason;
        text = bound_text(value, PropertyKind::Real);
    }
    if (item.validate) return item.validate(text);
    return std::nullopt;
}
void PropertyInspector::refuse(std::string reason) {
    reason_ = std::move(reason);
    reason_index_ = cursor_;
    rebuild_rows();
    if (editor_ == text_editor_) text_editor_->set_valid(false);
    if (editor_ == date_editor_) date_editor_->set_valid(false);
    if (editor_ == time_editor_) time_editor_->set_valid(false);
    // The reason takes the row under the edited one; the editor stays where
    // it is, and the rows after it move down.
    place_editor();
    invalidate();
}
void PropertyInspector::clear_reason() {
    if (reason_.empty()) return;
    reason_.clear();
    reason_index_ = -1;
    rebuild_rows();
    text_editor_->set_valid(true);
    date_editor_->set_valid(true);
    time_editor_->set_valid(true);
    // The rows the reason pushed down come back up, the editor's with them.
    place_editor();
    invalidate();
}
bool PropertyInspector::commit_edit() {
    if (!editing()) return false;
    const std::size_t index = static_cast<std::size_t>(cursor_);
    std::string text = editor_text();
    if (std::optional<std::string> reason = check(items_[index], text)) {
        refuse(std::move(*reason));
        return false;
    }
    const bool changed = items_[index].value != text;
    items_[index].value = std::move(text);
    close_editor();
    if (changed && on_change) on_change(index, items_[index].value);
    return true;
}
void PropertyInspector::cancel_edit() { close_editor(); }
void PropertyInspector::close_editor() {
    if (editor_ == nullptr) return;
    ui::View* const editor = editor_;
    clear_reason();
    editor_ = nullptr;
    // A popup the editor dropped closes with it, before the keyboard moves:
    // the calendar holds a modal scope of its own, and left open it would go
    // on editing a picker nobody can see.
    if (editor == choice_editor_) choice_editor_->close_dropdown();
    if (editor == date_editor_) date_editor_->close_calendar();
    // The keyboard comes back before the editor goes, so it is never left on
    // a view that is not there to hold it.
    if (context().app != nullptr && (editor->has_focus() || context().app->focused() == nullptr))
        context().app->set_focus(this);
    editor->set_visible(false);
    invalidate();
}
void PropertyInspector::toggle(std::size_t index) {
    PropertyItem& item = items_[index];
    std::string text = property_true(item.value) ? "false" : "true";
    // A refused toggle changes nothing and says why, under the row, until the
    // reader does something else.
    if (item.validate) {
        if (std::optional<std::string> reason = item.validate(text)) {
            reason_ = std::move(*reason);
            reason_index_ = static_cast<int>(index);
            rebuild_rows();
            invalidate();
            return;
        }
    }
    clear_reason();
    item.value = std::move(text);
    invalidate();
    if (on_change) on_change(index, item.value);
}
bool PropertyInspector::move_cursor_to(int index) {
    if (editing() && !commit_edit()) return false;
    clear_reason();
    cursor_ = std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
    invalidate();
    return true;
}
bool PropertyInspector::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event) || items_.empty()) return false;
    if (editing()) {
        // What the editor left: the keys that end an edit.
        switch (event.chord.key) {
            case Key::Enter: commit_edit(); return true;
            case Key::Escape: cancel_edit(); return true;
            // Leaving the field is a commit, as in a form; a refused value
            // keeps the reader where they can fix it.
            case Key::Tab: return !commit_edit();
            case Key::Up: move_cursor_to(cursor_ - 1); return true;
            case Key::Down: move_cursor_to(cursor_ + 1); return true;
            default: return false;
        }
    }
    const PropertyItem& item = items_[static_cast<std::size_t>(cursor_)];
    switch (event.chord.key) {
        case Key::Down: return move_cursor_to(cursor_ + 1);
        case Key::Up: return move_cursor_to(cursor_ - 1);
        case Key::Enter:
        case Key::F2: return begin_edit() || item.editable;
        case Key::Char:
            // Space is the check box's own key.
            if (event.chord.text == " " && item.kind == PropertyKind::Bool && item.editable &&
                event.chord.modifiers == Modifier::None)
                return begin_edit();
            return false;
        default: return false;
    }
}
bool PropertyInspector::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    if (!Rect{0, 0, bounds().width, bounds().height}.contains(local)) return false;
    const int index = item_at_row(local.y);
    if (index < 0) return false;
    if (index != cursor_ && !move_cursor_to(index)) return true;
    // A press on a value asks to change it; on a name, only to point at it.
    if (event.cell.x - abs.x >= value_x() && !editing()) begin_edit();
    return true;
}
void PropertyInspector::on_focus(const FocusEvent&) { invalidate(); }
void PropertyInspector::on_attached() {
    banded_role_ = context().roles->find("ckv.list.banded");
    heading_role_ = context().roles->find("ckv.label.text");
    divider_role_ = context().roles->find("ckv.label.disabled");
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
    if (selected_inactive_role_ == ui::kInvalidRole)
        selected_inactive_role_ = context().roles->find("ckv.list.selected.inactive");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.list.disabled");
    if (reason_role_ == ui::kInvalidRole) reason_role_ = context().roles->find("ckv.input.invalid");
}

namespace {

// The first view in `view`'s subtree, itself included, that can take the
// focus, in the order Tab walks: a page arriving hands the focus to it.
ui::View* first_focusable_in(ui::View& view) {
    if (view.focusable()) return &view;
    for (const auto& child : view.children())
        if (ui::View* found = first_focusable_in(*child)) return found;
    return nullptr;
}

bool is_within(const ui::View* view, const ui::View& ancestor) noexcept {
    for (; view != nullptr; view = view->parent())
        if (view == &ancestor) return true;
    return false;
}

// The title row keeps at least this many cells for the page's own title
// before the step indicator gives way to it.
constexpr int kWizardTitleMinimum = 4;

}  // namespace

Wizard::Wizard() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    back_button_ = make<Button>(labels_.back);
    next_button_ = make<Button>(labels_.next);
    finish_button_ = make<Button>(labels_.finish);
    cancel_button_ = make<Button>(labels_.cancel);
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_})
        button->set_presentation(ButtonPresentation::Flat);
    next_button_->set_default(true);
    finish_button_->set_default(true);
    back_button_->on_press = [this] { back(); };
    next_button_->on_press = [this] { next(); };
    finish_button_->on_press = [this] { finish(); };
    cancel_button_->on_press = [this] { cancel(); };
    refresh_navigation();
}
int Wizard::chrome_rows() const noexcept { return presentation_ == WizardPresentationStyle::Compact ? 1 : 3; }
int Wizard::rail_width() const noexcept { return presentation_ == WizardPresentationStyle::StepRail ? rail_width_ : 0; }
Rect Wizard::header_bounds() const noexcept {
    return Rect{0, 0, std::max(0, bounds().width), std::min(std::max(0, bounds().height), chrome_rows())};
}
Rect Wizard::footer_bounds() const noexcept {
    const int header = header_bounds().height;
    const int height = std::min(chrome_rows(), std::max(0, bounds().height - header));
    return Rect{0, std::max(header, bounds().height - height), std::max(0, bounds().width), height};
}
Rect Wizard::content_bounds() const noexcept {
    const int inset = presentation_ == WizardPresentationStyle::Compact ? 0 : 1;
    const int x = rail_width() + inset;
    const int top = header_bounds().height;
    return Rect{x, top, std::max(0, bounds().width - x - inset), std::max(0, footer_bounds().y - top)}
        .intersected(Rect{0, 0, bounds().width, bounds().height});
}
Rect Wizard::step_rail_bounds() const noexcept {
    return Rect{0, header_bounds().height, rail_width(), std::max(0, footer_bounds().y - header_bounds().height)}
        .intersected(Rect{0, 0, bounds().width, bounds().height});
}
void Wizard::prepare_captions() {
    indicators_.clear();
    rail_captions_.clear();
    indicators_.reserve(pages_.size());
    rail_captions_.reserve(pages_.size());
    rail_width_ = 12;
    for (std::size_t i = 0; i < pages_.size(); ++i) {
        indicators_.push_back(labels_.step ? labels_.step(i + 1, pages_.size()) : std::string{});
        char number[32];
        const auto converted = std::to_chars(std::begin(number), std::end(number), i + 1);
        std::string caption(number, converted.ptr);
        caption += " ";
        caption += pages_[i].title;
        rail_width_ = std::max(rail_width_, std::min(24, text::text_width(caption) + 4));
        rail_captions_.push_back(std::move(caption));
    }
}
void Wizard::set_presentation(WizardPresentationStyle presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_})
        button->set_presentation(presentation == WizardPresentationStyle::Compact ? ButtonPresentation::Flat : ButtonPresentation::Padded);
    on_resized();
    size_hint_changed();
    invalidate();
}
void Wizard::set_pages(std::vector<WizardPage> pages) {
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_}) button->cancel_press();
    for (ui::View* content : contents_) if (content != nullptr) remove_child(content);
    pages_ = std::move(pages);
    contents_.assign(pages_.size(), nullptr);
    current_page_ = 0;
    prepare_captions();
    refresh_navigation();
    on_resized();
    invalidate();
    size_hint_changed();
}
ui::View* Wizard::set_page_content(std::size_t page, std::unique_ptr<ui::View> content) {
    CKV_ASSERT(page < pages_.size());
    CKV_ASSERT(content != nullptr);
    if (contents_[page] != nullptr) remove_child(contents_[page]);
    ui::View* const added = add_child(std::move(content));
    added->set_bounds(content_bounds());
    added->set_visible(page == current_page_);
    contents_[page] = added;
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_}) raise_to_front(button);
    invalidate();
    size_hint_changed();
    return added;
}
ui::View* Wizard::page_content(std::size_t page) const noexcept { return page < contents_.size() ? contents_[page] : nullptr; }
void Wizard::set_labels(WizardLabels labels) {
    labels_ = std::move(labels);
    back_button_->set_text(labels_.back);
    next_button_->set_text(labels_.next);
    finish_button_->set_text(labels_.finish);
    cancel_button_->set_text(labels_.cancel);
    prepare_captions();
    on_resized();
    invalidate();
    size_hint_changed();
}
bool Wizard::page_allows_leaving() const {
    return !pages_.empty() && (!pages_[current_page_].can_continue || pages_[current_page_].can_continue());
}
bool Wizard::can_go_next() const { return !last_page() && page_allows_leaving(); }
void Wizard::refresh_navigation() {
    const bool forward = page_allows_leaving();
    back_button_->set_visible(can_go_back() && !back_button_->bounds().empty());
    back_button_->set_enabled(can_go_back());
    next_button_->set_visible(!last_page() && !next_button_->bounds().empty());
    finish_button_->set_visible(last_page() && !finish_button_->bounds().empty());
    cancel_button_->set_visible(!cancel_button_->bounds().empty());
    for (Button* button : {next_button_, finish_button_}) {
        if (button->enabled() != forward) button->cancel_press();
        button->set_enabled(forward);
    }
    const bool cancel_available = static_cast<bool>(on_complete);
    if (cancel_button_->enabled() != cancel_available) cancel_button_->cancel_press();
    cancel_button_->set_enabled(cancel_available);
}
void Wizard::show_page(std::size_t page) {
    ui::Application* const app = context().app;
    ui::View* const leaving = contents_[current_page_];
    const bool focus_was_inside = app != nullptr && leaving != nullptr && is_within(app->focused(), *leaving);
    ui::View* const previous_action = &forward_button();
    const bool focus_was_action = app != nullptr && app->focused() == previous_action;
    const bool focus_was_back = app != nullptr && app->focused() == back_button_;
    current_page_ = page;
    ui::View* const arriving = contents_[page];
    if (arriving != nullptr) arriving->set_visible(true);
    refresh_navigation();
    if (focus_was_inside) {
        ui::View* target = arriving != nullptr ? first_focusable_in(*arriving) : nullptr;
        if (target == nullptr && focusable()) target = this;
        app->set_focus(target);
    } else if (focus_was_action && previous_action != &forward_button()) {
        app->set_focus(forward_button().focusable() ? static_cast<ui::View*>(&forward_button()) : this);
    }
    if (focus_was_back && !back_button_->focusable())
        app->set_focus(forward_button().focusable() ? static_cast<ui::View*>(&forward_button()) : this);
    if (leaving != nullptr) leaving->set_visible(false);
    invalidate();
}
bool Wizard::next() {
    if (!can_go_next()) return false;
    show_page(current_page_ + 1);
    return true;
}
bool Wizard::back() {
    if (!can_go_back()) return false;
    show_page(current_page_ - 1);
    return true;
}
bool Wizard::finish() {
    if (pages_.empty() || !last_page() || !page_allows_leaving()) return false;
    const std::function<void(WizardOutcome)> complete = on_complete;
    if (complete) complete(WizardOutcome::Finished);
    return true;
}
bool Wizard::cancel() {
    if (!on_complete) return false;
    const std::function<void(WizardOutcome)> complete = on_complete;
    complete(WizardOutcome::Cancelled);
    return true;
}
int Wizard::action_width() const {
    return std::max(next_button_->horizontal_size_hint().preferred, finish_button_->horizontal_size_hint().preferred);
}
void Wizard::layout_navigation() {
    if (back_button_ == nullptr || next_button_ == nullptr || finish_button_ == nullptr || cancel_button_ == nullptr) return;
    const Rect footer = footer_bounds();
    const int inset = presentation_ == WizardPresentationStyle::Compact ? 0 : 1;
    const int y = footer.y + footer.height / 2;
    const int back_width = back_button_->horizontal_size_hint().preferred;
    const int forward_x = inset + back_width + 1;
    const int forward_width = action_width();
    const int cancel_width = cancel_button_->horizontal_size_hint().preferred;
    const int cancel_x = std::max(forward_x + forward_width + 1, bounds().width - inset - cancel_width);
    back_button_->set_bounds(Rect{inset, y, back_width, 1}.intersected(footer));
    next_button_->set_bounds(Rect{forward_x, y, forward_width, 1}.intersected(footer));
    finish_button_->set_bounds(next_button_->bounds());
    cancel_button_->set_bounds(Rect{cancel_x, y, cancel_width, 1}.intersected(footer));
}
void Wizard::draw(scene::Painter& target) {
    refresh_navigation();
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    const auto& theme = *context().theme;
    Style normal = theme.resolve(role_);
    const bool enabled = enabled_in_tree();
    if (!enabled) { normal = accent_style(normal, theme.resolve(disabled_role_)); normal.attrs |= Attr::Dim; }
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));
    const bool compact = presentation_ == WizardPresentationStyle::Compact;
    Style header = compact ? normal : theme.resolve(header_role_);
    Style footer = compact ? normal : theme.resolve(footer_role_);
    Style rail = theme.resolve(rail_role_);
    if (!enabled) { header.attrs |= Attr::Dim; footer.attrs |= Attr::Dim; rail.attrs |= Attr::Dim; }
    painter.fill(header_bounds(), Cell::from_grapheme(" ", header));
    painter.fill(footer_bounds(), Cell::from_grapheme(" ", footer));
    const Rect rail_bounds = step_rail_bounds();
    if (!rail_bounds.empty()) {
        auto rail_painter = painter.clipped(rail_bounds);
        rail_painter.fill(rail_bounds, Cell::from_grapheme(" ", rail));
        rail_painter.vline(Point{rail_bounds.right() - 1, rail_bounds.y}, rail_bounds.height, scene::LineStyle::Single, rail);
        for (std::size_t i = 0; i < pages_.size() && i < static_cast<std::size_t>((rail_bounds.height + 1) / 2); ++i) {
            const int y = rail_bounds.y + static_cast<int>(i) * 2;
            Style style = rail;
            if (i == current_page_) style.attrs |= Attr::Bold;
            else style.attrs |= Attr::Dim;
            rail_painter.draw_text(Point{1, y}, i < current_page_ ? "✓" : i == current_page_ ? "●" : "○", style);
            rail_painter.draw_text(Point{3, y}, text::clip_to_width_view(rail_captions_[i], std::max(0, rail_bounds.width - 5)), style);
        }
    }
    const Rect header_rect = header_bounds();
    auto header_painter = painter.clipped(header_rect);
    const int inset = compact ? 0 : 1;
    const int x = rail_width() + inset;
    const int y = compact ? 0 : 1;
    const int width = std::max(0, bounds().width - x - inset);
    const std::string_view indicator = pages_.empty() ? std::string_view{} : std::string_view(indicators_[current_page_]);
    const int indicator_width = text::text_width(indicator);
    int title_width = width;
    if (indicator_width > 0 && width - indicator_width - 1 >= kWizardTitleMinimum) {
        title_width = width - indicator_width - 1;
        header_painter.draw_text(Point{x + width - indicator_width, y}, indicator, header);
    }
    const std::string_view title = pages_.empty() ? "Wizard" : std::string_view(pages_[current_page_].title);
    Style title_style = enabled ? accent_style(header, theme.resolve(selected_role_)) : header;
    title_style.attrs |= Attr::Bold;
    if (enabled && has_focus()) title_style.attrs |= Attr::Underline;
    const bool elided = text::text_width(title) > title_width;
    const auto shown = text::clip_to_width_view(title, elided ? std::max(0, title_width - 1) : title_width);
    header_painter.draw_text(Point{x, y}, shown, title_style);
    if (elided && title_width > 0) header_painter.draw_text(Point{x + text::text_width(shown), y}, "…", title_style);
}
void Wizard::on_focus(const FocusEvent&) { invalidate(); }
bool Wizard::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event)) return false;
    refresh_navigation();
    if (event.chord.key == Key::Right || event.chord.key == Key::Enter) return next() || finish();
    if (event.chord.key == Key::Left) return back();
    if (event.chord.key == Key::Escape) return cancel();
    return false;
}
std::optional<PointerShape> Wizard::pointer_shape_at(Point local) const {
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_})
        if (button->visible() && button->bounds().contains(local))
            return enabled_in_tree() && button->enabled() ? PointerShape::Pointer : PointerShape::NotAllowed;
    return std::nullopt;
}
void Wizard::on_resized() {
    for (Button* button : {back_button_, next_button_, finish_button_, cancel_button_})
        if (button != nullptr) button->cancel_press();
    const Rect content = content_bounds();
    for (ui::View* page : contents_) if (page != nullptr) page->set_bounds(content);
    layout_navigation();
    refresh_navigation();
    auto* app = context().app;
    if (app != nullptr && app->focused() != nullptr &&
        (app->focused() == back_button_ || app->focused() == next_button_ ||
         app->focused() == finish_button_ || app->focused() == cancel_button_) &&
        !app->focused()->focusable()) app->set_focus(this);
}
void Wizard::on_child_size_hint_changed(ui::View&) { layout_navigation(); size_hint_changed(); }
ui::SizeHint Wizard::horizontal_size_hint() const {
    const int inset = presentation_ == WizardPresentationStyle::Compact ? 0 : 1;
    int preferred = inset * 2 + back_button_->horizontal_size_hint().preferred + 1 + action_width() + 1 + cancel_button_->horizontal_size_hint().preferred;
    for (std::size_t i = 0; i < pages_.size(); ++i) {
        const int indicator_width = text::text_width(indicators_[i]);
        preferred = std::max(preferred, rail_width() + inset * 2 + text::text_width(pages_[i].title) + (indicator_width > 0 ? indicator_width + 1 : 0));
        if (contents_[i] != nullptr) preferred = std::max(preferred, rail_width() + inset * 2 + contents_[i]->horizontal_size_hint().preferred);
    }
    return {0, preferred, ui::kUnboundedExtent};
}
ui::SizeHint Wizard::vertical_size_hint() const {
    int content = 0;
    for (const ui::View* page : contents_) if (page != nullptr) content = std::max(content, page->vertical_size_hint().preferred);
    if (presentation_ == WizardPresentationStyle::StepRail) content = std::max(content, static_cast<int>(pages_.size()) * 2 - 1);
    return {chrome_rows() * 2, content + chrome_rows() * 2, ui::kUnboundedExtent};
}
void Wizard::on_attached() {
    role_ = context().roles->find("ckv.dialog.background");
    selected_role_ = context().roles->find("ckv.label.text");
    disabled_role_ = context().roles->find("ckv.label.disabled");
    header_role_ = context().roles->find("ckv.wizard.header");
    footer_role_ = context().roles->find("ckv.wizard.footer");
    rail_role_ = context().roles->find("ckv.wizard.rail");
    on_resized();
}

WizardPresentation present_modal_wizard(std::unique_ptr<Wizard> wizard, std::string title, ui::Application& app,
                                        Desktop& desktop, const ui::StandardRoles& roles) {
    CKV_ASSERT(wizard != nullptr);
    using Access = detail::DialogPresentationAccess<WizardOutcome>;
    auto parts = Access::make();
    auto window = std::make_unique<Window>(std::move(title));
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                              roles.dialog_background);
    window->set_resizable(false);
    window->set_content_margin(1, 1);
    Window* const window_ptr = window.get();
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();
    Wizard* const wizard_ptr = wizard.get();
    // The reader starts on the page's first control, as a dialog's first
    // field takes the focus; a page with none leaves it on the navigation.
    ui::View* const content = wizard_ptr->page_content(wizard_ptr->current_page());
    ui::View* const page_control = content != nullptr ? first_focusable_in(*content) : nullptr;
    wizard_ptr->on_complete = [state = parts.state, window_ptr, window_liveness](WizardOutcome outcome) {
        // Recorded before the window goes, so the completion carries it
        // whatever closes the window afterwards.
        Access::record(state, outcome);
        if (!window_liveness.expired()) window_ptr->close();
    };
    window->set_content(std::move(wizard));
    // Escape reaches the wizard first and cancels through on_complete; a
    // window-level cancel is the same answer by the fallback.
    window->cancel_request = [window_ptr, window_liveness] {
        if (!window_liveness.expired()) window_ptr->close();
    };
    window->on_closed = [&app, window_ptr, window_liveness] {
        if (!window_liveness.expired()) schedule_self_detach(*window_ptr, app);
    };
    auto previous_on_detached = std::move(window->on_detached);
    window->on_detached = [previous = std::move(previous_on_detached), state = parts.state]() {
        if (previous) previous();
        Access::finish(state, WizardOutcome::Cancelled);
    };
    desktop.present_modal(WindowHandle{std::move(window), page_control != nullptr ? page_control : wizard_ptr},
                          app);
    return std::move(parts.presentation);
}

NotificationCenter::NotificationCenter() { set_focus_policy(ui::FocusPolicy::TabStop); }

std::size_t NotificationCenter::add(Notification notification) {
    // A persistent one is never due; nor is anything added while no interval
    // has been set, or while there is no clock to read. The deadline is fixed
    // HERE rather than derived at each sweep, so a notification's life is
    // measured from when the host posted it and not from when the widget
    // happened to look.
    std::int64_t deadline = kNever;
    ui::Application* const app = context().app;
    if (!notification.persistent && auto_dismiss_nanos_ > 0 && app != nullptr)
        deadline = app->clock().now_nanos() + auto_dismiss_nanos_;
    notifications_.push_back(std::move(notification));
    deadlines_.push_back(deadline);
    if (deadline != kNever) arm_expiry(deadline);
    changed();
    return notifications_.size() - 1;
}

void NotificationCenter::dismiss(std::size_t index) {
    if (index >= notifications_.size()) return;
    notifications_.erase(notifications_.begin() + static_cast<std::ptrdiff_t>(index));
    deadlines_.erase(deadlines_.begin() + static_cast<std::ptrdiff_t>(index));
    changed();
}

void NotificationCenter::set_auto_dismiss(std::int64_t nanos) {
    // Negative is read as off rather than asserted on: this is a duration a
    // host computes from a setting, and the answer to a nonsensical one is a
    // centre that keeps its notifications, not a dead application.
    auto_dismiss_nanos_ = std::max<std::int64_t>(0, nanos);
    // Re-time what is already on screen — see the header. Persistent entries
    // keep kNever whatever the interval says.
    ui::Application* const app = context().app;
    const std::int64_t now = app != nullptr ? app->clock().now_nanos() : 0;
    std::int64_t soonest = kNever;
    for (std::size_t i = 0; i < notifications_.size(); ++i) {
        if (notifications_[i].persistent || auto_dismiss_nanos_ == 0 || app == nullptr) {
            deadlines_[i] = kNever;
            continue;
        }
        deadlines_[i] = now + auto_dismiss_nanos_;
        if (soonest == kNever || deadlines_[i] < soonest) soonest = deadlines_[i];
    }
    if (soonest != kNever) arm_expiry(soonest);
}

void NotificationCenter::expire_due() {
    ui::Application* const app = context().app;
    if (app == nullptr) return;
    const std::int64_t now = app->clock().now_nanos();
    bool removed = false;
    for (std::size_t i = notifications_.size(); i-- > 0;) {
        if (deadlines_[i] == kNever || deadlines_[i] > now) continue;
        notifications_.erase(notifications_.begin() + static_cast<std::ptrdiff_t>(i));
        deadlines_.erase(deadlines_.begin() + static_cast<std::ptrdiff_t>(i));
        removed = true;
    }
    // Whatever is still waiting gets the next wake-up. Recomputed from what
    // remains rather than remembered, because a dismissal by the reader may
    // have taken the entry this timer was armed for.
    std::int64_t soonest = kNever;
    for (const std::int64_t deadline : deadlines_)
        if (deadline != kNever && (soonest == kNever || deadline < soonest)) soonest = deadline;
    if (soonest != kNever) arm_expiry(soonest);
    if (removed) changed();
}

void NotificationCenter::arm_expiry(std::int64_t deadline_nanos) {
    ui::Application* const app = context().app;
    if (app == nullptr) return;
    // An armed wake-up that comes sooner already covers this one; one that
    // comes later is replaced, so a toast posted now does not wait behind a
    // persistent-looking interval that was armed before it.
    if (expiry_timer_ != 0 && expiry_wake_nanos_ <= deadline_nanos) return;
    if (expiry_timer_ != 0) app->cancel_timer(expiry_timer_);
    const std::int64_t delay = std::max<std::int64_t>(0, deadline_nanos - app->clock().now_nanos());
    const std::weak_ptr<void> liveness = lifetime_token();
    expiry_wake_nanos_ = deadline_nanos;
    expiry_timer_ = app->start_timer(delay, /*repeating=*/false, [this, liveness] {
        // One-shot, so nothing has to be cancelled on the way out — but this
        // view can be destroyed between arming and firing, and a callback
        // reading `this` then would be reading freed storage.
        if (liveness.expired()) return;
        expiry_timer_ = 0;
        expiry_wake_nanos_ = 0;
        expire_due();
    });
}

void NotificationCenter::changed() {
    size_hint_changed();
    invalidate();
    if (on_changed) on_changed();
}

void NotificationCenter::on_focus(const FocusEvent&) { invalidate(); }

void NotificationCenter::set_presentation(NotificationPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    size_hint_changed();
    invalidate();
}
ui::SizeHint NotificationCenter::vertical_size_hint() const {
    const auto count = notifications_.size();
    const std::size_t stride = presentation_ == NotificationPresentation::Lines ? 1 : 4;
    const auto maximum = static_cast<std::size_t>(std::numeric_limits<int>::max());
    const int rows = count == 0 ? 0 : count > maximum / stride
        ? std::numeric_limits<int>::max() : static_cast<int>(count * stride - (stride == 1 ? 0 : 1));
    return {0, std::max(rows, preferred_size().height), ui::kUnboundedExtent};
}
Rect NotificationCenter::notification_bounds(std::size_t index) const noexcept {
    if (index >= notifications_.size()) return {};
    const int stride = presentation_ == NotificationPresentation::Lines ? 1 : 4;
    const int height = std::max(0, bounds().height);
    if (height == 0 || index >= static_cast<std::size_t>((height - 1) / stride + 1)) return {};
    const int row = static_cast<int>(index) * stride;
    return Rect{0, row, bounds().width, presentation_ == NotificationPresentation::Lines ? 1 : 3}.intersected(Rect{0, 0, bounds().width, bounds().height});
}
Rect NotificationCenter::dismiss_bounds(std::size_t index) const noexcept {
    const Rect card = notification_bounds(index);
    if (card.empty()) return {};
    if (presentation_ == NotificationPresentation::Lines) return card;
    const int row = static_cast<int>(index) * 4;
    return Rect{std::max(0, bounds().width - 3), row + 1, std::min(2, bounds().width), 1}.intersected(card);
}
std::optional<PointerShape> NotificationCenter::pointer_shape_at(Point local) const {
    if (local.x < 0 || local.y < 0 || local.x >= bounds().width || local.y >= bounds().height) return std::nullopt;
    const auto index = static_cast<std::size_t>(local.y / (presentation_ == NotificationPresentation::Lines ? 1 : 4));
    if (!dismiss_bounds(index).contains(local)) return std::nullopt;
    return enabled_in_tree() ? PointerShape::Pointer : PointerShape::NotAllowed;
}
void NotificationCenter::draw(scene::Painter& target) {
    if (bounds().width <= 0 || bounds().height <= 0) return;
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    const bool enabled = enabled_in_tree();
    Style base = context().theme->resolve(role_);
    if (!enabled) {
        const Style muted = accent_style(base, context().theme->resolve(disabled_role_));
        if (muted.fg != base.bg) base.fg = muted.fg;
        base.attrs |= Attr::Dim;
    }
    for (std::size_t i = 0; i < notifications_.size(); ++i) {
        const Rect card = notification_bounds(i);
        if (card.empty()) break;
        const auto severity = notifications_[i].severity;
        const std::string_view prefix = severity == NotificationSeverity::Info ? "i " : severity == NotificationSeverity::Warning ? "! " : "x ";
        const auto role = severity == NotificationSeverity::Info ? info_role_ : severity == NotificationSeverity::Warning ? warning_role_ : error_role_;
        Style line = base;
        const bool cards = presentation_ != NotificationPresentation::Lines;
        if (enabled_in_tree() && has_focus() && i + 1 == notifications_.size()) line.attrs |= cards ? Attr::Underline : Attr::Reverse;
        const Style accent = enabled ? accent_style(line, context().theme->resolve(role)) : line;
        auto clipped = painter.clipped(card);
        clipped.fill(card, Cell::from_grapheme(" ", cards ? base : line));
        if (presentation_ == NotificationPresentation::Framed) clipped.draw_box(Rect{0, static_cast<int>(i) * 4, bounds().width, 3}, scene::LineStyle::Single, accent);
        if (presentation_ == NotificationPresentation::Banners) clipped.vline(Point{0, card.y}, card.height, scene::LineStyle::Single, accent);
        const int y = static_cast<int>(i) * (cards ? 4 : 1) + (cards ? 1 : 0);
        const int inset = cards ? 1 : 0;
        const int reserved = cards ? 4 : 0;
        const int text_x = inset + 2;
        const int available = std::max(0, bounds().width - reserved - text_x);
        auto content = clipped.clipped(Rect{inset, y, std::max(0, bounds().width - reserved - inset), 1});
        content.draw_text(Point{inset, y}, prefix, accent);
        const auto& message = notifications_[i].text;
        const bool elided = text::text_width(message) > available;
        const auto shown = text::clip_to_width_view(message, elided ? std::max(0, available - 1) : available);
        content.draw_text(Point{text_x, y}, shown, line);
        if (elided && available > 0) content.draw_text(Point{text_x + text::text_width(shown), y}, "…", line);
        if (cards) {
            const Rect dismissal = dismiss_bounds(i);
            auto control = clipped.clipped(dismissal);
            control.draw_text(Point{dismissal.x, y}, "×", base);
        }
    }
}

bool NotificationCenter::on_key(const KeyEvent& event) {
    if (enabled_in_tree() && is_press(event) && event.chord.key == Key::Escape && !notifications_.empty()) { dismiss(notifications_.size() - 1); return true; }
    return false;
}

bool NotificationCenter::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    if (!pointer_shape_at(local)) return false;
    dismiss(static_cast<std::size_t>(local.y / (presentation_ == NotificationPresentation::Lines ? 1 : 4)));
    return true;
}

void NotificationCenter::on_attached() {
    disabled_role_ = context().roles->find("ckv.label.disabled");
    // The focus policy is NOT set here — see the constructor. Attaching is
    // where a view learns its context, not where it overrules decisions its
    // host has already made about it.
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.statusline.normal");
    if (info_role_ == ui::kInvalidRole) info_role_ = context().roles->find("ckv.message.info.text");
    if (warning_role_ == ui::kInvalidRole) warning_role_ = context().roles->find("ckv.message.warning.text");
    if (error_role_ == ui::kInvalidRole) error_role_ = context().roles->find("ckv.message.error.text");
    // Anything posted before there was a clock to read has no deadline yet.
    // Re-timing here is what makes "post first, attach later" behave the same
    // as the ordinary way round, rather than leaving those entries immortal.
    if (auto_dismiss_nanos_ > 0) set_auto_dismiss(auto_dismiss_nanos_);
}

Tooltip::Tooltip(std::string text) : text_(std::move(text)) {
    set_visible(false);
    set_preferred_size(Size{shown_width(), 1});
}
int Tooltip::shown_width() const noexcept { return std::max(2, text::text_width(text_) + 2); }
void Tooltip::set_text(std::string text) {
    text_ = std::move(text);
    set_preferred_size(Size{shown_width(), 1});
    invalidate();
    size_hint_changed();
}
void Tooltip::show_at(Point position) {
    set_bounds(Rect{position.x, position.y, shown_width(), 1});
    set_visible(true);
}
void Tooltip::show_near(Rect anchor, Rect area) {
    const auto inside = [&area](int row) { return row >= area.y && row < area.bottom(); };
    const int width = std::min(shown_width(), std::max(0, area.width));
    const int below = anchor.bottom();
    const int above = anchor.y - 1;
    const int y = inside(below)   ? below
                  : inside(above) ? above
                                  : std::clamp(anchor.bottom() - 1, area.y, std::max(area.y, area.bottom() - 1));
    const int x = std::max(area.x, std::min(anchor.x, area.right() - width));
    set_bounds(Rect{x, y, width, 1});
    set_visible(true);
}
void Tooltip::hide() { set_visible(false); }
void Tooltip::dismiss() {
    // Held locally: the host normally takes this tooltip down from here.
    const std::function<void()> callback = on_dismiss;
    if (callback) callback();
}
void Tooltip::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));
    painter.draw_text(Point{1, 0}, text::elide_to_width(text_, std::max(0, bounds().width - 2)), style);
}
bool Tooltip::on_key(const KeyEvent& event) {
    if (!held_ || !is_press(event)) return false;
    dismiss();
    return true;
}
bool Tooltip::on_text(const TextEvent&) {
    if (!held_) return false;
    dismiss();
    return true;
}
bool Tooltip::on_mouse(const MouseEvent& event) {
    if (!held_) return false;
    const Rect absolute = absolute_bounds();
    const bool inside = event.cell.x >= absolute.x && event.cell.x < absolute.right() && event.cell.y >= absolute.y &&
                        event.cell.y < absolute.bottom();
    // A press anywhere puts it away, as it puts away a menu: one outside is
    // light dismissal and stays unhandled, as a menu leaves it.
    if (event.action == MouseAction::Down) dismiss();
    return inside;
}
void Tooltip::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.tooltip");
}

TooltipController::TooltipController(ui::Application& app, Desktop& desktop)
    : app_(app), desktop_(desktop), desktop_liveness_(desktop.lifetime_token()) {
    observer_ = app_.add_attention_observer([this](ui::Application::AttentionChange change) { observe(change); });
    // Claimed only when unclaimed, as Desktop claims its own defaults: an
    // application that answers the key itself keeps its answer.
    const ui::CommandId command = app_.commands().standard().tooltip;
    if (!app_.commands().has_handler(command)) {
        app_.commands().set_handler(command, [this] { show_for_focus(); });
        installed_command_handler_ = true;
    }
}

TooltipController::~TooltipController() {
    app_.remove_attention_observer(observer_);
    if (installed_command_handler_) app_.commands().set_handler(app_.commands().standard().tooltip, {});
    dismiss();
}

const TooltipController::Tip* TooltipController::tip_entry_for(const ui::View& view) const {
    // The nearest view up the ancestry that has a tip: a control's parts
    // explain themselves by the control's tip, as they resolve help keys.
    for (const ui::View* current = &view; current != nullptr; current = current->parent())
        for (const Tip& tip : tips_)
            if (tip.view == current && !tip.liveness.expired()) return &tip;
    return nullptr;
}

std::string TooltipController::tip_for(const ui::View& view) const {
    const Tip* const tip = tip_entry_for(view);
    return tip != nullptr ? tip->text : std::string{};
}

void TooltipController::set_tip(const ui::View& view, std::string text) {
    std::erase_if(tips_, [&view](const Tip& tip) { return tip.liveness.expired() || tip.view == &view; });
    // A tip on show whose words change goes; the next time the reader turns
    // to the view it says the new ones.
    if (shown_owner_ == &view) take_down();
    if (!text.empty()) tips_.push_back(Tip{&view, view.lifetime_token(), std::move(text)});
}

void TooltipController::set_delay(std::int64_t nanos) { delay_nanos_ = std::max<std::int64_t>(1, nanos); }

void TooltipController::cancel_pending() {
    if (pending_timer_ != 0) app_.cancel_timer(pending_timer_);
    pending_timer_ = 0;
    pending_target_ = nullptr;
    pending_liveness_.reset();
}

void TooltipController::arm(Source source, const ui::View& target) {
    cancel_pending();
    pending_source_ = source;
    pending_target_ = &target;
    pending_liveness_ = target.lifetime_token();
    // The destructor cancels this timer, so the callback never outlives the
    // controller it names.
    pending_timer_ = app_.start_timer(delay_nanos_, /*repeating=*/false, [this] {
        pending_timer_ = 0;
        const ui::View* const target_view = pending_target_;
        const bool alive = !pending_liveness_.expired();
        const Source source_now = pending_source_;
        cancel_pending();
        if (!alive || target_view == nullptr) return;
        // Only while the reader is still where the wait began.
        if (source_now == Source::Focus && app_.focused() != target_view) return;
        if (source_now == Source::Pointer && app_.hovered_view() != target_view) return;
        const Tip* const tip = tip_entry_for(*target_view);
        if (tip == nullptr) return;
        Rect anchor = target_view->absolute_bounds();
        if (source_now == Source::Pointer && app_.last_mouse_event())
            anchor = Rect{app_.last_mouse_event()->cell.x, app_.last_mouse_event()->cell.y, 1, 1};
        show(source_now, *tip, anchor);
    });
}

void TooltipController::show(Source source, const Tip& tip, Rect anchor) {
    take_down();
    if (desktop_liveness_.expired()) return;
    const Rect desktop = desktop_.absolute_bounds();
    Tooltip* const view = desktop_.add_popup(std::make_unique<Tooltip>(tip.text));
    tooltip_ = view;
    tooltip_liveness_ = view->lifetime_token();
    shown_source_ = source;
    shown_owner_ = tip.view;
    view->show_near(Rect{anchor.x - desktop.x, anchor.y - desktop.y, anchor.width, anchor.height},
                    Rect{0, 0, desktop.width, desktop.height});
    if (source != Source::Command) return;
    // Asked for from the keyboard, it is held the way an open menu is: keys
    // come to it through a scope of its own, the pointer through capture,
    // and either one puts it away.
    view->set_held(true);
    view->on_dismiss = [this] { take_down(); };
    held_scope_ = app_.push_modal(*view);
    app_.set_input_capture(view);
}

void TooltipController::take_down() {
    if (tooltip_ == nullptr) return;
    Tooltip* const view = tooltip_;
    const bool alive = !tooltip_liveness_.expired();
    tooltip_ = nullptr;
    tooltip_liveness_.reset();
    shown_owner_ = nullptr;
    const ui::Application::ModalScopeId scope = held_scope_;
    held_scope_ = 0;
    if (!alive) return;  // the desktop took it down already, scope and all
    if (app_.input_capture() == view) app_.clear_input_capture();
    if (scope != 0) app_.pop_modal(scope);
    if (!desktop_liveness_.expired()) desktop_.remove_popup(view);  // destroys it
}

void TooltipController::dismiss() {
    cancel_pending();
    take_down();
}

const Tooltip* TooltipController::tooltip() const noexcept {
    return tooltip_liveness_.expired() ? nullptr : tooltip_;
}

bool TooltipController::show_for_focus() {
    const ui::View* const focused = app_.focused();
    const Tip* const tip = focused != nullptr ? tip_entry_for(*focused) : nullptr;
    if (tip == nullptr) return false;
    cancel_pending();
    show(Source::Command, *tip, focused->absolute_bounds());
    return true;
}

void TooltipController::observe(ui::Application::AttentionChange change) {
    // A held tip answers for itself, as an open menu does, until the reader
    // puts it away.
    if (tooltip() != nullptr && held_scope_ != 0) return;
    using Change = ui::Application::AttentionChange;
    if (change == Change::Input) {
        // The reader is doing something: an explanation in the way of it
        // goes, and one still waiting to appear does not.
        dismiss();
        return;
    }
    if (change == Change::Focus) {
        if (tooltip() != nullptr && shown_source_ == Source::Focus) take_down();
        const ui::View* const focused = app_.focused();
        if (focused != nullptr && tip_entry_for(*focused) != nullptr)
            arm(Source::Focus, *focused);
        else if (pending_timer_ != 0 && pending_source_ == Source::Focus)
            cancel_pending();
        return;
    }
    const ui::View* const hovered = app_.hovered_view();
    // Onto the tooltip itself: the reader has come to read it.
    if (hovered != nullptr && tooltip() != nullptr && is_within(hovered, *tooltip_)) return;
    const Tip* const tip = hovered != nullptr ? tip_entry_for(*hovered) : nullptr;
    const bool same_tip_on_show = tip != nullptr && tooltip() != nullptr && tip->view == shown_owner_;
    if (tooltip() != nullptr && shown_source_ == Source::Pointer && !same_tip_on_show) take_down();
    if (tip != nullptr && !same_tip_on_show)
        arm(Source::Pointer, *hovered);
    else if (pending_timer_ != 0 && pending_source_ == Source::Pointer)
        cancel_pending();
}

}  // namespace ckv::widgets
