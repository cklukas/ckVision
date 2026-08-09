// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <compare>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/command.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

struct DateValue {
    int year = 2026;
    int month = 1;
    int day = 1;

    friend bool operator==(const DateValue&, const DateValue&) = default;
    friend auto operator<=>(const DateValue&, const DateValue&) = default;
};

struct TimeValue {
    int hour = 0;
    int minute = 0;
    int second = 0;

    friend bool operator==(const TimeValue&, const TimeValue&) = default;
};

class CalendarView : public ui::View {
public:
    CalendarView();

    void set_month(DateValue first_of_month);
    DateValue month() const noexcept { return month_; }
    void set_selected(DateValue selected);
    DateValue selected() const noexcept { return selected_; }
    void set_today(std::optional<DateValue> today);
    const std::optional<DateValue>& today() const noexcept { return today_; }
    void set_range(std::optional<DateValue> minimum, std::optional<DateValue> maximum);
    void set_disabled_predicate(std::function<bool(DateValue)> predicate);
    void set_labels(std::vector<std::string> month_labels, std::vector<std::string> weekday_labels);
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
    std::optional<DateValue> minimum_;
    std::optional<DateValue> maximum_;
    std::function<bool(DateValue)> disabled_;
    std::vector<std::string> month_labels_;
    std::vector<std::string> weekday_labels_;
    bool show_iso_week_numbers_ = false;
    bool has_focus_ = false;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

class DatePicker : public ui::View {
public:
    DatePicker();
    CalendarView& calendar() noexcept { return calendar_; }
    const CalendarView& calendar() const noexcept { return calendar_; }
    void set_value(DateValue value);
    DateValue value() const noexcept { return value_; }
    std::function<void(DateValue)> on_change;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    CalendarView calendar_;
    DateValue value_{2026, 1, 1};
    bool open_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
};

class TimePicker : public ui::View {
public:
    TimePicker();
    void set_value(TimeValue value);
    TimeValue value() const noexcept { return value_; }
    void set_show_seconds(bool show);
    void set_24_hour(bool enabled);
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
    bool has_focus_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
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

private:
    std::string query_;
    bool has_focus_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
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

class CommandPalette : public ui::View {
public:
    CommandPalette();
    void set_query(std::string query);
    const std::string& query() const noexcept { return query_; }
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
    ui::RoleId role_ = ui::kInvalidRole;
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
    bool persistent = false;
};

class NotificationCenter : public ui::View {
public:
    std::size_t add(Notification notification);
    void dismiss(std::size_t index);
    const std::vector<Notification>& notifications() const noexcept { return notifications_; }

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    void on_attached() override;

private:
    std::vector<Notification> notifications_;
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
