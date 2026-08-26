// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "cvision/core/geometry.hpp"

namespace ckv {

enum class CursorShape {
    Block,
    Bar,
    Underline,
};

inline constexpr std::int64_t kDefaultCursorBlinkHalfPeriodNanos =
    250'000'000;

// Cursor position, visibility, and shape are scene outputs composed
// like everything else (the architecture §3), and term::Presenter must
// consume them without depending on scene (the architecture §1/§4) —
// hence living in core, like FrameView/RasterSlice.
struct CursorState {
    bool visible = false;
    Point position;
    CursorShape shape = CursorShape::Block;
    // Requests deterministic ckVision-managed blinking when visible. The
    // presenter owns the timing and emits a steady host cursor, so terminal
    // preferences cannot override this state. A producer that models real
    // hardware may supply that device's half-period explicitly.
    bool blink = false;
    std::int64_t blink_half_period_nanos =
        kDefaultCursorBlinkHalfPeriodNanos;

    friend bool operator==(const CursorState&, const CursorState&) = default;
};

}  // namespace ckv
