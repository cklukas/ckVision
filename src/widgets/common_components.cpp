// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/common_components.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <utility>

#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {
namespace {

bool is_press(const KeyEvent& event) noexcept { return event.action == KeyAction::Press; }

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
    int y = date.year;
    int days = 365 * y + y / 4 - y / 100 + y / 400;
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

std::string time_text(TimeValue time, bool seconds, bool twenty_four_hour) {
    int hour = std::clamp(time.hour, 0, 23);
    std::string suffix;
    if (!twenty_four_hour) {
        suffix = hour < 12 ? " AM" : " PM";
        hour %= 12;
        if (hour == 0) hour = 12;
    }
    std::string result = two_digit(hour) + ":" + two_digit(std::clamp(time.minute, 0, 59));
    if (seconds) result += ":" + two_digit(std::clamp(time.second, 0, 59));
    return result + suffix;
}

bool contains_ci(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            if (lower(static_cast<unsigned char>(haystack[i + j])) !=
                lower(static_cast<unsigned char>(needle[j]))) {
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
    const int x_offset = show_iso_week_numbers_ ? 3 : 0;
    if (local.y < 2 || local.y >= 8 || local.x < x_offset) return std::nullopt;
    const int column = (local.x - x_offset) / 3;
    const int row = local.y - grid_top();
    if (column < 0 || column >= 7) return std::nullopt;
    const int first_weekday = weekday_index(month_, first_weekday_);
    const int day = row * 7 + column - first_weekday + 1;
    if (day < 1 || day > days_in_month(month_.year, month_.month)) return std::nullopt;
    return DateValue{month_.year, month_.month, day};
}

void CalendarView::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(normal_role_);
    const Style selected = context().theme->resolve(selected_role_);
    const Style disabled = context().theme->resolve(disabled_role_);
    const Style today_style = context().theme->resolve(today_role_);
    const Style marked_style = context().theme->resolve(marked_role_);
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
    const int x_offset = show_iso_week_numbers_ ? 3 : 0;
    // The weekday row names the columns rather than being one of them, so it
    // is set apart from the days it heads.
    Style header = normal;
    header.attrs = header.attrs | Attr::Bold;
    if (show_iso_week_numbers_) painter.draw_text(Point{0, header_row()}, "Wk", header);
    // The weekday names start at whichever day the week starts on.
    const int first_offset = static_cast<int>(first_weekday_);
    for (int i = 0; i < 7; ++i)
        painter.draw_text(Point{x_offset + i * 3, header_row()}, weekday_labels_[(first_offset + i) % 7], header);
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
        painter.draw_text(Point{x_offset + column * 3, grid_top() + row}, (day < 10 ? " " : "") + std::to_string(day), style);
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

void CalendarView::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    invalidate();
}

ui::SizeHint CalendarView::horizontal_size_hint() const { return ui::SizeHint{21, show_iso_week_numbers_ ? 24 : 21, ui::kUnboundedExtent}; }
ui::SizeHint CalendarView::vertical_size_hint() const { return ui::SizeHint{8, 8, 8}; }

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
constexpr int kGridWidth = 7 * 3 - 1;
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
    if (month_labels_.empty())
        set_month_labels({"January", "February", "March", "April", "May", "June", "July", "August",
                          "September", "October", "November", "December"});
}

void CalendarDropdown::set_month_labels(std::vector<std::string> labels) {
    month_labels_ = std::move(labels);
    month_->set_items(month_labels_);
    calendar_->set_labels(month_labels_, {});
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
    painter.draw_text(Point{span.x + std::max(0, (span.width - 7) / 2), kControlRow}, "invalid", style);
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

DatePicker::DatePicker() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{14, 1});
}

void DatePicker::set_value(std::optional<DateValue> value) {
    if (value) {
        value->year = std::clamp(value->year, kFirstCalendarYear, kLastCalendarYear);
        *value = clamp_day(*value);
        seed_ = *value;
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

void DatePicker::draw(scene::Painter& painter) {
    const ui::RoleId role = !valid_ ? invalid_role_ : (focused_ ? focused_role_ : normal_role_);
    const Style style = context().theme->resolve(role);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));
    const bool has_dropdown = calendar_app_ != nullptr && calendar_desktop_ != nullptr && bounds().width >= 2;
    const int value_width = std::max(0, bounds().width - (has_dropdown ? 2 : 0));
    if (!value_) {
        painter.draw_text(Point{0, 0}, text::clip_to_width("— no date —", value_width), style);
        if (has_dropdown) painter.draw_text(Point{bounds().width - 1, 0}, "▾", style);
        return;
    }

    const std::string rendered = format_iso_date(*value_);
    painter.draw_text(Point{0, 0}, text::clip_to_width(rendered, value_width), style);
    if (has_dropdown) painter.draw_text(Point{bounds().width - 1, 0}, "▾", style);
    if (!focused_) return;
    const int start = active_field_ == 0 ? 0 : (active_field_ == 1 ? 5 : 8);
    const int width = active_field_ == 0 ? 4 : 2;
    if (start >= value_width) return;
    Style active = style;
    active.attrs |= Attr::Reverse;
    painter.draw_text(Point{start, 0}, text::clip_to_width(rendered.substr(static_cast<std::size_t>(start),
                                                                           static_cast<std::size_t>(width)),
                                                   std::min(width, value_width - start)),
                      active);
}

bool DatePicker::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    switch (event.chord.key) {
        case Key::Left:
            active_field_ = std::max(0, active_field_ - 1);
            invalidate();
            return true;
        case Key::Right:
            active_field_ = std::min(2, active_field_ + 1);
            invalidate();
            return true;
        case Key::Up: adjust_active(1); return true;
        case Key::Down: adjust_active(-1); return true;
        case Key::PageUp:
            active_field_ = 1;
            adjust_active(1);
            return true;
        case Key::PageDown:
            active_field_ = 1;
            adjust_active(-1);
            return true;
        case Key::Delete:
        case Key::Backspace:
            if (empty_allowed_) set_value(std::nullopt);
            return true;
        case Key::Char:
            if (event.chord.text == " ") return open_calendar();
            return false;
        default: return false;
    }
}

bool DatePicker::on_mouse(const MouseEvent& event) {
    if (event.action == MouseAction::Down && event.button == MouseButton::Left) {
        const int local_x = event.cell.x - absolute_bounds().x;
        if (calendar_app_ != nullptr && calendar_desktop_ != nullptr && local_x >= bounds().width - 2) {
            return open_calendar();
        }
        select_field_at(local_x);
        invalidate();
        return true;
    }
    if (event.action == MouseAction::Wheel && event.button == MouseButton::WheelUp) {
        adjust_active(1);
        return true;
    }
    if (event.action == MouseAction::Wheel && event.button == MouseButton::WheelDown) {
        adjust_active(-1);
        return true;
    }
    return false;
}

void DatePicker::on_focus(const FocusEvent& event) {
    if (focused_ == event.gained) return;
    focused_ = event.gained;
    invalidate();
}

void DatePicker::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (invalid_role_ == ui::kInvalidRole) invalid_role_ = context().roles->find("ckv.input.invalid");
}

void DatePicker::adjust_active(int delta) {
    DateValue adjusted = value_.value_or(seed_);
    if (active_field_ == 0) {
        adjusted.year = std::clamp(adjusted.year + delta, kFirstCalendarYear, kLastCalendarYear);
        adjusted = clamp_day(adjusted);
    } else if (active_field_ == 1) {
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
    if (x >= 8) active_field_ = 2;
    else if (x >= 5) active_field_ = 1;
    else active_field_ = 0;
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
    invalidate();
}

void TimePicker::set_24_hour(bool enabled) {
    twenty_four_hour_ = enabled;
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
    const Style style = context().theme->resolve(!valid_ ? invalid_role_ : has_focus_ ? focused_role_ : role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));
    painter.draw_text(Point{0, 0}, text::clip_to_width(time_text(value_, show_seconds_, twenty_four_hour_), bounds().width), style);
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
        return true;
    }
    if (event.chord.key == Key::Right) {
        field_ = std::min(show_seconds_ ? 2 : 1, field_ + 1);
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

void TimePicker::on_focus(const FocusEvent& event) { has_focus_ = event.gained; invalidate(); }
void TimePicker::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (invalid_role_ == ui::kInvalidRole) invalid_role_ = context().roles->find("ckv.input.invalid");
}

SpinBox::SpinBox() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{10, 1});
}
void SpinBox::set_range(int minimum, int maximum) { minimum_ = std::min(minimum, maximum); maximum_ = std::max(minimum, maximum); set_value(value_); }
void SpinBox::set_step(int step) { step_ = std::max(1, step); }
void SpinBox::set_value(int value) {
    value = std::clamp(value, minimum_, maximum_);
    if (value_ == value) return;
    value_ = value;
    invalidate();
    if (on_change) on_change(value_);
}
void SpinBox::adjust(int delta) { set_value(value_ + delta * step_); }
void SpinBox::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(has_focus_ ? focused_role_ : role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));
    painter.draw_text(Point{0, 0}, text::clip_to_width("< " + std::to_string(value_) + " >", bounds().width), style);
}
bool SpinBox::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    if (event.chord.key == Key::Up || event.chord.key == Key::Right) { adjust(1); return true; }
    if (event.chord.key == Key::Down || event.chord.key == Key::Left) { adjust(-1); return true; }
    return false;
}
bool SpinBox::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    adjust(event.cell.x - absolute_bounds().x >= bounds().width / 2 ? 1 : -1);
    return true;
}
void SpinBox::on_focus(const FocusEvent& event) { has_focus_ = event.gained; invalidate(); }
void SpinBox::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
}

Slider::Slider() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{20, 1});
}
void Slider::set_range(int minimum, int maximum) { minimum_ = std::min(minimum, maximum); maximum_ = std::max(minimum, maximum); set_value(value_); }
void Slider::set_step(int step) { step_ = std::max(1, step); }
void Slider::set_value(int value) {
    value = std::clamp(value, minimum_, maximum_);
    if (value_ == value) return;
    value_ = value;
    invalidate();
    if (on_change) on_change(value_);
}
void Slider::adjust(int delta) { set_value(value_ + delta * step_); }
int Slider::value_from_x(int x) const {
    if (bounds().width <= 1 || maximum_ == minimum_) return minimum_;
    return minimum_ + (maximum_ - minimum_) * std::clamp(x, 0, bounds().width - 1) / (bounds().width - 1);
}
void Slider::draw(scene::Painter& painter) {
    const Style track = context().theme->resolve(role_);
    const Style fill = context().theme->resolve(fill_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme("─", track));
    const int pos = value_from_x(bounds().width - 1) == minimum_ ? 0 :
        (value_ - minimum_) * std::max(1, bounds().width - 1) / std::max(1, maximum_ - minimum_);
    painter.fill(Rect{0, 0, std::clamp(pos, 0, bounds().width - 1), 1}, Cell::from_grapheme("━", fill));
    painter.draw_text(Point{std::clamp(pos, 0, std::max(0, bounds().width - 1)), 0}, has_focus_ ? "◆" : "●", fill);
}
bool Slider::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    if (event.chord.key == Key::Left) { adjust(-1); return true; }
    if (event.chord.key == Key::Right) { adjust(1); return true; }
    if (event.chord.key == Key::Home) { set_value(minimum_); return true; }
    if (event.chord.key == Key::End) { set_value(maximum_); return true; }
    return false;
}
bool Slider::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    set_value(value_from_x(event.cell.x - absolute_bounds().x));
    return true;
}
void Slider::on_focus(const FocusEvent& event) { has_focus_ = event.gained; invalidate(); }
void Slider::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.list.normal");
    if (fill_role_ == ui::kInvalidRole) fill_role_ = context().roles->find("ckv.menu.bar.active");
}

SearchBox::SearchBox() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{20, 1});
}
void SearchBox::set_query(std::string query) {
    if (query_ == query) return;
    query_ = std::move(query);
    invalidate();
    if (on_change) on_change(query_);
}
void SearchBox::clear() {
    set_query({});
    if (on_clear) on_clear();
}
void SearchBox::draw(scene::Painter& painter) {
    // A search box has to look like something you can type into. Drawing the
    // prompt, the text and a "[x]" as one run of label-coloured cells made it
    // read as a caption, and put the clear control wherever the query
    // happened to end — nowhere near the columns that actually respond to a
    // click on it.
    const Style style = context().theme->resolve(has_focus_ ? focused_role_ : role_);
    const Style label = context().theme->resolve(label_role_);
    const int width = bounds().width;
    if (width <= 0) return;
    painter.fill(Rect{0, 0, width, 1}, Cell::from_grapheme(" ", label));

    const std::string prompt = "Search ";
    const int prompt_width = std::min(text::text_width(prompt), width);
    painter.draw_text(Point{0, 0}, text::clip_to_width(prompt, prompt_width), label);

    // The field runs from the prompt to the right edge, less the clear
    // control's own columns when there is a query to clear. Its own
    // background is what says "text goes here".
    const int clear_width = query_.empty() ? 0 : kClearControlWidth;
    const int field_x = prompt_width;
    const int field_width = std::max(0, width - field_x - clear_width);
    if (field_width > 0) {
        painter.fill(Rect{field_x, 0, field_width, 1}, Cell::from_grapheme(" ", style));
        painter.draw_text(Point{field_x, 0}, text::clip_to_width(query_, field_width), style);
    }
    if (clear_width > 0 && width - clear_width >= 0)
        painter.draw_text(Point{width - clear_width, 0}, "[x]", label);
}

std::optional<CursorState> SearchBox::cursor_state() const {
    // The caret is the other half of "you can type here", and it also tells
    // the reader which pane the keyboard is in.
    if (!has_focus_) return std::nullopt;
    const int width = bounds().width;
    const int prompt_width = std::min(static_cast<int>(std::string_view("Search ").size()), width);
    const int clear_width = query_.empty() ? 0 : kClearControlWidth;
    const int field_width = std::max(0, width - prompt_width - clear_width);
    const int caret = std::min(text::text_width(query_), std::max(0, field_width - 1));
    const Rect absolute = absolute_bounds();
    return CursorState{true, Point{absolute.x + prompt_width + caret, absolute.y}, CursorShape::Bar, false};
}
bool SearchBox::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    if (event.chord.key == Key::Backspace && !query_.empty()) { set_query(query_.substr(0, query_.size() - 1)); return true; }
    if (event.chord.key == Key::Escape) { clear(); return true; }
    // Terminals report ordinary typed characters as Key::Char events; only
    // IMEs and bracketed paste arrive as TextEvent (the same normalization
    // InputLine performs, for the same reason). Without this a search box
    // cannot be typed into at all, which is its only purpose. Alt/Ctrl/Super
    // characters stay chords so command routing still sees them; Shift is
    // part of ordinary text production.
    if (event.chord.key == Key::Char && !event.chord.text.empty() &&
        !has_modifier(event.chord.modifiers, Modifier::Alt) &&
        !has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
        !has_modifier(event.chord.modifiers, Modifier::Super))
        return on_text(TextEvent{event.chord.text, false});
    return false;
}
bool SearchBox::on_text(const TextEvent& event) { set_query(query_ + event.text); return true; }
bool SearchBox::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down) return false;
    // The clear control only exists while there is something to clear, and
    // only where it is drawn — the right edge. It used to answer clicks on
    // the last three columns whether or not it was drawn there.
    if (!query_.empty() && event.cell.x >= absolute_bounds().x + bounds().width - kClearControlWidth) {
        clear();
        return true;
    }
    // A click anywhere else in the field is a request to type in it.
    return true;
}
void SearchBox::on_focus(const FocusEvent& event) { has_focus_ = event.gained; invalidate(); }
void SearchBox::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (label_role_ == ui::kInvalidRole) label_role_ = context().roles->find("ckv.label.text");
}

void ToolBar::set_commands(std::vector<ui::CommandId> commands) { commands_ = std::move(commands); invalidate(); }
void ToolBar::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(role_);
    const Style disabled = context().theme->resolve(disabled_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", normal));
    int x = 0;
    for (ui::CommandId command : commands_) {
        const bool enabled = context().app == nullptr || context().app->commands().is_enabled(command);
        const std::string label = "[" + command_title(context().app, command) + "]";
        painter.draw_text(Point{x, 0}, text::clip_to_width(label, bounds().width - x), enabled ? normal : disabled);
        x += text::text_width(label) + 1;
        if (x >= bounds().width) break;
    }
}
int ToolBar::command_at_x(int x) const {
    int cursor = 0;
    for (std::size_t i = 0; i < commands_.size(); ++i) {
        const int width = text::text_width("[" + command_title(context().app, commands_[i]) + "]");
        if (x >= cursor && x < cursor + width) return static_cast<int>(i);
        cursor += width + 1;
    }
    return -1;
}
bool ToolBar::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left || context().app == nullptr) return false;
    const int index = command_at_x(event.cell.x - absolute_bounds().x);
    if (index < 0) return false;
    return context().app->commands().execute(commands_[static_cast<std::size_t>(index)]);
}
ui::SizeHint ToolBar::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }
void ToolBar::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.menu.bar.normal");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.menu.dropdown.disabled");
}

CommandPalette::CommandPalette() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{40, 8});
}
void CommandPalette::set_query(std::string query) { query_ = std::move(query); highlighted_ = 0; invalidate(); }
std::vector<ui::CommandInfo> CommandPalette::filtered_commands() const {
    if (context().app == nullptr) return {};
    std::vector<ui::CommandInfo> out;
    for (const ui::CommandInfo& info : context().app->commands().all())
        if (info.visibility == ui::CommandVisibility::Palette && contains_ci(info.title, query_) &&
            context().app->commands().is_available(info.id))
            out.push_back(info);
    return out;
}
std::optional<ui::CommandId> CommandPalette::highlighted_command() const {
    const auto commands = filtered_commands();
    if (commands.empty()) return std::nullopt;
    return commands[std::min(highlighted_, commands.size() - 1)].id;
}
void CommandPalette::draw(scene::Painter& painter) {
    const Style input = context().theme->resolve(input_role_);
    const Style normal = context().theme->resolve(result_role_);
    const Style selected = context().theme->resolve(selected_role_);
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));

    // The palette deliberately reserves a one-cell inset on every side: the
    // search field reads as an edit control, while its results read as a
    // separate, scrollable choice surface.  This is also what keeps a
    // constrained palette legible when it sits above document content.
    constexpr int kInset = 2;
    constexpr int kSearchRow = 2;
    constexpr int kFirstResultRow = 4;
    const int result_width = std::max(0, bounds().width - 2 * kInset);
    painter.fill(Rect{kInset, kSearchRow, result_width, 1}, Cell::from_grapheme(" ", input));
    painter.draw_text(Point{kInset + 1, kSearchRow},
                      text::clip_to_width(query_, std::max(0, result_width - 1)), input);

    const auto commands = filtered_commands();
    const int visible_rows = std::max(0, bounds().height - kFirstResultRow);
    const std::size_t first = visible_rows == 0 || highlighted_ < static_cast<std::size_t>(visible_rows)
                                  ? 0
                                  : highlighted_ - static_cast<std::size_t>(visible_rows) + 1;
    const bool needs_scrollbar = commands.size() > static_cast<std::size_t>(visible_rows);
    const int text_columns = std::max(0, result_width - (needs_scrollbar ? 1 : 0));
    const Style mnemonic = accent_style(normal, context().theme->resolve(context().roles->find("ckv.hotkey")));
    const Style selected_mnemonic =
        accent_style(selected, context().theme->resolve(context().roles->find("ckv.hotkey")));
    for (std::size_t index = first;
         index < commands.size() && index - first < static_cast<std::size_t>(visible_rows); ++index) {
        const Style style = index == highlighted_ ? selected : normal;
        const int row = kFirstResultRow + static_cast<int>(index - first);
        painter.fill(Rect{kInset, row, result_width, 1}, Cell::from_grapheme(" ", style));
        // A palette that hides the binding teaches nobody the keyboard: a
        // command reached here once is meant to be reached by its chord
        // next time, so the chord travels with the title.
        const auto parsed = parse_mnemonic(commands[index].title);
        const int title_columns = std::max(0, text_columns - 1);
        draw_mnemonic(painter, Point{kInset + 1, row}, parsed, title_columns, style,
                      index == highlighted_ ? selected_mnemonic : mnemonic);
        const auto chord = context().app->commands().chord_for_command(commands[index].id);
        if (!chord) continue;
        const std::string annotation = "  (" + context().app->commands().format_chord(*chord) + ")";
        const int title_width = text::text_width(parsed.display);
        const int annotation_columns = title_columns - title_width;
        if (annotation_columns <= 0) continue;
        painter.draw_text(Point{kInset + 1 + title_width, row},
                          text::clip_to_width(annotation, annotation_columns), style);
    }
    if (needs_scrollbar) {
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
bool CommandPalette::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    const auto commands = filtered_commands();
    if (event.chord.key == Key::Down && !commands.empty()) { highlighted_ = std::min(highlighted_ + 1, commands.size() - 1); invalidate(); return true; }
    if (event.chord.key == Key::Up && highlighted_ > 0) { --highlighted_; invalidate(); return true; }
    if (event.chord.key == Key::Backspace && !query_.empty()) { query_.pop_back(); highlighted_ = 0; invalidate(); return true; }
    if (event.chord.key == Key::Enter && context().app != nullptr) {
        if (auto command = highlighted_command()) return context().app->commands().execute(*command);
    }
    return false;
}
bool CommandPalette::on_text(const TextEvent& event) { set_query(query_ + event.text); return true; }
void CommandPalette::on_focus(const FocusEvent& event) { has_focus_ = event.gained; invalidate(); }
void CommandPalette::on_attached() {
    if (input_role_ == ui::kInvalidRole) input_role_ = context().roles->find("ckv.input.normal");
    if (result_role_ == ui::kInvalidRole) result_role_ = context().roles->find("ckv.option.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.option.focused");
}

void BreadcrumbBar::set_segments(std::vector<std::string> segments) { segments_ = std::move(segments); focused_ = 0; invalidate(); }
void BreadcrumbBar::set_separator(std::string separator) { separator_ = std::move(separator); invalidate(); }
int BreadcrumbBar::segment_at_x(int x) const {
    int cursor = 0;
    for (std::size_t i = 0; i < segments_.size(); ++i) {
        const int width = text::text_width(segments_[i]);
        if (x >= cursor && x < cursor + width) return static_cast<int>(i);
        cursor += width + text::text_width(separator_);
    }
    return -1;
}
void BreadcrumbBar::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(role_);
    const Style focused = context().theme->resolve(focused_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", normal));
    int x = 0;
    for (std::size_t i = 0; i < segments_.size() && x < bounds().width; ++i) {
        painter.draw_text(Point{x, 0}, text::clip_to_width(segments_[i], bounds().width - x),
                          has_focus_ && i == focused_ ? focused : normal);
        x += text::text_width(segments_[i]);
        if (i + 1 < segments_.size()) {
            painter.draw_text(Point{x, 0}, separator_, normal);
            x += text::text_width(separator_);
        }
    }
}
bool BreadcrumbBar::on_key(const KeyEvent& event) {
    if (!is_press(event) || segments_.empty()) return false;
    if (event.chord.key == Key::Left && focused_ > 0) { --focused_; invalidate(); return true; }
    if (event.chord.key == Key::Right && focused_ + 1 < segments_.size()) { ++focused_; invalidate(); return true; }
    if (event.chord.key == Key::Enter && on_activate) { on_activate(focused_); return true; }
    return false;
}
bool BreadcrumbBar::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const int index = segment_at_x(event.cell.x - absolute_bounds().x);
    if (index < 0) return false;
    focused_ = static_cast<std::size_t>(index);
    if (on_activate) on_activate(focused_);
    invalidate();
    return true;
}
void BreadcrumbBar::on_focus(const FocusEvent& event) { has_focus_ = event.gained; invalidate(); }
void BreadcrumbBar::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.label.text");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.list.selected");
}

void PropertyInspector::set_items(std::vector<PropertyItem> items) { items_ = std::move(items); cursor_ = items_.empty() ? -1 : 0; invalidate(); }
void PropertyInspector::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(role_);
    const Style selected = context().theme->resolve(selected_role_);
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));
    for (std::size_t i = 0; i < items_.size() && static_cast<int>(i) < bounds().height; ++i) {
        const std::string row = items_[i].name + ": " + items_[i].value;
        painter.draw_text(Point{0, static_cast<int>(i)}, text::clip_to_width(row, bounds().width),
                          static_cast<int>(i) == cursor_ ? selected : normal);
    }
}
bool PropertyInspector::on_key(const KeyEvent& event) {
    if (!is_press(event) || items_.empty()) return false;
    if (event.chord.key == Key::Down) { cursor_ = std::min<int>(cursor_ + 1, static_cast<int>(items_.size()) - 1); invalidate(); return true; }
    if (event.chord.key == Key::Up) { cursor_ = std::max(0, cursor_ - 1); invalidate(); return true; }
    if (event.chord.key == Key::Enter && items_[cursor_].editable) { editing_ = !editing_; invalidate(); return true; }
    if (event.chord.key == Key::Backspace && editing_ && !items_[cursor_].value.empty()) {
        items_[cursor_].value.pop_back();
        if (on_change) on_change(static_cast<std::size_t>(cursor_), items_[cursor_].value);
        invalidate();
        return true;
    }
    return false;
}
bool PropertyInspector::on_text(const TextEvent& event) {
    if (!editing_ || cursor_ < 0 || !items_[cursor_].editable) return false;
    items_[cursor_].value += event.text;
    if (on_change) on_change(static_cast<std::size_t>(cursor_), items_[cursor_].value);
    invalidate();
    return true;
}
void PropertyInspector::on_focus(const FocusEvent& event) { has_focus_ = event.gained; invalidate(); }
void PropertyInspector::on_attached() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
}

void Wizard::set_pages(std::vector<WizardPage> pages) { pages_ = std::move(pages); current_page_ = 0; invalidate(); }
bool Wizard::can_go_next() const {
    if (pages_.empty() || current_page_ + 1 >= pages_.size()) return false;
    return !pages_[current_page_].can_continue || pages_[current_page_].can_continue();
}
bool Wizard::next() { if (!can_go_next()) return false; ++current_page_; invalidate(); return true; }
bool Wizard::back() { if (!can_go_back()) return false; --current_page_; invalidate(); return true; }
bool Wizard::finish() {
    if (pages_.empty() || current_page_ + 1 != pages_.size()) return false;
    if (pages_[current_page_].can_continue && !pages_[current_page_].can_continue()) return false;
    if (on_finish) on_finish();
    return true;
}
void Wizard::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(role_);
    const Style selected = context().theme->resolve(selected_role_);
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));
    const std::string title = pages_.empty() ? "Wizard" : pages_[current_page_].title;
    painter.draw_text(Point{0, 0}, text::clip_to_width(title, bounds().width), selected);
    painter.draw_text(Point{0, std::max(0, bounds().height - 1)},
                      text::clip_to_width((can_go_back() ? "< Back " : "       ") +
                                              std::string(can_go_next() ? "Next >" : "Finish"),
                                          bounds().width),
                      normal);
}
bool Wizard::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    if (event.chord.key == Key::Right || event.chord.key == Key::Enter) return next() || finish();
    if (event.chord.key == Key::Left) return back();
    if (event.chord.key == Key::Escape && on_cancel) { on_cancel(); return true; }
    return false;
}
void Wizard::on_attached() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.dialog.background");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.window.title.active");
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
    invalidate();
    if (on_changed) on_changed();
}

void NotificationCenter::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(role_);
    // Only the rows that have something on them. An empty centre paints
    // nothing at all, which is what lets a host leave one lying over its
    // desktop at a generous size instead of resizing it on every post: the
    // cells it does not write show whatever is underneath.
    for (std::size_t i = 0; i < notifications_.size() && static_cast<int>(i) < bounds().height; ++i) {
        const std::string prefix = notifications_[i].severity == NotificationSeverity::Info ? "i " :
                                   notifications_[i].severity == NotificationSeverity::Warning ? "! " : "x ";
        const int row = static_cast<int>(i);
        painter.fill(Rect{0, row, bounds().width, 1}, Cell::from_grapheme(" ", style));
        painter.draw_text(Point{0, row},
                          text::clip_to_width(prefix + notifications_[i].text, bounds().width), style);
    }
}

bool NotificationCenter::on_key(const KeyEvent& event) {
    if (is_press(event) && event.chord.key == Key::Escape && !notifications_.empty()) { dismiss(notifications_.size() - 1); return true; }
    return false;
}

bool NotificationCenter::on_mouse(const MouseEvent& event) {
    // A click takes away the line it landed on — including a persistent one,
    // which is the reader saying they have read it. Clicks past the last
    // notification are not ours: an empty centre draws nothing there, and
    // consuming a press over what looks like bare desktop would swallow the
    // click a reader aimed at whatever is beneath.
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Point local{event.cell.x - absolute_bounds().x, event.cell.y - absolute_bounds().y};
    if (local.y < 0 || static_cast<std::size_t>(local.y) >= notifications_.size()) return false;
    dismiss(static_cast<std::size_t>(local.y));
    return true;
}

void NotificationCenter::on_attached() {
    // The focus policy is NOT set here — see the constructor. Attaching is
    // where a view learns its context, not where it overrules decisions its
    // host has already made about it.
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.statusline.normal");
    // Anything posted before there was a clock to read has no deadline yet.
    // Re-timing here is what makes "post first, attach later" behave the same
    // as the ordinary way round, rather than leaving those entries immortal.
    if (auto_dismiss_nanos_ > 0) set_auto_dismiss(auto_dismiss_nanos_);
}

Tooltip::Tooltip(std::string text) : text_(std::move(text)) {
    set_visible(false);
    set_preferred_size(Size{static_cast<int>(text::text_width(text_)) + 2, 1});
}
void Tooltip::set_text(std::string text) {
    text_ = std::move(text);
    set_preferred_size(Size{static_cast<int>(text::text_width(text_)) + 2, 1});
    invalidate();
    size_hint_changed();
}
void Tooltip::show_at(Point position) {
    set_bounds(Rect{position.x, position.y, std::max(2, text::text_width(text_) + 2), 1});
    set_visible(true);
}
void Tooltip::hide() { set_visible(false); }
void Tooltip::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));
    painter.draw_text(Point{1, 0}, text::clip_to_width(text_, std::max(0, bounds().width - 2)), style);
}
void Tooltip::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.menu.dropdown.normal");
}

}  // namespace ckv::widgets
