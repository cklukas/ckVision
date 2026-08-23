// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/canvas.hpp"

#include "cvision/ui/application.hpp"

#include <algorithm>

namespace ckv::widgets {

Size fit_image_cells(Size image_pixels, Size cell_pixels, Size max_cells) noexcept {
    if (image_pixels.width <= 0 || image_pixels.height <= 0) return Size{};
    if (max_cells.width <= 0 || max_cells.height <= 0) return Size{};
    if (cell_pixels.width <= 0 || cell_pixels.height <= 0) cell_pixels = kAssumedCellPixels;
    // The image's width:height, expressed in cells rather than pixels: one
    // cell of width buys cell_pixels.width pixels, one cell of height buys
    // cell_pixels.height of them.
    const double aspect_in_cells = static_cast<double>(image_pixels.width) * cell_pixels.height /
                                   (static_cast<double>(image_pixels.height) * cell_pixels.width);
    // Take the full width allowed, then give back whatever the height cap
    // demands — so the box is as large as it may be and still proportionate.
    int cols = max_cells.width;
    int rows = static_cast<int>(cols / aspect_in_cells + 0.5);
    if (rows > max_cells.height) {
        rows = max_cells.height;
        cols = static_cast<int>(rows * aspect_in_cells + 0.5);
    }
    return Size{std::clamp(cols, 1, max_cells.width), std::clamp(rows, 1, max_cells.height)};
}

Canvas::Canvas() : raster_id_(0) {}

void Canvas::on_attached() {
    if (fallback_role_ == ui::kInvalidRole)
        fallback_role_ = context().roles->find("ckv.canvas.fallback");
}

void Canvas::set_pixel_size(int width, int height) {
    derive_pixel_size_from_metrics_ = false;
    resize_backing_image(Size{width, height});
}

Size Canvas::pixel_size() const noexcept {
    return image_ != nullptr ? Size{image_->width(), image_->height()} : Size{};
}

void Canvas::set_cell_metrics(Size cell_pixels) {
    // A terminal that draws images but never answered the cell-metric probe
    // reports {0,0}. Sizing a backing image from that yields no pixels at
    // all — the picture silently disappears where the terminal could in
    // fact have shown it. Fall back to a common modern cell so the image
    // renders; a host that cares about exactness supplies a measured metric
    // (that is what a calibration control is for).
    static constexpr Size kAssumedCell = kAssumedCellPixels;
    // Remember whether a real metric was supplied. The probe that measures
    // the cell answers milliseconds AFTER a dialog is built, so a canvas
    // created in between would otherwise keep the assumed cell forever and
    // draw an image sized for a cell this terminal does not have — visibly
    // too small, in exactly the proportion the two cells differ by.
    awaiting_measured_metrics_ = cell_pixels.width <= 0 || cell_pixels.height <= 0;
    if (awaiting_measured_metrics_) cell_pixels = kAssumedCell;
    cell_pixels_.width = std::max(0, cell_pixels.width);
    cell_pixels_.height = std::max(0, cell_pixels.height);
    derive_pixel_size_from_metrics_ = cell_pixels_.width > 0 && cell_pixels_.height > 0;
    update_pixel_size_from_metrics();
}

void Canvas::adopt_measured_cell_metrics() {
    if (!awaiting_measured_metrics_ || context().app == nullptr) return;
    const Size measured = context().app->terminal_cell_pixels();
    if (measured.width <= 0 || measured.height <= 0) return;
    awaiting_measured_metrics_ = false;
    if (measured == cell_pixels_) return;
    cell_pixels_ = measured;
    update_pixel_size_from_metrics();
}

void Canvas::resize_backing_image(Size pixel_size) {
    if (image_ != nullptr && image_->width() == pixel_size.width && image_->height() == pixel_size.height) return;
    image_ = (pixel_size.width > 0 && pixel_size.height > 0)
                 ? std::make_shared<Image>(pixel_size.width, pixel_size.height)
                 : nullptr;
    content_dirty_ = true;
    invalidate();
}

void Canvas::update_pixel_size_from_metrics() {
    if (!derive_pixel_size_from_metrics_) return;
    resize_backing_image(Size{bounds().width * cell_pixels_.width, bounds().height * cell_pixels_.height});
}

void Canvas::set_draw_callback(std::function<void(Image&)> draw_callback) {
    draw_callback_ = std::move(draw_callback);
    content_dirty_ = true;
    invalidate();
}

void Canvas::set_fallback_painter(std::function<void(scene::Painter&, Rect)> fallback) {
    fallback_painter_ = std::move(fallback);
    invalidate();
}

void Canvas::invalidate_content() {
    content_dirty_ = true;
    invalidate();
}

void Canvas::draw(scene::Painter& painter) {
    // The probe answers after the first frames; adopt the real cell as soon
    // as it exists rather than keeping the assumed one for this canvas's
    // whole life.
    adopt_measured_cell_metrics();
    const Style style = context().theme->resolve(fallback_role_);
    const Rect area{0, 0, bounds().width, bounds().height};

    if (image_ == nullptr || image_->width() <= 0 || image_->height() <= 0) {
        painter.fill(area, Cell::from_grapheme(" ", style));
        return;
    }
    if (area.width <= 0 || area.height <= 0) return;

    if (content_dirty_ && draw_callback_) {
        draw_callback_(*image_);
        content_dirty_ = false;
    }

    painter.draw_image(area, raster_id_, image_,
                       [this, style, area](scene::Painter& fallback_painter) {
                           fallback_painter.fill(area, Cell::from_grapheme(" ", style));
                           if (fallback_painter_) {
                               fallback_painter_(fallback_painter, area);
                               return;
                           }
                           if (area.width >= 8 && area.height >= 1)
                               fallback_painter.draw_text(Point{0, 0}, "[canvas]", style);
                       });
}

bool Canvas::on_mouse(const MouseEvent& event) {
    if (on_click) on_click(event);
    return true;
}

void Canvas::on_resized() {
    adopt_measured_cell_metrics();
    update_pixel_size_from_metrics();
}

}  // namespace ckv::widgets
