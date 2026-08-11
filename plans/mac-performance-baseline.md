<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Mac performance and resilience baseline

This is the repeatable WP-52 evidence record for the current Mac release
track. The benchmark is a deterministic workload gate: elapsed time is
reported for review, while the checked limits are pass/fail invariants.

## Toolchain

- Host architecture: `arm64`
- CMake: `4.3.4`
- Compiler: Apple Clang `21.0.0 (clang-2100.1.1.101)`
- Build configuration: `Release`

## Commands

```text
cmake --build build --target cvision_bench -j2
build/benchmarks/cvision_bench
ctest --test-dir build -R 'suite_test_terminal_emulator|scene_budget_gate' --output-on-failure
```

The benchmark covers a 900-line scrollback flood, 500 repeated resizes, and
eight large Sixel payloads. The terminal gate checks:

| Invariant | Limit |
|---|---:|
| Scrollback retention | 256 lines × 120 cells |
| Parser work per step | 131,072 bytes |
| Graphics payload | 131,072 bytes |
| Terminal cell storage after resize | exactly `width × height` |
| Diagnostics retention | at most 64 entries |

The latest run completed all benchmark and terminal-emulator checks. The
reported timings are retained as a review baseline, not as a machine-specific
failure threshold; the invariants above are the release gate.

