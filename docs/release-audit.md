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
project now declares version `0.1.8`. The technical run below belongs to the
earlier `0.1.0` package; it is historical evidence, not verification of a
`0.1.8` release candidate. Approval remains open until the current candidate
has fresh gate evidence and the final interactive acceptance review is signed
off against the bundle produced by [Client handoff](client-handoff.md).

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

## Final acceptance checklist

- [ ] The selected release candidate declares version `0.1.8` and exports
  `ckvision::cvision`; verify both from the candidate archive.
- [x] Framework-only and full-example installation contracts are independently
  exercised.
- [x] The documented examples, their visual capture paths, and client bundle
  are exercised by CTest.
- [x] Parser resilience, terminal lifecycle, menu interaction, and graphics
  fallback paths have current automated evidence.
- [ ] A release-candidate archive has been generated from the chosen clean
  revision and its SHA-256 recorded.
- [ ] A product owner has completed the interactive Mac acceptance walkthrough
  and approved the release decision.

## Approval record

| Field | Value |
|---|---|
| Release candidate revision | Pending |
| Archive SHA-256 | Pending |
| Interactive acceptance reviewer | Pending |
| Date | Pending |
| Decision | Pending |
