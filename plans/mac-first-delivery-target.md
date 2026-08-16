<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Mac-first delivery target

This is the active execution track for finishing ckVision professionally on
the current Mac host. It turns the already-landed implementation packages into
a shippable, documented, visually consistent, and performance-checked client
release. Work on this track is not held by deferred external-host work.

## Queue

Only one package is active at a time. Select the first `Ready` package, finish
its evidence, then move the next package to `In progress`.

| Order | Package | State | Exit proof |
|---:|---|---|---|
| 1 | WP-38-MAC — Mac release-gate closure | **Implementation complete — Mac gate passed** | [Mac traceability record](mac-first-traceability.md), complete CTest/fuzz/sanitizer evidence, documentation/link/provenance checks, and an explicit list of any remaining deferred rows |
| 2 | WP-51 — Mac interaction and visual polish | **Implementation complete — Mac gate passed** | Public-path scripts and reviewed cell/pixel goldens for menus, focus, resize, modal routing, terminal redraw, editor states, all built-in themes, and the documented examples |
| 3 | WP-52 — Mac performance and resilience hardening | **Implementation complete — Mac gate passed** | [Performance baseline](mac-performance-baseline.md), enforceable frame/allocation/parser/raster budgets, resilience tests, and focused sanitizer evidence |
| 4 | WP-53 — Mac packaging and client handoff | **Implementation complete — handoff checks passed** | Reproducible configure/build/run instructions, stable example executables, install/package smoke checks, generated documentation, and a clean-machine walkthrough |
| 5 | WP-54 — Mac 0.1 release audit | **In progress** | Versioned release checklist, API/reference coverage report, example matrix, known-limitations page, release notes, and a signed-off Mac acceptance record |

## WP-38-MAC — Mac release-gate closure

This package consumes the current implementation evidence for WP-23 through
WP-49 and closes the gates that can be observed on the Mac host:

- Generate a traceability table from each M0–M11 Mac exit item to its source,
  test, golden, benchmark, and documentation evidence.
- Run the complete configured build and CTest suite, terminal fuzz corpus,
  focused sanitizer suites, layer/provenance checks, documentation checks, and
  benchmark/budget gates.
- Reconcile every plan status with executable evidence; no implementation is
  called released merely because a local smoke test is green.
- Record deferred rows separately from Mac acceptance instead of allowing them
  to block the active finishing sequence.

## WP-51 — Mac interaction and visual polish

Finish the user-visible details that determine whether ckVision feels native
and dependable:

- Remove redraw, focus, clipping, menu, cursor, resize, and modal-routing
  defects found during manual use of the examples.
- Complete the public-path script and golden matrix for the terminal, editor,
  menus, windows, dialogs, and all built-in themes.
- Review generated screenshots for semantic correctness, not merely file
  existence; regenerate only after a deliberate visual diff.
- Keep every fix at the owning library layer and add a regression case for the
  observed defect.

## WP-52 — Mac performance and resilience hardening

Make the Mac build suitable for sustained use:

- Turn frame-time, allocation, parser-work, PTY-drain, and raster-size budgets
  into failing tests where they are currently advisory.
- Exercise large scrollback, noisy children, large Sixel payloads, repeated
  resize/focus cycles, close/reopen loops, and nested application lifetimes.
- Run sanitizer and leak checks on the terminal, editor, scene, and widget
  paths; record the exact toolchain and baseline.
- Add a repeatable benchmark report so performance regressions are visible in
  review.

The accepted baseline and exact commands are recorded in
[mac-performance-baseline.md](mac-performance-baseline.md).

## WP-53 — Mac packaging and client handoff

Make the result easy for a Mac developer or client to build and run:

- Stabilize the documented executable names and output locations.
- Verify clean configure/build/run from a fresh checkout with no hidden local
  environment assumptions.
- Package or archive the example binaries and generated documentation through a
  repeatable script.
- Add a short troubleshooting guide for terminal environment, Sixel fixture,
  fonts/cell metrics, and clean teardown.

## WP-54 — Mac 0.1 release audit

Perform the final product review after the preceding packages are complete:

- API/reference/tutorial coverage is complete and source-backed.
- Every example has a runnable acceptance path and reviewed visual evidence.
- Known limitations are explicit and do not contradict the public contract.
- The Mac acceptance record contains toolchain, test counts, benchmark results,
  artifact hashes, and the release decision.

## Working rule

This document is the active next-work-package list for Mac development. The
broader corrective-assurance ledger remains the historical/global traceability
record; it must be updated when a Mac-first package changes a shared status or
evidence row.
