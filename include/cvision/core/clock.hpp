// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace ckv {

// The library's only time source (the architecture §10, the engineering standard
// determinism rule): no wall-clock reads anywhere in core/scene/ui/
// widgets. Monotonic, injected: production code takes a real clock at
// the application boundary; tests step a fake one by hand.
class Clock {
public:
    virtual ~Clock() = default;

    // Monotonic, non-negative, implementation-defined epoch.
    virtual std::int64_t now_nanos() const noexcept = 0;
};

// A manually steppable Clock for tests and headless replay — never
// reads real time.
class ManualClock final : public Clock {
public:
    explicit ManualClock(std::int64_t start_nanos = 0) noexcept : now_(start_nanos) {}

    std::int64_t now_nanos() const noexcept override { return now_; }

    void advance(std::int64_t delta_nanos) noexcept { now_ += delta_nanos; }
    void set(std::int64_t nanos) noexcept { now_ = nanos; }

private:
    std::int64_t now_;
};

}  // namespace ckv
