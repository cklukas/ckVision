// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Message boxes: info/warning/error/confirm with standard button sets
// (plans/widgets.md M5 baseline). Built directly from Window + Button
// + StaticText rather than through materialize_dialog/wire_dialog_window
// — those are shaped for LABEL+INPUT fields with a single default-
// button accept path, whereas every message-box button independently
// closes the box with its own result, and reusing the dialog-accept
// wiring here would double-fire the default button's close (it would
// close itself directly on click AND again via accept_request).
//
// MessageBoxKind selects a standard message text role, so schemes can
// distinguish info/warning/error/confirm boxes without changing the
// dialog factory API.
#pragma once

#include <functional>
#include <string>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/standard_strings.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

enum class MessageBoxKind { Info, Warning, Error, Confirm };
enum class MessageBoxButtons { Ok, OkCancel, YesNo, YesNoCancel };
enum class MessageBoxResult { Ok, Cancel, Yes, No };

struct MessageBoxDescriptor {
    MessageBoxKind kind = MessageBoxKind::Info;
    std::string title;
    std::string message;
    MessageBoxButtons buttons = MessageBoxButtons::Ok;
};

// Builds (but does not attach to any Desktop) a Window presenting the
// message and the requested standard button set. The FIRST button in
// each set (Ok for Ok/OkCancel, Yes for YesNo/YesNoCancel) is the
// default — Enter anywhere in the box activates it. Esc activates
// Cancel/No where present; for a bare Ok box, Esc is equivalent to Ok
// (dismissing an alert), matching common convention. The returned standard
// dialog window is non-resizable by default. `on_result` fires
// exactly once, whichever button (or Esc) closes the box. It may detach or
// destroy the box; no factory-owned work touches the Window after the callback
// returns. On close, focus restores to `restore_focus_to` only if that exact
// view is still alive and focusable.
//
// Deliberately does NOT call Application::set_focus itself (unlike
// materialize_dialog, which reports initial_focus the same way): doing
// so before the caller attaches `window` under the app's tree would
// steal focus to a still-detached view, silently swallowing input
// meant for whatever UI is currently on screen. Desktop::present_modeless
// attaches the returned handle and focuses its initial_focus (the default
// button) in one call; present_message_box is the ordinary modal path.
// Attaching through Desktop's generic View API is equally window-management-
// safe when a caller needs a more custom flow.
WindowHandle make_message_box(const MessageBoxDescriptor& descriptor, const ui::StandardRoles& roles,
                               ui::Application& app, ui::View* restore_focus_to,
                               std::function<void(MessageBoxResult)> on_result,
                               const StandardStrings& strings = english_standard_strings());

// A move-only observer for a non-blocking message-box presentation
// (D-038). Completion happens after the box has detached and its modal
// scope has ended. An explicit button result wins; close, external
// detach, and quit resolve to the button set's documented Esc result.
// set_completion_handler may be called once; if completion already
// happened, it calls the handler immediately on the owning UI thread.
using MessageBoxPresentation = DialogPresentation<MessageBoxResult>;

// Presents a message box modally without starting a nested loop. The
// returned handle is the sole result-observation path; applications
// normally retain it and register a completion handler from the command
// handler that presented the box.
[[nodiscard]] MessageBoxPresentation present_message_box(ui::Application& app, Desktop& desktop,
                                                          const ui::StandardRoles& roles,
                                                          const MessageBoxDescriptor& descriptor,
                                                          const StandardStrings& strings = english_standard_strings());

// The blocking convenience (M9/WP-15, D-021's "blocking wrapper for
// applications that own the loop"): builds and shows the box exactly
// like make_message_box + Desktop::present_modal, scopes input to it for
// the duration (Desktop::exec_modal), and returns the button's (or
// Esc's) result once it closes — no nested native loop, built entirely
// over Application::step(). Focus restores to whatever was focused
// before the call, matching make_message_box's own restore_focus_to
// contract. The non-blocking convenience is present_message_box; a caller
// needing a modeless box explicitly combines make_message_box with
// Desktop::present_modeless.
MessageBoxResult exec_message_box(ui::Application& app, Desktop& desktop,
                                   const ui::StandardRoles& roles,
                                   const MessageBoxDescriptor& descriptor,
                                   const StandardStrings& strings = english_standard_strings());

}  // namespace ckv::widgets
