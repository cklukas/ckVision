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
paste work where the host supports them (Windows Terminal does). The
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
appropriate.

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
scroll-delta optimization are post-0.1.** The M6 catalog mixed first-release
control behavior with scale/performance extensions that need separate model
contracts. For 0.1, `ListView`, `Table`, and `TreeView` are materialized
widgets: the application supplies the visible row/tree data as values, the
widget owns deterministic cursor/selection/drawing behavior, and tests cover
keyboard, mouse, Unicode-safe clipping, and injected-clock activation. The
0.1 contract does not include a virtualized row provider, million-row lazy
fetching, columned `ListView` mode, typed/numeric table comparators, or a
cell-level table editor. D-043 promotes the concrete list/table requirements
without changing this release-scope record.

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
provides separate provider contracts for `ListView` and `Table`, rather than
one erased “items” interface. A provider is caller-owned, synchronous for one
requested visible item/cell, and identifies every row/item with a stable,
opaque value. It never owns UI state, starts work, reads a service, or calls a
widget from another thread. An application that loads data asynchronously
updates its own provider and calls the view’s explicit `model_changed()` on the
UI thread (normally through `Application::post()`).

Cursor, primary selection, and multi-selection store provider identities, not
display offsets. Filtering, sorting, paging, and a refreshed backing store
therefore preserve a selection whenever its identity remains present; a
removed identity is cleared deterministically. The view paints and queries
only its visible slice plus the cursor-resolution queries required after a
model change. Ordering belongs to the provider: header activation requests a
sort but the widget never silently materializes or reorders application data.

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

## Open

**D-OPEN-1 — License at first public release.**
Sole copyright rests with the project owner, so every option stays
available until release. Candidates: permissive (MIT or Apache-2.0) for
maximum adoption; GPL-3.0-only; or dual licensing. Decide no later than
the 0.1 release gate (ROADMAP M11).

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
