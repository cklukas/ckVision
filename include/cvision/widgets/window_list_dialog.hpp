// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Window-list dialog: select/activate/close from a list of a Desktop's
// open windows (the widget catalog M6c baseline "Window list").
//
// Built directly from Window + ListView + Button, not through
// materialize_dialog/wire_dialog_window — those are shaped for
// label+input fields with a single accept path, and this dialog's
// content is a selection list with per-row Activate semantics, closer
// to message_box.cpp's own hand-built composition than a form.
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

enum class WindowListDialogResult { Closed };
using WindowListDialogPresentation = DialogPresentation<WindowListDialogResult>;

// Lists `desktop`'s windows() (stable insertion/cycling order, the
// same order Desktop::select_by_number uses) with each window's
// current title. Printable typing performs ListView's first-letter
// type-ahead, and Enter or double-click activates the highlighted
// window (via desktop.activate()) and closes the dialog; Esc closes
// without activating anything. `desktop` must outlive the returned
// Window (the installed closures capture it by reference).
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
[[nodiscard]] WindowListDialogPresentation present_window_list_dialog(Desktop& desktop, ui::Application& app,
                                                                       const ui::StandardRoles& roles,
                                                                       const StandardStrings& strings = english_standard_strings());

}  // namespace ckv::widgets
