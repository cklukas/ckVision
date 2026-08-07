// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Window-list dialog: select/activate/close from a list of a Desktop's
// open windows (plans/widgets.md M6c baseline "Window list").
//
// Scope note: type-ahead filtering is beyond-baseline (not built).
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
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

// Lists `desktop`'s windows() (stable insertion/cycling order, the
// same order Desktop::select_by_number uses) with each window's
// current title. Enter or double-click activates the highlighted
// window (via desktop.activate()) and closes the dialog; Esc closes
// without activating anything. `desktop` must outlive the returned
// Window (the installed closures capture it by reference).
// Desktop::show_dialog (M8 WP-5) attaches the returned handle and
// focuses its initial_focus in one call.
WindowHandle make_window_list_dialog(Desktop& desktop, const ui::Theme& theme, const ui::StandardRoles& roles,
                                      ui::Application& app, ui::View* restore_focus_to);

}  // namespace ckv::widgets
