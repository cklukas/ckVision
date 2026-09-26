// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The single entry point for the cvision_bench executable — every
// other bench_*.cpp exposes a run_*_benchmarks() function and must NOT
// define its own main().
//
// Usage: cvision_bench [--gate]. Without arguments it times every benchmark; with --gate
// it runs each briefly and exits non-zero if any budget fails (scene_budget_gate).
#include <cstdio>
#include <optional>

#include "ckbench.hpp"

void run_golden_benchmarks(const ckbench::Runner& bench);
bool run_scene_benchmarks(const ckbench::Runner& bench);
bool run_editor_benchmarks(const ckbench::Runner& bench);
bool run_terminal_benchmarks(const ckbench::Runner& bench);

int main(int argc, char** argv) {
    const std::optional<ckbench::Mode> mode = ckbench::mode_from_arguments(argc, argv);
    if (!mode) {
        std::fprintf(stderr, "usage: cvision_bench [--gate]\n");
        return 2;
    }
    const ckbench::Runner bench(*mode);
    run_golden_benchmarks(bench);
    return run_scene_benchmarks(bench) && run_editor_benchmarks(bench) && run_terminal_benchmarks(bench) ? 0 : 1;
}
