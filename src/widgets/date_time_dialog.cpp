// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/date_time_dialog.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "cvision/core/text.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/text_layout.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {
namespace {

using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

// The two rows under the calendar that say why a typed year was refused:
// the year field's own reason, word-wrapped, in the colour a dialog gives a
// veto's message -- two rows, as a dialog's description panel keeps for one,
// hold a sentence of it. It reads the reason when it paints, and the window
// it is part of repaints whenever the field changes, so it never shows a
// reason that no longer stands.
class RefusalPanel final : public ui::View {
public:
    static constexpr int kRows = 2;

    RefusalPanel(const SpinBox& source, ui::RoleId role) : source_(source), role_(role) {}

    ui::SizeHint vertical_size_hint() const override { return ui::SizeHint{kRows, kRows, kRows}; }
    ui::SizeHint horizontal_size_hint() const override { return ui::SizeHint{0, 0, ui::kUnboundedExtent}; }

    void draw(scene::Painter& painter) override {
        const Style style = context().theme->resolve(role_);
        painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", style));
        const std::string& reason = source_.validation_message();
        int y = 0;
        for (const WrapSegment& segment : wrap_text(reason, WrapOptions{std::max(1, bounds().width), WrapMode::Word, 0})) {
            if (y >= bounds().height) break;
            painter.draw_text(Point{0, y++}, std::string_view(reason).substr(segment.begin, segment.end - segment.begin),
                              style);
        }
    }

private:
    const SpinBox& source_;
    ui::RoleId role_;
};

// `date` with its day brought into its month: the day a month or year change
// keeps, as far as the month allows.
DateValue within_month(DateValue date) noexcept {
    while (date.day > 28 && !is_valid_date(date)) --date.day;
    return date;
}

// The controls of a date dialog, which keep one another in step: the month
// picker and the year field follow the calendar's month, and choosing a
// month or a year moves the calendar's selection there.
struct DateControls {
    CalendarView* calendar = nullptr;
    ComboBox* month = nullptr;
    SpinBox* year = nullptr;
    // Set while the controls are being brought into step, so that doing so
    // is not read as the reader choosing something.
    bool syncing = false;

    void follow_calendar() {
        syncing = true;
        const DateValue shown = calendar->month();
        month->set_selected_index(static_cast<std::size_t>(shown.month - 1));
        year->set_value(shown.year);
        syncing = false;
    }

    void choose(int chosen_year, int chosen_month) {
        const DateValue selected = calendar->selected();
        // A day the range or the predicate refuses leaves the selection where
        // it was, and the calendar with it; the controls follow either way.
        calendar->set_selected(within_month(DateValue{chosen_year, chosen_month, selected.day}));
        follow_calendar();
    }
};

// A dialog window in the dialog roles, held one cell off its frame like
// every standard dialog.
std::unique_ptr<Window> dialog_window(std::string title, const ui::StandardRoles& roles) {
    auto window = std::make_unique<Window>(std::move(title));
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                              roles.dialog_background);
    window->set_resizable(false);
    window->set_content_margin(1, 1);
    return window;
}

// OK, the default, then Cancel, in a row as a message box lays its buttons.
struct ButtonPair {
    std::unique_ptr<Row> row;
    Button* ok = nullptr;
    Button* cancel = nullptr;
};
ButtonPair button_pair(const StandardStrings& strings) {
    ButtonPair pair;
    pair.row = std::make_unique<Row>();
    pair.row->set_spacing(2);
    auto ok = std::make_unique<Button>(strings.ok);
    ok->set_default(true);
    pair.ok = static_cast<Button*>(pair.row->add_item(std::move(ok), LayoutSpec{SizePolicy::Fixed, 1}));
    pair.cancel = static_cast<Button*>(
        pair.row->add_item(std::make_unique<Button>(strings.cancel), LayoutSpec{SizePolicy::Fixed, 1}));
    return pair;
}

// Hands a built dialog to the desktop as a modal presentation: the result
// recorded while it is up, or `fallback` when none was, completes the
// presentation once the window has detached.
template <class Result>
DialogPresentation<Result> present(std::unique_ptr<Window> window, ui::View* initial_focus,
                                   std::shared_ptr<typename detail::DialogPresentationAccess<Result>::State> state,
                                   DialogPresentation<Result> presentation, Result fallback,
                                   ui::Application& app, Desktop& desktop) {
    using Access = detail::DialogPresentationAccess<Result>;
    Window* const window_ptr = window.get();
    const std::weak_ptr<void> liveness = window_ptr->lifetime_token();
    window->on_closed = [&app, window_ptr, liveness] {
        if (!liveness.expired()) schedule_self_detach(*window_ptr, app);
    };
    auto previous_on_detached = std::move(window->on_detached);
    window->on_detached = [previous = std::move(previous_on_detached), state, fallback]() {
        if (previous) previous();
        Access::finish(state, fallback);
    };
    desktop.present_modal(WindowHandle{std::move(window), initial_focus}, app);
    return presentation;
}

}  // namespace

DateDialogPresentation present_modal_date_dialog(ui::Application& app, Desktop& desktop, const ui::StandardRoles& roles,
                                                 DateDialogOptions options, const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<DateDialogResult>;
    auto parts = Access::make();
    auto window = dialog_window(strings.select_date_title, roles);
    Window* const window_ptr = window.get();
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();
    auto controls = std::make_shared<DateControls>();

    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    auto control_row = std::make_unique<Row>();
    control_row->set_spacing(2);
    auto month = std::make_unique<ComboBox>(ComboBoxMode::PickOnly);
    month->set_items(options.labels.month_names.size() == 12 ? options.labels.month_names
                                                               : english_date_time_labels().month_names);
    controls->month = static_cast<ComboBox*>(control_row->add_item(std::move(month), LayoutSpec{SizePolicy::Fixed, 1}));
    auto year = std::make_unique<SpinBox>();
    year->set_range(kFirstCalendarYear, kLastCalendarYear);
    year->set_editable(true);
    year->set_preferred_size(Size{8, 1});  // "< 2026 >": the four digits a year is written in, and its arrows
    controls->year = static_cast<SpinBox*>(control_row->add_item(std::move(year), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(control_row), LayoutSpec{SizePolicy::Fixed, 1, ui::Alignment::Start});

    // The calendar and the reason under it are one block: the reason belongs
    // to the date being chosen, not to the buttons below.
    auto grid = std::make_unique<Column>();
    auto calendar = std::make_unique<CalendarView>();
    calendar->set_show_title(false);  // the controls above it say which month
    calendar->set_labels(options.labels.month_names, options.labels.weekday_names);
    calendar->set_first_weekday(options.first_weekday);
    calendar->set_show_iso_week_numbers(options.show_iso_week_numbers);
    calendar->set_range(options.minimum, options.maximum);
    calendar->set_disabled_predicate(std::move(options.disabled));
    calendar->set_today(options.today);
    DateValue initial = options.initial;
    initial.year = std::clamp(initial.year, kFirstCalendarYear, kLastCalendarYear);
    initial.month = std::clamp(initial.month, 1, 12);
    initial.day = std::clamp(initial.day, 1, 31);
    calendar->set_selected(within_month(initial));
    controls->calendar =
        static_cast<CalendarView*>(grid->add_item(std::move(calendar), LayoutSpec{SizePolicy::Fixed, 1, ui::Alignment::Start}));
    grid->add_item(std::make_unique<RefusalPanel>(*controls->year, roles.message_error_text),
                   LayoutSpec{SizePolicy::Fixed, 1});
    column->add_item(std::move(grid), LayoutSpec{SizePolicy::Fixed, 1});

    ButtonPair buttons = button_pair(strings);
    Button* const ok = buttons.ok;
    Button* const cancel = buttons.cancel;
    column->add_item(std::move(buttons.row), LayoutSpec{SizePolicy::Fixed, 1, ui::Alignment::Start});
    window->set_content(std::move(column));

    controls->follow_calendar();
    const std::weak_ptr<DateControls> weak_controls = controls;
    controls->calendar->on_select = [weak_controls](DateValue) {
        if (const auto held = weak_controls.lock(); held && !held->syncing) held->follow_calendar();
    };
    controls->month->on_text_changed = [weak_controls](const std::string&) {
        const auto held = weak_controls.lock();
        if (!held || held->syncing || !held->month->selected_index()) return;
        held->choose(held->calendar->month().year, static_cast<int>(*held->month->selected_index()) + 1);
    };
    controls->year->on_change = [weak_controls](int chosen) {
        const auto held = weak_controls.lock();
        if (!held || held->syncing) return;
        held->choose(chosen, held->calendar->month().month);
    };

    // The window keeps the controls' shared state for as long as it lives;
    // every closure above only observes it.
    window->accept_request = [&app, controls, state = parts.state, window_ptr, window_liveness] {
        // A year still being typed is committed first, as leaving the field
        // would; one that is refused vetoes the accept, as a failing field
        // does in any dialog: it takes the focus and its reason stands.
        SpinBox& year_field = *controls->year;
        if (!year_field.commit_entry() || !year_field.valid()) {
            app.set_focus(&year_field);
            return;
        }
        Access::record(state, DateDialogResult{true, controls->calendar->selected()});
        if (!window_liveness.expired()) window_ptr->close();
    };
    window->cancel_request = [window_ptr, window_liveness] {
        if (!window_liveness.expired()) window_ptr->close();
    };
    ok->on_press = [window_ptr] {
        if (window_ptr->accept_request) window_ptr->accept_request();
    };
    cancel->on_press = [window_ptr] {
        if (window_ptr->cancel_request) window_ptr->cancel_request();
    };
    CalendarView* const initial_focus = controls->calendar;
    return present<DateDialogResult>(std::move(window), initial_focus, parts.state, std::move(parts.presentation),
                                     DateDialogResult{}, app, desktop);
}

TimeDialogPresentation present_modal_time_dialog(ui::Application& app, Desktop& desktop, const ui::StandardRoles& roles,
                                                 TimeDialogOptions options, const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<TimeDialogResult>;
    auto parts = Access::make();
    auto window = dialog_window(strings.select_time_title, roles);
    Window* const window_ptr = window.get();
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();

    auto column = std::make_unique<Column>();
    column->set_spacing(1);
    auto picker = std::make_unique<TimePicker>();
    picker->set_show_seconds(options.show_seconds);
    picker->set_24_hour(options.hour_format == HourFormat::TwentyFour);
    picker->set_meridiem_labels(options.labels.am, options.labels.pm);
    picker->set_value(options.initial);
    TimePicker* const time = static_cast<TimePicker*>(
        column->add_item(std::move(picker), LayoutSpec{SizePolicy::Fixed, 1, ui::Alignment::Start}));
    ButtonPair buttons = button_pair(strings);
    Button* const ok = buttons.ok;
    Button* const cancel = buttons.cancel;
    column->add_item(std::move(buttons.row), LayoutSpec{SizePolicy::Fixed, 1, ui::Alignment::Start});
    window->set_content(std::move(column));

    const bool seconds = options.show_seconds;
    window->accept_request = [time, seconds, state = parts.state, window_ptr, window_liveness] {
        TimeValue value = time->value();
        if (!seconds) value.second = 0;  // a field the reader was not shown sets nothing
        Access::record(state, TimeDialogResult{true, value});
        if (!window_liveness.expired()) window_ptr->close();
    };
    window->cancel_request = [window_ptr, window_liveness] {
        if (!window_liveness.expired()) window_ptr->close();
    };
    ok->on_press = [window_ptr] {
        if (window_ptr->accept_request) window_ptr->accept_request();
    };
    cancel->on_press = [window_ptr] {
        if (window_ptr->cancel_request) window_ptr->cancel_request();
    };
    return present<TimeDialogResult>(std::move(window), time, parts.state, std::move(parts.presentation),
                                     TimeDialogResult{}, app, desktop);
}

}  // namespace ckv::widgets
