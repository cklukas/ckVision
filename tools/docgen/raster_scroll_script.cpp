// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "raster_scroll_script.hpp"

#include <cstdint>
#include <utility>

#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/flow_view.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::docgen {

namespace {

// Red runs across and green runs down, so every row of the picture differs
// from its neighbours and a row that moved by the wrong amount shows.
std::shared_ptr<const Image> make_viewport_picture() {
    auto image = std::make_shared<Image>(PixelSize{38, 80});
    for (int y = 0; y < image->height(); ++y)
        for (int x = 0; x < image->width(); ++x)
            image->set_pixel(x, y,
                             Image::Rgba{static_cast<std::uint8_t>(x * 255 / (image->width() - 1)),
                                         static_cast<std::uint8_t>(y * 255 / (image->height() - 1)),
                                         96, 255});
    return image;
}

// Four-pixel squares over a vertical ramp: a picture unlike the other one.
std::shared_ptr<const Image> make_flow_picture() {
    auto image = std::make_shared<Image>(PixelSize{24, 24});
    for (int y = 0; y < image->height(); ++y) {
        for (int x = 0; x < image->width(); ++x) {
            const bool dark = ((x / 4) + (y / 4)) % 2 == 0;
            image->set_pixel(x, y,
                             Image::Rgba{static_cast<std::uint8_t>(dark ? 32 : 224),
                                         static_cast<std::uint8_t>(64 + y * 8),
                                         static_cast<std::uint8_t>(dark ? 160 : 48), 255});
        }
    }
    return image;
}

widgets::FlowText text(std::string words) {
    return widgets::FlowText{std::move(words), static_cast<Attr>(0), std::nullopt};
}

}  // namespace

RasterScrollStage::RasterScrollStage(term::Capabilities profile)
    : terminal(Size{48, 14}, profile), player(terminal, app, raster_scroll_script()) {
    const ui::StandardRoles roles = ui::intern_standard_roles(app.roles());
    widgets::ApplicationShell shell(app, {.theme = ui::make_classic_theme(app.roles(), roles)});

    // The viewport's content area is 19 x 10 cells beside its scrollbar; a
    // 38 x 80 picture across 19 columns of 3 x 6 pixels is 20 rows, so half
    // of it is below the edge before anything scrolls.
    viewport_picture_ = make_viewport_picture();
    auto picture = std::make_unique<widgets::ImageView>();
    picture->set_preferred_size(Size{19, 20});
    picture->set_image(viewport_picture_);
    auto viewport = std::make_unique<widgets::ScrollViewport>(ui::FocusPolicy::TabStop);
    viewport->set_horizontal_scrollbar_policy(widgets::ScrollbarPolicy::Hidden);
    viewport->set_bounds(Rect{0, 0, 20, 10});
    viewport->set_content(std::move(picture));
    viewport_ = viewport.get();
    auto viewport_window = std::make_unique<widgets::Window>("Viewport");
    viewport_window->set_bounds(Rect{1, 1, 22, 12});
    viewport_window->set_content(std::move(viewport));
    shell.desktop().add_window(std::move(viewport_window));

    // The flow's picture takes rows 2 to 7 of 16, so four rows of scrolling
    // carry its top past the view's first row.
    flow_picture_ = make_flow_picture();
    widgets::FlowDocument document;
    document.blocks.push_back(widgets::FlowBlock{{text("Inline picture:")}});
    document.blocks.push_back(widgets::FlowBlock{{widgets::FlowImage{flow_picture_, Size{12, 6}, "[picture]"}}});
    document.blocks.push_back(widgets::FlowBlock{
        {text("The rows below the picture keep the flow going, so there is room to "
              "scroll it past the top edge of the view.")}});
    auto flow = std::make_unique<widgets::FlowView>();
    flow->set_bounds(Rect{0, 0, 20, 10});
    flow->set_document(std::move(document));
    flow_ = flow.get();
    auto flow_window = std::make_unique<widgets::Window>("Flow");
    flow_window->set_bounds(Rect{25, 1, 22, 12});
    flow_window->set_content(std::move(flow));
    shell.desktop().add_window(std::move(flow_window));
}

std::vector<ScriptBeat> raster_scroll_script() {
    using Stage = RasterScrollStage;
    // The viewport scrolls one row by the keyboard once a click on its
    // picture has focused it, then a wheel notch's worth more by the wheel.
    std::vector<term::TerminalEvent> viewport_by_key{press(Stage::kViewportPictureCell),
                                                     release(Stage::kViewportPictureCell), key(Key::Down)};
    std::vector<term::TerminalEvent> focus_and_scroll{press(Stage::kFlowTextCell),
                                                      release(Stage::kFlowTextCell), key(Key::Down)};
    std::vector<term::TerminalEvent> scroll_further;
    for (int row = 0; row < Stage::kSecondScrollRows; ++row) scroll_further.push_back(key(Key::Down));
    return {
        {"initial", {}, "raster_scroll_initial"},
        {"viewport_1", std::move(viewport_by_key), "raster_scroll_viewport_1"},
        {"viewport_4", {wheel(Stage::kViewportPictureCell, MouseButton::WheelDown)}, "raster_scroll_viewport_4"},
        {"flow_1", std::move(focus_and_scroll), "raster_scroll_flow_1"},
        {"flow_4", std::move(scroll_further), "raster_scroll_flow_4"},
    };
}

}  // namespace ckv::docgen
