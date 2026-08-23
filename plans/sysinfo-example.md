# SysInfo example — delivery target and acceptance plan

## Client target

A client evaluating ckVision wants to see the framework do the three things a
real application does and the existing examples do not: show comparative
numbers as a picture, keep a value on screen current while the user watches,
and run work that takes seconds without freezing the interface. `examples/
sysinfo` — a system information and benchmarking application in the
interaction grammar of the early-1990s diagnostic tools — is the proof point
for all three at once.

It is also the example that answers "what does this framework cost me on a
host I have not seen": every number it prints about the machine arrives
through one injected interface, so the reader can see exactly where platform
impurity enters and how much of the application is unaffected by it.

## Why this example, given the existing set

| Capability | Covered today | Covered by SysInfo |
|---|---|---|
| Comparative data visualization | — | `BarChartView`, cell-drawn, with a Sixel-rendered variant behind the same data |
| Live-updating value under a timer | — | memory and load panes refresh on `Application::start_timer` |
| Long, cancellable background work with progress | `examples/spin` animates continuously | a benchmark run: progress, Esc-cancel, results posted back on the owning thread |
| Injected non-filesystem platform service | `FileSystem` in `examples/filebrowser` | `SystemProbe` — a second, differently-shaped instance of the same D-039 pattern |
| Terminal capability self-report as content | internal to `capability_report_text` | the Terminal pane shows the host's real answer |

## Architecture

**No public library surface changes.** `SystemProbe`, `BenchmarkRunner` and
`BarChartView` live in `examples/sysinfo` and are client code written against
the public API. The example is a demonstration of what a client builds *on*
ckVision, not an extension of it. A chart widget or a system-information
service in `include/cvision` would be a DECISIONS.md matter and a catalog
obligation; neither is required to make this example complete.

**One impurity boundary.** `SystemProbe` is a pure virtual interface
returning plain report structures. `PosixSystemProbe` implements it for macOS
and Linux and is the only file in the example that includes a platform header
or reads the environment. `FixedSystemProbe` returns a scripted machine, which
is what the headless tests and the generated screenshots run against — the
same arrangement that makes `examples/filebrowser` capture identically on
every host.

**Benchmarks are pure functions plus a service.** Each kernel is an ordinary
function over its own memory that returns a measured result; nothing in a
kernel touches a view, a theme, or the terminal. `BenchmarkService` owns one
worker thread, takes a queued run, reports progress, honours a cancel flag
between kernels, and returns each result to the owning thread through
`Application::post` — the same and only sanctioned crossing `examples/spin`
uses, for the same reason.

**Measurements are never assertions.** A benchmark score depends on the
machine, its thermal state, and what else is running on it. No test asserts a
score, a rate, or a duration. Tests assert structure: that a kernel computes
the right answer (a sieve's prime count, a CRC's published vector, a triad's
arithmetic), that a run reports monotonic progress and terminates, that cancel
stops it, and that results reach the view. The application labels its own
output as indicative, and says so in the report it exports.

## Package sequence

### WP-51 — SysInfo probe boundary and machine report panes

**Purpose.** Establish the example's platform boundary and the static half of
the application: what the machine *is*.

**Work.**

- Add `examples/sysinfo` with the established split: a `sysinfo_app` static
  library holding the whole object graph, and a POSIX-only `main.cpp`.
- Define `SystemProbe` with report structures for the host (name, user, OS,
  kernel, architecture, uptime), the processor (brand, physical and logical
  core counts, reported frequency where the host has one), memory (total,
  available, and the host's own accounting terms), volumes (mount path,
  filesystem, capacity, free), power, and the build's own facts (compiler,
  standard library, C++ level, pointer size, endianness).
- Implement `PosixSystemProbe` for macOS (`sysctl`, `host_statistics64`,
  `getmntinfo`) and Linux (`/proc/cpuinfo`, `/proc/meminfo`, `/proc/uptime`,
  `/proc/mounts`, `/etc/os-release`), reading capacity through
  `std::filesystem::space` on both, with every unavailable field reported as
  absent rather than as a plausible zero.
- Implement `FixedSystemProbe`: a scripted machine for tests and captures.
- Build the System, Memory and Volumes windows over `Table`, `Label` and a
  capacity bar, on a `Desktop` with the menu bar and status line of the
  reference vocabulary, plus a one-second refresh timer on the live figures.

**Acceptance.**

- The application renders every pane headlessly against `FixedSystemProbe`
  with byte-identical content on macOS and Linux.
- No file outside `posix_system_probe.cpp` includes a platform header or reads
  the environment; the object-graph library compiles on a host with no probe
  implementation at all.
- A field the host cannot answer renders as an explicit "not reported", and a
  test proves it for at least one field per report structure.
- `PosixSystemProbe` is exercised on the real host by a POSIX-only suite that
  asserts internal consistency (available ≤ total, core counts ≥ 1, a
  non-empty OS name), never a specific machine.

### WP-52 — Bar chart view, benchmark kernels, and the threaded run

**Purpose.** The comparative picture and the work that fills it.

**Work.**

- Add `BarChartView`: a `ui::View` drawing labelled horizontal bars with a
  value axis, a caption, and a highlighted "this machine" row, using
  `scene::box_drawing` so it is correct on a terminal with no graphics at all.
- Add the first three kernels — an integer mix reported as a normalized
  computing index, a floating-point kernel reported in MFLOPS, and a
  STREAM-shaped memory-bandwidth kernel reported in GB/s — each with a fixed
  work quantum, a warm-up, and a best-of-N reduction.
- Add `BenchmarkService`: one worker, a queued run, progress reported per
  completed kernel, a cancel flag observed between kernels, results delivered
  through `Application::post`.
- Add the Benchmarks window: a kernel picker, a `Progress` bar, Esc-cancel,
  and the chart, which compares the kernels against each other on one stated
  scale and this run against the one before it.

**Acceptance.**

- Each kernel has a correctness test with a published or self-evident expected
  answer, independent of how long it took.
- A scripted run reports monotonically non-decreasing progress, terminates,
  and delivers exactly one result per selected kernel to the view.
- A cancel between kernels stops the run, leaves the application responsive,
  and is proven by a test that cancels deterministically rather than by
  timing.
- The chart states its scale on the chart, and every bar on it was measured by
  this program on this machine.
- The window says which build measured the numbers, and says so differently
  when that build was not an optimized one.

### WP-53 — Cache hierarchy, multicore scaling, and the host's own capabilities

**Purpose.** The two analyses that make the picture worth looking at, and the
report only a terminal framework can produce.

**Work.**

- Add a pointer-chase latency kernel over a growing working set, charted as a
  stair-step so the host's cache boundaries are visible.
- Add a scaling run: the integer kernel on 1..N threads, charted as measured
  speedup against the ideal line, with the thread count taken from the probe
  rather than from `hardware_concurrency` alone.
- Add the Terminal pane: `Application::terminal_capability_report_text`, the
  cell-pixel metric, the graphics answer, and what each means for a client
  choosing between `Canvas` and a cell-drawn fallback.
- Draw the latency chart through `Canvas` where the host reports graphics, and
  through `BarChartView` where it does not, from one data set.

**Acceptance.**

- The pointer chase defeats prefetch and is proven to by a test asserting the
  traversal visits every slot exactly once in a non-sequential order.
- The scaling run produces one sample per thread count and never reports more
  threads than the probe offered.
- Both chart paths render the same data set, and a capture exists for each.

### WP-54 — Report export, help, and documentation

**Purpose.** Make the example's output leave the program, and make the program
teachable.

**Work.**

- Compose the full report — every pane and every measured result, with the
  indicative-measurement statement — as text and as Markdown, and write it
  through the injected `FileSystem` from a save dialog.
- Add a help topic per pane and per benchmark: what the number means, what it
  does not mean, and what would change it.
- Add `docs/sysinfo-example.md`, the `docs/example-apps.md` row and section,
  generated screenshots through the existing capture harness, and the
  `plans/example-coverage.md` rows this example now covers.

**Acceptance.**

- The exported report is produced against `MemoryFileSystem` in a test and
  contains every pane, with no value the panes do not also show.
- Every pane and benchmark answers F1 with its own topic; a test proves no
  topic is missing.
- The documentation is source-backed by the same snippet mechanism as the
  other example guides, and the screenshots are generated, not hand-made.

## Explicitly out of scope

- **A Windows probe.** `PosixSystemProbe`'s counterpart belongs to the Windows
  host work (WP-37) and lands with it; the interface exists so that package is
  one new file and no change to the application. Until then the example builds
  its object graph on any host and its interactive binary on POSIX, exactly
  like every other example.
- **Disk throughput measurement.** Writing test files is a side effect on the
  user's machine, and a meaningful result needs cache defeat that differs per
  platform and filesystem. It is a candidate for a later package, not part of
  this one.
- **Reference bars, in this package.** The tools this example takes its shape
  from drew your computer against a fixed table — an XT at 1.0, a 386 at 34.7.
  Those bars are worth having, and WP-55 adds them, under one rule that keeps
  them honest: a bar this program did not measure must not be presented as a
  measurement. What a reference row may carry is a published figure or one
  computed from a published standard — a memory generation's peak transfer
  rate, an interface's ceiling, a core's peak floating-point throughput — with
  its provenance stated and its status marked on the chart. What this package
  ships is the comparison that needs no outside source: kernel against kernel
  on one stated scale, and this run against the last one.

- **A library chart widget.** If the chart proves generally useful, promoting
  it into `include/cvision/widgets` is a DECISIONS.md matter with its own
  catalog, golden and documentation obligations.
