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
// Scope note: severity-accented styling per MessageBoxKind ("theme-
// role styled severity accents" in plans/widgets.md) is explicitly
// beyond-baseline — this module stores `kind` on the descriptor for a
// future theming pass but renders every kind identically today via
// StandardRoles::static_text.
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
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
// (dismissing an alert), matching common convention. `on_result` fires
// exactly once, whichever button (or Esc) closes the box. On close,
// focus restores to `restore_focus_to` if it is still focusable.
//
// Deliberately does NOT call Application::set_focus itself (unlike
// materialize_dialog, which reports initial_focus the same way): doing
// so before the caller attaches `window` under the app's tree would
// steal focus to a still-detached view, silently swallowing input
// meant for whatever UI is currently on screen. Desktop::show_dialog
// (M8 WP-5) is the sanctioned way to attach the returned handle and
// focus its initial_focus (the default button) in one call.
WindowHandle make_message_box(const MessageBoxDescriptor& descriptor, const ui::Theme& theme,
                               const ui::StandardRoles& roles, ui::Application& app,
                               ui::View* restore_focus_to, std::function<void(MessageBoxResult)> on_result);

}  // namespace ckv::widgets
