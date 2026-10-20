<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# WP-84 live-log acceptance matrix

Status: In progress. Earlier implementation commits are not final acceptance.
The engineering standard and WP-73…WP-84 literal criteria remain binding.
Pending, skipped, unavailable and loaded-host observations are never passing
release evidence. Native Window inspection is distinct from replay/VT figures.

## 2026-10-20 resumed validation

The source remains on `eb41d932` plus the owned WP-84 changes; unrelated
completion work is excluded from isolated validation. The October13 Mac/Linux
sanitizer builds completed successfully. Current runs found infrastructure
issues rather than treating those old outputs as fresh acceptance: Mac's leak
detector is unsupported (use supported ASan/UBSan without LSan), snapshot
copying lost five executable script modes (restored from the verified archive),
and Linux kernel headers are newer than the built objects (confirmed by Ninja
dependency mtimes). Linux is rebuilding before its final suite rerun. Its first
263-gate run passed261, with package-triggered recompilation and raw-golden
generation timing out. Instrumented raw-golden comparison now has the same
600-second correctness allowance as the existing appearance matrix; no native
latency or deterministic work budget is relaxed.

The supported Mac sanitizer run passed248/249, including the six-second quiet
stream regression. The failed package gate and affected log/CPU checks are
rerunning after mode restoration. The repaired Windows ARM64 full graph now
passes strict compilation; its complete suite and native Event Log contract
are running. A clean Linux TSan full graph is also active.

The idle audit fixed a Windows-specific defect: continuous process streams
inherited the finite helper's inactivity budget and left their capacity event
signalled. Continuous workers now use process/I/O/cancellation/capacity signals,
consume the wake event, and reserve short retries for mutex contention. The
portable native regression requires records on both sides of six quiet seconds.
The load tool also records process user/system CPU and, where available,
context-switch counts without calling them timer-wakeup measurements.

See the [resumed evidence record](evidence/live-log-wp84-progress-2026-10-20.json).
Native windows, actual idle-host terminal/keyboard/physical-visible timing,
resource stability, final cross-host gates, commit and cleanup remain pending.

## Current verified checkpoint

The complete Mac Release graph builds with zero warnings and passes 249/249
CTest gates. The initial full suite found seven integration failures; the
verified repairs add log appearance specimens across all four themes, complete
API/header/type and interaction mappings, reference all demo figures, include
three existing overload goldens in the strict generator inventory, and declare
example commands through typed descriptors. The hygiene guard now scopes chord
collisions to independent example registries while retaining same-app and
global command-key collision checks; its rejection/acceptance fixtures pass.

The expanded appearance gate verifies 66 elements, 891 states × four schemes,
3663 files and 92 symbolic scenes. Its 112 new log frames preserve all 3551
pre-existing fixture files' bytes except the expanded catalog. A rendered
overload fixture was visually inspected; it is not native window evidence.
Real Apple tagged-event/filter/detail/pause/resume/retirement integration passes
on this build through the headless app graph.

All six load-tool simulation cases pass: short/long/Unicode records, low-match
filtering, repeated query changes and concurrent foreground/background sources.
Terminal acknowledgement accounting starts from the existing session counter,
preventing a reply from an earlier trial being counted as the next trial's
first receipt. Real native timing remains pending.

The exact Mac-tested source is now in the resumed Linux full ASan/UBSan build
and a clean Mac ASan/UBSan build. Windows' full ARM64 build is still active and
will receive the verified integration delta after it ends. Full cross-platform,
thread/fuzz/native resource and actual-window/performance acceptance remains
required; this checkpoint does not promote WP-84 or the overall goal to Done.

| Requirement | Existing evidence | Current final obligation |
|---|---|---|
| Clean committed source, zero-warning macOS/Linux/Windows builds | Focused Mac/Linux builds through WP-83; unrelated completion work excluded from commits | Clean snapshot, complete targets and current output fingerprints on all hosts |
| Full unit/golden/script gates including provenance | Focused per-package gates and API reference coverage | All suites on the same final snapshot |
| Log decoder/filter/record/follower fuzz coverage | WP-84 targets/corpora added | Strict builds, CI corpora and campaigns; preserve/reduce any finding |
| ASan/UBSan and threaded race coverage | Mac/Linux focused sanitizers; Linux process-stream TSan | Full current supported sanitizer lanes, source/export/thread stability |
| Native file observation/rotation/truncation/recovery | WP-75/76 focus; real file clients on Mac/Linux | Native Windows and sustained three-host reliability |
| OS native source tagged event/filter/detail/pause/resume/close | Real Mac/Linux headless app graph; Windows replay | Native Windows tagged/stale/denial/missing-resource/resource checks |
| Installed public-only clients and reference/figures | Mac/Linux client contracts; 16 decoded/pinned figures; zero undocumented declarations | Windows installation/clients and final snapshot freshness |
| Producer outruns terminal, filter cancellation, notification floods | Deterministic queues/work/frame gates | Real 1×/10×/100× terminal-rate workloads, burst/rotation and repeated-query runs |
| Peak memory/CPU/idle wakeups and stable resources | Declared capacities and allocation/work tests | Measured process/native allocations, worker stacks, handles/fds and idle behavior |
| Input p99 <= 2 ms and source-to-visible p50/p95/p99 | Requirement retained; no loaded-host baseline invented | Idle pinned-host real terminal timing, separate ingest/filter/frame/physical completion |
| Actual native Mac/Linux/Windows demo window | Headless replay/decoded SVG evidence only | Verified absolute process, one project instance, inspect real visible source/filter/inspect/overload flows |
| Durable proof, cleanup and package status | Per-package hashes/times/logs | Final concise records; remove superseded owned artifacts; mark Done only when proven |

No source/debug-port privilege escalation, logging configuration changes or
other task's checkout/build/VM cleanup is authorized or required. Windows
source-transfer/setup operations must be audited by current process and output
state before restarting. The earlier operation produced a source tree but no
build record; a persistent verifier now confirms all 4680 transferred files match. The
strict original-snapshot build failed and compiler diagnostics are being
resolved; its output is not a current clean-tree acceptance result. Mac Terminal computer-use access
was denied earlier; no workaround or replay substitutes for native acceptance.

The loaded Mac observations (tens of runnable processes) do not satisfy the
idle-host timing prerequisite. Implementation work and deterministic validation
continue while those environmental checks remain pending. This package cannot
be marked complete solely because WP-73…WP-83 are implementation complete.

## New WP-84 measurement tooling

`benchmarks/log_terminal_load.cpp` adds native-session receipt calibration and
1×/10×/100× independent producer trials, bounded queue/retention checks, explicit
producer misses and queue loss, posted-work dispatch percentiles, latest-tail
receipt percentiles and process peak memory. Its Mac strict compile/link passes
against the clean Release library snapshot (latest 4.54 seconds, recorded
input/output hashes, no warnings). The controlled 10ms slow-terminal check
passes: 29,999 offered records, 85 receipt samples, bounded storage/queue/outstanding frames
and independent worker-post dispatch. This is a synthetic check only. Real
native runs are pending; the active clean candidates
predate this tool. DSR receipt and worker-post dispatch do not satisfy physical
visibility or keyboard transport timing, and the 1000-record/s calibration
ceiling is explicit. No timing or matrix row is promoted by adding this tool.

The clean Linux Clang19 Debug ASan/UBSan build of the three new fuzz targets
passes (126-action graph, 1068.70 seconds under load, unchanged input hashes).
Their seed corpora pass with leak detection and UBSan halt-on-error enabled.
All three 120-second mutation campaigns pass. The full target build/suite and
the pre-existing fuzz targets remain pending. The concise
[progress record](evidence/live-log-wp84-progress-2026-10-13.json) retains exact
commands, timestamps and source/output fingerprints.

## Current clean validation candidates

The Linux candidate is `eb41d932` plus only the owned WP-84 fuzz additions,
archived independently of all unrelated dirty completion work. Its Debug
Clang19 ASan/UBSan build passes the three new fuzz targets and their library
dependencies; full-suite execution remains pending.
The complete 476-action remaining Linux graph is active and predates the latest
tool output-counter additions and fixture repair; those need an explicit later
overlay. The Mac Release candidate's clean 566-action graph failed at action
332 on a partially initialized fixture metadata aggregate. Explicit empty
timestamp/fields fix the warning while preserving the fixture's meaning. After
the stopped job was confirmed complete, its source gained that fix and the
owned load-tool/docs overlay; the 237-action remaining/recompiled graph is
active at one job/nice 10. Fuzzing is disabled for AppleClang.
That resumed graph then failed at its action 155 on three partially initialized
mouse-event aggregates in the log-view drag test. Named action/button/cell
fields preserve cell-only event semantics and fix the warning. After the job
ended, the snapshot gained that fix and the latest owned code repairs; the
second resumed full graph is active. Original failure/repair records remain
distinct from passing build evidence.
Source/archive fingerprints and start/end/work logs reside in the task-owned
validation workspaces until concise final records are retained.

Windows' original transferred snapshot passes 4680/4680 file fingerprints and
failed its native ARM64 SDK build. It predates the final clean candidate;
its compiler findings are diagnostic evidence, not final snapshot acceptance.
No observation timeout authorizes restarting a process still confirmed live.

The Windows diagnostic exposed implicit integer-to-byte `std::fill` conversions
in log-search marks and two stale member-clock references in the injected
private process pump. Typed byte fill values and the borrowed `clock` parameter
fix those errors. A new committed-source archive plus only owned WP-84 overlays
is transferred with an archive-hash assertion before extraction; its complete
Windows ARM64 Release build has started. Original failure logs/input hashes are
retained before removing the obsolete failed Windows build/source. The second
Mac repair includes these code changes. Linux's active snapshot must gain the
repairs after its job ends; Windows must gain the later named mouse-event test
initializers. Fresh compilation and applicable gates remain required before
final acceptance.
