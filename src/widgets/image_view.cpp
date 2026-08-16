// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/image_view.hpp"

namespace ckv::widgets {

ImageView::ImageView() : raster_id_(0) {}

void ImageView::on_attached() {
    if (fallback_role_ == ui::kInvalidRole)
        fallback_role_ = context().roles->find("ckv.image.fallback");
}

void ImageView::set_image(std::shared_ptr<const Image> image) {
    image_ = std::move(image);
    invalidate();
}

void ImageView::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(fallback_role_);
    const Rect area{0, 0, bounds().width, bounds().height};

    if (image_ == nullptr || image_->width() <= 0 || image_->height() <= 0) {
        painter.fill(area, Cell::from_grapheme(" ", style));
        return;
    }
    if (area.width <= 0 || area.height <= 0) return;  // draw_image requires a non-empty anchor

    painter.draw_image(area, raster_id_, image_, [style, area](scene::Painter& fallback_painter) {
        fallback_painter.fill(area, Cell::from_grapheme(" ", style));
        if (area.width >= 7 && area.height >= 1) fallback_painter.draw_text(Point{0, 0}, "[image]", style);
    });
}

bool ImageView::on_mouse(const MouseEvent& event) {
    if (on_click) on_click(event);
    return true;
}

}  // namespace ckv::widgets
