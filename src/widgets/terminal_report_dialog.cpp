// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/terminal_report_dialog.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/text_view.hpp"

namespace ckv::widgets {

namespace {

using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

// The report's columns: values start where the longest name ends, sources
// where the longest value usually does, so the evidence reads as a table
// without being one.
constexpr std::size_t kValueColumn = 24;
constexpr std::size_t kSourceColumn = 56;

std::string padded(std::string line, std::size_t column) {
    if (line.size() < column) line.append(column - line.size(), ' ');
    return line;
}

// Mouse diagnostics head the report: click a few times, then reopen the
// dialog from the keyboard, and whether the count moved says which side
// of the dispatch seam lost the click.
std::string mouse_diagnostics(const ui::Application& app,
                              const TerminalReportDialogOptions& options) {
    std::string text = padded("Mouse events seen", kValueColumn);
    text += std::to_string(app.mouse_events_dispatched());
    if (const auto& last = app.last_mouse_event()) {
        text += "  last cell=(" + std::to_string(last->cell.x) + "," +
                std::to_string(last->cell.y) + ")";
        if (last->pixel)
            text += " pixel=(" + std::to_string(last->pixel->x) + "," +
                    std::to_string(last->pixel->y) + ")";
    } else {
        text += "  (no mouse event has reached the application)";
    }
    text.push_back('\n');
    if (options.mouse_reports_decoded) {
        text += padded("Mouse reports decoded", kValueColumn);
        text += std::to_string(options.mouse_reports_decoded());
        text += "  (recognized in the terminal byte stream)";
        text.push_back('\n');
    }
    return text;
}

Rect centered_in(Rect available, Size preferred) {
    const int width = std::min(std::max(0, preferred.width), available.width);
    const int height = std::min(std::max(0, preferred.height), available.height);
    return {available.x + std::max(0, (available.width - width) / 2),
            available.y + std::max(0, (available.height - height) / 2), width, height};
}

}  // namespace

WindowHandle make_terminal_report_dialog(Desktop& desktop, const ui::StandardRoles& roles,
                                         ui::Application& app, ui::View* restore_focus_to,
                                         TerminalReportDialogOptions options,
                                         const StandardStrings& strings) {
    // The evidence is read through the Application, which fronts the
    // terminal for the widgets layer; both forms are snapshots of the same
    // moment, so what Copy exports is what the dialog shows.
    const auto entries = app.terminal_capability_report();
    const std::string report_text = app.terminal_capability_report_text();
    const std::string diagnostics = mouse_diagnostics(app, options);
    std::string table;
    for (const auto& entry : entries) {
        std::string line = padded(entry.name, kValueColumn);
        line += entry.value;
        line = padded(std::move(line), kSourceColumn);
        line += entry.source;
        table += line;
        table.push_back('\n');
    }
    const std::string body = diagnostics + table;
    const int body_rows =
        1 + (options.mouse_reports_decoded ? 1 : 0) + static_cast<int>(entries.size());

    auto window = std::make_unique<Window>(strings.terminal_report_title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                              roles.dialog_background);
    window->set_resizable(true);
    window->set_content_margin(1, 1);
    Window* const window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();

    auto content = std::make_unique<Column>();
    content->set_spacing(1);
    auto viewport = std::make_unique<ScrollViewport>();
    viewport->set_focus_policy(ui::FocusPolicy::TabStop);
    viewport->set_scrollbars_always_visible(true);
    auto document = std::make_unique<TextView>();
    document->set_text(body);
    document->set_preferred_size({86, body_rows});
    document->set_vertical_scrollbar_visible(false);
    // The viewport owns both visible tracks (set_scrollbars_always_visible
    // above); the vertical half of this document's own pair was hidden, but
    // its horizontal one defaults to Auto and every report row is wider than
    // the 86-column preferred size, so it was showing too — a second
    // horizontal bar stacked under the viewport's own.
    document->set_horizontal_scrollbar_policy(ScrollbarPolicy::Hidden);
    document->set_role_override(roles.help_text);
    viewport->set_content(std::move(document));
    ScrollViewport* const viewport_ptr = viewport.get();
    content->add_item(std::move(viewport), LayoutSpec{SizePolicy::Expanding, 1});

    auto buttons = std::make_unique<Row>();
    buttons->set_spacing(2);
    auto copy = std::make_unique<Button>(strings.copy_to_clipboard);
    Button* const copy_ptr = copy.get();
    buttons->add_item(std::move(copy), LayoutSpec{SizePolicy::Fixed, 1});
    auto close = std::make_unique<Button>(strings.close);
    close->set_default(true);
    Button* const close_ptr = close.get();
    buttons->add_item(std::move(close), LayoutSpec{SizePolicy::Fixed, 1});
    content->add_item(std::move(buttons),
                      LayoutSpec{SizePolicy::Fixed, 1, ui::Alignment::Center});
    window->set_content(std::move(content));
    window->set_bounds(centered_in(desktop.content_area(), {90, body_rows + 7}));

    // The clipboard export carries the plain-text report rather than the
    // padded table: it is meant to be pasted into a bug report. The
    // application diagnostics travel with it — a report without the mouse
    // counts answers only half the questions it gets asked.
    copy_ptr->on_press = [&app, report_text, diagnostics] {
        app.set_clipboard_text(report_text + "\nApplication diagnostics\n" + diagnostics);
    };
    close_ptr->on_press = [window_ptr] { window_ptr->close(); };
    window_ptr->accept_request = [close_ptr] {
        if (close_ptr->on_press) close_ptr->on_press();
    };
    window_ptr->cancel_request = [window_ptr] { window_ptr->close(); };
    window_ptr->on_closed = [&app, focus_restore, window_ptr, window_liveness] {
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        const std::weak_ptr<void> held_window_liveness = window_liveness;
        Window* const held_window = window_ptr;
        held_focus_restore.restore(app);
        if (!held_window_liveness.expired()) schedule_self_detach(*held_window, app);
    };
    return WindowHandle{std::move(window), viewport_ptr};
}

TerminalReportDialogPresentation present_terminal_report_dialog(Desktop& desktop,
                                                                ui::Application& app,
                                                                const ui::StandardRoles& roles,
                                                                TerminalReportDialogOptions options,
                                                                const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<TerminalReportDialogResult>;
    auto parts = Access::make();
    auto handle =
        make_terminal_report_dialog(desktop, roles, app, app.focused(), std::move(options), strings);
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached),
                                  state = parts.state]() {
        if (previous) previous();
        Access::finish(state, TerminalReportDialogResult::Closed);
    };
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}

}  // namespace ckv::widgets
