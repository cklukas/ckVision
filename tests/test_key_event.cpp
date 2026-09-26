// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/event.hpp"
#include "cvision/core/key.hpp"

#include "cvision/testing/cktest.hpp"

CK_TEST(modifier_combination) {
    ckv::Modifier m = ckv::Modifier::Ctrl | ckv::Modifier::Shift;
    CK_CHECK(ckv::has_modifier(m, ckv::Modifier::Ctrl));
    CK_CHECK(ckv::has_modifier(m, ckv::Modifier::Shift));
    CK_CHECK(!ckv::has_modifier(m, ckv::Modifier::Alt));
}

CK_TEST(key_chord_equality) {
    const ckv::KeyChord a{ckv::Key::Char, ckv::Modifier::None, "x"};
    const ckv::KeyChord b{ckv::Key::Char, ckv::Modifier::None, "x"};
    const ckv::KeyChord c{ckv::Key::Char, ckv::Modifier::Shift, "x"};
    CK_CHECK(a == b);
    CK_CHECK(a != c);
}

CK_TEST(mouse_event_dual_coordinate_space) {
    // Cell-only report: no terminal is claiming pixel precision it
    // doesn't have (D-018) — the field must be genuinely absent.
    ckv::MouseEvent cell_only;
    cell_only.action = ckv::MouseAction::Down;
    cell_only.button = ckv::MouseButton::Left;
    cell_only.cell = ckv::Point{3, 4};
    CK_CHECK(!cell_only.pixel.has_value());

    ckv::MouseEvent with_pixel = cell_only;
    with_pixel.pixel = ckv::PixelPoint{27, 36};
    CK_CHECK(with_pixel.pixel.has_value());
    CK_CHECK(with_pixel.pixel->x == 27);
    CK_CHECK(with_pixel != cell_only);
}

CK_TEST(text_event_marks_paste_origin) {
    ckv::TextEvent typed{"a", false};
    ckv::TextEvent pasted{"hello", true};
    CK_CHECK(!typed.from_paste);
    CK_CHECK(pasted.from_paste);
}


CK_TEST(mouse_event_image_pixel_maps_a_reported_pixel_the_way_the_presenter_scales_the_picture) {
    // A 40 x 20 picture over the 8 x 2 cells at (3, 1) of a 9 x 18 terminal:
    // 72 x 36 screen pixels starting at (27, 18), so 1.8 screen pixels to a
    // picture pixel each way.
    const ckv::Rect area{3, 1, 8, 2};
    const ckv::PixelSize cell{9, 18};
    const ckv::PixelSize picture{40, 20};
    ckv::MouseEvent event;
    event.cell = ckv::Point{3, 1};
    event.pixel = ckv::PixelPoint{27, 18};
    CK_CHECK(event.image_pixel(area, cell, picture) == (ckv::PixelPoint{0, 0}));
    event.pixel = ckv::PixelPoint{27 + 71, 18 + 35};
    CK_CHECK(event.image_pixel(area, cell, picture) == (ckv::PixelPoint{39, 19}));
    // Screen pixel 50 of 72 across and 10 of 36 down: floor(50 * 40 / 72) and
    // floor(10 * 20 / 36), the source pixels the presenter samples there.
    event.pixel = ckv::PixelPoint{27 + 50, 18 + 10};
    CK_CHECK(event.image_pixel(area, cell, picture) == (ckv::PixelPoint{27, 5}));
}

CK_TEST(mouse_event_image_pixel_is_empty_outside_the_picture_without_a_pixel_report_or_without_extents) {
    const ckv::Rect area{3, 1, 8, 2};
    const ckv::PixelSize cell{9, 18};
    const ckv::PixelSize picture{40, 20};
    ckv::MouseEvent event;
    event.cell = ckv::Point{4, 1};
    // Cell-only: never estimated from the cell.
    CK_CHECK(!event.image_pixel(area, cell, picture).has_value());
    event.pixel = ckv::PixelPoint{26, 18};  // one pixel left of the area
    CK_CHECK(!event.image_pixel(area, cell, picture).has_value());
    event.pixel = ckv::PixelPoint{27 + 72, 18};  // one pixel past its right edge
    CK_CHECK(!event.image_pixel(area, cell, picture).has_value());
    event.pixel = ckv::PixelPoint{30, 18 + 36};  // one pixel below it
    CK_CHECK(!event.image_pixel(area, cell, picture).has_value());
    event.pixel = ckv::PixelPoint{30, 20};
    CK_CHECK(event.image_pixel(area, cell, picture).has_value());
    CK_CHECK(!event.image_pixel(area, ckv::PixelSize{0, 0}, picture).has_value());
    CK_CHECK(!event.image_pixel(area, cell, ckv::PixelSize{0, 20}).has_value());
    CK_CHECK(!event.image_pixel(ckv::Rect{3, 1, 0, 2}, cell, picture).has_value());
}
