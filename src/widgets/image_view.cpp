// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/image_view.hpp"

#include "cvision/ui/application.hpp"
#include "cvision/widgets/canvas.hpp"

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

void ImageView::set_stretch_to_fill(bool stretch) noexcept {
    if (stretch_ == stretch) return;
    stretch_ = stretch;
    invalidate();
}

Rect ImageView::image_anchor() const noexcept {
    const Rect area{0, 0, bounds().width, bounds().height};
    if (image_ == nullptr || image_->width() <= 0 || image_->height() <= 0) return Rect{};
    if (area.width <= 0 || area.height <= 0) return Rect{};
    if (stretch_) return area;
    const Size cell = context().app != nullptr ? context().app->terminal_cell_pixels() : Size{};
    const Size box = fit_image_cells(Size{image_->width(), image_->height()}, cell,
                                     Size{area.width, area.height});
    if (box.width <= 0 || box.height <= 0) return area;
    // Centred: leftover space belongs equally to both sides, and an image
    // pinned to a corner reads as a mistake rather than as a choice.
    return Rect{area.x + (area.width - box.width) / 2, area.y + (area.height - box.height) / 2,
                box.width, box.height};
}

void ImageView::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(fallback_role_);
    const Rect area{0, 0, bounds().width, bounds().height};

    if (image_ == nullptr || image_->width() <= 0 || image_->height() <= 0) {
        painter.fill(area, Cell::from_grapheme(" ", style));
        return;
    }
    if (area.width <= 0 || area.height <= 0) return;  // draw_image requires a non-empty anchor

    const Rect anchor = image_anchor();
    if (anchor.empty()) return;
    // Whatever the image does not occupy is still this view's to paint.
    if (!(anchor == area)) painter.fill(area, Cell::from_grapheme(" ", style));
    painter.draw_image(anchor, raster_id_, image_, [style, anchor](scene::Painter& fallback_painter) {
        fallback_painter.fill(anchor, Cell::from_grapheme(" ", style));
        if (anchor.width >= 7 && anchor.height >= 1)
            fallback_painter.draw_text(Point{anchor.x, anchor.y}, "[image]", style);
    });
}

bool ImageView::on_mouse(const MouseEvent& event) {
    if (on_click) on_click(event);
    return true;
}

}  // namespace ckv::widgets
