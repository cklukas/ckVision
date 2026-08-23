// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Hash-pinned pixel-plane checks for the bounded, independently implemented
// VT/Sixel protocol display. The first fixture is hand-authored protocol input;
// the second proves Presenter cropping through its actual byte stream.
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/term/virtual_display.hpp"

using namespace ckv;
using namespace ckv::term;

namespace {

struct PixelBounds {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    std::size_t opaque = 0;
};

PixelBounds opaque_bounds(const Image& image) {
    PixelBounds result{image.width(), image.height(), 0, 0, 0};
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y).a == 0) continue;
            result.left = std::min(result.left, x);
            result.top = std::min(result.top, y);
            result.right = std::max(result.right, x + 1);
            result.bottom = std::max(result.bottom, y + 1);
            ++result.opaque;
        }
    }
    return result;
}

std::uint64_t pixel_hash(const Image& image) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const Image::Rgba pixel = image.pixel(x, y);
            for (const std::uint8_t byte : {pixel.r, pixel.g, pixel.b, pixel.a}) {
                hash ^= byte;
                hash *= 1099511628211ULL;
            }
        }
    return hash;
}

std::string plane_manifest(std::string_view name, const VirtualDisplay& display) {
    const Image& image = display.raster_plane();
    const PixelBounds bounds = opaque_bounds(image);
    std::ostringstream text;
    text << name << " cells " << display.size().width << ' ' << display.size().height;
    text << " pixels " << image.width() << ' ' << image.height();
    text << " bounds " << bounds.left << ' ' << bounds.top << ' ' << bounds.right << ' ' << bounds.bottom;
    text << " opaque " << bounds.opaque;
    text << " hash " << std::hex << std::setfill('0') << std::setw(16) << pixel_hash(image) << '\n';
    return text.str();
}

std::string read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

std::string protocol_manifests() {
    std::string result;

    VirtualDisplay overwrite(Size{3, 1}, Size{4, 6});
    if (!overwrite.write("\x1B[1;1H\x1BP0;0;0q\"1;1;4;6#0;2;100;0;0~~~~\x1B\\")) return {};
    if (!overwrite.write("\x1B[1;1H\x1BP0;0;0q\"1;1;4;6#0;2;0;100;0~~\x1B\\")) return {};
    result += plane_manifest("opaque-overwrite", overwrite);

    VirtualDisplay scroll(Size{2, 2}, Size{4, 6});
    if (!scroll.write("\x1B[2;1H\x1BP0;0;0q\"1;1;1;6#0;2;100;0;0~\x1B\\\x1B[1S")) return {};
    result += plane_manifest("scroll", scroll);

    VirtualDisplay offscreen(Size{2, 1}, Size{4, 6});
    if (!offscreen.write("\x1B[1;2H\x1BP0;0;0q\"1;1;8;6#0;2;100;0;0~~~~~~~~\x1B\\")) return {};
    result += plane_manifest("offscreen-clip", offscreen);

    VirtualDisplay multiple(Size{2, 1}, Size{4, 6});
    if (!multiple.write("\x1B[1;1H\x1BP0;0;0q\"1;1;1;6#0;2;100;0;0~\x1B\\")) return {};
    if (!multiple.write("\x1B[1;2H\x1BP0;0;0q\"1;1;1;6#0;2;0;100;0~\x1B\\")) return {};
    result += plane_manifest("multiple-rasters", multiple);

    VirtualDisplay resize(Size{2, 1}, Size{4, 6});
    if (!resize.write("\x1B[1;1HA\x1B[1;1H\x1BP0;0;0q\"1;1;4;6#0;2;100;0;0~~~~\x1B\\")) return {};
    resize.resize(Size{3, 2});
    if (resize.frame().at(Point{0, 0}).grapheme() != " ") return {};
    result += plane_manifest("resize-clear", resize);
    return result;
}

std::string presenter_occlusion_manifest() {
    HeadlessTerminal terminal(Size{4, 1}, headless_sixel_profile());
    Presenter presenter(terminal);
    scene::Compositor compositor(Size{4, 1});
    scene::Surface background(Size{4, 1}, Cell::from_grapheme(" ", Style{}));
    scene::Surface raster_layer(Size{4, 1}, Cell::from_grapheme(" ", Style{}));
    auto image = std::make_shared<Image>(8, 18);
    for (int y = 0; y < image->height(); ++y)
        for (int x = 0; x < image->width(); ++x)
            image->set_pixel(x, y, x < 4 ? Image::Rgba{255, 0, 0, 255} : Image::Rgba{0, 0, 255, 255});

    scene::Painter painter(raster_layer, Rect{0, 0, 4, 1});
    painter.draw_image(Rect{0, 0, 4, 1}, 7, image, [](scene::Painter& fallback) {
        fallback.fill(Rect{0, 0, 4, 1}, Cell::from_grapheme("#", Style{}));
    });
    scene::Surface left_occluder(Size{1, 1}, Cell::from_grapheme("L", Style{}));
    scene::Surface right_occluder(Size{1, 1}, Cell::from_grapheme("R", Style{}));
    std::vector<scene::Layer> layers{{1, &raster_layer, Point{0, 0}, false},
                                     {2, &left_occluder, Point{0, 0}, false},
                                     {3, &right_occluder, Point{3, 0}, false}};
    compositor.compose(layers, background);
    CK_CHECK(compositor.visible_rasters().size() == 1);
    if (compositor.visible_rasters().size() != 1 ||
        !(compositor.visible_rasters().front().visible_rect == Rect{1, 0, 2, 1}))
        return {};

    // The two higher layers leave a two-cell middle slice of the four-cell
    // source anchor; Presenter must crop the source proportionally.
    presenter.present(compositor.frame().view(), CursorState{}, compositor.visible_rasters());
    if (terminal.written_bytes().find("\x1BP0;0;0q") == std::string_view::npos) return {};
    std::string result = plane_manifest("proportional-partial-occlusion", terminal.display());

    HeadlessTerminal moved_terminal(Size{4, 1}, headless_sixel_profile());
    Presenter moved_presenter(moved_terminal);
    scene::Surface moved_surface(Size{4, 1}, Cell::from_grapheme(" ", Style{}));
    auto red = std::make_shared<Image>(9, 18);
    for (int y = 0; y < red->height(); ++y)
        for (int x = 0; x < red->width(); ++x) red->set_pixel(x, y, Image::Rgba{255, 0, 0, 255});
    moved_presenter.present(moved_surface.view(), CursorState{},
                            {{8, Rect{0, 0, 1, 1}, Rect{0, 0, 1, 1}, red, true}});
    moved_presenter.present(moved_surface.view(), CursorState{},
                            {{8, Rect{2, 0, 1, 1}, Rect{2, 0, 1, 1}, red, true}});
    result += plane_manifest("move-stale-clear", moved_terminal.display());
    return result;
}

}  // namespace

CK_TEST(protocol_pixel_plane_goldens_cover_overwrite_scroll_clipping_multiple_rasters_and_resize) {
    CK_CHECK(protocol_manifests() == read_file("golden/virtual_display_protocol.visual"));
}

CK_TEST(presenter_pixel_plane_golden_covers_proportional_partial_occlusion_and_stale_free_move) {
    CK_CHECK(presenter_occlusion_manifest() == read_file("golden/presenter_partial_occlusion.visual"));
}
