// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Paired visual acceptance for the Gallery graphics path. These checks consume
// exact Terminal::write bytes through VirtualDisplay; they do not read the
// Gallery ImageView's source Image.
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/virtual_display.hpp"
#include "frame_svg.hpp"
#include "gallery_app.hpp"

using ckv::Image;
using ckv::ManualClock;
using ckv::Size;
using ckv::scene::image_content_hash;
using ckv::term::HeadlessTerminal;
using ckv::term::headless_no_graphics_profile;
using ckv::term::headless_sixel_profile;
using ckv::ui::Application;

namespace {

struct PixelBounds {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    std::size_t opaque_count = 0;
};

PixelBounds opaque_bounds(const Image& image) {
    PixelBounds bounds{image.width(), image.height(), 0, 0, 0};
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y).a == 0) continue;
            bounds.left = std::min(bounds.left, x);
            bounds.top = std::min(bounds.top, y);
            bounds.right = std::max(bounds.right, x + 1);
            bounds.bottom = std::max(bounds.bottom, y + 1);
            ++bounds.opaque_count;
        }
    }
    return bounds;
}

bool pixel_equals(Image::Rgba actual, Image::Rgba expected) noexcept {
    return actual.r == expected.r && actual.g == expected.g && actual.b == expected.b && actual.a == expected.a;
}

std::string read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

bool frame_contains(const ckv::FrameView& frame, std::string_view needle) {
    for (int y = 0; y < frame.size().height; ++y) {
        std::string row;
        for (int x = 0; x < frame.size().width; ++x) {
            const ckv::Cell& cell = frame.at(ckv::Point{x, y});
            if (!cell.is_continuation()) row += cell.grapheme();
        }
        if (row.find(needle) != std::string::npos) return true;
    }
    return false;
}

std::string visual_manifest(std::string_view profile, const HeadlessTerminal& term) {
    const PixelBounds bounds = opaque_bounds(term.display().raster_plane());
    std::ostringstream manifest;
    manifest << "profile " << profile << '\n';
    manifest << "cell-pixels " << term.display().cell_pixels().width << ' '
             << term.display().cell_pixels().height << '\n';
    manifest << "pixels " << term.display().pixel_size().width << ' ' << term.display().pixel_size().height << '\n';
    manifest << "bounds " << bounds.left << ' ' << bounds.top << ' ' << bounds.right << ' ' << bounds.bottom << '\n';
    manifest << "opaque " << bounds.opaque_count << '\n';
    manifest << "hash " << image_content_hash(term.display().raster_plane()) << '\n';
    return manifest.str();
}

}  // namespace

CK_TEST(gallery_sixel_visual_golden_is_decoded_from_the_presented_byte_stream) {
    HeadlessTerminal sixel_term(Size{80, 24}, headless_sixel_profile());
    ManualClock sixel_clock;
    Application sixel_app(sixel_term, sixel_clock);
    ckv::gallery::GalleryApp sixel_gallery(sixel_app);
    sixel_app.step(0);

    const std::string presented(sixel_term.written_bytes());
    CK_CHECK(presented.find("\x1B" "P") != std::string::npos);
    CK_CHECK(visual_manifest("sixel", sixel_term) == read_file("golden/gallery_sixel.visual"));
    CK_CHECK(!frame_contains(sixel_term.display().frame(), "[image]"));
    // Where the picture landed and how big it is, asked of the view rather
    // than written down: it fills the cells it occupies, so pinning a pixel
    // size would pin the source image's dimensions instead of the contract.
    const ckv::Rect view_abs = sixel_gallery.image_view()->absolute_bounds();
    const ckv::Rect anchor = sixel_gallery.image_view()->image_anchor();
    const ckv::Size cell = sixel_term.display().cell_pixels();
    const int px_x = (view_abs.x + anchor.x) * cell.width;
    const int px_y = (view_abs.y + anchor.y) * cell.height;
    const int px_w = anchor.width * cell.width;
    const int px_h = anchor.height * cell.height;
    const std::shared_ptr<const Image>& source = sixel_gallery.image_view()->image();
    // Corners: nearest-neighbour scaling maps them exactly, so these also
    // catch an image drawn flipped, offset, or from the wrong source.
    const ckv::Image& plane = sixel_term.display().raster_plane();
    CK_CHECK(pixel_equals(plane.pixel(px_x, px_y), source->pixel(0, 0)));
    CK_CHECK(pixel_equals(plane.pixel(px_x + px_w - 1, px_y + px_h - 1),
                          source->pixel(source->width() - 1, source->height() - 1)));
    // The source is twice as wide as it is tall and stays that way.
    CK_CHECK(px_w == 2 * px_h);
    const std::string sixel_svg = ckv::docgen::render_virtual_display_svg(sixel_term.display());
    CK_CHECK(sixel_svg.find("id=\"raster-plane\"") != std::string::npos);
    CK_CHECK(sixel_svg.find("<image x=\"" + std::to_string(px_x) + "\" y=\"" + std::to_string(px_y) +
                            "\" width=\"" + std::to_string(px_w) + "\" height=\"" +
                            std::to_string(px_h) + "\"") != std::string::npos);
    CK_CHECK(sixel_svg.find("[image]") == std::string::npos);

    // This second decoder is deliberately fed only Terminal::write bytes.
    // Corrupting that stream must reject the visual capture without changing
    // GalleryApp's still-live source Image.
    ckv::term::VirtualDisplay replay(Size{80, 24}, sixel_term.display().cell_pixels());
    CK_CHECK(replay.write(presented));
    CK_CHECK(image_content_hash(replay.raster_plane()) == image_content_hash(sixel_term.display().raster_plane()));
    std::string corrupted = presented;
    const std::size_t dcs = corrupted.find("\x1B" "P");
    const std::size_t sixel = corrupted.find('q', dcs);
    CK_CHECK(dcs != std::string::npos && sixel != std::string::npos);
    if (dcs != std::string::npos && sixel != std::string::npos) {
        corrupted[sixel + 1] = '\x01';
        ckv::term::VirtualDisplay rejected(Size{80, 24}, sixel_term.display().cell_pixels());
        CK_CHECK(!rejected.write(corrupted));
        CK_CHECK(!rejected.valid());
    }
    (void)sixel_gallery;
}

CK_TEST(gallery_no_graphics_visual_golden_keeps_the_cell_fallback_and_no_raster_plane) {
    HeadlessTerminal fallback_term(Size{80, 24}, headless_no_graphics_profile());
    ManualClock fallback_clock;
    Application fallback_app(fallback_term, fallback_clock);
    ckv::gallery::GalleryApp fallback_gallery(fallback_app);
    fallback_app.step(0);

    CK_CHECK(fallback_term.written_bytes().find("\x1B" "P") == std::string::npos);
    CK_CHECK(visual_manifest("no-graphics", fallback_term) == read_file("golden/gallery_no_graphics.visual"));
    CK_CHECK(frame_contains(fallback_term.display().frame(), "[image]"));
    CK_CHECK(!fallback_term.display().has_raster_pixels());
    const std::string fallback_svg = ckv::docgen::render_virtual_display_svg(fallback_term.display());
    const std::size_t raster_group = fallback_svg.find("id=\"raster-plane\"");
    CK_CHECK(raster_group != std::string::npos);
    CK_CHECK(fallback_svg.find("<image", raster_group) == std::string::npos);
    CK_CHECK(fallback_svg.find(">[</text>") != std::string::npos);
    (void)fallback_gallery;
}

CK_TEST(gallery_capability_transitions_represent_no_graphics_to_sixel_and_back) {
    HeadlessTerminal term(Size{80, 24}, headless_no_graphics_profile());
    ManualClock clock;
    Application app(term, clock);
    ckv::gallery::GalleryApp gallery(app);

    app.step(0);
    CK_CHECK(visual_manifest("no-graphics", term) == read_file("golden/gallery_no_graphics.visual"));
    CK_CHECK(frame_contains(term.display().frame(), "[image]"));

    term.inject_capability_change(headless_sixel_profile());
    app.step(0);
    CK_CHECK(visual_manifest("sixel", term) == read_file("golden/gallery_sixel.visual"));
    CK_CHECK(!frame_contains(term.display().frame(), "[image]"));

    term.inject_capability_change(headless_no_graphics_profile());
    app.step(0);
    CK_CHECK(visual_manifest("no-graphics", term) == read_file("golden/gallery_no_graphics.visual"));
    CK_CHECK(frame_contains(term.display().frame(), "[image]"));
    CK_CHECK(!term.display().has_raster_pixels());
    (void)gallery;
}
