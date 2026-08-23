// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <memory>

#include "cvision/core/image.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {
class Canvas;
class ImageView;
class TabControl;
class Window;
}  // namespace ckv::widgets

namespace ckv::graphics {

class GraphicsApp {
public:
    explicit GraphicsApp(ui::Application& app);

    widgets::Desktop& desktop() noexcept { return *desktop_; }
    widgets::Window* window() const noexcept { return window_; }
    widgets::TabControl* tabs() const noexcept { return tabs_; }
    widgets::ImageView* image_view() const noexcept { return image_view_; }
    widgets::Canvas* canvas() const noexcept { return canvas_; }
    int image_clicks() const noexcept { return image_clicks_; }
    int canvas_clicks() const noexcept { return canvas_clicks_; }
    const std::shared_ptr<const Image>& demo_image() const noexcept { return demo_image_; }

private:
    void build_chrome();
    void build_window();
    std::shared_ptr<const Image> make_demo_image() const;

    ui::Application& app_;
    ui::StandardRoles roles_;

    widgets::Desktop* desktop_ = nullptr;
    widgets::Window* window_ = nullptr;
    widgets::TabControl* tabs_ = nullptr;
    widgets::ImageView* image_view_ = nullptr;
    widgets::Canvas* canvas_ = nullptr;
    int image_clicks_ = 0;
    int canvas_clicks_ = 0;
    std::shared_ptr<const Image> demo_image_;
};

}  // namespace ckv::graphics
