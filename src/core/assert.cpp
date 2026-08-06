// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/core/assert.hpp"

#include <cstdio>
#include <cstdlib>

namespace ckv::detail {

[[noreturn]] void assertion_failed(const char* expr, const char* file, int line) noexcept {
    std::fprintf(stderr, "ckVision contract violation: %s (%s:%d)\n", expr, file, line);
    std::abort();
}

}  // namespace ckv::detail
