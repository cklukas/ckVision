<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Command-button toolbar presentations — 2026-10-01

D-118 and plans/widgets.md specify Compact, Padded and Framed presentations over
one ToolBar interaction/layout contract. Explicit groups have separators only
between visible groups; overflow retains command order and group breaks. Theme
roles are independent of menus and ordinary buttons. Disabled, pressed,
focused, hovered and checked states have explicit precedence; keyboard focus
underlines the caption and checked marks remain visible. Pointer feedback is
limited to available button regions. Release-reporting keys arm once; Escape,
focus loss, resizing and reconfiguration cancel input. Commands restore their
originating document focus before execution. Workbench exposes all variants.
Behavior was derived from ckVision's own specification and executable evidence;
no external implementation source was used.

Coverage adds thirteen toolbar cases, one Workbench case, one allocation gate
and two text validation/clipping cases. It covers Unicode widths, tiny bounds,
grouping and popup separators, bottom docking, all-variant focus restoration,
keyboard releases, drag cancellation, theme independence, fallback roles and
prepared-caption refresh after registry/toggle changes. The allocation gate
exercises long Unicode captions, chords and command contexts, all presentations,
checked-state changes, overflow, pointer motion, painting and arrow navigation.
Toolbar-owned preparation retains parsed captions, both toggle faces and layout
storage; borrowed clipping and validated display-text borrowing avoid repeated
caption copies while preserving D-040. The text fuzzer checks borrowed/owning
clipping equivalence and validation/sanitization equivalence.

There are 132 toolbar appearance snapshots across four schemes. Gallery figures
show all three presentations. Generated framed, focused, padded and grouped SVG
scenes were visually inspected in a browser. No interactive app was launched;
those scene renders do not establish a running application's window.

| Configuration | Final clean build | Compiled inputs | Full CTest |
| --- | --- | --- | --- |
| macOS Release | 156.355 s | 393 C++ inputs / 481 actions | 218/218, 18.67 s |
| Linux GCC Release | 184.537 s | 393 C++ inputs | 218/218, 14.25 s |
| Linux Clang ASan/UBSan + fuzz | 169.122 s | 405 C++ inputs | 228/228, 69.62 s |
| Linux GCC TSan | 141.204 s | 392 C++ inputs | 217/217, 142.56 s |
| Windows VS2022 ARM64 Release | 600.079 s | 369 primary compiled objects | 204/204, 34.55 s |

All builds were warning-free. Full suites include mandatory provenance checks
and self-test, unit/golden/script/documentation checks, and benchmark budgets.
The ASan/UBSan configuration ran all ten fuzz corpus gates. Modern Windows
ConPTY additionally passed 22 cases. Release benchmark smoke targets passed on
macOS, Linux and Windows.

Freshness: all final clean builds used the same hash-verified 3349-file snapshot.
Every compiler lane rebuilt its complete input set without a compiler launcher;
Windows primary objects and retained output timestamps fall inside its recorded
build interval. Output hashes/timestamps are in the adjacent validation JSON.
A subsequent one-character gallery Markdown correction changed no compiled
input or generated asset; all seven documentation gates were rerun successfully
on each host. Every final source/asset byte was then hash-verified on all hosts.
The evidence documents themselves are excluded from these source manifests.

Build snapshot SHA-256: `c29aaded273c9dacb9c263dfab1f4804a9b3d80beba2fdd1d5d0db2786202a6c`.

Final source snapshot SHA-256: `85203b21a18d67de1b3d5fcb78cf5834c916a3500715a8218627912916d7cf35`.

Asset-map SHA-256 (sorted JSON of generated documentation/golden paths/hashes):
`53f18917e69eafd5787bf66b84ed696d9c7695e486c004fa116444a2eaa27cea`.

Comparable preceding builds in
[tab-presentations-2026-10-01.md](tab-presentations-2026-10-01.md) took 149.733 s
(macOS), 191.233 s (Linux Release), 303.573 s (ASan/fuzz), 192.343 s (TSan), and
791.039 s (Windows). The preceding toolbar behavior snapshot took 132.937 s,
140.692 s, 205.063 s, 197.084 s and 669.868 s respectively. Timing differences
were investigated through complete compiler-input counts, warning checks,
launcher configuration and fresh-object timestamps, rather than accepted as
proof of freshness. No duration baseline is asserted for differing incremental
workloads.

An earlier overlapping local build/source-change pass was rejected as evidence;
its processes finished before sole frozen-source verifiers ran. An earlier
Windows checker argument error was corrected without changing successful
output. The final Windows object audit initially included an install-consumer
fixture built by CTest after the primary build; restricting the audit to primary
objects confirmed all 369 were rebuilt within the primary interval. These were
verification-helper corrections, not failures of accepted application output.

Retained verified Release builds:

- /Volumes/PRO-BLADE/tmp/ckvision-toolbar-20261001-01/mac-release
- /home/klukas.guest/ckvision-toolbar-20261001-01/release
- C:\Users\klukas\ckvision-toolbar-20261001-01\release

Detailed start/end times, configurations, output hashes/timestamps and cleanup
results are recorded in the adjacent validation JSON. Sanitizer build trees,
superseded Release builds, staging archives and disposable preview/test copies
were removed after saving evidence. Required source workspaces and current
Release outputs remain; unrelated project processes and source assets were
preserved. No project app instance remains running.
