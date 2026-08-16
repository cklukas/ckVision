# ckVision

**ckVision** is a full-featured, windowed terminal-UI framework for modern
terminals, currently in its planning phase — overlapping movable windows,
drop-down menus with mnemonics and shadows, modal dialogs, complete
keyboard and mouse interaction, themes, first-class Sixel graphics with
pixel-precise mouse input, and a damage-tracked rendering pipeline that
repaints only what changed.

It is designed in C++20 with **zero mandatory dependencies**, targets
**macOS, Linux, and Windows (Windows Terminal / ConPTY)**, and is built to
be **easy to use, extensible, deterministic, and fast**.

ckVision is inspired by the interaction grammar of the classic desktop-in-a-
terminal applications of the early 1990s — the standard set by Turbo Vision —
and re-imagines it on modern grounds: modern C++, modern terminals, modern
library architecture. It shares no code and no API with any prior framework.

The library name is `cvision` (CMake target `ckvision::cvision`).

## Status

Substantial implementation has landed through M10, but **no milestone is
currently acceptance-complete**. A 2026-08-08 audit reopened M0–M6 and M8–M10
because literal exit criteria, cross-platform gates, security/performance
evidence, widget coverage, and documentation remain incomplete. Passing local
Release and ASan/UBSan suites is useful evidence, not the full definition of
done.

The authoritative task source is the
[corrective assurance plan](plans/corrective-assurance.md). “Work on the next
open WP” means its first `In progress` row, otherwise its first `Ready` row.
Earlier `Done` labels are historical landing records and must not be used to
skip reopened work.

- **M0-M2**: build skeleton; core primitives (geometry, Color/Style/
  Cell/Image, keys/events, the Unicode services in
  [docs/text-width.md](docs/text-width.md)); the scene layer
  (`Surface`/`Painter`/`Compositor` with damage tracking, junction
  merging scoped by view provenance so unrelated z-levels never alter one
  another's borders, binary/non-cumulative shadow occlusion, raster occlusion
  slicing).
- **M3 (term)**: the `Terminal` interface and `Capabilities` model; an
  incremental input decoder (legacy/kitty/modifyOtherKeys keys, SGR and
  SGR-Pixels mouse, sanitized bracketed paste, focus events, OSC 10/11
  and DECRPM probe responses — coverage documented in
  [docs/input-decoder.md](docs/input-decoder.md)); a `Presenter` (cell
  diffing, D-019 width-safe cursor re-addressing, color-depth
  degradation, synchronized output, OSC-safe emission, cropped Sixel
  raster overlay); a correct public-protocol Sixel encoder; headless,
  POSIX (real termios/signals/session-registry, PTY-tested), and
  record/replay backends.
- **M4 (ui core)**: `View` (the ui tree's base class), `Application`
  (focus traversal, positional/focused/command-keymap event dispatch,
  mouse input capture, timers, F1 context-help routing, the embeddable
  `step()`/`wake()` loop), the flat interned-role `Theme` system,
  `Row`/`Column` layout, the internal clipboard, `HistoryRegistry`,
  declarative dialog descriptors with validated materialization, and
  the four minimal controls (Label, StaticText, Button, InputLine).
- **M5 (windows/menus)**: `Window` (frame chrome, move/resize, the
  vetoable close protocol, zoom), `Desktop` (z-order, activation,
  cycling, tile/cascade, popups), dialog accept-validation veto with
  Esc-cancel, message boxes with every standard button set, and the
  `MenuBar`/`DropdownMenu`/context-menu system with light-dismiss.
- **M6 (widget completion)**: `Scrollbar`, `ScrollViewport`,
  `CheckGroup`/`RadioGroup`, `ListView`, `TreeView`, `TextView`,
  `Memo`, `Table`, `ImageView`, `Canvas`; InputLine completion
  (validators, input masks, password echo, history cycling); the
  Dark/Light/Mono theme schemes; the window-list dialog, file open/
  save dialog and directory picker (both on the injected
  `FileSystem`), and the Help viewer.

Independent review passes across M4, M5, and M6 found and fixed eleven
real bugs (five use-after-frees, five correctness issues, one z-order/
lifecycle desync), each landed with a regression test verified under
ASan/UBSan.

**Post-M6, the render pipeline and example apps landed**: the gap
where `Application::step()` dispatched input but nothing ever painted
anything is closed (`View::paint_children`, reusing
`scene::Compositor`/`term::Presenter`); window shadows actually
composite (`widgets::Desktop::paint_children`). Two example apps —
`examples/gallery` (rendering, keyboard/mouse navigation, Sixel images)
and `examples/filebrowser` (a `TreeView`-drives-`ListView` master-
detail folder browser over the real disk, via the new
`term::PosixFileSystem`) — with a matching documentation page and
generated screenshots, rendered to HTML/PDF via CK Office Write's
`ckwrite` (see [docs/example-apps.md](docs/example-apps.md) and
`tools/docgen/generate_docs.sh`).

A first ergonomics/resize review added the Desktop-level mechanisms:
`Desktop::dock_top`/`dock_bottom` + `content_area()` +
`Window::reposition_within` (chrome pinning and window clamping when
the *Desktop's* bounds change), `TreeView` selection/activation
callbacks (the master-detail hook), and a widget on the window's own
border, e.g. a "current line" indicator (`Window::add_frame_overlay`
as of the second review below — plural, positioned overlays replacing
this first review's original single-slot `set_frame_overlay`).

The 2026-08-07
[API-ergonomics and integration review](plans/api-ergonomics-review-2.md)
is a compact historical index for WP-1…WP-22. The corrective audit
found that several mechanisms landed without their specified end-to-end
acceptance and exposed additional lifetime, modality, provenance, Unicode,
performance, portability, fuzzing, Sixel visual-capture, catalog, and
documentation gaps. WP-23…WP-50
now own those repairs explicitly; ROADMAP's Current acceptance status table
maps every reopened milestone to them.

## Documents

## Learn the library

New clients should begin with [getting started](docs/getting-started.md), then
work through the complete [Hello tutorial](docs/tutorial-hello.md) and the
[object model](docs/object-model.md). The [widget gallery](docs/widget-gallery.md)
and [example applications](docs/example-apps.md) connect every public widget to
real source, generated screenshots, and executable evidence. Regenerate the
documentation visuals and rendered outputs with `tools/docgen/generate_docs.sh`.

| Document | Content |
|---|---|
| [VISION.md](VISION.md) | Mission, non-negotiable capabilities, design principles, non-goals |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Target architecture: layers, rendering pipeline, event/command system, backends |
| [ROADMAP.md](ROADMAP.md) | Development milestones with exit criteria |
| [DECISIONS.md](DECISIONS.md) | Architecture decision records (settled and open) |
| [plans/widgets.md](plans/widgets.md) | Widget catalog plus the current corrective feature ledger |
| [plans/api-ergonomics-review-2.md](plans/api-ergonomics-review-2.md) | Historical 2026-08-07 findings and landing record for WP-1…WP-22 |
| [plans/corrective-assurance.md](plans/corrective-assurance.md) | Authoritative current queue: reopened WP mapping and WP-23…WP-50 corrective acceptance |
| [plans/documentation-revamp.md](plans/documentation-revamp.md) | WP-36C plan for source-backed tutorials, widget gallery, screenshots, and documentation gates |
| [plans/editor-architecture.md](plans/editor-architecture.md) | Binding architecture for the document editor, profiles, incremental syntax cache, and file workflow |
| [plans/editor-delivery-target.md](plans/editor-delivery-target.md) | WP-39…WP-44 delivery order, public-client target, and executable acceptance matrix |
| [plans/embedded-terminal-architecture.md](plans/embedded-terminal-architecture.md) | Binding private-PTY/ConPTY terminal containment and Sixel-composition architecture |
| [plans/embedded-terminal-delivery-target.md](plans/embedded-terminal-delivery-target.md) | WP-45…WP-50 terminal delivery order, acceptance matrix, and proof of no-spill graphics |
| [docs/example-apps.md](docs/example-apps.md) | Example applications: what each one demonstrates, how it's tested, generated screenshots |
| [docs/getting-started.md](docs/getting-started.md) | Build/run path and minimal host application |
| [docs/client-handoff.md](docs/client-handoff.md) | Installable SDK, generated visual documentation, bundle workflow, and terminal troubleshooting |
| [docs/release-audit.md](docs/release-audit.md) | Current 0.1 Mac release evidence and final acceptance checklist |
| [docs/tutorial-hello.md](docs/tutorial-hello.md) | Complete Hello source, hierarchy, walkthrough, and generated frames |
| [docs/object-model.md](docs/object-model.md) | Application/view/Desktop/window ownership, focus, events, and painting |
| [docs/layout-guide.md](docs/layout-guide.md) | Layout container selection and resize evidence |
| [docs/widget-gallery.md](docs/widget-gallery.md) | Every public widget/component with usage and visual evidence |
| [docs/dialogs-and-commands.md](docs/dialogs-and-commands.md) | Commands, dialogs, validation, focus restoration, and wizards |
| [docs/themes-and-rendering.md](docs/themes-and-rendering.md) | Theme roles and client-level render model |
| [docs/graphics.md](docs/graphics.md) | ImageView/Canvas and Sixel/no-graphics behavior |
| [docs/platform-services.md](docs/platform-services.md) | Terminal/clock/filesystem/clipboard host boundary |
| [docs/editor.md](docs/editor.md) | Revisioned documents, TextEditor, language profiles, search/replace, and safe file workflow |
| [docs/api-index.md](docs/api-index.md) | Curated public-header index |
| [docs/coverage.md](docs/coverage.md) | Machine-checked docs-to-header/example/screenshot/test traceability |
| [docs/hello-example.md](docs/hello-example.md) | Hello verification appendix and golden evidence |
| [docs/performance.md](docs/performance.md) | Deterministic allocation/cost gates and the pinned-host p99 procedure |
| [plans/clean-room-derivation.md](plans/clean-room-derivation.md) | D-012 clean-room derivation record for rewritten material |
| [AGENTS.md](AGENTS.md) | Engineering standard for working in this repository |

## Copyright

Copyright 2026 Dr. Christian Klukas. All rights reserved.
The open-source license under which ckVision will be published is decided at
first public release (see DECISIONS.md, D-OPEN-1).
