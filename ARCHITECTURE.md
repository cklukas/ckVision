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
│            images, canvases, scrollers, trees, tables,        │
│            editor document/models and terminal views, ...     │
├───────────────────────────────────────────────────────────────┤
│  ui        view tree, windows, popups, desktop, focus,        │
│            events, commands, keymaps, layout, themes, help    │
│            contexts, clipboard, histories, timers, loop       │
├───────────────────────────────────────────────────────────────┤
│  scene     surfaces, painter, raster regions, compositor,     │
│            damage, shadows, cursor state, frame assembly      │
├───────────────────────────────────────────────────────────────┤
│  term      terminal backends, capabilities, input decoder,    │
│            frame presenter, graphics encoders, private child  │
│            sessions/emulator, platform adapters               │
│            (filesystem, clock, clipboard export)              │
├───────────────────────────────────────────────────────────────┤
│  core      geometry, text/width, color, style, cell, image,   │
│            events, keys, error, clock — no I/O, no platform   │
└───────────────────────────────────────────────────────────────┘
```

Allowed dependencies: `widgets → ui → scene → core`; `ui → term` only
through the `Terminal` and platform-service interfaces; `scene → core`;
`term → core`. The `Terminal` and embedded-session interfaces exchange **core types only**
(frame data views, events, capabilities), keeping `term` independent of
`scene`. Nothing depends on `widgets` except applications. No layer
reaches around a lower layer. No cycles, enforced by include checks in CI.

Environment and platform access follow the same discipline: `core`,
`scene`, `ui`, and `widgets` never read the environment, the filesystem,
or the clock directly. Core owns the dependency-neutral contracts; term owns
platform adapters and terminal-session I/O; applications and widgets receive
the particular service instances they consume explicitly (§5, D-039).

Namespace: `ckv::` (sub-namespaces per layer only where they pay for
themselves). Public headers: `<cvision/...>`. CMake target:
`ckvision::cvision`.

## 2. core — primitives

Value types used by everything above: `Point`, `Size`, `Rect` in cell
space and `PixelPoint`, `PixelSize` in pixel space (distinct types — the
two coordinate spaces never mix silently); `Color` (24-bit RGB +
default/palette handles), `Style` (fg, bg, attributes), `Cell` (grapheme +
style + width + a hyperlink id resolved through the owning grid's
`LinkTable`, D-088); `Image` (owned RGBA pixel buffer with size and stride —
the raster primitive everything above shares); key model (`Key`,
`KeyChord`: key + modifiers, printable text carried separately); event
payload structs; a monotonic `Clock` interface (injectable — the
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
- **Painter**: the one drawing API — styled text runs (optionally linked to
  a hyperlink target, D-088), fills, box drawing
  (single/double/rounded/heavy line sets with automatic junction merging),
  horizontal/vertical rules, clipping, style-transform regions, and
  raster-region placement. Widgets never emit escape sequences and never
  touch cells directly. Junction merging is provenance-scoped (D-036): each
  view paint has an isolated, surface-local scope, while translated/clipped
  painters within that view inherit it. Exact connector metadata, never a
  reverse interpretation of the visible glyph, controls merging. Thus
  same-view dividers can join their frame, while later z-order content always
  replaces unrelated lower-view lines normally.
- **Raster regions**: a surface can host cell-anchored raster content —
  an `Image` with a cell-rect anchor and a pixel extent. Raster regions
  are first-class scene citizens: z-ordered, clipped, and occluded by the
  compositor exactly like cells (partial occlusion yields visible
  sub-rectangles for the presenter to encode), damaged with their surface,
  and dimmed by shadows falling on them. A scrolling widget re-anchors and
  re-clips its raster regions with the content it scrolls, so a scrolled
  picture is a moved region (D-081). Every raster
  region carries its **fallback**: the painter callback that renders
  equivalent cell content when the terminal has no graphics support —
  a region without a fallback cannot be constructed.
- **Compositor**: assembles the frame from the desktop background and the
  z-ordered window/popup surfaces. It composes only damaged regions,
  computes visibility so occluded cells are never touched, and applies
  **shadows** as a compositor pass: a shadow is a declared geometry
  (default: two columns right, one row down — the classic look) rendered
  by style-transforming the cells *beneath* it, darkening whatever content
  is there in the way the theme states (D-106). Shadow coverage is a
  binary union (D-037): one or many overlapping shadow casters apply exactly
  one transform, never progressively darker transforms. A later-painted higher layer clears lower shadow coverage by
  replacing the cell normally. Window shadows and menu shadows fall out of the
  same mechanism.
- **Scroll intent** (post-0.1, D-041/D-081): when a viewport scrolls by whole
  rows, the shift can be recorded as an annotation on the compose/present
  handoff, so the presenter can apply scroll optimizations from declared
  intent instead of re-inferring it from cell diffs. 0.1 repaints the
  scrolled content.
- **Cursor state**: position, visibility, and shape are scene outputs
  composed like everything else, so the terminal cursor always lands
  where the focused widget says. Accurate cursor state is also the IME
  contract: emulator-drawn preedit and candidate windows appear at the
  focused control because the cursor is always truthful (D-OPEN-3). A
  blinking cursor also declares its half-period. The presenter emits a
  **steady** host cursor shape and owns the visibility phases on the injected
  monotonic clock; the application includes the next cursor phase in its wait
  deadline and a phase change writes only cursor bytes, without repainting the
  scene. Host cursor preferences therefore cannot disable, double, or retime
  application-declared blinking, while an embedded hardware model can publish
  its device-specific cadence.

The composed frame is core-typed frame data (cells and their link table,
plus visible raster slices, plus annotations; links are carried into the
frame by target, and a shadow dims a linked cell without unlinking it); the presenter (in `term`) diffs it against the
previously presented frame. This split — paint damage, compose damage,
present diff — is what makes every interaction cost proportional to what
actually changed.

## 4. term — terminal backends

One abstract seam: the `Terminal` interface — capability report, input
event stream, frame presentation, clipboard write, title, notifications
(bell). Implementations:

- **posix**: macOS and Linux. Raw-mode tty management, `poll`-based event
  loop with a self-pipe for cross-thread wakeups, SIGWINCH handling with
  resize coalescing, and SIGTSTP/SIGCONT session restoration with full frame
  re-presentation. SIGINT policy belongs to the embedding host.
- **windows**: Windows Terminal and ConPTY hosts, **VT-first** (D-020):
  VT processing on output, VT input mode feeding the same decoder as
  POSIX (kitty keyboard protocol and bracketed paste work where the host
  supports them). The UTF-16 VT character stream is converted to UTF-8 at
  the platform boundary; the console API is used only where VT has no channel
  (e.g. buffer-size change notification), and per-host downgrades are
  recorded in the capability report. Non-VT hosts are refused with a
  clear diagnostic.
- **headless**: a full in-memory terminal for tests and automation —
  scripted input, named capability profiles set per instance, raw output-byte
  capture, symbolic composed-frame capture, and a deterministic virtual display
  that consumes those output bytes into cell (with hyperlink targets) and
  RGBA planes. The virtual
  display implements only the documented VT subset Presenter emits plus the
  published Sixel grammar; unsupported output is a test failure, not ignored.
- **record/replay**: wraps any backend; records an initial capability/size
  snapshot, every event batch (including capability-probe responses and
  resize), and every observable terminal output operation (frame bytes, title,
  bell, and supported clipboard write). Replay applies capability and size
  changes before returning their recorded batch, then captures the same
  operation log so a deterministic rerun can prove byte-for-byte and
  side-effect-for-side-effect fidelity without consulting the original backend.

**Embedded terminal subsessions.** `term` also owns private child PTY/ConPTY
sessions and their off-screen terminal emulator (D-042). A child gets a
distinct terminal endpoint; its bytes are parsed into core-typed cells, cursor
state, and raster images, never forwarded to the outer `Terminal` writer.
`ui::Application` multiplexes explicitly attached child readiness with its own
terminal handles, and `widgets::TerminalView` consumes the core session seam
to paint an emulated snapshot through its clipped painter. The scene compositor therefore remains the sole
authority for child-cell and child-raster clipping, occlusion, shadowing,
clearing, and outer Sixel presentation. Details and acceptance live in
`plans/embedded-terminal-architecture.md`.

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

**Capabilities.** No terminfo. A curated profile database selected explicitly
by the host at backend construction sets the baseline; runtime probes refine
it; applications can force overrides. The library never inspects environment
state to infer terminal identity.
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
by mode 2031 notifications where supported), hyperlinks (OSC 8; not
probeable, so set by host profile, explicit value or override — D-088),
synchronized output, clipboard write, focus events, paste bracketing.
Every capability has a defined degradation and a headless test for both
the full and degraded paths. Graphics tests run the identical application
script under named `Sixel` and `NoGraphics` profiles with fixed cell-pixel
metrics, including runtime capability-change re-presentation. Terminal
multiplexers (tmux, screen) and the Linux console are curated profiles like
any other.

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
partial reads and malformed sequences, and fuzzed in CI. Decoder-owned quiet
deadlines are included in backend waits and serviced after every wait result,
even when unrelated readiness sources remain continuously signaled; readable
IPC must not starve Escape or bracketed-paste completion. Dead keys and
IME composition arrive as the terminal delivers them — composed text, as
unmodified character keys that editable controls insert as text — and are
never re-interpreted as chords. Only bracketed paste is decoded as a text
event directly.

**Presenter.** Cell-diffs the composed frame against the last presented
frame and emits minimal output: cursor-motion-optimized runs and
style-change batching over runs whose widths the *terminal* is known to
agree on, with **absolute cursor addressing re-established after any
grapheme run that is not terminal-width-safe** (emoji sequences, ZWJ,
VS16, ambiguous-width — D-019), so a width disagreement can corrupt at
most one run, never the frame. Scroll optimizations, once scroll intents
exist (post-0.1, D-081), are applied from the scene's scroll annotations,
opportunistically and only where the terminal model allows (full-width, unoccluded bands; left/right-margin support is
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
order. Cells that are part of a hyperlink are bracketed in OSC 8 only while
`Capabilities::hyperlinks` is set: one open per contiguous run of one target,
with an id derived from the target, closed at every run end and so before
any cursor addressing; without the capability links cost no bytes (D-088).
All OSC emissions (title, OSC 52, OSC 8) escape or reject embedded
terminator bytes; hyperlink targets are validated, not escaped (§12).

When frame-completion tracking is enabled, expensive frames are also
coalescing boundaries: every raster frame and every text frame of at least
4 KiB must be acknowledged before another frame is emitted. Input and scene
state continue to advance, but only the latest dirty state is retained, so a
pointer-rate drag never becomes a queue of obsolete window positions. Smaller
text frames remain latency-first and are emitted immediately. A host that has
never answered is never allowed to stall presentation; the existing bounded
write-off and capability give-up rules apply to both frame classes.

**Platform adapters.** `term` hosts platform-backed implementations of the
core service contracts: a filesystem model used by file dialogs, a monotonic
clock, and a terminal-session clipboard-export adapter. The corresponding
interfaces do not live in `ui`: `term` could not implement an upward `ui`
dependency. `Terminal` itself owns input polling, native wait handles, and
wake because those are session resources, not generic services. A scripted
in-memory implementation exists for each core contract used in deterministic
tests (D-039).

Windows native readiness is an instance-owned terminal-layer adapter (D-126):
`WindowsWaitSet` combines borrowed console, process and manual-reset event
sources through wait-completion packets and one completion port, without a
64-source limit or worker threads. `WindowsTerminal` uses that same adapter
for its input/wake channels and embedding-host sources. Registration lifetime,
generation-safe cancellation and rearming remain inside the platform adapter;
no operating-system wait machinery enters UI or application rendering code.

## 5. ui — the framework

### View tree and ownership

`View` is the one base class: bounds within the parent, size hints, draw
via Painter, event handling, focus policy, invalidation, and an optional
**help-context key** (§ Commands and help). Parents own children
(`std::unique_ptr`, explicit transfer API); destruction order is
deterministic; no view outlives its parent. The tree roots in a
`Desktop` (background + window management) under an `Application`.

There is no global state in the library's design: `Application` owns the
scene, focus, command registry, theme, internal clipboard, history registry,
event loop, and its default terminal clipboard adapter. It explicitly borrows
the terminal, clock, and any host-provided service; every such instance must
outlive its consumer and distinct Applications use distinct terminal sessions
and service instances. Multiple applications in one process are supported — on
distinct terminals; attaching two applications to one terminal is a documented
contract violation. The single sanctioned process-level
facility is the async-signal-safe registry of live terminal sessions that
fatal-signal handlers use to restore every attached tty (D-024), paired with
the bounded Windows `SetConsoleCtrlHandler` restoration ledger (D-159).

### Invalidation, frames, and the loop

Views never draw eagerly; they call `invalidate()` (whole view or a
rect). The loop batches: drain all pending input events, dispatch, run
due timers, then — only if something invalidated — layout the dirty
subtrees, paint the dirty regions, compose, present. One frame per batch,
never one frame per event. Mouse-move floods and resize storms coalesce
before dispatch. Steady-state dispatch and paint allocate nothing.

The loop is **embeddable by construction** (D-021): `run()` is a
convenience over a public `step(deadline)` / `wake()` pair with an
exported borrowed wait-handle set and next timer deadline, so a host can drive
ckVision from its own poll/epoll/IOCP loop without a duplicate wait. The POSIX
set contains both terminal input and the application wake channel; deterministic
backends have none. A blocking `run_until(done)` checks `done` only while no
quit request has been observed: a request already present, or received during
the preceding `step`, ends the loop before another application predicate call,
and reports interruption rather than completion. A host request that interrupts
a blocking standard-dialog convenience force-detaches its still-open dialog and
returns its documented cancellation fallback; it is not routed through a
vetoable user-close request. Consequently, **modality is loop state, not nested
native loops**: a modal dialog pushes onto the modality stack, which
scopes event routing (accelerators included) until it completes via
callback/notification; a blocking convenience wrapper exists for
applications that own the loop. Posted work and destruction during
modality follow the ordinary single-loop rules — there is no re-entrant
pump to reason about.

### Events

Typed value structs (key, mouse, text and paste, focus, resize,
capability-changed), dispatched by three explicit routes. Timers and posted
work run as callbacks on the loop, not as events:

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

   Pointer delivery has four ordered phases: resolve the target, notify its
   capture ancestors, deliver to that target, then apply click-to-focus. Every
   phase boundary is a user-code boundary. The Application keeps a
   per-view lifetime capability and revalidates both target attachment and
   current ancestry after each callback; a target that is detached or destroyed
   ends the old route, while a reparented target is considered only through its
   new attached path. Capture from a popup remains non-focusing, preserving
   light-dismiss semantics.
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

A first-class command system: a command's identity is a namespaced string
key it declares itself under, and the registry assigns the `CommandId`
that stands for it (a scoped enumeration over 32 bits, so id arithmetic
does not compile and no party — library, widget, extension library or
application — can pick a number that collides with another's). A
per-application registry holds the metadata (title, category, default
chord, palette visibility), context-scoped enablement (contexts are
named scopes declared by the application or a widget, activated by focus
ancestry or explicit push/pop), and dispatch to the focus chain. Menus and status lines reference commands and render
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
- **Platform services**: `Clock`, `FileSystem`, `ClipboardWriter` and
  `ProcessRunner` are
  injected core contracts (D-039). File dialogs enumerate through the
  filesystem interface, which is why they golden-test headlessly against a
  scripted tree; Application's clipboard export uses its explicitly selected
  bridge. Captured non-terminal child helpers use the separate D-132 runner
  contract, not terminal sessions; terminal and helper launches share the
  same `ProcessLaunchSpec`, with native argument/environment preparation in
  term. Root completion, bounded private captures and explicit successful
  descendant policy are part of that contract.
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
through the same protocol. A callback cannot recursively begin a second
default quit sweep while that request is active: the original sweep alone
observes later vetoes and decides whether to request application shutdown. If
a close callback destroys its owning Desktop, that active sweep ends without a
shutdown decision rather than dereferencing the departed owner.
Nor can a close callback recursively invoke the same window's close protocol:
the nested request is already accepted and is a no-op until the outer callback
returns. **Dialog accept** (default button, Enter) runs
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

**Root layout**: `Application` owns an internal fill policy for its direct
root children. A child that keeps `View::fills_root()` at its default is sized
to the current terminal grid immediately on attachment and after every terminal
resize; a fixed root overlay opts out before attachment. `on_bounds_changed`
is a public observer only: replacing it, or changing child geometry from it,
cannot replace the framework layout pass. Headless resize scripts enter through
`HeadlessTerminal` and `Application::step`, and pin the composed frame at each
resize step.

### Focus and traversal

One focused view per application, always visible, always consistent: a
view learns whether it is focused by asking (`View::has_focus()`, which is
the Application's answer) and never by remembering, so nothing can be drawn
focused beside the view that is; focus-in/focus-out notifications,
delivered after the Application has moved on, so that inside them the
answer is already the settled one; Tab/Shift-Tab traversal in declaration
order with explicit override; the arrow keys a focused control does not
use walk the controls of the window it sits in (Down and Right forward, Up
and Left back, wrapping within the window), so a button row is walked with
an arrow the way a dialog reader expects; arrow keys within radio/check
groups; mnemonic jump; initial focus = first focusable by construction (a
dialog cannot be built with wrong initial focus); and focus restoration on
window close and modal exit. Disabled and hidden views are transparent to
traversal (D-065).

### Layout and dialog construction

Explicit container layouts, computed in one top-down pass over dirty
subtrees: `Row`, `Column` (weights, fixed/minimum/expanding policies,
alignment, spacing), `Grid` (spans), `Dock` (edges + center), `Overlay`,
plus fixed geometry where cell precision is wanted (windows keep manual
placement). Size hints flow up, final rects flow down. The one sanctioned
exception to the single pass is **height-for-width**: wrapped widgets
(static text, memo, message boxes) declare a height-for-width query that
containers invoke once during the down pass — bounded, no measurement
loops. A container that reports intrinsic sizes (`Row`, `Column`) answers
the query for its own children, so the pass does not stop at it.

Sizing a window that arrives without bounds is placement, not layout, and
is the one place that asks the query more than once: a window opens at
the width its content asks for, and when that leaves it taller than the
desktop it is widened to the narrowest width its content does fit — found
by bisection, a bounded number of measurements, once, before the window
is first shown.

A widget whose own preferred size changes after that pass already ran
(a label growing, text wrapping to a new line count) calls
`View::size_hint_changed()`, which notifies its immediate parent via
`View::on_child_size_hint_changed()` — one hop, synchronous, never an
automatic multi-ancestor walk (D-030). Row/Column simply re-run their
own pass; Window and Desktop react only for the specific children
whose geometry actually depends on the change (a frame overlay, a
docked view).

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
forms at registration time (`ckv.window.frame.active`; third parties use
their own namespace) — resolution is an indexed lookup, not a string
search, and there is no cascade, no inheritance chain, no selectors.
Every custom-role registration **declares a fallback** (an existing role
or an explicit Style), so built-in schemes remain complete by
construction, and a printed theme — including extension roles with their
resolved fallbacks — is the whole truth. Resolution scope is application
default with per-window override, materialized at scope-change time so a
lookup is always single-step. Built-in schemes: **Classic** (the classic
text-mode desktop: blue windows and grey dialogs over a blue-on-grey
patterned desktop, with black shadows — specified in
`plans/classic-look.md`), **Dark**, **Light**, **Mono** (16-color-safe). The
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
  z-order, occlusion slicing, re-anchoring on scroll, shadow dimming (§3).
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
has one path. The textual fallback is not an underlay: under an active graphics
profile, Presenter writes style-preserving blank cells across each visible
raster slice, then emits an explicitly opaque, extent-bounded Sixel image.
Only the no-graphics profile emits the fallback glyphs. This prevents fallback
text from bleeding through during multi-pass Sixel painting or terminal
repaints and gives capability transitions a clean backing surface.

Pixel-space interaction is honest: pixel positions come only from the
terminal's own pixel reporting; cell metrics (for layout and image sizing
decisions) come from the terminal's reported cell pixel size and are
re-probed on resize. The library never fabricates pixel precision it does
not have, and applications can always query which precision is in effect.

Every pixel quantity in the API is typed in pixel space: the cell metric,
the text-area and window sizes, the Sixel geometry limit, an image's extent
and a canvas's backing size are all `PixelSize`, never the cell-space
`Size`, and neither type converts to the other implicitly. A conversion
needs the cell metric, which is a terminal capability, so the conversions
live in `term` beside it (`term::cells_to_pixels`,
`term::cell_pixels_from_area`) rather than in `core`; a layer that cannot
reach `term` converts in the one place that holds the metric it was given,
as `Canvas` does for its backing image.

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
- Window move-drag: input and scene state sustained at input event rate,
  composition-only, **zero content repaints**; expensive terminal frames
  coalesce behind host acknowledgement to the latest position. Window
  resize-drag: relayout and repaint
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

Signals are process-level by nature; the design confines ckVision's own
use of them to the one sanctioned facility: an async-signal-safe registry
of live terminal sessions (D-024). The registry installs its process signal
dispositions only for the lifetime of one or more live sessions, restoring the
host dispositions after the final session ends. SIGWINCH only interrupts a POSIX poll;
each session compares its own tty geometry, so a resize cannot be routed to
another application. SIGTSTP restores every live terminal before the process
stops; SIGCONT re-enters each session and queues an instance-local full
re-presentation. SIGINT belongs to the embedding host and never mutates an
`Application` from a signal handler. Fatal-signal handlers restore every
registered tty from its precomputed ledger bytes (§4) before the process dies
(D-024); on Windows, `WindowsTerminal` maintains a parallel bounded
`SetConsoleCtrlHandler` restoration ledger (D-159).
The complete host protocol is documented in `docs/terminal-host-integration.md`.

## 10. Determinism and testing architecture

- **Golden frames**: styled-cell dumps (text format: glyphs + a legible
  style annotation + hyperlink runs) captured from the headless backend, compared
  byte-exact. Raster regions appear in dumps symbolically — anchor,
  extent, content hash, active fallback — so graphics scenes, occlusion
  slicing, and fallback behavior golden-test headlessly. The dump format
  is stable and diff-friendly; goldens are the specification of widget
  appearance, including shadows, junctions, and degraded color modes.
  Goldens pin the library's own width tables; fidelity on real terminals
  is guaranteed by the width-agreement strategy (§4, D-019), which has
  its own test matrix. Symbolic raster records specify scene semantics;
  they do not prove terminal-protocol rendering (D-035).
- **Junction/z-order goldens**: line-topology fixtures cover both halves of
  D-036: rules produced within one view join its own frame, while crossing
  borders from overlapping sibling windows remain the exact glyph and style
  of the foreground window. Direct cell assertions accompany the full-frame
  golden so a topology failure is localized rather than hidden in a large
  visual diff.
- **Virtual-display graphics goldens**: the headless terminal feeds the exact
  bytes received by `Terminal::write` into an independently tested VT/Sixel
  decoder and captures the resulting cell + RGBA planes. These pixel-plane
  goldens specify Sixel encoding, placement, cropping, occlusion, clearing, and
  capability-change behavior. The same script also runs under `NoGraphics`,
  where the capture must contain the cell fallback and no raster pixels.
  Documentation screenshots are generated from this decoded virtual display,
  never from a cell-only `FrameView` or directly from the source `Image`.
  Recognized box-drawing cells are emitted as cell-edge-aligned SVG geometry,
  not font-dependent text, so HTML and PDF cannot introduce false gaps in
  otherwise correct frame corners.
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
ui, and widgets — no environment and no filesystem. `Clock`, `FileSystem`,
`ClipboardWriter` and `ProcessRunner` are the injected impurity boundaries, and tests script
each without platform access (D-039).

## 11. Errors and diagnostics

- Programming-contract violations (invalid child transfer, painting
  outside a frame, off-thread access) are assertions — fail fast, loudly,
  in all build types.
- Environmental failures follow D-078: a backend that cannot establish or
  continue its session throws (`std::runtime_error` with an actionable
  message, or `std::system_error` with the OS code), restoration is
  best-effort and never throws, externally supplied configuration that
  cannot be honoured throws `std::invalid_argument`, and an operation for
  which failure is ordinary returns its own status value. The event/draw
  hot paths are `noexcept`. A terminal the session can no longer use, for
  output it cannot take or input that has hung up, throws
  `term::TerminalLost`, which leaves `step()` and `run_until()` for the
  caller to handle (D-138).
- **Application callbacks invoked on hot paths are noexcept-in-effect**:
  a throw from a draw or event callback is a contract violation — the
  library performs the guaranteed terminal restore, emits the diagnostic,
  and terminates. There is no silent swallowing and no undefined
  propagation path. `term::TerminalLost` is the one exception that passes
  through: it reports the session ending, not a defect in the callback that
  happened to be writing when it ended (D-138).
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

- **Paste sanitization and recovery**: bracketed-paste payloads are filtered
  in the decoder before the atomic text event is formed — C0 controls stripped
  except tab and newline, ESC, DEL and C1 replaced (D-085). Because the
  protocol's end marker is in-band, it is held for D-040's quiet period; bytes
  received before that boundary remain sanitized paste text rather than
  synthetic keys, sequences, or commands. `TextEvent::paste_recovered` exposes ambiguous or
  interrupted recovery to applications. The boundary is deliberately not a
  claim that arbitrarily delayed raw terminal bytes have clipboard provenance.
- **Display-text neutralization**: the text-to-`Cell` boundary in core
  replaces C0/C1 controls in displayed strings, so a malicious file name
  rendered in a Label or list can never alter terminal state; the
  "widgets never emit escape sequences" rule is enforced by type, not
  convention.
- **OSC emission safety**: every OSC the presenter emits (title,
  clipboard write, hyperlink targets) escapes or rejects embedded
  terminator bytes; a hyperlink target is rejected unless it is an absolute
  URI of printable ASCII, checked where it enters a link table and again at
  emission (D-088).
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
- Versioning: the CMake package and shared-library identity bind consumers to
  their major and minor SDK line. The 2.1 API transition requires rebuilding
  consumers with the matching headers and library (D-152, D-160, D-164).

## 14. Extension points

The supported ways in, each documented with a worked example: custom
widgets (subclass `View`; Painter, size hints, focus policy, theme-role
and notification-type registration), custom windows/chrome, custom
layouts (one interface), custom themes and schemes, custom terminal
backends (implement `Terminal`), event filters at application and window
scope, and command providers with allocated id ranges (host applications
map their own command/config systems onto the registry — this is how any
host application integrates ckVision without ckVision knowing it exists),
and an opt-in `TextEditor` newline handler for application-owned semantic
editing rules.

## 15. Live log monitoring

D-140 and [plans/live-log-architecture.md](plans/live-log-architecture.md) define
the live-log service/model/view pipeline. Core owns neutral bounded source and
record contracts; term owns native observation/range I/O/private pipe services;
widgets own deterministic retained records, incremental filters and LogView.
The application owns platform log selection/schema adapters and service shutdown.
Every handoff has an explicit byte/record/work bound. Present coalescing never
implies retained-record loss; source lag and loss remain separate observable data.

D-141 adds opt-in bounded Application frame delivery. The live-log mailbox and
receiver expose explicit source overflow/loss and byte/record admission quotas.
The controller fairly yields ingress/matching across fixed borrowed lanes and
coalesces panel refreshes at a supplied-time deadline. Known terminal responders
retain frame debt regardless of lateness; input/timers continue through the
existing Application/Presenter path. Native synchronous write stalls and physical
host visibility are separate from processing/queued-state bounds.

D-142 adds continuous private process streams over the shared finite/continuous
pipe pump. Core owns bounded requests, independent queues and status; term owns
workers, root/group/job, native readiness and cancel/capacity wakes. No helper
bytes are written to the host terminal. Session retirement never joins; owned
service shutdown does so outside UI dispatch. Quiet streams impose no timeout.

D-143 composes the bounded log services in a portable named-source demo.
Models/followers are created by the outer loop, export snapshots hold only
their source during bounded copying, and injected workers perform file writes.
Tab removal retires caption/page observers while surviving anchors remain.

D-144 keeps native OS log discovery and schema/rendering in the log-monitor app.
Private POSIX streams and native Windows pull subscriptions publish bounded
records through shipped receivers/models/controllers; workers never own views.
Source limits, bookmarks, raw envelopes, metadata clipping and continuity gaps
are explicit. Windows native acceptance remains a recorded deferred gate.

D-145 supplies a Development SDK installation and public-header-only log
clients. Documentation captures verify pinned composition plus decoded VT
capability projection. These figures are separate from native-window evidence.
