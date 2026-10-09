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
project declares version `0.1.19`. The `0.1.0`, `0.1.8`, and `0.1.18` runs
below are historical evidence. The [0.1.19 release record](../plans/evidence/release-0.1.19-2026-10-09.md)
identifies the selected source and the owner's explicit release exceptions.
The Mac client archive is produced by [Client handoff](client-handoff.md).

## `0.1.19` release decision — 2026-10-09

The selected internal source is `d186c5dffff322d58d26a026792fe7b21f51da02`.
The owner approved release and expressly waived the interactive five-app
walkthrough. The owner also directed that the idle-Mac p99 and theme-switch
baseline be skipped for this release. Neither waived item is represented as a
passed check. The D-136 baseline remains open on the macOS 27.0.1 reference
host, so the M3 latency exit remains unaccepted. The exact build, package,
source and published-snapshot identities are recorded in the linked release
record.

The public source snapshot is `6b865375abaf6c92992f28e6d66b647178ac9e49`.
It passed its own warning-free Release build, 227 non-PTY and four PTY tests,
and strict documentation build. The Mac client archive built from a clean
checkout of that snapshot is
`ckvision-client-0.1.19-6b865375.tar.gz`, SHA-256
`8695a3d208fd3d09716966a13acc258caca3e7cafea4d4465bbb4007711e13f8`.
Its 225 generated figures match the public source, and its installed Terminal
and Progress binaries match the package build.

The [published v0.1.19 release](https://github.com/cklukas/ckVision/releases/tag/v0.1.19)
contains that exact archive. Mac, Linux and Windows ARM64 Release suites,
Linux ASan/UBSan with 11 fuzz corpora, Linux TSan, modern Windows ConPTY,
benchmark smoke and all seven public CI jobs passed as detailed in the
release record. The owner-directed baseline exception remains open for M3.

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
approval was not granted. Its retained `0.1.8` archive was superseded by
later candidates.

## Historical `0.1.18` candidate — 2026-10-09

Revision `60aaba3f27149520c8750eafc1c2691e182115de` on `main` was the
selected technical candidate. Its clean detached source checkout had 5,009
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
| Linux ARM64 Release and sanitizer/fuzz gates | Clean rpi4 Release build, 233/233 CTest and benchmark smoke; clean Debian VM ASan/UBSan 244/244 including all 11 fuzz corpora; clean TSan 232/232 passed |
| Windows ARM64 Release, modern ConPTY gates | Clean build, 227/227 CTest, benchmark smoke, and 34/34 modern ConPTY cases passed |
| D-136 idle-host PTY p99 and theme-switch baselines | Pending |
| Product-owner interactive Mac walkthrough and approval | Pending |

The [candidate evidence](../plans/evidence/client-candidate-60aaba3f-2026-10-09.md)
records build timing, source and asset identity, executable hashes, test logs
and archive verification. This `0.1.18` candidate and the earlier `0.1.8`
checklist below are historical and do not close the `0.1.19` gates.

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
- The `0.1.19` client archive is built from the clean public source snapshot
  and has its archive checksum recorded above.

## Historical `0.1.18` acceptance checklist

- [x] The selected candidate declares version `0.1.18`, exports
  `ckvision::cvision`, and contains the complete installed examples and 225
  tracked generated figures.
- [x] The Mac Release and ASan/UBSan builds have zero warnings and pass their
  full 233-test suites, including provenance, package, golden, script, and
  deterministic budget gates.
- [x] The then-selected candidate passed clean Linux and Windows ARM64 Release
  suites, benchmark smoke, modern Windows ConPTY, Linux ASan/UBSan with all
  11 fuzz corpora, and Linux TSan.
- [ ] The D-136 reference Mac is otherwise idle during accepted PTY p99 and
  theme-switch measurements; both meet the absolute limits and establish the
  checked-in baselines.
- [ ] The product owner completes the interactive Mac walkthrough against the
  exact candidate and approves the release decision.

For that walkthrough, use the bundle path above and launch one executable at
a time from `sdk/bin/` in a real Mac terminal. Confirm the rendered window and
input behavior, then exit each app before starting the next:

1. `ckvision_hello`: open the greeting with Alt+G, dismiss it with Enter, and
   quit with Alt+X.
2. `ckvision_gallery`: use F10 and **View → Scheme** to inspect Classic, Dark
   and Light; move and resize a window by mouse. Scroll the Sixel Demo and
   open a menu across its image in an iTerm2 Sixel session; confirm the
   documented cell fallback in a text-only terminal.
3. `ckvision_editor`: edit the sample, use Ctrl+Z/Ctrl+Y, **Search →
   Replace...** (Ctrl+R), and Save (Ctrl+S); inspect line/column status,
   focus and close behavior.
4. `ckvision_terminal`: create a second child through **File → New Terminal**,
   type in both, open **File → New Sixel Demo**, resize and close one child,
   and confirm the other remains interactive.
5. `ckvision_progress`: inspect its meter, task-row and activity variants and
   change a measurement option.

Record any discrepancy against the candidate revision and archive SHA-256
above. Approval applies to this exact archive; a rebuilt or modified archive
needs a new review.

| Historical approval field | Value |
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
