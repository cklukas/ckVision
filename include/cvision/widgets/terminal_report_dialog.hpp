// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Terminal report dialog: capability_report()'s evidence table together
// with the application's own mouse-dispatch diagnostics, as one standard
// dialog any ckVision application can open. Terminals disagree with their
// own self-descriptions in the field, so an application needs to be able
// to show, and a user to paste, the evidence rather than argue from
// behaviour (capability_report.hpp) — and the report has to be the same
// dialog wherever it is shown, or two applications on one terminal would
// present two accounts of it. Desktop installs it behind the standard
// `terminal_report` command; the Copy button exports the plain-text form
// for a bug report.
//
// Built directly from Window + ScrollViewport/TextView + Button, like
// window_list_dialog: the content is a read-only evidence table, not a
// label+input form, so materialize_dialog's single-accept shape does not
// fit.
#pragma once

#include <cstddef>
#include <functional>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/standard_strings.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

enum class TerminalReportDialogResult { Closed };
using TerminalReportDialogPresentation = DialogPresentation<TerminalReportDialogResult>;

// What the dialog cannot observe through the Application: how many SGR
// mouse reports the terminal layer recognized in the byte stream. Shown
// beside the events dispatch actually delivered, the two counts separate
// "the terminal never sent a click" from "it arrived and landed somewhere
// unexpected", which have nothing in common as causes. The line is
// omitted when no probe is given — a headless or mirrored terminal has no
// byte stream of its own to count.
struct TerminalReportDialogOptions {
    std::function<std::size_t()> mouse_reports_decoded;
};

// Builds the report from the live terminal at the moment of the call —
// the point of the dialog is to show what is true now, including after a
// resize re-probe — and returns it unpresented so a caller with its own
// modal bookkeeping can attach it. The returned window is resizable; its
// initial_focus is the scrolling report viewport.
WindowHandle make_terminal_report_dialog(Desktop& desktop, const ui::StandardRoles& roles,
                                         ui::Application& app, ui::View* restore_focus_to,
                                         TerminalReportDialogOptions options = {},
                                         const StandardStrings& strings = english_standard_strings());

// Presents the report modally without a nested loop. Completion occurs
// only after detachment; close, external detach, and quit all resolve to
// Closed because this dialog returns no separate selection value.
[[nodiscard]] TerminalReportDialogPresentation present_terminal_report_dialog(
    Desktop& desktop, ui::Application& app, const ui::StandardRoles& roles,
    TerminalReportDialogOptions options = {},
    const StandardStrings& strings = english_standard_strings());

}  // namespace ckv::widgets
