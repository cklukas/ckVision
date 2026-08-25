# ckVision — Roadmap

Milestones are sequential; each has binding exit criteria. No dates —
sequence and criteria only. Original milestone effort figures are coarse
historical planning aids (focused person-weeks, one senior developer driving
agent tooling); their former 26–34 pw total no longer describes the reopened
scope. The corrective queue carries a new 92–149 pw estimate, subject to WP-23's
provenance/authority rebaseline.

M8–M10 were inserted by the 2026-08-07 API-ergonomics and integration
review ([plans/api-ergonomics-review-2.md](plans/api-ergonomics-review-2.md)).
A 2026-08-08 implementation and verification audit found that the earlier
ledger used `Done` for partial landings even when literal exit criteria or
global gates did not hold. D-034 corrects that status model.

The sole executable work-package queue is now
[plans/corrective-assurance.md](plans/corrective-assurance.md). The instruction
**“work on the next open WP”** means the first `In progress` row, otherwise the
first `Ready` row, in that plan's **Current execution queue**. The old WP-1…WP-22
ledger is historical landing evidence only. Developers must not use its status
labels to choose work.

A milestone is done when its exit criteria hold **and** the global gates
hold: clean build with zero warnings on all target platforms, all tests
green, sanitizers green, machine-independent benchmark gates green (the
wall-clock budgets are checked on the pinned reference machine —
ARCHITECTURE §8), and reference documentation written for everything the
milestone added. The documentation site (M11) assembles per-milestone
docs; it does not backfill them.

## Current acceptance status

No milestone below is currently accepted as `Done`. Substantial code has
landed, but the following exit gaps are reopened and have explicit corrective
owners. Historical milestone scope remains useful; this table is the current
status authority.

| Milestone | Current status | Corrective owner(s) |
|---|---|---|
| M0 | **Reopened** — Windows test portability and complete gate reporting do not hold | WP-32, WP-50, WP-37, WP-38 |
| M1 | **Reopened** — Unicode tables/corpora are incomplete and widget clipping can split UTF-8 | WP-30, WP-38 |
| M2 | **Reopened** — paint-scope junction ownership and binary shadow union have initial fixes and overlap goldens, but per-window backing stores, zero-allocation/cells-touched budgets, contained child-raster composition, and the complete cross-platform golden matrix are not accepted | WP-31, WP-48, WP-38 |
| M3 | **Reopened** — the initial Sixel/no-graphics virtual display and captures have landed, but its full pixel-golden/state matrix, security, fuzzing, degradation/restoration, p99, backend evidence, and embedded-terminal containment are incomplete | WP-27, WP-29, WP-31, WP-32A, WP-33, WP-45…WP-50, WP-37, WP-38 |
| M4 | **Reopened** — root/input/modality/platform-service/zero-allocation/command-context exits and bounded child-session readiness integration are incomplete | WP-24, WP-25, WP-27, WP-28, WP-31, WP-32, WP-35, WP-46, WP-47, WP-38 |
| M5 | **Reopened** — Desktop lifetime, modality, backing stores, menu/catalog, hello, scripts, and goldens are incomplete | WP-25…WP-27, WP-31, WP-34A, WP-35, WP-38 |
| M6 | **Reopened** — catalog features, common modern components, terminal view, accepted paired Sixel/fallback pixel goldens, complete state/scheme scripts, comprehensive example coverage, usable client-facing tutorials/reference docs, and release evidence are incomplete; generated dual-profile captures alone do not close the golden gate | WP-30, WP-32A, WP-34A…WP-34C, WP-36A, WP-36B, WP-36C, WP-45…WP-49, WP-38 |
| M7 | **Open / host-blocked** — Windows backend/full matrix, including embedded ConPTY child sessions, has not met its exit | WP-32, WP-50, WP-37; host availability remains explicit |
| M8 | **Reopened** — corrected WP-24…WP-26 implementations still require example integration and the global matrix | WP-35, WP-38 |
| M9 | **Reopened** — WP-10 is unfinished and theme, modality, command-context, and remaining example exits are unmet | WP-27, WP-35, WP-38 |
| M10 | **Reopened** — WP-18…WP-22 landed code but the specified visible/script/golden/reference acceptance is incomplete | WP-36, WP-38 |
| M11 | **Open** — editor/language-profile, contained-terminal, and SysInfo proof-point implementations have current-host evidence; release remains blocked until the M7/global gates hold | WP-39…WP-56, WP-37, WP-38 |

## M0 — Bootstrap (≈1 pw)

Repository skeleton: CMake (strict central settings module), CI matrix
(macOS, Linux, Windows), test/benchmark harness wiring, the golden
styled-cell dump format specified and implemented, AGENTS.md enforcement
(include-cycle and layer-dependency checks).

**Exit:** empty library + one trivial test + one trivial benchmark build
and run green on all three CI platforms; a hand-authored dump file
round-trips through the golden format parser/serializer byte-exactly
(format-level only — Surface capture arrives with M2).

## M1 — core (≈1–2 pw)

Geometry and pixel-space types, Color/Style/Cell (with control-character
neutralization at the text-to-Cell boundary), Image, key model, event
structs, Error, Clock; UTF-8 grapheme segmentation and column-width from
versioned compiled-in tables; the diagnostics sink interface.

**Exit:** width/grapheme corpus tests green (ASCII, East Asian wide,
combining, zero-width, emoji sequences — including the known
terminal-divergent cases marked as such for D-019's later use); hostile
display-text neutralization golden-tested; no platform includes anywhere
in the layer; documented width policy.

## M2 — scene (≈2–3 pw)

Surface with row-damage, Painter (runs, fills, box drawing with junction
merging, clipping, style transforms, raster-region placement), raster
regions (cell-anchored Images with mandatory fallbacks and occlusion
slicing), Compositor (z-order, occlusion, damage-only composition),
shadow pass, scroll-intent annotations, cursor state.

**Exit:** golden suites for clipping, junction merging, occlusion, and
shadows (windows and menus dim underlying content correctly; each footprint is
non-overlapping and any number of overlapping shadows remains one uniform
binary shadow); junction coverage proves that same-view rules can join
their own frames while an overlapping foreground view never merges with a
lower-z frame; raster-region goldens for anchoring, occlusion
slicing, scrolling, and fallback activation via the symbolic dump
representation; a captured Surface round-trips through the golden format
byte-exactly; compose-stage machine-independent budgets (cells touched,
zero allocations) enforced in CI.

## M3 — term: POSIX + headless + record/replay (≈3–4 pw)

Terminal interface (core-typed); headless backend (scripted input, named
capability profiles, raw-byte and symbolic-frame capture, plus a deterministic
virtual display that decodes the emitted VT/Sixel stream into cell and RGBA
planes); record/replay backend (events including
probe responses, presented bytes); POSIX backend (raw mode, poll loop,
self-pipe, SIGWINCH coalescing, suspend/resume, session-state ledger,
alternate-screen policy, panic-safe restore); capability profiles +
probes with the explicit lifecycle (baseline-first-frame, bounded
deadlines on the injected clock, capability-changed events) — including
graphics detection (DA1, XTSMGRAPHICS), cell pixel metrics (XTWINOPS,
re-probed on resize), width policy (D-019), color-scheme hint
(OSC 10/11, mode 2031), hyperlinks (OSC 8); tmux/screen and Linux-console
profiles; input decoder (legacy with Clock-driven ESC disambiguation,
modifyOtherKeys, kitty keyboard; SGR mouse and SGR-Pixels dual-space
decoding; sanitized bracketed paste; focus events); presenter (cell
diff, width-safe run optimization with absolute re-addressing, scroll
shifts from scene annotations, synchronized output, OSC emission safety,
Sixel slice emission with encoding cache); system clipboard write
(OSC 52); the diagnostics stderr capture/flush policy.

**Exit:** interactive echo demo on macOS and Linux; p99 < 2 ms
event-to-bytes measured by the scripted PTY harness with timestamped
injection on the reference machine; image demo with correct occlusion
and scrolling on macOS and Linux, plus its fallback rendering under a
no-graphics profile; the identical image-demo script produces decoded
pixel-plane visual goldens under a fixed-metrics Sixel profile and fallback-only
goldens under NoGraphics, including runtime profile changes with no stale
pixels; dual-space mouse events verified headlessly and
interactively; decoder and sanitizer fuzz targets in CI with zero
findings; hostile-paste and OSC-injection cases golden-tested; the
degradation matrix (color depth, keyboard protocol, pixel mouse,
graphics, width policy, sync output, tmux/screen, Linux console, probe
non-response) covered headlessly; PTY-driven restoration verified on
normal exit, exception, abort/assertion, and a fatal signal in a child
process, with diagnostics emitted after restore; suspend/resume
round-trip re-establishes all session state with a byte-identical
re-presented frame.

## M4 — ui core (≈3 pw)

View tree with ownership and invalidation; the batched frame loop built
on public `step()`/`wake()` (D-021); event routing (positional with
capture, focused, notification with typed registration); focus and
traversal rules; layout containers with the height-for-width carve-out;
descriptor-based dialog materialization; command registry (reserved
ranges, contexts), keymaps, accelerator resolution; the theme system
(interned roles, fallback-declaring registration, scope
materialization, runtime switching) with the initial **Classic** scheme;
help-context keys on views with F1 routing to a registered provider;
internal clipboard and history registry; injected filesystem interface;
timers on the injected clock; `Application::post`.

Minimal widget set (marked M4 in the catalog): Label, Static text,
Button, Input line (core editing feature set).

**Exit:** a complete form demo — an undecorated root dialog surface with
the four M4 widgets, correct Tab/Shift-Tab/mnemonic/Enter/Esc behavior —
running headlessly and interactively; the focus specification runs as
headless event scripts; dialog-accept validation veto scripted;
steady-state zero-allocation dispatch verified by instrumented tests;
two Application instances on two PTYs in one process tested; the loop
driven externally via `step()`/`wake()` in a test host.

## M5 — Windows, menus, chrome (≈3–4 pw)

Window (frame, title, close/zoom, resize grips, backing store, shadow),
Desktop (z-order, activation, cycling, select-by-number, window list,
tile/cascade, snapshot/restore API), the **popup** node (menus,
dropdowns, context menus — composition, dismissal grammar, hit-test
priority), menu system (bar, drop-downs, nested submenus, mnemonics,
accelerator hints, separators, check/disable, context menus, shadows),
status line with context hints, modality stack, close-request/veto
protocol on every close path, message boxes, the min-size/shrink policy,
and the inspection overlay (view tree, traversal order, theme-role
resolution, damage visualization).

Deliverable before the exit review: `plans/classic-look.md` — the
provenance-safe written specification of the classic visual baseline
(frame glyphs, shadow geometry, palette roles, menu metrics — from
documentation and observation, per AGENTS.md).

This is the **classic-look milestone**: from here on, a ckVision
application looks and feels like the best of the classic desktops — or
better.

**Exit:** gallery goldens conform to the checked-in classic-look
specification, signed off by the project owner against its checklist;
`examples/hello` realizes the VISION one-screen sentence verbatim — menu
bar, status line, one dialog — within an enforced 60-line budget checked
in CI, with golden coverage; window move-drag benchmark shows
composition-only cost with zero content repaints, and resize-drag stays
confined to the resized window's subtree; modal scoping, close-veto
(including quit sweep), and focus-restore event scripts green;
shrink-storm scripts green down to and below the minimum supported size;
menu system fully operable by keyboard alone and mouse alone.

## M6 — Widget completion (≈6–8 pw, three slices)

Everything in [plans/widgets.md](plans/widgets.md), delivered in three
gated slices:

- **M6a — scrolling and selection:** scrollbars, scroll viewport, list
  views, check/radio groups, combo boxes, tab control, progress,
  splitter.
- **M6b — text and data:** input-line completion (validators, masks,
  history via the registry, undo), memo (with undo), table, tree, text
  view (links rendered as OSC 8 where available), image view, canvas.
- **M6c — standard dialogs and help:** file open/save and directory
  dialogs on the injected filesystem, window list, message-box polish,
  the Help viewer, first versions of the Dark, Light, and Mono schemes
  (required here — the exit exercises every scheme).

**Exit (per slice and overall):** the catalog's baseline and
beyond-baseline columns implemented; every widget has golden appearance
tests (all four states, every built-in scheme), interaction scripts, and
reference documentation; file-dialog goldens run against the scripted
in-memory filesystem; the gallery exercises every widget in every
built-in scheme. Raster-bearing widgets additionally have symbolic scene
goldens plus decoded pixel-plane visual goldens under Sixel and fallback-only
visual goldens under NoGraphics, driven by the same scripts.

## M7 — Windows platform + full matrix (≈2 pw; deferred until a Windows host is available — M8–M10 proceed first)

Windows backend (ConPTY VT output, VT-first input per D-020 with
console-API supplements, native clipboard read/write, per-host degraded
capability rows); CI matrix complete; cross-platform golden equality.

**Exit:** gallery — including Sixel graphics and pixel-mouse
interaction — fully functional in Windows Terminal; all suites green on
the three-platform matrix including PTY/ConPTY smokes; byte-identical
goldens across platforms; degraded-host rows (plain-VT ConPTY without
enhanced input) tested; legacy console refusal path tested.

## M8 — Integration correctness: the unplugged seams (≈1–2 pw)

**Acceptance status: reopened.** WP-1…WP-6 produced useful partial landings.
WP-24…WP-26 now have implementation-complete corrective evidence; WP-35 and
WP-38 own the remaining example and global acceptance criteria.

Repairs the review's findings F1–F7 — cases where a mechanism is
implemented and unit-tested but the wiring that lets a real terminal
session reach it was never made. Work packages WP-1…WP-6 in
[plans/api-ergonomics-review-2.md](plans/api-ergonomics-review-2.md):
the resize chain from `ResizeEvent` through root to the Desktop
(the Application-owned fill-the-root policy, independent of `View`'s public
bounds observer);
click-to-focus in dispatch; click-to-activate/raise via a capture-phase
mouse-descent hook Desktop implements; a working, lifecycle-bound zoom control,
zoomed windows tracking desktop growth, and the
per-window `DesktopGrowPolicy`; explicit modeless/modal dialog-presentation
paths plus correct generic Desktop attachment for every standard-dialog handle;
example hygiene (gallery on docking and `present_modal`, redundant
focus-policy calls removed).

**Exit:** end-to-end headless resize script through
`Application::step` — grow and shrink — asserting chrome re-pins to the
new edges, windows stay reachable, zoomed/`KeepFilling` windows track,
and no stale cells remain, with goldens per step; interaction scripts
for click-to-focus (typing follows the click) and click-to-activate
(buried window raises, frames restyle, WP-2 composes with WP-3); a
message box presented over a desktop shows the active frame and
participates in cycling; the zoom glyph works by mouse on a
desktop-owned window; `docs/example-apps.md`'s resize claims are true
and carry test references; every M8 behavior change is a library fix,
with examples only consuming public API.

WP-24 supplies the resize portion through
`tests/test_m8_integration.cpp` and `tests/test_root_resize_golden.cpp`, with
the checked-in `tests/golden/root_resize_grow.dump` and
`tests/golden/root_resize_shrink.dump` frames.

WP-25 supplies the safe pointer-routing and activation portion through
`tests/test_application.cpp` and `tests/test_window_activation_golden.cpp`,
with `tests/golden/window_activation_before.dump` and
`tests/golden/window_activation_after.dump` pinning the active/inactive frame
transition.

WP-26 supplies lifecycle-safe Desktop ownership and direct standard-dialog
attachment through polymorphic `View` insertion/removal; private, bound zoom
targets; modal-detachment cleanup; terminal-input zoom/restore; and
`tests/test_desktop_dialog_golden.cpp` with active/cycled/closed dialog frames.
The remaining M8 example integration is owned by WP-35; the portable release
matrix is owned by WP-38.

## M9 — API ergonomics: the 60-line hello (≈2–3 pw)

**Acceptance status: reopened.** Legacy WP-10 remains incomplete, and the
modality, theme-scope, command-context, and remaining example acceptance gaps
are owned by WP-27 and WP-35. WP-23 owns the separate D-012 provenance and
60-line gate repair.

Delivers the decided-but-unbuilt convenience layer (review findings
E1–E10, F8, F9; work packages WP-7…WP-17; decisions D-028/D-029):
widgets resolve their own theme roles through a root-installed ui
context with per-widget overrides and per-window theme scope
(constructors take content, not plumbing); `StandardRoles` completed
for the M6 widget set; typed `add<T>`/`make<T>` insertion; the
one-call command registration from ARCHITECTURE §5 (metadata, handler, and
chord in one descriptor) and its widgets-layer command-presentation
counterpart: menu/status entries may supply a surface-specific display label
and mnemonic while retaining one `CommandId`, handler, chord, and enablement
source of truth. It also supplies live, mutable `MenuBar`/`StatusLine`
structure — runtime add/remove of items and top-level menus, enablement dimming
on the status line, context-sensitive status hints, command retraction — see
the plan's revised WP-10; the
Key→display-name table with `KeyChord` parse/format, and menus/status
lines rendering title, chord, and enablement live from the registry;
the standard command set declared by every `CommandRegistry`;
the default keymap (Tab/Shift-Tab traversal, F10 menu, window
management) over rebindable bindings; `Application::run()` and
`request_quit()`; the modality stack with event scoping; explicitly named
modeless and modal presentation APIs; a typed, non-blocking presentation
completion handle; and the blocking outer-loop-only `exec_*` conveniences
(completing D-021). An optional widgets-layer application-shell composition
helper constructs the ordinary theme/desktop/menu/status skeleton declaratively
without adding a global, a second event loop, or a dependency from `ui` into
`widgets`; size-hint-change propagation replacing
`Window::relayout_frame_overlay`; `examples/hello` rewritten to the
target shape.

**Exit:** hello (app + main combined) within an enforced 60-line
non-comment budget checked in CI, with golden coverage — the M5 exit
criterion finally wired; every command in the examples declared exactly
once; no hand-encoded chord text and no `static_cast<widgets::` in
`examples/`; one command can intentionally present as `Exit` in a File menu
and `Quit` in a status line without a duplicate handler or a hand callback;
modal versus modeless presentation is explicit at every call site; typed
non-blocking completion, blocking `exec_message_box`, quit-sweep integration,
and runtime rebinding re-rendering menus/status lines all covered by scripts.
The application-shell helper is exercised by a one-screen example without
hiding ownership or loop control. `docs/hello-example.md` records the example's
ckVision-owned behavior contract and its golden/script evidence; theme
switching and per-window theme override golden-tested across all four
schemes.

## M10 — Layout and widget-model completion (≈2–3 pw)

**Acceptance status: reopened.** WP-18…WP-22 landed mechanisms, but WP-36 owns
the missing literal interaction scripts, appearance goldens, height-for-width
behavior, standard-dialog application, and reference evidence.

Completes D-006's decided container set and the data-widget model
(review packages WP-18…WP-22): `Anchors` for fixed-geometry children
(the modern per-view grow flags) with `AnchorPane`, honored by window
content; `Grid` (spans), `Dock` (four edges + center), `Overlay`,
per-child cross-axis alignment and margins for Row/Column, `Splitter`;
frame overlays plural and positioned (`add_frame_overlay` with
edge/alignment slots); dialog defaults — descriptor-built dialogs
non-resizable unless opting into resizable-with-layout, Esc-cancel by
default — and the minimum-terminal-size policy's degraded-chrome /
"terminal too small" end state; `TreeNode` user payload (id +
`std::any`), lazy population via `on_expand_request`, node/index
arguments on selection callbacks, Left-to-parent navigation; the file
browser rebuilt on lazy expansion, a Splitter, and payloads instead of
the sidecar path map.

**Exit:** anchor/grid/dock/overlay/alignment suites in the established
layout-test style (pure distribution math unit-tested, scripts for
behavior, goldens for appearance); a resized dialog keeps
bottom-right-anchored children in the corner and stretches
anchored-all-edges children; two frame overlays coexist and track
resize and size-hint changes; shrink-storm script reaches the
"terminal too small" state and recovers; filebrowser lazily populates
through the injected filesystem with unchanged observable behavior
plus new lazy/split coverage; every new container and model feature
reference-documented.

## M11 — editor, embedded terminal, graphics polish, themes, docs, release (≈48–74 pw)

**Entry status: Mac-first finishing active.** WP-39…WP-49 and the SysInfo
WP-51…WP-56 proof point have implementation evidence on the current Mac host.
The active finishing sequence is defined in
[plans/mac-first-delivery-target.md](plans/mac-first-delivery-target.md): close
the observable release gates, finish interaction and visual polish, harden
performance and resilience, stabilize packaging, and perform the final Mac
release audit.

First-class document/source editor and file-editor workflow: revisioned UTF-8
editing, undo/redo, search/replace, selection/gutter/indicator, deterministic
profile detection, incremental syntax styling, and shipped JSON/YAML/Bash
profiles with documented client extension. The binding design and delivery
target are [plans/editor-architecture.md](plans/editor-architecture.md) and
[plans/editor-delivery-target.md](plans/editor-delivery-target.md). Also:
private embedded terminal sessions: explicit process lifecycle, off-screen
terminal emulation, resize/input/focus, nested ckVision child applications,
and decoded Sixel content re-composited with no escape beyond the window. The
binding design and delivery target are
[plans/embedded-terminal-architecture.md](plans/embedded-terminal-architecture.md)
and [plans/embedded-terminal-delivery-target.md](plans/embedded-terminal-delivery-target.md).
The [SysInfo example](plans/sysinfo-example.md) demonstrates an injected
host-information service, live refresh, comparative charts, cancellable
background benchmarks, contextual help, and injected-filesystem report export.
Further M11 scope: graphics polish (kitty-graphics secondary encoder, Sixel palette optimization);
scheme polish; the theme-editor standard dialog; release documentation and
examples; fuzz/benchmark hardening.

Entry gate — WP-34A…WP-34C have closed or formally re-scoped every catalog
row, WP-36A has a complete executable example coverage matrix, WP-36B has
closed or explicitly deferred the common modern components, WP-36C has produced
source-backed client tutorials, screenshots, widget/API coverage docs, and
doc-generation gates, WP-39…WP-49 and WP-51…WP-56 have met their current-host
delivery targets, and WP-38-MAC has a traceability report with no unexplained Mac
acceptance gap. Release work does not inherit known corrective debt.

**Exit — 0.1 Mac release criteria:** every VISION.md non-negotiable
demonstrably works in the gallery/showcase examples on the Mac host, including
the document editor and its standard language profiles plus the contained
terminal view and its child-session/Sixel scenarios; documentation covers
every public API; the tutorial walkthrough is executed from a clean Mac
checkout and reaches a running hello in under ten minutes from documented
prerequisites; license decision (D-OPEN-1) resolved.

## Post-0.1

- **1.0 hardening:** public-API review, semver commitment, deprecation
  policy from 1.0 onward.
- Adoption by real applications: adopter-reported capability gaps are
  fixed in the library, never worked around in applications.
- Exploration: screen-reader/accessibility strategies for TUIs,
  bidirectional text (D-026), emerging width/text protocols (D-OPEN-7),
  package-registry distribution.
