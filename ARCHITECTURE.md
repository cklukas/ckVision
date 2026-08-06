# ckVision — Target Architecture

This document defines the architecture the implementation must conform to.
It is binding: deviations are design changes and go through DECISIONS.md.

## 1. Layer model

Five library layers with strictly downward dependencies. Each layer is
independently testable; `core` and `scene` do no I/O at all. Graphics are
not a module on the side: raster support is woven through every layer
(image primitive in `core`, raster regions in `scene`, encoders and
pixel-aware input in `term`, image widgets in `widgets`) — see §7.

```
┌───────────────────────────────────────────────────────────────┐
│  widgets   buttons, inputs, lists, menus, tabs, dialogs,      │
│            images, canvases, scrollers, trees, tables, ...    │
├───────────────────────────────────────────────────────────────┤
│  ui        view tree, windows, popups, desktop, focus,        │
│            events, commands, keymaps, layout, themes, help    │
│            contexts, clipboard, histories, timers, loop       │
├───────────────────────────────────────────────────────────────┤
│  scene     surfaces, painter, raster regions, compositor,     │
│            damage, shadows, cursor state, frame assembly      │
├───────────────────────────────────────────────────────────────┤
│  term      terminal backends, capabilities, input decoder,    │
│            frame presenter, graphics encoders, platform       │
│            services (filesystem, system clipboard bridge)     │
├───────────────────────────────────────────────────────────────┤
│  core      geometry, text/width, color, style, cell, image,   │
│            events, keys, error, clock — no I/O, no platform   │
└───────────────────────────────────────────────────────────────┘
```

Allowed dependencies: `widgets → ui → scene → core`; `ui → term` only
through the `Terminal` and platform-service interfaces; `scene → core`;
`term → core`. The `Terminal` interface exchanges **core types only**
(frame data views, events, capabilities), keeping `term` independent of
`scene`. Nothing depends on `widgets` except applications. No layer
reaches around a lower layer. No cycles, enforced by include checks in CI.

Environment and platform access follow the same discipline: `core`,
`scene`, `ui`, and `widgets` never read the environment, the filesystem,
or the clock directly — backends receive the environment explicitly at
construction, and applications consume platform services (filesystem,
clock, clipboard bridge) through injected interfaces (§5, D-025).

Namespace: `ckv::` (sub-namespaces per layer only where they pay for
themselves). Public headers: `<cvision/...>`. CMake target:
`ckvision::cvision`.

## 2. core — primitives

Value types used by everything above: `Point`, `Size`, `Rect` in cell
space and `PixelPoint`, `PixelSize` in pixel space (distinct types — the
two coordinate spaces never mix silently); `Color` (24-bit RGB +
default/palette handles), `Style` (fg, bg, attributes), `Cell` (grapheme +
style + width); `Image` (owned RGBA pixel buffer with size and stride —
the raster primitive everything above shares); key model (`Key`,
`KeyChord`: key + modifiers, printable text carried separately); event
payload structs; `Error`; a monotonic `Clock` interface (injectable — the
only time source the library ever uses).

Text handling is UTF-8 in `char`/`std::string_view` everywhere. `core`
owns grapheme segmentation and column-width computation (East Asian wide,
zero-width, combining, emoji sequences) from compiled-in, versioned
Unicode tables. Width policy is a single, central, testable function; no
widget computes width privately. The text-to-`Cell` boundary neutralizes
C0/C1 control characters in application-supplied strings (§12), so
hostile display text can never reach the presenter as control data.

Library width tables are the **internal** truth (goldens pin them);
agreement with the *actual* terminal's width behavior is a terminal
capability handled in `term` (§4, D-019). Rendering is in logical order:
ckVision performs no bidirectional reordering and does not compensate
terminal-side bidi (D-026).

## 3. scene — surfaces and composition

The rendering model is retained cell surfaces with damage tracking:

- **Surface**: an owned rectangular grid of `Cell`s with a per-row damage
  span. Windows and popups own backing-store surfaces; views paint into
  their surface through a clipped **Painter**.
- **Painter**: the one drawing API — styled text runs, fills, box drawing
  (single/double/rounded/heavy line sets with automatic junction merging),
  horizontal/vertical rules, clipping, style-transform regions, and
  raster-region placement. Widgets never emit escape sequences and never
  touch cells directly.
- **Raster regions**: a surface can host cell-anchored raster content —
  an `Image` with a cell-rect anchor and a pixel extent. Raster regions
  are first-class scene citizens: z-ordered, clipped, and occluded by the
  compositor exactly like cells (partial occlusion yields visible
  sub-rectangles for the presenter to encode), damaged and scrolled with
  their surface, and dimmed by shadows falling on them. Every raster
  region carries its **fallback**: the painter callback that renders
  equivalent cell content when the terminal has no graphics support —
  a region without a fallback cannot be constructed.
- **Compositor**: assembles the frame from the desktop background and the
  z-ordered window/popup surfaces. It composes only damaged regions,
  computes visibility so occluded cells are never touched, and applies
  **shadows** as a compositor pass: a shadow is a declared geometry
  (default: two columns right, one row down — the classic look) rendered
  by style-transforming the cells *beneath* it, dimming whatever content
  is there, exactly like the classics did it. Window shadows and menu
  shadows fall out of the same mechanism.
- **Scroll intent**: when a viewport scrolls by whole rows, the shift is
  recorded as an annotation on the compose/present handoff, so the
  presenter can apply scroll optimizations from declared intent instead
  of re-inferring it from cell diffs.
- **Cursor state**: position, visibility, and shape are scene outputs
  composed like everything else, so the terminal cursor always lands
  where the focused widget says. Accurate cursor state is also the IME
  contract: emulator-drawn preedit and candidate windows appear at the
  focused control because the cursor is always truthful (D-OPEN-3).

The composed frame is core-typed frame data (cells plus visible raster
slices plus annotations); the presenter (in `term`) diffs it against the
previously presented frame. This split — paint damage, compose damage,
present diff — is what makes every interaction cost proportional to what
actually changed.

## 4. term — terminal backends

One abstract seam: the `Terminal` interface — capability report, input
event stream, frame presentation, clipboard write, title, notifications
(bell). Implementations:

- **posix**: macOS and Linux. Raw-mode tty management, `poll`-based event
  loop with a self-pipe for cross-thread wakeups, SIGWINCH handling with
  resize coalescing, suspend/resume (Ctrl-Z, SIGTSTP/SIGCONT) with full
  state re-establishment.
- **windows**: Windows Terminal and ConPTY hosts, **VT-first** (D-020):
  VT processing on output, VT input mode feeding the same decoder as
  POSIX (kitty keyboard protocol and bracketed paste work where the host
  supports them); the console API is used only where VT has no channel
  (e.g. buffer-size change notification), and per-host downgrades are
  recorded in the capability report. Non-VT hosts are refused with a
  clear diagnostic.
- **headless**: a full in-memory terminal for tests and automation —
  scripted input, frame capture, capability profiles set per test.
- **record/replay**: wraps any backend; records the event stream —
  including capability-probe responses — and presented bytes for
  deterministic replay and debugging.

**Session-state ledger and restoration.** Every mode the backend sets
(alternate screen, raw mode, mouse modes, keyboard protocol pushes,
bracketed paste, focus reporting, cursor shape) is recorded in a ledger
with its reset sequence. Teardown replays the ledger in reverse — on
normal exit, suspend, and resume re-establishment — and the ledger yields
the precomputed byte string that the async-signal-safe panic restore
(§11) writes. Policy: the alternate screen (DECSET 1049) is entered on
init and left on every exit path; the user's scrollback is never
disturbed; suspend restores the primary screen and shell state until
resume.

**Capabilities.** No terminfo. A curated profile database (keyed on
terminal identity supplied to the backend at construction) sets the
baseline; runtime probes refine it; applications can force overrides.
The lifecycle is explicit: the first frame ships immediately on the
baseline profile — probing never blocks presentation; probe responses
(DA1, XTGETTCAP, mode and color queries) arrive as typed events on the
normal input stream (hence correlated, recorded, and replayed), are
resolved against a bounded deadline on the injected clock, and a
**capability-changed notification** — routed like resize — triggers a
defined re-present when refinement changes rendering (e.g. color depth).
Unanswered probes degrade silently to the baseline.

Capabilities are a plain inspectable struct: color depth, keyboard
protocol level, mouse protocol, **pixel-coordinate mouse reporting**,
**cell pixel metrics** (the character cell's pixel size, from XTWINOPS
queries, re-probed on every resize because font changes move it),
**graphics** (Sixel with geometry/palette limits via DA1 and XTSMGRAPHICS;
kitty graphics), **width policy** (the terminal's Unicode width behavior:
per-profile data plus an optional startup width probe — D-019),
**color-scheme hint** (dark/light/unknown via OSC 10/11 queries, updated
by mode 2031 notifications where supported), hyperlinks (OSC 8),
synchronized output, clipboard write, focus events, paste bracketing.
Every capability has a defined degradation and a headless test for both
the full and degraded paths. Terminal multiplexers (tmux, screen) and
the Linux console are curated profiles like any other.

**Input decoder.** A single incremental state machine turns bytes into
typed events: keys (legacy escape sequences, `modifyOtherKeys`, kitty
keyboard protocol), mouse (SGR encoding; SGR-Pixels when available —
decoded into **both coordinate spaces at once**, cell position always,
pixel position alongside), bracketed paste (delivered as one atomic,
**sanitized** text event — §12), focus in/out, resize, probe responses.
The decoder takes the injected `Clock` as an explicit input: a lone ESC
is resolved against a quiet deadline (the classic ESC-vs-sequence,
Esc-vs-Alt+key ambiguity exists only on legacy profiles; kitty protocol
and `modifyOtherKeys` levels eliminate it, and the per-profile policy is
documented). The decoder is allocation-free in steady state, tolerant of
partial reads and malformed sequences, and fuzzed in CI. Dead keys and
IME composition arrive as the terminal delivers them — composed text —
and are treated as text events, never re-interpreted as chords.

**Presenter.** Cell-diffs the composed frame against the last presented
frame and emits minimal output: cursor-motion-optimized runs and
style-change batching over runs whose widths the *terminal* is known to
agree on, with **absolute cursor addressing re-established after any
grapheme run that is not terminal-width-safe** (emoji sequences, ZWJ,
VS16, ambiguous-width — D-019), so a width disagreement can corrupt at
most one run, never the frame. Scroll optimizations are applied from the
scene's scroll annotations, opportunistically and only where the terminal
model allows (full-width, unoccluded bands; left/right-margin support is
a capability-gated extension, never a baseline assumption). Output is
wrapped in synchronized-output brackets (DEC mode 2026) when available —
one `write()` per frame, no flicker, no tearing. Raster slices are
emitted through the **graphics encoders** (Sixel primary, kitty graphics
secondary): encodings are cached and reused until image content,
geometry, palette mode, or visible slicing changes; partial occlusion
re-encodes only the slices it altered. Emission order is a presentation
detail: raster slices are emitted after the cell diff — on top — within
the same synchronized frame; the compositor's z-order and occlusion
slicing remain the semantic truth, so the terminal layers exactly what
the scene decided is visible, and library users never deal with emission
order. All OSC emissions (title, OSC 52, OSC 8 targets) escape or reject
embedded terminator bytes (§12).

**Platform services.** The `term` layer also hosts the platform-backed
implementations of the injectable service interfaces the upper layers
consume: the filesystem model (used by file dialogs; scripted in-memory
implementation for headless tests) and the system-clipboard bridge
(OSC 52 write on VT terminals; full native clipboard read/write on
Windows). Interfaces live where they are consumed (`ui`); platform code
lives here (D-025).

## 5. ui — the framework

### View tree and ownership

`View` is the one base class: bounds within the parent, size hints, draw
via Painter, event handling, focus policy, invalidation, and an optional
**help-context key** (§ Commands and help). Parents own children
(`std::unique_ptr`, explicit transfer API); destruction order is
deterministic; no view outlives its parent. The tree roots in a
`Desktop` (background + window management) under an `Application`.

There is no global state in the library's design: `Application` owns the
terminal, the scene, the focus, the command registry, the theme, the
internal clipboard, the history registry, the platform services, and the
event loop. Multiple applications in one process are supported — on
distinct terminals; attaching two applications to one terminal is a
documented contract violation. The single sanctioned process-level
facility is the async-signal-safe registry of live terminal sessions that
fatal-signal handlers use to restore every attached tty (D-024).

### Invalidation, frames, and the loop

Views never draw eagerly; they call `invalidate()` (whole view or a
rect). The loop batches: drain all pending input events, dispatch, run
due timers, then — only if something invalidated — layout the dirty
subtrees, paint the dirty regions, compose, present. One frame per batch,
never one frame per event. Mouse-move floods and resize storms coalesce
before dispatch. Steady-state dispatch and paint allocate nothing.

The loop is **embeddable by construction** (D-021): `run()` is a
convenience over a public `step(deadline)` / `wake()` pair with an
exported waitable handle, so a host can drive ckVision from its own
poll/epoll/IOCP loop. Consequently, **modality is loop state, not nested
native loops**: a modal dialog pushes onto the modality stack, which
scopes event routing (accelerators included) until it completes via
callback/notification; a blocking convenience wrapper exists for
applications that own the loop. Posted work and destruction during
modality follow the ordinary single-loop rules — there is no re-entrant
pump to reason about.

### Events

Typed value structs (key, mouse, text, paste, focus, resize, timer,
capability-changed, user), dispatched by three explicit routes:

1. **Positional** (mouse): hit-test down the z-ordered tree — popups
   first, then windows by z-order; implicit capture during drags so
   movement and release always reach the view that took the press. Mouse
   events natively carry **two coordinate spaces**: the cell position
   (always) and the pixel position (whenever the terminal reports
   pixels) — one event type, both spaces first-class, with accessors
   that map either space into view-local coordinates and, for raster
   regions, into image-local pixels. No second event system, no
   synthesized precision: if the terminal only reports cells, the pixel
   field is absent and the capability struct says so.
2. **Focused** (key/text): the focus chain from focused view up through
   its ancestors; first consumer wins. What survives goes to accelerator
   tables (window, then application), then menu mnemonics.
3. **Notification**: parent-directed or subtree broadcast for state
   changes (selection changed, value committed), carrying typed payloads —
   no `void*`, no numeric message pairs. Extension widgets define their
   own notification types through per-application registration (mirroring
   theme-role registration) with type-safe subscription, so the
   no-`void*` promise holds for third-party widgets too.

Threads other than the owning thread interact only via
`Application::post(fn)` — a thread-safe queue drained by the loop.
Everything else is single-threaded by contract (§9).

### Commands and help

A first-class command system: `CommandId` (32-bit; ids below 4096 are
reserved for the library's standard commands, and extension libraries
allocate documented ranges through the registry), a per-application
registry with metadata (title, category, default chord), context-scoped
enablement (contexts are named scopes declared by the application or a
widget, activated by focus ancestry or explicit push/pop), and dispatch
to the focus chain. Menus and status lines reference commands and render
their current enablement and chords automatically. Keymaps bind chords to
commands per named scheme; schemes are switchable and rebindable at
runtime. The registry is exportable (for host-application integration and
for generated documentation).

**Context help is framework machinery** (D-027): every view may carry a
help-context key; the help command (F1 by default) resolves the focused
view's key — walking ancestors for the nearest one — and hands it to the
application's registered help provider, which opens the Help viewer
widget (topics, activatable cross-links, back navigation, index — built
on Text view). The status line's context-sensitive hints key off the
same mechanism. Help *content* is supplied by the application through an
injected provider interface; the library defines no help file format.

### Application services

- **Internal clipboard** (D-022): `Application` owns a clipboard that
  always backs widget cut/copy/paste, so in-app editing works on every
  terminal. Copy writes through to the system clipboard where a write
  capability exists (OSC 52 on VT terminals, native on Windows); system
  *import* happens only via bracketed paste or native read on Windows —
  OSC 52 read is not used (universally disabled for security). Because
  mouse reporting captures drags, in-app selection + clipboard export is
  the primary copy path, and that is by design.
- **History registry**: named, application-scoped history lists shared by
  every widget that references the same key (input lines, combo boxes,
  file dialogs), with documented capacity and deduplication policy, and
  an inspectable/exportable surface so applications can persist them.
- **Desktop snapshot**: a structured, typed snapshot/restore API for
  window geometry, z-order, zoom state, and history lists. The
  application owns storage and re-creates window content; the library
  defines no file format.
- **Platform services**: the injected filesystem and clock interfaces
  (D-025) — file dialogs enumerate through the filesystem interface,
  which is why they golden-test headlessly against a scripted tree.
- **Localization**: every string the library displays routes through a
  replaceable string table (§13).

### Windows, popups, modality, window management

`Window` composes frame, title, close/zoom controls, resize grips, shadow
declaration, and a content view with its own backing surface. The desktop
provides z-order, activation, next/previous cycling, select-by-number, a
window-list dialog, tile and cascade.

**Popups are a first-class concept**: menus, dropdowns, completion lists,
and context menus are lightweight surface-owning nodes parented to the
desktop for composition, z-order, and lifetime, with an explicit
back-reference to their logical owner for theme resolution, command
scope, and dismissal. Popups hit-test above windows and follow a defined
light-dismiss grammar (outside click, Esc, focus loss). This is the
mechanism behind every menu and dropdown in the library.

**Closing is a protocol, not an act**: every close path — close control,
keyboard close, window-list close, application quit — issues a close
*request* that the content can veto or answer asynchronously (the
standard unsaved-changes flow); application quit sweeps all windows
through the same protocol. **Dialog accept** (default button, Enter) runs
validation across the dialog's controls; a failing validator vetoes the
accept, receives focus, and reports through its normal error styling.
Esc-cancel bypasses validation by definition.

Because windows have backing stores, **moving a window costs composition
only** — zero content repaints while dragging. Resizing relayouts and
repaints the resized window's dirty subtree — nothing else on the desktop
— under its own budget (§8).

**Sizing policy**: the framework declares a minimum supported terminal
size (80×24 full chrome; below it, degraded chrome down to a hard floor,
under which a "terminal too small" state is shown rather than corrupted
output). On shrink, windows are deterministically clamped and
repositioned to remain reachable; dialogs larger than the screen clamp
and scroll their content. Shrink storms are golden-tested.

### Focus and traversal

One focused view per application, always visible, always consistent:
focus-in/focus-out notifications, Tab/Shift-Tab traversal in declaration
order with explicit override, arrow keys within radio/check groups,
mnemonic jump, initial focus = first focusable by construction (a dialog
cannot be built with wrong initial focus), and focus restoration on
window close and modal exit. Disabled and hidden views are transparent
to traversal.

### Layout and dialog construction

Explicit container layouts, computed in one top-down pass over dirty
subtrees: `Row`, `Column` (weights, fixed/minimum/expanding policies,
alignment, spacing), `Grid` (spans), `Dock` (edges + center), `Overlay`,
plus fixed geometry where cell precision is wanted (windows keep manual
placement). Size hints flow up, final rects flow down. The one sanctioned
exception to the single pass is **height-for-width**: wrapped widgets
(static text, memo, message boxes) declare a height-for-width query that
containers invoke once during the down pass — bounded, no measurement
loops.

Dialog construction is **declarative descriptors, validated
materialization**: applications describe dialogs as plain descriptor
structs (designed for C++20 designated-initializer syntax), and a
materializer turns a descriptor into the parent-owned view tree —
binding label mnemonics to their controls, wiring the default button and
initial focus, and rejecting invalid descriptions at materialization.
Declarative syntax and enforced invariants are both kept, because
aggregates describe and the materializer constructs.

### Menus

Menu *structure* is authored (order, separators, nesting are meaning the
registry cannot invent); menu *items* reference commands and render
title, chord, and enablement live from the registry. A one-call
convenience registers command + handler + chord + menu item together, so
small applications stay terse while large ones keep central command
tables. Ad-hoc closures enter through that same call — every menu item
is a command underneath, always.

### Themes and schemes

A theme is a **flat, inspectable table**: semantic role → `Style`. Roles
are interned per-application identifiers created from namespaced string
forms at registration time (`ckv.dialog.frame.active`; third parties use
their own namespace) — resolution is an indexed lookup, not a string
search, and there is no cascade, no inheritance chain, no selectors.
Every custom-role registration **declares a fallback** (an existing role
or an explicit Style), so built-in schemes remain complete by
construction, and a printed theme — including extension roles with their
resolved fallbacks — is the whole truth. Resolution scope is application
default with per-window override, materialized at scope-change time so a
lookup is always single-step. Built-in schemes: **Classic** (the faithful
'90s blue desktop), **Dark**, **Light**, **Mono** (16-color-safe). The
capability layer's dark/light hint (§4) selects the startup scheme;
themes switch at runtime with a single full invalidation.

## 6. widgets

The full catalog with capability baseline and beyond-baseline features is
in [plans/widgets.md](plans/widgets.md). Architectural rules: every widget
is keyboard-complete and mouse-complete; every widget draws only through
Painter and styles only through theme roles; every widget is constructible
headless and covered by golden tests; every behavior contract (what keys
do what, when notifications fire) is documented and is part of the
compatibility promise.

## 7. Graphics — first-class, throughout

Inline raster graphics are a core capability of the library, not an
add-on. The design threads through every layer:

- `core` provides `Image` (RGBA) and the pixel-space types.
- `scene` hosts raster regions with full compositor citizenship —
  z-order, occlusion slicing, scrolling, shadow dimming (§3).
- `term` detects graphics capabilities and encodes visible slices (Sixel
  primary, kitty graphics secondary) with caching (§4), and decodes
  pixel-coordinate mouse input (§4).
- `ui` delivers dual-space mouse events with image-local mapping (§5).
- `widgets` supplies `ImageView` (show an Image) and `Canvas` (the
  application draws RGBA on demand at the widget's current pixel size) —
  both constructed with their mandatory cell-content fallback.

The **fallback contract** is architectural: no API in the library accepts
raster content without its textual fallback, so an application that runs
on a graphics terminal runs — fully functional, merely less pretty — on a
text-only terminal. Capability decides at render time; application code
has one path.

Pixel-space interaction is honest: pixel positions come only from the
terminal's own pixel reporting; cell metrics (for layout and image sizing
decisions) come from the terminal's reported cell pixel size and are
re-probed on resize. The library never fabricates pixel precision it does
not have, and applications can always query which precision is in effect.

## 8. Performance charter

Performance is a specified feature with budgets, not an aspiration.
Reference configuration: a 200×60 terminal.

Machine-independent budgets are **hard CI gates** (deterministic, no
timing noise): zero steady-state heap allocations in dispatch, paint,
compose, and present paths; cells-touched bounds per interaction class;
bytes-emitted bounds per frame class; frame counts (one frame per event
batch). Wall-clock budgets are verified on a **named pinned reference
machine** with ratcheted relative-regression thresholds in CI:

- Input-event-to-presented-bytes latency: **p99 < 2 ms** (keystroke echo,
  focus moves, menu navigation), measured by a scripted PTY harness with
  timestamped event injection and byte capture.
- Window move-drag: sustained at input event rate, composition-only,
  **zero content repaints**. Window resize-drag: relayout and repaint
  strictly confined to the resized window's dirty subtree, within the
  same sustained rate.
- Full-screen recompose + diff after theme switch: **< 5 ms**.
- Time-to-first-frame: the first present ships on the baseline capability
  profile immediately — probing never delays it.
- Raster regions: encodings cached until content, geometry, palette mode,
  or visible slicing changes; occlusion changes re-encode only affected
  slices; dual-space mouse delivery allocates nothing.
- Frame presentation: one `write()`, synchronized bracket, minimal diff.

Regressions on either class of gate fail the build like test failures.
Perf traces (per-stage timings, damage areas, bytes emitted) ride the
diagnostics facility (§11) from milestone M1.

## 9. Concurrency contract

The library is single-threaded by contract: every view, window, and
application object is confined to its application's **owning thread** (the
thread that runs the loop). The two blessed crossings: `Application::post`
(thread-safe, lock-based MPSC queue, wakes the loop) and cancellation
tokens passed into application-owned worker code. The library itself
creates no visible threads; backends may use internal threads for I/O but
surface everything through the event stream on the owning thread.

Signals are process-level by nature; the design confines them to the one
sanctioned facility: an async-signal-safe registry of live terminal
sessions (D-024). Runtime signals (SIGWINCH, SIGTSTP/SIGCONT) are routed
to the session that owns the tty; fatal-signal handlers restore every
registered tty from its precomputed ledger bytes (§4) before the process
dies.

## 10. Determinism and testing architecture

- **Golden frames**: styled-cell dumps (text format: glyphs + a legible
  style annotation) captured from the headless backend, compared
  byte-exact. Raster regions appear in dumps symbolically — anchor,
  extent, content hash, active fallback — so graphics scenes, occlusion
  slicing, and fallback behavior golden-test headlessly. The dump format
  is stable and diff-friendly; goldens are the specification of widget
  appearance, including shadows, junctions, and degraded color modes.
  Goldens pin the library's own width tables; fidelity on real terminals
  is guaranteed by the width-agreement strategy (§4, D-019), which has
  its own test matrix.
- **Event scripts**: recorded or hand-written typed-event sequences drive
  applications headlessly; script + capability profile + goldens =
  executable interaction specification (menu navigation, focus order,
  modal scoping, close-veto flows, pixel-mouse delivery, hostile-paste
  handling are all pinned this way).
- **Unit tests** per layer (core width tables, decoder sequences, painter
  clipping, compositor occlusion, layout mathematics, focus rules).
- **Fuzzing**: the input decoder and every parser of external bytes, in
  CI — including the semantic injection cases of §12, not just crashes.
- **Sanitizers**: ASan/UBSan/TSan jobs green as a merge condition.
- **PTY integration tests**: real end-to-end smoke on each platform's CI
  (macOS, Linux, Windows) against actual terminal plumbing, including
  restoration-on-crash and suspend/resume.
- **Benchmarks**: §8, enforced.
- **Inspection tooling**: a debug overlay and dump API — view tree with
  bounds and focus policy, computed traversal order, active theme-role
  resolution ("why is this cell this color"), damage visualization — so
  application developers can interrogate a running or replayed UI
  instead of reading library source.

The library reads no wall-clock time, no locale, and — in core, scene,
ui, and widgets — no environment and no filesystem; the injected Clock
and the injected filesystem interface are the two impurity boundaries,
and tests script both.

## 11. Errors and diagnostics

- Programming-contract violations (invalid child transfer, painting
  outside a frame, off-thread access) are assertions — fail fast, loudly,
  in all build types until 1.0.
- Environmental failures (terminal lost, unsupported host, write errors)
  surface as typed `ckv::Error` values on a small, documented set of
  fallible entry points; the event/draw hot paths are `noexcept`.
- **Application callbacks invoked on hot paths are noexcept-in-effect**:
  a throw from a draw or event callback is a contract violation — the
  library performs the guaranteed terminal restore, emits the diagnostic,
  and terminates. There is no silent swallowing and no undefined
  propagation path.
- **Diagnostics facility**: an injectable log sink owned by
  `Application` (no globals). While the UI owns the terminal, stderr is
  captured and buffered; on every exit path the terminal is restored
  *first* and buffered diagnostics — including the failing assertion's
  message on panic paths — are then written to stderr, so a crash is
  always diagnosable and never wrecks the shell. Perf traces (§8) and
  capability-probe logs ride the same sink.
- Terminal state restoration is guaranteed on every exit path — normal
  return, exception, assertion, and fatal signal (async-signal-safe
  ledger replay, §4/§9) — a corrupted user shell is treated as a
  release-blocking bug.

## 12. Security and hostile content

The trust model: bytes from the terminal and strings from application
data (file names, log lines, document content) are both untrusted for
*control* purposes.

- **Paste sanitization**: bracketed-paste payloads are filtered in the
  decoder before the atomic text event is formed — C0 controls stripped
  except tab and newline, ESC and C1 stripped, embedded paste
  terminators neutralized — so hostile clipboard content cannot inject
  synthetic keys or sequences.
- **Display-text neutralization**: the text-to-`Cell` boundary in core
  replaces C0/C1 controls in displayed strings, so a malicious file name
  rendered in a Label or list can never alter terminal state; the
  "widgets never emit escape sequences" rule is enforced by type, not
  convention.
- **OSC emission safety**: every OSC the presenter emits (title,
  clipboard write, hyperlink targets) escapes or rejects embedded
  terminator bytes.
- These are explicit fuzz and golden targets (§10) and exit criteria in
  ROADMAP M3 — not best-effort intentions.

## 13. Public API and repository shape

- Headers under `include/cvision/{core,term,scene,ui,widgets}/`;
  implementation under `src/` mirroring layers; `tests/`, `benchmarks/`,
  `examples/` (hello, dialogs, image showcase, full gallery, a small real
  application), `docs/`, `plans/`, `cmake/`.
- Build: CMake ≥ 3.25, C++20, no compiler extensions; consumable via
  `add_subdirectory`/FetchContent and installable package config. Warnings
  are errors; the strict flag set lives in one central CMake module.
- All strings the library itself displays (message-box buttons, window
  menu, file dialog) route through a replaceable string table —
  applications localize by supplying their own; the library hardcodes no
  user-visible English anywhere else.
- Versioning: 0.x may break freely with changelog notes; 1.0 begins
  semver. No deprecation shims pre-1.0 — callers move with the library.

## 14. Extension points

The supported ways in, each documented with a worked example: custom
widgets (subclass `View`; Painter, size hints, focus policy, theme-role
and notification-type registration), custom windows/chrome, custom
layouts (one interface), custom themes and schemes, custom terminal
backends (implement `Terminal`), event filters at application and window
scope, and command providers with allocated id ranges (host applications
map their own command/config systems onto the registry — this is how any
host application integrates ckVision without ckVision knowing it exists).
