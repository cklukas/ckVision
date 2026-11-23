// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/ui_strings.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/date_time.hpp"
#include "cvision/widgets/input_line.hpp"

namespace ckv::widgets {

// What a PropertyItem holds. The kind decides how the value is shown, which
// editor changes it, and what that editor accepts. Every kind's value is
// text in one canonical spelling, so a host reads any of them the same way
// and parses the ones it needs (parse_iso_date, parse_iso_time, and so on).
enum class PropertyKind {
    // Free text, edited in an InputLine; only the item's `validate` can refuse
    // it.
    Text,
    // "true" or "false", shown as a check box, "[X]" or "[ ]", and toggled in
    // place by Enter, Space or a click on it: there is nothing to type.
    Bool,
    // One of the item's `choices`, picked from a pick-only ComboBox whose list
    // drops as the edit begins.
    Choice,
    // A whole number, typed in an InputLine. Refused unless all of it reads as
    // one and it lies within `minimum`..`maximum`; committed in its plain
    // decimal spelling ("007" becomes "7").
    Integer,
    // A decimal number, likewise, in the "C" locale's spelling whatever the
    // host's; committed in its shortest exact spelling ("1.50" becomes "1.5").
    // Infinities and NaN are refused.
    Real,
    // A calendar date, YYYY-MM-DD, edited with a DatePicker; a value that does
    // not parse starts the picker on 2026-01-01.
    Date,
    // A time of day, HH:MM or HH:MM:SS, edited with a TimePicker that shows
    // the seconds exactly when the value has them.
    Time,
};

// One row of a PropertyInspector: a name, a value, and what kind of value it
// is.
struct CVISION_TYPE_API PropertyItem {
    // The label, and the value as text; the inspector edits the value in
    // place.
    std::string name;
    std::string value;
    // Whether the value may be edited. A read-only row still takes the cursor.
    bool editable = true;
    // Every member from here down has an initializer, so a row written
    // positionally as {name, value, editable} is a Text row.
    PropertyKind kind = PropertyKind::Text;
    // Choice only: what there is to choose from, in the order the list shows.
    std::vector<std::string> choices{};
    // Integer and Real only: the inclusive bounds, where a bound is real.
    std::optional<double> minimum{};
    std::optional<double> maximum{};
    // A check of the caller's, run on commit after the kind's own with the
    // canonical text: std::nullopt accepts, and anything else is the reason
    // the edit is refused, shown under the row.
    std::function<std::optional<std::string>(const std::string&)> validate{};
    // Optional owned section label; consecutive equal labels form one group.
    std::string group{};
};

// Explicit property chrome; Sectioned shows optional owned group headings.
enum class PropertyPresentation { Plain, Divided, Sectioned };

// A two-column list of properties: the names in a column as wide as the
// widest (up to half the view), the values aligned after a two-cell gutter,
// one row each from the top, with the cursor row drawn as a full-width bar
// (muted while neither the inspector nor its editor holds the keyboard),
// with vertical scrolling via scroll_y() / set_scroll_y().
//
// Up/Down move the cursor. Enter or F2 on an editable row, or a click on its
// value, edits it in place with the editor its kind calls for — an
// InputLine, a pick-only ComboBox, a DatePicker or a TimePicker, laid over
// the value column and given the keyboard — except that a Bool row toggles
// at once. While editing, the keys go to the editor first; what it leaves
// comes here: Enter commits, as does Tab or Up/Down on its way elsewhere, and
// Escape cancels. Choosing from a Choice row's list commits too. A commit
// checks the value (the kind's rule, then the item's `validate`); a refused
// value keeps the edit open, marks the editor invalid, and shows the reason
// on a row of its own under the edited one until the value is changed again.
// A refused toggle changes nothing and shows its reason the same way.
// Needs an Application to hand the keyboard to an editor: detached, only Bool
// rows can be changed.
//
// A Tab stop. Resolves "ckv.list.normal", "ckv.list.selected",
// "ckv.list.selected.inactive", "ckv.list.disabled" and "ckv.input.invalid"
// (the reason) on attach; the editors resolve their own.
class CVISION_TYPE_API PropertyInspector : public ui::View {
public:
    // A Tab stop with no rows, its editors built and hidden.
    CVISION_MEMBER_API PropertyInspector();
    // Plain is default; changing chrome preserves and repositions an open edit.
    CVISION_MEMBER_API void set_presentation(PropertyPresentation presentation);
    PropertyPresentation presentation() const noexcept { return presentation_; }
    CVISION_MEMBER_API void set_banded_rows(bool banded);
    bool banded_rows() const noexcept { return banded_rows_; }
    // Clipped local property/value rectangles; headings and reasons are excluded.
    CVISION_MEMBER_API Rect item_bounds(std::size_t index) const noexcept;
    CVISION_MEMBER_API Rect value_bounds(std::size_t index) const;
    CVISION_MEMBER_API ui::SizeHint vertical_size_hint() const override;
    CVISION_MEMBER_API std::optional<PointerShape> pointer_shape_at(Point local) const override;
    // Replaces the rows, puts the cursor on the first and cancels any edit.
    CVISION_MEMBER_API void set_items(std::vector<PropertyItem> items);
    // The rows, with every committed edit.
    const std::vector<PropertyItem>& items() const noexcept { return items_; }
    // The index of the cursor row, or -1 when there are no rows.
    int cursor() const noexcept { return cursor_; }
    // Current vertical scroll offset in rows.
    int scroll_y() const noexcept { return scroll_y_; }
    // Set vertical scroll offset (clamped to valid range).
    CVISION_MEMBER_API void set_scroll_y(int scroll_y);
    // Replaces the reasons the inspector gives of its own accord.
    // Replaces the validation strings used when refusing numeric edits.
    CVISION_MEMBER_API void set_messages(ui::UiStrings messages);

    // Whether an editor is open on the cursor row.
    bool editing() const noexcept { return editor_ != nullptr; }
    // Why the last commit or Bool toggle was refused; empty when it was not.
    // Cleared by any change to the value being edited, and by whatever the
    // reader does next with the inspector.
    const std::string& validation_message() const noexcept { return reason_; }
    // Opens the cursor row's editor, or toggles a Bool row (subject to its
    // `validate`). Returns false for a read-only row, with no rows, while
    // already editing, or — for every kind but Bool — without an Application.
    CVISION_MEMBER_API bool begin_edit();
    // Checks the edited value and, when it passes, stores it, closes the
    // editor, gives the keyboard back to the inspector and fires on_change if
    // the value changed. Returns whether it passed; false too when nothing is
    // being edited.
    CVISION_MEMBER_API bool commit_edit();
    // Closes the editor without storing anything.
    CVISION_MEMBER_API void cancel_edit();
    // Fires once per committed change of a value — an edit that passed its
    // check and left the value different, or a Bool toggle — with the row's
    // index and its new canonical text. Not fired by set_items.
    std::function<void(std::size_t, std::string)> on_change;

    CVISION_MEMBER_API void draw(scene::Painter& painter) override;
    CVISION_MEMBER_API bool on_key(const KeyEvent& event) override;
    CVISION_MEMBER_API bool on_mouse(const MouseEvent& event) override;
    CVISION_MEMBER_API void on_focus(const FocusEvent& event) override;
    CVISION_MEMBER_API void on_attached() override;
    CVISION_MEMBER_API void on_resized() override;

private:
    // The value column's first cell and the screen row of item `index`,
    // which a shown reason pushes down for every row after the edited one.
    int value_x() const;
    void rebuild_rows();
    bool begins_group(std::size_t index) const noexcept;
    int row_of(std::size_t index) const noexcept;
    // The item drawn on view row `y`, or -1 for the reason row and past the
    // last item.
    int item_at_row(int y) const noexcept;
    // The edited value as canonical text, before any check.
    std::string editor_text() const;
    // The kind's own check, then the item's; the reason, or std::nullopt.
    // `text` is rewritten to its canonical spelling when it passes.
    std::optional<std::string> check(const PropertyItem& item, std::string& text) const;
    void toggle(std::size_t index);
    void place_editor();
    void close_editor();
    void refuse(std::string reason);
    void clear_reason();
    // Commits an open edit (true when there was none), then moves the cursor.
    bool move_cursor_to(int index);
    void ensure_cursor_visible();

    PropertyPresentation presentation_ = PropertyPresentation::Plain;
    bool banded_rows_ = false;
    std::vector<int> rows_;
    int content_rows_ = 0;
    int scroll_y_ = 0;
    bool custom_messages_ = false;
    std::vector<PropertyItem> items_;
    int cursor_ = -1;
    ui::UiStrings messages_{};
    // The editors, built once and shown one at a time. They are never
    // destroyed mid-edit, so an editor's own callback may end the edit it is
    // part of.
    InputLine* text_editor_ = nullptr;
    ComboBox* choice_editor_ = nullptr;
    DatePicker* date_editor_ = nullptr;
    TimePicker* time_editor_ = nullptr;
    // The editor open on the cursor row, or nullptr.
    ui::View* editor_ = nullptr;
    // Why the last commit or toggle was refused, shown under item
    // `reason_index_`; empty when nothing is refused.
    std::string reason_;
    int reason_index_ = -1;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId selected_inactive_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId reason_role_ = ui::kInvalidRole;
    ui::RoleId banded_role_ = ui::kInvalidRole;
    ui::RoleId heading_role_ = ui::kInvalidRole;
    ui::RoleId divider_role_ = ui::kInvalidRole;
};


}  // namespace ckv::widgets
