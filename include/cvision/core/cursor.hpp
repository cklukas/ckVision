// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include "cvision/core/geometry.hpp"

namespace ckv {

enum class CursorShape {
    Block,
    Bar,
    Underline,
};

// Cursor position, visibility, and shape are scene outputs composed
// like everything else (ARCHITECTURE.md §3), and term::Presenter must
// consume them without depending on scene (ARCHITECTURE.md §1/§4) —
// hence living in core, like FrameView/RasterSlice.
struct CursorState {
    bool visible = false;
    Point position;
    CursorShape shape = CursorShape::Block;

    friend bool operator==(const CursorState&, const CursorState&) = default;
};

}  // namespace ckv
