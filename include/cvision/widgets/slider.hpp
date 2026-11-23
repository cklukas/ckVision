// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <functional>
#include <string>
#include <vector>
#include "cvision/ui/standard_roles.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/orientation.hpp"

namespace ckv::widgets {

// A labelled mark on a Slider's scale at a value the host chooses.
struct CVISION_API SliderTick {
    // Where the mark stands, in the slider's own units, and the text under
    // it. A value outside the range is not drawn.
    int value = 0;
    std::string label;
};

// Explicit track chrome; all styles share the same value and tick mapping.
enum class SliderPresentation { Line, Block, ProminentThumb };

// A horizontal slider: a track with the part up to the value filled and a
// thumb at the value's position ("◆" while focused, "●" otherwise).
// Left/Right step the value, Home/End jump to the minimum and maximum, and a
// left click sets the value in proportion to where it lands between the
// first and last column, without snapping to the step.
//
// With ticks (set_ticks) the slider is two rows high: each tick in the range
// marks its column on the track ("┬", "┯" on the filled part; the thumb
// stands over a mark it shares a column with), and its label is drawn on the
// row below, centred on that column and moved inward as far as it must to
// stay inside the slider. Labels never overlap and always keep one blank
// between them: they are placed in the order the ticks were given, and a
// label that would collide with one already placed is left out, while its
// mark stays. So a host lists first the labels that must show -- the two
// ends, typically -- and the choice of the rest is the same on every run.
// A label wider than the slider is elided to its width.
//
// Resolves "ckv.list.normal" for the track and the labels,
// "ckv.menu.bar.active" for the filled part and thumb, and
// "ckv.list.disabled" on attach.
class CVISION_API Slider : public ui::View {
public:
    // A Tab stop over 0..100, step 1, at 0, with no ticks.
    Slider();
    // Horizontal (default) or Vertical track orientation.
    void set_orientation(Orientation orientation);
    // Current track orientation.
    Orientation orientation() const noexcept { return orientation_; }
    // Line is the default; alternatives change chrome without changing values.
    void set_presentation(SliderPresentation presentation);
    SliderPresentation presentation() const noexcept { return presentation_; }
    // Optional adjacent numeric value, without brackets; hidden at tiny widths.
    void set_show_value(bool show);
    bool show_value() const noexcept { return show_value_; }
    // Local track rectangle used by rendering, ticks and pointer input.
    Rect track_bounds() const noexcept;
    // The inclusive bounds; given the wrong way round they are swapped. The
    // value is clamped into the new range, which fires on_change if it moves.
    void set_range(int minimum, int maximum);
    // Lower bound of the slider range.
    int minimum() const noexcept { return minimum_; }
    // Upper bound of the slider range.
    int maximum() const noexcept { return maximum_; }
    // How far one Left/Right press moves the value; at least 1. The last step
    // is clamped at the range's end.
    void set_step(int step);
    // Current keyboard/wheel step size.
    int step() const noexcept { return step_; }
    // Override the theme role used for the track and labels.
    void set_role_override(ui::RoleId role);
    // Clamped into the range.
    void set_value(int value);
    int value() const noexcept { return value_; }
    // The labelled ticks, in priority order (see the class comment). An empty
    // list takes them away and the slider back to one row; a change repaints
    // and reports a size-hint change.
    void set_ticks(std::vector<SliderTick> ticks);
    const std::vector<SliderTick>& ticks() const noexcept { return ticks_; }
    // Fires with the new value after every change, from the reader or from
    // set_value and set_range.
    std::function<void(int)> on_change;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;
    // Pointer affordance over the interactive track.
    std::optional<PointerShape> pointer_shape_at(Point local) const override;
    // Preferred horizontal extent for the track (or 1 col when Vertical).
    ui::SizeHint horizontal_size_hint() const override;
    // Two rows, exactly, with ticks; without them, the size asked for.
    ui::SizeHint vertical_size_hint() const override;

private:
    void adjust(int delta);
    // The value a press at column `x` sets, and the column the thumb is drawn
    // in for the current value: the one proportional map, both ways.
    int value_from_x(int x) const;
    int x_from_value(int value) const;
    int track_width() const noexcept;
    Orientation orientation_ = Orientation::Horizontal;
    SliderPresentation presentation_ = SliderPresentation::Line;
    bool show_value_ = false;
    std::vector<std::pair<int, int>> placed_labels_;
    int minimum_ = 0;
    int maximum_ = 100;
    int step_ = 1;
    int value_ = 0;
    std::vector<SliderTick> ticks_;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId fill_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId role_override_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
