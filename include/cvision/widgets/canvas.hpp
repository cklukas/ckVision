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
// Canvas never queries terminal cell-pixel metrics itself — core/ui/widgets
// do not touch the environment directly (D-039). The owner injects current
// cell metrics with set_cell_metrics(); Canvas then derives its pixel backing
// from its cell bounds automatically on metric or widget resize.
#pragma once

#include <functional>
#include <memory>

#include "cvision/core/image.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

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

    std::function<void(const MouseEvent&)> on_click;  // see ImageView's own note: forwards the full dual-space event

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_resized() override;
    void on_attached() override;

private:
    void resize_backing_image(Size pixel_size);
    void update_pixel_size_from_metrics();

    std::shared_ptr<Image> image_;
    Size cell_pixels_{0, 0};
    bool derive_pixel_size_from_metrics_ = false;
    bool content_dirty_ = true;
    std::function<void(Image&)> draw_callback_;
    int raster_id_;

    ui::RoleId fallback_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
