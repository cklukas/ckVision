// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <functional>
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"

namespace ckv::widgets {

// Why a SpinBox refused a typed entry: text that is not a whole number, or a
// number outside the box's range.
enum class SpinBoxRefusal { NotANumber, OutOfRange };

// Integer field and explicit adjustment controls.
enum class SpinBoxPresentation { Compact, Separate, Stacked };

// An integer field with explicit minus/plus steppers, or stacked up/down
// controls. Arrow keys and the wheel adjust the value.
class CVISION_TYPE_API SpinBox : public ui::View {
public:
    // A Tab stop over 0..100, step 1, at 0, not editable.
    CVISION_MEMBER_API SpinBox();
    // Explicit unbracketed field and stepper arrangement.
    CVISION_MEMBER_API void set_presentation(SpinBoxPresentation presentation);
    SpinBoxPresentation presentation() const noexcept { return presentation_; }
    CVISION_MEMBER_API Rect field_bounds() const noexcept;
    CVISION_MEMBER_API Rect decrement_bounds() const noexcept;
    CVISION_MEMBER_API Rect increment_bounds() const noexcept;
    // Preferred horizontal extent covering digits and stepper buttons.
    CVISION_MEMBER_API ui::SizeHint horizontal_size_hint() const override;
    CVISION_MEMBER_API ui::SizeHint vertical_size_hint() const override;
    // The inclusive bounds; given the wrong way round they are swapped. The
    // value is clamped into the new range, which fires on_change if it moves.
    // An entry being typed is kept, and judged against the new range when it
    // is committed.
    CVISION_MEMBER_API void set_range(int minimum, int maximum);
    int minimum() const noexcept { return minimum_; }
    int maximum() const noexcept { return maximum_; }
    // How far one key press or click moves the value; at least 1. Steps are
    // not aligned to multiples of it, and the last step is clamped at the
    // range's end.
    CVISION_MEMBER_API void set_step(int step);
    // Current keyboard/stepper step size.
    int step() const noexcept { return step_; }
    // Override the theme role used for the numeric field.
    CVISION_MEMBER_API void set_role_override(ui::RoleId role);
    // The owner's value, clamped into the range: an owner setting a value is
    // not a reader typing one, so nothing is refused. Abandons an entry
    // being typed.
    CVISION_MEMBER_API void set_value(int value);
    int value() const noexcept { return value_; }
    // Whether the reader may type a number. Off by default; turning it off
    // abandons an entry being typed.
    CVISION_MEMBER_API void set_editable(bool editable);
    bool editable() const noexcept { return editable_; }
    // Whether a typed entry is open, and its text so far (empty when not).
    bool editing() const noexcept { return entry_.has_value(); }
    std::string_view entry() const noexcept { return entry_ ? std::string_view(*entry_) : std::string_view{}; }
    // Commits the entry being typed, as Enter does, and returns whether it
    // was taken; true when none was open.
    CVISION_MEMBER_API bool commit_entry();
    // Abandons the entry being typed, as Escape does, clearing a refusal.
    CVISION_MEMBER_API void cancel_entry();
    // False while a refused entry stands, and validation_message() then
    // says why; true, with an empty message, otherwise.
    bool valid() const noexcept { return refusal_.empty(); }
    const std::string& validation_message() const noexcept { return refusal_; }
    // The reasons given for a refused entry, from the kind of refusal and
    // the range in force. The default is English: "Not a whole number." and
    // "Enter a number from <minimum> to <maximum>.", the numbers written in
    // plain ASCII digits. An empty function restores it.
    CVISION_MEMBER_API void set_refusal_text(std::function<std::string(SpinBoxRefusal, int minimum, int maximum)> text);
    // Fires with the new value after every change, from the reader or from
    // set_value and set_range.
    std::function<void(int)> on_change;
    // Fires with the reason each time a typed entry is refused.
    std::function<void(const std::string&)> on_invalid;

    CVISION_MEMBER_API void draw(scene::Painter& painter) override;
    CVISION_MEMBER_API bool on_key(const KeyEvent& event) override;
    CVISION_MEMBER_API bool on_text(const TextEvent& event) override;
    CVISION_MEMBER_API bool on_mouse(const MouseEvent& event) override;
    CVISION_MEMBER_API void on_focus(const FocusEvent& event) override;
    CVISION_MEMBER_API void on_attached() override;
    CVISION_MEMBER_API std::optional<PointerShape> pointer_shape_at(Point local) const override;
    CVISION_MEMBER_API void on_hover_changed(bool hovered) override;
    void on_resized() override { hover_position_.reset(); invalidate(); }
    // The caret at the end of an entry being typed, while focused.
    CVISION_MEMBER_API std::optional<CursorState> cursor_state() const override;

private:
    void adjust(int delta);
    void assign(int value);
    void edit_entry(std::string entry);
    // What draw() shows in a field `width` columns wide, and where in it the
    // entry's text ends: the one layout the drawing and the caret share.
    struct Shown {
        std::string_view text;
        int text_end = 0;
    };
    Shown shown_entry(int width) const;
    bool can_step(int delta) const noexcept;
    SpinBoxPresentation presentation_ = SpinBoxPresentation::Compact;
    int minimum_ = 0;
    int maximum_ = 100;
    int step_ = 1;
    int value_ = 0;
    bool editable_ = false;
    std::optional<std::string> entry_{};
    std::string refusal_;
    std::function<std::string(SpinBoxRefusal, int, int)> refusal_text_;
    std::optional<Point> hover_position_;
    ui::RoleId accessory_role_ = ui::kInvalidRole;
    ui::RoleId accessory_hovered_role_ = ui::kInvalidRole;
    ui::RoleId surface_role_ = ui::kInvalidRole;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId role_override_ = ui::kInvalidRole;
    ui::RoleId invalid_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};


}  // namespace ckv::widgets
