<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Performance verification

ckVision treats the machine-independent half of its performance charter as a
correctness property. `scene_budget_gate` runs `cvision_bench --gate` in CTest and
fails if a warmed compositor touches a cell on an unchanged frame, more than
one cell after a single-cell content change, or a complete retained Window
move (`retained_window_move`) exceeds its 696-cell / 6,272-byte caps. It
fails as well if a resize-drag step (`retained_window_resize`) repaints any
Window other than the resized one, repaints that one other than exactly once,
or exceeds 532 cells / 4,096 bytes. That benchmark drives the drag as a reader
does: a press on the resize grip and pointer motion injected through
`HeadlessTerminal`, alternating a 30x10 window with its 36x13 size beside a
peer window. 532 cells is the 38x14 rectangle around the larger size and its
shadow; a measured step touches 528 cells and emits at most 3,737 bytes. The
instrumented
`warmed_*allocate_nothing*` tests replace allocation only inside the test
executable and verify, after warm-up, that compositor, raster-layer movement,
Presenter, focused and pointer dispatch, traversal, due-timer delivery, and
posted-work draining perform no ordinary heap allocation. This instrumentation
is test-only; it does not introduce a library global or alter client allocation
behavior.

`--gate` runs each benchmark only four times: every iteration checks its
budgets, so four visits every state an alternating benchmark cycles through,
and the gate stays fast under Debug and sanitizer builds. The timed run, with
each benchmark's full iteration count, is `cmake --build build --target
bench_smoke_run`.

`Application::last_compose_cells_touched()`,
`Application::last_bytes_emitted()`, `Desktop::last_content_repaints()`, and
`Window::content_repaint_count()` are the deterministic per-frame counters
used by interaction tests and benchmarks. A move must repaint no Window
content and compose only the old/new window rectangles and their shadows; a
resize may repaint only the resized Window's local backing. The pointer paths
are gated the same way in `test_application_render.cpp`:
`dragging_a_window_recomposes_without_repainting_its_retained_content` for a
title-bar drag, and `grip_dragging_a_window_repaints_only_its_retained_subtree`
for a grip drag. In the grip drag, every press, motion and release step
repaints the resized window once and its peer never, within a cells-touched
bound. The same retained
rule applies to popups: a same-size move is layer composition only, whereas
their own content invalidation and a resize repaint that popup's backing.

The same CTest budget gate includes the editor's deterministic local-relex
oracle: on a 4,000-line YAML source, changing a state-independent first line
may invoke highlighting for at most that line and the one unchanged line that
proves the cache fixed point. The cap is two lines; it is independent of host
speed and causes `scene_budget_gate` to fail if a local edit starts scanning an
unbounded suffix again.
Window, drop-down-menu and popup-list layers use the same binary shadow compositor rule;
their shadows are included in the move damage bound and never compound.

## Pinned-host p99 procedure

The wall-clock gate runs separately from shared CI on the named reference host
`macos-27.0-arm64-m1-max-32g` (Apple M1 Max, 32 GiB, model Mac13,1, macOS 27.0
build 26A428). Run it from a clean Release build, with the host otherwise idle and a
real PTY endpoint. For each of 10,000 scripted operations—keystroke echo,
focus movement, menu navigation, window move, and resize—timestamp immediately
before the harness writes the input bytes and when it observes the final byte
of that operation's one Presenter write. Record the sorted samples' 99th
percentile, terminal capability profile, build compiler/version, and commit.

The acceptance limits are those in the architecture §8: input event to presented
bytes p99 below 2 ms and a full theme-switch recompose/diff below 5 ms. The
first accepted run establishes the checked-in baseline; later runs fail if
either absolute limit is exceeded or p99 regresses by more than 5% from that
baseline. Multi-host publication is owned by WP-32; this document fixes the
procedure and comparison rule so that later infrastructure cannot silently
redefine the gate.

### The PTY harness

`cvision_pty_latency` (`benchmarks/pty_latency.cpp`, POSIX only) implements the
event-to-bytes half of the procedure. It is a tool target and deliberately not
a CTest: shared CI never judges wall-clock time.

- **The session.** The harness opens a pseudo-terminal sized to the §8
  reference configuration, 200×60, and forks a child that runs a real
  `Application` on the slave side through `PosixTerminal`. The child's scene is
  ordinary chrome over a `Desktop`: a menu bar, a status line, an *Editor*
  window with three `InputLine`s, and a *Notes* window behind it. Its
  capability profile is stated rather than probed: the `ModernVt` baseline
  (legacy keyboard, SGR mouse, focus reports, bracketed paste) with true colour
  and synchronized output, probes off. The harness is the terminal and answers
  no probes.
- **The frame boundary.** With synchronized output in the profile, the
  Presenter writes every non-empty frame as one `write()` bracketed by
  `CSI ? 2026 h` … `CSI ? 2026 l`. The final byte of a frame is therefore the
  `l` of the closing bracket. Nothing else the session writes, such as cursor
  blink or pointer shapes, carries that bracket.
- **The operations.** One 25-operation cycle holds the documented mix, and 400
  cycles make the 10,000 operations. The cycle is 10 echo keys (five characters
  typed into the focused field, then five Backspaces), 4 focus moves (Tab, Tab,
  Shift+Tab, Shift+Tab), and 5 menu operations: Alt+F opens *File*; Down, Down
  and Up walk it; Enter chooses an item that does nothing, which closes the
  menu and returns the focus. The cycle ends with 3 title-bar drag steps and 3
  resize-grip drag steps as SGR mouse reports, each drag returning to where it
  began. Each operation is one `write()` of the bytes a host sends for it.
  A lone Esc is not scripted. A legacy keyboard delivers it only after the
  decoder's 50 ms quiet deadline, which comes from the protocol's Esc-versus-Alt
  ambiguity, not from event-to-bytes work.
- **The rehearsal.** Before any timing, the harness plays one cycle headlessly
  on the same scene and profile. Every operation must present exactly one frame,
  and the cycle must leave the composed frame and the focus exactly as it found
  them. The rehearsal proves that each timed operation measures the work it
  names. During the timed run, a frame that no operation asked for fails the
  run.
- **The measurement.** For each operation, the harness reads the steady clock
  immediately before the `write()` to the PTY master. It reads the clock again
  when the `read()` that delivers the closing bracket returns. Four warm-up
  cycles run first and are discarded. Percentiles are nearest-rank over the
  sorted samples. The harness prints p50, p90, p99 and maximum for each
  operation class (`echo`, `focus`, `menu_open`, `menu_navigate`,
  `menu_close`, `window_move`, `window_resize`) and overall, with the mean
  frame size in bytes. It writes the same figures as JSON, together with the
  build type, compiler, ckVision version, the commit it was given, `uname`, the
  capability profile and the host's load average before and after the run.
  It exits 0 when overall p99 is below 2 ms and within the optional baseline, 1
  when either check fails, and 3 when the run itself could not be completed.

Run it on the reference host from a Release build:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCKVISION_WARNINGS_AS_ERRORS=ON
cmake --build build-release --target cvision_pty_latency
uptime                      # record the load; the host should be otherwise idle
build-release/benchmarks/cvision_pty_latency \
    --commit "$(git rev-parse --short HEAD)" --output pty_latency.json
# later runs: add --baseline-p99-ns <the baseline's overall p99_ns>
```

`--operations N` and `--warmup-cycles N` change the counts. The procedure uses
the defaults, 10,000 and 4. The `pty_latency_run` target runs the harness with
defaults and writes `pty_latency.json` into the build tree. Each run is
recorded with the host identity (`sw_vers`, `sysctl hw.model
machdep.cpu.brand_string`), commit, build type, load average and date; the
first run was on `macos-27.0-arm64-m1-max-32g`.
