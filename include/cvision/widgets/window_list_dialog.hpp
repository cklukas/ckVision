// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Window-list dialog: select, activate or close any of a Desktop's open
// windows from a list that a typed filter narrows (the widget catalog "Window
// list").
//
// Built directly from Window + SearchBox + ListView + Button, not through
// materialize_dialog/wire_dialog_window — those are shaped for
// label+input fields with a single accept path, and this dialog's
// content is a selection list with per-row actions, closer to
// message_box.cpp's own hand-built composition than a form.
#pragma once

#include <memory>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/standard_strings.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

// The dialog's only outcome. Activating a window is its effect, not its
// answer, so an activation and a plain close both complete as Closed.
enum class WindowListDialogResult { Closed };
// The handle present_modal_window_list_dialog returns; see DialogPresentation.
using WindowListDialogPresentation = DialogPresentation<WindowListDialogResult>;

// Lists `desktop`'s windows() (stable insertion/cycling order, the
// same order Desktop::select_by_number uses) with each window's
// current title, leaving out the dialog itself. The list follows the
// desktop while the dialog is up: windows opening, closing or being
// renamed underneath it are reflected at once.
//
// Typing over the list filters it: each printable character is added to
// the filter line above the list, which then shows only the windows whose
// titles contain the filter (ASCII letters compared without case), and
// Backspace takes a character back. Escape clears a non-empty filter and
// otherwise closes the dialog without activating anything.
//
// Enter, a double click, the default "Switch To" button, or Enter anywhere
// else in the dialog activates the window under the cursor (via
// desktop.activate()) and closes the dialog. Delete or the "Close Window"
// button asks the window under the cursor to close through its own vetoable
// Window::close() — its close_request may refuse or ask first — and the
// dialog stays up, dropping the window from the list once it has left the
// desktop. Those two buttons are disabled while nothing is listed. "Cancel"
// closes the dialog. `desktop` must outlive the returned Window (the
// installed closures capture it by reference).
// Desktop::present_modeless attaches the returned handle and focuses
// its initial_focus in one call; modal presentation is explicit through
// Desktop::present_modal. The returned standard dialog window is
// non-resizable by default.
WindowHandle make_window_list_dialog(Desktop& desktop, const ui::StandardRoles& roles, ui::Application& app,
                                      ui::View* restore_focus_to,
                                      const StandardStrings& strings = english_standard_strings());

// Presents the list modally without a nested loop. Completion occurs only
// after detachment; activation, close, external detach, and quit all resolve
// to Closed because this dialog returns no separate selection value.
[[nodiscard]] WindowListDialogPresentation present_modal_window_list_dialog(Desktop& desktop, ui::Application& app,
                                                                       const ui::StandardRoles& roles,
                                                                       const StandardStrings& strings = english_standard_strings());

}  // namespace ckv::widgets
