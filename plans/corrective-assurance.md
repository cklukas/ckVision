# ckVision — Corrective Assurance Plan

Established 2026-08-08 after a critical review of the planning documents,
current implementation, and verification system. This is the authoritative
execution queue for correcting acceptance gaps in M0–M11.

The earlier work-package ledger in
[api-ergonomics-review-2.md](api-ergonomics-review-2.md) records what landed at
the time. It is historical evidence, not current completion status. Where this
plan reopens an earlier package, the corrective package below owns the whole
repair and its literal acceptance criteria.

For current Mac development, the actionable next-package sequence is the
dedicated [Mac-first delivery target](mac-first-delivery-target.md). The queue
below remains the global traceability ledger; it is not a reason to pause Mac
finishing while a deferred external-host row is outstanding.

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
| 3 | WP-25 — Lifetime-safe pointer input and activation | 1–2 pw | **Implementation complete — gate pending** | WP-24 | WP-2, WP-3; VISION click routing |
| 4 | WP-26 — Desktop/window ownership and dialog attachment | 1–2 pw | **Implementation complete — gate pending** | WP-25 | WP-4, WP-5 |
| 5 | WP-27 — Run loop, terminal session, wake, modality, and presentation API | 4–6 pw | **Implementation complete — gate pending** | WP-26 | WP-14, WP-15; D-008/D-021/D-024; M3 lifecycle |
| 6 | WP-28 — Platform-service layer correction | 1–2 pw | **Implementation complete — gate pending** | WP-27 | D-025 and ARCHITECTURE §1 contradiction |
| 7 | WP-29 — Hostile-paste threat contract and hardening | 1–2 pw | **Implementation complete — gate pending** | WP-28 | D-023; M3 security exit |
| 8 | WP-30 — Unicode conformance and grapheme-safe clipping | 3–5 pw | **Implementation complete — gate pending** | WP-29 | M1; cross-widget Unicode acceptance |
| 9 | WP-31 — Damage, paint provenance, backing stores, and enforceable budgets | 4–6 pw | **Implementation complete — gate pending** | WP-30 | D-010/D-036/D-037; M2/M4/M5 scene and performance exits |
| 10 | WP-32 — Portable, isolated test and CI infrastructure | 2–3 pw | **Implementation complete — gate pending** | WP-31 | M0/M3/M4 cross-platform gates |
| 11 | WP-32A — Dual-profile graphics virtual terminal and visual captures | 2–4 pw | **Implementation complete — gate pending** | WP-32 | M3 headless/raster acceptance; screenshot gap |
| 12 | WP-33 — Fuzzing and sanitizer program | 1–2 pw | **Implementation complete — gate pending** | WP-32A | M3 fuzz exit; verification standard |
| 13 | WP-34A — Chrome, menus, and standard-dialog catalog closure | 2–3 pw | **Implementation complete — gate pending** | WP-33 | M5/M6 catalog gaps |
| 14 | WP-34B — Controls, editing, selection, and scrolling closure | 3–4 pw | **Implementation complete — gate pending** | WP-34A | M4/M6 control gaps |
| 15 | WP-34C — Data, media, help, file, and catalog closure | 3–5 pw | **Implementation complete — gate pending** | WP-34B | M6 remaining catalog gaps |
| 16 | WP-35 — Theme, command presentation, composition, and example ergonomics closure | 4–6 pw | **Implementation complete — gate pending** | WP-34C | WP-6…WP-13, WP-17 and unfinished WP-10 |
| 17 | WP-36 — Layout, dialog, and tree literal acceptance | 2–3 pw | **Implementation complete — gate pending** | WP-35 | WP-16, WP-18…WP-22; M10 exit |
| 18 | WP-36A — Comprehensive examples and client capability demos | 2–4 pw | **Implementation complete — gate pending** | WP-36 | M6/M11 example coverage; client usability |
| 19 | WP-36B — Common widget and UI component expansion | 3–6 pw | **Implementation complete — gate pending** | WP-36A | Calendar/date/time, numeric controls, command palette, utility chrome |
| 20 | WP-36C — Client documentation and tutorial revamp | 3–5 pw | **Implementation complete — gate pending** | WP-36B | Usable library docs, source-backed tutorials, screenshots, API/widget guide |
| 21 | WP-39 — Revisioned editor document and Unicode editing core | 3–5 pw | **Implementation complete — gate pending** | WP-36C | First-class editor foundation; [architecture](editor-architecture.md), [delivery target](editor-delivery-target.md) |
| 22 | WP-40 — TextEditor interaction, viewport, and editor status | 4–6 pw | **Implementation complete — gate pending** | WP-39 | First-class editor interaction; [architecture](editor-architecture.md), [delivery target](editor-delivery-target.md) |
| 23 | WP-41 — Syntax profile engine and incremental styling | 4–6 pw | **Implementation complete — gate pending** | WP-40 | Deterministic profile/highlighter foundation; [architecture](editor-architecture.md), [delivery target](editor-delivery-target.md) |
| 24 | WP-42 — Standard JSON, YAML, and Bash profiles | 3–5 pw | **Implementation complete — gate pending** | WP-41 | Shipped profiles and extension guide; [architecture](editor-architecture.md), [delivery target](editor-delivery-target.md) |
| 25 | WP-43 — Editor search, replace, and command integration | 3–5 pw | **Implementation complete — gate pending** | WP-42 | Editor workflow commands; [architecture](editor-architecture.md), [delivery target](editor-delivery-target.md) |
| 26 | WP-44 — File-editor workflow, editor example, and documentation | 4–6 pw | **Implementation complete — gate pending** | WP-43 | File lifecycle and client proof point; [architecture](editor-architecture.md), [delivery target](editor-delivery-target.md) |
| 27 | WP-45 — Embedded-terminal emulator and containment foundation | 4–6 pw | **Implementation complete — Mac current-host gate passed** | WP-44 | D-042 and private terminal model; [architecture](embedded-terminal-architecture.md), [delivery target](embedded-terminal-delivery-target.md) |
| 28 | WP-46 — POSIX PTY child sessions and basic TerminalView | 4–6 pw | **Implementation complete — Mac PTY gate passed** | WP-45 | Mac child lifecycle and readiness; [architecture](embedded-terminal-architecture.md), [delivery target](embedded-terminal-delivery-target.md) |
| 29 | WP-47 — TerminalView interaction, resize, and nested ckVision sessions | 4–6 pw | **Implementation complete — Mac current-host gate passed** | WP-46 | Full terminal usability; [architecture](embedded-terminal-architecture.md), [delivery target](embedded-terminal-delivery-target.md) |
| 30 | WP-48 — Contained child Sixel and terminal hardening | 5–7 pw | **Implementation complete — Mac current-host gate passed** | WP-47 | Raster no-spill guarantee; [architecture](embedded-terminal-architecture.md), [delivery target](embedded-terminal-delivery-target.md) |
| 31 | WP-49 — Terminal example, documentation, and end-to-end acceptance | 4–6 pw | **Implementation complete — Mac current-host gate passed** | WP-48 | Client proof point; [architecture](embedded-terminal-architecture.md), [delivery target](embedded-terminal-delivery-target.md) |
| 32 | WP-51 — SysInfo probe boundary and machine report panes | 1–2 pw | **Implementation complete — Mac and Linux gates passed** | WP-49 | Example coverage: a second injected platform service; [SysInfo plan](sysinfo-example.md) |
| 33 | WP-52 — Bar chart view, benchmark kernels, and the threaded run | 1–2 pw | **Implementation complete — Mac and Linux gates passed** | WP-51 | Example coverage: comparative visualization and cancellable background work; [SysInfo plan](sysinfo-example.md) |
| 34 | WP-55 — Comparison bars from published figures | 0.5–1 pw | **Implementation complete — Mac and Linux gates passed** | WP-52 | Example coverage: a score against something, without a fabricated bar; [SysInfo plan](sysinfo-example.md) |
| 35 | WP-53 — Cache hierarchy, multicore scaling, and host capabilities | 1–2 pw | **Implementation complete — Mac and Linux gates passed** | WP-52 | Example coverage: capability-driven chart paths; [SysInfo plan](sysinfo-example.md) |
| 36 | WP-54 — SysInfo report export, help, and documentation | 1 pw | **Implementation complete — gate pending** | WP-53 | Example coverage: injected-filesystem export and example documentation; [SysInfo plan](sysinfo-example.md) |
| 37 | WP-56 — Storage throughput and its interface ceilings | 0.5–1 pw | **Implementation complete — Mac and Linux gates passed** | WP-54 | Example coverage: the disk question, measured and compared; [SysInfo plan](sysinfo-example.md) |
| 38 | WP-57 — TextEditor as an application's document editing surface | 2–3 pw | **Implementation complete — gate pending** | WP-44 | Editor verbs as data, edit requests, caret motion, virtual caret, host colouring, context menus; [architecture](editor-architecture.md) |
| 39 | WP-58 — Status legends and form help that follow focus | 0.5–1 pw | **Implementation complete — gate pending** | WP-57 | StatusLine item sets per command context; descriptor-dialog field description panel |
| 40 | WP-59 — Commands that belong to several contexts | 0.5 pw | **Implementation complete — gate pending** | WP-58 | CommandScope: several named contexts and the context-free place; command palette keeps its invoking contexts; PendingDialogs |
| 41 | WP-60 — Side panels a reader sizes, hides and gets back | 0.5 pw | **Implementation complete — gate pending** | WP-59 | Splitter resize anchor, remembered extent, move report, collapsing a hidden pane; containers follow child visibility |
| 42 | WP-61 — Forms that check the whole answer | 0.5 pw | **Implementation complete — gate pending** | WP-60 | DialogDescriptor::check and DialogVeto: the dialog stays open, the field is marked and focused, the reason stands in the panel |
| 43 | WP-62 — A save dialog that offers a name | 0.25 pw | **Implementation complete — gate pending** | WP-61 | FileDialogOptions::suggested_name, kept across directories and focused for Enter |
| 44 | WP-63 — A field that says it was edited | 0.25 pw | **Implementation complete — gate pending** | WP-62 | InputLine::on_edited for every reader change, once, and never for set_text |
| 45 | WP-64 — Forms whose answers line up | 0.25 pw | **Implementation complete — gate pending** | WP-63 | Label::set_column_width; descriptor dialogs put the labels beside their controls in one column |
| 46 | WP-65 — Trees a reader pages through | 0.1 pw | **Implementation complete — gate pending** | WP-64 | TreeView Home/End and PageUp/PageDown over the visible rows, for models and materialized trees; a cursor placed before layout revealed on first size; a dialog chain returns focus where it began |
| 47 | WP-66 — Text on a window's border | 0.25 pw | **Implementation complete — gate pending** | WP-65 | FrameText overlay in the border's own style; Window::frame_style; EditorWindow's readout and the spin example use it |
| 48 | WP-67 — Text fields that offer their preset | 0.25 pw | **Implementation complete — gate pending** | WP-66 | D-066: keyboard focus selects an input line's text, caret at the end; a press places the caret; Insert keeps the text |
| 49 | WP-68 — A cell grid | 1 pw | **Implementation complete — gate pending** | WP-67 | D-067: `CellGrid` over a provider that owns the cursor, the selection and the scroll origin; header, gutter, frozen bands, spans, cell styles, overlays; `MaterializedCellGridModel`; four theme roles |
| 50 | WP-69 — Option groups with captions that route and choices in columns | 0.25 pw | **Implementation complete — gate pending** | WP-68 | D-068: a group's caption carries a mnemonic that focuses the group from its window; `set_columns()` and `FieldDescriptor::columns` lay choices out row-major; arrows step the sequence and the column, and one that would not move is the window's |
| 51 | WP-70 — The status line's hint on an empty desktop | 0.1 pw | **Implementation complete — gate pending** | WP-69 | D-069: with nothing focused the hint resolves from `Application::root()`'s help-context key; a focused view's own key, or any ancestor's, outranks it |
| 52 | WP-71 — A list's keyboard search as a typed prefix | 0.1 pw | **Implementation complete — gate pending** | WP-70 | D-070: letters typed within a second form one prefix; the same letter again steps on; a miss drops the prefix; modified letters are chords; providers answer through `find_prefix` |
| 53 | WP-72 — An SQL syntax profile | 0.2 pw | **Implementation complete — gate pending** | WP-71 | D-071: SQL joins the standard profiles; quoted names and bound parameters painted as names, a word before `(` a call, block comment and unclosed text carried across lines |
| 54 | WP-50 — Windows ConPTY child sessions and embedded-terminal matrix | 4–8 pw | **Done — native child adapter, all seven named signed-in Windows scenarios, degraded child profiles, and 95 portable golden files verified; capable-host pixel mouse and global release matrix remain WP-37/WP-38 work** | WP-49 and Windows host | Windows terminal containment; [architecture](embedded-terminal-architecture.md), [delivery target](embedded-terminal-delivery-target.md) |
| 55 | WP-37 — Windows backend and three-platform matrix | 2–4 pw | **In progress — live keyboard/cell mouse, clipboard, Sixel/fallback, editor input/search/Save, plain-VT ConPTY, strict Windows 164/164, 111 byte-identical three-platform goldens, Mac/Linux sanitizer gates, Linux fuzz corpora, three-host benchmark gates, and live WezTerm pixel-mouse transport plus Gallery interaction Done; literal Windows Terminal pixel mode and legacy-console refusal (VM host launch failed) Open** | WP-50 and Windows host | M7; Windows portions of global gates, including editor and terminal view |
| 56 | WP-38 — Golden, script, reference, and release-gate closure | 3–5 pw | **In progress — M7 traceability, installed-header API index, Editor/terminal four-scheme initial/full-screen/nested appearance slices, and 111-file three-host manifest Done; full M0–M11 matrix, reference depth, and release audit Open** | WP-37 for final closure | M0–M11 global gates |

Corrective work is currently estimated at roughly **92–149 focused
person-weeks**. These are planning ranges, not completion evidence. WP-23 must
re-estimate later rows if its authority/provenance audit invalidates more
implementation than is currently known; that update changes the estimates,
never the acceptance criteria silently.

The dependency order deliberately puts correctness, lifetime, security, and
architecture before convenience work. WP-45…WP-49 now have current-host
implementation evidence; WP-50 is Done and the outer-backend portion of WP-37 remains in progress on the named Windows ARM64 VM after the
editor/file-workflow implementation packages. WP-39…WP-44 remain in the
queue until WP-37/WP-38 complete their cross-platform and release-audit gates.
The WP-37 outer backend began before WP-50 acceptance so the Windows
contained-terminal example can run under a real host; WP-37 consumes the
completed WP-50 interactive matrix.
A package may be split into reviewable
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
| WP-4 | Implementation complete — WP-26 replaced the public Desktop-capturing callback with a private lifetime capability and terminal-input coverage; global matrix pending | WP-26 |
| WP-5 | Implementation complete — WP-26 routes generic Window insertion/removal through Desktop bookkeeping and pins the dialog cycle visually; global matrix pending | WP-26 |
| WP-6 | Reopened — example hygiene has no durable source check and depends on unresolved M8 behavior | WP-35 |
| WP-7 | Reopened — per-window theme override is absent and Desktop still takes theme plumbing contrary to D-028 | WP-35 |
| WP-8 | Reopened — role additions landed, but the specified scripted runtime theme mutation was not accepted | WP-35 |
| WP-9 | Reopened — the zero-`static_cast<widgets::` example criterion is not met | WP-35 |
| WP-10 | Transferred — metadata/handler work landed; live menu/status structure, hints, enablement, retraction, and surface-specific command presentation remain | WP-35 |
| WP-11 | Revalidation required — registry-rendered labels/chords must pass the final command and golden matrix | WP-35, WP-38 |
| WP-12 | Revalidation required — standard commands must compose with modal routing and complete documentation | WP-27, WP-35 |
| WP-13 | Reopened — named command contexts remain incomplete and default traversal fails inside the current modal boundary | WP-27, WP-35 |
| WP-14 | Reopened — `run()` uses unsanctioned process-global Application state and wake cannot interrupt an in-flight wait | WP-27 |
| WP-15 | Implementation complete — the shared blocking-modal guard, modal routing/focus lifetime repair, and message/file/directory `exec_*` conveniences are covered locally; WP-27/38 retain lifecycle and global-matrix acceptance | WP-27, WP-38 |
| WP-16 | Implementation complete — Row, Column, Grid, Dock, and hosted dialog content consume height-for-width through executable layout acceptance; final global gate remains | WP-36 |
| WP-17 | Reopened — the former provenance and line-limit contradiction were repaired by WP-23; remaining example integration acceptance is owned by WP-35 | WP-35 |
| WP-18 | Implementation complete — resized hosted-dialog wrapping is covered in WP-36 acceptance; root resize goldens remain the visual baseline | WP-36 |
| WP-19 | Implementation complete — Row/Column/Grid/Dock height-for-width regressions and existing container suites cover the local implementation; final global gate remains | WP-36 |
| WP-20 | Implementation complete — frame-overlay and overlay behavior remain covered by their suites; WP-36 adds application-frame shrink/recovery evidence | WP-36 |
| WP-21 | Implementation complete — WP-36 shrink-storm script drives full/degraded/too-small/recovered frames with deterministic focus, chrome, and damage evidence | WP-36 |
| WP-22 | Implementation complete — filebrowser lazy expansion, injected filesystem, Splitter interaction, payloads, callbacks, navigation, and observable frames are covered locally | WP-36 |

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

**Implementation complete — gate pending (2026-08-08).** Application now
captures an application-local weak lifetime capability for each attached mouse
target and capture-route member, validating it and root attachment before every
post-callback use. Detached or destroyed views end the old route; reparented
views are resolved through their current ancestry before focus is applied.
`tests/test_application.cpp` covers synchronous self-destruction, ancestor
destruction, reparenting followed by text input, and a self-dismissing captured
popup. `tests/test_window_activation_golden.cpp` drives a terminal-input script
against genuinely overlapping windows, checks raise/restyle/focus/text routing,
and pins the before/after active-frame goldens; the
`generate_window_activation_goldens` tool regenerates them for review. Release
and ASan/UBSan runs passed all 8 CTest gates on 2026-08-08. WP-25 remains gate
pending until WP-38 records the complete cross-platform/global matrix. Its
package prerequisite is satisfied, so WP-26 is the next ready work package.

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
- Apply the same invariant to all standard dialog handles. Keep explicit
  modeless/modal presentation as the sanctioned API, not the only path that
  happens to be correct.
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

**Implementation complete — gate pending (2026-08-08).** `View` now makes
generic child insertion/removal polymorphic, and `Desktop` routes every direct
`Window` child through the same ownership, activation, z-order, resize, and
detachment path as `add_window()`/`remove_window()`. The old public
`zoom_area_provider` was removed: Desktop installs a private, weak-lifetime
capability, clears it before any detach observer runs, and Window safely falls
back to its own bounds after detachment. Application drops a modal scope when
its root detaches, so generic Desktop removal cannot leave stale routing state.
`present_modeless` is the concise attach-and-focus operation, while standard
`WindowHandle`s are correct through generic Desktop attachment too. D-038 adds
the distinct modal path and typed standard-dialog completion in WP-27.

`tests/test_desktop.cpp` covers base-typed generic attach/remove, retained
Window use after its former Desktop is destroyed, modal-scope cleanup, and
terminal mouse zoom/restore across a terminal resize. `tests/test_message_box.cpp`
covers generic standard-dialog attachment, cycling, close, and focus restore.
`tests/test_desktop_dialog_golden.cpp` drives F6/Enter through
HeadlessTerminal and pins active, cycled, and closed frames in
`desktop_dialog_open.dump`, `desktop_dialog_cycled.dump`, and
`desktop_dialog_closed.dump`; `generate_desktop_dialog_goldens` regenerates
those fixtures for review. Release and ASan/UBSan local CTest gates are green
on macOS on 2026-08-08. WP-38 still owns the complete cross-platform/global
matrix; WP-27 is now the next ready package.

### WP-27 — Run loop, terminal session, wake, modality, and presentation API

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
- Replace ambiguous standard-dialog presentation entry points with an explicit
  modal/modeless family. The non-blocking modal path must establish input
  scoping before it returns; the modeless path must never imply modal behavior.
  Re-derive and record the final public naming and ownership contract before
  implementation, then remove superseded pre-1.0 entry points and update every
  caller and reference page together.
- Give every non-blocking standard-dialog presentation a typed completion
  handle/result contract: a caller can register for exactly one typed result
  without retaining a raw `Window*`, and cancellation, close, detach, and quit
  have documented deterministic outcomes. It must be usable from a command
  handler without a nested event loop or unsafe lifetime capture.
- A blocking `exec_*` may drive the loop only when no loop/dispatch is already
  active. Calling it from a handler must not create a nested pump; the typed
  non-blocking modal path is the handler-safe alternative. Complete Window,
  descriptor dialog, message-box, file-dialog, and directory-picker
  conveniences consistently.
- Complete vetoable close and quit sweep behavior under modal and non-modal
  operation.

**Progress checkpoint (2026-08-08).** The Application-global signal handler
has been removed; `Terminal::wake()` now reaches the POSIX self-pipe and is
covered while a real poll is blocked; modal key/text/mouse/focus routing,
capture exclusion, detach-safe focus restoration, and re-entrant pump
rejection are implemented. Modal owners now retain an exact scope identity so
quitting or detaching an outer dialog cannot pop a newer nested dialog. POSIX
resize observation is per terminal session, not a process-wide SIGWINCH flag.
Record/replay now snapshots initial terminal state and verifies the complete
event/output operation stream, including title, bell, supported clipboard
writes, post-restore diagnostics, capability refinement, and resize. Replay applies each recorded size
and capability transition before returning its batch, and an Application-level
round trip proves the resized, capability-refined frames are byte-identical.
The local lifecycle, probe/degradation, suspend/resume, record/replay,
close/quit-sweep, sanitizer, and multi-Application PTY criteria are completed
by the evidence recorded below. The M3 global gates that remain outside this
package's implementation boundary are explicitly owned by WP-31 (budgets),
WP-32/WP-37 (portable matrix), WP-32A (paired visual capture), WP-33 (fuzzing),
and WP-38 (release audit).

D-038's first public slice is also landed: `Desktop` now distinguishes
modeless, modal, and blocking-modal presentation; `present_message_box`,
`present_file_dialog`, `present_directory_picker`, `present_window_list_dialog`,
and `present_help_viewer` share the typed non-blocking modal completion path and
complete only after detachment. The complete close, external-detach, and
quit-sweep result matrix is now directly exercised for all five families;
an explicit result always wins, while the documented fallback is delivered
once only after the modal root is gone. Nested completion can present a new
modal without stale scope, and a vetoed close keeps the existing presentation
modal until a later close succeeds; that same veto also stops a quit sweep
without requesting application shutdown. Queued external destruction also
resolves once only after scope removal. `tests/test_dialog_presentation.cpp`
is the cross-family executable contract; the individual dialog suites retain
their control-specific acceptance/cancellation tests.

The blocking family now covers message boxes, file dialogs, and directory
pickers: `exec_message_box`, `exec_file_dialog`, and `exec_directory_picker`
all build the same WindowHandle-based dialogs and delegate to
`Desktop::exec_modal`. They therefore reject re-entrant handler/post/timer use
through the one shared blocking-pump guard, restore the prior focus through the
ordinary close path, and return the same cancellation result if a host quit
ends the outer pump. Command handlers continue to use the corresponding typed
non-blocking `present_*` APIs.

`PosixTerminal` now also exercises the first bounded capability-probe
lifecycle rather than merely decoding hypothetical replies: it sends OSC 10/11,
briefly enables/queries/resets DEC mode 2026, queries DECRQM 1016, Primary DA,
XTSMGRAPHICS color-register/geometry
queries, XTWINOPS 16, and enters/probes DEC mode 2031 for live color-scheme
notifications after the baseline session is entered. Its deadline
is measured on the explicit Clock shared with its owning Application; it
accepts changed replies only during a 250 ms window, discards late replies,
and starts a fresh window after resize or resume. PTY acceptance proves the
baseline before a reply, timely dark-scheme/Sixel/cell-metric/pixel-mouse and
Sixel-limit refinement, no-response degradation, late-reply rejection, and
resize re-probing. Resize and resume immediately withdraw runtime-probed cell
metrics, pixel mouse, and window-sensitive Sixel geometry; they require fresh
1016-mode and XTWINOPS evidence before pixel conversion is re-enabled, and a
fresh finite XTSMGRAPHICS geometry response before raster output returns. A
decoder probe-window fence rejects a
capability reply that began before a resize/resume boundary; complete replies
have no protocol request identifier, so the bounded deadline is deliberately
not presented as stronger causal evidence than it is. A positively verified mode-2031 subscription accepts its
explicit DSR palette-preference changes after the ordinary probe deadline;
the DSR itself never establishes that subscription, so without prior proof it
is discarded with every other stale reply. A
verified finite Sixel geometry preserves mandatory text
fallback instead of emitting an oversized raster; a finite color-register
limit bounds the encoder palette, and record/replay retains both fields.
An application-level Gallery transition now proves that a new finite geometry
limit clears an already-presented but newly ineligible raster before drawing
the mandatory cell fallback, then restores the raster when the limit permits
it again.
Explicit host profiles may disable those probes. The named-profile
probe/degradation behavior is covered by the WP-27 headless and PTY contracts;
the broader M3 global matrix is owned by WP-32, WP-32A, WP-37, and WP-38.
D-019's conservative output boundary now treats every non-ASCII grapheme as width-unsafe and re-addresses
the following cell absolutely, so a terminal-width disagreement cannot shift
the rest of a frame; an interoperable width-reporting refinement remains the
post-0.1 D-OPEN-7 track. The first
curated degradation baselines are now explicit host-selected `ModernVt`,
`TmuxConservative`, `ScreenConservative`, and `LinuxConsole` profiles; the
last three disable unguaranteed input extensions and graphics, while Linux
console remains keyboard-only under D-OPEN-6. POSIX PTY coverage proves that
each conservative profile neither enables nor restores unsupported input modes.
The Gallery example now also runs end-to-end under each of those three named
profiles, proving that it emits its mandatory text image fallback (never Sixel
or raster pixels) and still accepts ordinary keyboard text input.
As a supplementary non-gating host observation, the built Hello example was
also launched in fresh detached tmux 3.6b and GNU Screen 4.00.03 sessions on
the local macOS host; its documented Alt+X command ended the application and
each session. This is deliberately limited to the launch/quit path and does
not replace the dependency-free profile tests or close the hostile-host matrix.

Capability admission now occurs inside `InputDecoder`, before later bytes from
the same terminal read are parsed, rather than as a backend-side event filter.
Consequently a reply that is late, unrequested, or disallowed by a curated
profile cannot transiently enable pixel-mouse decoding for a following SGR
report. `PosixTerminal` selects probe, verified-live-color-only, or reject
admission at the observed-byte boundary. `HeadlessTerminal` mirrors that
policy through its named-profile constructor, while explicit scripted
CapabilityChanged events remain available for deterministic profile-change
scenarios.

Quit sweep now snapshots its starting windows with per-instance lifetime tokens
before invoking any user close callback, so self-detachment and address-reused
replacement windows cannot invalidate or extend the traversal; regressions
close/destroy every swept window synchronously, including a topmost callback
that destroys an unvisited starting window while opening a replacement that
the original sweep must never close; the shared View-token contract directly
proves instance liveness identity. The default sweep is non-reentrant: a close
callback that executes Quit cannot recurse into the active window or let an
inner sweep request shutdown before the outer request reaches a later veto.
`Window::close()` applies the same rule at the protocol boundary, retaining
its independently-lived close state through callbacks that detach and destroy
the Window before the outer request returns. A quit-sweep state and Desktop
liveness token give the owning Desktop that same destruction safety: if a
close callback removes it from the view tree, the active sweep ends without
touching its former members or selecting shutdown. Application detachment
also clears focus/capture references for an entire departing subtree before a
window's completion callback can present a nested modal, eliminating the
otherwise stale focused-child restore target. The full local eight-gate suite
passes in the normal build, an AddressSanitizer/UndefinedBehaviorSanitizer
build, and a ThreadSanitizer build (including the concurrent two-PTY/wake
acceptance). Three-platform evidence and the remaining hostile degradation
matrix still remain WP-27 work.

The low-level message-box, file-dialog, and directory-picker factories apply
the same callback-boundary rule to typed results: their completion state is
independent of the returned Window and records delivery before invoking user
code. A result callback may therefore re-enter accept/cancel, detach, or
destroy its own dialog; it still observes exactly one result, and the factory
does not touch the former Window afterward. Each factory has a direct
destruction regression and a reentrant-result regression under ASan.

Deferred self-detachment obeys the same identity rule across the event-loop
boundary. A dialog closes before its Button callback unwinds, so its removal is
posted rather than performed inline; that post retains a weak per-instance
Window token and becomes a no-op if external code destroys the dialog first.
It cannot dereference a stale address or detach a later object allocated at the
same address. The public message-box close path exercises this under ASan.

The PTY lifecycle suite also throws across a live `PosixTerminal` scope and
proves that its restore sequence reaches the terminal before the enclosing
catch path emits output. This records ordinary exception unwinding as a
separate D-011 restoration path rather than inferring it from normal
destruction alone.

Keyboard protocol selection is now an actual decoder boundary rather than
metadata alone: legacy traffic swallows unnegotiated kitty and
modifyOtherKeys encodings, while a selected kitty profile accepts only CSI-u
key reports and resolves a bare Escape immediately. Kitty press/repeat/release
transitions are now distinct: release routes only to the dedicated View hook
and cannot execute a command binding. A POSIX kitty session now pushes that
protocol's saved state on alternate-screen entry and pops it before restore;
xterm modifyOtherKeys remains host-selected because its public control has no
exact state-restoration stack. Focused decoder tests pin
both directions of the degradation matrix so an extension sequence cannot
silently become an application command under a conservative profile.
The same boundary now applies to SGR and legacy X10 mouse, focus reports, and bracketed
paste: a conservative profile swallows those unnegotiated controls, and an
unnegotiated paste stream is discarded through its terminator rather than
leaking its payload into a widget as normal typing.

Application now fences every callback-bearing hot path (direct dispatch,
focus, command execution, `step`, painting, and blocking-loop predicates).
An exception cannot unwind across a live terminal: `PosixTerminal` marks the
fixed callback-failure diagnostic in D-024's lock-free handler-visible state,
then aborts; its fatal handler restores every registered session before writing
that diagnostic to stderr and re-raising `SIGABRT`. The PTY acceptance test
drives a throwing key callback and proves alternate-screen, synchronized-output,
and color-scheme-mode restoration all precede the diagnostic.

Core assertions now publish their immutable expression/source/line metadata
through D-024's lock-free session ledger rather than writing stderr directly.
The POSIX fatal handler consumes that metadata only after restoring every
registered terminal, then emits the full assertion diagnostic. A separate PTY
child aborts on `CKV_ASSERT(false)` and proves all three restoration modes
precede its exact source-location diagnostic.

The pre-existing diagnostics primitives are now part of the session lifecycle:
each `Application` owns a forwarding in-memory `DiagnosticsSink`, accepts one
owned injected observer, and restores its terminal before flushing its retained
severity-tagged records through the backend diagnostic channel. A POSIX PTY
acceptance observes the restore ledger before the buffered warning arrives on
stderr; headless injection proves no host I/O is required for test inspection.

The fatal-signal PTY child now also verifies the original canonical/echo mode
bits after `SIGABRT`, in addition to observing the alternate-screen restore
bytes. D-024 statically requires its handler-visible atomics to be lock-free;
the supported POSIX.1-2017 baseline lists `tcsetattr` as async-signal-safe.

The multi-Application PTY acceptance now creates two `PosixTerminal` /
`Application` pairs in one process and proves that input, wake, resize, and
quit activity for one does not mutate the other; in particular, it keeps both
applications in real terminal waits and proves a wake for the first leaves the
second blocked until its own wake. `docs/terminal-host-integration.md`
now also makes D-021's external-loop boundary executable: `Application`
forwards every backend wait source as a borrowed handle set and exposes its
earliest timer deadline, so a host can wait once with its own sources and call
non-blocking `step(now)` only when useful work is possible.
The host-integration page
records the complementary host boundary: the registry restores the host's
prior terminal-signal dispositions once its final session ends, while a live
session reserves those dispositions; SIGINT and job-control policy are
explicit. SIGINT is host-owned, while SIGWINCH merely interrupts polling and
each session observes its own geometry. A job-control-capable PTY hierarchy now
proves SIGTSTP restoration, a real stopped worker, SIGCONT raw/mode re-entry,
the instance-local full re-presentation event, byte-identical resumed frame,
resumed input, and a resize made while stopped applied before that first
resumed frame. A two-PTY child process also retains an injected host SIGINT
disposition while both Applications are live and proves that delivery cannot
request quit in either one; SIGINT remains host policy before, during, and
after terminal sessions. `run_until` now gives a request observed before or
during a `step` precedence over its next completion predicate, so a host
shutdown cannot be misreported as normal blocking-dialog completion.
The same host-shutdown path now force-detaches an `exec_*` dialog still open
when its pump ends, removes the exact modal scope, and returns its documented
cancellation fallback without treating shutdown as a vetoable user close.
`Application::request_quit()` now also wakes its terminal exactly once for the
sticky request, so a host shutdown request cannot leave `run()` in an
in-flight wait until its ordinary frame deadline. A blocking-terminal regression
drives `run()` on one thread, requests quit from another, and proves return
without a separate host `wake()` call; a companion PTY acceptance proves the
same behavior through a live `PosixTerminal` session. The public host-integration
contract now states that behavior explicitly. Ordinary POSIX output now waits for a
non-blocking terminal queue to become writable instead of truncating a frame or
session sequence; the two-PTY acceptance keeps both pseudo-terminal masters
drained concurrently while exercising wake, resize, quit, and restoration.
An independent PTY contract sends a 256 KiB payload through the same path,
forces backpressure, and proves every byte reaches the master before the child
is allowed to restore and close its terminal session.
An explicit verified SGR-pixel profile now supplies the interactive half of
the dual-space mouse contract: a real PTY report preserves its pixel coordinate
and maps its cell coordinate through the authoritative cell metric, while the
same session proves modes 1002, 1006, and 1016 are restored before the
alternate screen is left.

**Latest local evidence (2026-08-08).** `tests/test_posix_terminal.cpp`
contains the named PTY contracts for dual-space SGR-pixel input and host-owned
SIGINT across two live Applications. `tests/test_application.cpp` pins quit
precedence over completion predicates before entry and after a timer callback.
`tests/test_desktop.cpp`, `tests/test_message_box.cpp`,
`tests/test_file_dialog.cpp`, and `tests/test_directory_picker.cpp` pin forced
shutdown detachment, veto bypass, modal-scope cleanup, and the three typed
cancellation fallbacks; the Desktop contract also proves that forced-detach
cleanup cannot pop a replacement modal synchronously presented from the
departing dialog's detachment callback. PTY output-lifecycle assertions keep
their child slave open until an explicit parent acknowledgement has drained
the bytes, rather than relying on a close-time scheduling race. The local
normal, ASan/UBSan, and TSan configurations each completed all eight CTest
gates with **1313** unit/script tests passing. After the close-protocol
reentrancy repair, the normal configuration completed all eight gates with
**1314** unit/script tests; ASan/UBSan and TSan each completed the full unit,
PTY, and script binary with the same count. No sanitizer diagnostic was
reported. After the standard-dialog callback lifetime and reentrancy repair,
the normal, ASan/UBSan, and TSan configurations each completed all eight CTest
gates with **1321** unit/script tests passing; no sanitizer diagnostic was
reported. After the deferred self-detach lifetime repair, the same three
configurations again completed all eight CTest gates with **1322** unit/script
tests passing; no sanitizer diagnostic was reported. After capability-admission
became a decoder boundary, the same configurations completed all eight CTest
gates with **1325** unit/script tests passing; no sanitizer diagnostic was
reported. The explicit no-probe same-read PTY contract raised that evidence to
**1326** tests in all three configurations, again with no sanitizer diagnostic.
The runtime Sixel-geometry fallback transition raised it to **1327** tests in
all three configurations, again with no sanitizer diagnostic.
An empty `Application::post` callback is now explicitly a harmless no-op,
matching the timer-callback contract rather than reaching an empty
`std::function` during `step()`. Its regression is part of the aggregate
suite. The `RoleRegistry` mismatched-fallback assertion contract now runs in
its own single-threaded POSIX CTest process: it still requires the child to
terminate with `SIGABRT`, but no longer forks after thread-owning cases in the
aggregate process, which ASan can legitimately kill before that contract is
observed. The normal, ASan/UBSan, and TSan configurations each completed the
resulting full **nine**-CTest-gate suite with **1327** aggregate unit/script
tests and the separate assertion-contract process passing; no sanitizer
diagnostic was reported.
The D-019 boundary now also treats ordinary CJK text as width-unsafe, not only
emoji, selectors, and a host-declared ambiguous-width convention. A focused
Presenter regression proves the baseline re-addresses the following cell after
a width-two CJK grapheme; normal, ASan/UBSan, and TSan each completed all nine
CTest gates with **1328** aggregate unit/script tests and no sanitizer
diagnostic.
SGR-pixel admission now additionally has an explicit coordinate-space fence:
an authoritative non-pixel profile never enables DEC mode 1016; while a probe
has temporarily enabled that mode but has not yet established both DECRPM 1016
and an XTWINOPS cell metric, complete mouse reports are consumed rather than
misread as cell coordinates. A proof earlier in the same terminal read enables
pixel delivery for a later report in that read. An unsuccessful window resets
mode 1016 before ordinary SGR input resumes, and every resize re-probe enables
it again before querying. Decoder and PTY regressions cover all four
transitions. The normal, ASan/UBSan, and TSan configurations each completed
all nine CTest gates with **1331** aggregate unit/script tests and no sanitizer
diagnostic.
XTSMGRAPHICS maximum Sixel geometry is also now treated as resize-dependent:
the published xterm contract bounds it by the terminal window. A resize
therefore withdraws runtime graphics, palette, and geometry evidence before
the Application re-presents; DA1 cannot restore raster output on its own, and
only a fresh finite geometry reply does. Zero-sized geometry replies are not
accepted as a bound. The PTY contract proves the withdrawal, DA1 fence, and
fresh-geometry restoration; normal, ASan/UBSan, and TSan each completed all
nine CTest gates with **1332** aggregate unit/script tests and no sanitizer
diagnostic.
The conservative-profile Gallery script now injects ordinary terminal bytes,
rather than bypassing the decoder with a direct `TextEvent`. That exposed and
corrected the public editing contract: `InputLine` and `Memo` normalize
unmodified and Shift-modified `Key::Char` events through their existing text
insertion path, including validation, masks, selection, and multiline rules;
Alt, Ctrl, and Super character chords remain available to command routing.
Focused control regressions and the three-profile end-to-end Gallery contract
cover the boundary. Normal, ASan/UBSan, and TSan each completed all nine CTest
gates with **1334** aggregate unit/script tests and no sanitizer diagnostic.
Modal command routing now distinguishes scope-safe framework defaults from
background accelerators. After the focused modal route declines a key,
Tab/Shift-Tab and F1 continue to execute `kFocusNext`, `kFocusPrevious`, and
`kHelp` inside that modal's subtree; menu, window, quit, and application
commands remain excluded. The terminal-driven regression begins with focus on
the background invoker, proves Tab enters the modal's first field rather than
skipping it, cycles both directions, resolves the modal help context, and
rejects an application accelerator. Normal, ASan/UBSan, and TSan each
completed all nine CTest gates with **1335** aggregate unit/script tests and
no sanitizer diagnostic.
The ordinary standard-dialog path is now separately terminal-driven:
`present_file_dialog` receives a single decoded batch containing Shift-Tab,
Home/Delete editing, printable text, and Enter. The script proves traversal
enters the modal field, text never bypasses the decoder or modal router, the
Window accept path produces the typed result, and the deferred detachment
leaves no modal/window behind. Normal, ASan/UBSan, and TSan each completed all
nine CTest gates with **1336** aggregate unit/script tests and no sanitizer
diagnostic.
The named-profile PTY matrix now includes `ModernVt`, not only its conservative
rows. It proves the profile enters and resets its documented 1002/1006 SGR,
bracketed-paste, and focus modes while profile construction keeps raw probes
off. That audit corrected the restore ledger: a non-probing cell-mouse session
no longer resets unrelated mouse protocol 1000 or unentered pixel mode 1016;
1016 is reset only when an explicit pixel profile or a probe session established
it. Normal, ASan/UBSan, and TSan each completed all nine CTest gates with
**1337** aggregate unit/script tests and no sanitizer diagnostic.
Fatal-signal restoration now has a multi-session PTY contract in addition to
the single-session paths: one `SIGABRT` with two live `PosixTerminal` sessions
must restore both alternate screens, synchronized-output and color-notification
modes, and both original canonical/echo states before the process terminates.
Normal, ASan/UBSan, and TSan each completed all nine CTest gates with **1338**
aggregate unit/script tests and no sanitizer diagnostic.
The continuation half of D-024 now has its matching multi-session contract:
after the saved original modes simulate post-stop state, one `SIGCONT` must
re-enter raw mode and each session's own entry sequence for two live PTYs, and
each subsequent poll must emit exactly its instance-local capability change.
Normal, ASan/UBSan, and TSan each completed all nine CTest gates with **1339**
aggregate unit/script tests and no sanitizer diagnostic.
The descriptor-dialog family now has the same D-038 surface as the other
standard dialogs: `present_dialog` returns a typed non-blocking completion and
`exec_dialog` is its outer-loop-only blocking sibling. The descriptor is owned
by the presentation, an accepted result returns the fields in descriptor
order, and close, Esc, external detach, and host shutdown produce no partial
values. This audit also repaired the lifetime boundary shared by every
standard dialog: former focus is retained as a per-instance weak capability,
and close/accept callbacks retain their required state on the stack before
entering user code that may destroy the dialog. Regression coverage destroys
the former focus for all five existing standard families and destroys a
descriptor window from its default-button callback under ASan. Normal,
ASan/UBSan, and TSan each completed all nine CTest gates with **1343**
aggregate unit/script tests and no sanitizer diagnostic.
The named ModernVt profile now has a single PTY interaction contract, in
addition to its mode-ledger contract: one raw batch produces the profile's
documented focus event, sanitized bracketed-paste text, and cell-space SGR
mouse press, with no invented pixel coordinate. This complements the
conservative profiles' end-to-end rejection of unrequested extension streams.
Normal, ASan/UBSan, and TSan each completed all nine CTest gates with **1344**
aggregate unit/script tests and no sanitizer diagnostic.
Record/replay now has its missing live-backend bridge contract: a
`RecordingTerminal` around a real `PosixTerminal` captures one raw-PTY key
batch and forwarded output operation; `ReplayTerminal` then consumes only the
recorded initial state and operation stream, reproduces both in order, and
matches the recording exactly. Normal, ASan/UBSan, and TSan each completed all
nine CTest gates with **1345** aggregate unit/script tests and no sanitizer
diagnostic.
The close protocol now holds both user callbacks by value before entering the
first one and tests the Window's per-instance liveness before its second stage.
Thus an accepted `close_request` may detach and destroy its own Window without
an `on_closed` member read after destruction; that self-destruction is the
terminal close action, so no later `on_closed` callback runs. Normal,
ASan/UBSan, and TSan each completed all nine CTest gates with **1346**
aggregate unit/script tests and no sanitizer diagnostic.
The headless degradation matrix now also proves an ordered host-policy profile
transition: a Linux-console instance upgraded by an explicit
`CapabilityChangedEvent` decodes ModernVt focus, paste, and cell-mouse traffic
only after that event, then a later Linux-console downgrade consumes the same
extension stream without payload leakage. Normal, ASan/UBSan, and TSan each
completed all nine CTest gates with **1347** aggregate unit/script tests and no
sanitizer diagnostic.
The conservative PTY matrix now proves the input half of each named profile as
well as its mode ledger: tmux, screen, and Linux-console sessions consume
unrequested focus, bracketed-paste, and SGR-mouse streams on a real PTY, yet
deliver the ordinary key which follows them. Normal, ASan/UBSan, and TSan each
completed all nine CTest gates with **1348** aggregate unit/script tests and no
sanitizer diagnostic.
The blocking `Desktop::exec_modal` pump now carries only per-instance Desktop
and Window liveness observations across callback-bearing iterations. A posted
callback may remove and destroy its owning Desktop; the next completion check
ends the pump without consulting that former object's window list, and the
Application detach path removes the exact modal scope. Normal, ASan/UBSan, and
TSan each completed all nine CTest gates with **1349** aggregate unit/script
tests and no sanitizer diagnostic.
The non-blocking presentation boundary now has the matching callback rule:
`present_modeless` and `present_modal` hold new Window/owner liveness across
their focus transition and return `nullptr` rather than a dangling observer if
the former focus synchronously removes that window. Modal detachment removes
its scope before return. Normal, ASan/UBSan, and TSan each completed all nine
CTest gates with **1350** aggregate unit/script tests and no sanitizer
diagnostic.
The underlying attachment boundary now has the same explicit rule: a View is
owned by its parent before `on_attached()` runs, and the context walk holds
per-instance parent/child liveness across every attachment callback. A child
may therefore detach and destroy itself, or destroy the parent currently
adding it, without `View::add_child()` or recursive context propagation
touching stale tree storage; the operation returns `nullptr` rather than a
dangling observer. Direct regressions exercise both forms under ASan. Normal,
ASan/UBSan, and TSan each completed all nine CTest gates with **1352**
aggregate unit/script tests and no sanitizer diagnostic.
Root resizing now uses the same identity discipline. Its root-child layout
pass snapshots direct-child lifetimes before invoking `on_resized()`, so a
child that detaches and destroys itself cannot invalidate the remaining
iteration; `View::set_bounds()` also stops immediately after any callback
destroys its View, before consulting a later bounds observer. The regression
removes the first root child from its own resize callback and proves the next
child still receives the new root geometry. Normal, ASan/UBSan, and TSan each
completed all nine CTest gates with **1353** aggregate unit/script tests and no
sanitizer diagnostic.
Desktop resize now has the same callback-safe traversal: it snapshots each
owned Window's per-instance identity before applying growth policy and
reachability clamping, skipping a Window that removed or destroyed itself in
its `on_resized()` hook while still processing surviving siblings. Generic
dock removal also clears the relevant content-area observer before detachment.
The regression destroys one Window during a Desktop resize and proves its
sibling remains valid; normal, ASan/UBSan, and TSan each completed all nine
CTest gates with **1354** aggregate unit/script tests and no sanitizer
diagnostic.
The remaining Desktop geometry commands use that same snapshot: `tile()` and
`cascade()` preserve their starting slot geometry and z-order intent while
skipping a Window that destroys itself from `on_resized()`. The direct
regression exercises both commands with a self-removing Window and verifies
the surviving Window remains managed. Normal, ASan/UBSan, and TSan each
completed all nine CTest gates with **1355** aggregate unit/script tests and no
sanitizer diagnostic.
The dock lifecycle has a direct contract too: generic removal clears the
Desktop's top/bottom dock observer before the detached view can be destroyed,
so `content_area()` never retains a stale dock pointer. Normal, ASan/UBSan,
and TSan each completed all nine CTest gates with **1356** aggregate
unit/script tests and no sanitizer diagnostic.
This is local evidence only: the three-platform matrix, hostile-host coverage,
fuzz corpus, benchmark gates, paired visual captures, and release audit remain
separate required work under WP-31 through WP-38. They do not reopen the
completed WP-27 lifecycle and presentation implementation.

**Acceptance.**

- Tab, Shift-Tab, F1, local accelerators, text entry, and mouse input work
  inside an ordinary modal dialog while every background target and command is
  excluded.
- Scripts cover nested modals, destruction/detach at each event phase, focus
  restoration, close veto, and quit sweep.
- Modal and modeless calls are unmistakable in public examples and reference
  signatures. A non-blocking modal message box blocks background menu, status,
  shortcut, mouse, text, and focus routing immediately; its typed completion
  fires once for every normal close, cancellation, detach, and quit outcome.
- Blocking APIs reject re-entrant use deterministically and are tested in their
  supported outer-loop context; examples use typed non-blocking modal completion
  from handlers.
- Two Applications run on separate PTYs in one process; SIGINT/resize/wake for
  one cannot mutate or stop the other except according to documented host
  policy.
- Wake latency, in-flight interruption, exception restoration, and fatal-signal
  restoration are automated under the applicable sanitizers.
- Headless and PTY scripts cover capability success/non-response/change,
  tmux/screen/Linux-console degradation, suspend/resume, and record/replay; the
  restored session and re-presented frame meet M3's literal byte guarantees.

**Completion audit (2026-08-09; local macOS).** The current implementation
meets the literal package criteria. `test_application.cpp` covers top-modal
Tab/Shift-Tab/F1, local-command exclusion, text, mouse/capture, nested scopes,
detach-safe focus restoration, post/wake, and re-entrant-pump rejection.
`test_dialog_presentation.cpp` plus the message, file, and directory dialog
suites cover typed non-blocking completion, vetoes, host-shutdown cancellation,
and the blocking outer-loop-only family. Public ownership and loop contracts
are documented in the public headers and `docs/terminal-host-integration.md`.
Hello and Gallery command handlers now use `present_message_box` with typed
completion rather than the raw `Desktop::present_modal` primitive.

`test_posix_terminal.cpp` supplies the real-PTY evidence: bounded in-flight
wake and quit interruption; exception and fatal-signal restoration;
SIGTSTP/SIGCONT re-entry, resize, and byte-identical re-presentation; probe
success, non-response, late-reply rejection, and resize re-probing; named
ModernVt/tmux/screen/Linux-console degradation; record/replay; and two
Applications on separate PTYs with isolated SIGINT, resize, wake, and quit.
The headless profile-transition tests provide the deterministic counterpart.
Normal, ASan/UBSan, and TSan each passed all 10 CTest gates and 1,368 aggregate
test assertions, with no sanitizer finding. `git diff --check`, the layer
check and self-test, and the provenance check and self-test passed as well.

The queue state is therefore **Implementation complete — gate pending** rather
than `Done`: platform-wide and release gates remain with their explicit later
owners, not silently claimed by this local evidence.

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

**Implementation evidence (2026-08-09, local macOS).** D-039 supersedes the
inconsistent D-025 placement and establishes the lower-layer model: `Clock`,
`FileSystem`, and `ClipboardWriter` are core contracts; `PosixClock`,
`PosixFileSystem`, and `TerminalClipboardWriter` are explicit `term`
adapters. Terminal poll/wait/wake remains an instance-specific terminal-session
contract. `Application` can accept an explicit clipboard bridge and owns its
terminal-backed default bridge per instance. `MemoryClipboardWriter` joins the
existing `ManualClock` and `MemoryFileSystem` deterministic implementations.
The public examples compose their terminal clipboard bridge explicitly.

`test_platform_services` proves deterministic export, the per-Application
default adapter, paste-import/non-export separation, explicit terminal
adaptation, and isolation between two Applications. `layer_checker_self_test`
generates every forbidden source→target layer edge and proves the checker
rejects each. Normal, ASan/UBSan, and TSan configurations each completed all
ten CTest gates with **1362** aggregate unit/script tests and no sanitizer
diagnostic. `docs/terminal-host-integration.md` records the public construction
and lifetime rules. This is implementation evidence only: the three-platform
matrix and release-gate audit remain WP-37/WP-38 work, so it is not a global
acceptance claim.

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

**Implementation evidence (2026-08-09; local macOS).** D-040 replaces the
impossible D-023 guarantee with a protocol-honest 50 ms candidate-end quiet
boundary. `InputDecoder` keeps data received before that boundary in a
sanitized paste `TextEvent`; a later candidate makes the earlier marker safe
literal recovery text, and timeout or input disconnect returns the incomplete
payload through that same safe path. The public recovery bit allows an
application to present its own diagnostic without letting the decoder invent a
command.

`test_input_decoder.cpp` covers exact-deadline completion, embedded and nested
delimiters, command chords, OSC-shaped bytes, every two-part fragmentation,
and disconnect recovery. `test_headless_terminal.cpp` and
`test_posix_terminal.cpp` exercise the terminal-facing timeout/disconnect
paths. `test_paste_security.cpp` drives a focused `InputLine` through
`Application::step`, proves that a `Ctrl+Q` placed after an embedded delimiter
does not invoke its command handler, and asserts the recovered text in the
captured styled-cell frame. Normal, ASan/UBSan, and TSan builds each passed all
10 CTest gates and 1,368 aggregate test assertions; `git diff --check`, layer
check plus self-test, and provenance check plus self-test also passed.

This is implementation evidence, not a global acceptance claim. The decoder
fuzz target and corpus reuse these semantic assertions when WP-33's portable
fuzz infrastructure becomes available; the Windows/Linux matrix remains
WP-37/38 work.

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

**Implementation checkpoint (2026-08-09; package remains gate pending).**
Unicode 15.1.0 property tables and all 1,187 official extended-grapheme
conformance cases are generated reproducibly from published UCD files with
pinned hashes. `clip_to_width` and `elide_to_width` provide the single
grapheme/cell-safe truncation policy; Button, Window, Menu, StatusLine, Table,
and Memo now use it or explicit grapheme storage. Targeted normal and ASan
checks pass. The repository-wide test process still aborts in its pre-existing
POSIX-terminal lifecycle path, so this package cannot be marked Done until
WP-32 isolates that failure and all required platform gates are green.

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

**Implementation checkpoint (2026-08-09; package remains in progress).**
`Application` now composes a retained base surface with independent Window and
popup layers. A Window owns and repaints its local backing only when its own
content or size changes; translation updates only its compositor layer. Popup
backings follow the same rule: attachment and removal explicitly schedule a
layer recomposition, a same-size translation preserves local pixels, and a
content invalidation or resize repaints exactly that popup backing. The public
`ui::InvalidationKind` distinguishes content damage from geometry damage, so a
retained owner never has to guess whether its child called `invalidate()` or
`set_bounds()`. The Compositor holds instance-owned damage scratch storage,
resolves non-overlap damage once, and accepts a per-layer shadow clip so
Desktop keeps window and drop-down-menu shadows inside its content area.
Presenter reuses its output buffer and does not emit a repeated unchanged
cursor state.
Deterministic counters expose window content repaints, cells composed, and
bytes emitted; executable tests prove translation performs no content repaint,
popup lifecycle/content semantics are preserved, resize repaints only the
resized backing, and real mouse dragging stays within its old/new rectangles
and shadows. The scene benchmark now fails on steady-state, single-cell, or
complete retained-window-move composition budget regressions (696 cells and
6,144 Presenter bytes for the documented 30x10 non-overlapping move).
Test-executable allocation instrumentation now makes the warmed compositor,
raster layer movement, Presenter, dispatch, traversal, timer delivery, and
posted-work paths fail if they allocate; move tests also gate Presenter bytes.
The remaining WP work is the named-host/sanitizer evidence required by the
Acceptance section.

**Verification checkpoint (2026-08-09).** Before the popup lifecycle update,
the complete 1,383-test suite plus visual, provenance, layer, and scene-budget
CTest gates passed on `macos-26.5.2-arm64-m1-max-32g` under both
`address,undefined` and `thread` sanitizers. This update rebuilt the normal,
ASan/UBSan, and TSan test executables and passed retained-popup/menu-shadow,
mouse-drag, invalidation-kind, and warmed allocation/raster regressions in
each sanitizer mode. A 16-cycle retained Window plus shadow-casting-dropdown
open/remove stress test also passes under ASan/UBSan and TSan without stale
scene content. The normal visual, provenance, layer, and scene-budget CTest
gates also pass. CI now runs both sanitizer configurations on Linux. The
required Linux and Windows host results remain WP-32/WP-37 matrix work; the
pinned-host p99 procedure is recorded in `docs/performance.md`.

**Implementation closure (2026-08-09; gate pending).** The WP-31 code work is
complete: retained Window and popup backing lifecycles, content-versus-geometry
damage provenance, retained menu shadows, composition/Presenter counters,
failing scene and real-drag budgets, and warmed raster/dispatch/presentation
allocation checks are all executable. This is deliberately not a milestone or
release acceptance claim. The remaining named-host p99 capture automation and
published platform matrix evidence belong to WP-32/WP-37; their eventual
results remain required to accept the affected M2/M3/M4/M5 exits.

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

**Implementation closure (2026-08-09; gate pending).** `cktest` now identifies
the defining source suite for every case and accepts exact case, suite, filter,
and deterministic shard selection. Its contract-death primitive re-executes the
selected case: POSIX validates `SIGABRT` after `exec`, while Windows uses a
separate `CreateProcess` child and rejects the runner's own non-termination
sentinel. Consequently, the portable test sources no longer include POSIX
headers or call `fork`/`wait`; PTY and POSIX-filesystem coverage is compiled
only on UNIX and labelled `platform;posix`, while portable assertion contracts
continue to run on every target.

CTest now registers bounded source suites rather than one monolithic test. The
45-case PTY contract is deliberately split into four deterministic, 30-second
shards, and each emitted `RUN source:case` line locates a crash inside a shard.
The configured local inventory contains 90 CTest entries: 64 unit, 8 script,
11 golden, 1 benchmark, 6 platform (including 4 PTY) and an explicit 0
fuzz-corpus entries, the latter owned by WP-33. `report_test_inventory.py`
publishes that same label-based inventory locally and into GitHub Actions job
summaries. The release workflow exposes unit, scripted/policy, golden/visual,
platform filesystem, POSIX PTY (or an explicit pending ConPTY note),
fuzz-corpus inventory, benchmark, and sanitizer-support checks as separate
named steps; Debug ASan/UBSan and TSan remain explicit Ubuntu jobs.

Local macOS Release verification rebuilt `cvision_tests`, passed all 86
non-PTY CTest entries, and passed all four PTY shards. Combined Debug
ASan/UBSan and TSan rebuilds each passed focused post/wake and
two-application-PTY paths; ASan/UBSan also passed the portable
self-reexecuting assertion path. Full hosted macOS/Linux/Windows Release and
sanitizer matrices, including the required full TSan race result, are still
gate evidence; Windows ConPTY itself remains WP-37. This is therefore an
implementation closure only, not an M0/M3/M4 acceptance claim.

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
- review the generated semantic visual diff before D-034 promotion. Once the
  package-specific implementation and evidence below hold, use
  `Implementation complete — gate pending`; WP-33, WP-32/WP-37, and WP-38 own
  the remaining fuzz, hosted-matrix, and final-promotion evidence.

**Implementation update (2026-08-09).** WP-32A is now in progress with the
previously missing local implementation and executable evidence landed:

- the virtual display models ED/EL, SU/SD, synchronized-output visibility,
  wide-cell-aware erase, opaque Sixel overwrite, resize clearing, clipping,
  multiple rasters, and malformed/repeat/CSI-limit rejection;
- Presenter tracks active raster coverage across frames, forcing the old and
  new coverage to repaint when a raster moves, changes, or disappears so the
  exact terminal bytes clear stale virtual pixels;
- hash-pinned pixel-plane fixtures cover hand-authored protocol overwrite,
  scroll, clipping, multiple-raster, and resize cases, while an independent
  Presenter-to-virtual-display fixture covers proportional partial occlusion;
- paired Gallery Sixel/NoGraphics visual fixtures, byte-stream corruption,
  capability restoration, and the raw-byte / symbolic-scene / decoded-visual
  CTest and CI categories are executable; and
- the accepted VT/Sixel subset and parser bounds are reference-documented.

This is not an acceptance or milestone-closure claim. WP-33 still owns the
fuzz corpus, WP-32/WP-37 own the full hosted cross-platform evidence, and
D-034 promotion still requires the generated semantic visual-diff review.

**Evidence record (2026-08-09).** The full local CTest matrix passed **92/92**:
13 golden, 5 decoded-visual, 1 raw-byte, 1 symbolic-scene, 4 PTY, 1 benchmark,
and 6 platform-labelled suites. `suite_test_virtual_display`,
`suite_test_virtual_display_visual_golden`, `suite_test_presenter`, and
`suite_test_gallery_visual_golden` cover the new state, protocol, raw-byte,
paired-profile, corruption, documentation-SVG, and re-presentation contracts.
The focused visual paths also passed macOS Release, ASan, and TSan builds; the
examples-disabled test configuration builds without Gallery/docgen test
dependencies. Generated Sixel and NoGraphics Gallery SVGs were rendered and
reviewed: the first contains the expected gradient raster and the second the
mandatory `[image]` cell fallback with no raster plane. `layer_check`,
`provenance_check`, their self-tests, the budget gate, and `git diff --check`
were green. This record establishes package-specific implementation evidence;
it does not substitute for the remaining owners' global gates.

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

**Implementation complete — gate pending (2026-08-09).** WP-34A now has local
implementation evidence for the catalog gaps above:

- `DropdownMenu` supports checkable rows, nested submenus, one-level Esc
  dismissal, full-chain dismissal on leaf activation, and a focus-positioned
  Shift+F10 keyboard context-menu helper in addition to positional mouse
  context menus.
- `StatusLine` accepts item priorities and hides lower-priority entries on
  narrow widths while preserving original order and mouse routing for visible
  entries.
- `Desktop::snapshot()` / `Desktop::restore()` preserve direct-window
  geometry, z-order, active state, zoom state, restored bounds, and grow policy
  while ignoring windows that have left the Desktop.
- Message boxes resolve kind-specific text roles, and standard dialog factories
  use an injectable `widgets::StandardStrings` table for built-in titles and
  button labels without global mutable localization state.
- Window List activation, close, focus restore, modal completion, and
  first-letter type-ahead are covered through the dialog's public focus/input
  path. Directory picker and file/message/help/window-list factories continue
  to share the same default-button, Esc/cancel, focus-restore, and
  presentation-completion contracts from WP-27.

Focused suites passed:
`suite_test_menu`, `suite_test_status_line`, `suite_test_desktop`,
`suite_test_message_box`, `suite_test_standard_roles`,
`suite_test_standard_strings`, `suite_test_window_list_dialog`,
`suite_test_directory_picker`, `suite_test_file_dialog`,
`suite_test_help_viewer`, and `suite_test_dialog_presentation`.
The full local CTest matrix then passed **93/93** on macOS, including
`layer_check`, `provenance_check`, both self-tests, `hello_line_budget`,
`test_inventory`, and `scene_budget_gate`; `git diff --check` was also clean.
WP-38 still owns the exhaustive cross-scheme appearance matrix and final
promotion from gate-pending to `Done`.

### WP-34B — Controls, editing, selection, and scrolling closure

**Purpose.** Finish the ordinary form/control behaviors that headers and the
catalog currently promise but the implementation does not provide.

**Work.**

- Implement Label buddy mnemonic activation, StaticText alignment, Button
  repeat-on-hold, and complete CheckGroup/RadioGroup mnemonic behavior plus
  tri-state checks.
- Complete InputLine clipboard cut/copy/paste, pointer drag selection, history
  cycling, and undo; complete Memo clipboard, bounded undo, word-wrap toggle,
  and grapheme navigation using WP-30 primitives. ComboBox owns dropdown-style
  history per D-041.
- Give ListView true injected-Clock double-click semantics and finish its
  baseline search/selection behavior. D-041 formally moves virtualized and
  columned ListView models out of the 0.1 controls gate.
- Complete ComboBox, TabControl, Progress, Scrollbar auto-hide, and ScrollViewport
  wheel delivery over descendant content. D-041 classifies scroll-region diffs
  as renderer/performance optimization rather than a 0.1 widget behavior.

**Acceptance.**

- Every affected control is keyboard- and mouse-complete, deterministic under
  an injected Clock, and grapheme-correct without host clipboard/clock access.
- Editing regressions cover selection, clipboard, undo/history, validation,
  masking/password modes, focus loss, destruction, and terminal shrink.
- Every Controls catalog cell is implemented with linked unit and interaction
  evidence or explicitly removed from 0.1 by a settled decision.

**Implementation complete — gate pending (2026-08-09).** WP-34B closes the
control-surface gap locally: Label buddy mnemonics, StaticText alignment,
Button key-repeat activation, CheckGroup tri-state and complete option
mnemonics, InputLine clipboard/drag/undo, Memo clipboard/drag/undo/word-wrap,
Clock-based ListView double-click activation, Scrollbar auto-hide,
ScrollViewport wheel bubbling over descendants, and new ComboBox, TabControl,
and Progress widgets are implemented. D-041 records the settled 0.1 scope
change for virtualized/columned list-shaped models and scroll-region byte-diff
optimization.

Local evidence: `cmake --build build --target cvision_tests --parallel 4`
passed on macOS. Focused WP-34B CTest suites
`suite_test_combo_box`, `suite_test_tab_control`, `suite_test_progress`,
`suite_test_memo`, `suite_test_widgets`, `suite_test_option_group`,
`suite_test_list_view`, `suite_test_scrollbar`, and
`suite_test_scroll_viewport` passed 9/9. The full local CTest matrix then
passed **96/96**, including `layer_check`, `provenance_check`,
`provenance_checker_self_test`, `test_inventory`, `hello_line_budget`, and
`scene_budget_gate`; `git diff --check` was also clean.

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

**Progress checkpoint (2026-08-09).** TextView now supports structured spans,
inline attributes, activatable link spans, keyboard and mouse link activation,
and deterministic OSC 8 text export. HelpProvider now exposes provider-backed
index and keyword search, and MemoryHelpProvider supplies deterministic sorted
index/search results. D-041-aligned Table and Tree comments/catalog rows now
state the materialized 0.1 model and move virtualized providers post-0.1.
FileDialog now exposes options for file filters, hidden-entry toggling, and
recent-location history, uses FileSystem path normalization for separator
portable accept/completion behavior, and keeps all listing/completion tests on
MemoryFileSystem. Canvas now derives backing pixel size from injected cell
metrics on metric or widget resize, and ImageView/Canvas focused tests assert
their raster anchors, mandatory fallback, and dual-space mouse forwarding.

Local evidence: `cmake --build build --target cvision_tests --parallel 4`
passed on macOS, and focused WP-34C suites `suite_test_text_view` and
`suite_test_help_viewer` passed 2/2; focused filesystem/dialog suites
`suite_test_filesystem` and `suite_test_file_dialog` passed 2/2; focused media
suites `suite_test_canvas` and `suite_test_image_view` passed 2/2; focused
data/layout suites `suite_test_table`, `suite_test_tree_view`, and
`suite_test_splitter` passed 3/3; focused standard-dialog suites
`suite_test_directory_picker` and `suite_test_window_list_dialog` passed 2/2.
The full local CTest matrix then passed **96/96**, including
`gallery_visual_capture`, `filebrowser_visual_capture`, `hello_visual_capture`,
`layer_check`, `provenance_check`, `provenance_checker_self_test`,
`test_inventory`, `hello_line_budget`, and `scene_budget_gate`; `git diff
--check` was also clean. WP-38 still owns promotion from gate-pending to
`Done` through the final cross-platform/cross-scheme visual and reference
matrix.

### WP-35 — Theme, command presentation, composition, and example ergonomics closure

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
- Add a widgets-layer command-presentation value for menu and status entries:
  it references one `CommandId` and may provide a display label/mnemonic for
  that surface. Handler, chord, enablement, context, and command identity stay
  in the registry; an override changes presentation only and cannot create a
  second action path. Define mnemonic conflict, rebind, retraction, and
  localization behavior in the public reference before implementation.
- Add an optional widgets-layer application-shell composition helper for the
  ordinary theme, Desktop, menu bar, and status line arrangement. It must retain
  the existing explicit low-level construction path; expose non-owning typed
  access to constructed chrome; own no process state; start no event loop; and
  keep `ui` independent of `widgets`. Its purpose is declarative common-case
  construction, not a configuration language or hidden policy.
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
  and retraction. A single quit command is golden- and script-tested while
  presenting as `Exit` (with its own mnemonic) in a File menu and `Quit` in a
  status line; both keyboard and mouse activation must execute the same command
  handler.
- Named command contexts work under ordinary, popup, and modal focus scopes.
- `examples/` passes automated zero-plumbing, zero-duplicate-chord, and
  zero-`static_cast<widgets::` checks and the authoritative hello line gate.
- The application-shell helper is covered by a minimal application that
  declaratively installs File/status chrome and presents the WP-27 modal info
  dialog; the corresponding low-level example proves no ownership, lifecycle,
  or loop-control capability was lost.
- The public construction examples meet VISION's one-screen/sane-defaults bar
  without exceptions hidden in status notes.

**Implementation evidence.** WP-35 added per-subtree theme override through
`ui::View`, removed Desktop's theme/role constructor plumbing, added named
command contexts with explicit push/pop activation and focus-ancestry
activation, command retraction, menu/status `CommandPresentation`,
`widgets::ApplicationShell`, typed frame-overlay insertion, live
`MenuBar::set_menus`, and disabled status-line rendering. `examples/hello`
now uses the shell helper and presents one quit command as `Exit` in File and
`Quit` in status; filebrowser uses typed frame overlays and command
presentation. Evidence is concentrated in `tests/test_wp35.cpp`, refreshed
hello goldens, `example_hygiene`, and `hello_line_budget`.

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

**Implementation evidence.** WP-36 makes width-dependent height participate in
`ui::Row`, `ui::Column`, `ui::Grid`, and `ui::Dock` through the internal layout
metric helper, so `StaticText::height_for_width()` affects actual child bounds
instead of remaining a widget-only query. The standard message, file,
directory-picker, help-viewer, and window-list factories now explicitly build
non-resizable dialog windows, matching descriptor dialogs' default. Acceptance
is concentrated in `tests/test_wp36.cpp`: hosted dialog wrapping reallocates
height across resize frames; every standard dialog factory is checked for the
fixed-size default; descriptor validation veto and Esc cancel run through real
Window hooks; and a repeated full/degraded/too-small/recovered shrink storm
proves deterministic frames, focus, chrome, damage, and recovery. Lower-level
regression evidence lives in `suite_test_layout`, `suite_test_grid`, and
`suite_test_dock`; filebrowser lazy expansion/Splitter/payload/callback
evidence remains in `suite_test_filebrowser_smoke`.

### WP-36A — Comprehensive examples and client capability demos

**Purpose.** Turn examples from a few smoke/integration samples into a client-
facing demonstration suite comparable in seriousness to classic TUI framework
demo programs: a new user can run the examples to understand what ckVision can
build, and maintainers can use the same examples as executable coverage for the
public API surface.

**Example set.**

- Keep `examples/hello` as the minimal one-screen application and
  `examples/filebrowser` as the real-filesystem master/detail application.
- Promote `examples/gallery` into the top-level showcase shell, or replace it
  with `examples/showcase` if the cleaner design is a fresh object graph. The
  showcase must expose navigable pages for every widget catalog family:
  controls, forms, selection widgets, tables/trees/lists, text/memo, standard
  dialogs, menus/status/commands, themes, graphics/canvas/images, and desktop
  window management.
- Add `examples/layouts`: a resize-focused application covering Row, Column,
  Grid, Dock, AnchorPane, Overlay, Splitter, margins/alignment, frame overlays,
  terminal-too-small degradation, and recovery.
- Add `examples/forms`: descriptor dialogs, validation veto, default/cancel
  buttons, focus restore, message/file/directory/window-list/help dialogs,
  localization via `StandardStrings`, command contexts, and close veto.
- Add `examples/workbench`: a practical multi-window application combining
  `Memo`, `TextView` links/OSC8 export, `InputLine` completion/history,
  `Table`, `TreeView`, `TabControl`, `Progress`, clipboard operations, help,
  and status/menu command presentation.
- Add `examples/graphics`: `ImageView`, `Canvas`, Sixel/fallback switching,
  raster occlusion/scrolling, cell/pixel mouse, and capability degradation.

**Work.**

- Define a checked-in example coverage matrix:
  public widget/layout/capability → example → scripted interaction → golden or
  visual artifact → reference-doc section.
- Implement every example as the established split target:
  `examples/<name>/<name>_app.{hpp,cpp}` as a linkable static library plus a
  thin terminal `main.cpp`; tests must drive the same app object graph as the
  interactive binary.
- Extend `example_hygiene` so comprehensive examples still use only public
  headers, typed insertion APIs, declarative command presentation, injected
  services, and no duplicate command/chord declarations.
- Add headless script suites for each example covering keyboard-only,
  mouse-only where applicable, resize/shrink/recovery, theme switching, modal
  routing, command enable/disable/retraction where relevant, and deterministic
  teardown.
- Add visual capture tools and documentation screenshots for the stable states
  that teach the API; raster examples need paired Sixel and NoGraphics captures.
- Update `docs/example-apps.md` into a real example guide: what to run, what
  each example demonstrates, what code pattern it teaches, and which tests pin
  the behavior.

**Acceptance.**

- Every public widget, layout container, standard dialog, command/theme surface,
  injected platform service, graphics/degradation path, and desktop/window
  behavior has at least one named example and at least one scripted test
  exercising it through public API.
- The examples are useful as client templates: no demo-only private hooks, no
  hidden test-only objects in the interactive path, no hand-wired behavior that
  should be a library feature.
- The example coverage matrix has no missing catalog row and no smoke-only row;
  rows that cannot be visually demonstrated explain why and point to stronger
  executable evidence.
- The documentation page includes screenshots for the teaching states and
  command lines for building/running every example.
- WP-38 can consume the matrix directly for its final release-gate
  traceability report instead of rediscovering example coverage from scratch.

**Implementation evidence.** WP-36A adds the checked-in
`plans/example-coverage.md` matrix and four new split examples:
`examples/layouts`, `examples/forms`, `examples/workbench`, and
`examples/graphics`. Each follows the established app-library plus thin
terminal-main pattern, and each has a focused headless smoke suite wired into
CTest: `suite_test_layouts_smoke`, `suite_test_forms_smoke`,
`suite_test_workbench_smoke`, and `suite_test_graphics_smoke`. The examples
cover the existing layout catalog, descriptor/modal form patterns, practical
text/data application widgets, and raster/fallback graphics paths while keeping
`hello`, `gallery`, and `filebrowser` as the minimal/showcase/filesystem
examples. `docs/example-apps.md` now describes every runnable example and points
the final release audit to the matrix. The complete WP-38 visual/state/theme
matrix remains the release-gate owner for exhaustive screenshots; WP-36A
provides the executable public-path example coverage.

Focused verification passed on macOS on 2026-08-09:
`suite_test_layouts_smoke`, `suite_test_forms_smoke`,
`suite_test_workbench_smoke`, `suite_test_graphics_smoke`, `example_hygiene`,
`provenance_check`, and `provenance_checker_self_test`. Full CTest also passed:
103/103 tests, including `scene_budget_gate`.

### WP-36B — Common widget and UI component expansion

**Purpose.** Fill practical client-application gaps that were not present in
the original classic-widget catalog but are expected in a modern terminal UI
library. This package keeps the 0.1 surface coherent by adding common
components as first-class widgets instead of letting every example/application
hand-roll them.

**0.1 component set.**

- `CalendarView`: month grid with ISO week-row option, keyboard/mouse day
  selection, min/max date range, disabled dates, today/current-day styling, and
  locale-free caller-supplied month/weekday labels.
- `DatePicker`: input/dropdown or dialog-backed date selection built on
  `CalendarView`, using an injected date value rather than reading wall-clock
  time from the library.
- `TimePicker`: hour/minute/second selection with 12/24-hour display controlled
  by options and no locale/global state.
- `SpinBox` / numeric stepper: integer value entry with min/max/step, keyboard
  arrows, mouse buttons/wheel, validation feedback, and optional text editing.
- `Slider`: horizontal value selection for bounded numeric values, keyboard and
  mouse complete, with optional tick labels.
- `SearchBox` / filter bar: input line plus clear button/status, intended to
  drive ListView/Table/Tree/TextView filtering without each application
  reimplementing the pattern.
- `CommandPalette`: searchable command popup over `ui::CommandRegistry`,
  honoring command contexts, enablement, labels/chords, and retraction.
- `ToolBar` / action bar: compact command-button row with overflow policy,
  usable as top/bottom dock chrome and driven by the same command presentation
  model as menus/status lines.
- `BreadcrumbBar`: path/navigation component with segment activation, overflow
  elision, keyboard focus, and optional filesystem/path separators supplied by
  the caller.
- `PropertyInspector`: two-column property grid for name/value editing using
  typed editors (text, bool, enum, numeric, date/time where available), with
  validation veto and change notification.
- `Wizard`: multi-page guided workflow shell with Back/Next/Finish/Cancel,
  per-page validation, progress/step indicator, and focus restore.
- `NotificationCenter` plus toast/notification popup: transient and persistent
  messages with severity roles, keyboard dismissal, no hidden timers in library
  code unless driven by injected clock/timers.
- `Tooltip` / popover: focus or pointer triggered explanatory popup with
  deterministic placement and keyboard-accessible invocation.

**Explicit post-0.1 candidates unless promoted by decision.**

- Tree-table / outline table with hierarchical rows and columns.
- Data charts beyond `Progress` and `Canvas`; applications can draw charts on
  Canvas for 0.1.
- Dockable/floating tool palettes and MDI-style docking systems.

**Work.**

- Update the widget catalog and public docs before implementation so every new
  component has a behavior contract, theme roles, keyboard grammar, mouse
  grammar, accessibility/focus contract, and deterministic-test strategy.
- Implement components without global mutable state, locale reads, filesystem
  reads, or wall-clock reads. Calendar/date/time labels and "today" values are
  supplied by the application or injected services.
- Reuse existing primitives where appropriate: `CommandRegistry`,
  `CommandPresentation`, `Popup`/menu infrastructure, `InputLine`,
  `CompletingInputLine`, `ListView`, `Table`, `DialogPresentation`, and
  `StandardStrings`.
- Add every new component to the comprehensive examples from WP-36A and to the
  example coverage matrix; no new widget may land without a client-facing
  demonstration.
- Add unit, script, golden/visual where relevant, and documentation coverage for
  each new component.

**Acceptance.**

- Calendar/date/time selection is deterministic, keyboard/mouse complete,
  range-aware, theme-covered, and does not consult locale, timezone, clock, or
  filesystem from library code.
- CommandPalette and ToolBar are command-registry surfaces, not parallel command
  systems; they reflect enablement, context, rebindings, and retraction live.
- SearchBox, BreadcrumbBar, PropertyInspector, Wizard, notifications, and
  Tooltip/Popover each have public-path scripts and client-facing example pages.
- The widget catalog has no "common component" row without an owner, executable
  evidence plan, and post-0.1 decision if deferred.
- WP-38 can enumerate these components in the same state/scheme/reference
  matrix as the existing M5/M6 catalog.

**Implementation evidence.** WP-36B adds
`include/cvision/widgets/common_components.hpp` and
`src/widgets/common_components.cpp`, covering `CalendarView`, `DatePicker`,
`TimePicker`, `SpinBox`, `Slider`, `SearchBox`, `CommandPalette`, `ToolBar`,
`BreadcrumbBar`, `PropertyInspector`, `Wizard`, `NotificationCenter`, and
`Tooltip`. Date/time values, labels, "today", command data, separators, and
notification timing are all caller-supplied; the implementation performs no
locale, timezone, clock, filesystem, or global-registry reads. `CommandPalette`
and `ToolBar` read the owning `ui::CommandRegistry` through the existing
application context instead of inventing a parallel command model.

Example integration is public-path: `examples/forms` now shows date/time,
numeric, slider, and wizard components; `examples/workbench` shows search,
toolbar, command palette, breadcrumb, property inspector, notifications, and
tooltip/popover behavior. `suite_test_common_components` covers the new
component family directly, and the forms/workbench smoke suites assert the
example object graphs contain the WP-36B components.

Focused verification passed on macOS on 2026-08-09:
`suite_test_common_components`, `suite_test_forms_smoke`,
`suite_test_workbench_smoke`, `example_hygiene`, `provenance_check`, and
`provenance_checker_self_test`. WP-38 still owns the exhaustive
state/scheme/reference visual matrix for these new components.

### WP-36C — Client documentation and tutorial revamp

**Purpose.** Replace the current verification-heavy documentation with
credible client-facing library documentation. The docs must teach how to write
ckVision programs: object hierarchies, complete source, code snippets,
screenshots, widget selection guidance, layout/dialog/command patterns, and
platform-service usage. The implementation plan is
`plans/documentation-revamp.md`.

**Work.**

- Add a real tutorial path: getting started, full Hello walkthrough, object
  model, layout guide, widget gallery, dialogs/commands, themes/rendering,
  graphics, platform services, and an API index.
- Rewrite Hello documentation first. It must include full source, generated
  screenshots, object hierarchy, command/menu/status wiring, modal-dialog flow,
  and copy/build/run instructions.
- Convert example documentation from verification ledger to client guide:
  every example must show what pattern it teaches, relevant code, screenshots,
  and the view/window/widget hierarchy.
- Add a widget gallery covering every public widget and WP-36B common
  component with code, screenshot or justified exception, keyboard/mouse
  behavior, callbacks/state, examples, tests, and common mistakes.
- Add doc tooling: source/snippet extraction, screenshot generation for the new
  pages, link checking, widget/API coverage checking, and CTest gates.
- Add `docs/coverage.md`: public class/header → guide section → snippet →
  screenshot → example → test.

**Implementation closure (2026-08-09; gate pending).** The client-facing
documentation set now comprises Getting Started, the full source-backed Hello
tutorial, object model, layout/dialog/command/theme/graphics/platform-service
guides, widget gallery, API index, and the checked-in coverage matrix. The
snippet extractor validates 16 generated source blocks; the documentation
checker validates every public widget header, local link, and screenshot
reference; and four CTest documentation gates enforce those contracts. Eight
headless capture tools generate 27 declared screenshots from the real example
graphs (including the public-API navigation component gallery alongside Hello,
Gallery, File Browser, Layouts, Forms, Workbench, and Graphics), including
paired Sixel/no-graphics graphics frames. The documentation generator
successfully rendered 22 HTML and 22 PDF pages locally. Capture work also
exposed and corrected the zero-bounds standard-dialog presentation defect, with
a modal-bounds regression test and updated Hello golden. The full local CTest
matrix passed 113/113. Cross-platform/release documentation evidence remains
WP-37/WP-38 work, so this is not an M6 or release-closure claim.

**Acceptance.**

- A new C++ client can write and run the documented Hello app without reading
  tests or private implementation files.
- Every public widget/component has a client-facing documentation row with a
  compile-backed snippet and generated screenshot, or an explicit reason why a
  screenshot is not meaningful.
- Documentation generation fails on stale/missing snippets, missing
  screenshots, broken links, or undocumented public widgets.
- `README.md` points new users to the tutorial path before planning,
  verification, and internal-reference documents.
- WP-38 can consume `docs/coverage.md` directly for the release traceability
  report.

### WP-39 — Revisioned editor document and Unicode editing core

**Purpose.** Establish the reusable document model required for a professional
text/source editor without turning `Memo` into an accidental editor. The
binding design is [editor-architecture.md](editor-architecture.md); the client
and acceptance target is [editor-delivery-target.md](editor-delivery-target.md).

**Work.**

- Add the documented editor layer placement to `ARCHITECTURE.md` before source
  code, preserving every existing dependency direction and the zero-dependency
  rule.
- Implement `EditorDocument`, revisioned positions/ranges, transactions,
  bounded undo/redo, deterministic observer lifetime, LF normalization, and
  grapheme-safe line/column lookup with a large-document piece index or
  equivalent balanced representation.
- Define explicit options and error results for invalid UTF-8, BOM/encoding,
  newline conventions, document limits, and undo limits. Do not let widget
  paint or direct filesystem access decide any of them.
- Add unit/property/fuzz coverage for edit sequences, transactions, undo/redo,
  malformed input, and Unicode boundaries, plus an enforceable edit/lookup
  benchmark on the pinned host.

**Acceptance.**

- Every edit is revisioned, atomic at transaction scope, reversible when within
  configured history, and leaves positions and line indices valid.
- No exposed cursor, selection, deletion, or index result splits a grapheme
  cluster; tests include combining, wide, and malformed boundary cases.
- Storage, edit, and lookup work scales without whole-document copying for a
  local edit; the benchmark has a stated threshold that fails on regression.
- The public reference defines ownership, position invalidation/rebasing,
  errors, and all deterministic metadata behavior.

### WP-40 — TextEditor interaction, viewport, and editor status

**Purpose.** Turn the editor document into a first-class, keyboard-and-mouse
complete widget with an independently composable status/indicator model. This
package follows [editor-architecture.md](editor-architecture.md) and
[editor-delivery-target.md](editor-delivery-target.md).

**Work.**

- Implement `TextEditor` separately from `Memo`/`TextView`: focus, caret and
  selection state, insert/overwrite mode, editing/navigation commands,
  scrolling, gutter, line/column mapping, read-only/disabled behavior, and
  injected clipboard operations.
- Implement `EditorStatusModel` and frame/status presentation hooks without
  hard-coding a window shell. Ensure resize, narrow terminals, wide Unicode,
  and visible caret/selection clipping are deterministic.
- Add public-path keyboard and mouse scripts for typing, selection, drag,
  scroll, focus traversal, clipboard, command contexts, modality, and
  destruction/reentrancy; add four-theme styled-cell goldens.

**Acceptance.**

- A client can edit a shared `EditorDocument` through the public widget API;
  all ordinary input and viewport behavior has script and golden evidence.
- Gutter, caret, selection, and indicator report correct document positions
  after edits, undo/redo, scrolling, and resize without splitting graphemes.
- No viewport frame does unbounded off-screen document work; the budget is
  measured and enforced.

### WP-41 — Syntax profile engine and incremental styling

**Purpose.** Build a deterministic, extensible highlighter architecture before
shipping language grammars. The normative contract is
[editor-architecture.md](editor-architecture.md), with exit evidence in
[editor-delivery-target.md](editor-delivery-target.md).

**Work.**

- Add an instance-owned `SyntaxProfileRegistry`, stable profile IDs,
  explicit/deterministic detection inputs and tie behavior, highlighter factory,
  semantic token kinds, editor theme roles, and plain-text fallback.
- Implement the compiled rule-set building block and `ISyntaxHighlighter`
  extension seam. No mandatory grammar engine, dynamic global registration, or
  unbounded runtime-regex grammar is allowed.
- Implement the line-state `SyntaxCache`: changed-line invalidation, forward
  relexing to a state/span fixed point, visible-line priority, and bounded
  off-screen continuation without a hidden worker thread.
- Prove detector determinism, token range validation, malformed-input safety,
  cache convergence, paint precedence (syntax/search/selection/caret), and
  incremental relex performance with unit/fuzz/golden/benchmark evidence.

**Acceptance.**

- Equal editor inputs and explicit profile metadata produce byte-identical
  styling regardless of paint order, process state, locale, or elapsed time.
- A local edit relexes only the necessary suffix and stops at the documented
  fixed point; multiline state remains correct.
- All four built-in themes define every editor/syntax role, preserve readable
  selection/caret precedence, and fall back safely to plain text.

### WP-42 — Standard JSON, YAML, and Bash profiles

**Purpose.** Ship useful default syntax profiles while making extension a
documented client capability rather than a hidden implementation privilege.
The profile contract is in [editor-architecture.md](editor-architecture.md)
and [editor-delivery-target.md](editor-delivery-target.md).

**Work.**

- Implement JSON (`.json`, `.jsonc`), YAML (`.yaml`, `.yml`), and Bash
  (`.sh`, `.bash`, Bash shebang) detectors and highlighters, along with explicit
  profile selection and plain-text fallback.
- Cover the documented minimum categories, multiline lexical state, malformed
  text, long lines, and adversarial nesting without unbounded behavior.
- Write a profile-author guide showing declarative rule-set and custom
  stateful-highlighter registration, stable identifiers, detection tests,
  theme roles, fixtures, and extension lifetime.

**Acceptance.**

- Checked-in fixtures and public-path goldens demonstrate valid and malformed
  JSON/YAML/Bash in all built-in themes, including strings/comments and
  multiline state where each language requires it.
- Filename, shebang, explicit selection, fallback, and detection tie behavior
  are executable and documented.
- An independent sample profile compiles and runs through the public registry
  API without private headers or global side effects.

### WP-43 — Editor search, replace, and command integration

**Purpose.** Complete the everyday editing workflow with safe search/replace
and ordinary application command presentation. Design and delivery constraints
remain [editor-architecture.md](editor-architecture.md) and
[editor-delivery-target.md](editor-delivery-target.md).

**Work.**

- Implement revision-bound literal search, case/whole-word/direction options,
  visible match state, replace-one, atomic replace-all, and an optional bounded
  injected matcher extension.
- Compose standard Edit/Search commands with command contexts, menus, status
  hints, enablement, focus, modality, and editor status without global routing.
- Add unit/script/golden coverage for zero/one/many results, stale ranges,
  undo after replace-all, command disablement/read-only state, nested modal
  dialogs, and selection/search/caret style precedence.

**Acceptance.**

- Replace-all never applies a stale plan or leaves a partially changed
  document; it is one undoable transaction.
- Search cannot invoke an unbounded standard-library backtracking path; any
  extension exposes an explicit bounded/cancellable contract.
- Every command is usable through keyboard, menus, and status presentation as
  applicable and restores normal routing after dialogs close.

### WP-44 — File-editor workflow, editor example, and documentation

**Purpose.** Deliver the client-ready file editor and the public proof that the
editor subsystem is usable. This package implements the workflow in
[editor-architecture.md](editor-architecture.md) and the example/documentation
target in [editor-delivery-target.md](editor-delivery-target.md).

**Work.**

- Extend the injected platform filesystem boundary with explicit read,
  metadata/fingerprint, and atomic replacement operations; no editor code may
  access host files directly.
- Implement `FileEditorController` and optional `EditorWindow`: open/save/save
  as, clean revision/dirty marker, encoding/newline preservation policy,
  changed-on-disk conflict, Save/Discard/Cancel close veto, title and status
  composition, and failure diagnostics.
- Add `examples/editor` plus source-backed API/reference/tutorial material,
  screenshots, hierarchy, command table, profile author guide, and generated
  documentation checks in the style established by WP-36C.
- Exercise the complete workflow through `MemoryFileSystem` and public
  application scripts; add goldens for highlighted/open/modified/search/
  conflict/close states and all theme/degradation cases.

**Acceptance.**

- A client can use a document/editor without the file controller, or use the
  controller/window to obtain the documented safe file lifecycle.
- Save never silently overwrites a detected external change, marks a document
  clean only after full success, and preserves or explicitly converts declared
  encoding/newline metadata.
- `examples/editor` is runnable from its documentation and demonstrates all
  standard profiles, fallback, editing, search/replace, save, conflict, and
  close-veto paths through public API only.

### WP-45 — Embedded-terminal emulator and containment foundation

**Purpose.** Build the private off-screen terminal model that makes an embedded
terminal safe by construction. This is the first package for the binding
[embedded-terminal architecture](embedded-terminal-architecture.md) and
[delivery target](embedded-terminal-delivery-target.md); it must establish
D-042's layer and containment rules before a child process or widget exists.

**Work.**

- Add the D-042 layer placement and core-typed child snapshot contracts to the
  architecture, then implement the terminal emulator's primary/alternate cell
  buffers, cursor, SGR, UTF-8, edit/erase, scrolling, DEC modes, scrollback,
  declared capability replies, diagnostics, and explicit resource limits.
- Define explicit `TerminalLaunchSpec`, child capability profile, query/OSC
  policy, and `TerminalSubsessionOptions` values. No profile or launch
  environment may come from hidden ambient reads.
- Define the strict output boundary: parser input only updates private model
  state; it has no reference to the outer `Terminal` writer, scene, or widget.
- Add unit/property/fuzz evidence for normal and malformed sequences,
  alternate-screen transitions, UTF-8 boundaries, control-string recovery,
  resource limits, deterministic query replies, and two isolated emulators.

**Acceptance.**

- The off-screen model can represent the documented xterm-compatible child
  profile as core-typed cells/cursor/raster placeholders without I/O.
- No raw child byte has any outer-presenter path, including unsupported OSC/DCS
  and graphics control strings.
- Parser work/memory is bounded and recoverable for adversarial inputs; fuzzing
  has corpus seeds for every parser mode and limit boundary.

### WP-46 — POSIX PTY child sessions and basic TerminalView

**Purpose.** Connect the contained model to a real macOS/Linux child process
through the correct `term` ownership boundary, then expose a basic view. The
contract is [embedded-terminal-architecture.md](embedded-terminal-architecture.md)
and [embedded-terminal-delivery-target.md](embedded-terminal-delivery-target.md).

**Work.**

- Implement explicit POSIX PTY spawn, nonblocking reads/writes, process exit,
  signal/close policy, terminal-size propagation, and safe teardown in `term`.
- Let `Application` multiplex bounded child-session readiness with the outer
  terminal wait/wake loop, without a global registry or worker thread.
- Implement basic `TerminalView` snapshot painting, focus, keyboard/text input
  forwarding, visible exit/error state, and safe detach-before-destroy
  lifetime rules.
- Add real PTY scripts on macOS/Linux for Bash/readline, spawn failure, output
  flood, exit, close, input echo, resize, and two simultaneous sessions.

**Acceptance.**

- Bash runs in a `TerminalView` without the child attaching to or changing the
  parent application's outer terminal session.
- Child exit/close/error paths leave no handles, zombies, stale callbacks, or
  parent terminal-mode changes.
- A noisy child cannot starve outer input or frame presentation beyond the
  documented bounded budget.

### WP-47 — TerminalView interaction, resize, and nested ckVision sessions

**Purpose.** Make the terminal window genuinely usable for ordinary full-screen
terminal applications and a separately launched ckVision application. The
authoritative contract remains [embedded-terminal-architecture.md](embedded-terminal-architecture.md)
and [embedded-terminal-delivery-target.md](embedded-terminal-delivery-target.md).

**Work.**

- Complete input encoding for the child profile: keyboard, text, bracketed
  paste, eligible mouse reporting, a configurable parent escape chord, and
  explicit OSC/title/clipboard/hyperlink/notification/query policies.
- Implement resize ordering, pixel/cell metrics, scrollback, terminal
  selection/copy, alternate-screen presentation, view/window focus boundaries,
  and modeless/modal command scoping.
- Create an integration child that runs a second ckVision application through
  the private PTY, exercising independent session modes, resize, focus, and
  presentation within the parent window.
- Add public-path scripts and cell goldens for Bash, a full-screen alternate
  buffer sample, nested ckVision, mouse/paste, resize storms, parent window
  move/overlap/close, and return-to-parent input behavior.

**Acceptance.**

- The child sees only the terminal view's content extent and receives correct
  resize/input behavior; parent chrome and the escape chord remain local.
- A nested ckVision child is visually and input-wise confined to its terminal
  view while both parent and child remain responsive.
- Scrollback, selection/copy, alternate screen, focus restoration, and modal
  routing have public script and golden evidence.

### WP-48 — Contained child Sixel and terminal hardening

**Purpose.** Deliver the no-glitch/no-spill graphics guarantee, including
resource-hardened child Sixel handling. The detailed pixel and policy design is
[embedded-terminal-architecture.md](embedded-terminal-architecture.md) and the
acceptance matrix is [embedded-terminal-delivery-target.md](embedded-terminal-delivery-target.md).

**Work.**

- Implement a bounded Sixel decoder in the child emulator that produces
  RGBA/core image state plus mandatory fallback cells; it must never relay a
  child graphics sequence to the outer presenter.
- Bridge child images through `TerminalView` to ordinary clipped scene raster
  regions, preserving parent composition/occlusion/shadow/clear/close behavior
  and outer Sixel/no-graphics degradation.
- Cover palette/repeat/dimension/payload limits, malformed DCS recovery,
  raster clearing/alternate screen, resize, moving/resizing/overlapping parent
  windows, two child sessions, and child termination mid-image.
- Add parser fuzzing, allocation/work-limit gates, decoded outer-byte pixel
  goldens, and assertions that parent regions outside the terminal view stay
  unchanged.

**Acceptance.**

- Child Sixel is correct inside the terminal rectangle and cannot paint a cell
  or pixel outside it under move, resize, overlap, shadow, clear, close, or
  capability change.
- `NoGraphics` produces the deterministic child fallback with zero outer raster
  pixels; a Sixel parent profile proves decoded pixels from the actual outer
  presenter byte stream.
- Malformed or oversized graphics cannot hang, over-allocate, leak partial
  bytes, or corrupt another child/parent scene.

### WP-49 — Terminal example, documentation, and end-to-end acceptance

**Purpose.** Make the contained terminal usable and believable to clients,
rather than leaving it as internal PTY/parser evidence. It implements the
public proof point required by [embedded-terminal-delivery-target.md](embedded-terminal-delivery-target.md).

**Work.**

- Add `examples/terminal`: a resizable parent window with explicit launch,
  documented parent escape command, lifecycle diagnostics, Bash/full-screen
  scenarios, a nested ckVision child, and a child Sixel demonstration.
- Add a source-backed terminal guide, API reference, object hierarchy,
  launch-policy guide, security/containment explanation, generated screenshots,
  and exact build/run/test commands; integrate it into documentation coverage.
- Automate public-path PTY scripts and generated cell/pixel captures for all
  teaching states, including no-graphics fallback and explicit no-spill checks.

**Acceptance.**

- A client can follow the documented example without private headers or manual
  terminal setup and observe Bash, nested ckVision, resize, exit, and Sixel
  containment behavior.
- Documentation states clearly which child profile and policy is implemented,
  how to extend it, and why raw pass-through is prohibited.
- The example and guide meet the same source/screenshot/hierarchy/evidence
  gates as WP-36C and WP-44's editor example.

### WP-51 — SysInfo probe boundary and machine report panes

**Purpose.** Give the example set a second, differently-shaped instance of the
D-039 injected-impurity pattern, and the static half of the SysInfo
application. Its work and acceptance items are stated in full in
[sysinfo-example.md](sysinfo-example.md); this row is the queue's copy.

**Work.**

- Add `examples/sysinfo` with the established object-graph library plus
  POSIX-only interactive binary split.
- Define the `SystemProbe` interface and its host, processor, memory, volume,
  power and build report structures, with absence expressed as absence.
- Implement `PosixSystemProbe` (macOS and Linux) and `FixedSystemProbe`.
- Build the System, Memory and Volumes panes and the desktop chrome, with a
  refresh timer on the live figures.

**Acceptance.**

- Every pane renders headlessly against `FixedSystemProbe` with identical
  content on macOS and Linux.
- Only the POSIX probe translation unit includes a platform header or reads
  the environment; the object graph builds without any probe implementation.
- An unavailable field renders as an explicit absence, proven per report
  structure.
- A POSIX-only suite asserts the real probe's internal consistency and never a
  specific machine.

### WP-52 — Bar chart view, benchmark kernels, and the threaded run

**Purpose.** The comparative picture the example exists to show, and the
cancellable background work that fills it.

**Work.**

- Add `BarChartView`, cell-drawn through `scene::box_drawing`.
- Add the integer-index, floating-point and memory-bandwidth kernels with
  fixed work quanta, warm-up and best-of-N reduction.
- Add `BenchmarkService`: one worker, queued run, per-kernel progress, a
  cancel flag observed between kernels, results returned through
  `Application::post`.
- Add the Benchmarks window with picker, progress, Esc-cancel and the chart,
  comparing kernel against kernel on one stated scale and this run against the
  one before it. Reference bars from published figures are WP-55's, under the
  rule stated there: a bar this program did not measure is never presented as
  a measurement.
- Report the build configuration that took the measurements, and say plainly
  when it was not an optimized one.

**Acceptance.**

- Every kernel has a correctness test whose expected answer is independent of
  elapsed time; no test asserts a score, rate or duration.
- A scripted run reports non-decreasing progress, terminates, and delivers one
  result per selected kernel.
- Cancel is proven deterministically, not by timing.
- The chart states its scale, and every bar on it was measured by this program
  on this machine.

### WP-55 — Comparison bars from published figures

**Purpose.** Give a measured score something to be a score against, the way
the classic tools' tables of other computers did, without presenting a bar
this program never measured as a measurement. Stated in full in
[sysinfo-example.md](sysinfo-example.md).

**Work.**

- A reference table whose rows each carry a rate, the arithmetic that
  produced it, and the standard it came from.
- Reference bars drawn in a different character from measured ones, legended
  only when both kinds are present.
- A chart grouped by metric, each group normalized to its own longest bar.
- A picker choosing which metric gets its comparison.

**Acceptance.**

- A test redoes each row's arithmetic instead of pinning its number.
- A kernel with no published figure has no comparison bars at all.
- Reference bars appear only in their own metric's group.

### WP-56 — Storage throughput and its interface ceilings

**Purpose.** The disk question the classic tools answered, and the interface
ceilings that make its answer legible.

**Work.**

- A sequential write-then-read kernel over a file in a reader-chosen
  directory, sized and flushed to measure the device rather than the cache,
  opt-in because it writes to their machine.
- Interface reference rows with the arithmetic that produced each ceiling.

**Acceptance.**

- The benchmark writes only where it was told, removes what it wrote, and
  says how much it will write first.
- Cache defeat is stated in the source, including what it does not guarantee.
- No test writes to the machine running it.

### WP-53 — Cache hierarchy, multicore scaling, and host capabilities

**Purpose.** The two analyses that make the chart worth reading, and the
capability report only a terminal framework can produce about its own host.

**Work.**

- Add the pointer-chase latency kernel over a growing working set and its
  stair-step presentation.
- Add the 1..N thread scaling run charted against the ideal line, bounded by
  the probe's core count.
- Add the Terminal pane over
  `Application::terminal_capability_report_text`, the cell metric and the
  graphics answer.
- Render the latency data through `Canvas` where the host reports graphics and
  through `BarChartView` where it does not, from one data set.

**Acceptance.**

- The pointer chase is proven to visit every slot exactly once in a
  non-sequential order.
- The scaling run produces one sample per thread count and never exceeds the
  probe's core count.
- Both chart paths render the same data set and each has a capture.

### WP-54 — SysInfo report export, help, and documentation

**Purpose.** Let the example's output leave the program, and make the program
teachable to a client reading it as reference code.

**Work.**

- Compose the full report as text and Markdown and write it through the
  injected `FileSystem` from a save dialog.
- Add a help topic per pane and per benchmark.
- Add `docs/sysinfo-example.md`, the `docs/example-apps.md` row and section,
  generated screenshots, and the `plans/example-coverage.md` rows.

**Acceptance.**

- The export is produced against `MemoryFileSystem` in a test and contains
  every pane and no value the panes do not show.
- Every pane and benchmark answers F1 with its own topic, proven by a test
  that no topic is missing.
- The guide is source-backed by the shared snippet mechanism and its
  screenshots are generated.

**Implementation complete — gate pending (2026-08-25).** The text and Markdown
documents are composed from the same `system_rows`, `memory_rows`,
`volume_rows`, terminal report, and `BenchmarkResult` values used by the panes,
then written through the injected `FileSystem`. `test_sysinfo_report.cpp` is a
registered portable suite covering the complete document, both formats,
`MemoryFileSystem` export, failed writes, and every pane/benchmark help topic.
`docs/sysinfo-example.md` contains source-synchronized regions from the shipped
example; `capture_sysinfo_screenshots` drives `SysInfoApp` with
`FixedSystemProbe`, `FixedBenchmarkRunner`, and `MemoryFileSystem` to generate
summary, benchmark, help, report-dialog, Sixel-latency, and fallback-latency
figures. The example tour, documentation index, capture manifest, generator,
and `plans/example-coverage.md` consume those artifacts. On the current Mac
host, all eight SysInfo/POSIX suites (85 cases), `sysinfo_visual_capture`,
example hygiene, snippet synchronization, local-link checking, and both
mandatory provenance gates pass. A focused Homebrew Clang 22.1.8 ASan/UBSan
run passes the same eight suites plus the visual capture (9/9). This package
adds no parser or hot-path algorithm requiring a new fuzz target or benchmark.
Final cross-platform/global promotion remains WP-37/WP-38 work.

### WP-57 — TextEditor as an application's document editing surface

**Purpose.** An application that edits real documents through `TextEditor`
brings things the widget cannot assume: its own command table and keyboard
scheme, its own editing rules and undo journal, a parser that colours the
whole document, and a context menu. Each was either impossible or had to be
worked around in the application. This package makes each a stated widget
capability.

**Work.**

- Editor verbs (undo, redo, cut, copy, paste, select all, find selection,
  find next/previous, overwrite) as `EditorCommand` values bound to chords by
  replaceable `EditorKeyBinding` data, with `perform()`; a focused editor no
  longer takes an application's chords it was not given. Ctrl+A select all
  joins the defaults.
- `EditRequest` handed to an edit handler before every typed, pasted, broken,
  indented, erased or cut change, describing both the intent and the
  replacement the widget would commit; supersedes the single newline seam.
- Caret motion: a column vertical motion returns to; PageUp/PageDown move the
  caret by the viewport and extend a selection with Shift; Tab width as
  policy; the caret painted at the horizontally scrolled column.
- A virtual caret past the text (opt-in): a double-click past a line's end or
  below the last line places it, nothing is written until text arrives, and
  the padding and the text are one edit.
- Host colouring: revision-bound `HighlightSpan`s resolved through theme
  roles, replacing the profile's colouring, carried through edits.
- A context-menu request on right click, Ctrl+click and Shift+F10, with the
  caret placed first.
- `DocumentChange` reported as one covering replacement for multi-edit
  transactions and for undo/redo, so observers carry positions correctly.

**Acceptance.**

- Unit coverage for every binding, request kind, motion, virtual-caret path,
  highlight validation and carry, and context-menu route; the default
  bindings behave as before.
- A golden frame pins host colouring and a caret painted below the text.
- The editor guide and widget gallery document each capability.
- Gates: warnings-as-errors build, full suite, documentation gates,
  provenance check.

### WP-58 — Status legends and form help that follow focus

**Purpose.** A status legend that offers the same keys wherever the reader is
tells them nothing about where they are, and a long form with no room for a
line of help per field leaves every field unexplained. Both already follow
focus in part — the status line's hint does — and both should wholly.

**Work.**

- `StatusLine::set_context_items()`: an item set per command context, shown
  while the focused view's nearest command context is that one.
- `FieldDescriptor::description` and `DialogDescriptor::field_description_rows`:
  a non-scrolling panel between a form's fields and buttons that shows the
  focused field's description and keeps the last one on a button.

**Acceptance.**

- Unit coverage: the item set follows focus through an ancestor's context,
  falls back without one, and a click reaches the shown item; the panel shows
  each field's description as focus moves and keeps it on a button.
- Documentation in the widget gallery and the dialogs guide; documentation and
  provenance gates.

### WP-59 — Commands that belong to several contexts

**Purpose.** A command often means something in more than one place — Save in
a document and in the outline beside it, Close in every kind of window — and
some commands belong to the desktop with nothing focused, but not to a field
where the reader is typing a value. A command could name one context, which
states neither. And a command palette presented in a window of its own took
the focus from the place whose commands it lists, so it offered no
context-bound command at all.

**Work.**

- `CommandScope` replaces the single context name on `CommandDescriptor` and
  `CommandInfo`: the contexts a command belongs to, any one of which makes it
  available, and `outside_contexts` for where no context is active.
  `CommandRegistry::set_command_scope` restates it; `in_named_context` is what
  a modal scope admits a command by.
- `widgets::PendingDialogs` keeps the presentations an owner awaits, each
  until its answer arrives, so an application with many questions does not
  keep a member per dialog.
- `ui::command_context_path()` is the public form of the invoking-context
  capture menus already made; `CommandPalette::set_invocation_contexts()` lists
  and runs what that place allows, and without it the palette asks its own
  focus path through the Application.

**Acceptance.**

- Unit coverage: a command runs in each of its contexts and not in another; a
  command usable outside contexts is available with nothing focused, on a view
  that names no context and in a context it names, never in one it does not
  name, focused or pushed; a modal admits it only by a named context; a menu
  walk from the bare desktop runs it; a palette runs a context-bound command
  given its invocation contexts and from inside the context; a kept
  presentation answers once and is released before the answer runs, an
  answer may present the next dialog, an owner's destruction withdraws every
  outstanding answer, and an already-completed presentation answers at once.
- Documentation in the standard-commands guide and the widget gallery;
  documentation and provenance gates.

### WP-60 — Side panels a reader sizes, hides and gets back

**Purpose.** A document window with a panel on each side — an outline on the
left, a preview on the right — needs the far panel to keep its width as the
window grows, the width the reader chose to come back exactly after a restart
or a moment of too little room, and a hidden panel to leave nothing where it
was. A splitter kept only its first pane's size, clamped away a width set
before it had any size of its own, reported nothing when the reader moved it,
and kept an empty region and a divider for a hidden pane; containers did not
rearrange when a child was hidden or shown until something else resized them.

**Work.**

- `Splitter::set_resize_anchor()` names the pane that keeps its extent;
  `set_anchored_extent()` / `anchored_extent()` hold that extent as asked for,
  clamped only when laid out; `on_split_moved` fires for a divider the reader
  moved; `collapsed()` answers whether a hidden pane gave the other the whole
  splitter, which then draws, grabs and focuses no divider.
- `View::set_visible()` tells the parent, so Row, Column and Splitter
  rearrange when a child is hidden or shown.

**Acceptance.**

- Unit coverage: the second pane keeps its extent across a resize; an extent
  survives a splitter briefly too small and one asked for before any size is
  honoured; changing the anchor keeps the arrangement; key and pointer moves
  are reported and programmatic ones are not; a hidden pane takes no room,
  leaves no divider, focus stop or grab, and comes back at its extent; a row
  rearranges when a child is hidden and shown.
- Documentation in the widget gallery; documentation and provenance gates.

### WP-61 — Forms that check the whole answer

**Purpose.** A field's validator answers for its own text, and a form whose
answers are wrong only in combination — an abbreviation with no expansion, a
range that ends before it starts — had nowhere to say so. The application
either closed the form and asked again, losing what the reader typed, or said
nothing and let a bad answer through.

**Work.**

- `DialogDescriptor::check` runs at accept time on the answers the dialog
  would complete with, once every field has passed; a `DialogVeto` keeps the
  dialog open, marks and focuses the field it names, and shows its message in
  the description panel until the focus moves on. A descriptor with a check
  always has the panel. Both the presented dialog and a window wired with
  `wire_dialog_window` honour it.

**Acceptance.**

- Unit coverage: a veto keeps a presented form open with the field marked and
  focused and the reason shown, the reason retires when the focus moves, and a
  corrected answer is accepted; a wired window vetoes and then accepts.
- Documentation in the dialogs guide and the widget gallery; documentation and
  provenance gates.

### WP-62 — A save dialog that offers a name

**Purpose.** An application saving something it can already name — Save As
for a document with a name, an export beside it — had to leave the reader to
type the whole name again, because the save dialog's path field could only
start as the directory.

**Work.**

- `FileDialogOptions::suggested_name`: the path field reads the shown
  directory with the name, in whichever directory the reader browses to, and
  takes the initial focus; a file chosen from the list replaces it.

**Acceptance.**

- Unit coverage: the name is offered in the starting directory and after
  browsing into another, and accepting reports that path.
- Documentation in the widget gallery; documentation and provenance gates.

### WP-63 — A field that says it was edited

**Purpose.** A search box that filters as the reader types, or a form that
reacts to one field while it is filled in, needs to hear that the text
changed. An input line reported only Enter, so applications subclassed it
and reported every handled key — caret moves included — while missing a
change made by paste or undo through another route.

**Work.**

- `InputLine::on_edited`: fired once for each reader action that changed the
  text — typing, erasing, pasting, cutting, undoing, stepping through
  history — however many internal steps the action took, and never for
  `set_text()` or an action that left the text as it was.

**Acceptance.**

- Unit coverage: typing, erasing, a paste through its key and undo report
  once each; a caret move, a same-text paste over a selection, a refused
  grapheme and set_text report nothing.
- Documentation in the widget gallery; documentation and provenance gates.

### WP-64 — Forms whose answers line up

**Purpose.** A descriptor form placed each control right after its own label,
so a form whose labels differ in length started every control somewhere else
and read as a ragged list rather than a set of answers.

**Work.**

- `Label::set_column_width`: a label at least as wide as the column it stands
  in, its text at the start; never narrower than its own text.
- `materialize_dialog`: the labels beside a control share one column as wide
  as the widest of them. Check boxes carry their own text, radio groups
  caption their choices and notes are prose; none of them joins the column.

**Acceptance.**

- Unit coverage: text and combo labels share the widest label's width, a check
  field has no label, and a label's column width never narrows its text.
- Golden and documentation captures of descriptor dialogs regenerated; the
  widget gallery and the dialog guide describe the column.

### WP-65 — Trees a reader pages through

**Purpose.** A grouped chooser of thirty entries, a file tree or a help index
was walked one row per key: TreeView answered neither Home and End nor PageUp
and PageDown, which a list answers. Two defects met on the way to such a
chooser: a cursor selected while the view had no size scrolled the rows above
it away for good, and a dialog opened from the answer of another returned
focus nowhere when it closed.

**Work.**

- `TreeView`: Home and End select the first and the last visible row, PageUp
  and PageDown move the viewport's height, for a `TreeModel` and for
  materialized roots alike. ListView and TreeView defer a reveal requested
  before they have a height until they get one.
- `Application::push_modal`: a modal pushed while no view has focus and a
  departed scope's restoration is pending takes that restoration over.

**Acceptance.**

- Unit coverage: ends and paging over expanded and collapsed families and a
  provider model; list and tree cursors placed before layout; a modal opened
  while another detaches returns focus to the first one's origin.
- Documentation in the widget gallery.

### WP-66 — Text on a window's border

**Purpose.** A readout set into a window's frame — a caret position, a page
count, a path — was a StaticText or Label overlay, which paints the dialog
text colours: a grey block cut into a blue border that stayed the same when
the window lost activation. The spin example drew one correctly by resolving
the frame roles itself, which ignored a window's role override.

**Work.**

- `Window::frame_style()`: the style the border is drawn in now — active or
  inactive role, chrome background override applied; the window draws its
  border with it.
- `FrameText`: an overlay that paints its text and a space either side in that
  style, with a reserved width and an alignment within it; outside a window it
  draws nothing.
- `EditorWindow`'s line/column readout is a FrameText; the spin example's
  readout reads `frame_style()`.

**Acceptance.**

- Unit coverage: the text on the border in the active and the inactive frame
  style, a reserved width that keeps the readout still while the rest stays
  border line, nothing drawn outside a window.
- Widget gallery section, API index and coverage rows, `widget-frametext`
  figure; the EditorWindow figure regenerated.

### WP-67 — Text fields that offer their preset

**Purpose.** A field that opened with a value put the caret after it, so the
most common edit — retyping the value — started with erasing it. The classic
dialog convention selects the value on arrival (D-066).

**Work.**

- `InputLine::on_focus` selects the whole text, caret at the end, when the
  focus arrives and the field holds text; masked fields excepted.
- `InputLine::on_mouse` takes the focus before placing the caret; Insert
  clears a selection.

**Acceptance.**

- Unit coverage: typing replaces the preset; Right, Left, Home, Ctrl+Left,
  Backspace, Delete and Insert each do what the observed convention does;
  the selection returns with the focus; a press places the caret with nothing
  selected; an empty field selects nothing.
- Tests that typed into presets updated to the rule; the todo task editor
  golden and the input line, label, descriptor and todo figures regenerated;
  the widget gallery describes the rule.

### WP-68 — A cell grid

**Purpose.** A worksheet, a query result and a matrix are all a grid of cells
under a column header, and the toolkit had no widget for it: `Table` is a
list of typed rows. An application had to paint its own grid, cell by cell,
and reinvent the cursor, the selection, the frozen bands and the spans.

**Work.**

- `widgets::CellGrid` and `CellGridModel` (D-067): the provider owns the
  cursor, the selection and the scroll origin; the widget paints the frame,
  the header and the gutter, resolves spans, composes the cursor and the
  selection over a cell's own colours, and routes the keyboard and the mouse
  to the provider as requests.
- `MaterializedCellGridModel`: a table of values with the generic rules.
- Roles `ckv.cellgrid.normal`, `.header`, `.cursor` and `.selection` in every
  built-in scheme.

**Acceptance.**

- Unit coverage: the materialized model's clamping, extension, paging, jumps,
  frozen bands, fitted columns, scroll-follow and span listing; the widget's
  header, gutter and alignment, the overlays on plain, coloured and
  unreadable cells, spans and covered cells, the keys, the mouse, the owner
  callbacks, and that only listed cells are asked for.
- Data views and widget gallery sections, API index and coverage rows, the
  `widget-cellgrid` figure.

### WP-69 — Option groups with captions that route and choices in columns

**Purpose.** A captioned option group drew its caption's `&` literally, so a
form's radio fields were the one control Alt could not reach; and the groups
stacked one choice per row, so a form asking the same short question of four
properties scrolled where every classic dialog fits on one screen.

**Work.**

- The caption is parsed and drawn as a `Label`'s text, and the window's
  mnemonic routes focus a group whose caption marks the letter (D-068).
- `set_columns()` on both groups: row-major flow, columns as wide as the
  widest choice, a choice's span from its marker to the next column's for
  highlight and press alike; Left and Right step the sequence, Up and Down
  the column, and an arrow that would not move the cursor goes to the window.
- `FieldDescriptor::columns` for a descriptor's `Radio` field, with the row
  count taken from the group's own size hint.

**Acceptance.**

- Unit coverage: the caption's mnemonic parsed, drawn without its marker and
  accented; Alt+letter focusing a group from another control in its window;
  size hints, the row-major flow and the half-full last row; the arrows in a
  row, in a column and on a lone box; presses by column and in the gap; a
  descriptor radio field on one row that Alt reaches and Right moves.
- Gallery sections for both groups, the dialogs page, the header comments,
  and the option group figures showing a two-column captioned group.

### WP-70 — The status line's hint on an empty desktop

**Purpose.** An application whose desktop is empty had no hint: the status
line resolved its help-context key only through the focused view, and with
no window open nothing is focused.

**Work.**

- `StatusLine::current_hint()` resolves from the focused view when there is
  one and from `Application::root()` otherwise (D-069); the header and the
  gallery say so.

**Acceptance.**

- Unit coverage: the root's key answers with nothing focused; a focused
  view's own key outranks it; a focused view without a key inherits it.

### WP-71 — A list's keyboard search as a typed prefix

**Purpose.** Each typed letter was its own search, so a name typed into a
file dialog's list left the cursor on the row beginning with its last letter.

**Work.**

- `ListView` keeps the letters typed within a second as one prefix (D-070):
  a prefix the cursor's row still begins with keeps it, any other searches on
  from the cursor, the same letter again steps on, a miss drops the prefix,
  and a letter with Alt, Ctrl or Super is left to the chords. The gallery
  says so.

**Acceptance.**

- Unit coverage: a prefix formed and kept, a miss dropped, the same letter
  stepping on after the window, a modified letter declined, and a provider
  reached through `find_prefix` with a two-letter prefix.

### WP-72 — An SQL syntax profile

**Purpose.** An editor over a database's own language had no profile to lex
it with, so an application embedding one showed plain text or shipped a
private lexer.

**Work.**

- `register_standard_syntax_profiles()` also registers `sql` (D-071):
  keywords, declared types, numbers with hex and exponents, `--` and `/* */`
  comments (the block one stateful), `'...'` text with the doubled-quote
  escape (stateful when unclosed), `"..."`/`[...]`/`` `...` `` quoted names
  and every spelling of a bound parameter as names, and a word before `(` as
  a call. Detection by `.sql` or a leading statement word, above YAML's
  content score. The editor page says so.

**Acceptance.**

- Unit coverage: detection by suffix, by statement word, against YAML's
  colon rule and against prose; keywords, types, numbers, calls, operators
  and an untouched ordinary name; the three quoted-name brackets and the
  doubled quote; every parameter spelling and the lone `:`; the block
  comment and the unclosed text carried across lines, and a line comment
  ending at its line.

### WP-50 — Windows ConPTY child sessions and embedded-terminal matrix

**Purpose.** Provide the Windows-specific child session implementation and
evidence rather than assuming the outer ConPTY backend covers nested terminal
processes. A named Windows ARM64 VM is now available; interactive Windows Terminal
acceptance is still required. This package uses [embedded-terminal-architecture.md](embedded-terminal-architecture.md) and
[embedded-terminal-delivery-target.md](embedded-terminal-delivery-target.md).

**Work.**

- Implement the ConPTY child-session adapter with explicit process, resize,
  pipe, exit, cancellation, and teardown ownership equivalent to the POSIX
  session contract.
- Run the terminal example and named child fixtures under Windows Terminal,
  including input, resize, alternate buffer, nested ckVision, Sixel/no-graphics
  profiles, close/error, resource recovery, and simultaneous sessions.
- Record deliberate platform differences only at the adapter/capability
  boundary; platform-independent emulator and scene goldens must match.

**Acceptance.**

- Named Windows host/Terminal versions, capability report, ConPTY evidence,
  and all degraded rows are recorded; a headless substitute cannot close this
  package.
- Every containment and lifecycle assertion from WP-46 through WP-49 passes on
  Windows, with byte-identical platform-independent cells/pixels where the
  declared profiles match.

**Current WP-50 status (2026-09-26): Done.** The requested
signed-in Windows Terminal scenarios—nested ckVision, contained Sixel and its
no-graphics fallback, resize, close/child loss, and resource recovery—are
**Done** with named-host evidence. The portable 95-file three-platform
golden manifest is also **Done**. The exact row-by-row status is in
[embedded-terminal-delivery-target.md](embedded-terminal-delivery-target.md#wp-50-named-windows-scenario-status--2026-09-25).
The native ConPTY adapter, alternate buffer, simultaneous sessions, lifecycle,
and declared degraded child profiles are covered by the recorded Windows
fixtures and live runs. The package's two acceptance bullets are satisfied by
the named host/capability record and the Windows containment and lifecycle
evidence. True live pixel mouse on a capable Windows host and the remaining
release matrix are separate WP-37/WP-38 obligations; they do not reopen WP-50.

### WP-37 — Windows backend and three-platform matrix

**Purpose.** Put M7 in the same executable queue as every other remaining
obligation so “next open WP” cannot skip the Windows release gate.

**Work.**

- Complete the ConPTY VT backend, VT-first input with the documented console-API
  supplements, native clipboard integration, session restoration, and explicit
  degraded-host capability rows without a parallel legacy renderer.
- Run real Windows Terminal scenarios for keyboard, cell/pixel mouse, resize,
  Sixel/fallback graphics, clipboard, suspend/close restoration where supported,
  multi-Application isolation, and the editor's Unicode input, highlighting,
  search, and injected file workflow plus embedded-terminal ConPTY child
  sessions, nested ckVision, and contained Sixel. Keep headless equivalents
  for host features CI cannot expose, but do not label them interactive
  acceptance.
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

**Current WP-37 status (2026-09-26): In progress.** The following named
rows are **Done** with [Windows evidence](windows-wp37-evidence.md): live
keyboard and cell mouse, Sixel graphics and Console Host text fallback,
native clipboard export/import, the editor's Unicode input/highlighting/search
and injected Save/reopen workflow, plain-VT ConPTY degradation, live
two-outer-Application isolation, and the 95-file byte-identical portable
golden manifest on macOS, Linux, and Windows.
Live SGR pixel-mouse **transport and Gallery interaction are Done** in the
signed-in WezTerm Windows session: a delayed beyond-grid report establishes
mode 1016, a held drag delivers pixel coordinates on down, motion, and up,
and the Gallery's live Terminal Report shows `Pixel mouse: yes` alongside
pixel-bearing dispatched input; see the [host evidence](windows-wp37-evidence.md).
The installed Windows Terminal hosts do not advertise mode
1016, and earlier mintty, raw iTerm-to-Windows ConPTY, and Contour trials did
not provide a usable pixel GUI route. Native two-Application isolation across
independent private ConPTY hosts is **Done**. The separate live
two-outer-Application GUI isolation route is also **Done**. The real
legacy-console refusal route and final cross-platform release audit are **Open**.
The named VM does not start a usable `ForceV2=0` legacy host; the matched
normal/legacy launch results are in [Windows evidence](windows-wp37-evidence.md).
Supported macOS/Linux sanitizer gates, the nine Linux fuzz corpora, and the
three-host benchmark gates are **Done**; the installed Windows ARM64 MSVC
toolchain rejects ASan instrumentation and that limitation is recorded in
[Windows evidence](windows-wp37-evidence.md). Finished rows stay finished
while the open rows are completed and audited.

### WP-38 — Golden, script, reference, and release-gate closure

**Purpose.** Independently audit the evidence after corrective implementation
and prevent milestone status from outrunning verification again.

**Work.**

- Generate a traceability report from milestone exit item → corrective WP →
  implementation → unit/script/golden/fuzz/benchmark evidence → public docs.
- Complete the appearance matrix for every widget/chrome element, including
  WP-36B's common modern components, applicable active/inactive/focused/disabled
  state, WP-39…WP-44's editor/document/profile states, WP-45…WP-50's embedded
  terminal/process/graphics states, all four built-in schemes, wide text,
  terminal shrink, and paired Sixel/no-graphics profiles.
  Raster-bearing rows require both the symbolic
  scene golden and WP-32A's decoded pixel-plane golden; neither substitutes for
  the other.
- Consume WP-36A's example coverage matrix and promote gallery/showcase, hello,
  filebrowser, editor, layouts, forms, workbench, graphics, menu-only-keyboard,
  menu-only-mouse, terminal, modal, close-veto, restoration, and degradation scenarios
  to the final byte-exact scripted/golden release evidence.
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
- Only after this package lands may M0–M11 be reconsidered for `Done`;
  each milestone is changed individually only when its own exit row is fully
  supported by the report.

**Current WP-38 status (2026-09-26): In progress.** The
[M7 Windows traceability slice](windows-m7-traceability.md) maps every literal
M7 exit item to implementation, executable and live evidence, public reference,
and its remaining gap. It does not close M7: live pixel-mouse interaction and
real legacy-console refusal remain Open. The
[API index](../docs/api-index.md) now enumerates all 124 installed headers
exactly once, enforced by the documentation checker. This closes the
header-inventory gap. The
[editor/contained-terminal appearance audit](wp38-editor-terminal-appearance.md)
identifies the exact default-profile and four-scheme evidence already present
and the combinations still missing. Runtime scheme changes now repaint retained
views, and the Editor and Terminal examples expose all four schemes in their
View menus. Their 69-file editor/terminal visual slice matches byte for byte
on macOS, Linux, and Windows ARM64 at `4b41188`; all seven generators
subsequently matched the full 79-file manifest on those three hosts. The
Editor example now names the active sample in its window and dirty-close
prompt and asks Save/Discard/Cancel before switching samples with unsaved edits;
full current-source Release suites and portable goldens passed on macOS, Linux
ARM64, and Windows ARM64 at `bbdd972`. Eight Editor initial/dirty-close symbolic
frames now cover all four schemes; the then-current 85-file manifest and full
three-host Release suites pass. The shipped Terminal example now supplies a deterministic child and clock through
explicit host services for its four-scheme initial-shell capture. Eight new
cell/RGBA files brought that checkpoint's three-host byte-identical manifest to **93**;
macOS and Linux ARM64 pass 177/177, Windows ARM64 passes 164/164, and both
provenance gates pass on each host. The Terminal initial-shell appearance row is
Done. The later child text-over-raster cell/RGBA pair brought that checkpoint's
manifest to **95** on all three hosts. It proves the visible overwrite state
for the default profile. Sixteen further files pin the Terminal example's
actual full-screen alternate-buffer and nested Gallery output under classic,
dark, light, and mono schemes in the no-graphics profile. The now-current
**111-file** portable manifest and full Release suites pass on macOS ARM64
(177/177), Linux ARM64 (177/177), and Windows ARM64 (164/164), with zero build
warnings and both provenance gates on each host. The earlier 95 files stayed
byte-identical. That exact state/scheme slice is **Done**; the remaining
graphics-capable and broader state/scheme matrix stays Open.
Per-API reference depth and the complete
M0–M11 appearance, example, and release-gate audit remain Open.

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
| Example suite now covers the public surface locally; exhaustive state/theme/reference release traceability remains | WP-36A, WP-38 |
| Common modern components now exist locally; exhaustive state/theme/reference release traceability remains | WP-36B, WP-38 |
| First-class editor, syntax-profile, search/replace, and safe file-editor workflow promoted into 0.1 scope | WP-39…WP-44, WP-37, WP-38 |
| Contained terminal view, private child PTY/ConPTY, nested application, and no-spill Sixel requirement promoted into 0.1 scope | WP-45…WP-50, WP-37, WP-38 |
| Windows backend/host and full three-platform matrix remain unaccepted | WP-32, WP-37 |
| Six appearance dumps, smoke-only examples, incomplete public docs and release evidence | WP-38 |
