// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// One-row meter geometry. Solid preserves the classic background span;
// Block uses visible full/shaded blocks, Segmented uses a glyph and a gap,
// and Smooth uses eight Unicode subcell steps.
enum class ProgressPresentation { Solid, Block, Segmented, Smooth };

// Unknown-total movement, independent of meter geometry.
enum class ProgressActivityStyle { Sweep, Bounce, Pulse };
// Explicit glyph policy; never inferred from color or environment.
enum class ProgressGlyphs { Unicode, Ascii };
// Static mode uses stable unknown-total activity and schedules no animation.
enum class ProgressMotion { Animated, Static };

// A one-row progress bar: a track with a lit span, and an optional label laid
// over it. The span is either the finished share of a known amount of work, or
// in indeterminate mode a moving block that says only "still working". It
// takes no input and does not animate by itself: the caller advances it.
//
// Resolves dedicated ckv.progress.track/fill/label/disabled roles on attach.
class Progress : public ui::View {
public:
    // Starts determinate and empty (fraction 0.0), with no label.
    Progress();

    // Select meter geometry without changing fraction, label or pulse.
    void set_presentation(ProgressPresentation presentation);
    ProgressPresentation presentation() const noexcept { return presentation_; }
    // Select unknown-total movement; Sweep preserves the original behavior.
    void set_activity_style(ProgressActivityStyle style);
    ProgressActivityStyle activity_style() const noexcept { return activity_style_; }
    // Select Unicode or portable ASCII meter glyphs.
    void set_glyphs(ProgressGlyphs glyphs);
    ProgressGlyphs glyphs() const noexcept { return glyphs_; }
    // Static mode freezes activity at a visible marker; fraction updates still paint.
    void set_motion(ProgressMotion motion);
    // True for a visible attached animated unknown-total meter.
    bool needs_animation() const noexcept;
    // Controller-owned scheduling observer; manual meters leave it empty.
    std::function<void()> on_animation_changed;
    // Adjacent percentage, omitted in indeterminate mode or below six columns.
    // Reserves five columns (gap plus four-column readout) when it is visible.
    void set_show_percentage(bool show);
    bool show_percentage() const noexcept { return show_percentage_; }

    // The finished share, 0.0 to 1.0. Values outside that range are clamped
    // and a NaN or infinity is taken as 0.0. The lit span is the share of the
    // meter rounded to its nearest unit, or floored to eighths in Unicode Smooth.
    // Ignored for drawing while the bar is
    // indeterminate, but kept.
    void set_fraction(double fraction);
    double fraction() const noexcept { return fraction_; }

    // Indeterminate mode, for work of unknown length: instead of the fraction
    // the bar lights a block a quarter of its width (at least one cell) whose
    // place is set by the pulse. The pulse is a step count the caller
    // advances, typically from a timer; each step moves the block one meter unit to
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
    void on_detaching() override;
    void on_effective_visibility_changed() override;
    void draw(scene::Painter& painter) override;
    // Exactly one row high. At least four cells wide, preferring 20 cells or
    // the label's width plus four, whichever is larger, and free to stretch.
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

private:
    ProgressPresentation presentation_ = ProgressPresentation::Solid;
    ProgressActivityStyle activity_style_ = ProgressActivityStyle::Sweep;
    ProgressGlyphs glyphs_ = ProgressGlyphs::Unicode;
    ProgressMotion motion_ = ProgressMotion::Animated;
    bool attached_ = false;
    bool show_percentage_ = false;
    double fraction_ = 0.0;
    bool indeterminate_ = false;
    int pulse_ = 0;
    std::string label_;
    ui::RoleId track_role_ = ui::kInvalidRole;
    ui::RoleId fill_role_ = ui::kInvalidRole;
    ui::RoleId label_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
