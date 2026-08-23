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
inline constexpr Size kAssumedCellPixels{10, 20};

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
Size fit_image_cells(Size image_pixels, Size cell_pixels, Size max_cells) noexcept;

// Resolves its own theme role from context() once attached (M9
// WP-7, D-028): "ckv.canvas.fallback".
class Canvas : public ui::View {
public:
    Canvas();

    void set_role_override(ui::RoleId fallback_role) noexcept { fallback_role_ = fallback_role; }

    // (width, height) in PIXELS (not cells) — the owner computes this
    // when manual sizing is desired. Reallocates the backing Image and marks
    // content stale; a no-op if the size is unchanged. Manual sizing disables
    // automatic metric-derived sizing until set_cell_metrics() is called again.
    void set_pixel_size(int width, int height);
    Size pixel_size() const noexcept;

    void set_cell_metrics(Size cell_pixels);
    Size cell_metrics() const noexcept { return cell_pixels_; }

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

    std::function<void(const MouseEvent&)> on_click;  // see ImageView's own note: forwards the full dual-space event

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    // A drawing surface, where which cell the pointer is on is the whole
    // question and an arrow tip is a poor way to answer it.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Crosshair;
    }
    void on_resized() override;
    void on_attached() override;

private:
    void resize_backing_image(Size pixel_size);
    void update_pixel_size_from_metrics();

    std::shared_ptr<Image> image_;
    Size cell_pixels_{0, 0};
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
