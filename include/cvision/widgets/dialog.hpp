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
// Scope note: this produces CONTENT only — a Column of field rows plus
// a button row. Window chrome (title bar, close control, shadow) and
// the close/accept protocol (validation veto, Esc-cancel) are M5
// concerns (Window doesn't exist yet); wiring a MaterializedDialog's
// root into a Window's content view is a follow-on, not a redesign,
// once Window lands.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"

namespace ckv::widgets {

struct FieldDescriptor {
    std::string label;         // may carry a '&' mnemonic; empty = unlabeled field
    std::string initial_text;
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

MaterializedDialog materialize_dialog(const DialogDescriptor& descriptor, const ui::Theme& theme,
                                       const ui::StandardRoles& roles);

}  // namespace ckv::widgets
