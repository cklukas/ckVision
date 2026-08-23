// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The one concrete real-time Clock (D-039): core/scene/ui/widgets never
// read wall-clock time directly, so applications need SOMETHING at the
// term-layer boundary to inject. PosixClock is that something on
// POSIX hosts — CLOCK_MONOTONIC. The application passes this same
// instance to PosixTerminal, so application timers and terminal
// read/probe deadline math agree on what "now" means.
#pragma once

#include "cvision/core/clock.hpp"

namespace ckv::term {

class PosixClock final : public Clock {
public:
    std::int64_t now_nanos() const noexcept override;
};

}  // namespace ckv::term
