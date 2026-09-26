// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Monotonic clock for Windows terminal sessions and Application timers.
#pragma once

#include "cvision/core/clock.hpp"

namespace ckv::term {

// now_nanos() reads the performance counter, converted to nanoseconds from an unspecified
// origin; if the counter is unavailable it falls back to GetTickCount64() (millisecond
// resolution). The application passes the same instance to WindowsTerminal so timers and
// terminal deadlines share one time base.
class WindowsClock final : public Clock {
public:
    // Reads the performance-counter frequency once; a zero or failed reading selects the
    // tick-count fallback for this clock's lifetime.
    WindowsClock() noexcept;
    std::int64_t now_nanos() const noexcept override;

private:
    std::int64_t frequency_ = 0;
};

}  // namespace ckv::term
