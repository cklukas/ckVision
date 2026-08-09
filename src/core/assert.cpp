// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/core/assert.hpp"

#if !defined(CKVISION_HAS_POSIX_TERMINAL)
#include <cstdio>
#endif
#include <cstdlib>

namespace ckv::detail {

[[noreturn]] void assertion_failed(const char* expr, const char* file, int line) noexcept {
#if defined(CKVISION_HAS_POSIX_TERMINAL)
    publish_assertion_failure(expr, file, line);
#else
    std::fprintf(stderr, "ckVision contract violation: %s (%s:%d)\n", expr, file, line);
#endif
    std::abort();
}

}  // namespace ckv::detail
