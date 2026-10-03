// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// CheckGroup (multi-select) and RadioGroup (exclusive-select): arrow
// navigation, mnemonics (the widget catalog M6a baseline). CheckGroup renders
// [ ] / [X]; RadioGroup renders ( ) / (•). Each group is ONE Tab stop —
// arrows move an internal cursor among the group's own items rather than each
// item being independently focusable, matching the classic clustered
// check/radio control.
//
// A group's choices stack in one column unless `set_columns()` lets them flow
// into several, row-major, each column as wide as the widest choice in it, so
// one row packs its choices and several rows align as a table (D-068).
// Left and Right step through the choices in order; Up and Down move within
// a column; an arrow that would not move the cursor is not the group's, so a
// window walks its controls with it (D-065).
//
// The caption `set_group_label()` gives a group is its label in the sense a
// `Label` is a field's: it may carry a '&'-marked mnemonic, drawn as a label
// draws one, and Alt+<mnemonic> pressed anywhere in the group's window gives
// the group the focus (widgets/label.hpp's routes). A choice's own mnemonic
// acts while the group has the focus.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

using ui::SizeHint;

// Explicit choice chrome; Classic is the default for both group types.
enum class OptionPresentation { Classic, BoxedRows, Buttons };

// The state of one CheckGroup choice.
enum class CheckState {
    // Drawn "[ ] "; reads as false through CheckGroup::checked.
    Unchecked,
    // Drawn "[X] "; the only state CheckGroup::checked reports as true.
    Checked,
    // Drawn "[~] ": partly on, typically a choice standing for several
    // settings that disagree. Reached by Space only in a tristate group, but
    // settable by the application in any group; reads as false through
    // CheckGroup::checked.
    Mixed,
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.option.normal"/"ckv.option.focused"/"ckv.option.disabled"
// (D-076: marks kept, no cursor row or mnemonic accent; the caption then
// wears "ckv.label.disabled") and
// "ckv.label.mnemonic".
class CheckGroup : public ui::View {
public:
    // `labels` may each carry a '&' mnemonic.
    explicit CheckGroup(std::vector<std::string> labels);
    // Boxed rows use three cells vertically; Buttons use one padded row.
    void set_presentation(OptionPresentation presentation);
    OptionPresentation presentation() const noexcept { return presentation_; }

    // Optional caption owned by this group. When present it occupies the row
    // above the choices and adopts the focused-group foreground while the
    // group has keyboard focus. It may carry a '&' mnemonic, which reaches
    // the group from anywhere in its window.
    void set_group_label(std::string label);
    const std::string& group_label() const noexcept { return caption_raw_; }
    // The caption's marked grapheme; empty when the caption marks none.
    const std::string& group_mnemonic() const noexcept { return caption_.mnemonic; }
    // How many columns the choices flow into, row-major. One, the default,
    // stacks them. Values below one are treated as one.
    void set_columns(int columns);
    int columns() const noexcept { return columns_; }
    // Optional exact display width for a measured form column. Long labels
    // clip at this edge; zero restores the natural-width contract.
    void set_column_width(int columns);
    int column_width() const noexcept { return column_width_; }

    // Replace the roles the choices are drawn with: `normal_role` for the
    // choice rows, `focused_role` for the cursor row while the group has
    // focus (its foreground also colours the caption then). A role left as
    // kInvalidRole is resolved from the standard names at attach.
    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role) noexcept {
        if (normal_role_ == normal_role && focused_role_ == focused_role) return;
        normal_role_ = normal_role;
        focused_role_ = focused_role;
        invalidate();
    }
    // The role whose colours accent the mnemonic letters of the choices and
    // the caption, in place of "ckv.label.mnemonic".
    void set_mnemonic_role_override(ui::RoleId role) noexcept {
        if (mnemonic_role_ == role) return;
        mnemonic_role_ = role;
        invalidate();
    }

    // Per-choice state; `index` must be below the number of labels (checked
    // by assertion). checked() is true only for CheckState::Checked;
    // set_checked(i, v) is set_check_state with Checked or Unchecked. A change
    // fires on_state_changed and then on_changed — programmatic changes
    // included — and setting the state a choice already has fires nothing.
    bool checked(std::size_t index) const;
    void set_checked(std::size_t index, bool value);
    CheckState check_state(std::size_t index) const;
    void set_check_state(std::size_t index, CheckState state);
    // Whether Space, a click or a mnemonic cycles a choice through
    // Unchecked, Checked, Mixed and back, rather than toggling Checked and
    // Unchecked (where a Mixed choice becomes Checked). Off by default.
    void set_tristate(bool enabled) noexcept { tristate_ = enabled; }
    bool tristate() const noexcept { return tristate_; }

    // Fired whenever an item's checked state changes, with its index
    // and new bool state. Mixed reports false here; use on_state_changed
    // when the caller needs to distinguish Mixed from Unchecked.
    std::function<void(std::size_t, bool)> on_changed;
    // Fired on the same changes with the full new state, before on_changed.
    std::function<void(std::size_t, CheckState)> on_state_changed;

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    // Arrows move the cursor (see the file comment); Space toggles the cursor
    // choice, and a choice's mnemonic letter (typed without Ctrl or Super)
    // moves the cursor to it and toggles it. Enter is deliberately left
    // unhandled so it reaches the dialog's default button.
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Each choice toggles when clicked.
    std::optional<PointerShape> pointer_shape_at(Point local) const override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    void toggle(std::size_t index);

    OptionPresentation presentation_ = OptionPresentation::Classic;
    std::vector<MnemonicText> parsed_labels_;
    mutable std::vector<int> column_positions_;
    mutable std::vector<int> column_extents_;
    std::vector<std::string> labels_;
    std::string caption_raw_;
    MnemonicText caption_;
    std::vector<CheckState> states_;
    std::size_t cursor_ = 0;
    bool tristate_ = false;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId mnemonic_role_ = ui::kInvalidRole;
    ui::RoleId group_label_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId label_disabled_role_ = ui::kInvalidRole;
    int columns_ = 1;
    int column_width_ = 0;
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.option.normal"/"ckv.option.focused"/"ckv.option.disabled",
// drawn disabled exactly as CheckGroup is.
class RadioGroup : public ui::View {
public:
    // `labels` may each carry a '&' mnemonic. Nothing is selected initially.
    explicit RadioGroup(std::vector<std::string> labels);
    // Boxed rows use three cells vertically; Buttons use one padded row.
    void set_presentation(OptionPresentation presentation);
    OptionPresentation presentation() const noexcept { return presentation_; }

    // Optional caption owned by this group. When present it occupies the row
    // above the choices and adopts the focused-group foreground while the
    // group has keyboard focus. It may carry a '&' mnemonic, which reaches
    // the group from anywhere in its window.
    void set_group_label(std::string label);
    const std::string& group_label() const noexcept { return caption_raw_; }
    // The caption's marked grapheme; empty when the caption marks none.
    const std::string& group_mnemonic() const noexcept { return caption_.mnemonic; }
    // How many columns the choices flow into, row-major. One, the default,
    // stacks them. Values below one are treated as one.
    void set_columns(int columns);
    int columns() const noexcept { return columns_; }
    // Optional exact display width for a measured form column. Long labels
    // clip at this edge; zero restores the natural-width contract.
    void set_column_width(int columns);
    int column_width() const noexcept { return column_width_; }

    // Replace the roles the choices are drawn with, as CheckGroup's
    // set_role_override does.
    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role) noexcept {
        if (normal_role_ == normal_role && focused_role_ == focused_role) return;
        normal_role_ = normal_role;
        focused_role_ = focused_role;
        invalidate();
    }
    // The role whose colours accent the mnemonic letters of the choices and
    // the caption, in place of "ckv.label.mnemonic".
    void set_mnemonic_role_override(ui::RoleId role) noexcept {
        if (mnemonic_role_ == role) return;
        mnemonic_role_ = role;
        invalidate();
    }

    // The selected choice's index, or -1 when none is. set_selected accepts
    // -1 (clear) or a valid index and ignores anything else; a valid index
    // also moves the keyboard cursor there. A change fires on_changed,
    // programmatic changes included; re-selecting the selected choice does not.
    int selected() const noexcept { return selected_; }  // -1 means none selected
    void set_selected(int index);

    // Fired whenever selection changes, with the newly selected index.
    std::function<void(int)> on_changed;

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    // An arrow that moves the cursor also selects the choice it lands on;
    // Space selects the cursor choice, and a choice's mnemonic letter (typed
    // without Ctrl or Super) selects it. Enter is deliberately left unhandled
    // so it reaches the dialog's default button.
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Each choice selects when clicked.
    std::optional<PointerShape> pointer_shape_at(Point local) const override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    OptionPresentation presentation_ = OptionPresentation::Classic;
    std::vector<MnemonicText> parsed_labels_;
    mutable std::vector<int> column_positions_;
    mutable std::vector<int> column_extents_;
    std::vector<std::string> labels_;
    std::string caption_raw_;
    MnemonicText caption_;
    int selected_ = -1;
    std::size_t cursor_ = 0;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId mnemonic_role_ = ui::kInvalidRole;
    ui::RoleId group_label_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId label_disabled_role_ = ui::kInvalidRole;
    int columns_ = 1;
    int column_width_ = 0;
};

}  // namespace ckv::widgets
