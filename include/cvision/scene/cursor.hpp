// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include "cvision/core/geometry.hpp"

namespace ckv::scene {

enum class CursorShape {
    Block,
    Bar,
    Underline,
};

// Cursor position, visibility, and shape are scene outputs composed
// like everything else (ARCHITECTURE.md §3). Which view's cursor wins
// is a focus decision the ui layer makes (M4+); this type is just the
// value that decision produces.
struct CursorState {
    bool visible = false;
    Point position;
    CursorShape shape = CursorShape::Block;

    friend bool operator==(const CursorState&, const CursorState&) = default;
};

}  // namespace ckv::scene
