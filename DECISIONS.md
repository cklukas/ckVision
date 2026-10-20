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
the environment.** (Its `ckv::Error` clause is superseded by D-078.)
Fallible entry points return/report `ckv::Error`;
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
and a copyleft link dependency would also sit badly with the MIT licence
D-OPEN-1 settled on. The Linux console profile ships
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
executable (`conpty.dll` matching the application architecture, plus every
OS-compatible `OpenConsole.exe` in the package's prescribed subdirectories:
x86 applications need x86/x64/ARM64 hosts, x64 applications need x64/ARM64
hosts, and ARM64 applications need the ARM64 host). The Windows adapter looks only beside
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

**D-075 — Windows pixel mouse is accepted on a host that implements it;
Windows Terminal is accepted on its cell-mouse degradation.** M7's exit asked
for Gallery pixel-mouse interaction *in Windows Terminal*. That is a claim
about a host, not about ckVision: SGR-Pixels mode 1016 is an optional outer
capability, recorded per host like every other Windows downgrade (D-020), and
as of 2026-09-26 no released Windows Terminal implements it. Microsoft's
[feature request](https://github.com/microsoft/terminal/issues/18591) sits in
its Backlog milestone and the [implementing pull
request](https://github.com/microsoft/terminal/pull/19949) is unmerged; the
named VM's Windows Terminal 1.24 Stable and 1.25 Preview answer no DECRQM 1016
and send no beyond-grid report. An exit criterion that only a third party's
unreleased feature can satisfy cannot be met by any change to this repository,
so it measures nothing about the library. M7 now requires two things instead,
each on a named Windows host: (1) Gallery pixel-mouse interaction works end to
end in a Windows GUI terminal that implements mode 1016 — verified in WezTerm
on the named ARM64 VM, where the Gallery's Terminal Report shows `Pixel mouse:
yes` and a dispatched event with cell and pixel coordinates; and (2) Windows
Terminal, which lacks the mode, degrades deterministically: the bounded probe
expires, `Capabilities::pixel_mouse` stays false, and input arrives as SGR
cell coordinates — verified live in Stable and Preview, with a real Gallery
click opening its modal over a Sixel image in Stable. Sixel graphics stay
required in Windows Terminal itself, because it implements them. When a
released Windows Terminal answers DECRQM 1016, its pixel route is re-run on
the named host as ordinary WP-37 regression evidence; nothing in the backend
is specific to one host, so no code change is expected. *Rejected:* keeping
the literal criterion (M7 and every milestone gated on it would stay Open for
a reason no ckVision change can remove); accepting a Windows Terminal built
from the unmerged pull request (a development build is not the shipped host
the criterion names, and its behavior can still change before release);
synthesizing pixel coordinates from cell positions (a false capability claim,
and exactly the fabricated precision the bounded capability probe exists to
prevent).

**D-076 — Disabled is a state of a subtree: the Application withholds input
from it, and the control draws it.** `View::set_enabled(false)` used to mean
only "skip me in focus traversal": a pointer press still reached a disabled
button and fired it, and every ordinary control drew exactly as it did when
enabled, so the reader had no way to see what would not respond. Disabled now
means the same thing as hidden does for visibility. `View::enabled_in_tree()`
answers for the view and every ancestor, and a disabled container disables
everything inside it. The Application enforces it at routing: `focusable()`
requires the whole ancestry to be enabled; a pointer event whose hit lands in
a disabled subtree is delivered to the nearest enabled ancestor instead (so a
press on a disabled button still activates and raises its window, and the
button never hears it); a key reaches only views enabled in the tree; a
capture held by a view that becomes disabled is dropped; and the terminal
cursor is not placed for a focused view that is disabled. Widgets draw it
through their own family's role — `ckv.button.disabled`, `ckv.input.disabled`,
`ckv.memo.disabled`, `ckv.option.disabled`, `ckv.list.disabled`, and
`ckv.label.disabled` — set by every built-in scheme. A disabled control shows
no focus, hover, pressed, caret, selection, or mnemonic accent, and keeps its
content and state marks (a checked box stays checked, a list keeps its cursor
row in the muted inactive selection) so the reader still sees the value it
will hold when it comes back. A tab strip wears the menu family and takes
`ckv.menu.dropdown.disabled`'s foreground on its own surfaces. Views that set
no value of the reader's — document surfaces such as the editor, a cell grid,
or a terminal, and dividers, chrome, and indicators — draw their unfocused
state, which is exactly what a view that cannot hold input looks like. Menu
items, status items, and toolbar commands keep their existing command-driven
disabled roles; they are disabled by a command's predicate, not by a view
flag. *Rejected:* checking `enabled()` in each widget's handlers (the existing
partial practice; every new widget has to remember, and a container cannot
disable its children); dropping a pointer press on a disabled control (the
window under it would neither activate nor raise, which is not what a reader
clicking a greyed button expects); one shared disabled foreground composited
over each control's surface (no single colour contrasts with a green button, a
blue field, and a grey dialog at once, and the codebase leaves state colours
to the theme); a Dim attribute alone (terminals render faint text
inconsistently, and a monochrome scheme needs to say more than that).

**D-077 — Graphics tracing and output capture are sinks a host injects, never
settings the library looks up.** The library had two process-wide hooks
switched on by environment variables. `term::graphics_log` read
`CKVISION_GRAPHICS_LOG` on first use, opened and truncated a file, set a
variable of its own so child processes would append, and kept the open stream
in a function-local singleton. `PosixTerminal` read `CKVISION_OUTPUT_CAPTURE`
and copied every byte it wrote into the file that named. Both break D-039,
which admits no service that is a singleton or an environment lookup, and the
first also made the deterministic Presenter and the embedded emulator consult
ambient process state. Tracing now goes to an optional `DiagnosticsSink`, the
existing core contract: `ui::Application::set_graphics_trace()` hands one to
its Presenter, and a `TerminalView`'s session receives it through
`TerminalSubsession::set_graphics_trace()` so its emulator can report. The
sink is not the Application's own buffered diagnostics, because a trace is
read while the program runs, not after it restores the terminal. `term`
provides `FileTraceSink`, a platform adapter that stamps each line with the
process id and a monotonic time and appends or truncates as its owner chooses.
Output capture becomes a callback the host sets on `PosixTerminal`
(`set_output_capture`), which receives every byte the session sends from then
on, and the backend's own summary of what the host supports goes to a trace
set with `PosixTerminal::set_graphics_trace`. The examples decide from their
own environment whether to create either and pass that choice to their
children, which keeps the host-and-child single-file log. The library reads no
environment variable to decide what to do. *Rejected:* routing traces into
`Application::diagnostics()` (they would surface only after exit, interleaved
with every other message); keeping the singleton with a DECISIONS exception
(the trace has no reason to be process-global; it belongs to the session that
produces it); deleting tracing (it answered questions about hosts that nothing
else could).

**D-078 — Environmental failures: a session that cannot go on throws; an
operation that may fail returns its own status; `ckv::Error` is withdrawn.**
D-011 said fallible entry points return or report `ckv::Error`, and
ARCHITECTURE §11 named a small set of them. None was ever written. The library
settled on two mechanisms instead, each suited to its case, and this record
makes them the rule. A backend that cannot establish or continue its terminal
session throws: `std::runtime_error` naming what failed and what to do (a
console that refuses VT mode, handles that are not a terminal), or
`std::system_error` carrying the operating system's code (a read, wait, or
write that the terminal can no longer serve). Both backends do this; the POSIX
one used to drop output silently once the terminal was gone, which left a
program painting into nothing. Restoring the session never throws and is
best-effort to the end, because it runs on the way out of exactly these
failures. Configuration that arrives from outside the program and cannot be
honoured (capability overrides with non-positive metrics, say) throws
`std::invalid_argument`. An operation for which failure is an ordinary outcome
returns a status specific to it (`FileWriteStatus`, an empty `FileReadResult`,
a session in `TerminalSubsessionState::Failed` with its reason in
`diagnostics()`), because callers branch on those outcomes and one
cross-cutting code set would name none of them precisely. Contract violations
remain assertions (D-011). The examples run through
`examples::run_reporting_failure`, which prints the message after the terminal
has been restored; Hello, held to its 60-line budget (ROADMAP M9), leaves the
exception to the runtime, whose termination path restores the terminal as
every fatal exit does. *Rejected:* adopting `ckv::Error` everywhere (every
constructor would need a factory returning a result, for failures a caller
cannot recover from inside the session anyway); keeping the unused type (a
documented contract nothing honours is worse than none).

**D-079 — A child's maximum limits the spare space it is given, and hidden
children take no space.** `SizeHint::max` was declared by many widgets and
read by no container, so a Row or Column grew an Expanding child past the
largest extent it said it could use. Row and Column now share out spare space
among Expanding children by weight, each no further than its max. What one
child cannot take goes to the others, and what none can take stays empty
after the last child rather than stretching anything. A container's own max
stays unbounded, because it leaves unused room empty. Where a container is
told to give a child an extent (Fill cross-axis alignment, a Grid cell, the
Dock centre, an Overlay layer), the child gets that extent: max is the
answer to "how much of the spare may you take", not a cap on a place assigned
to it. A hidden child takes nothing. Row, Column, and Dock lay it out as
absent, so a hidden Dock edge keeps no strip, and a Row or Column's shadow
row is decided by its visible children alone. Grid, Overlay, and AnchorPane
place each child independently, so a hidden one there already takes nothing
from its siblings. Every container forgets a child's layout spec when the
child leaves by any route (`remove_child`, `detach_child`, a view closing
itself), and never records one for a child that detached during its own
attachment. *Rejected:* treating max as a hard cap everywhere (a Label's own
contract is to take its column's width, and many controls declare max equal
to their natural width); ignoring max (a declared contract nothing honours).

**D-080 — A raster region records no fallback state.** `RasterRegion` and
`RasterSlice` carried a `fallback_active` flag, and every golden `raster` line
ended in `fallback active|hidden`. Outside tests the flag was always true, and
the Presenter never read it. Whether a region's pixels cover its fallback cells
is decided per frame by the Presenter from the host's capabilities and the
slice's visibility, not by the scene. A region's cells always hold the
fallback, so there was never a second state to record. The flag, its setter,
and the golden token are removed. The golden grammar's `raster` line ends at
its hash, and the format keeps version 1, because nothing outside this
repository reads it before 1.0. The rendered-graphics capture (D-035) remains
the evidence for what a terminal shows over those cells, so the M2 exit's
"fallback activation via the symbolic dump" now reads "fallback activation
via the paired Sixel and NoGraphics decoded captures". The symbolic dump
still pins where a raster slice sits and what its cells hold. *Rejected:* giving
the flag a meaning, such as suppressing the fallback under a picture. The
fallback is exactly what a terminal that drops the picture must show, so the
scene cannot know when it is safe to omit.

**D-081 — A scrolled picture is a re-anchored raster region; scroll intents are
post-0.1.** ARCHITECTURE §3 said raster regions are "scrolled with their
surface", and that a viewport declares whole-row scroll intents for the
Presenter to turn into terminal scroll operations. Neither was built.
D-041 had already moved scroll-delta optimization after 0.1. A scrolling
widget (`ScrollViewport`, `FlowView`, `TextView`) repaints its visible
content, and a picture inside it is drawn again as a raster region with its
new anchor and clip. The compositor then slices and the Presenter encodes it
like any moved region. That is the 0.1 contract. Its evidence is a symbolic
golden and a decoded pixel-plane golden of a picture scrolled by one row and
by several. The M2 and M3 exit items "raster-region goldens for …
scrolling" and "image demo with correct … scrolling" stand, and are
satisfied this way. Scroll-intent annotations and terminal scroll regions
return only through D-043's promotion rule, with a measured need.
*Rejected:* building scroll intents for 0.1. A terminal scroll region
cannot move a Sixel picture with the text around it, because terminals
disagree on whether pictures scroll, so a raster would be re-sent anyway.

**D-082 — The appearance matrix is the "gallery" of every widget in every
scheme.** M6's exit says "the gallery exercises every widget in every
built-in scheme". The Gallery example is a showcase of about nine widget
types. The appearance matrix renders every public view type in every state
its traits require, under all four schemes, and gates completeness
mechanically. The exit now names the appearance matrix
(`tests/golden/appearance`, `appearance_matrix`) and the widget-gallery
reference page as that evidence. The Gallery example gains a View → Scheme
menu, and a test captures it under all four schemes, so the example a
reader runs also shows the schemes. *Rejected:* growing the Gallery example
to host all 59 elements. An example is teaching material, and one that
holds every widget teaches nothing in particular.

**D-083 — Menu arrows stop on disabled rows.** The widget catalog said
disabled menu items are "skipped by arrows but visible". The menus stop on
them instead, and Enter on one does nothing. A reader can land on a
disabled row, and the status line then shows why it is disabled (D-076).
Skipping it would hide that reason from anyone without a pointer. The
catalog row is amended to match.

**D-084 — A cluster that begins with an emoji modifier is two columns
wide.** An emoji modifier (U+1F3FB…U+1F3FF) has Grapheme_Cluster_Break
Extend, so it folds into a preceding base. With no base, it begins its own
cluster, and the width rule gave that cluster zero columns, so the next
cluster overwrote it. Such a cluster now takes the modifier's own UAX #11
width (East Asian Width W: two columns), and terminals draw the swatch.
It remains a terminal-divergent case under D-019.

**D-085 — Pasted DEL is replaced like the other controls.** ARCHITECTURE
§12 names C0 and C1 controls as what paste sanitization replaces. DEL
(0x7F) is neither, but a line discipline reads it as erase, so pasted text
that kept it could delete what came before it in a child's input. Paste
sanitization now replaces DEL with U+FFFD, as display-text sanitization
already did. §12 is amended to name it.

**D-086 — Alt+1…Alt+9 select a window by number.** The Desktop could
select its n-th window (`Desktop::select_by_number`), but no command or key
reached it. The standard command set gains `ckv.window.select.1`…`9`, and
the default keymap binds them to Alt+1…Alt+9. This is the observed
convention of classic desktops, and no other standard binding or decoder
path uses Alt with a digit. Like every default binding, they can be
rebound at runtime.

**D-087 — Every presentation call names its modality.** D-038 made modality
explicit for arbitrary windows (`Desktop::present_modal`,
`Desktop::present_modeless`). The standard-dialog families were still named
`present_<family>` and `exec_<family>`, modal only by their documentation, so
a call such as `present_message_box(...)` did not state its modality, as the
M9 exit requires. Every presentation function now names it:
`present_modal_<family>` and `exec_modal_<family>`, plus
`present_modeless_<family>` for a family that offers modeless presentation.
The old names are removed; this is pre-1.0, so no aliases are kept.
`example_hygiene` rejects a presentation call whose name does not state
modality. *Rejected:* documenting per family that `present_*` means modal.
The call site still would not say it, and a reader would need the reference
to know.

**D-088 — A cell can be part of a hyperlink; the Presenter emits it as OSC 8
where the host renders hyperlinks.** The catalog promised TextView links
"rendered as OSC 8 hyperlinks where supported", but OSC 8 existed only as a
string a widget built itself, which §3 forbids, and nothing presented it. A
`Cell` now carries a `LinkId`, held in padding its layout already had, so an
unlinked cell costs no memory. The id resolves through a reference-counted
`LinkTable` owned by the grid that holds the cell: a `Surface`, the composed
frame, or the decoded display. Ids are grid-local, and a link moves between
grids by target (`Surface::set_cell(p, cell, target)`). Only a valid terminal
hyperlink enters a table: an absolute RFC 3986 URI of printable ASCII
without spaces, at most 2083 bytes. Any other target is dropped, never
repaired (§12), so an application-internal link never reaches the terminal.
`Capabilities::hyperlinks` is never probed, because OSC 8 has no query. It
is off in every curated profile and is set explicitly or by
`CapabilityOverrides::hyperlinks`. With it, the Presenter opens
`OSC 8 ; id=<FNV-1a-64 of the target> ; URI ST` once per contiguous run of
one target and closes the link at every run end. A link is therefore never
open across a cursor move or a frame end, and every piece of one target is
one hyperlink to the host. Without the capability, links are stripped before
the diff and the output is byte-identical. The golden format gains optional
`link` records and stays version 1 (the D-080 precedent). *Rejected:* a
shared pointer per cell (memory and atomic traffic on every copy); a link on
`Style` (a link is not appearance); sanitizing targets (it sends a different
address than the one named); per-span ids (they need a namespace shared
across windows).

**D-089 — The Menu key is an ordinary key that asks for the context menu
(amends D-074).** D-074 made Menu a standalone key because nothing had a
meaning for it. The context menu does. `Key::Menu` is routed through the
focus chain, falls through to a binding, and can be named in a chord.
`is_keyboard_context_menu_request` accepts it exactly as it accepts
Shift+F10. The decoder maps `CSI 29 ~` (the VT220 Do/F16 position, where
rxvt-unicode and the xterm lineage report Menu) and kitty's functional key
57363 to it. The Windows console documents no sequence for the key. The
modifier and Super keys stay standalone.

**D-090 — A menu bar that does not fit overflows into one title, and Esc
closes one menu level.** A bar draws only titles that fit whole, never
clipped, and ends them with `»`. Its dropdown lists the hidden menus as
submenus. It is one stop on the keyboard walk, and a hidden menu's mnemonic
opens the list with that menu entered. A resize that shows or hides an open
menu's title moves the menu between its title and the list without losing
the reader's place. Esc now closes exactly one level: the submenu, then the
dropdown with the bar still on its title, then the bar itself, which
restores the prior focus. This is what the catalog always said. *Rejected:*
clipping (it hides names and mnemonics); scrolling the bar (the titles
around it would move under the reader).

**D-091 — A theme is saved as the whole truth, and the theme editor returns
a value.** The theme text format (version 1, `ui/theme_format.hpp`) writes
one line for every role in the registry, in byte order of the role names,
with the style the role resolves to, spelled as in the golden format.
`parse_theme` overlays the text on a base theme the caller chooses. A
well-formed line naming a role the registry lacks is skipped and reported,
so themes survive widget sets changing. Anything else malformed is refused,
with its line, before any of it applies. The standard theme editor edits a
copy, previews it through a subtree theme override, and completes with
`ThemeEditorResult{optional<Theme>}`. The application installs and saves the
result itself, through its injected `FileSystem` (D-039). Setting or
clearing a theme override repaints the whole subtree. *Rejected:* saving
only overrides (the text would mean different things over different bases);
refusing unknown roles (a theme saved by a richer build would not load); an
editor that applies live (Cancel would have to undo).

**D-092 — A symbolic raster record describes the slice it shows.** A raster
record's `pixels` and `hash` now describe the part of the picture its slice
shows, not the whole image. A picture scrolled under a fixed viewport
otherwise produced identical records, and the symbolic golden could not show
the scroll that D-081 relies on it to show. A picture shown whole records
exactly what it did before. `ImageView` and `Canvas` report every mouse event
to their host but no longer consume the wheel, so a wheel over a picture
reaches the viewport that scrolls it.

**D-093 — The keyboard moves and sizes a window, and every edge resizes.**
The standard set gains `ckv.window.size_move`, bound to Ctrl+F5. That is the
Size/Move key observed on classic desktops and the Ctrl sibling of F5,
which zooms; no other standard binding uses Ctrl with a function key. It
puts the active window into one mode: arrows move it, Shift+arrows resize it
by its bottom-right corner, Enter keeps the result, and Esc restores the
starting bounds. The window holds the keyboard while the mode lasts. The
command is allowed in modal scope, where it moves the modal window, because
a dialog covering what the reader needs has to be movable without a
pointer. A press on the left, right or bottom edge resizes along that axis
only; the top edge is the title bar and moves the window. Every resize, by
pointer or key, stops each moving edge at the desktop's content area.
*Rejected:* separate move and resize modes; an application-level key grab
(a second routing path).

**D-094 — Frame line sets are a per-window choice, and the desktop
background is an application hook.** `Window::set_frame_lines(FrameLines)`
selects `ByActivation` (the default: double while active, single while
inactive), `Single`, `Double` or `Rounded`. A theme is a table of styles
(D-007), so the line set is not a theme entry. A rounded corner that a line
of the same scope joins becomes the light tee or cross the shape needs,
because Unicode has rounded corners only. `Desktop::set_background_painter`
draws after the fill pattern, on the desktop's own surface, clipped to the
area between the docks. It runs only when that surface repaints; window
traffic is composition and never calls it.

**D-095 — The window list filters and closes.** Typing over the window list
narrows it to the titles containing the typed text, with ASCII letters
matched regardless of case. This is a filter rather than D-070's prefix
search, because the catalog asks this dialog for one; D-070 still governs
lists in general. Esc clears a filter before it dismisses the dialog.
Delete, or the Close Window button, closes the listed window through its
vetoable `close()`, and the list follows the desktop live.

**D-096 — The Application counts clicks, and the wheel steps three rows.**
No backend reports a double click, so a widget that waited for one never
saw it on a real terminal. The Application now counts clicks on its injected
clock and sets `MouseEvent::click_count`. A 2 means the second press of the
same button on the same cell within `ui::kDoubleClickIntervalNanos` (500 ms).
The press after a double click starts a new count, so there is no triple
click. `MouseAction::DoubleClick` is removed. Every widget uses this one
rule: TextEditor and Memo select a word, ListView, CellGrid and TreeView
activate (TreeView used to activate on a second click of the selected
node). A wheel notch scrolls `ui::kWheelRows` (3) rows everywhere, and
List, Tree, Table and Memo now scroll with it. This is not wall-clock
timing: the clock is the injected one (ARCHITECTURE §10).

**D-097 — A wide glyph the left clip edge cuts shows as a blank in its
style.** When a clip's left edge cuts a wide cluster in half, its visible
half becomes a blank in that cluster's style and link, and every glyph after
it keeps its column. Before, such a glyph was skipped, and horizontal
scrolling shifted the rest of the line. A selection or a link therefore
reads continuously across the edge.

**D-098 — The virtual display and the embedded terminal keep the cursor on
the page.** Per ECMA-48 and xterm, cursor positioning is clamped to the
page. The virtual display (the test oracle) follows the autowrap mode a host
starts in: writing the last column leaves a wrap pending, and the next
cluster wraps, scrolling from the bottom row. Erase in display follows
ECMA-48's extents, and xterm's ED 3 (saved lines) leaves the page alone.
The embedded terminal's parser accepts digits only in parameters, so a
signed parameter takes its default, and it saturates oversized numbers. No
cursor arithmetic can overflow. A fuzz campaign found the overflow; its
input is in the corpus.

**D-099 — Desktop edges hold dock stacks, and a command's on/off state
belongs to the registry.** `Desktop::dock(view, DockEdge)` pushes onto an
edge's stack: the first view docked sits against the edge, later ones sit
inward, and removing one closes the stack up. This replaces "a second dock
replaces the first", which had forced tool bars into hand-built columns.
`CommandRegistry::set_checked_predicate` and `checked()` sit beside
enablement, so a menu row's check mark and a tool-bar button's face come
from one answer. The tool bar takes its items as `CommandPresentation`s and
follows the menu bar's focus model: `activate()` saves the focus, choosing a
command hands it back before running it, and a pointer press on an
unfocused bar borrows the focus only while the button is down.

**D-100 — A tab strip keeps its active caption shown, and a button can
repeat while held.** When captions do not fit, the strip scrolls so that the
active caption is always shown. ◂ and ▸ mark the sides that hide captions
and take a column only then; a click on a mark scrolls one caption, and if
that would push the active caption off, the selection moves to the nearest
shown caption. An opt-in repeating button fires on the press, again after an
initial delay, then at an interval. It uses the Application's timers on the
injected clock, and stops on release, when the pointer leaves, or when the
button is disabled.

**D-101 — Help cross-links are runs, and links are stops on the Tab path.**
`HelpTopic::body` is a sequence of `HelpSpan` runs, each plain prose or a
cross-link to a topic key. The library defines no help file format, and a
malformed link cannot be represented. The viewer renders body links and the
see-also list as `TextView` links. Tab past the last link continues focus
traversal instead of cycling, so a page of links cannot trap the keyboard,
and leaving the view resets the walk to the first link.

**D-102 — The command palette is a standard popup (Ctrl+Shift+P).**
`ckv.app.command_palette` opens the palette as a popup with the menus'
dismissal rules: Escape, an outside press, or running a command. It answers
for the invoking view's command contexts and runs the chosen command after
restoring focus. It lists commands that are in scope but disabled, greyed,
through `CommandRegistry::in_scope`. Ctrl+Shift+P is the convention observed
across contemporary editors and terminals. Only the kitty keyboard protocol
distinguishes it from Ctrl+P; legacy terminals decode it as Ctrl+P, so there
it never opens the palette by accident.

**D-103 — Property editing is typed and commit-based, and history is keyed
and owned by the Application.** A property has a `PropertyKind` (text,
integer, real, choice, bool, date, time) and is edited with the matching
existing control. A commit is checked by the kind's rule and then by the
item's own validator. A refused value keeps the editor open and shows its
reason on a row beneath it, and `on_change` reports committed changes only.
Every history-bearing control (InputLine, ComboBox, SearchBox, descriptor
fields, file dialogs) names a history key, and the list lives in
`Application::history()`. No widget takes a registry pointer, and a detached
widget has no history.

**D-104 — The Application reports attention moves, and Ctrl+F1 shows the
focused view's tooltip.** Tooltips must follow focus and hover, which each
view hears only for itself. The Application gains attention observers for
focus changes, hover changes and input (reported before routing). They are
instance-owned, called in registration order, and safe to add or remove
during a notification. `ckv.app.tooltip` (Ctrl+F1) has no library handler; a
`TooltipController` claims it. Like help, it is allowed in modal scope. The
tip it shows is held like a menu, so Esc closes the tip and not the dialog
behind it. Pointer- and focus-triggered tips are passive and vanish on any
input. *Rejected:* polling on a timer (idle wake-ups, against the
performance charter); Shift+F1 (the Forms example binds it).

**D-105 — A typed entry is refused, never clamped; dates follow a stated
policy; a wizard completes typed.** An editable SpinBox or DatePicker keeps a
refused entry as typed. It is drawn in `ckv.input.invalid`, states its
reason, and in a descriptor dialog vetoes accept exactly as a `DialogVeto`
does. `DateFormat` options, or caller format and parse callbacks, define how
a date is shown and read; ISO stays the default, and month names, weekdays
and meridiem come from an explicit `DateTimeLabels` table, never a locale.
`Wizard::on_complete(WizardOutcome)` replaces `on_finish`/`on_cancel`, and
`present_wizard` presents it as a D-038 standard dialog.

**D-106 — A theme states how a shadow darkens; Classic draws the classic
black shadow (amends D-037).** Published screenshots of the classic text-mode
desktops (their IDEs and demo programs) draw a shadowed cell as its own
glyph in dark grey on a black ground. D-037 halved every colour channel
instead, which reads as a darker copy of whatever lies beneath. The binary
union stays: one or many overlapping casters apply exactly one transform.
The transform itself now comes from the theme. A scheme either halves the
colour channels (Dark, Light and Mono keep this) or recolours the shadowed
cell with a fixed foreground and background while keeping its glyph and
attributes. Classic recolours to dark grey on black, as the classic desktops
did. Pixels under a Classic shadow are darkened to match: each pixel's
luminance is mapped into the black-to-dark-grey range, so a picture under a
shadow stays recognisable, as its glyphs would. The Classic scrollbar keeps
its blank page area. Its thumb ends are half-block characters, and a dotted
track would show through their empty halves. *Rejected:* halving everywhere
(the Classic scheme would not look like the desktops it is modelled on); a
black shadow in every scheme (it would be a black hole on the Light scheme).

**D-107 — The keyboard's window is the window drawn active.** Focus and
window activation were independent, so the Gallery could show its focus in
one window while drawing another active, against ARCHITECTURE §5's "one
focused view per application, always visible, always consistent". Two
rules now keep them together. When a view inside a window gains the focus
(Tab, a mnemonic, a click, focus restoration or the application's own
`set_focus`), the Application tells every ancestor through
`View::on_descendant_focused`, and the Desktop activates and raises that
window. When a window is activated without the focus moving into it (F6,
Alt+1…9, a title-bar click, a switcher entry), the Desktop moves the focus
to the view that last held it in that window, which it remembers per
window, or else to the window's first focus stop. Focusing a docked bar or
a popup changes no activation.

**D-108 — Light dismissal ends the menu interaction.** A press outside every
menu of an open chain, or a gesture released outside it, dismisses the menu
with `MenuDismissReason::Outside`. A menu bar then deactivates and restores
the prior focus, as it does after a choice, but runs nothing. The press is
consumed, not delivered to the view beneath the popup. Esc still closes one
level (D-090).

**D-109 — A search box is a composite whose field is an InputLine.** The
catalog asks for "InputLine plus clear/status affordance". `SearchBox` holds
an `InputLine` (`field()`) as its focus stop, so the query has full caret
movement, selection, clipboard, undo and shared history. Escape clears a
query, and passes through when there is nothing to clear; Enter records the
query in the history. Both keep their box-level meaning by bubbling up from
the field.

**D-110 — A saved theme states its shadow (amends D-091 under D-106).** The
theme text's second line names the theme's shadow: `shadow halve` or
`shadow recolor fg <color> bg <color>`. It is read only in that place, so a
role named `shadow` still reads as a role. A text without the line keeps the
base theme's shadow, as it keeps the roles it does not name. The shadow
style is a core value (`ShadowStyle`), because the compositor and the
Presenter both apply it and neither may depend on the other. The theme is
its one source: `Application::set_shadow_spec` is withdrawn, and the offset
is the standard two columns and one row. *Rejected:* a shadow role
(halving is not a style); leaving the shadow unsaved (a Classic theme saved
and loaded over another scheme would lose its shadow).

**D-111 — A refused focus request waits for its modal to end, and outranks
the modal's restoration (refines D-038 under D-107).** A `set_focus` refused
because a modal scope excludes the view is kept until the scope ends. It is
dropped only if the view goes away or can no longer take the focus. When
the scope ends, it replaces the scope's own focus restoration instead of
following it, so a dialog that hands the keyboard elsewhere, such as the
window list's Switch To, does not bounce it back to the window it was opened
from. *Rejected:* applying the restoration first (it activates and raises
the window being left); dropping the request at the next routing point (the
dialog is still up then, so the request was lost).

**D-112 — Every published screenshot is gated by the capture tool that draws
it.** The screenshot manifest groups figures under their capture tool, and
each group becomes one `<name>_visual_capture` test that requires exactly
the group's figures, byte for byte. `check_docs --screenshots` fails on a
published SVG that no group lists. *Rejected:* per-test lists of names; they
drifted, which is how two help screenshots went stale without any test
failing.

**D-113 — A self-hosted CI lane runs on the owner's ck-git-hosting server.**
`.ckgit/ci.yml` builds the strict Release configuration with the host's GCC
and runs the complete CTest suite, with one retry for the `pty` label, on
every push to `main` of the project hosted on the owner's Linux ARM64
ck-git-hosting server. The lane is supplementary: it adds a second GCC and a
second ARM64 Linux host that runs on every push, while the release gates
stay the three-host matrix, the sanitizer lanes and the fuzz lane. Its
sandbox has no network, which ckVision's zero-dependency build does not
need. *Rejected:* making it a release gate (it covers neither macOS nor
Windows, nor the sanitizers); sanitizer or fuzz jobs on that host (it has no
clang for the fuzzers, and the Linux sanitizer lanes already run the suite
under a GCC ASan/UBSan and TSan build).

**D-114 — ckdocs builds the documentation site.** The site on GitHub Pages
and on the self-hosted CI's Pages is built by ckdocs, the documentation-site
generator of the owner's ck-git-hosting project, from `docs/` and
`ckdocs.yml`, with `--strict`, so a broken link or heading fragment fails the
build. Both CI lanes compile ckdocs from its source at a pinned commit; nothing
is downloaded as a binary, and ckVision's own build and test suite do not use
it, so it is a documentation tool, not a dependency of the library. GitHub
Pages publishes only a commit whose `ci` workflow succeeded, which is what
proves its screenshots are byte-identical to what the current code draws
(D-112). The pages' `author` and `date` front matter become each page's
byline; `format`, which only the local ckwrite export reads, is declared to
ckdocs as a foreign key, so an undeclared key still fails the strict build.
*Rejected:* Jekyll (it needed `{% raw %}` guards around every page with
braces and could not check links); publishing on every push (a commit with a
stale figure would reach the site before CI failed it).

**D-115 — The hosted Linux release lane pins GCC 13.** The GitHub `release`
matrix runs Linux on `ubuntu-24.04` with an explicit `g++-13` instead of
`ubuntu-latest`'s image default. The named verification hosts and the
self-hosted lane run GCC 14, whose `-Wdangling-reference` heuristic is
narrower than GCC 13's, and a downstream GCC 13 build failed on a diagnostic
none of the gate compilers produced. Pinning keeps GCC 13 in the zero-warning
gate when the runner image moves on. A GCC 13 diagnostic is fixed in the
code, not suppressed. *Rejected:* disabling `-Wdangling-reference` (it finds
real dangling references too); relying on `ubuntu-latest` (it already moved
from one GCC to the next once and will again).

**D-116 — A report beyond the grid proves pixel mode only while the host can
be sending pixels.** The SGR decoder took any report beyond the cell grid as
proof of mode 1016, because iTerm2 answers DECRQM 1016 "permanently reset" and
engages the mode anyway. But a host in cell mode also reports beyond the grid:
a drag carried outside the window does, and resizing a window by its bottom or
right edge is exactly such a drag. On Terminal.app, which never enters 1016,
one such drag "proved" pixel mode for good, and every later click was divided
by the cell size and landed in the top-left corner (owner report, ckmux 0.1.6
on 0.1.7). The decoder now takes the proof only while
`set_pixel_reports_possible(true)`; the POSIX backend sets that while a probe
window has the mode enabled, once it is proved, and for 250 ms after
`CSI ? 1016 l` resets an unproved mode, for reports the host produced before
it read the reset. Outside those, a beyond-grid report is delivered as the
cell it is. The default stays true, so a backend that does not track the
host's mode (the Windows backend, whose late-report timing D-075 measured
separately) keeps its behaviour. *Rejected:* a magnitude threshold (a pixel
report is not always far beyond the grid, and an overshoot is unbounded on a
host that does not clamp); dropping late proof altogether (reports in flight
at the reset are real pixel data, 42bde73).


**D-117 — TabControl has three presentations over one interaction and layout
contract (refines D-100).** `TabPresentation::Underlined` is the default:
a caption row above a thin baseline, with a heavy segment beneath the selected
caption. `Framed` draws three header rows (cap, caption, page connection),
a single-line page frame, and an opening beneath the selected caption;
the page is inset one column and excludes the bottom frame. `Compact` uses
one row, with color and bold weight distinguishing the selected caption. A presentation may be
changed on an attached control without losing pages, selection or focus;
it relays changed size hints, relays out the active page and settles overflow.
Painting and pointer hit testing consume the same geometry. Focus underlines
the selected label; selection remains marked when focus is in the page.
Captions use complete-grapheme ellipsis when oversized. All presentations
reserve both end columns whenever captions overflow (an unavailable arrow
is blank), so captions do not shift as an arrow appears. The D-100 selection
and scroll-click contract is retained. Tiny bounds clip the presentation;
they never produce negative page extents. Dedicated `ckv.tab.*` theme roles
separate tabs from menus. Geometry belongs to the presentation, colours and
attributes to the theme. Behaviour is specified here and in `plans/widgets.md`,
with executable evidence in `test_tab_control.cpp` and the appearance matrix;
no outside implementation source was consulted. *Rejected:* three public
subclasses or a public renderer interface (one behaviour would acquire an
unnecessary extension contract); silently switching presentation at small
sizes (application-selected geometry must remain deterministic).

**D-118 — Three command-button toolbar presentations share one interaction
contract (refines D-099).** Compact (default) uses one blank inset per side;
Padded uses two blank insets per side; Framed uses a three-row single-line box with a padded caption.
Presentation changes preserve commands, grouping and keyboard position, cancel
an in-flight press, and relay size hints. Explicit groups are flattened in
order; empty groups are ignored. A `│` separator with one blank either side
occurs only between two visible groups, never at a bar edge or beside overflow.
Trailing commands that do not fit move whole into an edge overflow control,
whose presentation matches the buttons and whose popup retains group breaks.
Overflow anchors use the full presentation height. Tiny bounds clip chrome
without negative extents; painting and pointer routing use one geometry.

Independent `ckv.toolbar.normal/focused/hovered/pressed/disabled/checked/mnemonic/
separator` roles govern appearance. Disabled overrides all interactive states;
otherwise pressed overrides focus, focus overrides hover, and hover overrides
checked. The toggle mark persists under every state. Keyboard focus underlines
the caption, including on a checked button. Hover is optional host motion
feedback and cannot be required for activation. Only actionable button regions
request the hand pointer; gaps, separators and unavailable actions do not.
Pointer presses execute on release inside the same command; keyboard Enter or
Space uses release reports when present and immediate activation otherwise.
Escape, focus loss, detachment, item/presentation changes or resize cancel armed
input. Unavailable commands remain keyboard-reachable but cannot execute.
Existing context restoration and mnemonic behavior from D-099 is retained.
Behavior is re-derived here from ckVision requirements and verified by its own
tests and scene snapshots; no outside implementation source supplies behavior.

Prepared toolbar captions, both toggle faces and layout storage are owned by the
widget. Registry revisions and presentation/content changes refresh preparation;
toggle predicate presence is checked dynamically. Steady painting, hit testing,
hover, arrow navigation and checked-state changes reuse this storage. Borrowed
grapheme clipping and validated display-text borrowing preserve D-040 while
avoiding caption copies in the painting layer; command-context scratch strings
retain capacity for steady availability queries (ARCHITECTURE §8).

**D-119 — Explicit widget presentations extend the command-toolbar design.**
The approved scope and per-family requirements are recorded in
`plans/widget-presentations.md`. Each widget keeps its interaction/model
implementation and chooses geometry through an instance-owned presentation.
Semantic roles permit independent appearance without borrowing unrelated menu,
list or status surfaces. Focus must remain distinguishable from selection,
checked state and default-action emphasis in monochrome. Geometry controls
painting, size hints, caret/editor placement and hit testing; reconfiguration
cancels armed input. Small bounds clip the requested style rather than silently
selecting another. No external implementation source supplies this behavior:
these are ckVision requirements with its own tests and rendered evidence.

The owner requires sophisticated, elegant presentation without crowding or
complexity. Defaults favor restrained color, consistent spacing and alignment.
Additional chrome must clarify grouping, hierarchy or interaction, and must not
repeat a signal already conveyed by the field surface or hotkey accent.

Progress starts with Solid (existing colored span), Block (filled blocks against
an explicit shaded track) and Segmented (two-column units, one glyph and a gap).
All are one row. An optional four-column, right-aligned percentage and one-column
gap are reserved when the determinate bar has at least six columns; smaller bars
omit the readout to retain a meter. Indeterminate bars have no percentage. Labels
remain centered on the meter and use the style of the span beneath each cluster.
Dedicated track/fill/label/disabled roles are independent of lists and menus.
The caller's injected pulse drives animation; preparation and paint read no clock.

StatusLine retains the established unbracketed chord-before-label presentation.
The owner explicitly rejected keycap brackets for this surface. Improvements
are spacing, optional group separators, hotkey color/attribute accents and a
separate status-message region, not extra brackets around command hints.

StatusLine's Plain presentation remains the default. Grouped inserts a three-cell
blank/rule/blank gap only at logical group boundaries, including when a group's
first item was removed by priority clipping. Chords retain their leading accent
without brackets. Dividers and messages are inert. Reconfiguration, resizing and
changes of the active item context cancel a held action. Pointer shapes identify
available and unavailable command cells rather than the entire strip. Prepared
labels and layout use retained instance storage keyed by item source, registry
revision, width and presentation. Help-provider results are cached by resolved
key and registry revision; `refresh_hint()` announces a host mapping change for
the same key. Paint and hit testing borrow these results without heap allocation.

The same owner preference applies beyond StatusLine: input fields and values
use foreground/background color, padding and optional underline rather than
decorative square brackets. Input alternatives are Flat, Padded and Underlined;
SpinBox alternatives use separate or stacked steppers without boxed values.
Button padding, a prominent Slider thumb and padded Breadcrumb segments replace
the proposed ornamental bracket variants. Meaningful checkbox markers remain.

D-119 row surfaces: ListView and Table retain Plain by default and expose
`set_banded_rows(bool)`. Banding follows absolute display-row parity through
scrolling and sorting, never changes model identity, and is superseded by an
explicit item/cell style or selection/editing emphasis. Separate list/table
banded roles permit subtle neutral surfaces. Table optionally draws column
rules in its existing one-cell gaps: widths, editor rectangles, sorting and
resize boundaries remain one shared geometry. Rules have an independent role.

D-119 Splitter presentations are Line (one cell), CentralGrip (one cell with
three centered grip glyphs) and Gutter (three cells with a centered line/grip).
The anchored pane extent survives style changes. One clipped divider rectangle
drives painting, clicks and resize pointers. A gutter drag retains its initial
grab offset, so pressing an edge does not make the divider jump. Bounds too small
for pane minima clamp within the available extent, never produce out-of-bounds
handles. Resize, style, visibility and external position changes cancel dragging.
Hover uses an independent role; focus/drag emphasis takes precedence. Disabled
splitters refuse keys and drags. Key releases never adjust the position.

D-119 Slider presentations retain Line as default, with Block track and
ProminentThumb alternatives. An optional unbracketed, right-aligned value
reserves a stable width derived from the range endpoints, separated by one blank.
Track, thumb, ticks and pointer mapping share the remaining track width. Readout
and tick-label rows are inert; disabled input is rejected. Tiny bounds clip the
chosen style and omit the readout when it would leave fewer than two track cells.
Tick label priority and spacing remain deterministic; retained scratch capacity
is prepared when ticks change so steady painting allocates nothing.

D-119 Button extends Classic and Flat with Padded (one-cell horizontal inset,
one row) and Outlined (one-cell enclosure, three rows). Each style is explicit
and clips at small bounds. Default action is bold; focus adds underline to the
caption independently in the new styles. Geometry or caption changes cancel an
armed press. Parsed mnemonic data is retained when text changes, never recreated
during painting. Existing activation and hold-repeat behavior stays shared.

D-119 BreadcrumbBar retains Plain separators and adds Padded segments and
Connected segments joined by a chevron. The last segment is the current location
and is bold independently of focus; focus adds underline. Padded cells belong
to their segment's hit region; separators are inert. Layout and elision are
prepared once per width/content/style change and shared by paint, input and
pointer shape. Reconfiguration cancels an armed ellipsis; disabled input and
points outside the one-row bar are rejected. Overflow still lists hidden levels.

D-119 option groups retain Classic markers and add BoxedRows (three-row
enclosures) and Buttons (one-row padded choices). Buttons retain a visible check,
mixed or radio dot indicator, without decorative brackets. Selection is bold;
keyboard focus adds underline independently. Alternate choice gaps and captions
are inert. Drawing, pointer shapes and activation share layout, including boxed
row height. Parsed captions and column scratch storage are prepared at
construction; steady paint/navigation allocate nothing. Disabled and key-release
input cannot change state. All existing callback, tristate and column semantics
remain shared.

Focused option captions preserve their surface contrast: if a focus foreground
would equal the caption background, keep the caption foreground and underline it.

Option mnemonic accents also preserve the face foreground when their configured
accent would equal the face background, retaining the mnemonic attributes.

D-119 SpinBox uses Compact adjacent one-cell minus/plus steppers by default,
Separate three-cell steppers separated from the field by a blank, or Stacked
three-cell up/down controls beside a two-row field. The value is unbracketed.
Shared local rectangles govern paint, caret and activation; the field is inert
for stepping and wheel/keys retain adjustment. Tiny bounds keep at least one
field cell and clip accessory regions. Disabled input is rejected.

SpinBox field painting and caret borrow complete-grapheme entry suffixes; numeric
values use a local to_chars buffer with explicit ellipsis. No paint/caret string
is allocated. Focus underlines the numeric text. Steppers use input accessory
roles, hover accents and muted limit/refusal feedback; their pointer region
reports refusal accurately, while an editable field reports Text.

D-119 NotificationCenter retains Lines and adds three-row Banners and Framed
cards with one blank row between cards. Severity identity stays in a visible
marker and accent; text stays on the normal surface. Alternate cards reserve an
explicit unbracketed dismissal control at the right. Only that control dismisses
on click; Lines retains whole-line dismissal. Escape dismisses the newest entry.
The newest card underlines its content when focused. Empty space and card gaps
remain inert and transparent. Geometry is clipped and shared across paint/input.
Timers, persistence and callbacks remain shared. Disabled input is rejected.

Disabled notification presentations retain their severity marker identities and
surface, with a muted foreground and Dim attribute; focus emphasis is suppressed.

D-119 PropertyInspector retains Plain as default, adds a quiet Divided name/value
column and Sectioned group headings with a blank before subsequent named groups.
Each PropertyItem owns an optional group label; consecutive equal labels form
one section. Empty labels insert no heading. Group headings and separators are
inert; keyboard navigation continues to enumerate properties only. Optional
banding follows property index, independent of inserted headings and validation
rows. Selection, disabled state and the active editor override banding.
Prepared row positions are rebuilt on content, presentation or validation changes;
paint, hit testing, editor placement and validation use the same positions.
Changing presentation preserves an open edit. Disabled direct input is rejected.
Text painting borrows complete graphemes without steady allocations.

NotificationCenter reports an intrinsic stack-height hint (three rows per card,
one between cards; one row per Line), independent of live bounds. Posting,
dismissal and expiry relay size-hint changes. A caller's larger preferred height
is respected. PropertyInspector editor bounds use its clipped value rectangle,
including after host resize; an invisible row has no visible edit region.

D-119 Wizard retains a Compact one-row header/footer, adds Bands with three-row
header/footer surfaces, and StepRail with those bands plus an inert left-hand
step list. The rail never navigates or bypasses validation. Content and all
navigation controls use clipped shared local rectangles; tiny bounds clip the
chosen presentation rather than changing it. All navigation controls are owned
Button instances (Flat in Compact, Padded in the alternates), retaining hover,
focus, Down/Up cancellation and release-reporting keyboard behavior. Forward
and Finish are separate instances, sharing a stable footprint. Default action
emphasis is independent of focus; wizard focus underlines its page title.
Page-field Enter and directional shortcuts retain immediate validated navigation;
Escape retains cancellation. Disabled direct input is rejected.
Step formatter results and numbered rail captions are prepared when pages or
labels change. Rendering borrows complete graphemes without steady allocations.
refresh_navigation updates the buttons' live availability; hosts call it when
external validation dependencies change. Draw also synchronizes availability
before children paint, retaining the live can_continue contract. Button callbacks
recheck validation when committing. Page transitions retain focus in arriving
page controls when focus was inside the departing page. Dedicated wizard.header,
wizard.footer and wizard.rail roles provide independently configurable surfaces.

Wizard headings use the label foreground on their own surface, preserving
contrast in Classic as well as the other schemes. Navigation buttons reject
release after ancestor disablement and report pointers only within their bounds.
Replacing pages or changing layout cancels outstanding Button activations.

Wizard controls whose clipped rectangles are empty leave the Tab traversal;
shrinking a focused navigation control out of view returns focus to the wizard.

D-119 refines D-117/D-118 to the owner's restrained visual standard: compact tab
selection uses color and bold weight, with focus underlining independently.
Compact/Padded toolbar captions and their overflow control use blank padding,
without decorative square brackets. Hit rectangles, spacing, command grouping
and overflow geometry retain their intentional padding. Framed containment and
meaningful toggle/checkbox markers remain. No interaction contract changes.

D-119 expands the exhaustive four-scheme appearance matrix. Its complete TSan
regeneration and byte comparison takes 130.731 seconds on the named Linux
verification host, exceeding the previous 120-second test deadline even when
run alone. The appearance-matrix test allows 300 seconds under ThreadSanitizer
for instrumentation and shared-runner overhead; other configurations retain
120 seconds. This is a functional-test completion deadline, not a runtime
performance budget. All golden comparisons, coverage checks, sanitizer checks
and deterministic benchmark limits remain required.

## D-121 — Portable externally timed terminal termination (2026-10-03)

Hosts holding `term::TerminalSubsession` must not depend on a concrete
platform class to end a child while servicing other sessions. Promote
`request_termination()` and `request_kill()` to that interface: POSIX retains
process-group signals; Windows sends private ConPTY Control-C and explicitly
escalates with `TerminateJobObject`. Neither request waits or releases native
resources. The host keeps draining and times grace through its injected clock.
Model-only sessions have no child and do nothing. Repeated requests, failed
launches and already closed sessions are safe.

Promote the existing `snapshot(TerminalSnapshotOptions)` overload as a pure
portable operation too. Implementations construct only requested history and
rasters, rather than copying and pruning. Grid cells remain included; callers
needing only scalar state use `status()`. Native tests exercise the base
interface, uncooperative children, live peers and Windows descendants. Behavior
derives from the embedded-terminal architecture and Microsoft's published
[Job Objects contract](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects).
The implementation and release gates are recorded in
[portable-terminal-lifecycle](plans/portable-terminal-lifecycle.md).

## D-122 — Instance-owned native clipboard export (2026-10-03)

`term::WindowsClipboardWriter` implements the existing D-022/D-039 write-only,
best-effort service independently of console attachment. `WindowsTerminal`
owns this adapter for its default export path. Each write owns one invisible
message-only window on the adapter's constructing thread, using the built-in `STATIC`
class rather than registering process-global state. It publishes immediate
`CF_UNICODETEXT` data and destroys the publication window before returning.
The system retains that immediately rendered data after owner destruction;
later writers therefore do not send `WM_DESTROYCLIPBOARD` to a lingering window
whose owning terminal thread is waiting on native I/O rather than GUI messages.
Neither delayed-rendering callbacks nor a private message pump is introduced.
The clipboard owner is never a borrowed console
host's window. UTF-8 conversion and allocation finish before the clipboard is
opened; invalid UTF-8 and embedded NUL are rejected without replacing content.
Contention drops the export without a retry loop on the UI thread.
The independent Application clipboard remains authoritative for in-app edits.

This corrects the former `OpenClipboard(nullptr)` followed by `EmptyClipboard`:
Microsoft's [OpenClipboard contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openclipboard)
explicitly states that the resulting null owner prevents `SetClipboardData`
from succeeding. The owner lifetime follows the published
[message-only window construction contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw).
Native regression tests run in a freshly created, separate window station,
which has its own clipboard; isolation failure is a test failure and never a
fallback to the user's clipboard. The public Application copy route, Unicode,
owner replacement/destruction, contention and recovery are executable cases.
This decision does not change the portable void/best-effort export interface.

The named Windows 11 25H2 ARM64 host nevertheless accepts a null-owner Unicode
write in a private window station. That observation is not a reproduction of
data loss in the previous backend. The adapter verifies `GetClipboardOwner`
after emptying and publishes only when it names this call's owned window,
enforcing the documented postcondition rather than depending on permissive OS
behavior. A compiled null-owner mutation exercises this check.

## D-123 — Native Windows injected filesystem (2026-10-03)

`term::WindowsFileSystem` implements D-039's existing `FileSystem` byte-file
contract. Host applications inject this adapter rather than writing their own
Windows file-dialog, editor or save backend. UTF-8 paths are converted strictly
to wide drive/UNC paths, including extended-length spelling; path helpers retain
the actual drive/share root. Native relative resolution belongs at this term
boundary, never in deterministic core/widgets. Device namespaces and alternate
streams are not ordinary byte files and are rejected.

Reads pair contents with the opened handle's identity/size/revision, never a
fresh path lookup. Revisions also hash the opened bytes using the system SHA-256
service: native timestamp granularity can hide fast same-size writes. Reads
hash the chunks they return; fingerprint-only operations stream without
retaining the whole file. Native sharing excludes ordinary write handles during a
read. Conditional writes serialize cooperating processes with a persistent
directory lock; native random exclusive siblings are flushed and renamed by
handle with replacement/POSIX semantics. The latter permits an open reader to
retain an old complete file while new opens see the replacement. Failed
publication does not remove the existing path or copy over its live contents.
Existing discretionary permissions are preserved without privilege elevation;
creation inherits directory permissions. Other metadata/streams are not part
of this service. No GUI host, global registry, retry thread or new external
dependency is involved: `advapi32` and `bcrypt` are Windows system libraries.

Authority: Microsoft's [native rename contract](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info),
[replacement flags](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information),
[sharing rules](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew),
[file information](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex),
[file timestamp lifetime](https://learn.microsoft.com/en-us/windows/win32/sysinfo/file-times),
[directory locking](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex),
[DACL access](https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-getsecurityinfo)
and [system random generation](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom).
Digest ownership follows [BCryptCreateHash](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptcreatehash).
The same-timestamp regression uses the documented [native timestamp suppression](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_file_basic_information).
`ReplaceFileW` is not used: its documented partial-failure states can remove
the target, unlike the FileSystem old-or-new-content requirement. Implementation
and native evidence are tracked in [the filesystem plan](plans/windows-filesystem.md).

## D-124 — Clipboard export returns an explicit outcome (2026-10-03)

D-022's internal clipboard remains independent from host export. D-039's
`ClipboardWriter::write_text`, `Terminal::write_clipboard` and
`Application::set_clipboard_text` return a core `ClipboardWriteResult`:
`Ok` means the native/memory service accepted the contents; `Submitted`
means a terminal request was written, not that a remote host confirmed it;
`Unsupported`, `InvalidText`, `Unavailable` and `Error` describe refusal.
The result carries a native error number where available and warns when a
failed operation may already have changed external state. It never reads the
clipboard or guesses whether a remote paste will succeed.

Application stores its internal text before export, retains the last export
result, and permits one explicitly installed result handler for widget-driven
copies. A paste import does not export or replace that result. The handler
receives a value snapshot; replacing the handler from its callback is safe,
and callback exceptions remain contract violations, never environmental
error signaling. No host-capability lookup or singleton enters UI code.

Native Windows reports invalid UTF-8/NUL before touching the clipboard,
contention without retrying, and the first OS failure before cleanup can
overwrite its error code. An error after EmptyClipboard may have changed the
external clipboard. Successful native publication is not silently lost in
the void terminal adapter. POSIX returns Submitted only after writing the
entire OSC 52 request; an output error becomes a typed error. In-memory
services report Ok. Recording captures every attempt and its result,
including refusals; replay returns those recorded outcomes rather than
inventing success. Unexpected replay attempts report Error and make the
operation log differ. This preserves deterministic error-driven UI behavior.

Authority: Microsoft's [OpenClipboard](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openclipboard),
[MultiByteToWideChar](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar),
[EmptyClipboard](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-emptyclipboard),
[SetClipboardData](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setclipboarddata)
and [CloseClipboard](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-closeclipboard)
contracts; the terminal protocol provides no acknowledgment for OSC 52 export.
Rejected: a bool conflating confirmed publication and unacknowledged submission;
silently ignoring native errors; OS calls in the application to compensate
for a void library contract; clipboard import as verification of a write.

## D-125 — Explicit Windows command-processor launches (2026-10-03)

`TerminalLaunchSpec::windows_command_processor(executable, command)` states
cmd command syntax separately from an ordinary argument vector. Its optional
`windows_command` value cannot be combined with arguments or argv0. Its command
is itself optional: absent means interactive cmd; present (even empty) means
execute that command and exit. Windows forms an explicit `/d` interactive or
`/d /s /c` command launch, wrapping the unchanged command
in the outer quote pair cmd removes. It does not apply MSVC argv escaping to
command text. AutoRun is disabled; UTF-8/NUL and the total native command-line
length are validated before creating a child. POSIX rejects the Windows-only
form before forking. Program/PowerShell argv launches remain unchanged.

The cmd executable token uses native backslashes while CreateProcess receives
the explicitly named image; no PATH search or executable-name guessing occurs.
A forward-slash image spelling is otherwise a valid Windows API path, but the
native cmd parser treated `/k` in `/klukas` as an option in a copied executable's
image token. An eight-case native separator matrix fails with forward/mixed
separators and passes with backslashes at identical command, cwd and ComSpec.
ASCII/Unicode names, spaces/ampersands and system/relocated ComSpec are independent
controls. The conversion belongs to this explicit library encoder, not an app.

Authority: Microsoft's [cmd syntax and quote handling](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/cmd).
The native 0.1.10 black-box probe compiled with MSVC /W4 /WX: both cmd cases
failed (including `echo CK-CMD-BASIC`, exit 1), while two PowerShell cases
passed. Seeing a marker in cmd's error message did not count as success:
the probe also required a completed child with exit 0. This is a library
launch-contract gap, not a reason for an adopter to compose native quotes.
The implementation candidate is tracked in [the command-launch plan](plans/windows-command-launch.md).

## D-126 — Scalable instance-owned Windows readiness (2026-10-04)

The terminal layer supplies `WindowsWaitSet`, borrowing a Clock and a set of
native readiness sources. It waits through an instance-owned completion port
and Microsoft's documented wait-completion packet association. No application
thread, thread pool, global registry, incomplete-source polling or 64-source
limit is introduced. WindowsTerminal uses it for console input, its wake event
and extra sources supplied by the embedding host. Core/UI remain OS-free.

Sources are borrowed manual-reset readiness events, console input or process
handles; mutexes/semaphores are not readiness sources. Callers hold their
objects live until the next source-set update or clear. Duplicate sources are
coalesced; non-Windows/zero sources are ignored, invalid native sources fail
explicitly. A call returns one batch of distinct currently completed sources,
not an atomic snapshot of all kernel states. A hot source is rearmed only on
the next call, so it cannot monopolize its own batch. Deadline conversion
rounds a positive fraction upward and caps finite waits below INFINITE.

Registrations persist across calls. Removal first retires the unique generation
key, cancels the association and closes the packet. A pending cancellation
never permits immediate reuse; a replacement receives a new packet/key, and
late completions are discarded without dereferencing retired storage. Clear
releases registrations before borrowed handles may be closed. Results are
instance-owned spans valid until the next wait/clear; steady unchanged-set
waits reuse scratch storage. Native failures throw std::system_error rather
than report an empty successful wait. No optional thread-based fallback hides
a missing OS API.

Authority: Microsoft's published [creation](https://learn.microsoft.com/en-us/windows/win32/devnotes/ntcreatewaitcompletionpacket),
[association](https://learn.microsoft.com/en-us/windows/win32/devnotes/ntassociatewaitcompletionpacket)
and [cancellation](https://learn.microsoft.com/en-us/windows/win32/devnotes/ntcancelwaitcompletionpacket)
contracts. Native Windows 11 ARM64 black-box experiments delivered 130 unique
event completions with unchanged thread count, then independently verified an
empty console stays unsignaled and a written key generates an input completion.
These establish feasibility only. The implementation's regression, mutation,
public-terminal and platform gates are recorded separately in
[the readiness validation plan](plans/windows-readiness-validation.md).

## D-127 — Native terminal diagnostics on both hosts (2026-10-04)

The explicitly constructed POSIX and Windows outer backends share D-077's
native diagnostics contract: `set_output_capture`, `set_graphics_trace` and
the cumulative `mouse_reports_seen` observation. Host code injects sinks into
its chosen native backend; UI still consumes only Terminal and injected data.
No environment lookup, global file or adopter-side decoder/capture is added.

Capture observes ordered output attempts before native writing, including
title/bell/probe sequences and normal restoration from the time it is set.
Constructor bytes precede installation and are not retroactively delivered.
Empty callbacks disable capture; exceptions from this best-effort diagnostic
sink cannot suppress native output or interrupt restoration. Native clipboard
publication is not a terminal byte stream and is not captured. Fatal POSIX
signal-handler output remains signal-safe and cannot invoke an arbitrary sink.

Both native backends use one internal capability-summary formatter. Tracing
reports capabilities when each probe window settles, with borrowed sinks and
clock, and does not construct a message when disabled. Decoded SGR reports
are counted by their existing InputDecoder; no second parser or guessed count
enters an application. Recording a native terminal still forwards its output
and inputs, so native capture sees the same emitted stream while replay remains
deterministic operation playback rather than a second native session.

## D-128 — Unicode native diagnostic file paths (2026-10-04)

`FileTraceSink::open` takes a UTF-8 filename on Windows, consistent with the
text boundary in ARCHITECTURE §2 and the native filesystem adapter. Convert
strictly to UTF-16 and open through the wide CRT interface; never interpret
the filename through the current ANSI code page. Empty names and embedded
NULs are refused rather than invoking CRT invalid-argument handling or opening
a truncated prefix. Invalid Windows UTF-8 returns no sink, like an ordinary
file-open failure. Unix filenames retain their native byte spelling.
Append/truncate behavior and per-line binary LF output remain unchanged.
The file remains readable while the sink is alive and permits a child sink
to append concurrently. Native wide opening therefore states `_SH_DENYNO`
explicitly rather than adopting the restrictive default of `_wfopen_s`.

The correction belongs to the diagnostic adapter, not its host. Regression
cases use an explicitly selected unique scratch directory, Unicode filenames,
append/truncate, a missing parent, malformed Windows UTF-8 and embedded NULs.
Candidate evidence before this record update: full frozen inputs
`95510baeb9e5f0aec18904a71540660600115a9560ee928ea481937b841cd41e`
pass Apple Clang strict Release 223/223 (43.62 s), native Windows 11 ARM64
MSVC strict Release 213/213 (35.21 s), and both benchmark gates, exits 0.
A compiled narrow-open mutation fails three Unicode content/spelling
assertions; restored native six-case contract passes, exit 0. The first
wide-open candidate failed existing sharing assertions; `_SH_DENYNO` restores
that contract. A later Windows full gate failed twelve scanners on binary
AppleDouble metadata transported by the Mac archive. Removing only verified
generated metadata left all source/assets and the executable unchanged;
the complete repeated gate passed. Failed logs are retained. The complete
pre-version candidate now passes Mac, Linux and native Windows, including
Mac sanitizer/fuzz configurations, as recorded in the native host text
validation plan. D-128 is not in released 0.1.13; the 0.1.14 version/assets,
clean committed integration and filtered publication gates remain required.

## D-129 — Shared Windows argv encoding (2026-10-04)

Native hosts and ConPTY launches need the same deterministic argv-to-command-
line boundary. Expose pure `term/windows_argv.hpp` functions rather than
duplicating the adapter's private encoder in each host. This is the Microsoft
CRT argv convention, not shell syntax; cmd command text remains D-125's
explicit command-processor route. Ordinary arguments preserve empty values,
Unicode, whitespace, quotes and trailing backslashes. The first token has
the documented distinct executable-name grammar: surround it with quotes,
do not double its backslashes, and refuse an empty name or embedded quotes.
Embedded NULs and command lines exceeding 32766 wide code units are refused.
No process, environment or filesystem access enters these pure functions.

Authority: Microsoft's [argument parsing rules](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments)
and [CreateProcessW limits](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).
Portable literal goldens and native launch regressions are required; full
library, sanitizer and publication gates remain required before release.

## D-130 — Strict native Windows text composition (2026-10-04)

Expose `term/windows_text.hpp` for native hosts crossing between UTF-8 and
Windows UTF-16, including a wide entry point's command-line arguments.
`windows_utf16` and `windows_utf8` return owned strings or no value for
malformed Unicode, native conversion failure, or an unrepresentable native
length. Empty text succeeds; explicit lengths preserve embedded NULs.
Filename/process boundaries still refuse NULs under their own contracts.
The conversions use only CP_UTF8 with strict invalid-character flags, never
the ANSI code page, locale, environment, normalization or replacement text.
ConPTY and filesystem adapters reuse this boundary rather than carrying
private conversion implementations. Native hosts need no adopter-side
transcoder. No operating-system call enters the pure core layers.

Authority: Microsoft's [MultiByteToWideChar](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar)
and [WideCharToMultiByte](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte)
contracts. Native literal Unicode goldens, invalid UTF-8/surrogate rejection,
empty/NUL-preserving cases and real wide-argv process observations are gates.

## D-131 — Explicit owned-process resource observations (2026-10-04)

Core defines full-width signed `ProcessId` (no process = -1) and clock-free
`ProcessResources`: optional cumulative CPU, resident and private-resident
bytes, live/unreadable member counts, typed availability and a native error.
`TerminalSubsession::process_resources()` defaults to Unsupported; the native
Windows adapter observes its own job through reusable explicit term samplers.
There is no system-wide parent-guessing, native-handle narrowing, hidden cache,
rate calculation or process control in this observation contract.

Windows job CPU includes exited members. Current memory enumerates nested-job
membership, checks opened-process membership against PID reuse, skips racing
exits, and refuses to label partial sums as full memory totals. Retries for
growing membership/working sets are bounded and fail visibly, never truncate.
Private resident bytes count non-sharable resident pages from QueryWorkingSet,
available throughout the supported ConPTY OS range; PrivateUsage is virtual
commit charge and is never presented as resident physical memory. Permission
and native-query failures are distinct from measured zero and absent processes.

Authority: Microsoft [job accounting](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information),
[nested membership](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_process_id_list),
[working-set query](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-queryworkingset)
and [resident page attributes](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-psapi_working_set_block).
Portable value/default contracts and real native owned jobs, exiting CPU
workers, resident allocations, permission refusal and ConPTY descendants are
required gates. The implementation is prepared for version 0.1.15; complete
application statistics acceptance is a separate downstream gate, not a claim
made by the library contract tests.

## D-132 — Captured non-terminal helper processes (2026-10-04)

Terminal applications may hand selected bytes to an explicitly configured
helper through private stdin and captured stdout/stderr, without a PTY or
ConPTY and without exposing selection bytes in argv. The scoped service
follows D-039: shared core invocation/result values and an injected runner,
native ownership/waits only in term, timing only through a borrowed Clock.
Extract one shared launch model from the terminal-specific spec rather than
copy native quoting, environment or validation rules. Terminal-only profile
and lifecycle remain in the embedded-session contract.

Completion requires all input delivered and a known root exit. Captures are
bounded but drained; idle deadlines reset only on actual byte transfer.
Successful background-descendant release is an explicit required policy,
not an accidental wait for output EOF or kill on destruction. Windows owns
an assigned job and an explicit stdio inheritance whitelist; POSIX owns an
honestly scoped process group. Failures retain typed status/native errors and
clean up owned resources. No persistent I/O worker threads are introduced.

The detailed contract, published references, real-child acceptance and
compiled-negative requirements are in [plans/pipe-process.md](plans/pipe-process.md).
Status: the implementation landed at b1a4ccd; generic independent launch and
failure diagnostics landed at dc3312f3 / filtered786617f8. Local native,
production-negative and sanitizer gates pass. Hosted CI37239077787 records
one byte at4.275ms, then over150ms without further byte progress before the
correctly typed idle timeout. This is not startup failure. The positive
fixture's sleep has no bounded wake-up guarantee under the
[C++ timing contract](https://eel.is/c++draft/thread.req.timing). It now
continuously transfers output for360ms instead: the150ms idle limit, successful
exit and exact12 retained bytes remain, with truncation explicitly required.
No production timeout or backend changed. Revised frozen-candidate full gates
pass Mac/GCC Release 229/229, Windows ARM64 MSVC 222/222, LLVM23 Debug
ASan/UBSan plus fuzz 239/239 and TSan 228/228; the existing compiled production
negatives retain their assertions. Clean-commit/publication and hosted gates
still precede release0.1.16 and downstream adoption. The hosted OS's exact
scheduling cause is not claimed.

## D-133 — Verified independent Windows process launch (2026-10-04)

An independently living process is different from a captured helper or a
terminal child. Add an injected core `DetachedProcessLauncher` and a native
Windows backend using the shared invocation/environment preparation. No handles
are inherited. Create console-detached, with breakaway requested, but suspended;
verify absence from any job before resuming application code. Breakaway alone
does not prove independence: it can leave a child in a restrictive ancestor.
Refuse that launch and terminate the never-resumed child. Preserve the primary
failure, full-width child identity, and separate bounded cleanup result.
Started means native launch, not application readiness. Endpoint discovery,
protocols and application shutdown remain outside the library.

Authority: Microsoft's [nested-job semantics](https://learn.microsoft.com/en-us/windows/win32/procthread/nested-jobs)
and [IsProcessInJob](https://learn.microsoft.com/en-us/windows/win32/api/jobapi/nf-jobapi-isprocessinjob).
Six actual native cases cover controlled permissive/restrictive immediate and
nested jobs, child survival after permissive ancestor closure, and absence of
child application entry after refusal. A compiled omission of retained-job
refusal fails three intended assertions, including the independent entry
witness. Two pure result cases cover full-width identity and separate cleanup
failures. Candidate gates pass Mac/GCC229/229, Windows ARM64 MSVC222/222 and
Mac LLVM23 ASan/UBSan plus fuzz239/239. The first full native repeat exposed
an instantaneous zero-active-job assertion after the controller signaled exit;
the final fixture observes retirement with a bounded wait, independently of
native launch/cleanup budgets. Clean committed gates pass Mac229/229,
GCC13.3 ARM64229/229 and MSVC ARM64222/222. The generic capability is on
private main dc3312f3 and filtered public786617f8; release0.1.16 still waits
on the complete hosted gate. No released independent-launch capability yet.
Hosted Windows37239077787 retained an ambient job in all five controlled
launch setups, before exercising the backend. A test-only host adapter now
queries ambient membership and, when present, uses a unique same-user limited
S4U scheduled task to run the same complete contract independently. The
contract still proves actual controller independence; all assertions and typed
refusals remain. Task setup/timeout/exit failures are red, and registration is
removed in `finally`. No scheduled launch enters library code. Ten native
repeats and the explicit forced-scheduled path pass all six cases; the revised
full native gate passes 222/222. Its first full invocation omitted Git from
the external validation driver's PATH, failing three documentation/license
checks before their work; the explicit runtime correction passes the whole
gate. Clean-commit/publication and hosted gates still precede release.

## D-120 — Advanced progress and task displays (2026-10-03)

Implement `plans/advanced-progress.md` through a lightweight Progress meter,
an ActivityIndicator, an owned ProgressModel, a responsive ProgressView and an
explicit ProgressController. Smooth meters use eighth-cell Unicode block fill;
ASCII and static motion choices are explicit. Lifecycle state and unknown totals
remain separate. Parent aggregation is manual by default or explicitly weighted;
unknown children prevent an exact aggregate percentage. Counts are never summed
across unrelated units. Completion and cancellation require explicit task state.

Timing enters only through explicit model timestamps and Application's injected
Clock. Use one one-shot animation timer per controller, no worker threads, no
draw-time clock reads, and no replay of missed frames. Scoped observations and
effective visibility notifications stop hidden or detached animation. Worker
updates use a bounded coalescing mailbox and Application::post. Prepared painting
borrows strings and does not allocate per row or frame.

Rate uses a bounded eight-sample window, requires two distinct timestamps and
positive work, and expires after three seconds without advancement. Pause excludes
paused duration and resets rate history; regression/reset restarts measurement.
ETA is unavailable for unknown totals, weighted parents and stale estimates.
Smooth fill floors eighth-cell coverage with exact zero/full endpoints.

Behavior derives from ckVision's architecture and the explicit contracts in
the plan. External references are public documentation of presentation patterns;
Unicode specifies glyphs. No outside implementation source is used. A portable
interactive Progress Lab, renderer-generated documentation and deterministic
tests are required evidence for all styles, state transitions and options.

## D-135 — Read-only cross-architecture Windows process-image inspection (2026-10-05)

Applications selecting an executable need a generic native preflight, separate
from launch arguments, terminal ownership and application roles. Inspect
bounded PE/COFF headers and query host user-mode architecture support; never
execute a candidate merely to classify it. Refuse DLLs, malformed/truncated
headers and non-Windows process subsystems. Return distinct native observation
failures and structural/architecture refusals. The preflight is not complete
loader validation, authentication, dependency resolution or a TOCTOU guarantee.

On the observed Windows11 ARM64 host, the x64 GetBinaryTypeW rejects system
cmd.exe with193 while CreateProcessW of that same file succeeds and exits37.
The native ARM64 caller reports type6 and succeeds too. Do not special-case a
shell or adopter: use GetMachineTypeAttributes on Windows11 and the documented
native/x86 support baseline on older ConPTY-capable Windows. No shell naming,
server, IPC or installer policy enters the library. Native tests include real
system-image execution and synthetic malformed/architecture header goldens.
Authority: Microsoft PE/COFF, GetMachineTypeAttributes, IsWow64Process2 and
Windows on Arm emulation documentation; no outside implementation source.

## D-134 — Opt-in, pinned ConPTY runtime deployment (2026-10-05)

D-073's optional runtime requires a complete deployment, not just a DLL.
Provide `ckvision_deploy_conpty` in the source and installed CMake package.
The caller explicitly supplies an existing archive, its SHA256, application
architecture, build-owned executable target and relative install destination.
No download, package-manager invocation, default runtime dependency or
application-specific policy enters the library. The application chooses its
package pin and owns archive/installer publication and update policy.

Check the pin before extracting, retain each pin in its own build-directory
namespace, check package ID/license/version metadata, and inspect PE/COFF
architecture and executable/DLL kind before deploying. Metadata is a format
check, not publisher authentication: the caller must obtain and approve the
archive from the publisher. Match the DLL to the application and include all
OS-compatible console hosts, including ARM64 for an x64 application running
under emulation. Retain Microsoft's MIT notice and a package identity record.
Explicit file installation excludes unrelated or stale package contents.
Both link completion and ordinary no-op builds refresh deployment; a changed
archive invalidates configuration and is rechecked before copying.

Authority: the [Microsoft ConPTY package](https://www.nuget.org/packages/Microsoft.Windows.Console.ConPTY/1.25.260930003)
and its published native deployment metadata, plus the
[PE/COFF format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
Synthetic PE/package fixtures test layouts, pins, missing/wrong-architecture
payloads, application ABI refusal, refresh and changed-pin isolation. They
do not establish native DLL execution or graphics support; actual native
consumer and ConPTY gates remain required before release. No server, session,
protocol, installer or application naming enters this deployment API.

Preparation evidence, not release acceptance: the 12 deployment/layout/refusal
cases pass on Apple Clang, Debian ARM64 GCC14.2 and native ARM64 MSVC17.
The helper-deployed official runtimes1.24.260710001 and1.24.261001001 pass all34 native
`test_windows_terminal_subsession.cpp` cases using the unchanged b8aca7e5
test executable. Runtime1.25.260930003 fails the pixel-mouse proof and initial
graphics/resize proof cases on two separate runs (six assertions each), while
the other32 cases pass. The cause is not yet established. Do not substitute
the synthetic packaging gate for this red native version check or claim
unqualified compatibility with that runtime. The October1.24 maintenance
archive pin is4d6aaddc1d2385c9f5897df28f33879f699f8f2783315d5204cf3d8c3616ac5f;
it remains consumer-selected, never an implicit helper default. Complete SDK
gates and a new release remain pending.

## D-136 — Re-pin the idle performance reference host (2026-10-09)

The owner re-pins the wall-clock performance reference to the same Mac13,1,
Apple M1 Max, 32 GiB machine after its update to macOS 27.0.1 (build 26A434).
The reference identifier is `macos-27.0.1-arm64-m1-max-32g`. The earlier
macOS 27.0 (26A428) runs remain historical diagnostics; none was an accepted
idle-host baseline, and their numbers do not transfer to the new pin.

The event-to-bytes p99 limit remains below 2 ms and the full theme-switch
recompose/diff limit remains below 5 ms. Record a clean Release source and
output identity, both harness results, and host-idle evidence on the new pin
before accepting the first baselines and their later 5% regression ratchets.

## D-137 — Release 1.0.0 and version compatibility (2026-10-09)

The owner directs a 1.0.0 release of the current framework. Version 1.0 does
not assert the absence of defects or convert an unperformed acceptance check
into a passing result. The previously waived five-app walkthrough and deferred
idle-host p99/theme-switch baseline remain disclosed; D-136 and the open
milestone acceptance rows continue to apply. The 0.1.19 release remains a
historical, immutable release rather than being relabeled in place.

Starting with 1.0.0, the public C++ and CMake package surface follows semantic
versioning: compatible additions use a minor release, compatible fixes use a
patch release, and incompatible public-surface changes require a new major
release. Internal APIs and examples are not a compatibility promise. An API
may be deprecated with a documented replacement in a minor release; removal
or an incompatible signature or behavior change waits for a major release.
Defect corrections can change behavior when the previous behavior violated
the documented contract. This supersedes D-016's pre-1.0 freedom for releases
from 1.0.0 onward; trunk-only linear history still applies.

## D-139 — Provider-driven inline completion and responsive directory entry (2026-10-11)

The owner's requested interaction is specified in
[the completion plan](plans/completion-provider.md). InputLine owns optional
completion presentation and acceptance through an instance-owned provider:
subdued suffixes remain outside committed text, plain Right accepts at an
unselected end caret, Tab retains focus traversal, and acceptance is one normal
undoable edit. Explicit UI-thread refresh publishes asynchronous provider
results only for the current request revision. Providers must not block or
scan unbounded data in dispatch. The field has no filesystem knowledge or
hidden execution facility. Completion lists remain explicit composites under
D-042.

File and directory dialogs share directory data between their path field and
visible entries. Active extension filters and hidden-file policy apply to both,
with directories retained for navigation. Enter on committed directory text
navigates; relative paths and dot segments resolve through injected filesystem
semantics. Completion previews never silently become accepted file results.
Directory discovery, full-source matching, and large-data retirement stay off
the UI thread, with bounded publication and explicit cancellation/lifetime.
For the same canonical directory and generation, the visible bounded subset
can be re-filtered immediately as a fresh partial result while the worker
matches the full index. Canonical aliases reuse discovery; an unavailable
typed parent does not discard the preceding usable index. Old worker replies
keep their revisions. Grapheme-safe ranges use the full editing state, and
suffix preview includes the preceding cluster when checking its boundary.

FileSystem gains an incremental DirectoryCursor contract (one entry, end, or
failure) and resolve_directory for existing-directory identity, including dot
segments and native link semantics. These are injected blocking operations for
the discovery service, never InputLine or dialog dispatch. Unsupported adapters
return no cursor rather than secretly materializing a directory. Native cursors
use POSIX opendir/readdir and Windows
[FindFirstFileW/FindNextFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilew),
with native directory resolution using realpath and
[GetFinalPathNameByHandleW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew).
MemoryFileSystem provides deterministic scripted equivalents without OS access.

The responsive DirectoryService overloads implement this workflow; the released
filesystem-only conveniences retain their earlier synchronous contract under
D-137. Native execution has an explicit fixed worker limit, one coalesced query
and result, fixed handoff buffers, and a worker-owned disk index with bounded
ordered pages. Navigation probes readability before committing the directory;
Select-only folder identity does not require readable contents. Service teardown
joins only at the host boundary after dispatch. No global worker facility or
implicit temporary-directory choice is introduced.

Directory pages prepare their bounded display labels when published. ListItem
adds an optional borrowed_text and caption() accessor so provider-owned labels
can be painted and identified without copying a long filename on every read.
Borrowed storage remains valid until the next model mutation; existing owning
text rows keep their contract. This is an additive D-137 surface change.
The publication byte bound includes duplicated first/last pagination keys.
Native browse paths reserve room for one maximum-size row and both keys, so a
normal byte-limited page has a usable boundary. A preferred row removed by
trimming is retained as a pin; when only the pin fits, paging clears it and
keeps the existing boundary. Every eligible row remains reachable without
narrowing the supported path or filename limits. Demo controllers retain
presentation handles with PendingDialogs.

Authority: the owner's requested behavior, D-039's injected service boundary,
D-043's visible-slice model contracts, and ARCHITECTURE.md §8. This decision
adopts the behavior; implementation and acceptance evidence are tracked in the
completion plan and do not follow merely from this entry.

## D-138 — A lost terminal ends the session through the caller, not the process (2026-10-11)

D-078 has a backend throw when its terminal can no longer serve the session,
so that the caller can end it. `Application::step()` and `run_until()` ran that
throw into the catch-all that enforces ARCHITECTURE §11's noexcept-in-effect
callbacks, so a host that had gone away was reported as "application callback
threw" and the process died by `SIGABRT`. The exception was never the
application's. A program whose terminal was closed under it, by a closed
window, a killed parent or a dropped connection, crashed with a crash report
instead of ending in order.

The session-ending exception is now its own type, `ckv::term::TerminalLost`, a
`std::system_error` carrying the operating system's code. Both session-owning
backends throw it for a write the terminal no longer takes and for a read the
console no longer serves. Every Application callback boundary lets it through
to the caller. That includes a callback that was itself painting, ringing the
bell or setting the title when the terminal went, because the terminal ending
is not a defect in that callback. Every other exception from a callback is
still the contract violation it was. The caller catches `TerminalLost` around
its loop. The Application and Terminal then unwind, restoring the session
best-effort as D-078 requires, and the caller reports `what()` once that is
done.

The POSIX backend also ends the session when its input hangs up. In raw mode a
read returns no bytes only at end of file, so a zero read or a read error means
the terminal is gone. A session that is not drawing anything otherwise waits
for input that cannot come, and runs on unseen and unstoppable. That is the
outcome D-078 exists to prevent. Whatever the input still held, a recovered
paste included (D-040), is delivered first. The poll that has nothing left to
deliver throws.

`TerminalLost` is a compatible addition under D-137. Code that catches
`std::system_error` still catches it. *Rejected:* letting every exception out
of `step()` (it would undo §11's guarantee for real callback defects), and
telling the two apart by a flag on the terminal rather than by type (every
boundary would need the terminal's state as well as the exception, and an
application that drives its own loop could not tell them apart).

## D-140 — Bounded live log services, models and presentation (2026-10-12)

The owner requested implementation of WP-73…WP-84, including a three-platform
log-monitor demo and optimal behavior when producers exceed terminal throughput.
[The live log architecture](plans/live-log-architecture.md) is binding.
Dependency-neutral source/record contracts live in core; native file/pipe
execution lives in term; pure widgets own bounded retention, decoding, filtering
and view state. OS-specific log selection/schema policy belongs to the demo.
No dependencies or hidden registries are introduced.

Records use source/generation/sequence identities. Native events are change
hints, read results are consumed once, and query/source revisions retire stale
publications. One bounded payload arena plus fixed record slots and separately
bounded decoder/index/mailbox storage establishes explicit memory accounting.
Painting resolves visible rows only. Input and filter drains yield by work
quantum; terminal frames coalesce behind existing host pacing. File backlog
retains offsets; non-replayable streams select explicit backpressure/drop policy
and show loss counters. Paused viewing continues ingestion.

UTF-8/control safety uses ckVision core policy. Literal predicates support exact
UTF-8 or explicit ASCII-insensitive matching. Structured records carry bounded
fields; no regex/ANSI execution engine or guessed multiline parser is introduced.
Product limits are distinguished from measured throughput. Existing performance
budgets remain binding; new timing thresholds require named-host measurements.

The owner permits load-deferred testing across intermediate packages, through
WP-84 at the latest. Such evidence remains pending and does not promote a row
to Done. Lightweight checks continue where feasible; intermediate commits report
the checks actually run and explicitly deferred matrix. The authority is the
owner request and ckVision specification, with native behavior derived from
published platform references linked in the delivery plan.

### D-140 native file publication refinement

WP-75 settles the unreleased file-result prototype as a caller-owned bounded
value transferred by take(result), with independent changes_pending hints.
The native adapter preallocates request/publication capacity and reads directly
into that fixed result. No per-read heap objects, hidden polling timer or UI
worker waits are introduced. Result operation, before-read observation and
128-bit native file identity complete the race/reconciliation contract. The
service owns workers and native wake/watch handles; sessions only retire them.
This refines the initial WP-73 development prototype before client release;
[the live-log architecture](plans/live-log-architecture.md) records the contract.

The native file service accepts an explicit instance-owned LogWake target.
TerminalLogWake borrows a terminal with lifetime enclosing the service, and
signals only readiness, a newly pending hint or failure. It retains no view or
Application callback. Repeated hints coalesce and callers can avoid result-poll
timers when has_wake is true. This realizes D-140's explicit wake bridge.

### D-140 file follower refinement

WP-76 follows the bounded execution contract in the live-log architecture.
Requests borrow their values for immediate fixed-buffer copying; exposing
read_limit avoids guessing adapter capacity. The model's generation start
explicitly controls BOM admission at a nonzero source offset. The follower
verifies a bounded overlap, drains old objects by quota, retries through supplied
monotonic time, and reports known skipped bytes separately from uncertain
continuity. No filesystem, clock or locale access enters widgets.

## D-141 — Opt-in bounded frame delivery for live monitors

**Status:** accepted, 2026-10-13, under the owner's WP-73…WP-84 delivery request.

Application exposes Adaptive (existing behavior) and Bounded delivery policies.
Bounded enables completion tracking and holds all subsequent frames, including
small text, behind one outstanding question. Before first proof, the existing
nonresponder probe fallback still applies. After a host answers, lateness never
retires its debt or admits another frame; input/timers and latest dirty state
continue independently. Reuse Presenter and the single terminal writer.

This closes the log monitor's small-frame overload path without changing the
default policy of other clients. Native synchronous write semantics are retained;
terminal-limited visibility is reported separately from processing/input latency.
Authority and executable work/queue/frame budgets are in the
[live-log contracts](plans/live-log-architecture.md), WP-79 and WP-84.

## D-142 — Private continuous process streams

**Status:** accepted, 2026-10-13, for WP-80 under the owner's delivery request.

Core exposes an injected process-stream service with separate bounded stdout/
stderr mailboxes, explicit source epochs, cancellation, native error and full
root exit observations. term owns prestarted workers and their native wake
transports. Source selection may allocate bounded request/queue storage; start,
status and cancellation perform no process launch, pipe data read or worker join
in UI dispatch. Session retirement cancels without joining. Service shutdown
joins outside dispatch. Inject a thread-safe monotonic clock and work-wake target.

Reuse ProcessLaunchSpec, native launch preparation and the private pipe pump
with finite ProcessRunner. Each output endpoint keeps at most one native chunk
when its mailbox is backpressured; the other stream still progresses. Cancel
and capacity wakes join the native wait. Quiet streams have no idle timeout;
native root readiness avoids periodic polling where available, with explicit
portable fallback. All helper output stays private. The stream service always
terminates its owned process group/job, including descendants, on completion
or cancellation. A root observation never waits for descendant EOF. Cross-
stream arrival order is not producer causal order. POSIX group ownership is
not a sandbox against a helper deliberately escaping that group.

The [live-log architecture](plans/live-log-architecture.md) fixes limits,
resource ownership and test obligations. No dependency is added.

## D-143 — Log monitor composition and bounded snapshots

**Status:** accepted, 2026-10-13, for WP-81 under the owner's delivery request.

The log-monitor demo composes shipped file/stream/model/filter/view/controller
services. Eight named sources retain independent models, while shared include/
exclude/severity/source controls apply explicit queries to all tabs. Synthetic
samples use supplied time and bounded mailboxes or injected range/identity
observations, including rotation and truncation, without generating disk files.
Native system-source selection is supplied through an application-owned catalog
and bounded feed contract; OS discovery/schema mapping remains WP-82.

Export captures an explicit selected-record or published-filter retained scope,
with source/query revisions and a bounded byte limit. Its source's ingress is
held only during bounded snapshot construction, then resumed before worker I/O.
Other sources continue. Atomic create-only writes run through an injected export
service; existing files are not overwritten implicitly. Details/copy use retained
identities and safe display; raw controls are escaped for inspection. UI teardown
retires source lanes and views before their borrowed models.

TabControl gains owned page removal and optional active-change notification,
so a monitor can retire a source without rebuilding other pages or losing their
anchors. Detached pages have no caption observers left behind. Native services
and export workers are constructed/joined outside UI dispatch. The demo's
public composition and actual view graph are test/golden targets.

## D-144 — Native log catalogs and bounded application adapters

**Status:** accepted, 2026-10-13, under the owner's WP-82 delivery request.

Native catalogs are constructed at the app boundary before UI dispatch. macOS
verifies installed log help advertises NDJSON (normal help exit 0 or Apple
EX_USAGE 64, observed on this host) before enabling an explicit private
`/usr/bin/log stream --style ndjson --level info` invocation. Linux discovers an
absolute journalctl executable and uses bounded recent history plus follow in
one JSON invocation. Neither assumes source permissions; private stderr, native
exit/error and continuity uncertainty remain visible, with File/Sample recovery.

A deterministic, dependency-free parser bounds JSON frame bytes, nesting, keys,
extracted fields and work per turn. Native JSON envelopes are retained as the
record's raw text (including unknown fields/redaction); selected metadata fields
are decoded for inspection/filtering. Source timestamps remain data. Invalid or
oversized envelopes produce explicit diagnostics, never a fabricated event.
Frames split across arbitrary pipe chunks retain bounded staging. Private raw
pipes use backpressure; the structured display queue uses explicit DropOldest
with shipped gap/loss accounting. No hidden durable history is promised.

macOS subscribes to live info/default events; debug narrowing is explicit.
Linux requests the most recent 100 records and hands off to live events within
one journalctl invocation; source cursors are retained metadata. Reopening
starts a new generation and declares possible duplicate/history gaps. Native
Windows pull subscriptions own worker-only Event Log handles, bounded rendered
event storage, channel/provider/event ID/level/time/bookmark metadata and
strict stale-result diagnostics. Initial future-only subscription and retries
declare the absence of replay continuity. An explicit bounded channel/bookmark
option uses strict start-after-bookmark semantics; invalid/stale state fails
visibly. Windows discovers at most eight Admin/Operational channels from a
bounded 256-path enumeration. Eight prestarted workers own bounded rendering
buffers and one publisher-metadata cache each, refreshed by provider change or
subscription retry. No automatic elevation or logging
configuration changes. Worker shutdown occurs outside UI dispatch.

Published installed Apple log documentation, systemd journalctl documentation,
RFC 8259 and Microsoft Windows Event Log API references derive behavior.
Replay tests do not substitute for tagged controlled events on each real OS.

## D-145 — Installed live-log recipes and SDK-only installation

**Status:** accepted, 2026-10-13, under the owner's WP-83 delivery request.

Public-header-only log clients compose shipped models/followers/receivers,
filters/panels/controllers and explicit native services. They own resources and
borrowed views for enclosing lifetimes; no custom tailer, filter engine or queue
pump is required. An installed-client project uses only find_package and the
exported target, with strict warnings and an executable contract.

The Development install component contains the SDK library/headers/package
metadata and public recipes. A normal full install retains its existing scope;
SDK-only installation does not require unrelated example executables. No new
external dependency is introduced. Generated log-monitor SVGs use the same
scenario graph as pinned goldens and verify the exact golden before writing.
Documentation figures remain headless composed-frame evidence, distinct from
actual native-window or physical-visibility acceptance.
