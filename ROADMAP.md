# ckVision — Roadmap

Milestones are sequential; each has binding exit criteria. No dates —
sequence and criteria only. Effort figures are coarse planning aids
(focused person-weeks, one senior developer driving agent tooling);
the full sequence totals roughly 26–34 pw.

M8–M10 were inserted by the 2026-08-07 API-ergonomics and integration
review ([plans/api-ergonomics-review-2.md](plans/api-ergonomics-review-2.md),
"the review" below): M8 repairs seams whose mechanisms are built and
unit-tested but never wired end to end; M9 delivers the decided-but-
unbuilt ergonomics layer down to the enforced 60-line hello; M10
completes the decided layout system. The former M8 (release) is now
M11, unchanged in content. M7 (Windows) is deferred until a Windows
host is available and does not block M8–M10. Historical milestone
texts (M5/M6) are left as written; where one of their exit items is
still open — the enforced 60-line hello, the modality stack, backing
stores, the menu-system remainder, and peers — the
[plans/widgets.md](plans/widgets.md) open-items ledger records it with
its new landing milestone, so the sequence stays honest without
rewriting history.

A milestone is done when its exit criteria hold **and** the global gates
hold: clean build with zero warnings on all target platforms, all tests
green, sanitizers green, machine-independent benchmark gates green (the
wall-clock budgets are checked on the pinned reference machine —
ARCHITECTURE §8), and reference documentation written for everything the
milestone added. The documentation site (M11) assembles per-milestone
docs; it does not backfill them.

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
shadows (windows and menus dim underlying content correctly, including
overlapping shadows); raster-region goldens for anchoring, occlusion
slicing, scrolling, and fallback activation via the symbolic dump
representation; a captured Surface round-trips through the golden format
byte-exactly; compose-stage machine-independent budgets (cells touched,
zero allocations) enforced in CI.

## M3 — term: POSIX + headless + record/replay (≈3–4 pw)

Terminal interface (core-typed); headless backend (scripted input, frame
capture, capability profiles); record/replay backend (events including
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
no-graphics profile; dual-space mouse events verified headlessly and
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
built-in scheme.

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

Repairs the review's findings F1–F7 — cases where a mechanism is
implemented and unit-tested but the wiring that lets a real terminal
session reach it was never made. Work packages WP-1…WP-6 in
[plans/api-ergonomics-review-2.md](plans/api-ergonomics-review-2.md):
the resize chain from `ResizeEvent` through root to the Desktop
(fill-the-root policy plus a settable bounds observer on `View`);
click-to-focus in dispatch; click-to-activate/raise via a capture-phase
mouse-descent hook Desktop implements; a working zoom control
(`zoom_area_provider`), zoomed windows tracking desktop growth, and the
per-window `DesktopGrowPolicy`; one sanctioned dialog-attach path
(`Desktop::show_dialog`) used by every standard-dialog handle; example
hygiene (gallery on docking and `show_dialog`, redundant
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

## M9 — API ergonomics: the 60-line hello (≈2–3 pw)

Delivers the decided-but-unbuilt convenience layer (review findings
E1–E10, F8, F9; work packages WP-7…WP-17; decisions D-028/D-029):
widgets resolve their own theme roles through a root-installed ui
context with per-widget overrides and per-window theme scope
(constructors take content, not plumbing); `StandardRoles` completed
for the M6 widget set; typed `add<T>`/`make<T>` insertion; the
one-call command registration from ARCHITECTURE §5 (metadata + handler
+ chord + menu item + status item from one descriptor); the
Key→display-name table with `KeyChord` parse/format, and menus/status
lines rendering title, chord, and enablement live from the registry;
the standard command set below id 4096 registered by `Application`;
the default keymap (Tab/Shift-Tab traversal, F10 menu, window
management) over rebindable bindings; `Application::run()` and
`request_quit()`; the modality stack with event scoping and the
blocking `exec_*` conveniences (completing D-021), plus non-blocking
`show_*` one-liners; size-hint-change propagation replacing
`Window::relayout_frame_overlay`; `examples/hello` rewritten to the
target shape.

**Exit:** hello (app + main combined) within an enforced 60-line
non-comment budget checked in CI, with golden coverage — the M5 exit
criterion finally wired; every command in the examples declared exactly
once; no hand-encoded chord text and no `static_cast<widgets::` in
`examples/`; modal scoping, blocking `exec_message_box`, quit-sweep
integration, and runtime rebinding re-rendering menus/status lines all
covered by scripts; `docs/turbo-vision-conversion.md` regenerated from
the new hello with its Part-III corrections applied, its side-by-side
now favoring ckVision without caveats; theme switching and per-window
theme override golden-tested across all four schemes.

## M10 — Layout and widget-model completion (≈2–3 pw)

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

## M11 — graphics polish, themes, docs, release (≈2 pw)

Graphics polish (kitty-graphics secondary encoder, Sixel palette
optimization); scheme polish; the theme-editor standard dialog; the
documentation site assembled from per-milestone reference docs plus
tutorial and extension guides (worked examples for every §14 extension
point); examples finalized; fuzz/bench hardening pass.

Entry gate — the catalog remainder ledger in
[plans/widgets.md](plans/widgets.md) (window backing stores and
damage-scoped painting per the performance charter, nested submenus
and checkable menu items, combo box, tab control, progress, the
localization string table, desktop snapshot/restore) is worked off
here or explicitly re-scoped by a recorded decision; the release does
not ship with the ledger silently open.

**Exit — 0.1 release criteria:** every VISION.md non-negotiable
demonstrably works in the gallery on all three platforms; documentation
covers every public API; the tutorial walkthrough is executed by script
in a clean container/VM per platform and reaches a running hello in
under ten minutes from documented prerequisites; license decision
(D-OPEN-1) resolved.

## Post-0.1

- **1.0 hardening:** public-API review, semver commitment, deprecation
  policy from 1.0 onward.
- Adoption by real applications: adopter-reported capability gaps are
  fixed in the library, never worked around in applications.
- Exploration: screen-reader/accessibility strategies for TUIs,
  bidirectional text (D-026), emerging width/text protocols (D-OPEN-7),
  package-registry distribution.
