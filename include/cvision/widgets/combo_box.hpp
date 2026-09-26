// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/history.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/popup_list.hpp"

namespace ckv::widgets {

// Whether the value can only be one of the items (PickOnly) or can also be
// typed freely in an embedded InputLine (Editable).
enum class ComboBoxMode { PickOnly, Editable };

// Editable or pick-only combo box with deterministic in-application history.
// Opening it drops a PopupList: a real floating popup on the desktop, framed
// and coloured like a dropdown menu, dismissed by Escape or a press outside
// it. Nothing about the layout it sits in changes while the list is open --
// the list is over the surface, not inside the control -- so a combo box in a
// dense row stays one row tall with its neighbours undisturbed.
//
// Where there is no desktop to drop a popup onto (a bare unit-test view, an
// embedded use with no application), opening is a no-op and the arrow keys
// still move the selection: the control is usable without its list.
//
// Disabled (D-076), it shows its current text and arrow in
// "ckv.input.disabled", with no caret or selection even when editable.
class ComboBox : public ui::View {
public:
    // An empty tab-stop combo box with no items, no text and no selection,
    // asking for 16 x 1 cells. Constructing it PickOnly selects nothing yet.
    explicit ComboBox(ComboBoxMode mode = ComboBoxMode::PickOnly);

    // The choices, in list order. Replacing them keeps the selection when its
    // index is still in range, taking that item's text into the value and the
    // editable field alike, and drops it otherwise; neither callback fires.
    void set_items(std::vector<std::string> items);
    const std::vector<std::string>& items() const noexcept { return items_; }

    // Switching to PickOnly while nothing is selected selects the first item,
    // if any; that fires on_text_changed but not on_select.
    void set_mode(ComboBoxMode mode);
    ComboBoxMode mode() const noexcept { return mode_; }
    bool editable() const noexcept { return mode_ == ComboBoxMode::Editable; }

    // The current value: the selected item's text, or what was typed or set.
    // Setting different text clears the selection (even when it equals an
    // item) and fires on_text_changed; setting the same text does nothing.
    // The value may be set in either mode.
    void set_text(std::string text);
    const std::string& text() const noexcept { return text_; }

    // The selected item, if any. Selecting an in-range index takes that item's
    // text and fires on_text_changed (never on_select); nullopt or an index
    // out of range clears the selection and leaves the text as it is.
    void set_selected_index(std::optional<std::size_t> index);
    std::optional<std::size_t> selected_index() const noexcept { return selected_index_; }

    // Names the history list this combo shares: the list under `key` in the
    // Application::history() of the Application it is attached to, which
    // every input line, combo box, search box and dialog field naming the same
    // key shares too. While a key is set and the combo is attached, an
    // Editable combo's Up and Down (list closed) recall entries, Down stepping
    // to older ones and Up back towards the text that was there before recall
    // began, instead of opening the list. Empty (the default) turns history
    // off.
    void set_history_key(std::string key);
    const std::string& history_key() const noexcept { return history_key_; }
    // Records the current text as the newest entry of the history list (empty
    // text is not recorded; nothing is without a key or an Application). Enter
    // on the closed control calls it and leaves the key unhandled, so a
    // dialog's default button still receives it.
    void commit_to_history();

    // Opens the item list as a PopupList floating over the nearest ancestor
    // Desktop, below the control (above it when too little room is below and
    // more is above), at least as wide as the control. Does nothing when it is
    // already open, when there are no items, or without an Application and a
    // Desktop. The list's cursor starts on the selected item, or on the first
    // when nothing is selected; opening selects nothing. Choosing an item in
    // the list selects it and fires on_select, then on_text_changed.
    // close_dropdown dismisses the list without choosing; nothing fires.
    void open_dropdown();
    void close_dropdown();
    bool dropdown_open() const noexcept { return popup_ != nullptr; }

    // on_select fires with the index when the reader chooses an item from the
    // open list, and only then. on_text_changed fires with the text when
    // typing or set_text changes it, on each history recall step, and on every
    // selection — a choice from the list, an arrow step where no list can
    // open, set_selected_index, or set_mode's automatic pick — even when the
    // text stays the same.
    std::function<void(std::size_t)> on_select;
    std::function<void(const std::string&)> on_text_changed;

    void on_attached() override;
    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_text(const TextEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Opens its list when clicked, anywhere on it.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return enabled() ? PointerShape::Pointer : PointerShape::NotAllowed;
    }
    void on_focus(const FocusEvent& event) override;
    void on_resized() override;

private:
    void select_index(std::size_t index, bool notify);
    void move_selection(int delta);
    void recall_history(int index);
    // The application's history registry while a key is set and the combo is
    // attached; nullptr otherwise.
    ui::HistoryRegistry* history_registry() const noexcept;
    void sync_text_from_editor();

    ComboBoxMode mode_;
    std::vector<std::string> items_;
    std::string text_;
    std::optional<std::size_t> selected_index_;
    PopupList* popup_ = nullptr;
    InputLine editor_;

    std::string history_key_;
    int history_index_ = -1;
    std::string history_saved_text_;

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;

};

}  // namespace ckv::widgets
