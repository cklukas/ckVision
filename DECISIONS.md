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

**D-013 — Commands are first-class.**
32-bit `CommandId`: ids below 4096 are reserved for the library's
standard commands; applications own the space above; extension libraries
allocate documented ranges through the registry at registration time.
Metadata (title, category, default chord), scheme-based rebindable
keymaps, and context-scoped enablement — a context is a named scope
declared by application or widget, activated by focus ancestry or
explicit push/pop. Menus and status lines render from the registry. No
numeric limits that force id-space rationing.

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
detail the API never exposes. *Supersedes the earlier "optional gfx
module" shape.*

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
exported waitable handle. Modal dialogs push a modality stack that
scopes event routing and complete via callback/notification; a blocking
convenience exists for applications that own the loop. *Rejected:*
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

**D-023 — Hostile content is neutralized at the boundaries.**
Paste payloads are sanitized in the decoder (C0 except tab/newline
stripped, ESC/C1 stripped, embedded paste terminators neutralized);
displayed strings are control-neutralized at the text-to-Cell boundary;
every OSC emission escapes or rejects terminator bytes. These are fuzz
and golden targets, and M3 exit criteria (ARCHITECTURE §12).

**D-024 — One sanctioned process-level facility: the session registry.**
Fatal-signal restoration and tty-owned runtime signals (SIGWINCH,
SIGTSTP/SIGCONT) cannot be instance members; they are served by a single
async-signal-safe registry of live terminal sessions, each with its
precomputed restore bytes from the session-state ledger. Everything else
remains instance-owned (D-008). One application per terminal; N
applications on N terminals is supported and tested.

**D-025 — Platform impurities are injected interfaces.**
The injectable Clock (D-002 era) is joined by an injectable filesystem
interface (consumed by file dialogs; scripted in-memory implementation
for headless goldens) and the system-clipboard bridge. Interfaces live
in `ui`; platform implementations live in `term`; backends receive the
environment explicitly at construction. `core`/`scene`/`ui`/`widgets`
never touch environment, filesystem, or wall clock directly.

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

## Open

**D-OPEN-1 — License at first public release.**
Sole copyright rests with the project owner, so every option stays
available until release. Candidates: permissive (MIT or Apache-2.0) for
maximum adoption; GPL-3.0-only; or dual licensing. Decide no later than
the 0.1 release gate (ROADMAP M8).

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
