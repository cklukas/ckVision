// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/geometry.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "cvision/core/image.hpp"
#include "cvision/core/terminal_subsession.hpp"
#include "cvision/term/capabilities.hpp"
#include "cvision/term/sixel_decoder.hpp"
#include "cvision/term/virtual_display.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/canvas.hpp"
#include "cvision/widgets/terminal_view.hpp"

CK_TEST(rect_basic_accessors) {
    const ckv::Rect r{2, 3, 10, 5};
    CK_CHECK(r.left() == 2);
    CK_CHECK(r.top() == 3);
    CK_CHECK(r.right() == 12);
    CK_CHECK(r.bottom() == 8);
    CK_CHECK(!r.empty());
}

CK_TEST(rect_empty_when_nonpositive) {
    CK_CHECK((ckv::Rect{0, 0, 0, 5}).empty());
    CK_CHECK((ckv::Rect{0, 0, 5, 0}).empty());
    CK_CHECK((ckv::Rect{0, 0, -1, 5}).empty());
}

CK_TEST(rect_contains) {
    const ckv::Rect r{0, 0, 10, 10};
    CK_CHECK(r.contains(ckv::Point{0, 0}));
    CK_CHECK(r.contains(ckv::Point{9, 9}));
    CK_CHECK(!r.contains(ckv::Point{10, 5}));   // right edge is exclusive
    CK_CHECK(!r.contains(ckv::Point{5, 10}));   // bottom edge is exclusive
    CK_CHECK(!r.contains(ckv::Point{-1, 5}));
    CK_CHECK(!(ckv::Rect{}).contains(ckv::Point{0, 0}));  // empty rect contains nothing
}

CK_TEST(rect_intersected) {
    const ckv::Rect a{0, 0, 10, 10};
    const ckv::Rect b{5, 5, 10, 10};
    const ckv::Rect i = a.intersected(b);
    CK_CHECK(i.x == 5);
    CK_CHECK(i.y == 5);
    CK_CHECK(i.width == 5);
    CK_CHECK(i.height == 5);

    const ckv::Rect c{20, 20, 5, 5};
    CK_CHECK(a.intersected(c).empty());
}

CK_TEST(point_and_size_equality) {
    CK_CHECK((ckv::Point{1, 2}) == (ckv::Point{1, 2}));
    CK_CHECK((ckv::Point{1, 2}) != (ckv::Point{1, 3}));
    CK_CHECK((ckv::Size{4, 5}) == (ckv::Size{4, 5}));
    CK_CHECK((ckv::PixelPoint{1, 2}) == (ckv::PixelPoint{1, 2}));
    CK_CHECK((ckv::PixelSize{4, 5}) != (ckv::PixelSize{4, 6}));
}


// --- The two coordinate spaces never mix (the architecture §2, D-018) -------
//
// Compile-time guards: a pixel extent retyped as a cell-space Size, or an
// implicit conversion added between the spaces, fails the build here.

namespace {

using ckv::PixelPoint;
using ckv::PixelSize;
using ckv::Point;
using ckv::Size;

// Neither space becomes the other, implicitly or by construction.
static_assert(!std::is_convertible_v<Size, PixelSize>);
static_assert(!std::is_convertible_v<PixelSize, Size>);
static_assert(!std::is_constructible_v<PixelSize, Size>);
static_assert(!std::is_constructible_v<Size, PixelSize>);
static_assert(!std::is_convertible_v<Point, PixelPoint>);
static_assert(!std::is_convertible_v<PixelPoint, Point>);

// The terminal's pixel metrics and limits, as reported, overridden and
// declared to a child session.
static_assert(std::is_same_v<decltype(ckv::term::Capabilities{}.cell_pixels), PixelSize>);
static_assert(std::is_same_v<decltype(ckv::term::Capabilities{}.text_area_pixels), PixelSize>);
static_assert(std::is_same_v<decltype(ckv::term::Capabilities{}.window_pixels), PixelSize>);
static_assert(std::is_same_v<decltype(ckv::term::Capabilities{}.sixel_max_geometry), PixelSize>);
static_assert(std::is_same_v<decltype(ckv::term::CapabilityOverrides{}.cell_pixels), std::optional<PixelSize>>);
static_assert(std::is_same_v<decltype(ckv::core::TerminalCapabilityProfile{}.cell_pixels), PixelSize>);
static_assert(std::is_same_v<decltype(&ckv::core::TerminalSubsession::resize),
                             void (ckv::core::TerminalSubsession::*)(Size, PixelSize)>);

// The explicit conversions take the cell metric and change the space.
static_assert(std::is_same_v<decltype(&ckv::term::cells_to_pixels), PixelSize (*)(Size, PixelSize) noexcept>);
static_assert(
    std::is_same_v<decltype(&ckv::term::cell_pixels_from_area), PixelSize (*)(PixelSize, Size) noexcept>);

// Raster sizes: images, decoded Sixels and the virtual display's planes.
static_assert(std::is_constructible_v<ckv::Image, PixelSize>);
static_assert(!std::is_constructible_v<ckv::Image, Size>);
static_assert(!std::is_constructible_v<ckv::Image, int, int>);
static_assert(std::is_same_v<decltype(std::declval<const ckv::Image&>().size()), PixelSize>);
static_assert(std::is_same_v<decltype(ckv::term::DecodedSixel{}.declared), PixelSize>);
static_assert(std::is_same_v<decltype(&ckv::term::decode_sixel),
                             std::optional<ckv::term::DecodedSixel> (*)(std::string_view, PixelSize, std::size_t,
                                                                        ckv::term::SixelPalette&, std::string&)>);
static_assert(std::is_constructible_v<ckv::term::VirtualDisplay, Size, PixelSize>);
static_assert(!std::is_constructible_v<ckv::term::VirtualDisplay, Size, Size>);
static_assert(std::is_same_v<decltype(std::declval<const ckv::term::VirtualDisplay&>().cell_pixels()), PixelSize>);
static_assert(std::is_same_v<decltype(std::declval<const ckv::term::VirtualDisplay&>().pixel_size()), PixelSize>);
static_assert(std::is_same_v<decltype(&ckv::term::VirtualDisplay::set_cell_pixels),
                             void (ckv::term::VirtualDisplay::*)(PixelSize)>);

// What an application and its widgets are handed and hand back.
static_assert(std::is_same_v<decltype(std::declval<const ckv::ui::Application&>().terminal_cell_pixels()), PixelSize>);
static_assert(std::is_same_v<std::remove_cv_t<decltype(ckv::widgets::kAssumedCellPixels)>, PixelSize>);
static_assert(std::is_same_v<decltype(&ckv::widgets::fit_image_cells), Size (*)(PixelSize, PixelSize, Size) noexcept>);
static_assert(std::is_same_v<decltype(&ckv::widgets::Canvas::set_pixel_size), void (ckv::widgets::Canvas::*)(PixelSize)>);
static_assert(
    std::is_same_v<decltype(&ckv::widgets::Canvas::set_cell_metrics), void (ckv::widgets::Canvas::*)(PixelSize)>);
static_assert(std::is_same_v<decltype(std::declval<const ckv::widgets::Canvas&>().pixel_size()), PixelSize>);
static_assert(std::is_same_v<decltype(std::declval<const ckv::widgets::Canvas&>().cell_metrics()), PixelSize>);
static_assert(std::is_same_v<decltype(&ckv::widgets::TerminalView::set_cell_metrics),
                             void (ckv::widgets::TerminalView::*)(PixelSize)>);

}  // namespace

CK_TEST(cells_convert_to_pixels_only_through_the_cell_metric) {
    CK_CHECK(ckv::term::cells_to_pixels(Size{80, 24}, PixelSize{9, 18}) == (PixelSize{720, 432}));
    CK_CHECK(ckv::term::cells_to_pixels(Size{0, 24}, PixelSize{9, 18}) == (PixelSize{0, 432}));
}

CK_TEST(a_pixel_area_over_a_grid_yields_the_whole_pixel_cell_metric) {
    // 890x637 px over 89x29 cells: 10x21.96, rounded down to whole pixels.
    CK_CHECK(ckv::term::cell_pixels_from_area(PixelSize{890, 637}, Size{89, 29}) == (PixelSize{10, 21}));
    // No metric follows from an unknown area or an unknown grid.
    CK_CHECK(ckv::term::cell_pixels_from_area(PixelSize{0, 637}, Size{89, 29}) == PixelSize{});
    CK_CHECK(ckv::term::cell_pixels_from_area(PixelSize{890, 637}, Size{89, 0}) == PixelSize{});
    // An area smaller than its grid gives a zero dimension, not a guess.
    CK_CHECK(ckv::term::cell_pixels_from_area(PixelSize{40, 637}, Size{89, 29}) == (PixelSize{0, 21}));
}
