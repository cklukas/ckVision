# ckVision — Corrective Assurance Plan

Established 2026-08-08 after a critical review of the planning documents,
current implementation, and verification system. This is the authoritative
execution queue for correcting acceptance gaps in M0–M10.

The earlier work-package ledger in
[api-ergonomics-review-2.md](api-ergonomics-review-2.md) records what landed at
the time. It is historical evidence, not current completion status. Where this
plan reopens an earlier package, the corrective package below owns the whole
repair and its literal acceptance criteria.

## How to choose the next work package

The instruction **“work on the next open WP”** has exactly one meaning:

1. Open this file and use the **Current execution queue** below.
2. If a row is `In progress`, continue the first such row.
3. Otherwise take the first row marked `Ready`.
4. Never select work from a legacy status ledger, milestone prose, issue list,
   or a numerically earlier `WP-*` merely because it says `Done`, `Open`, or
   `In progress` there.
5. When a package's implementation and package-specific evidence land, update
   this queue, its reopened-WP mapping, the ROADMAP acceptance table, and every
   affected catalog ledger in the same change. Use `Implementation complete —
   gate pending` until the global matrix is proven, then mark the next row
   `Ready`.

There may be at most one `In progress` row. `Blocked` always names its
prerequisite. `Implementation complete — gate pending` means the literal
package criteria hold but a later cross-platform/global audit still prevents a
`Done` claim; it is not selected as the next WP. WP-38 either promotes such a
row to `Done` with evidence or returns it to `Ready` with the failed criterion.
`Done` means the criteria hold literally, all global verification gates are
recorded, and no known contradiction remains. Partial code, substituted tests,
an owner-approved compromise, or a green local build is not `Done`. A later
defect or missing gate reopens the owning package under D-034.
For queue sequencing, `Implementation complete — gate pending` satisfies a
dependent row's package prerequisite; `Blocked` does not.

## Current execution queue

| Order | Package | Estimate | State | Prerequisite | Reopens / closes |
|---:|---|---:|---|---|---|
| 1 | WP-23 — Provenance containment and authority repair | 1–2 pw | **Implementation complete — gate pending** | — | D-012; contradictory status and hello gates |
| 2 | WP-24 — Root layout and resize correctness | 0.5–1 pw | **Implementation complete — gate pending** | WP-23 | WP-1; M4/M8 resize exit |
| 3 | WP-25 — Lifetime-safe pointer input and activation | 1–2 pw | **Ready** | WP-24 | WP-2, WP-3; VISION click routing |
| 4 | WP-26 — Desktop/window ownership and dialog attachment | 1–2 pw | Blocked | WP-25 | WP-4, WP-5 |
| 5 | WP-27 — Run loop, terminal session, wake, modality, and blocking API | 4–6 pw | Blocked | WP-26 | WP-14, WP-15; D-008/D-021/D-024; M3 lifecycle |
| 6 | WP-28 — Platform-service layer correction | 1–2 pw | Blocked | WP-27 | D-025 and ARCHITECTURE §1 contradiction |
| 7 | WP-29 — Hostile-paste threat contract and hardening | 1–2 pw | Blocked | WP-28 | D-023; M3 security exit |
| 8 | WP-30 — Unicode conformance and grapheme-safe clipping | 3–5 pw | Blocked | WP-29 | M1; cross-widget Unicode acceptance |
| 9 | WP-31 — Damage, paint provenance, backing stores, and enforceable budgets | 4–6 pw | Blocked | WP-30 | D-010/D-036/D-037; M2/M4/M5 scene and performance exits |
| 10 | WP-32 — Portable, isolated test and CI infrastructure | 2–3 pw | Blocked | WP-31 | M0/M3/M4 cross-platform gates |
| 11 | WP-32A — Dual-profile graphics virtual terminal and visual captures | 2–4 pw | Blocked (partial landing recorded below) | WP-32 | M3 headless/raster acceptance; screenshot gap |
| 12 | WP-33 — Fuzzing and sanitizer program | 1–2 pw | Blocked | WP-32A | M3 fuzz exit; verification standard |
| 13 | WP-34A — Chrome, menus, and standard-dialog catalog closure | 2–3 pw | Blocked | WP-33 | M5/M6 catalog gaps |
| 14 | WP-34B — Controls, editing, selection, and scrolling closure | 3–4 pw | Blocked | WP-34A | M4/M6 control gaps |
| 15 | WP-34C — Data, media, help, file, and catalog closure | 3–5 pw | Blocked | WP-34B | M6 remaining catalog gaps |
| 16 | WP-35 — Theme, command, and example ergonomics closure | 3–4 pw | Blocked | WP-34C | WP-6…WP-13, WP-17 and unfinished WP-10 |
| 17 | WP-36 — Layout, dialog, and tree literal acceptance | 2–3 pw | Blocked | WP-35 | WP-16, WP-18…WP-22; M10 exit |
| 18 | WP-37 — Windows backend and three-platform matrix | 2–4 pw | Blocked | WP-36 and Windows host | M7; Windows portions of global gates |
| 19 | WP-38 — Golden, script, reference, and release-gate closure | 3–5 pw | Blocked | WP-37 | M0–M10 global gates |

Corrective work is currently estimated at roughly **40–65 focused
person-weeks**. These are planning ranges, not completion evidence. WP-23 must
re-estimate later rows if its authority/provenance audit invalidates more
implementation than is currently known; that update changes the estimates,
never the acceptance criteria silently.

The dependency order deliberately puts correctness, lifetime, security, and
architecture before convenience work. A package may be split into reviewable
commits, but its queue row stays `In progress` until every acceptance item is
met. New findings discovered while implementing a package are added to that
package unless they require a new architectural decision; in that case add a
new queued package and an explicit prerequisite rather than silently deferring
the work.

## Verification policy for every corrective package

Every package supplies evidence at the layer where its claim is observable:

- **Regression:** a named test must fail against the pre-fix behavior (or the
  landing record must explain why safely reproducing it is impossible) and pass
  after the design-level fix.
- **Unit/property:** pure algorithms, state transitions, lifetimes, and negative
  contracts are tested without a terminal or wall clock.
- **Public-path script:** user-visible behavior enters through a Terminal,
  `Application::step`, or the documented public host API. Direct handler calls
  cannot substitute for an end-to-end criterion.
- **Golden:** cell appearance is a byte-exact styled-cell frame; raster scene
  semantics use symbolic raster records; protocol presentation uses a pixel-
  plane capture decoded from the actual emitted terminal bytes. A Rect,
  substring, smoke, decorative screenshot, or “did not crash” assertion cannot
  substitute. Golden regeneration must include a reviewed semantic diff.
- **Adversarial:** destruction/reentrancy, malformed input, shrink boundaries,
  capability degradation, allocation counters, and relevant sanitizer/fuzz
  corpora accompany the happy path.
- **Portable:** platform-independent contracts run on every OS; platform-only
  omissions are labelled and linked to equivalent headless coverage plus their
  owning platform package.

Each landing records test names and counts by unit/script/golden/fuzz/benchmark
category, sanitizer configurations, platforms actually run, and any remaining
global gate. “All tests pass” without that inventory is insufficient evidence.

## Reopened legacy work packages

This table is the current interpretation of WP-1…WP-22. It overrides the
historical status labels in `api-ergonomics-review-2.md`.

| Legacy package | Current acceptance status | Corrective owner |
|---|---|---|
| WP-1 | Reopened — root fill used a replaceable public callback, add-time fill and the specified end-to-end resize golden are absent | WP-24 |
| WP-2 | Reopened — post-handler focus logic can dereference a destroyed target; click-then-type integration is incomplete | WP-25 |
| WP-3 | Reopened — the intended buried, overlapping-window activation and active-style golden are absent | WP-25 |
| WP-4 | Reopened — a detached Window can retain a Desktop-capturing zoom provider; mouse-to-zoom end-to-end coverage is absent | WP-26 |
| WP-5 | Reopened — public attachment paths can still bypass Desktop bookkeeping; the required cycling/appearance scenario is absent | WP-26 |
| WP-6 | Reopened — example hygiene has no durable source check and depends on unresolved M8 behavior | WP-35 |
| WP-7 | Reopened — per-window theme override is absent and Desktop still takes theme plumbing contrary to D-028 | WP-35 |
| WP-8 | Reopened — role additions landed, but the specified scripted runtime theme mutation was not accepted | WP-35 |
| WP-9 | Reopened — the zero-`static_cast<widgets::` example criterion is not met | WP-35 |
| WP-10 | Transferred — metadata/handler work landed; live menu/status structure, hints, enablement, and retraction remain | WP-35 |
| WP-11 | Revalidation required — registry-rendered labels/chords must pass the final command and golden matrix | WP-35, WP-38 |
| WP-12 | Revalidation required — standard commands must compose with modal routing and complete documentation | WP-27, WP-35 |
| WP-13 | Reopened — named command contexts remain incomplete and default traversal fails inside the current modal boundary | WP-27, WP-35 |
| WP-14 | Reopened — `run()` uses unsanctioned process-global Application state and wake cannot interrupt an in-flight wait | WP-27 |
| WP-15 | Reopened — current execution is re-entrant, modal routing/focus lifetimes are incomplete, and dialog/file-dialog blocking conveniences are absent | WP-27 |
| WP-16 | Reopened — height-for-width propagation exists but containers do not make wrapped height visibly effective | WP-36 |
| WP-17 | Reopened — the former provenance and line-limit contradiction were repaired by WP-23; remaining example integration acceptance is owned by WP-35 | WP-35 |
| WP-18 | Reopened — the required resized-dialog interaction and appearance golden were replaced by narrower geometry assertions | WP-36 |
| WP-19 | Reopened — container implementation requires literal scripted/golden/reference acceptance | WP-36 |
| WP-20 | Reopened — overlay coexistence/resize/size-hint behavior requires the specified end-to-end appearance evidence | WP-36 |
| WP-21 | Reopened — the shrink-storm/recovery script and complete dialog-default application remain to be demonstrated | WP-36 |
| WP-22 | Reopened — lazy filesystem behavior, Splitter use, and unchanged-observable-behavior evidence require the specified integrated acceptance | WP-36 |

## Corrective work packages

### WP-23 — Provenance containment and authority repair

**Purpose.** Restore trust in the project's authority chain before using any
possibly tainted behavior as a specification.

**Work.**

- Treat discovered source-derived claims and the contradictory Hello budget as
  D-012 incidents. Do not open or consult prohibited source while investigating.
- Inventory ckVision-owned artifacts whose rationale explicitly depends on
  prohibited source access. Quarantine or replace affected material wholesale;
  independently re-derive retained behavior from VISION, ARCHITECTURE,
  published standards, ckVision-authored goldens, and documented observation.
- Add a repository provenance check and a mandatory review checklist that
  reject claims attributing behavior or wording to a prior implementation.
  The automated check supplements human review; it does not claim to prove
  provenance.
- Resolve every contradictory status or acceptance statement found by this
  review. In particular, keep the ROADMAP's 60-line hello gate unless a new
  decision explicitly changes it, then make ROADMAP, plan, script, tests, and
  docs agree in one change. An entry in a historical landing note is not such
  a decision.
- Verify every planning/documentation link and remove references to nonexistent
  tests. Preserve useful landing evidence, but label it historical.

**Acceptance.**

- No current source, tool, plan, or public document attributes behavior or
  wording to prohibited source.
- A clean-room re-derivation record names the approved specification for every
  affected ckVision behavior and the tests that pin it.
- The provenance checker is a mandatory CTest/CI gate and has positive and
  negative self-tests.
- One hello line limit appears consistently in every authoritative document
  and executable gate; the current default is 60 until formally changed.
- ROADMAP, README, this queue, the legacy-plan banner, and widget ledger expose
  no conflicting current completion claims.

**Implementation complete — gate pending (2026-08-08).** The working tree no
longer contains the affected conversion material or source-fidelity claims.
`docs/hello-example.md` and `plans/clean-room-derivation.md` establish the
ckVision-owned behavior contract and approved evidence for Hello. The
`provenance_check` CTest gate scans tracked/non-ignored text artifacts, and
`provenance_checker_self_test` demonstrates both rejection and acceptance
cases. `hello_line_budget` enforces 60 lines and currently reports 58. Release
and ASan/UBSan CTest runs are green. WP-23 cannot become `Done` until WP-38
records the cross-platform/global gates; its package prerequisite is satisfied,
so WP-24 is now the next ready work package.

### WP-24 — Root layout and resize correctness

**Purpose.** Make root sizing a framework invariant rather than a convention
implemented through a user-replaceable callback.

**Work.**

- Give Application an internal, non-replaceable root-layout path. Public bounds
  observation remains independently settable and cannot disable framework
  layout.
- Apply the root fill policy both when a direct child is attached and whenever
  terminal bounds change. Define and test opt-out behavior explicitly.
- Drive resize exclusively through `HeadlessTerminal` input and public
  `Application::step`; do not call `dispatch` directly in the acceptance test.
- Cover grow and shrink, docked chrome, ordinary/zoomed/`KeepFilling` windows,
  reachability, and stale-cell removal.

**Acceptance.**

- Replacing the public bounds observer cannot alter root-child layout.
- A child added after the last resize is correct before the next presented
  frame.
- A multi-step headless event script captures a byte-exact golden after every
  resize step and proves all M8 resize exit statements.
- The real test paths are cited from ROADMAP and example documentation.

**Implementation complete — gate pending (2026-08-08).** Application now owns
the root bounds-change and direct-child-attachment layout hooks privately;
`View::on_bounds_changed` remains a public observer and cannot replace or
override that layout. `tests/test_m8_integration.cpp` drives terminal resizes
through `HeadlessTerminal::resize` and `Application::step`, including opt-out
semantics and observer replacement. `tests/test_root_resize_golden.cpp` drives
a grow/shrink desktop script through the same path, checks docked chrome,
ordinary, zoomed, and `KeepFilling` windows plus reachability and current-frame
size, and pins `root_resize_grow.dump` and `root_resize_shrink.dump`. The
manual `generate_root_resize_goldens` tool regenerates those fixtures for
review. Release and ASan/UBSan runs passed all 8 CTest gates (including the
1206-test `cvision_tests` executable) on 2026-08-08. WP-24 remains gate
pending until WP-38 records the complete cross-platform/global matrix. Its
package prerequisite is satisfied, so WP-25 is the next ready work package.

### WP-25 — Lifetime-safe pointer input and activation

**Purpose.** Preserve click-to-focus/activate without dereferencing event
targets that handlers synchronously detach, destroy, or reparent.

**Work.**

- Define event-phase lifetime rules for target delivery, ancestor capture, and
  post-delivery focus. No phase may retain an unchecked raw target across a
  user callback.
- Make destruction, detachment, and reparenting from mouse handlers safe and
  deterministic. Re-hit-testing or a validated attachment identity may be
  used; moving focus before delivery is allowed only if menu/popup semantics
  and D-029 remain correct.
- Exercise activation through Application dispatch on two genuinely
  overlapping windows, including a click on a descendant in the buried one,
  raise/restyle, and subsequent typed input routed to the clicked control.

**Acceptance.**

- ASan/UBSan regressions cover self-destruction, ancestor destruction,
  detach/reparent, and popup dismissal during mouse delivery with no stale
  dereference or focus pointer.
- The overlapping-window interaction has event-script coverage and a golden
  showing active/inactive frame roles before and after the click.
- Click-to-focus and click-to-activate remain overridable and keyboard focus is
  unchanged when the click target is not focusable.

### WP-26 — Desktop/window ownership and dialog attachment

**Purpose.** Make every public attachment/detachment path maintain one coherent
Desktop model and eliminate callbacks that outlive the Desktop.

**Work.**

- Replace or lifecycle-bind `zoom_area_provider`; detaching either side clears
  the relationship before user code can observe it.
- Make it impossible for a publicly attached direct Window child to exist
  outside Desktop's window order, activation, cycling, grow policy, and modal
  bookkeeping. Either constrain the public API or make generic attachment
  correct by construction; documentation alone is insufficient.
- Apply the same invariant to all standard dialog handles. Keep
  `show_dialog` as the convenient sanctioned API, not the only path that happens
  to be correct.
- Test the zoom glyph through terminal mouse input and Application dispatch,
  not by invoking Window handlers directly.

**Acceptance.**

- Calling any Window API after it or its former Desktop is detached cannot
  reach a dangling Desktop capture.
- Every public attach/remove/close path preserves identical tree, z-order,
  active-window, focus, and modal state.
- A message box is visibly active over the desktop, appears in the defined
  cycling behavior, closes safely, and restores prior state; script and golden
  evidence cover the full cycle.
- Zoom/restore follows an injected mouse click and remains correct after a
  desktop resize.

### WP-27 — Run loop, terminal session, wake, modality, and blocking API

**Purpose.** Complete D-021 without global Application state, nested modal
pumps, background event leakage, or stale modal/focus pointers.

**Work.**

- Remove the process-global `Application*`. Route terminal/session signals only
  through D-024's async-signal-safe session facility or an explicitly injected
  host policy. Multiple Applications on distinct terminals must coexist.
- Make signal/session installation exception-safe and prove terminal restore on
  every D-011 path. Use only operations whose async-signal safety is justified.
- Complete M3's terminal lifecycle around that loop: baseline-first-frame,
  injected-clock probe deadlines, capability-change events, pixel-metric and
  width re-probes on resize, record/replay fidelity, the complete degradation
  profile matrix, suspend/resume state re-establishment, and byte-identical
  frame re-presentation. A small collection of direct POSIX tests is not a
  substitute for the milestone's PTY/session contract.
- Implement a real waitable wake mechanism that interrupts a currently blocked
  terminal wait. Document the public host-integration contract.
- Route pointer, key, text, default-keymap commands, help, capture, and focus
  traversal within the top modal scope. Never start routing from a focused view
  outside that scope. Nested modal scopes must be deterministic.
- Remove modal/focus entries on detach/destruction and restore the saved focus
  only if it is still attached, visible, enabled, and in the newly active scope.
- A blocking `exec_*` may drive the loop only when no loop/dispatch is already
  active. Calling it from a handler must not create a nested pump; offer the
  callback/non-blocking path there. Complete Window, descriptor dialog,
  message-box, file-dialog, and directory-picker conveniences consistently.
- Complete vetoable close and quit sweep behavior under modal and non-modal
  operation.

**Acceptance.**

- Tab, Shift-Tab, F1, local accelerators, text entry, and mouse input work
  inside an ordinary modal dialog while every background target and command is
  excluded.
- Scripts cover nested modals, destruction/detach at each event phase, focus
  restoration, close veto, and quit sweep.
- Blocking APIs reject re-entrant use deterministically and are tested in their
  supported outer-loop context; examples use non-blocking completion from
  handlers.
- Two Applications run on separate PTYs in one process; SIGINT/resize/wake for
  one cannot mutate or stop the other except according to documented host
  policy.
- Wake latency, in-flight interruption, exception restoration, and fatal-signal
  restoration are automated under the applicable sanitizers.
- Headless and PTY scripts cover capability success/non-response/change,
  tmux/screen/Linux-console degradation, suspend/resume, and record/replay; the
  restored session and re-presented frame meet M3's literal byte guarantees.

### WP-28 — Platform-service layer correction

**Purpose.** Resolve the current impossibility between ARCHITECTURE §1 and
D-025 before adding more filesystem or clipboard code.

**Work.**

- Record a new decision that supersedes the inconsistent part of D-025. Choose
  one dependency-valid model: lower-layer platform-service interfaces with
  explicit upper-layer consumption, or application-owned adapters that do not
  require `term` to depend on `ui`. Do not bless the current accidental
  placement merely because it compiles.
- Update the architecture dependency graph, include-direction checker, public
  headers, factories, scripted implementations, and platform backends together.
- Cover filesystem, system clipboard, clock, wait/wake, and future injected
  services with one consistent ownership rule. Keep core/scene/ui/widgets free
  of direct environment access and mandatory dependencies.

**Acceptance.**

- The decision, architecture graph, actual include graph, and public namespaces
  agree; all forbidden reverse-dependency fixtures fail the layer checker.
- Platform implementations are constructed explicitly and can be replaced by
  deterministic in-memory implementations without filesystem/environment/
  clock access in pure layers.
- All public service lifetimes are instance-owned and safe for two Applications.

### WP-29 — Hostile-paste threat contract and hardening

**Purpose.** Replace the currently false embedded-terminator guarantee with a
protocol-honest, testable security boundary that cannot silently turn paste
bytes into application commands.

**Work.**

- Write the byte-stream threat model, including the fact that an in-band paste
  terminator can be indistinguishable from the terminal's closing delimiter.
  Supersede D-023 where its guarantee is impossible; never retain impossible
  wording as an aspiration.
- Define decoder/Application behavior for early terminators, nested delimiters,
  trailing bytes, timeouts, terminal disconnect, and malformed UTF-8. Preserve
  useful input without allowing a claimed paste payload to synthesize command
  execution silently.
- Keep C0/ESC/C1 and OSC safety at all text/output boundaries and add semantic
  tests that observe command handlers, not only decoded event types.

**Acceptance.**

- A new decision states the exact trust boundary and attainable guarantee; the
  architecture, decoder docs, and public API say the same thing.
- Adversarial streams containing embedded begin/end delimiters, command chords,
  OSC terminators, fragmentation at every byte, and disconnects cannot execute
  a command contrary to that guarantee.
- Golden tests cover sanitized payloads and user-visible recovery; the decoder
  fuzz target consumes the same semantic assertions.

### WP-30 — Unicode conformance and grapheme-safe clipping

**Purpose.** Replace hand-curated partial coverage with a versioned,
standards-derived text engine and prevent widgets from clipping UTF-8 by byte.

**Work.**

- Generate checked-in compiled tables from a named Unicode release using only
  published Unicode data and reports. Record generator provenance, input hashes,
  regeneration steps, and the policy for terminal-divergent sequences.
- Run the official grapheme-break conformance corpus plus project width cases
  for combining marks, spacing marks, Arabic/Hebrew/South and Southeast Asian
  scripts, rare wide scripts, emoji/ZWJ/VS, malformed UTF-8, and ambiguous width.
- Introduce one grapheme/column-aware elision and clipping primitive and migrate
  every text-emitting widget. Button and Window title clipping are mandatory
  regressions; audit menus, labels, inputs, tables, trees, help, and status text
  rather than fixing only the two known sites.

**Acceptance.**

- The pinned conformance corpus passes for the declared Unicode version, with
  every intentional deviation documented and tested.
- No widget truncates or indexes display text by UTF-8 byte when the operation
  is defined in cells or graphemes.
- Goldens cover clipping at every boundary of multi-byte, combining, wide, and
  emoji clusters in all chrome/text widget families.
- `docs/text-width.md` describes complete generated coverage accurately and
  contains no hand-curated-completeness claim.

### WP-31 — Damage, paint provenance, backing stores, and enforceable budgets

**Purpose.** Make D-010 executable correctness rather than architectural prose,
while preserving D-036's line ownership through retained repaint and
composition.

**Implementation checkpoint (2026-08-08; package remains blocked, not
complete).** The immediate cross-window junction defect has a design-level
repair: `Surface` stores connector provenance, `Painter` scopes it, and every
view paint is isolated from siblings. The same compact metadata now records
binary shadow coverage: a content write clears it and `apply_shadow` transforms
the current cell only on its first covering shadow. The retained Compositor
likewise resolves any number of overlapping casters as one boolean. Named unit
regressions cover isolated replacement, inherited translation/clipping,
ordinary-write clearing, style-transform preservation, non-overlapping
footprint geometry, a single caster's corner, and two-caster overlap through
both Compositor and Desktop. The full overlapping-window frame is pinned by
`golden/window_z_order_junction.dump`, and generated Gallery captures exercise
the same overlap visually. This does not satisfy WP-31: the retained
per-window backing-store design, damage-scoped traversal, counters, budgets,
portable/sanitizer matrix, and post-backing-store revalidation below remain
open.

**Work.**

- Implement damage-scoped traversal, per-window backing stores, composition-
  only window movement, resize confinement to the affected subtree, and
  presenter reuse without steady-state heap allocation.
- Carry D-036 junction provenance correctly through partial repaint, retained
  backing stores, window movement, shadow style transforms, clipping, and
  compositor handoff. Never infer a merge from the destination cell's Unicode
  glyph. Re-run the same-view and cross-z overlap regressions after the
  backing-store migration; do not treat today's single-surface pass as final
  evidence for the new retained path.
- Preserve D-037's binary shadow union through the same migration. One caster's
  right/bottom corner, multiple window/menu casters, partial damage, moves,
  resize, and content repainted above a prior shadow must never compound or
  retain stale coverage.
- Remove per-event/per-frame temporary allocations in dispatch, focus
  traversal, posted-work draining, and paint/present paths or provision bounded
  instance-owned storage with explicit overflow behavior.
- Add counters for allocations, content repaint calls, cells touched/composed,
  and bytes emitted. Benchmark programs must compare them with checked-in
  budgets and exit nonzero on regression; printing a target is not a gate.
- Define the pinned reference host and p99 event-to-bytes procedure separately
  from machine-independent CI counters.

**Acceptance.**

- Steady-state dispatch and unchanged-frame presentation allocate zero bytes.
- A move drag causes zero content repaints; resize repaint is confined to the
  resized window subtree; automated counters prove both.
- Same-view dividers still form T/cross junctions, while foreground window,
  popup, overlay, and frame-overlay borders crossing unrelated lower-z lines
  remain byte-exact foreground glyphs and styles across full repaint, partial
  damage repaint, move, resize, and shadow passes. Named cell assertions and
  full-frame goldens run on all target platforms and under sanitizers.
- Shadow footprints are non-overlapping, and every cell under one or more
  window/menu shadows has the exact once-transformed style across the same
  full/partial/move/resize matrix. Goldens contain no second dimmed style for
  footprint corners or cross-caster intersections.
- Every ARCHITECTURE §8 machine-independent budget is a failing CI assertion.
  Pinned-host p99 results and ratcheted thresholds are recorded reproducibly.
- ASan/UBSan and TSan remain clean under damage, post/wake, resize, and rapid
  open/close stress.

### WP-32 — Portable, isolated test and CI infrastructure

**Purpose.** Make “green on macOS, Linux, and Windows” real and make failures
locatable instead of hiding all cases in one monolithic process.

**Work.**

- Remove unconditional POSIX headers and `fork`/`wait` use from tests compiled
  on Windows. Provide a portable child/death-test facility or build narrowly
  labelled platform suites only where the exercised API exists.
- Register independently runnable test cases or small suites with CTest labels,
  filters, and bounded timeouts. A single crash/hang must identify the case and
  not suppress unrelated results.
- Run Release warnings-as-errors, Debug ASan/UBSan, and TSan where supported;
  make unsupported combinations explicit rather than silently absent.
- Make script, line-budget, layer, provenance, golden, PTY/ConPTY, and benchmark
  gates visible as separately named CI checks.

**Acceptance.**

- The complete test target compiles and runs on current macOS, Linux, and
  Windows CI; platform-only tests are explicitly labelled with equivalent
  contract coverage where required.
- Every test has a timeout and can be run/reported independently enough to name
  the failing case after a crash or hang.
- TSan exercises `post`, `wake`, terminal/session signal crossings, and two
  Applications with no race findings.
- CI publishes counts by unit, script, golden, fuzz-corpus, sanitizer, and
  benchmark category.

### WP-32A — Dual-profile graphics virtual terminal and visual captures

**Purpose.** Make Sixel-capable and no-graphics presentation equally
simulatable and visually testable. The current HeadlessTerminal can toggle the
capability and retain raw bytes, but documentation screenshots render only
`Application::current_frame()`'s cell plane; they never consume or display the
Sixel bytes Presenter emitted.

**Design.** Preserve two independent oracles rather than letting one internal
Image object prove itself:

1. Scene/compositor goldens keep their diff-friendly symbolic raster records
   (anchor, visible slice, pixel extent, content hash, fallback state).
2. A deterministic virtual display consumes the exact bytes passed to
   `Terminal::write`, independently parses the VT subset Presenter emits, and
   decodes Sixel DCS data into an RGBA pixel plane positioned over its cell
   plane. Visual goldens and generated screenshots come from that virtual
   display, not directly from `Image`, `Surface`, or `RasterSlice` objects.

**Implementation checkpoint (2026-08-08; package remains blocked, not
complete).** The initial protocol-display foundation has landed:

- `VirtualDisplay` incrementally parses the exact Presenter output subset,
  independently decodes the published Sixel primitives into a clipped RGBA
  plane, and rejects unsupported output with bounded buffers/repeats.
- `HeadlessTerminal` now retains both raw bytes and the decoded display, exposes
  fixed-metrics `headless_sixel_profile()` / `headless_no_graphics_profile()`
  profiles, and scripts real capability-change events.
- Gallery tests cover decoded pixels, fallback-only presentation, fragmented
  terminal writes, and Sixel → NoGraphics → Sixel restoration. CTest's
  separately labelled `gallery_visual_capture` produces inspectable SVG
  artifacts under `build/test-artifacts/gallery/`.
- Documentation captures render the decoded cell + raster planes and include
  paired `gallery-initial.svg` / `gallery-no-graphics.svg` images.
- Recognized box-drawing glyphs in SVG/PDF captures use deterministic
  cell-edge geometry rather than host-font glyph metrics, so frame corners and
  adjoining rules remain connected across HTML/PDF renderers.
- Active Sixel slices now suppress fallback glyphs in favor of
  style-preserving blank backing cells; the encoder explicitly selects opaque
  background mode and exact extents, and SVG/PDF captures embed the decoded
  raster as one lossless bitmap to avoid per-pixel antialiasing seams.

The following work is still open and must not be inferred complete from those
initial green tests:

- implement and test the remaining terminal state model named below: erase and
  scroll controls, synchronized-frame visibility, complete wide-cell overwrite
  behavior, raster-over-raster overwrite/clearing, resize semantics, and every
  Presenter output form;
- add byte-exact or hash-pinned pixel-plane goldens (the current generated SVGs
  are inspectable test artifacts, not accepted golden baselines) for slice
  cropping, partial occlusion, move/resize/scroll, off-screen clipping,
  multiple rasters, and capability transitions;
- add malformed/truncated/control-limit fixtures beyond the initial negative
  cases, then fuzz the incremental virtual-display parser in WP-33;
- split raw-byte, symbolic-scene, and decoded-visual checks into independently
  reported categories under WP-32, publish their artifact inventory in CI, and
  prove the required macOS/Linux/Windows plus sanitizer matrix;
- review the generated semantic visual diff and only then promote this package
  according to D-034. Until every Acceptance item below holds, keep WP-32A
  `Blocked`/later `In progress`, never `Done` or `Implementation complete —
  gate pending`.

**Work.**

- Define named, immutable test capability profiles at minimum for
  `NoGraphics` and `Sixel` with explicit color depth and fixed cell-pixel
  metrics. Tests must select a profile deliberately; ad-hoc boolean mutation is
  not acceptable for acceptance scenarios.
- Upgrade HeadlessTerminal with a composed virtual-display capture or add a
  test-support VirtualTerminal behind the same `Terminal` seam. It must retain
  raw bytes and feed them into a bounded incremental parser for the exact
  ECMA-48/xterm controls Presenter emits plus the published Sixel grammar.
- Model a cell plane and an RGBA plane with deterministic cell metrics,
  clipping, cursor-addressed Sixel placement, alpha policy, overwriting,
  scrolling, resize, erase, synchronized-frame boundaries, and stale-raster
  removal. Unsupported output must fail the capture explicitly rather than be
  ignored and produce a plausible screenshot.
- Validate the virtual display with small hand-authored published-protocol
  fixtures and negative fixtures, independent of ckVision's encoder. Then use
  encoder-to-virtual-display tests as end-to-end coverage; an encoder and a
  decoder sharing assumptions is not the only oracle.
- Make capability changes scriptable as real `CapabilityChangedEvent`s. Cover
  `NoGraphics → Sixel → NoGraphics` re-presentation, including fallback
  visibility and removal of pixels from the previous capability mode.
- Replace cell-only documentation screenshot capture with deterministic SVG
  output from the virtual display. The SVG must include decoded raster pixels
  (without a new mandatory dependency) and the existing styled-cell content.
  Produce clearly labelled Sixel and no-graphics Gallery captures from the
  same application state and interaction script.
- Add pixel-plane golden fixtures for full image presentation, proportional
  slice cropping, partial occlusion by higher windows, move/resize/scroll,
  off-screen clipping, multiple rasters, capability changes, malformed/truncated
  Sixel, and fallback-only presentation.

**Acceptance.**

- The same Gallery script under `Sixel` and `NoGraphics` produces two
  deterministic visual goldens: the first visibly contains the decoded gradient
  at the correct position and occlusion; the second contains the documented
  cell fallback and no raster pixels.
- The Sixel visual capture demonstrably depends on bytes written through
  `Terminal::write`: changing/corrupting the stream changes or rejects the
  capture even when the source `Image` object is unchanged.
- Raw-byte Presenter assertions, symbolic raster goldens, and decoded visual
  goldens all remain separate named test categories and fail independently.
- Runtime capability toggling leaves neither stale raster pixels nor a hidden
  fallback, and triggers the defined full re-present.
- Generated documentation embeds both capability scenarios and no longer calls
  a cell-only `FrameView` rendering a Sixel screenshot.
- The parser is bounded under malformed input and is included in WP-33's fuzz
  corpus; its supported control/Sixel subset is reference-documented.

### WP-33 — Fuzzing and sanitizer program

**Purpose.** Implement the fuzz gates repeatedly claimed by the roadmap and
engineering standard.

**Work.**

- Add buildable fuzz targets for input decoding/paste state, UTF-8 grapheme and
  clipping, golden parsing/round-trip, OSC emission, the WP-32A virtual-display
  VT/Sixel decoder, and any stateful parser identified by the threat model.
- Seed checked-in corpora with every parser/security regression and boundary
  case. Add structure-aware generators where raw bytes cannot reach meaningful
  states.
- Run a bounded corpus in ordinary CI and scheduled/owner-run extended jobs
  under ASan/UBSan; make crash minimization and regression promotion documented.

**Acceptance.**

- Every target builds with the documented compiler configuration and its CI
  corpus completes with zero findings under sanitizers.
- Decoder fuzzing asserts semantic invariants: no unexpected command execution,
  bounded memory/work, valid event sequencing, and safe recovery after malformed
  input.
- Every discovered issue becomes a minimal permanent corpus entry and a named
  unit/golden regression where applicable.

### WP-34A — Chrome, menus, and standard-dialog catalog closure

**Purpose.** Close the remaining M5/M6 catalog behavior above the control
layer, after lifetime/modality/performance foundations are correct.

**Work.**

- Complete nested and checkable dropdowns, keyboard context-menu invocation,
  narrow-width menu overflow and status-line priority layout. Live structural
  mutation and command-driven content remain WP-35's separate acceptance.
- Complete Desktop snapshot/restore, frame-set/maximize promises that remain in
  0.1 scope, message-box severity roles, and the localization string table.
- Complete standard-dialog catalog behavior not owned by the modal engine:
  Window-list type-ahead/activate/close, Directory-picker selection behavior,
  and consistent default-button/Esc/validation integration for every factory.
- Add focused unit and interaction evidence now; WP-38 owns the exhaustive
  cross-scheme appearance matrix.

**Acceptance.**

- Every Desktop/Window/Dialog/Message-box/Menu/Status catalog cell is either
  implemented with linked unit and interaction evidence or explicitly removed
  from 0.1 by a settled decision.
- Menus are operable keyboard-only and mouse-only, including nested dismissal,
  disabled/check states, context invocation, and narrow widths.
- Snapshot/restore and every standard-dialog close path preserve z-order,
  focus, modality, and veto invariants under sanitizers.

### WP-34B — Controls, editing, selection, and scrolling closure

**Purpose.** Finish the ordinary form/control behaviors that headers and the
catalog currently promise but the implementation does not provide.

**Work.**

- Implement Label buddy mnemonic activation, StaticText alignment, Button
  repeat-on-hold, and complete CheckGroup/RadioGroup mnemonic behavior plus
  tri-state checks.
- Complete InputLine clipboard cut/copy/paste, pointer drag selection, history
  dropdown, and undo; complete Memo clipboard, bounded undo, word-wrap toggle,
  and grapheme navigation using WP-30 primitives.
- Give ListView true injected-Clock double-click semantics and finish its
  baseline search/selection behavior. Implement or formally re-scope its
  virtual/incremental/columned promises.
- Complete ComboBox, TabControl, Progress, Scrollbar auto-hide, and ScrollViewport
  wheel delivery over descendant content. Smooth scroll-region diffs either
  land with WP-31 integration or require a settled 0.1 re-scope.

**Acceptance.**

- Every affected control is keyboard- and mouse-complete, deterministic under
  an injected Clock, and grapheme-correct without host clipboard/clock access.
- Editing regressions cover selection, clipboard, undo/history, validation,
  masking/password modes, focus loss, destruction, and terminal shrink.
- Every Controls catalog cell is implemented with linked unit and interaction
  evidence or explicitly removed from 0.1 by a settled decision.

### WP-34C — Data, media, help, file, and catalog closure

**Purpose.** Close the remaining complex widgets and produce the mechanical
catalog ledger that prevents another silent scope gap.

**Work.**

- Complete Table resize/sort/virtualized/styled-cell behavior and Tree
  virtualization/connector styles, or formally re-scope individual promises.
  Complete Splitter keyboard behavior; WP-36 owns its integrated resize golden.
- Implement TextView links, OSC 8 emission, activation and inline styles; finish
  Help viewer cross-links, back, index/keyword lookup, and injected provider.
- Complete ImageView scale/fallback/occlusion/scroll/dual-space behavior and
  make Canvas derive pixel size automatically from current cell metrics,
  redrawing on terminal/widget metric changes. Exercise both through WP-32A's
  `Sixel` and `NoGraphics` profiles; symbolic raster presence alone is not
  visual acceptance.
- Complete File open/save path completion, filters, recent locations, hidden
  toggle, and cross-platform paths through the WP-28 service abstraction.
- Replace the narrative ledger with a row-by-row traceability matrix: every
  baseline feature, beyond-baseline feature, cross-cutting requirement,
  implementation symbol, unit/script/golden tests, reference page, target WP,
  and current state. Reconcile public header promises too.
- For any beyond-baseline item intentionally moved after 0.1, record a settled
  decision changing ROADMAP/catalog scope. A ledger annotation is insufficient.

**Acceptance.**

- Every catalog cell and cross-cutting bullet is `Done` with linked evidence or
  explicitly out of 0.1 through a settled decision; no header/document promise
  remains untracked.
- File dialogs use only the injected filesystem in tests and cover POSIX,
  Windows, root, relative, inaccessible, empty, and changing-directory cases
  without host filesystem access.
- Media/help widgets meet fallback, link, scrolling, occlusion, pixel-metric,
  and dual-space interaction contracts under the relevant capability profiles.
- ImageView and Canvas have matching scripted symbolic and decoded pixel-plane
  goldens under Sixel plus cell-fallback goldens under no-graphics.
- Gallery inventory and draft public reference pages include every shipped
  widget and standard dialog, ready for WP-38's final matrix audit.

### WP-35 — Theme, command, and example ergonomics closure

**Purpose.** Finish the actual M9/D-028/D-029 contract instead of accepting
documented exceptions to literal criteria.

**Work.**

- Implement per-window theme scope/override and remove `Theme&`/`RoleId`
  plumbing from Desktop and every widget constructor. D-028 has no undocumented
  Desktop exception.
- Complete named command contexts, focus-ancestry and explicit push/pop
  activation, live menu/status structure, safe mutation while open, status
  hints/enablement, command retraction, and declare-once example usage. This
  absorbs the unfinished half of legacy WP-10.
- Make every typed insertion surface return the inserted concrete type,
  including frame overlays, so examples require no `static_cast<widgets::`.
- Add durable source checks for forbidden constructor plumbing, duplicate
  command/chord declarations, casts, redundant focus defaults, bypassed dialog
  attachment, and the agreed hello line budget.
- Rewrite hello and conversion/tutorial material only from ckVision-owned
  requirements after WP-23; handler-triggered dialogs use the non-blocking modal
  API required by WP-27.

**Acceptance.**

- Runtime theme switching and a per-window override are script- and golden-
  tested in Classic, Dark, Light, and Mono without reconstructing widgets.
- Hello and filebrowser declare each command once; menu/status content and
  chords update live after rebinding, context change, enable/disable, add/remove,
  and retraction.
- Named command contexts work under ordinary, popup, and modal focus scopes.
- `examples/` passes automated zero-plumbing, zero-duplicate-chord, and
  zero-`static_cast<widgets::` checks and the authoritative hello line gate.
- The public construction examples meet VISION's one-screen/sane-defaults bar
  without exceptions hidden in status notes.

### WP-36 — Layout, dialog, and tree literal acceptance

**Purpose.** Close M10 against its written exit rather than treating narrower
unit assertions as substitutes for scripts, goldens, and visible behavior.

**Work.**

- Make height-for-width participate in actual Row/Column/Grid/Dock/dialog
  layout so wrapped StaticText changes allocated height and repaints correctly.
- Re-run Anchors, Grid, Dock, Overlay, alignment/margins, Splitter, and plural
  frame-overlay behavior through real Application event scripts and resize
  frames. Preserve pure distribution tests as lower-level evidence only.
- Apply non-resizable-by-default/Esc-cancel/validation rules to every standard
  dialog construction path, not only an otherwise-unused descriptor helper.
- Drive shrink storms across full/degraded/too-small/recovered states and prove
  deterministic focus, chrome, damage, and recovery.
- Exercise filebrowser lazy expansion through the injected filesystem, Splitter
  interaction, TreeNode payloads, callbacks, and navigation while pinning
  observable frames.

**Acceptance.**

- A resized dialog visibly keeps bottom-right anchors, stretches all-edge
  anchors, and reallocates wrapped height; an event script and byte-exact
  goldens cover every step.
- Container, Splitter, overlay, shrink/recovery, and lazy-filebrowser scenarios
  meet the literal M10 exit with scripts for behavior and goldens for appearance.
- Every container/model feature has public reference documentation and a
  gallery/filebrowser demonstration where relevant.

### WP-37 — Windows backend and three-platform matrix

**Purpose.** Put M7 in the same executable queue as every other remaining
obligation so “next open WP” cannot skip the Windows release gate.

**Work.**

- Complete the ConPTY VT backend, VT-first input with the documented console-API
  supplements, native clipboard integration, session restoration, and explicit
  degraded-host capability rows without a parallel legacy renderer.
- Run real Windows Terminal scenarios for keyboard, cell/pixel mouse, resize,
  Sixel/fallback graphics, clipboard, suspend/close restoration where supported,
  and multi-Application isolation. Keep headless equivalents for host features
  CI cannot expose, but do not label them interactive acceptance.
- Make the full macOS/Linux/Windows matrix build warnings-as-errors and prove
  byte-identical platform-independent goldens. Record and test every deliberate
  backend-specific byte difference at the capability/presenter boundary.
- Exercise plain-VT ConPTY degradation and the legacy-console refusal path with
  actionable diagnostics.

**Acceptance.**

- Every literal M7 exit criterion passes on a named Windows host; host identity,
  Windows/Terminal versions, and capability report are recorded.
- Unit, scripted, golden, ConPTY smoke, sanitizer-supported, and benchmark
  suites pass across the three-platform matrix with no unlabelled omission.
- Graphics and pixel-mouse behavior work in Windows Terminal and degrade
  deterministically under the declared no-graphics/plain-VT profiles.
- The queue may remain `Blocked` on an unavailable Windows host, but neither M7
  nor WP-38 may be marked `Done` in its place.

### WP-38 — Golden, script, reference, and release-gate closure

**Purpose.** Independently audit the evidence after corrective implementation
and prevent milestone status from outrunning verification again.

**Work.**

- Generate a traceability report from milestone exit item → corrective WP →
  implementation → unit/script/golden/fuzz/benchmark evidence → public docs.
- Complete the appearance matrix for every widget/chrome element, applicable
  active/inactive/focused/disabled state, all four built-in schemes, wide text,
  terminal shrink, and paired Sixel/no-graphics profiles. Raster-bearing rows
  require both the symbolic scene golden and WP-32A's decoded pixel-plane
  golden; neither substitutes for the other.
- Make gallery, hello, filebrowser, form/dialog, menu-only-keyboard,
  menu-only-mouse, modal, close-veto, restoration, and degradation scenarios
  byte-exact scripted tests rather than smoke-only assertions.
- Write reference documentation for every public API and run a link/symbol
  coverage check. Examples and tutorials must be executable from documented
  prerequisites.
- Run the full engineering-standard gates in order and record counts/results,
  including all OS targets, fuzz corpora, sanitizers, TSan, machine-independent
  budgets, and pinned-host performance.

**Acceptance.**

- The traceability report has no missing, substituted, smoke-only, or
  documentation-free milestone exit item.
- Golden coverage is mechanically enumerable and complete against the widget
  catalog/state/scheme matrix.
- All verification gates pass and failures in any category block completion.
- Only after this package lands may M0–M10 be reconsidered for `Done`;
  each milestone is changed individually only when its own exit row is fully
  supported by the report.

## Review finding traceability

| Finding from the 2026-08-08 critical review | Owning package(s) |
|---|---|
| Prohibited-source comparison/fidelity claims | WP-23 |
| Misleading Done/delivered claims; D-012 provenance repair; nonexistent test citation | WP-23 |
| Root observer collision, late-child sizing, non-end-to-end resize acceptance | WP-24 |
| Post-handler click-focus use-after-free; incomplete overlapping activation | WP-25 |
| Detached zoom callback; bypassable Desktop/dialog bookkeeping | WP-26 |
| Global run instance, non-RAII signal path, non-interrupting wake | WP-27 |
| Modal Tab/help/command/text leakage; stale stack/focus; nested pump; missing `exec_*` | WP-27 |
| D-025 versus layer graph/interface placement contradiction | WP-28 |
| Embedded paste terminator contradicts D-023 and can yield ordinary keys | WP-29, WP-33 |
| Partial Unicode tables and UTF-8 byte clipping | WP-30 |
| Full-tree repaint/temporary allocations; benchmark that cannot fail | WP-31 |
| Windows-incompatible test sources; monolithic runner; no TSan | WP-32 |
| Headless capability flag proves Sixel bytes only; screenshots discard the raster plane and no paired Sixel/fallback visual oracle exists | WP-32A, WP-38 |
| No fuzz targets/corpora despite mandatory gates | WP-33 |
| Silent widget/header/catalog gaps and unrecorded post-0.1 deferrals | WP-34A…WP-34C |
| Missing per-window themes, Desktop plumbing, WP-8 mutation, example cast/checks | WP-35 |
| Named command contexts and incomplete live menu/status WP-10 | WP-35 |
| Height-for-width not consumed; unit assertions substituted for M10 scripts/goldens | WP-36 |
| Windows backend/host and full three-platform matrix remain unaccepted | WP-32, WP-37 |
| Six appearance dumps, smoke-only examples, incomplete public docs and release evidence | WP-38 |
