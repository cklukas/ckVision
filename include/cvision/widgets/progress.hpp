// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// A one-row progress bar: a track with a lit span, and an optional label laid
// over it. The span is either the finished share of a known amount of work, or
// in indeterminate mode a moving block that says only "still working". It
// takes no input and does not animate by itself: the caller advances it.
//
// Resolves its theme roles from context() once attached: "ckv.list.normal"
// for the track and "ckv.menu.bar.active" for the lit span.
class Progress : public ui::View {
public:
    // Starts determinate and empty (fraction 0.0), with no label.
    Progress();

    // The finished share, 0.0 to 1.0. Values outside that range are clamped
    // and a NaN or infinity is taken as 0.0. The lit span is the share of the
    // width rounded to the nearest cell. Ignored for drawing while the bar is
    // indeterminate, but kept.
    void set_fraction(double fraction);
    double fraction() const noexcept { return fraction_; }

    // Indeterminate mode, for work of unknown length: instead of the fraction
    // the bar lights a block a quarter of its width (at least one cell) whose
    // place is set by the pulse. The pulse is a step count the caller
    // advances, typically from a timer; each step moves the block one cell to
    // the right. At pulse 0 the block sits just off the left edge; it sweeps
    // across and wraps every width-plus-block-width steps, so any int,
    // negative included, is a valid pulse.
    void set_indeterminate(bool indeterminate);
    bool indeterminate() const noexcept { return indeterminate_; }
    void set_pulse(int offset);
    int pulse() const noexcept { return pulse_; }

    // Text centred over the bar, such as "42 %" or "Copying...". Each
    // character takes the style of the surface under it, lit or unlit, so the
    // label never hides how far the bar has come. Clipped to the width at a
    // grapheme boundary; empty draws no label. A change re-publishes the size
    // hint, whose preferred width grows to fit the label plus four cells.
    void set_label(std::string label);
    const std::string& label() const noexcept { return label_; }

    void on_attached() override;
    void draw(scene::Painter& painter) override;
    // Exactly one row high. At least four cells wide, preferring 20 cells or
    // the label's width plus four, whichever is larger, and free to stretch.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

private:
    double fraction_ = 0.0;
    bool indeterminate_ = false;
    int pulse_ = 0;
    std::string label_;
    ui::RoleId track_role_ = ui::kInvalidRole;
    ui::RoleId fill_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
