// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "widget_stage.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include "frame_svg.hpp"

namespace ckv::docgen {

namespace {
int written_count = 0;
}  // namespace

int screenshots_written() noexcept { return written_count; }

WidgetStage::WidgetStage(Size screen, StageGraphics graphics)
    : terminal_(screen, graphics == StageGraphics::On ? term::headless_sixel_profile()
                                                      : term::headless_no_graphics_profile()),
      app_(terminal_, clock_) {
    roles_ = ui::intern_standard_roles(app_.roles());
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);
    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));
}

ui::View& WidgetStage::window(std::string title, Rect bounds) {
    widgets::Window& frame = window_with_content(std::move(title), bounds, std::make_unique<ui::View>());
    return *frame.content();
}

ui::View& WidgetStage::dialog_window(std::string title, Rect bounds) {
    widgets::Window& frame =
        window_with_content(std::move(title), bounds, std::make_unique<ui::View>());
    frame.set_role_override(roles_.dialog_frame, roles_.dialog_background, roles_.dialog_frame,
                            roles_.dialog_background);
    return *frame.content();
}

widgets::Window& WidgetStage::window_with_content(std::string title, Rect bounds,
                                                  std::unique_ptr<ui::View> content) {
    auto frame = std::make_unique<widgets::Window>(std::move(title));
    frame->set_bounds(bounds);
    frame->set_content(std::move(content));
    window_ = desktop_->add_window(std::move(frame));
    content_ = window_->content();
    return *window_;
}

void WidgetStage::focus(ui::View* view) {
    if (view != nullptr && view->focusable()) app_.set_focus(view);
}

void WidgetStage::step() { app_.step(0); }

void WidgetStage::save(const std::filesystem::path& dir, std::string_view name) {
    save(dir, name, Rect{});
}

void WidgetStage::save(const std::filesystem::path& dir, std::string_view name, Rect crop) {
    const std::filesystem::path path = dir / (std::string(name) + ".svg");
    FrameSvgOptions options;
    options.crop = crop;
    std::ofstream out(path);
    out << render_virtual_display_svg(terminal_.display(), options);
    ++written_count;
    const Rect emitted = crop.empty() ? Rect{0, 0, terminal_.display().size().width,
                                             terminal_.display().size().height}
                                      : crop;
    std::fprintf(stderr, "wrote %s (%dx%d cells)\n", path.string().c_str(), emitted.width,
                 emitted.height);
}

void WidgetStage::save_window(const std::filesystem::path& dir, std::string_view name) {
    const Rect bounds = window_ != nullptr ? window_->bounds() : Rect{};
    // One cell of desktop on every side: the window's own cast shadow
    // falls one cell right and below, and a frame flush with the edge of
    // a figure reads as a cropping accident rather than as a window.
    save(dir, name, Rect{bounds.x - 1, bounds.y - 1, bounds.width + 3, bounds.height + 3});
}

void WidgetStage::save_active_window(const std::filesystem::path& dir, std::string_view name) {
    if (widgets::Window* active = desktop_->active_window()) window_ = active;
    save_window(dir, name);
}

Rect WidgetStage::content_to_screen(Rect content_rect) const {
    if (content_ == nullptr) return content_rect;
    const Rect area = content_->bounds();
    return Rect{area.x + content_rect.x, area.y + content_rect.y, content_rect.width,
                content_rect.height};
}

void WidgetStage::save_content(const std::filesystem::path& dir, std::string_view name,
                               Rect content_rect, int margin) {
    const Rect screen = content_to_screen(content_rect);
    save(dir, name,
         Rect{screen.x - margin, screen.y - margin, screen.width + 2 * margin,
              screen.height + 2 * margin});
}

}  // namespace ckv::docgen
