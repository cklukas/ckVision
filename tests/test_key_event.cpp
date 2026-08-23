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

