// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Canvas: application draws RGBA at the widget's current pixel size,
// mandatory fallback painter, dual-space mouse events (plans/
// widgets.md — Canvas has NO baseline column; this implements its
// full beyond-baseline listed feature set). Built on the same
// Painter::draw_image foundation as ImageView, but the pixel buffer is
// owned by Canvas and repainted by an application-supplied callback
// rather than displaying a caller-owned Image.
//
// Scope note: Canvas does NOT query terminal cell-pixel metrics
// itself — core/ui/widgets never touch the environment directly
// (D-025's injected-impurity discipline extends here too), so
// "redraw on resize" means the OWNER calls set_pixel_size() with
// (cell box × cell metrics) computed from the term layer's own
// capability data whenever either changes; Canvas's own on_resized()
// only marks content stale, it cannot compute the new pixel size
// without that externally-owned information.
#pragma once

#include <functional>
#include <memory>

#include "cvision/core/image.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

class Canvas : public ui::View {
public:
    Canvas(const ui::Theme& theme, ui::RoleId fallback_role);

    // (width, height) in PIXELS (not cells) — the owner computes this
    // from the widget's cell box and the terminal's cell-pixel metrics.
    // Reallocates the backing Image and marks content stale; a no-op
    // if the size is unchanged.
    void set_pixel_size(int width, int height);

    // Invoked with the backing Image whenever content needs repainting
    // (after set_pixel_size changes the size, or after
    // invalidate_content()) — the application draws whatever it wants
    // directly into the buffer.
    void set_draw_callback(std::function<void(Image&)> draw_callback);

    // Marks the current content stale without resizing — the next
    // draw() re-invokes the draw callback before presenting.
    void invalidate_content();

    std::function<void(const MouseEvent&)> on_click;  // see ImageView's own note: forwards the full dual-space event

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;

private:
    std::shared_ptr<Image> image_;
    bool content_dirty_ = true;
    std::function<void(Image&)> draw_callback_;
    int raster_id_;

    const ui::Theme& theme_;
    ui::RoleId fallback_role_;
};

}  // namespace ckv::widgets
