// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Canvas: application draws RGBA at the widget's current pixel size,
// mandatory fallback painter, dual-space mouse events (the internal plans
// widgets.md — Canvas has NO baseline column; this implements its
// full beyond-baseline listed feature set). Built on the same
// Painter::draw_image foundation as ImageView, but the pixel buffer is
// owned by Canvas and repainted by an application-supplied callback
// rather than displaying a caller-owned Image.
//
// Canvas never queries terminal cell-pixel metrics itself — core/ui/widgets
// do not touch the environment directly (D-039). The owner injects current
// cell metrics with set_cell_metrics(); Canvas then derives its pixel backing
// from its cell bounds automatically on metric or widget resize.
#pragma once

#include <optional>

#include <functional>
#include <memory>

#include "cvision/core/image.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// The cell assumed when a terminal draws images but never reports a cell
// metric. Sizing a backing image from {0,0} yields no pixels at all, so the
// picture would silently vanish where the terminal could in fact show it.
inline constexpr PixelSize kAssumedCellPixels{10, 20};

// The cell box that shows an image of `image_pixels` at its true
// proportions on a terminal whose character cell is `cell_pixels`, never
// exceeding `max_cells` in either direction.
//
// Terminal cells are nothing like square, and how far from square varies
// enormously between terminals — 1:2 is typical, but 1:5 occurs. So a box
// chosen directly in cells silently assumes one cell shape, and distorts
// the picture by exactly the ratio between that assumption and the cell
// the reader actually has. Deriving the box from the metric is what keeps
// a picture's proportions independent of the terminal it is shown on.
//
// A `cell_pixels` with a non-positive extent falls back to
// kAssumedCellPixels, for the same reason Canvas does.
Size fit_image_cells(PixelSize image_pixels, PixelSize cell_pixels, Size max_cells) noexcept;

// Resolves its own theme role from context() once attached (M9
// WP-7, D-028): "ckv.canvas.fallback".
class Canvas : public ui::View {
public:
    // A canvas with no backing image (pixel size 0 by 0) until it is given a
    // pixel size or cell metrics; until then it draws a blank fill in the
    // fallback role. Not focusable.
    Canvas();

    // Replaces "ckv.canvas.fallback", the style of the blank fill and of the
    // text-only fallback. An override set before attachment survives it; a
    // change repaints.
    void set_role_override(ui::RoleId fallback_role) noexcept {
        if (fallback_role_ == fallback_role) return;
        fallback_role_ = fallback_role;
        invalidate();
    }

    // The backing image's size in pixels — the owner computes this when
    // manual sizing is desired. Reallocates the backing Image and marks
    // content stale; a no-op if the size is unchanged. Manual sizing disables
    // automatic metric-derived sizing until set_cell_metrics() is called again.
    void set_pixel_size(PixelSize size);
    PixelSize pixel_size() const noexcept;

    // The terminal's cell size in pixels. Setting it switches to automatic
    // sizing: the backing image becomes the canvas's cell bounds times this
    // metric, recomputed on every resize. A metric with a non-positive
    // extent is replaced by kAssumedCellPixels, and the canvas then adopts
    // the Application's measured cell metric by itself once one is reported
    // (checked at each paint and resize). cell_metrics() is the metric in
    // use, the assumed one included; 0 by 0 before any has been set.
    void set_cell_metrics(PixelSize cell_pixels);
    PixelSize cell_metrics() const noexcept { return cell_pixels_; }

    // Invoked with the backing Image whenever content needs repainting
    // (after set_pixel_size changes the size, or after
    // invalidate_content()) — the application draws whatever it wants
    // directly into the buffer.
    void set_draw_callback(std::function<void(Image&)> draw_callback);

    // Marks the current content stale without resizing — the next
    // draw() re-invokes the draw callback before presenting.
    void invalidate_content();

    // What this canvas draws where the terminal cannot show its picture,
    // called with the painter and the canvas's own area.
    //
    // The default names the widget — a placeholder, and deliberately a poor
    // one: a reader on a terminal without raster graphics is exactly the
    // reader who needs the picture's information most, and only the
    // application knows what that information is. A canvas showing a
    // measured value can say the value; one showing a diagram can say what
    // is in it. The fallback is mandatory in this design (every raster
    // region has one), so making it worth reading is the application's part
    // of that bargain.
    void set_fallback_painter(std::function<void(scene::Painter&, Rect)> fallback);

    // Called with every mouse event the canvas receives (presses, releases,
    // moves and wheel alike, not only clicks), unchanged, so both the cell
    // and any pixel position the host reported are available.
    std::function<void(const MouseEvent&)> on_click;  // see ImageView's own note: forwards the full dual-space event

    // The backing-image pixel `event` points at: the backing image fills the
    // canvas, so this maps the reported pixel position through the canvas's
    // cells and the attached Application's cell metric — the terminal's, not
    // the one set_cell_metrics() sized the backing image by
    // (MouseEvent::image_pixel). Empty when the terminal reported no pixel
    // position, when there is no backing image, no Application or no
    // measured cell metric, and when the pointer is not over the canvas.
    std::optional<PixelPoint> image_pixel_at(const MouseEvent& event) const noexcept;

    void draw(scene::Painter& painter) override;
    // Reports every mouse event to on_click, when set, and consumes all of
    // them except the wheel: a picture does not scroll, so a wheel notch goes
    // on to the enclosing view that does, such as a ScrollViewport.
    bool on_mouse(const MouseEvent& event) override;
    // A drawing surface, where which cell the pointer is on is the whole
    // question and an arrow tip is a poor way to answer it.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Crosshair;
    }
    void on_resized() override;
    void on_attached() override;

private:
    void resize_backing_image(PixelSize pixel_size);
    void update_pixel_size_from_metrics();

    std::shared_ptr<Image> image_;
    PixelSize cell_pixels_{0, 0};
    // True while this canvas is drawing at an assumed cell because the
    // terminal had not measured one yet; cleared once a real metric lands.
    bool awaiting_measured_metrics_ = false;
    void adopt_measured_cell_metrics();
    bool derive_pixel_size_from_metrics_ = false;
    bool content_dirty_ = true;
    std::function<void(Image&)> draw_callback_;
    std::function<void(scene::Painter&, Rect)> fallback_painter_;
    int raster_id_;

    ui::RoleId fallback_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
