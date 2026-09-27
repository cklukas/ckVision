// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "cvision/core/geometry.hpp"

namespace ckv {

// The text cursor's shape: a block covering the cell, a vertical bar at its
// left edge, or an underline. The presenter requests it from the host with
// DECSCUSR, always in the steady form (see CursorState::blink).
enum class CursorShape {
    Block,
    Bar,
    Underline,
};

// The default blink half-period: 250 ms visible, then 250 ms hidden, in
// nanoseconds.
inline constexpr std::int64_t kDefaultCursorBlinkHalfPeriodNanos =
    250'000'000;

// Cursor position, visibility, and shape are scene outputs composed
// like everything else (the architecture §3), and term::Presenter must
// consume them without depending on scene (the architecture §1/§4) —
// hence living in core, like FrameView/RasterSlice.
struct CursorState {
    // Whether the cursor is shown, the 0-based frame cell it sits on, and its
    // shape. A default state is hidden; position and shape matter only while
    // `visible` is true.
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

    // Memberwise equality, blink settings included.
    friend bool operator==(const CursorState&, const CursorState&) = default;
};

}  // namespace ckv
