// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Declarative dialog descriptors, validated materialization
// (ARCHITECTURE.md §5 "Layout and dialog construction"): an
// application describes a dialog's content as plain descriptor
// structs, and materialize_dialog() turns that into a parent-owned
// view tree — binding label mnemonics to their buddy input, wiring the
// default button, and computing initial focus (the first focusable
// control by construction, never a caller afterthought). Invalid
// descriptions (nothing to materialize, more than one default button)
// are rejected at materialization via CKV_ASSERT rather than producing
// a dialog with undefined behavior.
//
// wire_dialog_window() below adds window chrome integration: it hosts
// a MaterializedDialog inside a Window's content, wiring Window's
// accept_request (default button + accept-time validation veto — a
// failing validator blocks the accept, marks its field invalid, and
// takes focus, per ARCHITECTURE.md §5), cancel_request (Esc closes,
// bypassing validation by definition), resizable() per the descriptor's
// own `resizable` field (M10/WP-21 — false unless opted in), plus focus
// restoration to whatever view had focus before the dialog opened.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class Desktop;

struct FieldDescriptor {
    std::string label;         // may carry a '&' mnemonic; empty = unlabeled field
    std::string initial_text;

    // Optional; empty means "always valid". Runs at accept time only
    // (ARCHITECTURE.md §5 dialog-accept veto) — never on every
    // keystroke, since a field mid-edit is expected to be transiently
    // invalid.
    std::function<bool(const std::string&)> validate;

    // Secret fields retain their real value for validation/completion while
    // displaying only `password_echo_char`. This belongs on the descriptor,
    // rather than requiring a caller to reconstruct a materialized dialog,
    // so declarative input boxes can safely request password entry too.
    bool password_echo = false;
    char password_echo_char = '*';
};

struct ButtonDescriptor {
    std::string label;
    bool is_default = false;
    std::function<void()> on_press;
};

struct DialogDescriptor {
    std::string title;  // not yet rendered anywhere (no window chrome until M5) — carried for that wiring
    std::vector<FieldDescriptor> fields;
    std::vector<ButtonDescriptor> buttons;

    // Whether wire_dialog_window() lets the user resize the hosting
    // window (M10/WP-21; plans/widgets.md's "resizable dialogs with
    // layout" beyond-baseline row). Defaults to false: most dialogs
    // are a fixed-size prompt, and a resize grip on one invites a user
    // to make it a size its content was never designed for. Safe to
    // opt in — materialize_dialog() already builds every dialog's
    // content from ui::Column/Row (SizePolicy::Expanding on each
    // InputLine), so it already reflows correctly if the window
    // resizes; this flag only controls whether the USER is offered
    // that resize at all, not whether the layout could support it.
    bool resizable = false;

    // Optional measured lower bound for the framed hosting window. This is
    // presentation geometry, not field padding: a compact form can retain
    // deliberate whitespace while every field still reflows in that space.
    Size minimum_window_size{};
    // Placement of the action row within a deliberately wider form.
    ui::Alignment button_alignment = ui::Alignment::Start;
    // When the minimum height leaves space below the fields, place actions at
    // the bottom rather than treating that intentional space as a trailing
    // blank area. Defaults preserve existing compact descriptor dialogs.
    bool anchor_buttons_to_bottom = false;
};

struct MaterializedDialog {
    std::unique_ptr<ui::View> root;
    std::vector<Label*> labels;     // parallel to descriptor.fields; nullptr where a field had no label
    std::vector<InputLine*> inputs;  // parallel to descriptor.fields
    std::vector<Button*> buttons;    // parallel to descriptor.buttons
    ui::View* initial_focus = nullptr;
    Button* default_button = nullptr;
};

// The typed outcome of a descriptor dialog. An accepted result contains one
// value for each descriptor field, in descriptor order. Close, Esc, external
// detach, and a host quit produce the default cancellation result
// `{false, {}}`; no partial field values escape a cancelled dialog.
struct DialogResult {
    bool accepted = false;
    std::vector<std::string> values;

    friend bool operator==(const DialogResult&, const DialogResult&) = default;
};

using DescriptorDialogPresentation = DialogPresentation<DialogResult>;

MaterializedDialog materialize_dialog(const DialogDescriptor& descriptor);

// Runs every field's validator against its CURRENT text, marking each
// InputLine valid/invalid accordingly. On the first invalid field,
// focuses it and returns false (the veto); if every field is valid,
// returns true. `descriptor` must be the same one `dialog` was
// materialized from (parallel arrays, by index).
bool validate_dialog(MaterializedDialog& dialog, const DialogDescriptor& descriptor, ui::Application& app);

// Moves dialog.root into `window`'s content, then wires:
//  - accept_request: validate_dialog(); on success, runs the default
//    button's on_press (if any) and closes the window; on veto, does
//    nothing further (the invalid field already took focus). Safe for
//    the descriptor's own default-button on_press to call window.close()
//    itself (a natural "the OK button closes the dialog" pattern) —
//    accept_request detects that and does not close it a second time. It is
//    equally safe for that callback to detach and destroy the Window: the
//    remaining accept state is independently owned and lifetime-checked.
//  - cancel_request: closes the window directly, bypassing validation.
//  - on_closed (composed with any handler already set): restores
//    focus to `restore_focus_to` if that exact view is still alive and
//    focusable.
// Also calls window.set_resizable(descriptor.resizable) — false unless
// the descriptor opts in (see DialogDescriptor::resizable's own doc
// comment).
// The installed closures capture `app`, `window`, and `descriptor` by
// reference (and `restore_focus_to` by value) — all three must outlive
// `window` itself; a typical caller keeps `descriptor` alive for the
// window's whole lifetime (e.g. as a member) rather than a local.
void wire_dialog_window(Window& window, MaterializedDialog dialog, const DialogDescriptor& descriptor,
                         ui::Application& app, ui::View* restore_focus_to);

// Presents a descriptor-built dialog modally without starting a nested loop.
// The descriptor is taken by value and retained by the presentation until its
// window detaches, so field validators and button handlers never borrow a
// caller-local descriptor. Completion fires exactly once after detachment.
[[nodiscard]] DescriptorDialogPresentation present_dialog(DialogDescriptor descriptor, ui::Application& app,
                                                           Desktop& desktop, const ui::StandardRoles& roles);

// Outer-loop-only blocking counterpart to present_dialog. It delegates to
// Desktop::exec_modal, so calling it from a handler, posted callback, or timer
// is rejected before a nested pump starts. A host quit returns `{false, {}}`.
DialogResult exec_dialog(DialogDescriptor descriptor, ui::Application& app,
                         Desktop& desktop, const ui::StandardRoles& roles);

}  // namespace ckv::widgets
