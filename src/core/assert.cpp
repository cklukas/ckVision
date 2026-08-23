// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/assert.hpp"

#include <cstdio>
#include <cstdlib>

namespace ckv::detail {

[[noreturn]] void assertion_failed(const char* expr, const char* file, int line) noexcept {
#if defined(CKVISION_HAS_POSIX_TERMINAL)
    // True only when a live session's fatal handler is there to print it after
    // restoring the screen. It is the handler's job in that case precisely
    // BECAUSE the screen must be restored first.
    const bool someone_will_say_it = publish_assertion_failure(expr, file, line);
#else
    constexpr bool someone_will_say_it = false;
#endif
    if (!someone_will_say_it)
        std::fprintf(stderr, "ckVision contract violation: %s (%s:%d)\n", expr, file, line);
    std::abort();
}

}  // namespace ckv::detail
