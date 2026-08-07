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
// takes focus, per ARCHITECTURE.md §5) and cancel_request (Esc closes,
// bypassing validation by definition), plus focus restoration to
// whatever view had focus before the dialog opened.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

struct FieldDescriptor {
    std::string label;         // may carry a '&' mnemonic; empty = unlabeled field
    std::string initial_text;

    // Optional; empty means "always valid". Runs at accept time only
    // (ARCHITECTURE.md §5 dialog-accept veto) — never on every
    // keystroke, since a field mid-edit is expected to be transiently
    // invalid.
    std::function<bool(const std::string&)> validate;
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
};

struct MaterializedDialog {
    std::unique_ptr<ui::View> root;
    std::vector<Label*> labels;     // parallel to descriptor.fields; nullptr where a field had no label
    std::vector<InputLine*> inputs;  // parallel to descriptor.fields
    std::vector<Button*> buttons;    // parallel to descriptor.buttons
    ui::View* initial_focus = nullptr;
    Button* default_button = nullptr;
};

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
//    accept_request detects that and does not close it a second time.
//    NOT safe for on_press to destroy the Window object directly
//    (rather than closing it) — only close() is tracked, not arbitrary
//    destruction; doing so remains a use-after-free.
//  - cancel_request: closes the window directly, bypassing validation.
//  - on_closed (composed with any handler already set): restores
//    focus to `restore_focus_to` if it is still focusable.
// The installed closures capture `app`, `window`, and `descriptor` by
// reference (and `restore_focus_to` by value) — all three must outlive
// `window` itself; a typical caller keeps `descriptor` alive for the
// window's whole lifetime (e.g. as a member) rather than a local.
void wire_dialog_window(Window& window, MaterializedDialog dialog, const DialogDescriptor& descriptor,
                         ui::Application& app, ui::View* restore_focus_to);

}  // namespace ckv::widgets
