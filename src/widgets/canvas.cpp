// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/canvas.hpp"

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
    if (image_ != nullptr && image_->width() == width && image_->height() == height) return;
    image_ = (width > 0 && height > 0) ? std::make_shared<Image>(width, height) : nullptr;
    content_dirty_ = true;
    invalidate();
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

}  // namespace ckv::widgets
