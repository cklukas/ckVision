// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/date_time.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <utility>
#include "cvision/core/ascii.hpp"
#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "widgets/common_internal.hpp"
#include "widgets/mnemonic_internal.hpp"

namespace ckv::widgets {
using detail::is_press;
using detail::stepped;
using detail::enclosing_desktop;
using detail::tail_to_width;
using detail::without_last_grapheme;
using detail::trim_spaces;
using detail::typed_text;
using detail::entry_caret;

namespace {
// A month is drawn on six week rows: enough for a 31-day month that begins
// on the last day of a week. Each row is seven days, each day two digits and
// a one-cell gap.
constexpr int kWeekRows = 6;
constexpr int kDayColumns = 7;
constexpr int kDayColumnWidth = 3;

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
const std::string& month_name(int month, const ui::UiStrings& labels) {
    const std::vector<std::string>& names =
        labels.month_names.size() == 12 ? labels.month_names : ui::english_ui_strings().month_names;
    return names[static_cast<std::size_t>(std::clamp(month, 1, 12) - 1)];
}

std::string field_text(DateValue date, DateField field, const DateFormat& format, const ui::UiStrings& labels) {
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
DateLayout layout_date(DateValue date, const DateFormat& format, const ui::UiStrings& labels) {
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

}  // namespace

std::string format_date(DateValue date, const DateFormat& format, const ui::UiStrings& labels) {
    if (format.format) return format.format(date);
    return layout_date(date, format, labels).text;
}

std::optional<DateValue> parse_date(std::string_view text, const DateFormat& format, const ui::UiStrings& labels) {
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
    if (notify && on_change) on_change(selected_);
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
    if (!enabled_in_tree() || !is_press(event)) return false;
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
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
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
    month->on_change = [this](std::size_t index) {
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
    if (month_labels_.empty()) set_month_labels(ui::english_ui_strings().month_names);
}

void CalendarDropdown::set_month_labels(std::vector<std::string> labels) {
    month_labels_ = std::move(labels);
    month_->set_items(month_labels_);
    calendar_->set_labels(month_labels_, {});
}

void CalendarDropdown::set_labels(const ui::UiStrings& labels) {
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

void DatePicker::set_labels(ui::UiStrings labels) {
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
    dropdown->calendar().on_change = [this, liveness](DateValue date) {
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
    if (!enabled_in_tree() || !is_press(event)) return false;
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
    if (!enabled_in_tree()) return false;
    edit_entry(entry_.value_or(std::string{}) + event.text);
    return true;
}

bool DatePicker::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree()) return false;
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
    if (!enabled_in_tree() || !is_press(event)) return false;
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
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
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


}  // namespace ckv::widgets
