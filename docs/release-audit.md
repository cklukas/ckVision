<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

---
title: ckVision 0.1 Mac Release Audit
author: C. Klukas
date: 2026-09-27
format: report
description: Evidence-led release audit and acceptance checklist for the current Mac build.
---

# ckVision 0.1 Mac release audit

This is the WP-54 audit record for the Mac CMake package. The checked-in
project declares version `0.1.18`. The `0.1.0` and `0.1.8` runs below
are historical evidence. A current `0.1.18` client candidate is identified
below. Approval remains open until its remaining gates are recorded and the
owner signs the final interactive acceptance review against that bundle produced by
[Client handoff](client-handoff.md).

## Release contents

| Delivered surface | Runnable acceptance path | Reference |
|---|---|---|
| Installable framework and CMake package `ckvision::cvision` | `install_package_smoke` configures, links, and runs an independent HeadlessTerminal + Application consumer | [Getting started](getting-started.md) |
| Complete example set | Each `ckvision_*` executable is installed by the handoff bundle and its headless public path is covered by the suite | [Example applications](example-apps.md) |
| Terminal application | Start `ckvision_terminal`; open a second shell with **File → New Terminal**, then use **Window → Tile/Cascade** | [Embedded terminal](embedded-terminal.md) |
| Client archive and visual evidence | `client_bundle_smoke` creates an SDK, example binaries, source documentation, fresh SVG captures, and a tar archive | [Client handoff](client-handoff.md) |

The public API is indexed by header and client task in the
[API index](api-index.md); its widget coverage is checked by
`doc_widget_coverage`, and `interaction_script_coverage` checks that every
public view type is driven by an Application-level
[interaction script](coverage.md#interaction-scripts). The [example coverage matrix](../plans/example-coverage.md)
maps every example surface to its interaction and visual evidence.

## Historical `0.1.0` evidence rerun on 2026-08-13

| Gate | Result |
|---|---|
| Full configured build | Passed |
| Full configured CTest suite | 133 / 133 passed |
| Installed package, full example build | Passed: independent consumer configures, links, presents a frame, and exits |
| Installed package, library-only build | Passed: same independent consumer contract without example targets |
| Client handoff archive | Passed: SDK, runnable examples, generated captures, and archive verified |
| Terminal/editor/input fuzz corpus | 8 / 8 passed |
| Focused sanitizer terminal and menu suite | 6 / 6 passed |
| Documentation, architecture, provenance, and budget gates | Passed as part of the full configured suite |
| Working-tree whitespace check | Passed |

The exact rerunnable commands are:

```text
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
ctest --test-dir build-fuzz-llvm -L fuzz-corpus --output-on-failure
ctest --test-dir build-asan-llvm -R 'suite_test_(terminal_emulator|terminal_view|posix_terminal_subsession|menu|terminal_cell_golden|presenter)' --output-on-failure
git diff --check
```

## Historical `0.1.8` Mac evidence — 2026-09-27

The clean `f08b2ae` checkout completed a zero-warning Release build and passed
**218/218** CTest tests, including the provenance gates. From the documented
prerequisites and build commands, with the build output redirected to the
project's required temporary volume, it reached a live `ckvision_hello` frame
in **194 seconds (3 minutes 14 seconds)**. Alt+G opened the greeting dialog and
Alt+X exited cleanly. The exact revision, source/tree and executable hashes,
timestamps, commands and PID are in the
[timed tutorial record](../plans/evidence/hello-tutorial-2026-09-27.md).
This establishes the M11 tutorial exit on the Mac; it is not a release
candidate archive or the product owner's final walkthrough.

## Historical `0.1.8` client candidate — 2026-09-27

The then-selected clean revision `7ecd1d9` passed the full Mac Release suite,
**218/218** tests, including provenance and package gates. The client archive
was generated from that revision, verified as a readable tar/gzip, and checked
for version `0.1.8`, the `ckvision::cvision` export, installed Mac examples,
source documentation and 46 generated SVG captures. Its SHA-256 is
`6d958afeda9a3da9e2665f0f4a5f6efea05347ed1093c5f86616a08d9554acfd`.
The path, build and package timestamps, input/output fingerprints and checks
are in the [historical candidate record](../plans/evidence/client-candidate-7ecd1d9-2026-09-27.md).
The earlier [`b4783d0` package](../plans/evidence/client-candidate-2026-09-27.md)
was superseded and removed. The Root Dialog change passed the full Linux
Release suite (218/218) and Windows Release suite (204/204); a
[signed-in Windows run](../plans/evidence/windows-rootdialog-2026-09-27.md)
also exercised validation and acceptance.
After candidate packaging, the same executable code was exercised in actual
Mac Terminal and iTerm windows: [Echo and Root Dialog](../plans/evidence/live-mac-examples-2026-09-27.md)
and [Gallery with Sixel scrolling and dual-space pixel mouse](../plans/evidence/gallery-iterm-2026-09-27.md).
These agent-directed observations supplement that candidate's gates; owner
approval was not granted. Its retained `0.1.8` archive is not the current
`0.1.18` release candidate.

## Current `0.1.18` candidate — 2026-10-09

Revision `60aaba3f27149520c8750eafc1c2691e182115de` on `main` is the
selected technical candidate. Its clean detached source checkout has 5,009
tracked files matching the candidate manifest, with no tracked changes. The
unrelated untracked `.claude/` worktrees in the primary checkout are not an
input. The Mac ARM64 Release build completed in 122 seconds with zero warnings
and passed **233/233** CTest tests, including both provenance gates, the
installed-package consumer, client-bundle smoke, and deterministic benchmark
gates. A clean Mac ASan/UBSan build passed **233/233** tests. The full Mac
benchmark smoke completed. The candidate archive contains the installed SDK,
examples, source documentation and all **225** manifest figures; every figure
matches its source SVG byte for byte.

| Candidate field | Value |
|---|---|
| Bundle | `/Volumes/PRO-BLADE/tmp/ckvision-wp38-rc-Ob01bw/ckvision-client-0.1.18-60aaba3f` |
| Archive | `/Volumes/PRO-BLADE/tmp/ckvision-wp38-rc-Ob01bw/ckvision-client-0.1.18-60aaba3f.tar.gz` |
| Archive SHA-256 | `54146819e4e50450294a3bfc9a9ee4b3baea902316872eabf737e9230b3af8c2` |
| Archive size and members | 14,443,315 bytes; 676 members |
| Mac Release CTest | 233/233 passed |
| Mac ASan/UBSan CTest | 233/233 passed |
| Linux ARM64 Release and sanitizer/fuzz gates | In progress |
| Windows ARM64 Release, modern ConPTY gates | Clean build, 227/227 CTest, benchmark smoke, and 34/34 modern ConPTY cases passed |
| D-136 idle-host PTY p99 and theme-switch baselines | Pending |
| Product-owner interactive Mac walkthrough and approval | Pending |

The [candidate evidence](../plans/evidence/client-candidate-60aaba3f-2026-10-09.md)
records build timing, source and asset identity, executable hashes, test logs
and archive verification. The earlier `0.1.8` checklist below is historical
and does not close the current-version gates.

## Known limits and operating requirements

- Terminal graphics require a usable reported pixel geometry; otherwise the
  corresponding view uses its documented deterministic cell fallback.
- The terminal example intentionally names its child command outright —
  `$SHELL` with a `/bin/sh` fallback — and supplies its demo environment
  explicitly, confined to `TERM` and `COLORTERM`. Library clients remain
  responsible for their own child command, working directory, and environment
  policy.
- A monospaced display font and stable cell metrics are required for correct
  box geometry. The terminal guide explains recovery after a font or window
  size change.
- The supplied archive is a local, reproducible handoff artifact. The final
  release artifact must be built from the selected clean revision and have its
  archive checksum recorded with the sign-off below.

## Current `0.1.18` acceptance checklist

- [x] The selected candidate declares version `0.1.18`, exports
  `ckvision::cvision`, and contains the complete installed examples and 225
  tracked generated figures.
- [x] The Mac Release and ASan/UBSan builds have zero warnings and pass their
  full 233-test suites, including provenance, package, golden, script, and
  deterministic budget gates.
- [ ] The current candidate passes the clean Linux Release suite and supported
  sanitizer/fuzz lanes. Windows ARM64 Release, benchmark smoke and the modern
  ConPTY gate have passed.
- [ ] The D-136 reference Mac is otherwise idle during accepted PTY p99 and
  theme-switch measurements; both meet the absolute limits and establish the
  checked-in baselines.
- [ ] The product owner completes the interactive Mac walkthrough against the
  exact candidate and approves the release decision.

| Current approval field | Value |
|---|---|
| Release candidate revision | `60aaba3f27149520c8750eafc1c2691e182115de` |
| Archive SHA-256 | `54146819e4e50450294a3bfc9a9ee4b3baea902316872eabf737e9230b3af8c2` |
| Interactive acceptance reviewer | Pending |
| Date | Pending |
| Decision | Pending |

## Historical `0.1.8` acceptance checklist

The checked boxes below record the former candidate's technical evidence only.
They are not approval or current-version release evidence.

- [x] The selected release candidate declares version `0.1.8` and exports
  `ckvision::cvision`; verify both from the candidate archive.
- [x] Framework-only and full-example installation contracts are independently
  exercised.
- [x] The documented examples, their visual capture paths, and client bundle
  are exercised by CTest.
- [x] Parser resilience, terminal lifecycle, menu interaction, and graphics
  fallback paths have current automated evidence.
- [x] A clean Mac checkout reached a running Hello from documented prerequisites
  in under ten minutes, with the timed run and executable identity recorded.
- [x] A release-candidate archive has been generated from the chosen clean
  revision and its SHA-256 recorded.
- [ ] A product owner has completed the interactive Mac acceptance walkthrough
  and approved the release decision.

## Historical `0.1.8` approval record

| Field | Value |
|---|---|
| Release candidate revision | `7ecd1d9d221a26e7a81bf334978e3ef92f85e81d` |
| Archive SHA-256 | `6d958afeda9a3da9e2665f0f4a5f6efea05347ed1093c5f86616a08d9554acfd` |
| Interactive acceptance reviewer | Pending |
| Date | Pending |
| Decision | Pending |
