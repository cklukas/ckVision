// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// ImageView: shows an Image scaled to its cell box, mandatory fallback
// content, dual-space (cell + pixel) mouse events where the terminal
// reports pixels (plans/widgets.md — Image view has NO baseline
// column; this implements its full beyond-baseline listed feature
// set). Built directly on Painter::draw_image, which already makes a
// fallback mandatory by construction (D-017) — this widget's own
// fallback is a simple placeholder fill+glyph; a real application can
// swap in anything by subclassing or by requesting a richer fallback
// hook if a concrete need arises.
//
// Scope note: "scaled to its cell box" means the underlying Image is
// handed to draw_image at the widget's own cell-box anchor — actual
// pixel scaling/resampling is a term-layer/presenter concern per
// ARCHITECTURE.md §7, not something this widget does itself.
#pragma once

#include <functional>
#include <memory>

#include "cvision/core/image.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// Resolves its own theme role from context() once attached (M9
// WP-7, D-028): "ckv.image.fallback".
class ImageView : public ui::View {
public:
    ImageView();

    void set_role_override(ui::RoleId fallback_role) noexcept { fallback_role_ = fallback_role; }

    void set_image(std::shared_ptr<const Image> image);
    const std::shared_ptr<const Image>& image() const noexcept { return image_; }

    // Fires for every mouse event over this view, carrying the full
    // MouseEvent — which already natively carries both the cell
    // position (always) and the pixel position (whenever the terminal
    // reports pixels, per D-018) — so "dual-space" here is simply never
    // discarding the pixel field on the way through, not a second event
    // path.
    std::function<void(const MouseEvent&)> on_click;

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    std::shared_ptr<const Image> image_;
    int raster_id_;

    ui::RoleId fallback_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
