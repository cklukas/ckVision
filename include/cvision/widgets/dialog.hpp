// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Declarative dialog descriptors, validated materialization
// (the architecture §5 "Layout and dialog construction"): an
// application describes a dialog's content as plain descriptor
// structs, and materialize_dialog() turns that into a parent-owned
// view tree — binding label mnemonics to their buddy input, wiring the
// default button, and computing initial focus (the first focusable
// control by construction, never a caller afterthought). Invalid
// descriptions (nothing to materialize, more than one default button)
// are rejected at materialization via CKV_ASSERT rather than producing
// a dialog with undefined behavior.
//
// The materialized tree always has the same two-part shape (U4-g): every
// field goes inside a ScrollViewport, and the button row is that
// viewport's SIBLING, below it. There is no second shape for a dialog
// that happens not to fit — a dialog cannot know at materialization time
// how much room it will be given, and it is given a different amount
// every time the terminal is resized. What varies is only the layout the
// pane computes from the height it actually has:
//   * room for everything — the viewport is exactly as tall as the fields
//     need, the button row sits directly under it, and the viewport's
//     ScrollbarPolicy::Auto vertical bar stays off screen. Nothing about
//     the result differs from a dialog built before any of this existed.
//   * not enough room — the button row keeps the bottom of the pane and
//     the viewport takes what is left, so the content scrolls (by keyboard,
//     by the bar, and by the wheel where the field under it does not want
//     it) and the buttons stay on screen and clickable. Buttons that
//     scroll out of reach are the defect this exists to prevent.
// Horizontal scrolling is switched off outright: a form whose left column
// has scrolled away is not a view of that form.
//
// wire_dialog_window() below adds window chrome integration: it hosts
// a MaterializedDialog inside a Window's content, wiring Window's
// accept_request (default button + accept-time validation veto — a
// failing validator blocks the accept, marks its field invalid, and
// takes focus, per the architecture §5), cancel_request (Esc closes,
// bypassing validation by definition), resizable() per the descriptor's
// own `resizable` field (M10/WP-21 — false unless opted in), plus focus
// restoration to whatever view had focus before the dialog opened.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

class Desktop;

// Which control a field materializes as. A settings form is mostly not text:
// it is answers to yes/no questions, and a descriptor that can only make
// input lines forces every such form to be hand-built, which is how two
// dialogs in one application come to disagree about their own layout.
enum class FieldKind {
    // An InputLine. The default, and what every descriptor written before
    // this enum existed still gets.
    Text,
    // A single checkbox carrying `label` as its own text. There is no
    // separate caption: "[X] Ask for a filename every time" reads as one
    // sentence, and splitting it across a label column would break it.
    Check,
    // Text the form says rather than asks: what a setting will do, what it
    // will not do, where its effect shows up. A settings dialog needs this
    // more than a data-entry form does — a checkbox can be labelled
    // truthfully and still leave its reader guessing what ticking it costs
    // them. Takes no focus, contributes an empty value, and is skipped by
    // validation.
    Note,
    // A RadioGroup over `options`, captioned by `label`. One of a few
    // alternatives, all of them visible at once — which is the difference
    // from a combo: a reader choosing between three printer modes should be
    // able to see the three without opening anything.
    Radio,
    // A ComboBox over `options`, `editable` deciding whether a value outside
    // them may be typed. For a list too long to show at once, or one whose
    // entries are only suggestions.
    Combo,
    // An InputLine that must hold a whole number, optionally within
    // `minimum`..`maximum`. The check runs at accept time like every other
    // validator — a field mid-edit is expected to be transiently invalid, and
    // a form that refuses the keystroke instead cannot be corrected from the
    // middle. A field's own `validate` still runs, after the number itself
    // has been established, so it never has to parse the text again.
    Number,
    // A typed DatePicker. Unlike a Text field with a date-looking validator,
    // this cannot publish a malformed calendar date and exposes segmented
    // keyboard/pointer editing. `initial_date` is the optional answer;
    // `date_seed` is the deterministic date used when an empty picker is
    // first adjusted. The accepted DialogResult carries both the typed date
    // and its canonical YYYY-MM-DD text.
    Date,
    // A typed TimePicker. `initial_time` is explicit and deterministic;
    // `time_show_seconds` controls both presentation and the canonical
    // HH:MM or HH:MM:SS text returned on acceptance.
    Time,
    // A multi-line Memo. It preserves ordinary Enter-for-newline editing;
    // the form's default action remains reachable by Tab or pointer.
    Memo,
};

struct FieldDescriptor {
    std::string label;         // may carry a '&' mnemonic; empty = unlabeled field
    std::string initial_text;  // Text fields only

    // Optional; empty means "always valid". Runs at accept time only
    // (the architecture §5 dialog-accept veto) — never on every
    // keystroke, since a field mid-edit is expected to be transiently
    // invalid.
    std::function<bool(const std::string&)> validate;

    // Secret fields retain their real value for validation/completion while
    // displaying only `password_echo_char`. This belongs on the descriptor,
    // rather than requiring a caller to reconstruct a materialized dialog,
    // so declarative input boxes can safely request password entry too.
    bool password_echo = false;
    char password_echo_char = '*';

    // Last, not first, although a field's kind is the first thing about it:
    // this is an aggregate that callers brace-initialize positionally
    // (`{"&Name:", "", nullptr}`), and putting a new member ahead of `label`
    // would break every one of those for a field kind they do not use. New
    // members go on the end, and a Check field names its members.
    FieldKind kind = FieldKind::Text;
    // Check fields only. Ignored, and left false, for a Text field.
    bool initial_checked = false;
    // Radio and Combo: what there is to choose from, in the order a reader
    // sees them.
    //
    // Every member from here down carries an explicit initializer, empty
    // though it is. Without one, a call site that brace-initializes the first
    // three members positionally — which is how nearly every dialog in this
    // tree is written — trips -Wmissing-field-initializers for a field kind
    // it is not using.
    std::vector<std::string> options{};
    // Radio and Combo: which option starts chosen. -1 is "none", which a
    // Radio shows as nothing selected and a Combo as an empty box.
    int initial_selection = -1;
    // Combo only: whether a value that is not one of `options` may be typed.
    bool editable = false;
    // Number only, and only where a bound is real. A field with no minimum
    // and no maximum still has to be a number; these narrow it further.
    std::optional<long long> minimum{};
    std::optional<long long> maximum{};
    // Date only. Empty is permitted by default because optional bounds and
    // due dates are ordinary form facts; set `date_optional` false for a
    // mandatory date. No clock or locale is consulted by the control.
    std::optional<DateValue> initial_date{};
    std::optional<DateValue> date_seed{};
    bool date_optional = true;
    // Time only. TimePicker has no implicit clock or locale; the caller
    // supplies the initial value and presentation choices.
    TimeValue initial_time{};
    bool time_show_seconds = true;
    bool time_24_hour = true;
    // Memo only: its requested visible height inside the form. A dialog may
    // still scroll its field area on a smaller terminal. Values below one are
    // treated as one row rather than creating a non-interactive field.
    int memo_rows = 5;
};

// What pressing a button does to the dialog around it. A button descriptor is
// written before its window exists, so the role is how a descriptor says what
// the press means; the wiring that closes a window is done where the window is.
//
// This replaces an `is_default` flag, which could only say which button Enter
// reaches. A dialog has three kinds of button and the flag could express one:
// a Cancel written as a non-default button ran its own handler and nothing
// else, so a dialog whose Cancel had nothing to do stayed on screen when the
// reader pressed it. That was not a caller's mistake to make.
enum class ButtonRole {
    // Runs its handler and leaves the dialog open: Apply, Browse..., Reset.
    // The default, because it is the role that does nothing on its own.
    Neutral,
    // The default button. Enter reaches it from anywhere in the dialog, the
    // fields are validated first (a failing validator vetoes the press), and
    // the completion carries `accepted == true`. At most one per dialog.
    Accept,
    // Dismisses the dialog exactly as Esc does: no validation, no values, and
    // a completion carrying `accepted == false`. Its `on_press`, if it has
    // one, runs before the dialog goes.
    Dismiss,
};

struct ButtonDescriptor {
    std::string label;
    ButtonRole role = ButtonRole::Neutral;
    std::function<void()> on_press;
};

struct DialogDescriptor {
    std::string title;  // not yet rendered anywhere (no window chrome until M5) — carried for that wiring
    std::vector<FieldDescriptor> fields;
    std::vector<ButtonDescriptor> buttons;

    // Whether wire_dialog_window() lets the user resize the hosting
    // window (M10/WP-21; the widget catalog's "resizable dialogs with
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
    // Contextual F1 topic inherited by every field and button through the
    // hosting Window. Keeping this on the descriptor prevents declarative
    // forms from losing help merely because their callers do not hand-build
    // the Window that presents them.
    std::string help_context_key{};
};

struct MaterializedDialog {
    std::unique_ptr<ui::View> root;
    // All three are parallel to descriptor.fields and are indexed by field,
    // so a caller reads the one its own field kind filled in: `labels` is
    // nullptr where a field had no label (and always for a Check field,
    // which carries its label itself), `inputs` is nullptr for a Check
    // field, and `checks` is nullptr for a Text field.
    std::vector<Label*> labels;
    std::vector<InputLine*> inputs;
    std::vector<Memo*> memos;
    std::vector<CheckGroup*> checks;
    std::vector<RadioGroup*> radios;
    std::vector<ComboBox*> combos;
    std::vector<DatePicker*> dates;
    std::vector<TimePicker*> times;
    std::vector<Button*> buttons;    // parallel to descriptor.buttons
    ui::View* initial_focus = nullptr;
    Button* default_button = nullptr;
    // The viewport the fields live in (never null; see this file's header
    // for the shape). Owned by `root`. A caller reads it to scroll the form
    // itself — to bring its own field into view after changing something,
    // or to put the form back at the top — and a test reads it to state, in
    // numbers, what the reader can currently see. The button row is NOT
    // inside it and never scrolls.
    ScrollViewport* content_viewport = nullptr;
};

// The typed outcome of a descriptor dialog. An accepted result contains one
// value for each descriptor field, in descriptor order. Close, Esc, external
// detach, and a host quit produce a default-constructed result with
// `accepted == false`; no partial field values escape a cancelled dialog.
struct DialogResult {
    bool accepted = false;
    // Both are parallel to descriptor.fields, so a field's answer is always
    // at its own index whatever kind it is: `values` holds a Text field's
    // contents (empty string for a Check field) and `checked` a Check
    // field's state (false for a Text field). Two aligned vectors rather
    // than one variant, so the long-standing `result.values[i]` reading of
    // a text form keeps working unchanged.
    std::vector<std::string> values;
    std::vector<bool> checked;
    // Radio and Combo: the chosen index, or -1 for "none chosen" and for
    // every field of another kind. A Combo also fills `values` with its text,
    // so an editable one that was typed into rather than picked from still
    // answers — its index is -1 and its text is what the reader wrote.
    std::vector<int> selected{};
    // Number: the value, already parsed, so no caller has to parse the text a
    // validator has just proved is a number. Empty for every other kind.
    std::vector<std::optional<long long>> numbers{};
    // Date: the typed answer, empty for an optional blank date and for every
    // field of another kind. `values[i]` simultaneously carries canonical
    // YYYY-MM-DD text so generic form consumers retain a uniform text view.
    std::vector<std::optional<DateValue>> dates{};
    // Time: the typed value. Empty for every other field; `values[i]`
    // simultaneously carries canonical HH:MM or HH:MM:SS text.
    std::vector<std::optional<TimeValue>> times{};

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
//  - every ButtonRole::Dismiss button: runs its own on_press (if any) and
//    then takes the cancel path, so a Cancel button and Esc are one
//    behaviour rather than two that can drift apart.
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
// The content scrolls and pins its buttons exactly as it does under
// present_dialog (see this file's header) — that is a property of the
// materialized tree, not of the presentation. What this function does not
// do is re-fit `window` to the desktop on a terminal resize: the window is
// the caller's, and so is its geometry policy.
void wire_dialog_window(Window& window, MaterializedDialog dialog, const DialogDescriptor& descriptor,
                         ui::Application& app, ui::View* restore_focus_to);

// Presents a descriptor-built dialog modally without starting a nested loop.
// The descriptor is taken by value and retained by the presentation until its
// window detaches, so field validators and button handlers never borrow a
// caller-local descriptor. Completion fires exactly once after detachment.
//
// The window opens at the dialog's own recommended height, clamped to what
// `desktop` can show, and re-answers that question on every desktop resize
// (U4-g): a terminal that shrinks below the form turns the dialog into a
// scrolling one with its buttons still against its bottom edge, and one that
// grows again gives the dialog its full height back. A `resizable` descriptor
// is left alone after it opens — once the reader has a resize grip, the size
// is theirs.
[[nodiscard]] DescriptorDialogPresentation present_dialog(DialogDescriptor descriptor, ui::Application& app,
                                                           Desktop& desktop, const ui::StandardRoles& roles);

// Outer-loop-only blocking counterpart to present_dialog. It delegates to
// Desktop::exec_modal, so calling it from a handler, posted callback, or timer
// is rejected before a nested pump starts. A host quit returns `{false, {}}`.
// Same sizing and resize behaviour as present_dialog.
DialogResult exec_dialog(DialogDescriptor descriptor, ui::Application& app,
                         Desktop& desktop, const ui::StandardRoles& roles);

}  // namespace ckv::widgets
