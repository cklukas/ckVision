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

enum class CheckState {
    Unchecked,
    Checked,
    Mixed,
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.option.normal"/"ckv.option.focused" and
// "ckv.label.mnemonic".
class CheckGroup : public ui::View {
public:
    // `labels` may each carry a '&' mnemonic.
    explicit CheckGroup(std::vector<std::string> labels);

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

    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role) noexcept {
        normal_role_ = normal_role;
        focused_role_ = focused_role;
    }
    void set_mnemonic_role_override(ui::RoleId role) noexcept { mnemonic_role_ = role; }

    bool checked(std::size_t index) const;
    void set_checked(std::size_t index, bool value);
    CheckState check_state(std::size_t index) const;
    void set_check_state(std::size_t index, CheckState state);
    void set_tristate(bool enabled) noexcept { tristate_ = enabled; }
    bool tristate() const noexcept { return tristate_; }

    // Fired whenever an item's checked state changes, with its index
    // and new bool state. Mixed reports false here; use on_state_changed
    // when the caller needs to distinguish Mixed from Unchecked.
    std::function<void(std::size_t, bool)> on_changed;
    std::function<void(std::size_t, CheckState)> on_state_changed;

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Each choice toggles when clicked.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return enabled() ? PointerShape::Pointer : PointerShape::NotAllowed;
    }
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    void toggle(std::size_t index);

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
    int columns_ = 1;
    int column_width_ = 0;
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.option.normal"/"ckv.option.focused".
class RadioGroup : public ui::View {
public:
    explicit RadioGroup(std::vector<std::string> labels);

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

    void set_role_override(ui::RoleId normal_role, ui::RoleId focused_role) noexcept {
        normal_role_ = normal_role;
        focused_role_ = focused_role;
    }
    void set_mnemonic_role_override(ui::RoleId role) noexcept { mnemonic_role_ = role; }

    int selected() const noexcept { return selected_; }  // -1 means none selected
    void set_selected(int index);

    // Fired whenever selection changes, with the newly selected index.
    std::function<void(int)> on_changed;

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Each choice selects when clicked.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return enabled() ? PointerShape::Pointer : PointerShape::NotAllowed;
    }
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    std::vector<std::string> labels_;
    std::string caption_raw_;
    MnemonicText caption_;
    int selected_ = -1;
    std::size_t cursor_ = 0;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId mnemonic_role_ = ui::kInvalidRole;
    ui::RoleId group_label_role_ = ui::kInvalidRole;
    int columns_ = 1;
    int column_width_ = 0;
};

}  // namespace ckv::widgets
