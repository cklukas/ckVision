// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The single entry point for the cvision_bench executable — every
// other bench_*.cpp exposes a run_*_benchmarks() function and must NOT
// define its own main().
void run_golden_benchmarks();
bool run_scene_benchmarks();

int main() {
    run_golden_benchmarks();
    return run_scene_benchmarks() ? 0 : 1;
}
