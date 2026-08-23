<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Performance verification

ckVision treats the machine-independent half of its performance charter as a
correctness property. `scene_budget_gate` runs `cvision_bench` in CTest and
fails if a warmed compositor touches a cell on an unchanged frame, more than
one cell after a single-cell content change, or a complete retained Window
move exceeds its 696-cell / 6,144-byte caps. The instrumented
`warmed_*allocate_nothing*` tests replace allocation only inside the test
executable and verify, after warm-up, that compositor, raster-layer movement,
Presenter, focused and pointer dispatch, traversal, due-timer delivery, and
posted-work draining perform no ordinary heap allocation. This instrumentation
is test-only; it does not introduce a library global or alter client allocation
behavior.

`Application::last_compose_cells_touched()`,
`Application::last_bytes_emitted()`, `Desktop::last_content_repaints()`, and
`Window::content_repaint_count()` are the deterministic per-frame counters
used by interaction tests and benchmarks. A move must repaint no Window
content and compose only the old/new window rectangles and their shadows; a
resize may repaint only the resized Window's local backing. The same retained
rule applies to popups: a same-size move is layer composition only, whereas
their own content invalidation and a resize repaint that popup's backing.

The same CTest budget gate includes the editor's deterministic local-relex
oracle: on a 4,000-line YAML source, changing a state-independent first line
may invoke highlighting for at most that line and the one unchanged line that
proves the cache fixed point. The cap is two lines; it is independent of host
speed and causes `scene_budget_gate` to fail if a local edit starts scanning an
unbounded suffix again.
Window and drop-down-menu layers use the same binary shadow compositor rule;
their shadows are included in the move damage bound and never compound.

## Pinned-host p99 procedure

The wall-clock gate runs separately from shared CI on the named reference host
`macos-26.5.2-arm64-m1-max-32g` (Apple M1 Max, 32 GiB, macOS 26.5.2 build
25F84). Run it from a clean Release build, with the host otherwise idle and a
real PTY endpoint. For each of 10,000 scripted operations—keystroke echo,
focus movement, menu navigation, window move, and resize—timestamp immediately
before the harness writes the input bytes and when it observes the final byte
of that operation's one Presenter write. Record the sorted samples' 99th
percentile, terminal capability profile, build compiler/version, and commit.

The acceptance limits are those in the architecture §8: input event to presented
bytes p99 below 2 ms and a full theme-switch recompose/diff below 5 ms. The
first accepted run establishes the checked-in baseline; later runs fail if
either absolute limit is exceeded or p99 regresses by more than 5% from that
baseline. PTY timestamp/capture automation and multi-host publication are
owned by WP-32; this document fixes the procedure and comparison rule now so
that later infrastructure cannot silently redefine the gate.
