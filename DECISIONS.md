# ckVision — Decision Records

Settled decisions are binding until superseded by a new entry. Altering
one requires recording the supersession here, with rationale.

## Settled

**D-001 — Retained-mode, windowed, event-driven model.**
A persistent view tree with windows, z-order, focus, and modality.
*Rejected:* immediate-mode (cannot carry overlapping windows, modal
scoping, or focus discipline without reinventing retained state);
web-style reconciliation (solves a problem terminals don't have, at the
cost of indirection everywhere).

**D-002 — C++20; no modules; no coroutines in the core.**
Concepts, designated initializers, `span`, ranges where they clarify.
Modules wait for mature tooling (D-OPEN-4). Coroutines add scheduler
obligations the single-threaded contract doesn't need.

**D-003 — UTF-8 in `char`/`std::string`/`string_view` throughout.**
No `wchar_t`, no `char8_t` surfaces, no dual encodings. Width and
segmentation are central library services (ARCHITECTURE §2).

**D-004 — Zero mandatory dependencies; no curses, no terminfo.**
Capabilities come from curated profiles plus runtime probes, both
inspectable and overridable. *Rationale:* determinism, portability,
build simplicity, and independence outweigh coverage of exotic legacy
terminals, which are explicitly out of scope.

**D-005 — One VT/ANSI rendering model on every platform.**
Windows is served through ConPTY/VT (Windows 10 1903+); non-VT consoles
are refused with a clear diagnostic. *Rejected:* a parallel legacy
console-API renderer (doubles the rendering surface for hosts the
project does not target).

**D-006 — Explicit container layout; no style-sheet system.**
Row/Column/Grid/Dock/Overlay with size policies, one-pass computation
(with a single sanctioned height-for-width query for wrapped widgets),
plus manual geometry where wanted. *Rejected:* CSS-like cascades,
selectors, and constraint solvers — unpredictable cost and debugging
opacity for no expressiveness the target applications need.

**D-007 — Themes are flat semantic tables.**
Interned typed role ids (namespaced string forms at registration only),
per-widget role sets, application/window scope materialized at
scope-change time, runtime switching. Custom-role registration must
declare a fallback, so schemes stay complete by construction and a
printed theme is the whole truth. The classic '90s look ships as a
built-in scheme (Classic) beside Dark, Light, and Mono. *Rejected:*
palette index chains (historically proven confusing) and string-keyed
cascading styles.

**D-008 — Instance-based everything; no singletons, no globals.**
`Application` owns terminal, scene, focus, commands, theme, clipboard,
histories, platform services, loop. Multiple instances per process are
supported — on distinct terminals (one application per tty; see D-024
for the single sanctioned process-level facility).

**D-009 — Single owning thread plus `post()`.**
All UI objects are confined to their application's owning thread; the
only crossings are the post queue and cancellation tokens. The library
creates no visible threads.

**D-010 — The performance charter is binding.**
Damage-tracked painting, per-window backing stores, composition-only
window movement, cell-diff presentation, synchronized output, budgeted
latencies, zero steady-state allocations — enforced by CI (ARCHITECTURE
§8). Machine-independent budgets (allocations, cells touched, bytes
emitted) are hard CI gates; wall-clock budgets are verified on a named
pinned reference machine with ratcheted relative thresholds, because
absolute timing gates on shared runners flake and then get ignored.
A regression is a build failure, not a note.

**D-011 — Errors: assertions for contract violations, typed values for
the environment.** Fallible entry points return/report `ckv::Error`;
event and paint paths are `noexcept`; application callbacks on hot paths
are noexcept-in-effect — a throw is a contract violation that triggers
the guaranteed terminal restore, diagnostic emission, then termination.
Terminal restoration is guaranteed on every exit path including fatal
signals; diagnostics are emitted after restore (ARCHITECTURE §11).

**D-012 — Clean provenance.**
No Turbo Vision source (original or any port) is consulted, copied, or
transcribed — ever, by anyone, for any part of this project. Approved
references: published standards and documentation (ECMA-48, xterm
control sequences, kitty protocol specs, Microsoft console VT docs,
Unicode reports) and this project's own specifications and goldens.
The binding working rule lives in AGENTS.md.

The D-012 review record is
`plans/clean-room-derivation.md`. It identifies the approved ckVision
specification and executable evidence for material re-derived during an
authority repair; the `provenance_check` and
`provenance_checker_self_test` CTest gates are mandatory lexical safeguards,
not proof of provenance.

**D-013 — Commands are first-class, and the registry assigns their ids.**
A command's identity is a namespaced string key it declares itself under
(`"ckv.window.close"`, `"editor.save"`); `CommandRegistry::declare()`
returns the `CommandId` standing for that key, taken from a monotonic
per-registry counter. Nobody picks a number, so no two parties can pick
the same one; the prefix of the key belongs to the declaring party.
`CommandId` is an `enum class` over `std::int32_t`, which makes
`base + index` a compile error rather than a discouraged habit.
`declare()` is idempotent per key (a re-declaration returns the same id
and replaces the metadata) and `withdraw()` retracts one, with the key
keeping its id reserved so a recycled handle can never come to mean
something else. Ids are per-registry, per-run handles: everything
durable — configuration, menu definitions, cross-process references —
keys on the string, via `id_for()`/`key_for()`. Metadata (title,
category, default chord, palette visibility), scheme-based rebindable
keymaps, and context-scoped enablement — a context is a named scope
declared by application or widget, activated by focus ancestry or
explicit push/pop. Menus and status lines render from the registry;
`widgets::CommandPalette` filters on the declared
`CommandVisibility`, because "should the reader see this?" is metadata
only the declaring party can state. No numeric limits that force id-space
rationing.
*Supersedes* the original partitioned space (ids below 4096 reserved for
the library, applications above, extension libraries "allocating
documented ranges through the registry at registration time"). That
allocation was never implemented, so every party kept picking numbers
from a base of its own and the partitions duly collided; the boundary
also became load-bearing for semantics it could not express, with the
command palette deciding visibility by comparing an id against 4096.
*Rejected:* adding a fourth partition for widgets and extension
libraries — it re-runs the same negotiation with the next party, breaks
existing consumers of the application range, and leaves the palette
doing arithmetic.

**D-014 — Golden styled-cell dumps are the specification medium.**
A stable, diff-friendly text format for frames (glyphs + style
annotations, raster regions represented symbolically with anchor,
extent, content hash, and active fallback) captured headlessly;
appearance and interaction specs are executable (dump + event script),
byte-exact across platforms. Goldens pin the library's own width tables;
on-terminal width fidelity is D-019's job.

**D-015 — Naming.**
Product **ckVision**; library `cvision`; namespace `ckv::`; headers
`<cvision/...>`; CMake target `ckvision::cvision`. (`cv::` was rejected:
it collides with OpenCV's namespace.)

**D-016 — Trunk-only, no pre-1.0 compatibility machinery.**
Development on `main` with linear history. Until 1.0, breaking changes
are made cleanly and callers move; no shims, no deprecation layers.

**D-017 — Graphics are first-class, throughout — with mandatory
fallbacks.** Raster support is woven through every layer (`Image` in
core, raster regions as full compositor citizens in scene, encoders and
pixel input in term, image widgets on top); Sixel is the primary
protocol, kitty graphics secondary. No API accepts raster content
without its textual fallback, so applications keep one code path and
remain fully functional on text-only terminals. Emission order (raster
after cells, on top, within the synchronized frame) is a presentation
detail the API never exposes. When graphics are active, Presenter replaces the
visible raster region's fallback glyphs with style-preserving blank cells
before emitting the image; it must never rely on an image covering textual
fallback. ckVision's Sixel stream explicitly selects the opaque background
mode and declares its raster extent; source alpha is ignored because Sixel has
no alpha channel. The fallback text is emitted only by a no-graphics profile.
*Supersedes the earlier "optional gfx module" shape.*

**D-018 — One mouse model, two native coordinate spaces.**
Every mouse event carries the cell position; it also carries the pixel
position whenever the terminal reports pixels (SGR-Pixels), decoded into
the same event — no parallel event system, no per-widget workarounds.
Image-local mapping is an accessor on the event. Precision is honest:
pixel data is never synthesized from cell data; capability reporting
tells applications which precision is active. Cell pixel metrics come
from terminal queries and are re-probed on resize.

**D-019 — Width agreement with real terminals is a capability, not an
assumption.** The library's compiled-in Unicode tables are the internal
truth (goldens pin them), but terminals disagree — with any fixed table
and with each other — on emoji sequences, ZWJ, VS16, and ambiguous-width
characters. Therefore: the capability struct carries a per-profile width
policy (Unicode version / known deviations, refinable by an optional
startup width probe); the presenter re-establishes absolute cursor
addressing after any grapheme run that is not terminal-width-safe, so a
disagreement corrupts at most one run, never the frame; the East-Asian-
ambiguous policy is explicit and overridable. Mode 2027 and related
protocols are tracked in D-OPEN-7.

**D-020 — Windows input is VT-first.**
The Windows backend enables VT input and feeds the same decoder as
POSIX — one decoder everywhere, kitty keyboard protocol and bracketed
paste work where the host supports them. Its console input transport reads
the VT character stream with `ReadConsoleW`, combines UTF-16 surrogate pairs
across reads, and encodes Unicode scalars as UTF-8 before that decoder.
`ReadFile` with console code page UTF-8 replaces each half of a supplementary
scalar with U+FFFD on the named Windows Terminal Preview host; a live
clipboard roundtrip proved this while `ReadConsoleW` preserved both halves.
Reading VT characters as UTF-16 does not interpret console key semantics or
fork the VT decoder. [Windows Terminal 1.25 Preview](https://github.com/microsoft/terminal/discussions/19947)
implements the kitty keyboard protocol; the named 1.24 Stable host does not,
so it must keep the legacy keyboard fallback. The
console API is used only where VT has no channel (e.g. buffer-size
change notification). Per-host downgrades (plain-VT ConPTY hosts without
enhanced input) are recorded in the capability report and tested as
degraded rows. *Rejected:* console-API-primary input (loses protocol
fidelity on the flagship host and forks the decoder).

**D-021 — Modality is loop state; the loop is embeddable.**
`run()` is a convenience over public `step(deadline)`/`wake()` with an
exported borrowed set of wait handles and the next application timer deadline.
The handle set contains every backend source that can make `step()` useful
(POSIX terminal input and its wake channel; a future Windows backend supplies
its native equivalent); deterministic backends expose an empty set. An
embedding host waits on that set plus its own sources, then calls
`step(clock.now_nanos())` on the owning thread. Modal dialogs push a modality stack that
scopes event routing and complete via callback/notification; a blocking
convenience exists for applications that own the loop. A host `request_quit()`
that ends a blocking standard-dialog convenience force-detaches its still-open
dialog, removes its scope, and returns that factory's documented cancellation
fallback; this is terminal shutdown, not a vetoable user close request.
*Rejected:*
nested native modal loops (hostile to embedding, re-entrancy hazards
with `post()` and mid-modal destruction).

**D-022 — Clipboard: internal always, system best-effort.**
`Application` owns an internal clipboard that unconditionally backs
widget cut/copy/paste. Copy writes through to the system clipboard where
possible (OSC 52 write on VT terminals — noting tmux `set-clipboard` —
native clipboard on Windows). System import comes only from bracketed
paste or native read on Windows; OSC 52 read is never used (disabled
almost everywhere for good security reasons). In-app selection +
clipboard export is the primary copy path because mouse reporting
captures drags.

**D-023 — Superseded by D-040.**

**D-024 — One sanctioned process-level facility: the session registry.**
Fatal-signal restoration and tty-owned runtime signals (SIGWINCH,
SIGTSTP/SIGCONT) cannot be instance members; they are served by a single
async-signal-safe registry of live terminal sessions, each with its
precomputed restore bytes from the session-state ledger. Everything else
remains instance-owned (D-008). SIGTSTP restores every registered session
before the process stops; SIGCONT restores raw mode and entry sequences for
every session, then emits an instance-local capability-change event so each
Application fully re-presents its frame. SIGINT remains host policy. One
application per terminal; N applications on N terminals is supported and
tested. The registry owns its handler dispositions only while at least one
session is live: it snapshots the host dispositions before the first session
and restores them after the last. The host must not replace those same
dispositions while a ckVision terminal session is live. The supported POSIX.1-2017 baseline explicitly lists `tcsetattr` as
async-signal-safe; registry atomics are required to be lock-free and are
compile-time-asserted before any handler uses them. The registry also carries
the immutable expression, source path, and line of a pending contract
assertion: core publishes those static-lifetime fields before raising SIGABRT;
the handler restores every live session and only then writes the complete
diagnostic. This narrow termination seam is part of D-024, not a second global
diagnostics facility.

**D-025 — Superseded by D-039.**

**D-026 — No bidirectional reordering in 0.1.**
Rendering is logical-order; terminal-side bidi is not compensated
(double-reordering is worse than none). RTL support is a scoped
post-1.0 exploration; VISION's Unicode claims carry this boundary
honestly.

**D-027 — Context help is framework machinery.**
Views carry an optional help-context key; F1 resolves the focused
view's nearest key and dispatches to an application-registered help
provider; the Help viewer widget (topics, links, back, index — built on
Text view) ships in the library; status-line hints key off the same
context mechanism. Help *content* and its storage format belong to the
application.

**D-028 — Ergonomic construction: constructors take content, not
plumbing.** Widget constructors carry what the widget *is* (text,
items, mode) — never `Theme&`, `RoleId`s, `Application&`, or `Desktop&`.
The Application installs a ui context (theme, role registry,
application) on its root, propagated to every current and future
descendant exactly like the dirty-rect and detach sinks; widgets
resolve their documented standard roles lazily at attach/first paint
(interning is idempotent per D-007), with `set_role(...)` per-instance
overrides and per-window theme scope (`Window` override, nearest-scope
resolution, materialized at scope change — the ARCHITECTURE §5 shape).
A `View::on_attached()` notification marks context arrival. Typed
insertion (`add<T>`/`make<T>` returning `T*`) replaces
downcast-after-`add_child`. This preserves D-007 (flat tables,
per-widget role sets — the *sets* move from call sites into the
widgets that own them) and D-008 (the context is instance state on the
root, not a global). *Rejected:* status quo (role parameters at every
construction site — the review measured ~40% plumbing per expression,
and M6 widgets borrowing `input_*`/`dialog_*` roles because
`StandardRoles` never grew); a process-global theme (violates D-008);
passing a context parameter to every constructor (still one plumbing
argument everywhere, and it leaks into every signature forever).
Scheduled as M9/WP-7…WP-9 (plans/api-ergonomics-review-2.md).

**D-029 — Framework-supplied interaction defaults, all overridable.**
The framework itself wires the interaction grammar users assume:
click-to-focus (mouse Down focuses the nearest focusable view before
delivery), click-to-activate (a capture-phase mouse-descent hook lets
Desktop raise/activate the window containing any clicked descendant),
a working zoom control (Desktop supplies the zoom area), terminal
resize propagation to root children (fill-the-root default), a default
keymap (Tab/Shift-Tab traversal, F10 menu, window-management chords)
over the standard command set (D-013 — declared by every
`CommandRegistry` at construction and reached through `standard()`), and `run()`/`request_quit()` over
`step()`/`wake()`. Every default is an ordinary rebindable binding or
a documented opt-out — an application can clear or replace any of it;
a focused view consuming a key already preempts the keymap by routing
order, so "Tab means something else here" needs no configuration at
all. The default Quit sweep is also non-reentrant: a `kQuit` execution
triggered from one of that sweep's vetoable close callbacks is a no-op until
the original sweep returns. The original front-to-back request retains sole
authority over later vetoes and over whether it calls `request_quit()`.
*Supersedes* the previous stance that the library deliberately
binds nothing (recorded in example comments and docs/example-apps.md):
that stance made every application re-implement the same table stakes
and left VISION non-negotiable #7 (click-to-focus) unmet. *Rejected:*
defaults hard-coded outside the keymap (would make the grammar
non-rebindable, contradicting VISION #4). Scheduled as M8/WP-1…WP-4
and M9/WP-12…WP-14; the window-management chords' own Desktop-side
wiring (`kZoom`/`kNextWindow`/`kPreviousWindow`/`kTile`/`kCascade` —
`kClose`/`kQuit` landed with WP-15) completed later, once the
modality stack it needed had landed.

**D-030 — Size-hint changes propagate one hop, synchronously.**
A widget whose own preferred size changes after construction (Label/
Button's `set_text`, StaticText's wrapping) calls a new protected
`View::size_hint_changed()`, which notifies exactly its immediate
parent via a new virtual `View::on_child_size_hint_changed(View&)` —
never further up the tree automatically, and never deferred through
`Application::post()`. A container whose own aggregate hint depends on
a child's (Row/Column, which already compute their hints fresh from
`children()` on every call) just re-runs its existing layout pass;
Window and Desktop react only when the changed child is specifically
their frame overlay or a docked view, since their own layout otherwise
depends only on their own bounds. A container that itself needs to
bubble the change further up calls its own `size_hint_changed()` from
inside its override — single-hop is the primitive, not a limitation.
*Rejected:* an automatic multi-ancestor walk (mirroring
`on_descendant_mouse_down`'s "notify every ancestor" shape) — every
concrete container this landed for (Row/Column/Window/Desktop) only
ever needs to react to its OWN direct children, and a walk that always
continues to the root would force every intermediate View to opt out
explicitly rather than the reverse. *Rejected:* deferring the
notification through `post()` — nothing else in this propagation path
is deferred (layout itself is a synchronous, immediate pass per
ARCHITECTURE.md §5), and introducing a deferred hop here would let a
container render one frame with a stale layout before catching up.
Retired `Window::relayout_frame_overlay()`, the explicit manual hook
this replaces. Scheduled as M9/WP-16.

**D-031 — Frame-overlay slots: two edges, `ui::Alignment` reused, one
overlay per (edge, alignment).**
`widgets::FrameSlot{Edge, ui::Alignment, offset}` positions a
`Window::add_frame_overlay` child on the border itself. `Edge` is
deliberately just `{Top, Bottom}`, NOT `ui::Dock`'s full `{Top, Bottom,
Left, Right, Center}` set — a window's side borders are one cell wide,
with no room to host meaningful overlay content, so a fifth/sixth value
would have no real use. Alignment within an edge's own row reuses
`ui::Alignment` (`Start`/`Center`/`End`/`Fill`) rather than a new
enum — the same values Row/Column/Grid already position children with,
now positioning an overlay within its row exactly the way
`align_cross_axis` positions a child within its container (frame_
overlay_rect literally calls it). At most one overlay per (edge,
alignment) pair — `CKV_ASSERT`s on a second, mirroring `ui::Dock`'s own
"at most one child per edge" contract; several overlays at the same
position nest in a Row the caller builds, the same answer every WP-19
container gives for anything beyond its own scope. `Edge::Top` further
restricts alignment to `Start`/`End` only — `CKV_ASSERT`s against
`Center`/`Fill` there, since the title already owns Top's center; this
is the "deterministic precedence" WP-20's own acceptance text asks
for, made an explicit contract violation rather than an implicit
"whoever paints last wins" stacking rule. Retired the single hard-wired
bottom-right `set_frame_overlay`/`frame_overlay()`; all three call
sites (the implementation itself, `examples/filebrowser`, and
`tests/test_window.cpp`) migrated in the same change, per this
project's own no-shim policy. Scheduled as M10/WP-20.

**D-032 — "Terminal too small" is a framework-level render substitution,
not a widget.**
ARCHITECTURE.md §5's sizing policy names three zones: full chrome
(≥`kMinFullChromeSize`, 80×24), degraded chrome down to a hard floor,
and a "terminal too small" state below it. Only the third zone needed
new code: `Window::reposition_within` (M8 WP-1/WP-4) already clamps
every window to fit and stay reachable as `root()` shrinks, and that
existing clamped rendering IS "degraded chrome" — there is no separate
degraded visual mode for any widget to implement. `kHardFloorSize`
(20×6) is the one new threshold: `Application::paint_and_present()`
checks it directly and, below it, paints a fixed two-line message
("Terminal too small" / "Resize to at least 80x24") instead of walking
the normal `View` tree at all — a fixed `Style` NOT resolved through
`theme()`/`roles()`, since this is a last-resort fallback that must
render sanely even for an application that never configured a theme.
Recovery is automatic: the next frame `root()` is back at or above
`kHardFloorSize`, `paint_and_present()` resumes the normal
`root_.paint_children()` call — nothing is undone or reset on either
side, since nothing besides which branch runs was ever touched.
Boundary is strict-less-than (`kHardFloorSize` itself still renders) —
consistent with `Window::min_size_`'s own "the minimum is itself a
valid, attainable size" convention. Scheduled as M10/WP-21.

**D-033 — TreeNode payload is non-template; parent-tracking is a
per-flatten transient, not a stored field.**
`TreeNode` gains `std::uint64_t id` plus `std::any user_data` rather
than a template parameter — `TreeNode` stays a plain, ABI-stable value
type usable without a widget header knowing an application's own
payload type; `ckv::filebrowser` stores each node's own full path in
`user_data`, retiring a separate sidecar
`std::unordered_map<const TreeNode*, std::string>`. Lazy population:
`bool children_known` (default `true`, so every existing tree stays
unaffected) — a node built with it `false` still shows an expander
(`TreeNode::might_have_children()`) even though `children` is
currently empty, since whether it HAS children simply isn't known yet.
`TreeView::on_expand_request(TreeNode&)` fires the first time such a
node is asked to expand (`Right`, `Enter`/`Space`, or a twisty click —
all three now route through one shared `TreeView::set_expanded`
helper so the gate lives in exactly one place); `children_known`
becomes `true` immediately afterward regardless of what the callback
did, so a genuinely empty directory reads as an ordinary leaf from
then on rather than re-asking on every later expand attempt.

Parent-tracking for `Left`-jumps-to-parent is deliberately NOT a
stored `TreeNode* parent` field: `TreeView::flatten_into`'s own
existing recursive walk already knows each node's parent the moment it
recurses into `children`, so `VisibleEntry` gained a third field
(`TreeNode* parent`, `nullptr` for a root) computed fresh on every
flatten pass instead. A stored field would need its own bookkeeping to
stay valid across `set_roots()` rebuilding the whole forest, and reuses
zero of the walk flatten_into already performs; a transient, always-
fresh value has nothing to go stale. `on_selection_changed`/
`on_activate` (`TreeView`) become `std::function<void(TreeNode&)>` —
passing the node directly, mirroring `ListView::on_activate`'s own
existing index-carrying shape; `ListView::on_selection_changed` gains
the matching `std::size_t` index argument it previously lacked, for
the same "the callback gets what it needs without re-querying the
widget" reason. `TreeView::set_roots()` with an empty forest no longer
fires `on_selection_changed` at all (a real, small contract change
from "always fires, even to nothing" — there is no `TreeNode&` to pass
a reference to when nothing is selected). Scheduled as M10/WP-22.

**D-034 — Completion is acceptance-based; historical landing is not current
status.** A work package or milestone is `Done` only while every literal exit
criterion and applicable global verification gate holds. Landing some planned
code, substituting a narrower test, accepting an implementation compromise, or
recording a green local build is useful historical evidence but never changes
an unmet criterion. A later defect, contradiction, missing platform gate, or
missing required evidence reopens the owning package and every dependent
milestone. Reopening does not erase landing history; it separates that history
from the current acceptance state.

Before the final global matrix is available, a package whose literal
package-specific criteria hold may use `Implementation complete — gate pending`.
That state advances the execution queue but is explicitly not `Done`; WP-38
must either promote it with the missing global evidence or return it to `Ready`
with the failed criterion named.

The authoritative meaning of “work on the next open WP” is the first
`In progress` row, otherwise the first `Ready` row, in
`plans/corrective-assurance.md`'s Current execution queue. Legacy ledgers and
milestone prose are not executable queues. Completion changes must update the
queue, reopened-package mapping, ROADMAP acceptance table, and affected catalog
ledgers together. *Rejected:* treating `Done` as “the scoped commit landed”
while known acceptance gaps remain — that interpretation allowed later
developers to skip required repairs in numerically earlier packages.

**D-035 — Graphics verification uses symbolic and protocol-decoded oracles.**
Raster scene semantics and terminal presentation are different contracts and
must not validate each other through one shared object path. Styled-cell golden
dumps retain D-014's symbolic raster records (visible anchor/slice, pixel
extent, content hash, fallback state) to specify scene/compositor behavior in a
stable, diff-friendly form. Separately, a deterministic virtual display consumes
the exact bytes written through `Terminal::write`, parses the supported VT
output subset, decodes Sixel from its published protocol, and produces a cell +
RGBA pixel-plane capture. End-to-end graphics visual goldens and generated
screenshots come from that decoded output, never directly from the source
`Image`, `Surface`, or `RasterSlice`.

Tests use named per-instance capability profiles, including at least Sixel and
NoGraphics with fixed cell-pixel metrics, and drive identical application
scripts through both. Sixel acceptance requires actual decoded pixels;
NoGraphics acceptance requires the mandatory cell fallback and zero raster
pixels. Raw-byte assertions, symbolic raster goldens, and decoded visual
goldens remain independent gates. The virtual display is validated with
hand-authored fixtures derived from published terminal/Sixel documentation and
fails on unsupported output rather than silently ignoring it. *Rejected:*
calling a cell-only `FrameView` SVG a graphics screenshot; rendering the source
Image directly in test tooling (bypasses Presenter/encoder); relying only on a
DCS substring assertion (proves emission, not rendering or placement).
Documentation SVG/PDF renders reconstruct recognized Unicode box-drawing cells
as exact cell-edge geometry instead of delegating them to ambient font metrics;
otherwise font fallback can make a correct corner glyph appear disconnected.

**D-036 — Junction merging is scoped to one logical view paint.** A visible
box-drawing glyph does not identify the drawing operation or z-layer that
produced it. `Surface` therefore retains the exact connector directions and an
opaque, surface-local paint-scope id beside every line cell. A view receives a
fresh scope; translated and clipped painters derived inside that view retain
it. Ordinary cell writes clear the provenance, while style-only transforms
preserve it.

`Painter` merges connector directions only when the stored scope matches its
own. Otherwise the new line replaces the old cell normally, so a foreground
window border crossing a background window border remains the foreground
border instead of becoming a cross. Intentional dividers and rules drawn by
the same view still join its own frame automatically. Scope ids are allocated
per `Surface`, deterministically, with no global registry or object-address
identity. *Rejected:* reverse-inferring ownership from any Unicode
box-drawing glyph already visible in the cell (conflates unrelated z-levels);
disabling junction merging altogether (loses intentional same-view joins);
relying on eventual per-window backing stores alone (does not protect
overlapping sibling/overlay views that share a surface).

**D-037 — Shadow coverage is binary and idempotent.** A composed cell is either
covered by at least one shadow above its visible content or it is not. Exactly
one configured shadow transform applies in the covered case, regardless of how
many footprint pieces or distinct windows cover the cell. The standard
right/bottom footprint is represented as two non-overlapping rectangles. The
retained Compositor accumulates a boolean while resolving z-order; the interim
single-surface Desktop path carries the same binary marker beside each cell,
cleared by any later content write and preserved by style-only operations.

This preserves z-order: a lower window's shadow cannot dim a higher window that
is painted afterward, while shadows from multiple higher windows still dim
lower content exactly once. *Rejected:* applying a multiplicative dim transform
for every covering rectangle or caster (creates artificial dark intersections);
special-casing only the lower-right corner (leaves cross-window overlap broken);
making `default_dim` itself idempotent through text attributes (conflates shadow
coverage with legitimate content styling and does not govern custom transforms).

**D-038 — Dialog presentation makes modality and completion explicit.**
Constructing a `WindowHandle` is separate from presenting it. `Desktop` exposes
distinct modeless and modal presentation operations; no method named `show_*`
silently chooses modality. A modal operation establishes its input scope before
returning and that scope ends when its root detaches, so neither menu/status
input nor background accelerators can leak into the interval.

Every standard-dialog family, including the generic descriptor dialog, supplies
a non-blocking `present_*` convenience that returns a typed presentation
handle. It reports one deterministic typed completion after the presented
window has detached: an explicit accepted result wins; a close control,
external detach, or quit uses that dialog family's documented cancellation
result. Completion is therefore safe to use from a command handler and never
starts a nested loop. The matching `exec_*` family is the outer-loop-only
blocking wrapper over the same presentation path. Raw `Window*` observers
remain appropriate for general application windows, not for obtaining
standard-dialog results. A saved focus target is a per-instance lifetime
capability, never an unchecked raw pointer retained across a modal interval.

*Rejected:* a Boolean modal option (call sites conceal the consequential
choice); callbacks that fire while the dialog is still attached (a callback can
open a nested dialog against a stale scope); a future/promise or coroutine
surface (would add scheduler or polling obligations contrary to D-002/D-009).

**D-039 — Platform-service contracts live at the lowest dependency-valid
layer.** The old D-025 rule put service interfaces in `ui` while requiring
`term` to implement them. That is an upward dependency and is therefore
impossible under the layer graph. Abstract contracts carrying only core value
types live in `core`: `Clock`, `FileSystem`, and `ClipboardWriter`.
Deterministic `ManualClock`, `MemoryFileSystem`, and `MemoryClipboardWriter`
are ordinary in-memory instances, not global test switches. Platform adapters
live in `term`: `PosixClock`, `PosixFileSystem`, and
`TerminalClipboardWriter`; the latter deliberately adapts a specific
`Terminal` session instead of making `Terminal` a general platform-services
bag.

Terminal input, wait handles, and wake remain the `term::Terminal` session
contract: they cannot be a `ui` platform service because the terminal backend
is the resource that owns those native handles. `ui::Application` receives a
terminal and clock explicitly. It accepts an explicit `ClipboardWriter` for
system export, or owns one `TerminalClipboardWriter` for its compact
two-argument construction. A caller that supplies a bridge or filesystem
retains it for the consuming application/widget lifetime and uses distinct
instances for distinct applications. No service contract or implementation is
a singleton, static default, environment lookup, or implicit cross-application
channel.

System clipboard import is not a service read: it enters as a terminal/native
`TextEvent` marked as paste and is copied to Application's portable internal
clipboard (D-022). This preserves the no-OSC-52-read security boundary.
Future platform services follow the same rule: their abstract contract is
placed in the lowest layer that can express its types and all consumers inject
it; an adapter may only depend downward. *Rejected:* `ui`-owned interfaces
implemented by `term` (reverse dependency); a monolithic service locator
(hidden global state and unnecessary coupling); treating terminal session I/O
as a generic host service (obscures lifetime and wait/wake ownership).

**D-040 — Bracketed-paste framing is a bounded recovery signal, not a
cryptographic provenance proof.** The protocol carries both its end delimiter
and pasted bytes in one undifferentiated byte stream. A literal `CSI 201~` in
clipboard text is therefore indistinguishable from the terminal's real closing
delimiter. No decoder can prove which occurrence was intended; D-023's former
claim that embedded terminators are simply neutralized was false.

ckVision holds every candidate end marker for a documented 50 ms quiet period.
Bytes arriving before that period expires remain a sanitized `TextEvent` with
`from_paste = true`; they cannot become `KeyEvent`, mouse, OSC, or command
input. If a later candidate arrives, the earlier marker is rendered as safe
literal replacement text and the final candidate is held again. An incomplete
or disconnected paste likewise produces a recovered, sanitized paste text
event. `TextEvent::paste_recovered` tells applications that the delivered text
crossed this conservative recovery path, so they can show their own status if
appropriate. A negotiated kitty host may report the release of the paste
shortcut immediately after the end marker. A candidate tail consisting only
of valid kitty key-release events is consumed as protocol bookkeeping; it is
neither inserted into the pasted text nor dispatched as keys. Any press,
ordinary byte, or incomplete sequence keeps the entire tail in sanitized
paste recovery. As with the marker itself, an intentionally pasted suffix
identical to those release bytes cannot be distinguished from a real release
and may be omitted; it cannot become a command.

The quiet period is deliberately a usability/security trade-off, not a trust
oracle. After it expires, a later byte is ordinary terminal input; an entity
that can inject arbitrary terminal bytes with chosen timing is outside the
bracketed-paste protocol's attainable trust boundary. Applications requiring
stronger provenance must obtain it from an out-of-band host input channel or
require an application-specific confirmation. OSC output and displayed text
remain separately sanitized at their boundaries.

*Rejected:* delivering bytes after the first candidate as ordinary keys
(silently turns common embedded-delimiter clipboard payloads into commands);
claiming timing proves origin; dropping interrupted content (needlessly loses
user text); and a decoder-only warning invisible to applications.

**D-041 — 0.1 controls are materialized cell controls; virtualized models and
scroll-delta optimization are post-0.1 unless D-043 promotes a proven client
need.** The M6 catalog mixed first-release control behavior with
scale/performance extensions that need separate model contracts. For 0.1,
`ListView`, `Table`, and `TreeView` retain compact materialized convenience
APIs: the application may supply row/tree values and the widget owns
deterministic cursor, selection, and drawing behavior. The baseline contract
does not include a generic erased row provider, columned `ListView` mode,
typed/numeric table comparators, or a cell-level table editor. D-043 now
promotes the concrete List, Table, and Tree provider contracts from real
clients without changing the compact value APIs.

`ComboBox` is the first 0.1 dropdown/history control: it owns editable and
pick-only state, draws its own in-bounds dropdown, and integrates with the
application-scoped `HistoryRegistry`. `InputLine` history remains cycling,
not a popup; a popup field is a combo box, not a second input-line mode.

`ScrollViewport`'s 0.1 behavior is semantic scrolling: content bounds,
auto-hidden scrollbars, keyboard/wheel input, and correct event delivery.
Terminal scroll-region diffing is an output optimization owned by damage and
presentation work, not a public widget behavior required for 0.1. It may be
added later if benchmarks prove it necessary, without changing the
`ScrollViewport` API.

*Rejected:* forcing every list-shaped widget through one generic virtual model
before there is a concrete client; treating table cell editing as part of a
forms/widget library rather than an application editor concern; making
`InputLine` open popups when `ComboBox` is the explicit dropdown control; and
calling scroll-region byte savings a catalog feature instead of a renderer
optimization.

**D-043 — Provider-backed data views and flow content are explicit widget
contracts.** Cworks-class interactive applications establish concrete clients
for large data navigation and rich read-only presentation. ckVision therefore
provides separate provider contracts for `ListView`, `Table`, and `TreeView`,
rather than one erased “items” interface. A provider is caller-owned,
synchronous for one requested visible item, cell, or tree path, and identifies
every row/item with a stable, opaque value. It never owns UI state, starts
work, reads a service, or calls a widget from another thread. An application
that loads data asynchronously updates its own provider and calls the view’s
explicit `model_changed()` on the UI thread (normally through
`Application::post()`).

Cursor, primary selection, and multi-selection store provider identities, not
display offsets. Filtering, sorting, paging, and a refreshed backing store
therefore preserve a selection whenever its identity remains present; a
removed identity is cleared deterministically. The view paints and queries
only its visible slice plus the cursor-resolution queries required after a
model change. Ordering belongs to the provider: header activation requests a
sort but the widget never silently materializes or reorders application data.

`TreeModel` additionally supplies root, parent, child-index, and reverse-path
lookups. `TreeView` owns expansion, cursor, and selection by stable item id;
it resolves only the expanded path needed to count, navigate, or paint a
visible row. Refresh keeps every surviving expansion and selection identity,
while a removed identity is discarded. The model describes hierarchy and item
content only; it never stores widget expansion state or performs a filesystem,
network, or background query on behalf of the view.

`Table` exposes a closed, portable `CellValue` value vocabulary and a
column-owned edit specification. It performs the standard text, Boolean,
integer, and real conversion rules, but a provider remains the final authority
for validation and commit. Failed commits retain the editor and report a
caller-supplied diagnostic; successful commits preserve stable row/column
selection. Application-specific types, formulas, database transactions, and
range semantics remain application models built on this contract.

`FlowView` is the reusable read-only counterpart to `TextEditor`: wrapped,
styled text runs, links, explicit block breaks, and caller-owned inline raster
objects share one deterministic layout and scrolling model. Raster content
always goes through `Painter::draw_image`, so every inline image has an
extent-bounded cell fallback and participates in ordinary clipping,
occlusion, and capability degradation. Flow content is an application-owned
value document; parsing Markdown, chat protocols, document layout policy, and
background raster generation remain outside the widget.

*Rejected:* indexing selection by a volatile display row; an asynchronous
provider callback protocol (it would conceal threading, lifetime, and retry
policy); a framework SQL/formula/document model; and treating rich flow as a
special terminal-output path that bypasses the scene compositor.

**D-042 — Embedded terminals are private emulated subsessions, never outer
terminal pass-through.** An embedded terminal window owns an explicit child
process and private PTY/ConPTY endpoint through the `term` layer. Child output
is parsed into an off-screen terminal model and then painted by
`widgets::TerminalView` through the normal scene compositor. The application's
outer `Terminal` remains the sole writer to the user's terminal. This confines
child cursor movement, alternate screens, OSC/DCS, and graphics to the child
model, and lets the compositor correctly clip, occlude, shadow, clear, and
re-encode child Sixel content.

The child terminal profile, launch specification, input escape policy, query
replies, OSC/clipboard/title behavior, parser limits, and termination policy
are explicit instance values. No child attaches to the parent application's
outer tty and no profile is inferred from the environment. `term` owns native
handles, process lifecycle, byte I/O, and emulator state; its public child
session adapter derives from the dependency-neutral `core::TerminalSubsession`
contract. `ui::Application` multiplexes readiness; `TerminalView` consumes
only that core contract and forwards focused view-relative input. The complete
design and required evidence are in
`plans/embedded-terminal-architecture.md` and
`plans/embedded-terminal-delivery-target.md`.

*Rejected:* forwarding raw child bytes (allows cursor/graphics escape and
corrupts parent presentation); treating a PTY as a generic widget-owned file
descriptor (breaks session and wait ownership); spawning through an implicit
shell (ambiguous quoting and policy); global child-session registries; and
accepting a screenshot without decoded pixel-plane proof of containment.

**D-044 — A colour keeps the kind it was asked for.** `Color` is one of three
things — the terminal's own default, a palette index, or a specific 24-bit
value — and the distinction survives every layer. Resolving an index to RGB on
the way in answered the palette question once, permanently, at the moment the
byte arrived: the cell remembered red-as-it-was-then rather than "red", and no
later re-theming could reach it. `core/palette.hpp` is the single place that
turns an index into channels, for the things that genuinely need pixels
(shadow dimming, SVG capture, quantisation for a host with fewer colours);
everything in between carries the index, and the `Presenter` passes it out to
the host unchanged so the reader's own theme applies to it. The golden format
spells the three kinds `default`, `@<index>` and `#RRGGBB` for the same
reason: a dump is a specification, and it must not record an answer the source
never gave. *Consequence, deliberate:* bold brightening applies to the low
eight palette entries only — a program's own `38;2;205;49;49` is a colour it
chose, where before it was indistinguishable from palette red and was
brightened with it. *Rejected:* resolving at parse time (loses the fact);
carrying a parallel "original index" beside the RGB (two representations of
one thing, and every consumer must remember which is authoritative).

**D-045 — An underline has a shape and a colour, and only while it exists.**
`Attr::Underline` says a rule is drawn; `Style::underline` says which rule and
`Style::underline_color` says what colour, and both are meaningful only while
the attribute is set. Clearing the underline restores them, so two cells that
render alike compare alike and the presenter's diff does not redraw cells
nothing happened to. The shapes are emulated always and emitted only to a host
that declares `Capabilities::underline_styles`: a terminal that has never
heard of the colon form reads `4:3` as two parameters and applies italics, so
a shape is never offered on the chance that it lands. Where the capability is
absent every shape degrades to the plain rule, which still reads as emphasis.
The behaviour itself — `SGR 21` as a double rule, `4:0` as none, `4:5` as
dashed — was **measured** against a reference terminal through
`tools/vtconform/`, not decided. *Rejected:* removing `Attr::Underline` in
favour of a shape enum alone (`Attr` is the project's vocabulary for text
attributes and several public surfaces carry it); separate `Attr` bits per
shape (mutually exclusive states modelled as flags).

**D-046 — A child may write the clipboard by opt-in, and may never read it.**
`OSC 52` is the one child-initiated action that reaches outside the terminal
window: it replaces what the person at the keyboard had copied, without their
having asked. That is a feature when it is `yank` in an editor they are
driving and an attack when it is a file they happened to `cat`, and the
difference is not visible from inside the terminal — so writing is opt-in per
profile (`TerminalClipboardPolicy`, default `Deny`), bounded by
`max_clipboard_bytes`, strictly base64-decoded, and sanitized for a clipboard
rather than for a cell. The emulator records the text and a count of granted
requests; `TerminalView` exports it when the count moves, because a snapshot
is a value that may be read any number of times and reading must not be what
causes an export. Reading is refused under **every** policy, extending D-022's
no-OSC-52-read boundary inward to child sessions: a program that can read the
clipboard can read whatever its reader last copied. *Rejected:* a policy value
that enables reads (there is no safe form of it); delivering the text through
a queue that `snapshot()` drains (makes an observation destructive).

**D-047 — Keyboard enhancements are masked to what will really be delivered.**
The kitty keyboard protocol is progressive enhancement: a program asks for
flags, reads back what it got, and keeps its fallback for the rest. ckVision
therefore masks a requested flag to what the session can actually honour *at
the moment it is set*, so `CSI ? u` reports what will really arrive rather
than what was asked for. `ReportAlternateKeys` is consequently dropped —
ckVision's input model carries a key and the text it produced, not the layout
behind them — and a program is told so rather than being led to stop looking
for the fallback it still needs. The flag stack is per screen: a full-screen
program's settings must not follow it out onto the shell's screen, and a
program that dies without popping must leave nothing behind. The legacy
encoding remains the default and the fallback for every key the flags in force
do not cover. *Rejected:* storing every requested flag and honouring a subset
(reports a promise the terminal does not keep); one stack shared by both
screens (leaks a dead program's settings to the shell).

**D-048 — A child's picture is pixels on a screen, decoded at its own size and
erased by writing over it.** A Sixel arriving from a child is decoded into an
image the size of the picture, anchored to the cell the cursor was on. It is
not decoded into a plane the size of the terminal and searched for afterwards:
that made the cost of a small logo a function of how large the reader had made
their window (0.7 ms in a small one, 15 ms in a large one, for identical
bytes), and made the pixel budget a window budget, so a maximised terminal
silently had no graphics at all. A picture larger than the room it has is cut
off at the edge of the screen, as any terminal cuts one off, rather than
refused; `max_image_pixels` bounds the picture, which is the quantity a child
controls.

Once placed, a picture behaves the way a terminal's pixels behave. Writing or
erasing the cells it covers takes those pixels away, one cell at a time, and a
picture with no cells left is gone — this is what makes a program's repaint
remove its own image instead of leaving it stranded over the new text.
Pictures accumulate rather than replace: a child drawing two of them has two,
and a nested ckVision emitting one Sixel per visible slice of an occluded
image gets all of its slices. They ride with the text when it scrolls and
leave when they would have gone off the edge, since a cell-anchored picture
cannot be shown half-way off.

The presenter states the same rule from the other side: a picture already on
the host, on cells nothing repainted this frame, is not sent again, and what
is sent again is re-sent from its cached encoding. Re-encoding and re-sending
every frame cost hundreds of milliseconds per frame for a dialog-sized image —
a terminal with a picture in it was slower than one without, for pixels that
were already correct.

*Rejected:* holding one picture at a time (loses every slice of an occluded
image but the last); dropping a whole picture when any cell of it is written
(a status line beside an image would erase it, and the program has no reason
to send it again); clipping a scrolled picture to the region (a half picture
anchored to a cell it no longer starts at); and re-emitting every frame as
insurance against a host that might have lost it (the cost is certain, the
loss is not, and `invalidate()` already exists for the cases that are).

**D-049 — The pointer's shape is asked for without asking permission, and the
pointer's position is always reported.** Two decisions that only make sense
together, because neither is worth anything alone.

A session enters DEC mode 1003 rather than 1002, so motion is reported whether
or not a button is held. Mode 1002 reports motion only during a drag, which
means the position of the pointer between clicks — the one fact a pointer
shape or a hover highlight is a function of — is never known. The cost is one
report per cell the pointer crosses; the ui layer resolves the view under it
and drops the report when nothing changed, so a motion that hovers the same
view as the last one reaches no widget and writes no bytes. Profiles with no
mouse at all are untouched, and keep their documented keyboard completeness.

Pointer shapes (OSC 22) are then written to every host, without waiting for
one to say it can draw them. This is the only capability here that is not
gated on evidence, and the reason is that the protocol affords no evidence to
gate on. Its support query is an optional extension: hosts implementing the
original xterm proposal — which draw the shapes perfectly well — never reply
to anything, so silence cannot distinguish "cannot" from "did not answer". A
capability that waited for proof would switch the feature off on exactly the
hosts that have it. Writing it unproven is safe in a way no other unproven
capability here would be, because an unrecognized OSC is discarded: the
failure mode is that nothing happens. Contrast D-045's underline shapes,
gated precisely because a host that misreads one draws the wrong thing.

Shapes come in pairs where the pair is the feedback: a title bar is Grab and
becomes Grabbing while the window is actually in hand. A host with only one of
the two shows no transition, which understates the moment instead of
misstating it. The legacy names are chosen for agreement rather than
literalness for the same reason -- the two X11 hands mean different things on
different hosts (kitty reads `hand1` as the open grab hand, iTerm2 as the
pointing finger), so a grab is written `fleur`, which both agree on.

The query is still sent, as refinement rather than permission. A host that
answers has identified itself as implementing the kitty specification and has
listed which shapes it has, so the session switches to the CSS vocabulary and
degrades per shape against that list. A host that says nothing gets the X11
cursorfont names of the original proposal — the vocabulary both host families
accept — with two documented degradations: the diagonal corner resizes are
written as the crosshair (no host in this vocabulary has a diagonal, and the
single-axis arrows would state an axis a corner does not have, since a corner
pulls both; the crosshair claims no axis and marks the point being taken hold
of), and a closing hand stays the open one, and progress borrows the wait pointer.

Having no opinion is the ordinary arrow, stated rather than left to the host.
Handing the pointer back with the protocol's reset looks more polite and is
wrong: a host is entitled to draw its own pointer while mouse reporting is on
— iTerm2 draws an I-beam with a circle, to say the mouse is going to the
running program — and over a full-screen application with windows and menus
that reads as a text cursor everywhere, including the desktop behind them. An
application that has taken the screen owns the pointer on it. The reset is
kept for the end of the session, where it hands the pointer back for good. The query
is itself an unknown shape name to a host that does not implement it, and an
unknown name resets the pointer, so a probe is followed by a re-present that
states the shape again. The shape is reset when the session ends, from the
same ledger a fatal signal replays, so a crash mid-hover cannot leave a
resize arrow over somebody's shell.

A shape is a request about appearance and never about behaviour. Nothing may
depend on hover for correctness — a terminal that reports no motion has no
hover at all, and every pointer affordance keeps its keyboard equivalent.
Widgets say what they mean (`View::pointer_shape_at` returning nullopt to let
the container answer) and are held to what they can do: `Splitter` is
keyboard-adjustable in v1, so it does not offer a resize pointer it would not
honour. An embedded child's OSC 22 is understood and deliberately dropped:
the pointer belongs to the window the child is a view inside, and a child in
a background window could otherwise change it.

*Rejected:* emitting only after a host answers (kitty yes, iTerm2 never —
switching the feature off where it works); probing the host's name and
version through XTVERSION to recognise set-only implementations (a
name-to-feature table that ages, for a question whose answer changes nothing,
since the unproven path has to exist regardless); entering 1003 only once
shapes are established (couples hover highlighting to a capability users
experience separately); and treating hover as an application opt-in (a
default that leaves the feature invisible is a feature nobody has).

**D-050 — A dialog button states its role, and the role is the behaviour.**
`ButtonDescriptor` carries a `ButtonRole` — `Accept`, `Dismiss` or `Neutral` —
in place of the `is_default` flag it had. The flag could describe exactly one
of the three kinds of button an action row holds. Everything else was "not the
default", which the descriptor path honoured by running that button's own
handler and doing nothing further: a Cancel with nothing of its own to do was
an inert control, and an application that wrote one shipped a dialog that
ignored its own Cancel. A descriptor is written before its window exists, so no
caller could wire the close from its own side either.

`Accept` is the default button: Enter reaches it from anywhere in the dialog,
the fields are validated first and a failing validator vetoes the press, and
the completion carries the values. `Dismiss` runs its own handler if it has
one and then takes the window's cancel path — the same closure Esc runs, so the
two cannot drift apart — completing with `accepted == false` and no values.
`Neutral` runs its handler and leaves the dialog up, which is what Apply,
Browse… and Reset need; it is the default role because it is the one that does
nothing on its own. At most one `Accept` per dialog, asserted at
materialization as the flag was.

*Rejected:* a second boolean beside `is_default` (two flags, three meaningful
combinations, and the inert-Cancel spelling stays writable); making every
non-default button close the dialog (silently turns an Apply into a close); and
closing from `materialize_dialog` (it has no window — the wiring belongs where
the window is).

**D-051 — A terminal's scrollbar stands on the window frame, and side borders
can carry an overlay.**
`Window`'s frame overlays (M10/WP-20) accept `Edge::Left`/`Edge::Right`: a
one-cell-wide slot spanning the border rows between the corner cells, aligned
down the edge the way a top slot aligns across it. The reason is the one
overlay a side border has always carried on the classic desktop — a
scrollbar — and the reason THAT belongs on the frame rather than inside the
content is specific to terminals: a bar inside the view either covers the
content's last column or costs it one, and a column that comes and goes is a
PTY resize the child reflows for. The border cell was always there, so the
bar's appearing and disappearing is free. `attach_terminal_scrollbar(window,
view)` packages the binding: `ScrollbarPolicy::Auto` while the terminal is on
its primary screen (shown exactly while there is somewhere to go), `Hidden` on
the alternate buffer, positions mapped end-for-end (the bar counts from the
top of history, the view counts back from the live edge), with
`TerminalView::scroll_state()`/`on_scroll_state_changed` as the one seam the
binding observes.

*Rejected:* a Scrollbar child inside `TerminalView` (covers or costs a
content column, and hosts that treat the window's content view as the
terminal — a legitimate reading `Window::content()` invites — would find a
wrapper where they expected the view); `Edge` growing a `Center` (a border
has no middle ground to give); wheel entry into a separate copy-mode surface
as the default gesture (reading history is lighter than selecting from it,
and the transient scroll state costs the reader nothing to leave).

**D-052 — Everything a terminal changes about itself sets a damage flag, and
the flags name what happened rather than where it happened.**
`TerminalDamage` is not a repaint hint. It is the answer to "what do I have to
send", asked once per terminal per tick by a host that reads nothing else when
the answer is nothing — which is what a delta transport is, and what
`damage().any()` was built for. Three state changes did not answer it: the
kitty keyboard flags, the clipboard serial, and the child's own death. Each was
therefore invisible to exactly the host the flags exist for, and each failed in
its own way — a child answered in an encoding it had stopped expecting, text a
child put on somebody's clipboard sitting until the next keystroke moved a
cell, and a dead child's window still looking alive because a terminal that has
stopped makes no further changes to be noticed by.

The flags now cover them, and the choice of flag is the decision. The keyboard
enhancements are `modes`: they are a set of switches a program turns on for as
long as it needs them and puts back on the way out, which is what every other
mode in that flag is, and a host re-reads the mode scalars for all of them
together. The others are not modes, and each gets its own: `clipboard`, `bell`,
`printer`, `diagnostics`, `lifecycle`. Three of those gate a read that is not
free — the clipboard text, the print spool and the diagnostic ring are
deliberately outside `TerminalStatus`, so the flag is the only thing that says
fetching one is worth doing now — and the other two are things a host acts on
differently from anything else (ring or badge; close a window and stop asking).
The bell moved off `modes`, where it had been borrowing a flag that told a host
to re-read a set of switches none of which had moved. `TerminalStatus` gains
`diagnostics_serial` so the ring has the count every other payload has: the
ring is bounded and drops its oldest entry, so its size goes back down and
cannot be watched. A finished print job stopped claiming a full repaint —
`damage_everything()` on a print screen cost a host the entire grid to say a
document was ready.

*Rejected:* one `meta` flag for everything that is not cells (it is the
cheapest thing to write and the most expensive to consume — a host learns that
something happened and has to fetch the clipboard, the spool and the ring to
find out which); leaving the bell on `modes` once its siblings had flags of
their own (three notices, one of them hiding); and letting hosts poll
`status()` every tick regardless of damage (that is the copy-per-frame U0-b
exists to remove, one level up).

**D-053 — The keypad stays numeric, and the terminal says so by staying
quiet.**
`ESC =` and `ESC >` (DECPAM/DECPNM, terminfo's `smkx`/`rmkx`) are consumed as
well-formed input and have no effect, and DEC mode 66 with them. What they
change is which bytes the *keypad* keys send, and ckVision's input model has no
keypad key: `Key` names `Enter`, the arrows, the function keys and `Char`, and
the numeric keypad reaches an application as the digits and operators it
produced, indistinguishable from the top row. A tracked flag would therefore be
a setting nothing could read and nothing could act on, and a `TerminalStatus`
field for it would tell a host something it cannot use.

Real support is a larger change than this one, and it belongs to the input
model rather than to the emulator: `Key` grows the keypad identities, the
decoder learns `SS3 p`…`SS3 y`, ckVision itself enters application keypad mode
on its *host* terminal (nothing else makes the host report those keys
distinctly), the mode joins the ledger a fatal signal replays, key names for
configuration and documentation grow with the enum, and `TerminalView` gains
the second encoding. Until that is worth doing, the honest state is none.

The diagnostic is deliberately not raised. Every curses program sends one of
these on the way in and the other on the way out, so reporting them as
unsupported filled a bounded ring — 64 entries, oldest dropped — with the one
thing every child does, crowding out the complaints a reader needs to see. This
is the same treatment as OSC 1 and OSC 22: known, deliberately without effect,
and consumed rather than complained about. A diagnostic in ckVision means "a
child asked for something and did not get it, and could not tell"; the keypad
mode is a request whose whole effect is on keys this terminal will never send
differently, so there is nothing the child was denied that it could observe.

*Rejected:* tracking the mode in the emulator and exposing it in
`TerminalStatus` (dead state, and a field a host would carry across a wire to
nowhere); mapping the keypad onto the existing `Char` keys with a flag on the
side (a keypad `5` and a top-row `5` would still be the same event, so the flag
would decide by guessing); and leaving the diagnostic in place as an
"unimplemented" marker (the ring is a bounded channel to a reader, not a to-do
list).

**D-054 — Mouse tracking is one setting with three levels, and a child hears
only the level it asked for.**
DEC 1000, 1002 and 1003 are alternatives, not switches to be held at once: 1000
reports presses and releases, 1002 adds motion while a button is held, 1003
adds motion with nothing held. The emulator kept one boolean for all three and
`TerminalView` reported motion to every child that had any of them on — that
is, it behaved as 1003 always. A program that asked for 1000 was reading its
input for presses and releases and got a motion report between them, which it
reads as something else entirely; a program that asked for 1002 got the
pointer's whole path across the screen while nothing was held.

`TerminalMouseTracking` (`None`, `Buttons`, `ButtonMotion`, `AnyMotion`) is now
the emulator's state and a `TerminalStatus`/`TerminalSnapshot` field beside the
encoding. Setting any of the three selects that level — a program sending 1002
means "instead of", not "as well as" — and resetting any of them ends tracking,
because a program putting the terminal back sends several of these on its way
out and half-tracking after the first would be nobody's intent.
`mouse_reporting_enabled` stays, deriving from `mouse_tracking != None`: it is
the question most hosts have, and a host that only forwards bytes is unchanged
by this. `TerminalSubsession` is a seam other projects implement, so a status
that reports tracking on while naming no level is read as the coarsest one —
"on, unspecified" is not "off", and reading it as "off" would silence every
pointer event a mirror over a protocol delivers. The view drops a motion the
level does not cover and does not claim the event, so it stays available to
whatever else wants it. A drag is told from
a hover by the button the event carries, which is what the input model already
records.

Note that this is the *inner* terminal, the one a child runs inside. Which mode
ckVision enters on its own *host* is D-049, and the answer there is 1003 for a
reason that does not apply here: ckVision needs the pointer's position to
resolve hover, and a child needs only what it asked for.

*Rejected:* two more booleans beside the first (three flags, eight states, and
five of them meaningless); reporting motion to everyone and letting the child
ignore what it did not ask for (it cannot — an unexpected report is
indistinguishable from a shorter one it was parsing for); and dropping
`mouse_reporting_enabled` in favour of the level alone (a public seam other
projects implement, and the derived answer costs nothing).

**D-055 — A key's release is requested in full, verified by readback, and only
then promised — and a button follows its key the way it follows the pointer.**
The kitty keyboard protocol's event types alone do not report what a button
needs: Enter, Space and every text-producing key keep their legacy encodings —
bytes with no identity a release could be paired with — until the
all-keys-as-escape-codes enhancement is also in force, and once it is, the
associated-text enhancement is what keeps typing correct, because a shifted or
composed character then arrives only in the text field (hosts confirm this:
one was observed suppressing the plain Enter release without the full set and
carrying dead-key compositions in the text field with it). So the POSIX
session, once `CSI ? u` has proven the protocol, pushes the whole set ckVision
can carry — disambiguate, event types, all keys as escape codes, associated
text, never alternate keys (D-047) — and then asks again: the push is a
request, only the readback is an answer, and what the readback says is what
`Capabilities::kitty_keyboard_flags` records and what every promise derives
from. A readback may land after its probe window; a session whose protocol was
verified inside one accepts later flag readbacks for its lifetime (the query
is only ever solicited by this side, and decode never trusts the record — it
follows the shape of what arrives; the flags only set what a key *promises*).
A host honouring all-keys-as-escape-codes without associated text would force
layout reconstruction, so the session steps its own stack entry — never a
pre-existing one — back to the baseline and re-reads. An embedder that stated
an explicit flag contract keeps exactly that contract.

`KeyEvent::reports_release` is the per-event form of the verified promise:
true exactly for events that arrived escape-coded on a session whose verified
set includes event types. On that promise the button becomes symmetric with
the pointer: press arms and depresses, repeat is the same press being held,
release is what commits — and Tab away, Escape, or the terminal losing focus
takes the press back, exactly as dragging off the button does. Escape's
cancel consumes the Escape: taking back a press must not also close the
dialog. Without the promise the press fires immediately behind a brief
depressed flash, and auto-repeat re-fires, because a legacy repeat *is* a
fresh keystroke. Release events keep their dedicated `on_key_release` route
and can never execute a command binding; the button and the terminal view opt
into that route explicitly.

*Rejected:* trusting the push without the readback (reports a promise the
terminal does not keep — the reason D-047 exists); a per-terminal flag
instead of the per-event one (one session mixes encodings, and the earlier
per-terminal reading held a button down forever waiting on a release its
legacy-coded key could never send); synthesizing shifted text from key
numbers when a host omits the text field (layout modelling, refused
throughout); and inferring the release promise from an event's own
subparameters (presses omit the event type by design, so the inference could
never arm on a real press).

**D-060 — An animation describes a change that has already happened.** ckVision
had no animation machinery at all, so U4-k decided what animation IS here
before it drew anything. `ui::Animation` is a duration, a progress callback and
a terminal callback, driven by the injected `Clock` — and, deliberately, it
never owns the end state. The caller applies the end state FIRST and then
optionally animates; the frames are a decoration over a state that has already
arrived. That is the opposite of how a retained-mode toolkit usually animates,
where the animation drives the property and the end state comes with the last
frame, and it is chosen because of which failure each design makes possible. A
text user interface is not a game loop: frames are delivered by a terminal that
may be slow, over a link that may be a network, to a reader who may press a key
at any moment — so an effect here must be skippable, interruptible, and
survivable by a host that never gets a timer tick at all. Under the usual
design each of those is a path along which the end state can be missed, and
every effect has to get all three right. Under this one there is no half-
applied state for an interruption to resolve: the desktop is already correct
before the first frame could be drawn, and the whole of "interrupt" is "stop
drawing". `Window::set_minimized` already had the right shape for this — it
applies `minimized_` and `set_visible` and notifies afterwards — so the flight
starts from an observer that runs on a window whose state is settled.
Three rules make the mechanism safe to build on. **Progress is read from the
clock, never counted per tick**: a host delivering half the frames sees the
same run in the same wall time covering more ground per frame, where a fixed
step per tick would turn a slow terminal into slow motion and leave the effect
outliving the thing it described. **`on_finished` runs exactly once for every
ending** — completed, cut short, or a duration of zero — so an effect has one
place to tear its decoration down and cannot leak one along a path its author
forgot; destruction is the sole exception and is silent, because an owner being
destroyed is already tearing down. **A zero duration is a run that is already
over**, which is the whole of "animations off": no call site carries a branch
for it. The minimize flight is the first consumer. Its destination is a host
provider (`Desktop::set_minimize_target_provider`) because a `Desktop` does not
know what lists its windows — a taskbar, a window-list dialog and an
application with no listing at all answer differently, and one of the answers
is `nullopt`. **No provider, no flight**, so every application written before
this package is unchanged by it; and with the effect off the provider is not
even called, so "off" costs not even the question. The decoration is a popup
like any other — topmost, its own compositor layer, added when a flight starts
and removed when it ends — that answers no input and takes no focus, since the
desktop beneath it is already in its end state and a click it swallowed would
be one stolen from a window the reader can see. *Rejected:* animating
`Window::bounds()` itself (the window would pass through sizes no layout asked
for, every arrangement query would see them, and an interrupted flight would
leave a real window at an invented size); a general animated-property system
over View (a scene graph of tweens is the game-loop design this file's first
paragraph rejects, and nothing in the toolkit has asked for more than one
effect); driving frames from a frame counter (see the clock rule above);
`on_finished` NOT running when a run is cut short (it is the teardown, and the
one ending most likely to be forgotten is the one that then leaks a
decoration); making the flight animate the switcher-bar row rather than fly to
it (`PagedStrip` resolves clicks by index, so relaying the row 30 times a
second under a reader's pointer is the press-in-flight hazard of D-057 at frame
rate).

**D-061 — A terminal's exit policy is named by its caller, never defaulted.**
`TerminalExitPolicy` carries `Unspecified` as its first enumerator, and a
`TerminalLaunchSpec` that still holds it is refused by `launch()` before
native child creation, as a Failed session with a diagnostic naming both real
choices. There is
no default.

The two policies differ in whether `close()` is BOUNDED, and `close()` runs
from the destructor. `WaitForExit` requests graceful termination and waits for
the child without escalating — SIGHUP/SIGTERM on POSIX, Control-C through
ConPTY on Windows. It is a legitimate choice for a child whose output must
never be lost, and a poor thing to acquire by omission, because a child that
ignores the request blocks the destructor for as long as it likes.
An interactive shell ignores SIGTERM by design. This library's own
`bash -i` test hung in roughly one run in three for exactly that reason, having
never said which policy it wanted; four sessions read the 60-second timeout as
one of the test's waits rather than as its destructor, and the helper that
would have refuted that reading — bounded at five seconds — went unopened for
hours.

Rejected: flipping the default to the bounded policy (it makes the accident
quieter rather than absent, and silently changes what every existing consumer's
`close()` does); leaving the default and documenting it harder (the enumeration
was already documented, and documentation cannot make an omission visible at
the call site); requiring the policy as a parameter of
`TerminalLaunchSpec::program()` (it forces the choice at exactly one
construction path while aggregate initialisation still reaches the field, so
the guarantee would hold only where it was least needed).

The cost is a breaking change for any consumer outside this repository and
ckmux: such a caller now fails at launch with a clear message instead of
silently taking the unbounded policy. That is the right failure — the
alternative is a program that hangs on exit for reasons its author cannot see —
but it is a real break, and no gate in either repository can detect one.

## Open

**D-OPEN-1 — Resolved by the owner, 2026-08-20: MIT, for ckVision and ckmux
jointly (ckmux WP-26).** The standard MIT license, deliberately not MIT-0
and deliberately not Apache-2.0: attribution stays required, and what
Apache-2.0 would have added in writing — contribution licensing and a
patent grant — is recovered in `CONTRIBUTING.md` instead (DCO sign-off; an
explicit patent grant from contributors; and a relicense grant to the
owner, so a future commercial dual-license of the library needs no hunt
for past contributors). What decided it over Apache-2.0: MIT is compatible
with everything including GPLv2-only, and it keeps the BSD base-system
door open — OpenBSD imports no Apache-2.0 code into base, and tmux is IN
OpenBSD base precisely because it is ISC; a permissively licensed
multiplexer keeps that story available. Sole copyright rests with the
owner, whose own commercial applications may therefore use both projects,
with any accepted contributions, without restriction. Mechanics: `LICENSE`
at each repo root is the authoritative text; every code file keeps its
copyright line verbatim and gains `SPDX-License-Identifier: MIT` beneath
it (swept by ckmux's `tools/add_spdx.py`); the "All rights reserved"
phrase in the existing lines is vestigial alongside the MIT grant and is
retained rather than churning every header a second time.

**D-OPEN-2 — Resolved (superseded by D-017).**
Graphics are mainline 0.1 scope, first-class in every layer, not an
optional module.

**D-OPEN-3 — Resolved: IME support is cursor honesty.**
Mainstream terminals expose no preedit/composition segments to
applications; the emulator draws composition UI at the reported cursor
position and delivers only composed text. The library's obligations are
therefore: always-accurate cursor position/visibility (ARCHITECTURE §3)
and composed text as atomic text events. Inline composition preview is
parked until a real protocol exists.

**D-OPEN-4 — C++ modules adoption timing.** Revisit post-1.0.

**D-OPEN-5 — Package-registry distribution (vcpkg, conan).** Post-0.1.

**D-OPEN-6 — Bare Linux console mouse support.**
Mouse on the framebuffer console traditionally requires linking gpm,
which D-004's zero-dependency policy rules out regardless of license —
and while D-OPEN-1 is open, avoiding copyleft link dependencies also
keeps every licensing option available. The Linux console profile ships
keyboard-only unless a clean dependency-free alternative appears.

**D-OPEN-7 — Emerging width/text protocols.**
Track mode 2027 (grapheme clustering), terminal Unicode-version
reporting, and kitty's text-sizing protocol; adopt as probes/capability
refinements under D-019 when real-world support justifies it.

**D-056 — Minimizing withholds a window's presentation, not its geometry.**
A minimized `Window` is hidden (`set_visible(false)`) but stays in its
`Desktop`'s `windows()`: being listed is what a hidden window has left, and it
is how a reader gets it back. Its bounds, its zoom state and its position in the
z-order are untouched for as long as it is hidden, so restoring replays nothing
— a window minimized while maximized comes back maximized because it never
stopped being maximized. Every operation that *arranges* or *cycles* windows
asks one shared question (`Desktop::shown`) and steps over the hidden ones: the
three tilings and the grid divide by the count of shown windows, `cascade` gives
them no step, `activate_next`/`activate_previous` walk past them, and
`filled_tile_fractions()` does not count them. Activation is the single
exception and restores instead of skipping — naming a window is asking for it —
which keeps the invariant "no activation points at a window nobody can see" in
one place; a desktop whose windows are all minimized has no active window at
all, which is the honest answer rather than an incoherent one. The desktop's own
sizing policy still reaches a hidden window: it is re-clamped and, if maximized,
kept filling. The width gate for the minimize control IS the title budget rather
than a second number beside it — the control appears at the first width whose
budget still leaves the title four columns, so there is no band in which the
frame claims room for a control while the caption has room for no name at all.
*Rejected:* recording the pre-minimize rectangle, stack index and zoom flag for
replay (a second source of truth that a desktop resize or a neighbour closing
silently makes wrong, and a window that comes back in the wrong place is
precisely the failure a reader sees); a private "hidden" flag separate from
visibility (hit-testing, painting and focus traversal already agree on
`visible()`); freezing a hidden window's geometry (it returns off the edge of a
desktop that shrank while it was away).

**D-057 — A paged item strip, and paging as a widget rather than a feature.**
A one-row strip of variable-width items that does not fit is paged, not
squeezed. The behaviour lives in `widgets::PagedStrip` and not in any one bar:
`WindowSwitcherBar` derives from it as its first consumer, exactly as its own
first application was ckmux. `PagedStrip` is provider-driven — an item source
answers each item's *content* width and text, and the strip never measures the
text, so an item carrying a leading icon is a wider item and not a special case.
Items take their natural width and overflow to the next page. Elision survives
in exactly one case: an item wider than the whole item area, alone on its page,
elided to it. An item box too narrow for its padding spends every cell on text.
Once there is more than one page the chrome band — collapse toggle, previous
control, page index, next control — is reserved identically on every page, the
previous cell drawn blank on the first. Page composition is therefore a pure
function of the width and the item widths, and paging forward and back returns
the reader to the set they came from. The index is sized for the page count's
digits, settled at a fixed point, and is not drawn at one page. Where the row
cannot carry all the chrome it is given up in order: index, then collapse
toggle, then the controls. Pages are recomputed on every change to the item set,
the width, or the toggle. A current page that no longer exists falls back to the
last one that does; a page that survives is kept, so removing an item from an
earlier page does not move the reader. The strip collapses nothing: `▼`/`▲`
report through `on_collapse_changed` and `collapsed()`, and what collapsing
means is the host's other chrome. The toggle is off until a host asks for it.
Glyphs: `▼` U+25BC, `▲` U+25B2, `◁` U+25C1, `▷` U+25B7 — all East Asian
Ambiguous, one column under D-019, and none `Extended_Pictographic`. The black
triangles U+25C0/U+25B6 *are* `Extended_Pictographic` and are deliberately not
used: a variation selector would make them two cells and shear the row.

**D-058 — A resize re-lays the arrangement a reader stated, from the original.**
A tiling is a statement about proportions, so a `Desktop` whose content area
changes size re-divides the arrangement by those proportions instead of clamping
each window on its own. Clamping independently cannot preserve a partition even
on the way down — it pulls a band up without shortening it, so two neighbours end
up sharing a row — and because it only ever shrinks, growing back never repairs
it: one row off the terminal used to cost a filled tiling its
`filled_tile_fractions()` verdict, its shadow suppression and every host's
proportional-restore tag, permanently. So `Desktop` remembers the cells as stated
and the content area they were stated in, and maps from that ORIGINAL on every
pass, never from the previous pass's output. That is not an optimization:
rounding to nearest is not an involution, so re-deriving the reference at each
intermediate size walks a seam a row at a time and never walks it back, whereas
returning to the original area is the identity by construction. Edges are mapped
rather than rectangles, so a seam shared by two neighbours is one coordinate and
the partition stays exact. The arrangement is captured at the end of every
command that lays windows out — `tile`, `tile_horizontally`, `tile_vertically`,
`tile_grid`, `cascade`, `restore` — and when a reader's own move/resize gesture
ends having left a tiling, the gesture bracket being the one signal that a bounds
change came from the reader rather than from a host's layout code. It is
validated against the cells the LAST mapping wrote and not against the cells
originally stated, so a reader's own drag is never snapped back. It is forgotten
when one of its own windows leaves the desktop (which is also what stops a cell
naming a window that no longer exists), and when a mapped cell would be empty —
the floor is a content extent below the number of bands across it, not twice that
— at which point the ordinary clamp resumes and the arrangement is honestly lost.
Membership is settled at capture and frozen: a window that is later minimized
keeps its cell, because it left the screen and not the tiling, and it comes back
into the cell it left at whatever size that cell is by then. A window merely
opening over the arrangement is not one of its cells and does not disturb it; the
verdict, which is a question about what is on the desktop now rather than about
what was arranged, does go while such a window covers part of the grid. Cells are
written with raw `set_bounds`, exactly as the tile commands write bands, and a
reflowed window skips `reposition_within` for that pass: a band is the
arrangement's answer, and letting one window's own minimum push it back over its
neighbour would reopen the overlap this exists to prevent. A window that is
zoomed, or that carries a `DesktopGrowPolicy` other than `None`, vetoes the
capture outright — it is already sized by `refresh_zoom_area`/`fill` on every
resize, and this is not a corner case, because a single window covering the area
is a one-cell partition and an ordinary maximized window would otherwise be
captured here. The partition test itself lives in one `Rect`-parameterised helper
that `filled_tile_fractions()` is expressed in terms of, so the verdict and the
replay cannot become two opinions about one desktop.
*Rejected:* re-capturing the reference on every pass (24 → 23 → 24 then hands two
neighbours each other's heights and keeps them); validating against the stated
cells rather than the last mapped ones (it snaps a reader's own drag back into
the grid); forgetting the arrangement whenever any window is added (a reader
would lose their tiling to opening a message box, including the two dialogs a
`Desktop` presents itself); a flag set by the tile commands (it would still
report "tiled" after a drag and would never report a reader who tiled by hand —
the same reason `filled_tile_fractions()` measures rather than records); routing
cells through `Window::clamp_size` (one window's minimum reopens the overlap);
re-normalising the surviving cells when a member leaves (a hole no proportion
describes, and an arrangement nobody stated).

**D-059 — A switcher-bar row says which of three states its window is in, and
the click does what the icon promises.** A taskbar entry is not a label with an
action bolted on: the icon and the click are one statement, so both are decided
in `WindowSwitcherBar` rather than left to each host. A row draws one of three
glyphs a space left of the title — active, visible-but-behind, minimized — and a
left click on it minimizes the active window, activates and raises one that is
merely behind, and brings back one that was put away. The third and second are
the same call: `Desktop::activate` restores a hidden window on its way to the
front (D-056), so naming a window is asking for it however it left. The host
supplies the two VERBS and keeps its own side effects — ckmux moves the keyboard
with the window and re-reads its footer — while the library keeps the policy;
that is why `set_minimize_action` joins `set_activate_action` instead of the bar
testing a state inside one action. A window whose `minimizable()` is false is
never minimized from its row, because its own frame draws no `_` control and a
taskbar that hid it anyway would be a second route past a gate the window set.
Status is derived from two separately sourced answers — activity from the active
provider, hiddenness from a new minimized provider defaulting to
`Window::minimized()` — and minimized WINS where a host's model answers both: a
`Desktop` never produces that pair, but a host driving the bar from a model of
its own can, and a row drawn as the reader's current window while that window is
nowhere on screen is the one lie a taskbar must not tell. The glyphs are
`▮`/`▯`/`▄` (U+25AE, U+25AF, U+2584), each checked against `ckv::text` before
being chosen: none is East Asian Wide or Extended_Pictographic, so no host
widens them. `■`/`□` (U+25A0/U+25A1), which the request was first written with,
are East Asian **Ambiguous** — one column here by D-019 and two on a host
rendering a CJK font, which would shear every entry on the row. All three are
one cell, so a window changing state does not change its button's length and
minimize/restore cannot re-flow the row the damped widths exist to hold still.
(The glyph table is amended by D-063, which replaces the three shapes with the
frame's own two controls and moves the active/behind distinction into colour;
the one-cell property and everything else in this entry stand.)
The icon is part of the item's TEXT and paid for in `natural_width`, the one
place an entry's content is measured, so the paging and the damping follow it
without being told; it sits at the FRONT, so the lone-item elision takes the
tail of the name and leaves the state legible. The verb also exists as a
standard command, `ckv.window.minimize`, with a `Desktop` default handler
beside `kZoom`'s: a menu and a key are how a reader reaches the window they are
already IN, whose own `_` control is the one they would have to aim at.
*Rejected:* one `bool minimized` beside `bool active` with no resolution (two
flags spell a fourth state that means nothing, and every reader of the pair
would have to settle the precedence again); a single click action with a state
test inside it (every host that wanted its own focus handling would re-derive
the three transitions, and one of them would get it wrong); teaching
`PagedStrip` about icons (it pages words of a given width — what those words
say is its consumer's business, and D-057 put the strip below the bar precisely
so the bar could keep what is window-specific); reading `Window::minimized()`
directly with no provider (the label and the active window are both host-
replaceable, and a host whose windows are put away by its own model — a
detached session, a document closed but listed — would have had no seam);
drawing the icon only for minimized entries (it would say "put away" and
nothing else, and the active/behind distinction would rest on colour alone,
which a monochrome theme does not have).

**D-062 — A private marker is read before the final byte decides anything, and
modifyOtherKeys is declined out loud.**
A CSI whose parameters begin with `<`, `=`, `>` or `?` — ECMA-48's reserved
parameter bytes 03/12 to 03/15 — is a different control from the public one
that shares its final byte: DECSET is not SM, XTSMGRAPHICS is not SU, the kitty
keyboard forms are not SCORC, XTMODKEYS is not SGR, XTSAVE/XTRESTORE are not
SCOSC/DECSTBM. `TerminalEmulator::handle_csi` reads the marker FIRST and hands
a marked sequence to `handle_private_csi`, where every marked form this
terminal honours lives, dispatched by final byte; a marked form it does not
implement is refused with a diagnostic there. No public handler is reachable
with a marker any more.

The defect this replaces was a class, found through one instance. Each handler
in the old dispatch decided for itself whether to look for a marker, and the
ones that did not were reached by whatever marked sequence happened to share
their final byte: `CSI > 4 m` — what vim, neovim and Claude Code write on their
way out to reset modifyOtherKeys — reached the SGR parser, which rejected the
`>` as a malformed colour and left "malformed child SGR sequence" at the bottom
of every ckmux window a Claude Code session had exited from; `CSI ? 25 r`
(XTRESTORE) would have set a scrolling region at row 25; `CSI > 2 T`
(XTRMTITLE) would have scrolled the screen; `CSI ? 6 n` (DECXCPR) was answered
as CPR, without the marker a program that asked that way reads for. Guarding
each handler in turn would have fixed the instances and kept the class; reading
the marker first is the grammar ECMA-48 gives, and it makes the next such
sequence a diagnostic rather than a fresh mystery.

modifyOtherKeys itself — `CSI > 4 ; 1 m` / `CSI > 4 ; 2 m`, the xterm side of
what the kitty flags do, under which an ordinary key held with a modifier
travels as `CSI 27 ; mod ; code ~` — is not implemented, and the decision is
how to say so. The set and reset forms are consumed without effect and without
a diagnostic, by D-053's reasoning: vim, neovim, emacs, tmux and Claude Code
all send them on the way in and out, so a diagnostic would fill the bounded
ring with the one thing most full-screen programs do. Unlike the keypad, the
setting can be asked about, and `CSI ? 4 m` (XTQMODKEYS) is answered — in the
XTMODKEYS set-form, as xterm answers it, so the reply restores the state — and
answered truthfully: `0`, the level really in force, including right after a
program has set 2. A program told 2 would stop expecting the legacy encoding
it is about to receive; a program told 0 keeps its fallback. The cursor and
function keys already travel in the form xterm's default level 2 produces
(`CSI 1 ; mod A`, `CSI 1 ; mod P`, `CSI 15 ; mod ~`), so those two resources
answer `2`; modifyKeyboard and modifyKeypadKeys answer `0`; a resource this
terminal has no value for is answered the way xterm answers for one it lacks,
named and with the value left empty. XTFMTKEYS (`CSI > Pp ; Pv f`), the
formats behind those resources, is consumed exactly as XTMODKEYS is — none of
the keys it formats is sent here.

What real support would take is recorded in `plans/term_capabilities.md`'s
gaps: a level per screen beside the kitty flags in the emulator's
`KeyboardState` and in `TerminalStatus`, the view's `encode_key` producing the
`CSI 27` form for Ctrl- and Alt-held keys and for modified Enter, Tab, Escape
and Backspace (Shift+Tab staying `CSI Z`, a shifted printable staying text),
level 1 sparing the keys whose legacy byte is well known, the query answering
the level in force, and the kitty encoding taking precedence where both are on.
Until that is worth doing, the honest state is none, and the query says so.

*Rejected:* tracking the level and exposing it in `TerminalStatus` without an
encoder behind it (the query would then report a level the keys do not
follow, which is the one lie the protocol is designed to let a program detect);
an `UnsupportedSequence` diagnostic for each set (the ring would hold every
vim's entry and exit, crowding out complaints a reader needs — and the child
can ask); answering the query with silence (a program that asked waits for a
timeout, and one restoring state from the answer gets nothing); a marker check
added to each public handler the audit found (that is the shape of the defect,
repeated); making XTMODKEYS an `UnsupportedSequence` only for levels 1 and 2
and not for the reset (a distinction nothing could read, since the query tells
the truth either way).


**D-063 — A taskbar row wears the window frame's own controls, and says which
window the reader is in the way the frame does: in colour.** Amends D-059's
glyph table. `WindowSwitcherBar::status_glyph` now answers `■` (U+25A0) for a
window that is on the desktop — `Active` and `Visible` alike — and `_`
(U+005F) for one that has been put away: the square the frame draws in its
close control, and the line it draws in its minimize control. The mark says
WHERE the window is, which is the one thing a taskbar knows that the desktop
cannot show. Which window the reader is IN is carried the way the frame
carries it: `PagedStrip::Item` gains an `icon_role` and an `icon_width`, and
the leading cells of an item take that role's foreground and attributes while
keeping the row's own background, so the active row draws its mark in
`ckv.window.control` and every other row lets it fall back — precisely what
`Window::draw` does with its controls, for the reason it does it: a desktop
shows one set of live controls, not several competing for the eye. Only the
foreground is borrowed, because a mark that brought its own background would
punch a hole in the highlight it stands on.

D-059 chose three distinct shapes, `▮`/`▯`/`▄` (U+25AE/25AF/2584), and
rejected `■`/`□` as East Asian **Ambiguous**. That rejection does not survive
contact with the rest of the widget: `PagedStrip`'s own steering chrome
(`▼▲◁▷`) is Ambiguous and rests on D-019's one-column resolution, and the
window frame has drawn U+25A0 in its close control since the control existed.
A host resolving ambiguity the other way widens the frame's control and the
bar's mark together — it cannot make the two disagree, and it was already
drawing a broken frame before the bar was asked. What the three shapes cost
was plain on screen: `▄` is a cell-wide, half-cell-tall slab carrying an order
of magnitude more ink than the thin rectangles beside it, so the state a
reader looks at least — a window put away — shouted loudest of the three;
and no mark on the bar said anything the window it stood for said. The
monochrome legibility D-059 bought with a third shape is bought again by the
strip's selected highlight, which sits under the active row already.

*Rejected:* keeping three shapes and replacing only the slab (the lightest
change, and it leaves the bar teaching a vocabulary the frame does not use);
`▬`/`▭`/`_` (better proportioned than what they replace and Neutral by width,
but still an alphabet that exists nowhere else in the library); drawing the
mark bracketed as the frame draws it, `[■]` (two more cells on every entry, in
a row whose whole design problem is width); a `set_control_role` seam on the
bar (the role is resolved by the name `Window` resolves, so a theme that
retints window controls retints the marks with them — a second seam would let
one half of that be retinted without the other).

**D-064 — A window put away is still on the desktop, as its own top frame.**
Amends D-056, which said a minimized `Window` is hidden and stays in
`windows()` because "being listed is what a hidden window has left, and it is
how a reader gets it back". That was right about the mechanism and silent
about the default: `Desktop` listed nothing itself, so every application that
had not built a listing offered a `_` control that led nowhere. Nine of the
nine shipped examples were in exactly that state — `ckvision_editor`'s only
window could be minimized and then only be got back by quitting, since the
example installs no `WindowSwitcherBar` and `ckv.window.list` is declared with
no key. A control whose whole meaning is "you can get this back" must not be
the one control that cannot be undone.

So a `Desktop` now draws the way back itself unless something else already
does. `MinimizedWindowPlacement` names the three answers and `Parked` is the
default: a minimized window leaves a `MinimizedWindowStub` — one row,
`┌─[■] Editor — config.yaml [↑]─┐` — on the desktop's bottom edge. `HostListed`
is D-056's behaviour unchanged, for a host with its own listing (ckmux sets
it, and its switcher bar would otherwise be a second row saying the same
thing). `Disabled` takes the control off every window, ignores the command,
restores anything already parked, and puts back a window an application
minimizes directly — "not reachable" that covered only the reader's routes
would leave the same trap one level down.

The stub is drawn rather than iconified because the window's own frame is a
vocabulary the reader has already been taught: the corners, the rule, the
bracketed `[■]` of the close control and the `[↑]` of the zoom control are
`Window::draw`'s, and the one control that is missing is the `_`, because it
has already happened. `[↑]` and not `[↕]`: the pair means "maximize" and
"restore from maximized", a question about SIZE, and a parked window's size is
not what this control changes. It draws in the window's INACTIVE roles, since
there is no state in which a stub is the active window — activating one
restores it, and the stub is gone before the activation lands.

It is a popup, not an ordinary child, and that is the load-bearing choice: a
stub composed into the base surface would be under the windows, and a parked
window a maximized neighbour could cover is back where it started. It is
therefore in `popups()`, which is why the flight tests set `HostListed` — they
read that list to ask whether a decoration is on screen. It is reached by Tab
and answers Enter, because a window recoverable only by pointer is only half
recovered on a keyboard-first desktop.

Nothing about the window changes. D-056's geometry rule is what makes the
stub possible rather than something it has to work around: the window keeps
its bounds, its zoom state and its z-order while it is away, the stub owns no
part of it, and restoring deletes the stub and makes the window visible again
— which is why a window minimized while maximized still comes back maximized.
Restoring goes through `Desktop::activate`, not `set_minimized(false)`, so
every route back agrees on what naming a window means; closing goes through
`Window::close`, so a parked editor with unsaved changes still gets to ask.

*Rejected:* leaving it to each application and fixing only the examples (the
same trap for every application written next, and the examples are meant to
show what the library does, not to paper over it); docking a
`WindowSwitcherBar` in each example (a permanent row in every one of them,
listing every window rather than the put-away ones, and it re-blesses most of
the golden dumps and every docs screenshot to fix a case that arises when a
window is minimized); reserving a row in `content_area()` for the parking
strip (every minimize would re-flow the maximized windows, and D-056's "its
geometry is untouched" is the promise that buys the whole feature);
`Window::set_minimized` refusing when `minimizable()` is false (that flag
gates the CONTROL — `present_modal` clears it on a window that is perfectly
capable of being hidden — and overloading it would make a modal's gate mean
two things); a blanket `set_minimizable(true)` when leaving `Disabled` (it
hands the control to modals and to windows an application fixed itself, so
the gate records who it took the control FROM).

**D-065 — Focus is a question a view asks its Application, and an arrow a
control has no use for walks the window it sits in.** Two rules with one
cause. Every widget used to keep a `has_focus_` flag, set from `on_focus()`
and read when it painted. The Application, though, cannot deliver a focus-out
to a view that leaves the tree while focused — the detach sink must not call
into a view that may be mid-destruction — so a window removed from the
desktop and put back, or a popup dismissed and re-shown, came back with its
flag still up, drawing itself focused beside the view that really was. The
menu bar's trailing clock stayed lit the same way after the calendar it
dropped took the focus. A flag is a second truth, and every second truth
eventually disagrees with the first.

So the truth is asked for: `View::has_focus()` is `Application::focused() ==
this`, and no widget keeps a flag. `on_focus()` remains a notification, for
what a view does on the way in or out — repaint, cancel a press in flight,
send a focus report to a child terminal — and it is delivered after the
Application has moved its pointer, so that `has_focus()` inside
`on_focus(false)` already answers no: a view deciding what to leave on screen
gets the settled answer. A view whose context names no Application is never
focused; a test that wants a widget focused attaches it to one and asks the
Application, as an application does. `MenuBar` follows the same rule for its
walk: `active()` is having the focus, a focus loss of any origin ends the walk
and takes the trailing title's highlight and the bar's bookmark with it, and
activating the trailing title — Enter, Space, or a pointer on it — ends the
walk first, so what the title opens saves and later restores the reader's own
view rather than a bar that would wake up highlighted.

The second rule is the reader's: in a dialog, an arrow reaches the next
control, and `Right` then `Enter` on a Yes/No/Cancel row answers No — the
convention of every classic desktop dialog, observed rather than derived.
`Window::on_key` therefore walks the window's own controls for a plain arrow
that reached it — Down and Right to the next, Up and Left to the previous,
wrapping within the window, through `Application::focus_next_within()` and
`focus_previous_within()`. A control that means something by an arrow — an
input line by Left, a list by Down, a radio group by Up — has consumed it
before it reaches the window, so only arrows that would otherwise do nothing
move the focus; a modified arrow is left alone.

*Rejected:* binding the arrows to the standard `focus_next`/`focus_previous`
commands in the keymap — it would apply outside windows, and collide with an
application that gives the arrows a meaning of its own, such as a slide
show's next and previous. *Rejected:* delivering the focus-out from the
detach sink — the sink's one contract is that it never calls into the view.
*Rejected:* a flag kept in `View` and set by the Application — a truth with a
setter is a truth anyone can set, and the notification order problem stays.

**D-066 — A text field the keyboard reaches offers its text selected.**
A field that opens with a value — a web address's "https://", a page's current
name, the search remembered from last time — is most often retyped, and
sometimes continued. The classic desktop dialog convention, observed on a
running classic-dialog application rather than derived from any
implementation, serves both with one key: arriving at the field selects its
text with the caret at the end, so typing replaces it, Backspace or Delete
clears it, and a single caret key keeps it — Right or End continue after it,
Left steps one character back from its end, Home and a word-left go to its
start — while Insert switches the typing mode and keeps it too. Leaving the
field and coming back selects it again. A pointer press is different: the
reader has pointed at a place in the text, and the caret goes there with
nothing selected.

So `InputLine::on_focus` selects the whole text, anchor at the start and caret
at the end, whenever the focus arrives and the field holds text, and
`InputLine::on_mouse` takes the focus itself before it places the caret, so a
press is never followed by that selection. Insert clears a selection. A masked
field has fixed positions to type into and is left as it is. An editable
ComboBox's field follows, as it receives the same focus notification.

*Rejected:* selecting only when a dialog opens — the same field reached with
Tab, or returned to after a message box, would behave differently for no
reason the reader can see. *Rejected:* a per-field option — the convention is
what a reader of this kind of application expects everywhere, and an option
is a second behaviour every application would have to choose between.

**D-067 — A cell grid paints what its provider decides, cursor included.**
`widgets::CellGrid` is the surface a worksheet, a query result or a matrix is
shown on: cells under a column header and beside a row gutter, a cursor, a
rectangular selection, frozen leading bands and merged spans. Its provider,
`CellGridModel`, departs from D-043 in one respect: it OWNS the cursor, the
selection and the scroll origin, and the widget asks it to move them —
`navigate(GridMove, extend)`, `place_cursor`, `scroll_by` — rather than keeping
a cursor of its own and reporting where it went. The reason is what those
three things are in a grid of cells. A list's cursor is a place in a
presentation; a grid's cursor and selection are the operands of the
application's commands — fill this range, sort that one, format the cell
here — and the rules for moving them belong to the application: where a block
of data ends when the reader holds Ctrl, which rows are hidden and so take no
step, how a frozen band scrolls under a page. A widget that kept a second copy
would have to be told the answer to each of those and then reconciled with the
application's own state on every key, and two copies of one truth disagree
eventually — D-065's lesson about focus flags, applied to a cursor. So there
is one copy, the provider's, read whenever the widget paints or hit-tests.
`TerminalView` has the same shape with its emulator, and D-043's own `Table`
already delegates ordering and commits to its provider. What the widget keeps
is presentation: the frame as last laid out, the header and gutter geometry,
the click-to-cell mapping, the overlays. Indices are the provider's own
ordered integers; the frame lists the visible ones, each frozen band first, so
a hidden row is simply absent and a span is resolved over the listed indices.
A cell states only what it changes about the theme's cell style, and the
cursor and selection compose over it: a cell with no colour of its own wears
the role; a coloured cell keeps both colours and swaps them, falling back to
the role when the swapped pair is unreadable by the widget's own integer
luminance estimate — so a document's colouring stays visible under the
highlight, and a theme still decides what a highlight is.
`MaterializedCellGridModel` supplies the generic rules for a grid with no
application behind it, and is what the figure and the tests drive.
*Rejected:* a widget-owned cursor mirrored into the application (the
two-truths reconciliation above); a display-index contract (hidden rows would
occupy display slots, or every provider would renumber its rows on each
frame); scrollbars in this first version (the grids that asked for the widget
are unbounded surfaces, and a bounded consumer can pair it with a Scrollbar
through `scroll_by`); in-cell editing (a worksheet edits in a formula line,
and `Table` already covers the typed-cell case).

**D-068 — An option group's caption is its label, and its choices may run in
columns.** A `CheckGroup` or `RadioGroup` with a caption drew the caption's
'&' literally: the caption was text where every other label in a form is a
label, with a marked letter that Alt reaches. So the caption is parsed as
`Label` parses its text and drawn the same way, and the window's mnemonic
routes (`activate_label_mnemonic`, `activate_control_mnemonic`) treat a group
whose caption marks the letter as they treat a label's buddy: Alt with the
letter, pressed anywhere in the window, gives the group the focus. A choice's
own mnemonic keeps acting while the group has the focus, as before; routing
choices from the window too was not done, because a form's captions and
labels already share one alphabet and its choices would crowd it.

The second half is shape. The groups stacked one choice per row, and a form
that asks the same short question several times over — keep, on, off or
reset, of bold, italic, strike and underline — took twenty rows and a
scrollbar for what every classic dialog fits in four. `set_columns()` lets
the choices flow into columns, row-major, two cells apart and each column as
wide as the widest choice in it — so one row packs its choices, as the classic
dialogs do, and several rows align as a table — and `FieldDescriptor::columns` says the same for
a descriptor's `Radio` field — which, laid out in columns and fitting one
row, is labelled beside that row in the form's label column like a text
field, since the reason a group of several rows captions its choices above
them (a label beside would sit level with the first answer) does not arise;
a stacked list keeps its caption above even when it holds one entry, so a
form does not change shape with the number of entries it offers. A choice owns its row from its marker to the
next column's, or to the edge for the last column, for its highlight and for
a press alike, so a stacked group paints exactly as it did. The arrows follow
the shape: Left and Right step through the choices in order and wrap, Up and
Down move within the cursor's column and wrap there, and an arrow that would
not move the cursor — Up on a single row, any arrow on a lone box — is not
the group's, so it reaches the window and walks its controls, which is D-065's
rule stated for a group. *Rejected:* a horizontal flag (a count says how the
flow looks, and a flag would still need the count for anything but one row);
columns all as wide as the group's widest choice (a row of four short
answers grew to twice its width for one long one, and no longer fit beside
its label on the eighty-column baseline).

**D-069 — With nothing focused, the status line's hint is the root's.** The
hint resolved only through the focused view, so an application whose desktop
was empty — a database manager before its first file is opened — showed no
hint at all where the Turbo Vision convention shows the line that says how to
begin ("F3 opens a database"). The status line now resolves from the focused
view when there is one and from `Application::root()` otherwise: the root is
the one view that stands for the application as a whole, and a key given to
it answers exactly the situation of nothing else answering. A focused view's
own key, or any ancestor's, still outranks it, since the walk from a focused
view reaches the root last. *Rejected:* a separate "default hint" string on
the status line (a second mechanism for what the provider already maps, and
one the F1 route could not share); resolving from the desktop (a dock's
sibling, which the status line would have to be told about).

**D-070 — A list's keyboard search is a prefix, typed together.** Typing a
letter over a list stepped to the next row beginning with it, and only that:
each letter was its own search, so a reader who typed a file's name into a
file dialog's list found the cursor on whatever row began with the name's
LAST letter. Letters typed within a second of each other now form one prefix
(`sa` keeps "sample" past "parts"), the same letter typed again steps to the
next row beginning with it, and a prefix nothing begins with is dropped so
the next letter starts afresh; a letter with a command modifier is a chord,
not a search. The window is timed by the application's clock, never a wall
clock, so a test drives it with a `ManualClock`. A provider answers the
search through the existing `find_prefix`, whose prefix may now be longer
than one letter. *Rejected:* a visible search field in every list (a chooser
is not a filter; the search is momentary and the rows never move).

**D-071 — SQL is one of the standard syntax profiles, and it paints a quoted
name as a name.** An editor over a database's own language had nothing to
lex it with, so every application embedding one either shipped a private
profile or showed plain text. SQL now joins the standard set, detected by a
`.sql` name or by a leading statement word — which outranks YAML's "there is
a colon somewhere", since a statement's bound parameters put one in most of
them. Its two deliberate rules are about where SQL misleads: only `'...'` is
text, and `"..."`, `[...]` and `` `...` `` are quoted NAMES painted as names,
so `"abc"` visibly is not the text `abc` — the mistake an engine's own
permissiveness invites; and a bound parameter (`:name`, `@name`, `$name`,
`?`, `?1`) is a name as well, because it is what a saved statement leaves for
its caller. A word immediately before `(` is a call, which keeps the profile
true for every engine's function set and for the ones an application adds.
*Rejected:* a list of built-in function names (one engine's list, wrong in
the next, and stale by the following release); a dialect flag (the spelling
differences a terminal editor can see are the quoting brackets, and all three
are accepted as names).

**D-072 — ConPTY exit is a drained close, with one owned shutdown call.** A
Windows child can exit while its host still owns the pseudoconsole and its
output pipe remains open. Publishing `Exited` at the process signal would lose
ConPTY's final VT bytes; waiting for pipe EOF without closing the pseudoconsole
can wait forever. `ClosePseudoConsole` may itself write a final frame and block
on releases before Windows 11 24H2, so calling it on the owner thread while
that same thread drains output can deadlock. On observed child exit, the adapter
starts one short-lived, session-owned native thread whose sole operation is
`ClosePseudoConsole`. The owner continues bounded overlapped reads and marks
`Exited` only after the close operation has finished and output reaches EOF.
The thread never touches the emulator, scene, or UI and is joined during native
teardown. The Windows process job still supplies bounded escalation for a
child tree that ignores graceful close. Its grace uses the platform's
monotonic uptime deadline, because counting short sleeps stretched a nominal
one-second grace to nearly fifteen seconds under the named ARM64 VM. This is
a platform transport timing decision, not a wall-clock read in deterministic
core/UI code. This is the sole exception to the
architecture's no-worker rule; ordinary transport and parsing remain on the
owner thread. The behavior follows Microsoft's
[ConPTY close contract](https://learn.microsoft.com/en-us/windows/console/closepseudoconsole).
*Rejected:* treating process exit alone as terminal exit (drops the tail);
waiting for EOF with a live pseudoconsole (can never finish); closing the
output sink before reading it (drops the tail); a persistent reader thread
(duplicates the application readiness loop).

**D-073 — Windows child graphics use an optional app-local modern ConPTY.**
The inbox `CreatePseudoConsole` remains the dependency-free Windows baseline.
On the named ARM64 Windows 11 host its console server (version 10.0.26100.1)
consumes a child Sixel DCS before it reaches the host pipe, even when the
child enables VT output processing. An application may place Microsoft's
published `Microsoft.Windows.Console.ConPTY` runtime files alongside its
executable (`conpty.dll` and the architecture-matched `OpenConsole.exe` in the
package's prescribed subdirectory). The Windows adapter looks only beside
its own executable, loads that DLL with a restricted search path, and uses
its documented create/resize/close exports for one session. The module stays
loaded until the session's close operation has joined. No binary, header,
package manager, or test-only package becomes a mandatory ckVision build or
runtime dependency. If the app-local runtime is absent, the adapter uses the
inbox APIs and changes the **effective** child profile to `sixel=false`;
callers read that profile and must not advertise graphics to the child.
Incomplete or incompatible app-local deployments fail with an actionable
error rather than silently claiming Sixel support. The choice is made at the
platform adapter boundary, with no registry, process-global mutable state, or
core/UI filesystem access. Evidence is the named Windows Sixel probe using
the [Microsoft ConPTY package](https://www.nuget.org/packages/Microsoft.Windows.Console.ConPTY/1.24.260710001)
and the [published ConPTY API](https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session).
*Rejected:* treating an installed Windows Terminal GUI as an update to the
inbox `CreatePseudoConsole` server (it is not); passing child DCS bytes around
ConPTY (cannot preserve console API behavior or the private endpoint); always
advertising Sixel on the inbox path (false capability claim); bundling a
mandatory third-party runtime into the library (violates the zero-dependency
charter).

**D-074 — The modifier, Super and Menu keys are keys in their own right, for a
view that asks.** Under kitty's all-keys-as-escape-codes enhancement a host
reports every modifier key's press and release, both Super keys and the Menu
key as events of their own. The decoder used to consume them, because no
control had a meaning for them. An emulator does: a guest that reads raw scan
codes must see Ctrl go down before Esc and Win arrive with nothing else at
all. So `Key` names them (`LeftShift` … `RightSuper`, `Menu`;
`is_standalone_key()`), the decoder delivers them with their event type and
release promise like every escape-coded key, and `Application` routes them
only to views whose `View::accepts_standalone_keys()` answers true, skipping
every other view on the route and never consulting a command binding. A
chord cannot name one (`KeyChord::parse` refuses), so no binding can wait for
a key it will never receive. A legacy session never produces them, and an
application whose views do not opt in sees exactly the events it saw before.
Lock keys, Hyper, Meta and the media keys stay consumed: nothing yet asks for
them. *Rejected:* delivering them to every view (a text field or menu would
start receiving keys it can only misread, and a bare Alt could begin to act
as an accelerator); a new `TerminalEvent` alternative (every exhaustive
visitor in every embedding breaks for a key that is still a key); a
session-wide decoder switch (the question is which view wants physical keys,
and it moves with focus).
