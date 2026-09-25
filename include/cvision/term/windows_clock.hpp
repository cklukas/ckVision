// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Monotonic clock for Windows terminal sessions and Application timers.
#pragma once

#include "cvision/core/clock.hpp"

namespace ckv::term {

class WindowsClock final : public Clock {
public:
    WindowsClock() noexcept;
    std::int64_t now_nanos() const noexcept override;

private:
    std::int64_t frequency_ = 0;
};

}  // namespace ckv::term
