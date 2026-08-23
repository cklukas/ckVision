// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Message boxes: info/warning/error/confirm with standard button sets
// (the widget catalog M5 baseline). Built directly from Window + Button
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
#include <memory>
#include <string>
#include <utility>

#include "cvision/core/image.hpp"
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
    // Optional raster content for presentations such as About. The caller
    // owns the immutable pixels; the box shows them through an ImageView and
    // therefore keeps its raster and textual fallback behaviour.
    std::shared_ptr<const Image> graphic;
    // The most room the artwork may take, in cells. A maximum rather than a
    // size: how many cells a picture needs to look like itself depends on
    // the terminal's cell, which the caller would have to ask for and divide
    // by to work out. Stating the ceiling and letting the box fit within it
    // keeps that arithmetic in one place.
    Size graphic_max_cells{};
    // A lower bound for the content area. This lets an information-rich
    // presentation retain deliberate whitespace around compact content
    // without encoding that whitespace into its message text or artwork.
    int minimum_content_width = 0;
    // Cross-axis placement inside a wider message box. Plain alerts keep
    // their established left-aligned composition; callers with a visual
    // identity panel may opt into a centered composition explicitly.
    ui::Alignment graphic_alignment = ui::Alignment::Start;
    ui::Alignment message_alignment = ui::Alignment::Start;
    ui::Alignment button_alignment = ui::Alignment::Start;
    // Leading message lines drawn bold — for an identity panel whose
    // first line is the product name and whose remaining lines are the
    // detail beneath it. Zero leaves the message uniformly styled.
    int emphasized_leading_lines = 0;

    MessageBoxDescriptor() = default;
    MessageBoxDescriptor(MessageBoxKind message_kind, std::string message_title,
                         std::string message_text, MessageBoxButtons message_buttons)
        : kind(message_kind), title(std::move(message_title)), message(std::move(message_text)),
          buttons(message_buttons) {}
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

// Answers the help command (F1) with a box naming the application and
// saying what it is.
//
// Every application should answer F1 with something. Silence is the one
// response a reader cannot interpret: it is indistinguishable from a key
// that did not arrive, a window that did not have focus, and a feature that
// was never built. An application with a real help system installs that
// instead; this is for the rest, and for the moment before the real one
// exists.
//
// Installs a handler for the standard help command, replacing any already
// there.
void install_about_help(ui::Application& app, Desktop& desktop, const ui::StandardRoles& roles,
                        std::string title, std::string body);

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
