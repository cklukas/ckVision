// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/canvas.hpp"

#include <algorithm>

namespace ckv::widgets {

namespace {
int next_raster_id() {
    static int counter = 1;
    return counter++;
}
}  // namespace

Canvas::Canvas() : raster_id_(next_raster_id()) {}

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
    cell_pixels_.width = std::max(0, cell_pixels.width);
    cell_pixels_.height = std::max(0, cell_pixels.height);
    derive_pixel_size_from_metrics_ = cell_pixels_.width > 0 && cell_pixels_.height > 0;
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

void Canvas::invalidate_content() {
    content_dirty_ = true;
    invalidate();
}

void Canvas::draw(scene::Painter& painter) {
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

    painter.draw_image(area, raster_id_, image_, [style, area](scene::Painter& fallback_painter) {
        fallback_painter.fill(area, Cell::from_grapheme(" ", style));
        if (area.width >= 8 && area.height >= 1) fallback_painter.draw_text(Point{0, 0}, "[canvas]", style);
    });
}

bool Canvas::on_mouse(const MouseEvent& event) {
    if (on_click) on_click(event);
    return true;
}

void Canvas::on_resized() { update_pixel_size_from_metrics(); }

}  // namespace ckv::widgets
