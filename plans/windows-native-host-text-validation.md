<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Native host text and test contracts — candidate validation

Status: isolated, unreleased 0.1.14 candidate · 2026-10-04

## Completed pre-version candidate gates

The complete corrected source/assets manifest
`ea4dea0000c8d2bfb41e4e482757fbe4bed347cc292ec0a8cf3ae9f39652b962`
passes all five configurations below. Every CTest exit is 0. Normal
configurations have examples ON and pass their separate benchmark gates.

| Host/compiler/configuration | CTest entries | Elapsed |
|---|---:|---:|
| macOS ARM64 Apple Clang strict Release | 225/225 | 43.32 s |
| Debian 13 ARM64 GCC 13.3 Release/FORTIFY | 225/225 | 60.65 s |
| Windows 11 25H2 ARM64 MSVC strict Release | 216/216 | 51.51 s |
| macOS ARM64 LLVM 23 Debug ASan/UBSan plus fuzz | 235/235 | 185.56 s |
| macOS ARM64 LLVM 23 Debug TSan | 224/224 | 392.15 s |

These gates include the acknowledged nested child and delayed-reader
regression described below. Sequential Mac sanitizer execution avoids the
earlier concurrent expensive appearance/golden checks; serialization alone
had not fixed the nested child's premature exit. No CTest timeout was enlarged.

Apple Clang's current 241-action incremental rebuild took 52 s
(10:50:41Z–10:51:33Z), versus the earlier one-second/no-delta invocation.
The actual compile log includes the changed test runner, nested fixture,
package fixture and new tests; elapsed time is not its freshness proof.
Test executable SHA256:
`5a6e49c38b6aa0682dc88d5053820ad4f33fe421430d7f3cce4f4bbebb12392b`.
Windows' final fixture/two-forced-TU incremental build took 17.855 s
(10:28:42.347Z–10:29:00.216Z); archive SHA256:
`60f7577ea6cff06e0a4cfd5703f196ff8a92397a7a0167ecda53aeb9395a508e`.
Linux's 243-action rebuild took 798 s (10:27:50Z–10:41:08Z), including
an unnecessary repeated mtime invalidation of the unchanged test header;
it is not a claim of another header-code change. Source bytes are checked
before and after each full gate.

The version declaration, literal version test, sixteen version-bearing
TerminalReportDialog appearance goldens and its documentation SVG now prepare
0.1.14. The first isolated versioned commit `04b316ad` passes 224/225 in
Apple Clang Release (42.79 s); widget_shots_visual_capture correctly refuses
the documentation SVG's old version digit. Appearance goldens and the literal
version test pass. Comparing the actual generated SVG changes exactly one
text glyph, from 3 to 4; the reviewed correction keeps the screenshot checker
unchanged. This failed sweep is retained, not reported as a full gate.
Fresh committed
and filtered-publication gates must include those final bytes before landing
or release. The five rows above prove the pre-version candidate, not the
new version, a GitHub release, an installed package or a native user window.

## Earlier failed and passing evidence

D-128 supplies Unicode diagnostic filenames; D-129 supplies deterministic CRT
argv encoding; D-130 supplies strict native UTF-8/UTF-16 conversion. Their
reference contracts are in DECISIONS.md and docs/terminal-host-integration.md.
The test runner additionally moves condition reporting behind an inline bool
bridge, preserving one evaluation and contextual negation without placing a
constant if condition in an MSVC adopter. A standalone strict contract checks
literal/constexpr positives, explicit-bool/custom-negation values, one
evaluation, and an actual false check's exit code and original diagnostic.
No warning suppression or skipped assertion is introduced.

Frozen source/assets manifest
`6553ac4eaa9a4b36da35230c8bdd163667590d1f4d5d147e6ce93017451868e5`
passes Windows 11 25H2 ARM64 MSVC strict Release 216/216, CTest exit 0,
115.39 s, plus its benchmark gate. A changed public test header forces the
test inventory to rebuild: 09:58:47.718Z–10:06:24.935Z, 457.219 s, rather than
the preceding two-TU 20.039 s incremental build. Native archive SHA256:
`f5176b38cb4e8d9cf5cb1c1a65ced5334adfa6d849326a04ebc8c42b0365449c`.

The same inputs pass Debian 13 ARM64 GCC 13.3 Release/FORTIFY 225/225,
CTest exit 0, 58.78 s, and the benchmark gate. Header-triggered 242-action
rebuild: 10:00:08Z–10:13:10Z, 782 s. These are candidate input gates, not
isolated committed or published-release gates.

The first full Mac LLVM 23 Debug ASan/UBSan plus fuzz run rebuilt 175 actions
and failed appearance_matrix's unchanged 120 s deadline, 234/235. Concurrent
LLVM 23 Debug TSan rebuilt 174 actions and failed generated_golden_bytes's
unchanged deadline plus two nested-child output assertions, 222/224. An
earlier driver label incorrectly called the ASan rebuild fixture-only with
unchanged compiled inputs; actual compile logs contradict that label. The
correct scope is the public-header incremental rebuild, not a clean build.

A serialized ASan repeat passes the isolated appearance check (119.29 s),
then its complete 235-entry run fails one nested-child assertion (234/235,
376.46 s). Thus serialization does not by itself resolve the nested fixture;
none of these failed sweeps is a full sanitizer pass.

The nested independently launched library application formerly exited 200 ms
after its initial frame on POSIX. A delayed parent could drain that frame
together with teardown and observe only the restored empty screen. The new
regression obtains the real initial frame, deliberately stops draining for
400 ms, then requires the child and frame still present. Compiled against
the frozen original child, it fails both lifecycle/frame assertions (build
exit 0, test exit 1). No stale executable answered the negative check.

The fixture now holds its frame until real private input reaches its focused
InputLine, on both POSIX and Windows. The parent sends `goq` after its frame
or confinement observation and checks normal child exit. Related observations
use bounded elapsed-time guards rather than a fixed number of readiness
polls. The rendering/confinement assertions remain; no child output is
fabricated and no CTest timeout is enlarged. The corrected Mac LLVM 23 Debug
ASan/UBSan POSIX subsession suite passes 1/1, CTest exit 0, 8.93 s, after four
actual compile/link actions. New complete all-platform, sanitizer/fuzz and
clean-commit gates must include this final fixture delta before a release.

Temporary build outputs and compact failed/passing records remain in the
owner-selected task directories. This record does not certify a user-facing
native window, an installed package, or any newer public release.
