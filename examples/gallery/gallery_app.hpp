// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The object graph behind the ckVision Gallery example, factored out
// of main.cpp so tests/test_gallery_smoke.cpp can build and drive the
// exact same application headlessly (HeadlessTerminal) rather than
// testing a copy of the wiring.
#pragma once

#include <memory>

#include "cvision/core/image.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/status_line.hpp"

namespace ckv::widgets {
class Window;
class InputLine;
class ImageView;
}  // namespace ckv::widgets

namespace ckv::gallery {

class GalleryApp {
public:
    explicit GalleryApp(ui::Application& app);

    // Exposed for the smoke test to drive/inspect specific windows
    // without re-deriving the wiring.
    widgets::Desktop& desktop() noexcept { return *desktop_; }
    widgets::Window* controls_window() const noexcept { return controls_window_; }
    widgets::Window* image_window() const noexcept { return image_window_; }
    widgets::ImageView* image_view() const noexcept { return image_view_; }
    widgets::InputLine* name_input() const noexcept { return name_input_; }

private:
    void build_menu_bar();
    void build_controls_window();
    void build_image_window();
    std::shared_ptr<const Image> make_demo_gradient() const;

    ui::Application& app_;
    ui::StandardRoles roles_;

    widgets::Desktop* desktop_ = nullptr;
    widgets::StatusLine* status_ = nullptr;
    widgets::Window* controls_window_ = nullptr;
    widgets::Window* image_window_ = nullptr;
    widgets::ImageView* image_view_ = nullptr;
    widgets::InputLine* name_input_ = nullptr;
};

}  // namespace ckv::gallery
