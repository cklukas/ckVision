// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckbench — ckVision's minimal, dependency-free benchmark harness.
// Note: benchmarks are tooling and may read the steady clock; library
// code may not (the architecture §10).
#pragma once

#include <chrono>
#include <cstdio>

namespace ckbench {

template <typename F>
inline void run(const char* label, int iterations, F&& fn) {
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    for (int i = 0; i < iterations; ++i) fn();
    const auto stop = clock::now();
    const long long ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
    const double per_op = iterations > 0 ? static_cast<double>(ns) / iterations : 0.0;
    std::printf("%-32s %8d iters %12lld ns total %12.1f ns/op\n", label, iterations, ns,
                per_op);
}

}  // namespace ckbench
