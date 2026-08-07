// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The one concrete real-time Clock (D-025): core/scene/ui/widgets never
// read wall-clock time directly, so applications need SOMETHING at the
// term-layer boundary to inject. PosixClock is that something on
// POSIX hosts — CLOCK_MONOTONIC, the same source PosixTerminal's own
// internal timing already uses, so an application's timers and the
// terminal's own read-timeout math agree on what "now" means.
#pragma once

#include "cvision/core/clock.hpp"

namespace ckv::term {

class PosixClock final : public Clock {
public:
    std::int64_t now_nanos() const noexcept override;
};

}  // namespace ckv::term
