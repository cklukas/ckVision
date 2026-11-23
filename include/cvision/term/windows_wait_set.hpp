// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#if defined(_WIN32)

#include <memory>
#include <span>

#include "cvision/core/clock.hpp"
#include "cvision/term/terminal.hpp"

namespace ckv::term {

// Instance-owned Windows readiness wait without worker threads or a 64-source
// limit (D-126). Driven by one thread. Sources are borrowed manual-reset
// readiness events, console input or process handles, not mutexes/semaphores.
class CVISION_TYPE_API WindowsWaitSet {
public:
    // Borrows a clock for absolute deadlines. Native setup failures throw
    // std::system_error. The clock must outlive this set.
    CVISION_MEMBER_API explicit WindowsWaitSet(const Clock& clock);
    // Cancels registrations and releases the owned completion port/packets.
    CVISION_MEMBER_API ~WindowsWaitSet();

    // One owner; copying or moving would invalidate registration ownership.
    CVISION_MEMBER_API WindowsWaitSet(const WindowsWaitSet&) = delete;
    CVISION_MEMBER_API WindowsWaitSet& operator=(const WindowsWaitSet&) = delete;

    // Updates borrowed sources, then waits for a distinct batch until the
    // absolute deadline (INT64_MAX means indefinitely). Positive fractional
    // milliseconds round up. Duplicate sources coalesce; zero/non-Windows
    // entries are ignored, and an empty set returns immediately. Each source
    // may occur once per batch and is rearmed on the next call. Native errors
    // throw std::system_error; exceptions clear registrations before escaping.
    // The returned span belongs to this instance until the next wait/clear.
    // Keep sources alive until a later call omits them, or call clear first.
    CVISION_MEMBER_API std::span<const WaitHandle> wait(std::int64_t deadline_nanos,
                                     std::span<const WaitHandle> sources);
    // Retires all registrations before borrowed source handles may be closed;
    // pending completions cannot be mistaken for a subsequently added source.
    CVISION_MEMBER_API void clear() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ckv::term

#endif
