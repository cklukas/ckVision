// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The standard date and time dialogs (plans/widgets.md "Date/time picker
// dialogs"): a date chosen on a CalendarView, a time set on a TimePicker,
// each presented modally with a typed, non-blocking completion (D-038).
//
// Built directly from Window and the common components rather than through
// materialize_dialog: a calendar is not a labelled field. Every word either
// dialog shows comes from `app.strings()` (`UiStrings`), defaulting to
// English. Neither dialog reads a clock or a locale: the date or time it opens
// on, and today, are the caller's.
#pragma once

#include "cvision/core/export.hpp"

#include <functional>
#include <optional>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/date_time.hpp"
#include "cvision/widgets/spin_box.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/ui/ui_strings.hpp"

namespace ckv::widgets {

// What the date dialog opens on and offers.
struct CVISION_TYPE_API DateDialogOptions {
    // The day selected when the dialog opens, clamped into its month and the
    // drawable years; its month is the one shown.
    DateValue initial{2026, 1, 1};
    // The day marked as today, or none: a caller's value, never a clock's.
    std::optional<DateValue> today{};
    // The earliest and latest days that may be chosen, and a predicate
    // refusing others, exactly as CalendarView::set_range and
    // set_disabled_predicate take them.
    std::optional<DateValue> minimum{};
    std::optional<DateValue> maximum{};
    std::function<bool(DateValue)> disabled{};
    // The calendar's week layout: the day it starts on, and the ISO week
    // column.
    Weekday first_weekday = Weekday::Monday;
    bool show_iso_week_numbers = false;
};

// How the date dialog ended: accepted with the day selected in it, or not
// accepted (Cancel, Escape, the close control, an external detach or a
// quit), when `date` is the default and means nothing.
struct CVISION_TYPE_API DateDialogResult {
    // Whether the reader accepted, and the day they chose.
    bool accepted = false;
    DateValue date{};

    // Memberwise equality, so a test can compare a whole answer at once.
    friend bool operator==(const DateDialogResult&, const DateDialogResult&) = default;
};

// The handle present_modal_date_dialog returns; see DialogPresentation.
using DateDialogPresentation = DialogPresentation<DateDialogResult>;

// Presents a modal date dialog on `desktop` without starting a nested loop.
// It shows a month picker and an editable year SpinBox over a CalendarView
// (whose own title is off, since the controls above it name the month), two
// rows for the reason a typed year is refused, and OK and Cancel. The
// calendar takes the focus. Choosing a month or a year moves the selection
// to the same day there, clamped into a shorter month; a day the range or
// the predicate refuses leaves the selection where it was. Enter or OK
// accepts with the selected day, unless the year field holds a refused entry:
// then the dialog stays, the field takes the focus, and its reason stands in
// those rows, word-wrapped in the error-text role, as a dialog veto's message
// stands in its description panel. Escape or Cancel dismisses.
// The window's title is `strings.select_date_title`; the buttons are
// `strings.ok` and `strings.cancel`.
[[nodiscard]] CVISION_API DateDialogPresentation present_modal_date_dialog(
    ui::Application& app, Desktop& desktop, DateDialogOptions options = {});

// What the time dialog opens on and how it shows the time.
struct CVISION_TYPE_API TimeDialogOptions {
    // The time shown when the dialog opens, each field clamped into range.
    TimeValue initial{};
    // Whether the seconds field is shown, and which clock face.
    bool show_seconds = true;
    HourFormat hour_format = HourFormat::TwentyFour;
};

// How the time dialog ended: accepted with the time set in it, or not, when
// `time` is the default and means nothing.
struct CVISION_TYPE_API TimeDialogResult {
    // Whether the reader accepted, and the time they set.
    bool accepted = false;
    TimeValue time{};

    // Memberwise equality.
    friend bool operator==(const TimeDialogResult&, const TimeDialogResult&) = default;
};

// The handle present_modal_time_dialog returns; see DialogPresentation.
using TimeDialogPresentation = DialogPresentation<TimeDialogResult>;

// Presents a modal time dialog: a TimePicker, which takes the focus, over OK
// and Cancel. Enter or OK accepts with the time set; Escape or Cancel
// dismisses. Without seconds shown, the accepted time's seconds are zero.
// The window's title is `strings.select_time_title`.
[[nodiscard]] CVISION_API TimeDialogPresentation present_modal_time_dialog(
    ui::Application& app, Desktop& desktop, TimeDialogOptions options = {});

}  // namespace ckv::widgets
