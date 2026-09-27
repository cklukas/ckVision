// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ImageView: shows an Image scaled to its cell box, mandatory fallback
// content, dual-space (cell + pixel) mouse events where the terminal
// reports pixels (the widget catalog — Image view has NO baseline
// column; this implements its full beyond-baseline listed feature
// set). Built directly on Painter::draw_image, which already makes a
// fallback mandatory by construction (D-017) — this widget's own
// fallback is a simple placeholder fill+glyph; a real application can
// swap in anything by subclassing or by requesting a richer fallback
// hook if a concrete need arises.
//
// Scope note: "scaled to its cell box" means the underlying Image is
// handed to draw_image at a cell-box anchor and the term layer resizes
// the pixels to fit it (the architecture §7) — this widget never
// resamples. What it does decide is WHICH cells to claim: a picture
// stretched to whatever rectangle a layout happened to give it is not
// the picture, so the anchor keeps the image's proportions on the cell
// the terminal actually has, and the view centres it in the space left
// over.
#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "cvision/core/image.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// Resolves its own theme role from context() once attached (M9
// WP-7, D-028): "ckv.image.fallback".
class ImageView : public ui::View {
public:
    // A view with no image, which draws a blank fill in the fallback role.
    // Not focusable.
    ImageView();

    // Replaces "ckv.image.fallback", the style of the blank fill around or
    // instead of the image and of the "[image]" text fallback. An override
    // set before attachment survives it; a change repaints.
    void set_role_override(ui::RoleId fallback_role) noexcept {
        if (fallback_role_ == fallback_role) return;
        fallback_role_ = fallback_role;
        invalidate();
    }

    // The image shown, shared with the caller rather than copied, and never
    // modified by the view. nullptr, or an image with no pixels, shows the
    // blank fill. Setting it repaints.
    void set_image(std::shared_ptr<const Image> image);
    const std::shared_ptr<const Image>& image() const noexcept { return image_; }

    // Fill the whole view instead of keeping the image's proportions.
    // For a picture this distorts it; for a deliberately generated
    // texture or gradient, filling is the point.
    void set_stretch_to_fill(bool stretch) noexcept;
    bool stretch_to_fill() const noexcept { return stretch_; }

    // The cells the image occupies inside `bounds()` right now, in the
    // view's own coordinates: the whole view when stretching, otherwise the
    // largest proportionate box (by the Application's reported cell metric,
    // or kAssumedCellPixels without one) centred in it. Empty when there is
    // no image or the view has no area.
    Rect image_anchor() const noexcept;

    // Fires for every mouse event over this view, carrying the full
    // MouseEvent — which already natively carries both the cell
    // position (always) and the pixel position (whenever the terminal
    // reports pixels, per D-018) — so "dual-space" here is simply never
    // discarding the pixel field on the way through, not a second event
    // path.
    std::function<void(const MouseEvent&)> on_click;

    // The image pixel `event` points at, mapped the way this view shows the
    // image: through image_anchor() and the attached Application's cell
    // metric (MouseEvent::image_pixel). Empty when the terminal reported no
    // pixel position, when there is no image, no Application or no measured
    // cell metric, and when the pointer is not over the image.
    std::optional<PixelPoint> image_pixel_at(const MouseEvent& event) const noexcept;

    void draw(scene::Painter& painter) override;
    // Reports every mouse event to on_click, when set, and consumes all of
    // them except the wheel: a picture does not scroll, so a wheel notch goes
    // on to the enclosing view that does, such as a ScrollViewport.
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    std::shared_ptr<const Image> image_;
    int raster_id_;
    bool stretch_ = false;

    ui::RoleId fallback_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
