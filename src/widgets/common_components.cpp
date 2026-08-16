// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/common_components.hpp"

#include <algorithm>
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

int weekday_monday_zero(DateValue date) noexcept {
    // 0000-03-01 style weekday math is unnecessary here; the absolute offset
    // only needs to be stable. 1970-01-01 was Thursday, hence +3 for Monday=0.
    const int days = serial(date) - serial(DateValue{1970, 1, 1});
    return ((days + 3) % 7 + 7) % 7;
}

std::string two_digit(int value) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02d", value);
    return buffer;
}

std::string date_text(DateValue date) {
    return std::to_string(date.year) + "-" + two_digit(date.month) + "-" + two_digit(date.day);
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

std::string command_title(ui::Application* app, ui::CommandId id) {
    if (app == nullptr) return "#" + std::to_string(id);
    if (const ui::CommandInfo* info = app->commands().find(id); info != nullptr) return info->title;
    return "#" + std::to_string(id);
}

}  // namespace

CalendarView::CalendarView() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{24, 9});
    month_labels_ = {"January", "February", "March", "April", "May", "June",
                     "July", "August", "September", "October", "November", "December"};
    weekday_labels_ = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
}

void CalendarView::set_month(DateValue first_of_month) {
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
    const int row = local.y - 2;
    if (column < 0 || column >= 7) return std::nullopt;
    const int first_weekday = weekday_monday_zero(month_);
    const int day = row * 7 + column - first_weekday + 1;
    if (day < 1 || day > days_in_month(month_.year, month_.month)) return std::nullopt;
    return DateValue{month_.year, month_.month, day};
}

void CalendarView::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(normal_role_);
    const Style selected = context().theme->resolve(selected_role_);
    const Style disabled = context().theme->resolve(disabled_role_);
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));
    const std::string title = month_labels_[std::clamp(month_.month, 1, 12) - 1] + " " + std::to_string(month_.year);
    painter.draw_text(Point{0, 0}, text::clip_to_width(title, bounds().width), normal);
    const int x_offset = show_iso_week_numbers_ ? 3 : 0;
    if (show_iso_week_numbers_) painter.draw_text(Point{0, 1}, "Wk", normal);
    for (int i = 0; i < 7; ++i) painter.draw_text(Point{x_offset + i * 3, 1}, weekday_labels_[i], normal);
    for (int day = 1; day <= days_in_month(month_.year, month_.month); ++day) {
        const DateValue date{month_.year, month_.month, day};
        const int index = weekday_monday_zero(month_) + day - 1;
        const int row = index / 7;
        const int column = index % 7;
        const Style style = !selectable(date) ? disabled : (date == selected_ ? selected : normal);
        painter.draw_text(Point{x_offset + column * 3, 2 + row}, (day < 10 ? " " : "") + std::to_string(day), style);
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

void CalendarView::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.menu.dropdown.disabled");
}

DatePicker::DatePicker() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{12, 1});
    calendar_.set_selected(value_);
    calendar_.on_select = [this](DateValue value) {
        set_value(value);
        open_ = false;
    };
}

void DatePicker::set_value(DateValue value) {
    value = clamp_day(value);
    if (value_ == value) return;
    value_ = value;
    calendar_.set_selected(value);
    invalidate();
    if (on_change) on_change(value_);
}

void DatePicker::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));
    painter.draw_text(Point{0, 0}, text::clip_to_width(date_text(value_) + " v", bounds().width), style);
}

bool DatePicker::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    if (event.chord.key == Key::Enter) {
        open_ = !open_;
        invalidate();
        return true;
    }
    return open_ ? calendar_.on_key(event) : false;
}

bool DatePicker::on_mouse(const MouseEvent& event) {
    if (event.action == MouseAction::Down && event.button == MouseButton::Left) {
        open_ = !open_;
        invalidate();
        return true;
    }
    return false;
}

void DatePicker::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.input.normal");
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
    const Style style = context().theme->resolve(has_focus_ ? focused_role_ : role_);
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
    const Style style = context().theme->resolve(has_focus_ ? focused_role_ : role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));
    painter.draw_text(Point{0, 0}, text::clip_to_width("Search: " + query_ + " [x]", bounds().width), style);
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
    if (event.action == MouseAction::Down && event.cell.x >= absolute_bounds().x + bounds().width - 3) { clear(); return true; }
    return false;
}
void SearchBox::on_focus(const FocusEvent& event) { has_focus_ = event.gained; invalidate(); }
void SearchBox::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
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
        if (info.id >= ui::kFirstApplicationCommandId && contains_ci(info.title, query_) &&
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

std::size_t NotificationCenter::add(Notification notification) {
    notifications_.push_back(std::move(notification));
    invalidate();
    return notifications_.size() - 1;
}
void NotificationCenter::dismiss(std::size_t index) {
    if (index >= notifications_.size()) return;
    notifications_.erase(notifications_.begin() + static_cast<std::ptrdiff_t>(index));
    invalidate();
}
void NotificationCenter::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(role_);
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", style));
    for (std::size_t i = 0; i < notifications_.size() && static_cast<int>(i) < bounds().height; ++i) {
        std::string prefix = notifications_[i].severity == NotificationSeverity::Info ? "i " :
                             notifications_[i].severity == NotificationSeverity::Warning ? "! " : "x ";
        painter.draw_text(Point{0, static_cast<int>(i)}, text::clip_to_width(prefix + notifications_[i].text, bounds().width), style);
    }
}
bool NotificationCenter::on_key(const KeyEvent& event) {
    if (is_press(event) && event.chord.key == Key::Escape && !notifications_.empty()) { dismiss(notifications_.size() - 1); return true; }
    return false;
}
void NotificationCenter::on_attached() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.statusline.normal");
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
