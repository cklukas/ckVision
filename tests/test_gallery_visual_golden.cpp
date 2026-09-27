// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Paired visual acceptance for the Gallery graphics path. These checks consume
// exact Terminal::write bytes through VirtualDisplay; they do not read the
// Gallery ImageView's source Image.
//
// The image demo (the roadmap M3, D-081) and the scheme switch (D-082) are
// event scripts over the shipped Gallery (tools/docgen/gallery_script.hpp),
// played on a fixed-metric Sixel profile and on NoGraphics in lockstep. Each
// beat is compared with the files generate_gallery_goldens pinned from the
// same script: its symbolic scene, which both profiles compose byte for
// byte, its decoded cells and pixel plane under Sixel, and its decoded
// cells under NoGraphics.
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/virtual_display.hpp"
#include "cvision/widgets/menu.hpp"
#include "frame_svg.hpp"
#include "gallery_app.hpp"
#include "gallery_script.hpp"
#include "plane_capture.hpp"
#include "raster_checks.hpp"

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

// The cells the Gallery's picture viewport shows its content in.
ckv::Rect viewport_clip(const ckv::gallery::GalleryApp& gallery) {
    const ckv::ui::View* const frame = gallery.picture_viewport()->content()->parent();
    return frame != nullptr ? frame->absolute_bounds() : ckv::Rect{};
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
    // Where the picture landed and how much of it shows, asked of the views
    // rather than written down: it fills the cells it occupies, so pinning a
    // pixel size would pin the source image's dimensions instead of the
    // contract.
    const ckv::Rect view_abs = sixel_gallery.image_view()->absolute_bounds();
    const ckv::Rect anchor = sixel_gallery.image_view()->image_anchor();
    const ckv::Rect picture{view_abs.x + anchor.x, view_abs.y + anchor.y, anchor.width, anchor.height};
    const ckv::Rect shown = picture.intersected(viewport_clip(sixel_gallery));
    // The picture is twice as tall as its viewport: the top half shows.
    CK_CHECK(shown.y == picture.y && shown.height * 2 == picture.height);
    const ckv::PixelSize cell = sixel_term.display().cell_pixels();
    const int px_x = shown.x * cell.width;
    const int px_y = shown.y * cell.height;
    const int px_w = shown.width * cell.width;
    const int px_h = shown.height * cell.height;
    const std::shared_ptr<const Image>& source = sixel_gallery.image_view()->image();
    // The top corners: nearest-neighbour scaling maps them exactly, and
    // their colours survive a Sixel register exactly, so these also catch an
    // image drawn flipped, offset, or from the wrong source.
    const ckv::Image& plane = sixel_term.display().raster_plane();
    const int picture_px_w = picture.width * cell.width;
    const int picture_px_h = picture.height * cell.height;
    CK_CHECK(pixel_equals(plane.pixel(px_x, px_y), source->pixel(0, 0)));
    CK_CHECK(pixel_equals(plane.pixel(px_x + px_w - 1, px_y), source->pixel(source->width() - 1, 0)));
    // The lowest row shown is the middle of the picture, not its bottom:
    // green, which runs down it, is past the top's and short of the bottom's.
    const Image::Rgba lowest = plane.pixel(px_x, px_y + px_h - 1);
    CK_CHECK(lowest.g > plane.pixel(px_x, px_y).g && lowest.g < source->pixel(0, source->height() - 1).g);
    // The picture keeps the source's proportions.
    CK_CHECK(picture_px_w * source->height() == picture_px_h * source->width());
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

namespace {

using ckv::docgen::GalleryStage;
using ckv::docgen::ScriptBeat;

constexpr ckv::PixelSize kCell = ckv::docgen::kScriptCellPixels;

// One Gallery script, played on both profiles at once.
struct GalleryPair {
    explicit GalleryPair(std::vector<ScriptBeat> (*script)())
        : sixel(ckv::docgen::script_sixel_profile(), script()),
          fallback(ckv::docgen::script_no_graphics_profile(), script()) {}

    GalleryStage sixel;
    GalleryStage fallback;

    // Plays both runs through the beat called `name`, and answers whether
    // the frame it ends on is exactly the one its files pin and the
    // NoGraphics run has never written a Sixel.
    bool plays_to_pinned(std::string_view name) {
        const ScriptBeat* const beat = sixel.player.play_to(name);
        if (beat == nullptr || fallback.player.play_to(name) == nullptr) return false;
        return ckv::docgen::matches_pinned(
                   ckv::docgen::capture_paired_raster_beat(beat->golden, sixel.app, sixel.terminal,
                                                           fallback.app, fallback.terminal),
                   "golden") &&
               fallback.terminal.written_bytes().find("\x1BP") == std::string_view::npos;
    }

    const std::vector<ckv::RasterSlice>& slices() const { return sixel.app.compositor().visible_rasters(); }
    const Image& plane() const { return sixel.terminal.display().raster_plane(); }
    const Image* picture() const { return sixel.gallery.image_view()->image().get(); }
    ckv::Rect anchor() const {
        return ckv::docgen::full_anchor_of(slices(), picture()).value_or(ckv::Rect{});
    }
};

ckv::Rect moved_up(ckv::Rect rect, int rows) { return ckv::Rect{rect.x, rect.y - rows, rect.width, rect.height}; }

}  // namespace

CK_TEST(the_image_demo_scrolls_its_picture_by_exactly_the_rows_scrolled) {
    GalleryPair pair(ckv::docgen::gallery_picture_script);
    const ckv::widgets::ScrollViewport& viewport = *pair.sixel.gallery.picture_viewport();
    const ckv::Rect clip = viewport_clip(pair.sixel.gallery);

    CK_CHECK(pair.plays_to_pinned("initial"));
    const ckv::Rect initial = pair.anchor();
    const Image initial_plane = pair.plane();
    const std::vector<ckv::RasterSlice> initial_slices = pair.slices();
    CK_CHECK(initial.y == clip.y && initial.bottom() > clip.bottom());  // cut off below
    CK_CHECK(ckv::docgen::stray_pixels(pair.plane(), pair.slices(), kCell) == 0);

    // A click on the picture focuses the viewport around it; Down scrolls a row.
    CK_CHECK(pair.plays_to_pinned("scrolled_1"));
    CK_CHECK(viewport.scroll_y() == 1);
    CK_CHECK(pair.anchor() == moved_up(initial, 1));
    CK_CHECK(pair.slices().size() == 1U && pair.slices().front().visible_rect == clip);
    CK_CHECK(ckv::docgen::moved_pixels_compared(initial_plane, initial, initial_slices, pair.plane(),
                                                pair.anchor(), pair.slices(), kCell) > 0);
    CK_CHECK(ckv::docgen::stray_pixels(pair.plane(), pair.slices(), kCell) == 0);
    const Image one_plane = pair.plane();
    const std::vector<ckv::RasterSlice> one_slices = pair.slices();
    const ckv::Rect one = pair.anchor();

    // A click focuses the viewport, and PageDown moves a page further.
    CK_CHECK(pair.plays_to_pinned("scrolled_page"));
    const int page = viewport.scroll_y() - 1;
    CK_CHECK(page > 1);
    CK_CHECK(pair.anchor() == moved_up(initial, 1 + page));
    CK_CHECK(pair.slices().size() == 1U && pair.slices().front().visible_rect == clip);
    CK_CHECK(ckv::docgen::moved_pixels_compared(one_plane, one, one_slices, pair.plane(), pair.anchor(),
                                                pair.slices(), kCell) > 0);
    CK_CHECK(ckv::docgen::stray_pixels(pair.plane(), pair.slices(), kCell) == 0);
}

CK_TEST(the_view_menu_occludes_the_scrolled_picture_and_closing_it_leaves_no_stale_pixel) {
    GalleryPair pair(ckv::docgen::gallery_picture_script);
    CK_CHECK(pair.plays_to_pinned("scrolled_page"));
    const Image uncovered = pair.plane();
    const ckv::Rect anchor = pair.anchor();

    // View > Scheme is open: the dropdown and its submenu, whose frame and
    // shadow fall across the picture's top rows.
    CK_CHECK(pair.plays_to_pinned("menu"));
    CK_CHECK(pair.sixel.gallery.desktop().popups().size() == 2U);
    const std::vector<ckv::RasterSlice> covered = ckv::docgen::slices_of(pair.slices(), pair.picture());
    CK_CHECK(covered.size() > 1U);
    CK_CHECK(pair.anchor() == anchor);  // occlusion cuts the picture; it does not move it
    CK_CHECK(std::any_of(covered.begin(), covered.end(),
                         [](const ckv::RasterSlice& slice) { return slice.shadow.has_value(); }));
    // No pixel under the menu, the rest unchanged, and the shadowed part
    // darkened by the scheme's shadow.
    CK_CHECK(ckv::docgen::stray_pixels(pair.plane(), pair.slices(), kCell) == 0);
    CK_CHECK(ckv::docgen::covered_pixels_compared(uncovered, pair.plane(), covered, kCell) > 0);
    CK_CHECK(image_content_hash(pair.plane()) != image_content_hash(uncovered));

    // Closing both menus puts back exactly the frame they opened over, which
    // is what the script pins this beat with.
    CK_CHECK(pair.plays_to_pinned("closed"));
    CK_CHECK(pair.sixel.gallery.desktop().popups().empty());
    CK_CHECK(image_content_hash(pair.plane()) == image_content_hash(uncovered));
}

CK_TEST(the_gallery_shows_every_built_in_scheme_chosen_through_view_scheme) {
    GalleryPair pair(ckv::docgen::gallery_scheme_script);
    pair.sixel.app.step(0);
    const std::string classic_first = ckv::docgen::capture_scene(pair.sixel.app);

    std::vector<std::string> scenes;
    for (const char* scheme : {"dark", "light", "mono", "classic"}) {
        CK_CHECK(pair.plays_to_pinned(scheme));
        // The choice closes the menu and repaints everything in the scheme.
        CK_CHECK(pair.sixel.gallery.desktop().popups().empty());
        CK_CHECK(!pair.slices().empty());
        CK_CHECK(ckv::docgen::stray_pixels(pair.plane(), pair.slices(), kCell) == 0);
        scenes.push_back(ckv::docgen::capture_scene(pair.sixel.app));
    }
    // Four schemes, four different frames; and Classic chosen again is the
    // frame the Gallery opened with.
    for (std::size_t first = 0; first < scenes.size(); ++first)
        for (std::size_t second = first + 1; second < scenes.size(); ++second)
            CK_CHECK(scenes[first] != scenes[second]);
    CK_CHECK(scenes.back() == classic_first);
}

CK_TEST(view_scheme_marks_the_scheme_that_is_showing) {
    GalleryStage stage(ckv::docgen::script_no_graphics_profile(), ckv::docgen::gallery_scheme_script());
    CK_CHECK(stage.player.play_to("light") != nullptr);
    // Open View > Scheme again and read the submenu's marks.
    stage.terminal.inject_event(ckv::docgen::alt("v"));
    stage.app.step(0);
    stage.terminal.inject_event(ckv::docgen::key(ckv::Key::Right));
    stage.app.step(0);
    const std::vector<ckv::ui::View*>& popups = stage.gallery.desktop().popups();
    CK_CHECK(popups.size() == 2U);
    if (popups.size() != 2U) return;
    const auto* const schemes = dynamic_cast<const ckv::widgets::DropdownMenu*>(popups.back());
    CK_CHECK(schemes != nullptr);
    if (schemes == nullptr) return;
    std::vector<ckv::widgets::MenuMark> marks;
    for (const ckv::widgets::MenuItem& item : schemes->items()) marks.push_back(item.mark());
    CK_CHECK(marks == (std::vector<ckv::widgets::MenuMark>{
                          ckv::widgets::MenuMark::RadioOff, ckv::widgets::MenuMark::RadioOff,
                          ckv::widgets::MenuMark::RadioOn, ckv::widgets::MenuMark::RadioOff}));
}
