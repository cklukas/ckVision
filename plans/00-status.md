<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Status and document map

Development-internal. The public README states what ckVision is, how to build
it, and where the client documentation lives; it deliberately says nothing
about milestones, work-package numbers, or which audit reopened what. That
material lives here, so the record can stay detailed and honest without the
front page reading like a project-management console.

## Current acceptance status

Substantial implementation has landed through M10, but **no milestone is
currently acceptance-complete**. A 2026-08-08 audit reopened M0–M6 and M8–M10
because literal exit criteria, cross-platform gates, security/performance
evidence, widget coverage, and documentation remain incomplete. Passing local
Release and ASan/UBSan suites is useful evidence, not the full definition of
done.

The authoritative task source is the
[corrective assurance plan](corrective-assurance.md). "Work on the next open
WP" means its first `In progress` row, otherwise its first `Ready` row.
Earlier `Done` labels are historical landing records and must not be used to
skip reopened work. A `Done` row is a claim about the code, not the code.

[ROADMAP.md](../ROADMAP.md)'s current-acceptance-status table maps every
reopened milestone to the work packages that now own it.

## What has landed, by milestone

- **M0–M2**: build skeleton; core primitives (geometry, Color/Style/Cell/
  Image, keys/events, the Unicode services in
  [docs/text-width.md](../docs/text-width.md)); the scene layer
  (`Surface`/`Painter`/`Compositor` with damage tracking, junction merging
  scoped by view provenance so unrelated z-levels never alter one another's
  borders, binary/non-cumulative shadow occlusion, raster occlusion slicing).
- **M3 (term)**: the `Terminal` interface and `Capabilities` model; an
  incremental input decoder (legacy/kitty/modifyOtherKeys keys, SGR and
  SGR-Pixels mouse, sanitized bracketed paste, focus events, OSC 10/11 and
  DECRPM probe responses — coverage documented in
  [docs/input-decoder.md](../docs/input-decoder.md)); a `Presenter` (cell
  diffing, D-019 width-safe cursor re-addressing, color-depth degradation,
  synchronized output, OSC-safe emission, cropped Sixel raster overlay); a
  correct public-protocol Sixel encoder; headless, POSIX (real termios/
  signals/session-registry, PTY-tested), and record/replay backends.
- **M4 (ui core)**: `View` (the ui tree's base class), `Application` (focus
  traversal, positional/focused/command-keymap event dispatch, mouse input
  capture, timers, F1 context-help routing, the embeddable `step()`/`wake()`
  loop), the flat interned-role `Theme` system, `Row`/`Column` layout, the
  internal clipboard, `HistoryRegistry`, declarative dialog descriptors with
  validated materialization, and the four minimal controls (Label, StaticText,
  Button, InputLine).
- **M5 (windows/menus)**: `Window` (frame chrome, move/resize, the vetoable
  close protocol, zoom), `Desktop` (z-order, activation, cycling, tile/cascade,
  popups), dialog accept-validation veto with Esc-cancel, message boxes with
  every standard button set, and the `MenuBar`/`DropdownMenu`/context-menu
  system with light-dismiss.
- **M6 (widget completion)**: `Scrollbar`, `ScrollViewport`, `CheckGroup`/
  `RadioGroup`, `ListView`, `TreeView`, `TextView`, `Memo`, `Table`,
  `ImageView`, `Canvas`; InputLine completion (validators, input masks,
  password echo, history cycling); the Dark/Light/Mono theme schemes; the
  window-list dialog, file open/save dialog and directory picker (both on the
  injected `FileSystem`), and the Help viewer.

Independent review passes across M4, M5, and M6 found and fixed eleven real
bugs (five use-after-frees, five correctness issues, one z-order/lifecycle
desync), each landed with a regression test verified under ASan/UBSan.

**Post-M6, the render pipeline and example apps landed**: the gap where
`Application::step()` dispatched input but nothing ever painted anything is
closed (`View::paint_children`, reusing `scene::Compositor`/`term::Presenter`);
window shadows actually composite (`widgets::Desktop::paint_children`). Two
example apps — `examples/gallery` (rendering, keyboard/mouse navigation, Sixel
images) and `examples/filebrowser` (a `TreeView`-drives-`ListView` master-detail
folder browser over the real disk, via `term::PosixFileSystem`) — with a
matching documentation page and generated screenshots, rendered to HTML/PDF via
CK Office Write's `ckwrite` (see [docs/example-apps.md](../docs/example-apps.md)
and `tools/docgen/generate_docs.sh`).

A first ergonomics/resize review added the Desktop-level mechanisms:
`Desktop::dock_top`/`dock_bottom` + `content_area()` +
`Window::reposition_within` (chrome pinning and window clamping when the
*Desktop's* bounds change), `TreeView` selection/activation callbacks (the
master-detail hook), and a widget on the window's own border, e.g. a "current
line" indicator (`Window::add_frame_overlay` as of the second review — plural,
positioned overlays replacing the first review's single-slot
`set_frame_overlay`).

The 2026-08-07 [API-ergonomics and integration review](api-ergonomics-review-2.md)
is a compact historical index for WP-1…WP-22. The corrective audit found that
several mechanisms landed without their specified end-to-end acceptance and
exposed additional lifetime, modality, provenance, Unicode, performance,
portability, fuzzing, Sixel visual-capture, catalog, and documentation gaps.
WP-23…WP-72 (including WP-32A and WP-36A…WP-36C) now own those repairs and
added release proof points explicitly. Their current states are in the
corrective-assurance queue, not in the historical landing record.

## Internal document map

Root-level design record (not published):

| Document | Content |
|---|---|
| [VISION.md](../VISION.md) | Mission, non-negotiable capabilities, design principles, non-goals |
| [ARCHITECTURE.md](../ARCHITECTURE.md) | Target architecture: layers, rendering pipeline, event/command system, backends |
| [ROADMAP.md](../ROADMAP.md) | Development milestones with exit criteria |
| [DECISIONS.md](../DECISIONS.md) | Architecture decision records (settled and open) |
| [AGENTS.md](../AGENTS.md) | Engineering standard for working in this repository |

Plans in this directory (not published):

| Document | Content |
|---|---|
| [corrective-assurance.md](corrective-assurance.md) | Authoritative current queue: reopened WP mapping and WP-23…WP-56 corrective acceptance |
| [widgets.md](widgets.md) | Widget catalog plus the current corrective feature ledger |
| [api-ergonomics-review-2.md](api-ergonomics-review-2.md) | Historical 2026-08-07 findings and landing record for WP-1…WP-22 |
| [documentation-revamp.md](documentation-revamp.md) | WP-36C plan for source-backed tutorials, widget gallery, screenshots, and documentation gates |
| [editor-architecture.md](editor-architecture.md) | Binding architecture for the document editor, profiles, incremental syntax cache, and file workflow |
| [editor-delivery-target.md](editor-delivery-target.md) | WP-39…WP-44 delivery order, public-client target, and executable acceptance matrix |
| [embedded-terminal-architecture.md](embedded-terminal-architecture.md) | Binding private-PTY/ConPTY terminal containment and Sixel-composition architecture |
| [embedded-terminal-delivery-target.md](embedded-terminal-delivery-target.md) | WP-45…WP-50 terminal delivery order, acceptance matrix, and proof of no-spill graphics |
| [data-flow-architecture.md](data-flow-architecture.md) | Data view and flow architecture |
| [clean-room-derivation.md](clean-room-derivation.md) | D-012 clean-room derivation record for rewritten material |
| [example-coverage.md](example-coverage.md) | Example-to-widget coverage matrix |
| [sysinfo-example.md](sysinfo-example.md) | WP-51…WP-56 system-information, benchmark, comparison, export, help, and documentation target |
| [mac-first-delivery-target.md](mac-first-delivery-target.md) | Mac-first delivery scope |
| [mac-first-traceability.md](mac-first-traceability.md) | Mac-first traceability matrix |
| [mac-performance-baseline.md](mac-performance-baseline.md) | Pinned-host performance baseline |
| [term_capabilities.md](term_capabilities.md) | Observed terminal capability matrix |
| [wp38-appearance-matrix.md](wp38-appearance-matrix.md) | WP-38 appearance matrix: contract, current record, and the findings it exposed |
| [wp38-api-review-findings.md](wp38-api-review-findings.md) | WP-38 API reference review: documentation coverage and suspected defects |
| [../docs/release-audit.md](../docs/release-audit.md) | 0.1 Mac release evidence and final acceptance checklist |

The published client documentation is `docs/` (minus `release-audit.md`); it
is indexed from the public README and must stand on its own without any of the
above.

## Publication

`tools/publish/make-public.sh` builds the public snapshot branch from `main`
minus everything named in `tools/publish/publicignore`. This directory, the
root-level design record, and `docs/release-audit.md` are on that list.
