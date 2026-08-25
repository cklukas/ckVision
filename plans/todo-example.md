<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# TODO example — product contract and delivery plan

## Goal

Add `examples/todo`, an approachable but complete Kanban application that a
new ckVision client can run, understand, and learn from. It must retain the
useful product capabilities of the audited TODO application while expressing
them through ckVision's own interaction model: a desktop, resizable windows,
menus, command-backed status items, contextual help, modal dialogs, a built-in
note editor, themes, keyboard discipline, and full mouse access.

This is a real application proof point, not a static widget exhibit. A user
must be able to keep daily work in it: task and lane changes persist without a
manual Save command, named boards survive restarts, another running instance is
noticed, archives and daily backups are durable, and every destructive action
has an explicit outcome.

Estimated scope: **5–8 person-weeks**, delivered as `TODO-WP-01` through
`TODO-WP-08`. The `TODO-` prefix deliberately avoids colliding with the
repository's existing numeric WP sequences.

## Delivery status

Implemented on `main` through TODO-WP-08 on 2026-08-26. The shipped object
graph, POSIX executable, deterministic/real repositories, conflict UI, full
note editor, workflow goldens and SVG captures, codec fuzzer, real
two-process poll proof, focused benchmarks, and client guide are registered in
the build and test manifests. The acceptance authority is the implementation
plus the executable evidence mapped at the end of this plan; the original
person-week estimate is retained as planning history.

## Authority and provenance

The behavior specified below is now the self-contained ckVision requirement.
Acceptance is based on this document, VISION.md, ARCHITECTURE.md, DECISIONS.md,
the public ckVision API, and executable evidence in this repository. It is not
based on line-by-line similarity to another implementation.

The product audit used the public `github.com/cklukas/todo` application at
commit `2a079c20f4cd90c43edfe986f70c69ce89700f46` (2025-06-10), including its
README, tests, current Go model/UI code, and four README screenshots. That audit
is requirements discovery only. No source from a prohibited implementation was
consulted, and no outside implementation is an acceptance oracle.

Published authorities used by the new implementation are RFC 8259 for the
versioned JSON document, ISO 8601 calendar representations, Unicode behavior
through ckVision's own text services, and ckVision's documented command,
focus, modality, window, rendering, and platform-service contracts.

No public library change or external dependency is expected. If the example
finds that a generally reusable ckVision capability is missing, the design
change is recorded in DECISIONS.md and the library is fixed before the example
works around it.

## Reference-product audit

### Feature inventory

| Area | Audited behavior | ckVision TODO requirement |
|---|---|---|
| Board | Equal-width bordered Kanban lanes; defaults to `To Do`, `Doing`, `Done` | One resizable Board window containing horizontally scrollable lane panes; same three defaults |
| Tasks | Title, second detail line, long note, stable GUID, priority, due date, color, creator/updater and timestamps | Preserve every user-visible fact; represent archive/origin and board membership structurally; use deterministic workspace-scoped numeric ids and explicit injected identity/time sources |
| Task creation/editing | Centered form; title, details, due date, four priorities, 16 task colors; edit shows creation/modification facts | Rich ckVision dialog using InputLine, DatePicker, ComboBox/RadioGroup, StaticText and standard accept/cancel behavior |
| Notes | Suspends the TUI and invokes `$VISUAL`, `$EDITOR`, vim, or Notepad on a temporary file | A modeless, resizable ckVision TextEditor window with undo/redo, search, clipboard, wrap and automatic persistence |
| Navigation | Up/Down selects tasks; Tab/Shift-Tab or Left/Right changes lane | Preserve the grammar; focus and visibility are handled by ckVision, with mouse equivalents |
| Moving | Enter/Space/F6 enters a visibly distinct move mode; arrows reorder or move the task; Enter/Space/Esc finishes | Preserve two-stage move mode; add direct pointer drag with an insertion marker |
| Delete/archive | Delete removes after confirmation; Archive writes a separate item JSON then removes after confirmation | Preserve both and make archive failure non-destructive; no archive browser/restore is implied |
| Priority | 1 high (`↑`), 2 normal, 3 low (`↓`), 4 idle (`⌛`) | Preserve values and visible marks; labels remain understandable without glyph color |
| Due dates | Locale-shaped input; current rendering marks today and tomorrow | Store ISO dates; show overdue, due today and tomorrow badges using an injected calendar clock |
| Colors | Optional task foreground and optional lane background from a 16-color named palette | Preserve named choices with contrast-safe rendering in every ckVision theme |
| Lane sorting | Stable manual, color, due, created, modified, and priority modes | Preserve all six; manual reorder is enabled only in Manual mode and disabled with an explanatory hint otherwise |
| Lane management | Rename, color, insert left/right, sort, or merge/remove into another lane or archive | Preserve all operations; forbid deleting the final lane; keep operations atomic |
| Named lists | `main` plus named “modes”; CLI mode selection; last selected mode persists; UI can add/switch modes | Call them Boards in the UI, accept a board on the CLI, remember the last board, and provide full create/rename/switch/merge/delete management |
| Persistence | Human-readable JSON, rewritten after every completed change | Versioned JSON, atomic writes, optimistic revision checks, no manual Save command |
| Backup | One dated JSON snapshot per day | Snapshot the last persisted workspace before the first mutation of a calendar day; never overwrite that day's snapshot |
| Live instances | Watches `todo.json`, reloads on remove/write/create | Poll an opaque repository revision on a ckVision timer; reload/rebase without silently losing local edits |
| Status | Clickable F-key actions, active mode, contextual move instructions, date/time when width permits | Command-backed ckVision StatusLine, clickable Board item, transient move/conflict hints and optional clock with priority-based narrow layout |
| Help | F1/h/? opens an About dialog containing shortcuts and version/update information | F1 opens contextual HelpViewer topics; About is separate and shows build/version facts |
| Mouse | Click-to-focus/select, form controls, modal buttons and clickable status actions | Preserve all of those and add task drag/drop, lane/task context menus, window move/resize/zoom, and menu navigation |
| CLI extras | `version --check` and `version --update` use GitHub HTTP releases | `--version` remains; network version checking and self-update are intentionally excluded from a zero-dependency UI example |

### Audit qualifications

- The README and TODO ledger say mode merge/removal is complete, but the
  audited Mode dialog does not expose it and the remove action is a stub. The
  ckVision target implements the complete workflow specified here instead of
  preserving that unreachable state.
- The reference's date formatting reads process locale/environment state. Its
  test suite passed all packages except two date-input cases on the audit host,
  where runtime US-format detection contradicted fixed non-US expectations.
  The ckVision target uses injected date/time facts and ISO display policy in
  deterministic paths.
- The older README screenshots show title/details forms and a status bar that
  predate some current fields. Current source plus tests, not screenshot age,
  determined the feature inventory.
- Data compatibility with the reference JSON layout is not required. The new
  format is versioned and documented; a future importer must be a separately
  accepted feature with fixtures and fuzz coverage.

## Target user experience

### First launch and demo mode

`ckvision_todo` opens the persistent `main` board. When no workspace exists, a
welcome dialog offers **Guided sample** (the default) and **Empty board**. The
sample contains three small tasks that teach Add, Help, and move mode; they are
ordinary removable tasks, not hard-coded UI.

`ckvision_todo --demo` opens a fixed, in-memory workspace and writes nothing.
This is the zero-risk tour used by generated screenshots and is the easiest
way to evaluate the example. `--data-dir PATH`, `--board NAME`, `--version`,
and `--help` are the only other required CLI surfaces. A single positional
board name is accepted as the familiar compact form.

The interactive host resolves the default data root at its platform boundary;
on POSIX it is `~/.ckvision/todo`, not the reference application's directory.
No model, controller or view reads environment variables, user records, the
wall clock, locale, or filesystem directly.

### Desktop and windows

- A MenuBar is docked at the top and a StatusLine at the bottom.
- The initial Board window is movable, resizable, zoomable, minimizable and
  closable. It opens large enough for three lanes but does not pretend to be
  the desktop itself.
- Each lane has a titled frame (`Doing (4)`), its configured background,
  independently scrollable two-line task cards, visible focus, and a stable
  selected task.
- The board's horizontal ScrollViewport keeps a minimum useful lane width and
  reveals additional lanes rather than shrinking them into unreadability.
  Changing lanes calls `ensure_visible`, so keyboard focus is never offscreen.
- A Note Editor is a modeless Window over the board. Multiple notes may be
  open; Window menu commands can cycle, list, tile and cascade them.
- Task editing, destructive confirmations, lane operations, board management,
  conflict resolution and first-run choice are modal and restore the prior
  focus on completion.
- Help opens through the standard HelpViewer with Board, Tasks, Moving,
  Lanes, Boards, Persistence and Keyboard topics. About is a normal info
  dialog, not the only place shortcuts are documented.

### Task cards

Each visible task occupies two rows:

1. priority mark, Unicode-safe elided title, and the highest-priority due badge;
2. Unicode-safe elided details, with color applied without hiding selection.

An empty details line remains visually reserved so arrow navigation never
causes cards below it to jump. Focused selection, inactive selection, move
source, drag source and insertion target have distinct theme-derived states.
Color is supplementary; text/glyph shape identifies priority and due state in
Mono and for readers who cannot distinguish the palette.

### Autosave and concurrent instances

Every completed task, lane, board, archive, delete, sort or color command is
durable before the UI reports success. Note-editor document transactions are
coalesced for at most 150 ms, show `Saving…`/`Saved` on the note window, and are
flushed on close and quit. There is no Save command for TODO data.

A one-second repeating Application timer asks the injected repository for its
opaque revision. A remote change is loaded immediately when there is no local
edit. An id-based local operation can be replayed once over the new revision
when its target still exists and is unchanged. Same-field edits, deletion of a
locally edited object, or note conflicts open a resolution dialog; the app
never silently overwrites either version.

## Commands, keys and pointer actions

All application commands are declared once in CommandRegistry, use named
contexts, and drive menus, status items, enablement and keymaps. Bare-letter
aliases exist only in the `todo.board` context, so typing into a dialog or note
editor never runs a board command.

| Intent | Reference key | ckVision TODO binding |
|---|---|---|
| Context help | F1, `h`, `?` | F1; `h`/`?` aliases on the Board only |
| Add task | F2, Insert, `+` | F2, Insert, `+` |
| Edit task | F3, `e` | F3, `e` |
| Edit note | F4, `n` | F4, `n` |
| Archive task | F5, `a` | F8, `a`; F5 remains standard Window Zoom |
| Start/finish move | F6, Enter, Space | F9, Enter, Space; Esc cancels and restores the original position; F6 remains standard Next Window |
| Lane actions | F7 | F7 or context menu on the lane title |
| Delete task | Delete, `d` | Delete, `d` |
| Next/previous lane | Tab/Shift-Tab, Left/Right | Tab/Shift-Tab and Left/Right while not moving |
| Select/reorder task | Up/Down | Up/Down selects; in move mode it changes manual order |
| Move across lanes | Left/Right in move mode | Left/Right in move mode; pointer drag is equivalent |
| Select board | `m`, status mode click | `m`; click the `Board: name` StatusLine item |
| Menu | — | F10 and Alt+menu mnemonic, using ckVision's standard menu command |
| Quit | F10, `q` | Alt+X and standard Quit menu/status item; `q` remains a Board-only alias |
| Window management | — | Standard next/previous/list/tile/cascade/zoom/minimize commands |

F5, F6 and F10 deliberately retain ckVision's standard Zoom, Next Window and
Menu bindings. Archive and move shift to F8 and F9. This is the most visible
intentional UX change: a ckVision showcase must teach the framework's standard
window/menu grammar without duplicate chords. Quit remains one keystroke and
is visible in File and StatusLine.

Pointer parity is literal: every visible status command is clickable; task
selection and activation work by click/double-click; task drag/drop performs
the same validated move as move mode; right-click opens task/lane context
menus; every dialog control and menu item works by pointer; and standard
Window chrome supplies move, resize, zoom, minimize and close.

## Menus and status line

### Menu structure

- **File** — Board Manager…, New Board…, Demo/real workspace facts, separator,
  Quit.
- **Task** — Add…, Edit…, Edit Note, separator, Move, Archive…, Delete….
- **Lane** — Sort submenu (Manual/Color/Due/Created/Modified/Priority), Color…,
  Rename…, Insert Left…, Insert Right…, separator, Merge/Remove….
- **View** — Classic/Dark/Light/Mono theme choices plus the standard Window
  submenu (Next, Previous, List, Tile, Cascade).
- **Help** — Context Help, Keyboard Reference, About.

Sort and theme entries are checkable. Task commands are disabled without a
task; vertical reorder is disabled outside Manual sort; destructive lane and
board operations are disabled while move/drag is active. Menus and context
menus render those states live from the registry.

### Status behavior

The default Board status items are Help, Add, Edit, Note, Archive, Move,
Lane, Board and Quit, with priorities so Help/Add/Move/Board survive narrow
width first. Focus-derived hints explain the current task/lane/control. Move
mode replaces the hint with `Arrow keys move; Enter commits; Esc cancels`.
Conflict and autosave facts temporarily outrank both.

`Board: <name>` executes the board-selection command. A hand-labelled ISO
date/time item appears only when space permits and is updated from the injected
calendar clock. It is informative, never an input source for model logic.

## Domain and persistence design

### Model

`TodoWorkspace` owns all user-visible state so cross-board operations can be
one optimistic transaction:

- `schema_version`, `last_board_id`, and monotonically increasing
  `next_board_id`, `next_lane_id`, `next_task_id`;
- ordered Boards with stable id, unique display name and ordered Lanes;
- Lanes with stable id, title, named optional color, SortMode and ordered
  Task ids;
- Tasks with stable id, title, details, note, priority 1–4, optional ISO due
  date, optional named color, created/modified timestamps and identities.

IDs are workspace-scoped counters stored in the document. They need no random
source, remain deterministic in tests, and stay unique through board/lane
merges. Views and commands retain ids, never volatile vector indexes.

Domain operations are the only mutation surface: add/edit/move/delete/archive
task; set lane sort/color; add/rename/merge/remove lane; add/rename/switch/
merge/remove board. Each validates first and returns a typed result. UI code
does not edit vectors directly.

Stable sort semantics are explicit: color by stored palette key; due ascending
with missing dates last; created/modified oldest first; priority 1→4; Manual
preserves the stored order. Moving into a sorted lane changes ownership but
not its derived display order. Removing the final lane or `main` board is
rejected.

### Repository and host boundaries

`TodoRepository` is an example-owned injected interface. It exchanges only
plain workspace/archive/revision/result values and has no view dependency.
`JsonTodoRepository` is the sole real filesystem implementation;
`MemoryTodoRepository` scripts tests and captures. `CalendarClock` supplies an
ISO timestamp and local calendar date; `FixedCalendarClock` is deterministic.
The executable supplies the real clock and current identity explicitly.

The on-disk root contains:

```text
todo.json
backup/YYYY-MM-DD.json
archive/YYYY-MM-DDTHH-MM-SS--task-ID.json
```

`todo.json` is one versioned workspace document. Writes use a temporary file,
flush, and atomic replace with an expected revision. The daily backup copies
the previously persisted workspace before the first successful mutation of
that date and uses create-if-absent semantics. Archive writes the complete task
snapshot plus origin board/lane and archival facts before the primary document
can remove it; a failure leaves the task in place. Recovery treats an
already-written identical archive record as idempotent.

The JSON codec is bounded: maximum input bytes, boards, lanes, tasks and string
bytes are explicit constants. It validates UTF-8 and the schema before
publishing state. Unknown fields are ignored for additive evolution; unknown
schema versions are rejected with a diagnostic and no rewrite. Malformed and
hostile bytes are fuzzed.

### Application composition

`TodoApp` receives Application, TodoRepository, CalendarClock and identity at
construction. It owns a `TodoController`, `TodoWorkspace`, `Desktop`, command
ids, presentations, windows and modal presentation handles. The controller
owns transaction/rebase/autosave policy; views observe immutable snapshots and
submit typed intents.

`TodoBoardView` is a public-API-only custom View composition:

- a horizontal ScrollViewport contains a Row of `TodoLaneView`s;
- each lane view draws its frame/cards with Painter, owns a vertical Scrollbar,
  exposes stable-id cursor/activation callbacks, and uses ckVision text
  measurement/elision;
- BoardView coordinates lane focus, `ensure_visible`, move mode and drag/drop;
- no widget reaches into the repository, wall clock or controller internals.

The custom view is intentional example value: it demonstrates the documented
View/Painter/focus/mouse/theme extension seam while all application chrome,
dialogs, controls, editor, scrolling, commands and windows remain stock
ckVision components.

## Planned repository artifacts

```text
examples/todo/
  main.cpp
  todo_app.hpp / todo_app.cpp
  todo_controller.hpp / todo_controller.cpp
  todo_model.hpp / todo_model.cpp
  todo_codec.hpp / todo_codec.cpp
  todo_repository.hpp
  json_todo_repository.hpp / json_todo_repository.cpp
  memory_todo_repository.hpp / memory_todo_repository.cpp
  calendar_clock.hpp / system_calendar_clock.cpp
  todo_board_view.hpp / todo_board_view.cpp
  todo_lane_view.hpp / todo_lane_view.cpp
  task_dialog.hpp / task_dialog.cpp
  board_dialog.hpp / board_dialog.cpp
tests/test_todo_model.cpp
tests/test_todo_codec.cpp
tests/test_todo_repository.cpp
tests/test_todo_board.cpp
tests/test_todo_smoke.cpp
tests/test_todo_golden.cpp
fuzz/fuzz_todo_codec.cpp
tools/docgen/capture_todo_screenshots.cpp
docs/todo-example.md
```

Names may be consolidated when a responsibility remains small, but model,
codec, repository, controller and view responsibilities must not collapse into
one application class. Every new source/build/tooling file carries the required
copyright and SPDX lines.

## Work packages

### TODO-WP-01 — Self-contained model, transitions and JSON codec

**Purpose.** Establish deterministic application semantics before UI or real
I/O can conceal mistakes.

**Work.**

- Implement stable ids, Task/Lane/Board/Workspace values, validation limits,
  sort derivation and typed domain results.
- Implement all task/lane/board transitions, including cancelable move
  transactions, lane merge/archive and board merge/archive rules.
- Implement the bounded RFC 8259 codec and schema-version policy.
- Supply guided-sample and empty-workspace factories.

**Acceptance.**

- Unit tests cover every valid transition and invalid boundary, stable sorting,
  missing due dates, id preservation, final-lane/main-board protection, and
  exact merge order.
- Encode→decode preserves every field; canonical encoding is byte-identical
  for identical state on every host.
- Malformed UTF-8/JSON, duplicate ids, dangling task ids, limit excess and an
  unknown schema version fail without partial state.
- `fuzz_todo_codec` has seed corpora for empty, guided, Unicode, malformed,
  oversized and future-schema documents.

### TODO-WP-02 — Durable repository, backups, archives and revisions

**Purpose.** Make “no Save command” trustworthy and keep all platform impurity
behind one example-owned boundary.

**Work.**

- Define TodoRepository revision/load/commit/archive contracts and typed
  conflict/error results.
- Implement deterministic MemoryTodoRepository and real JsonTodoRepository.
- Add default-path resolution, identity and SystemCalendarClock only in host
  composition files.
- Implement atomic expected-revision writes, dated pre-mutation backup,
  idempotent archive-first removal and startup recovery diagnostics.

**Acceptance.**

- Memory tests prove create, reopen, atomic conflict, no-op revision stability,
  daily backup exactly once, archive-before-remove, idempotent retry and every
  injected I/O failure point.
- Real-filesystem tests use a unique temporary directory and prove the same
  observable contract without reading or writing user data.
- A failed primary write leaves the last valid file readable; a failed archive
  leaves the task live; a new process observes the committed revision.
- No core/scene/ui/widgets change, environment read, locale read or wall-clock
  read is introduced.

### TODO-WP-03 — Board and lane views, navigation and movement

**Purpose.** Deliver the recognizable, responsive Kanban surface and its full
keyboard/mouse grammar.

**Work.**

- Build TodoLaneView's two-row cards, scrollbar, focus states, due/priority
  badges, color contrast and stable-id cursor.
- Build TodoBoardView's Row/ScrollViewport composition, dynamic lanes,
  focus traversal, ensure-visible behavior and responsive size hints.
- Implement select/move/cancel, manual reorder, cross-lane moves and pointer
  drag/drop through the same controller intent.
- Add task and lane context menus with live command enablement.

**Acceptance.**

- Event scripts cover Up/Down, Tab/Shift-Tab, Left/Right, wrap at edges,
  Enter/Space/F9, Esc rollback, sorted-lane restrictions, empty lanes and
  deletion of the selected task.
- Pointer scripts cover select, double-click edit, wheel, scrollbar, drag
  reorder, drag across lanes, canceled drag and right-click menus.
- Unicode titles/details are never split; all state remains legible in
  Classic, Dark, Light and Mono.
- Resize scripts cover three lanes, one lane, many lanes, terminal shrink and
  focused-lane auto-reveal without inaccessible content.

### TODO-WP-04 — Task dialog and built-in note editor

**Purpose.** Replace external-editor interruption with a ckVision-native,
fully discoverable editing flow.

**Work.**

- Compose Add/Edit Task dialogs from public form widgets with validation,
  DatePicker, priority/color selection, metadata and help contexts.
- Add Delete and Archive message-box flows with exact error recovery.
- Add modeless TextEditor note windows keyed by TaskId, note command scopes,
  undo/redo/find/clipboard/wrap, status overlay and 150 ms autosave.
- Handle task deletion/archive/remote edit while its note is open.

**Acceptance.**

- Keyboard and pointer scripts prove initial focus, Tab order, label mnemonics,
  default Enter, Esc cancel, invalid date retention and focus restoration.
- Add/edit round-trips every task field; created facts never change and
  modified facts change only on accepted content changes.
- Note tests cover Unicode editing, undo/redo, selection clipboard, search,
  wrap, autosave coalescing, close/quit flush and same-task conflict.
- Modal task commands cannot leak to the board; modeless note windows retain
  standard desktop/window behavior.

### TODO-WP-05 — Lane and board lifecycle

**Purpose.** Complete the dynamic organization features, including the
reference product's intended but unreachable mode-removal workflow.

**Work.**

- Add sort/color/rename/insert-left/insert-right lane dialogs and checked menu
  state.
- Add lane merge/remove with explicit target-lane or Archive-all outcomes.
- Add Board Manager with create, rename, switch, merge/remove and last-board
  persistence; accept CLI board selection.
- Define board merge precisely: same-title lanes merge in target order,
  unmatched source lanes append in source order, global ids remain stable.

**Acceptance.**

- Every lane operation persists, refreshes titles/counts and leaves a valid
  focused lane/task.
- Merge tests cover empty/non-empty, same/unmatched titles, sorted targets,
  colors, duplicate display names, archive failure and cancel.
- Board tests cover first launch, guided/empty choice, CLI selection, remembered
  selection, invalid name, rename, merge/remove, protected `main`, and another
  instance currently showing a removed board.
- No destructive operation can strand zero boards or zero lanes.

### TODO-WP-06 — Commands, menus, status, help, themes and desktop polish

**Purpose.** Make the application a ckVision showcase rather than a board
drawn inside a terminal.

**Work.**

- Declare commands/contexts/keymap once; construct File/Task/Lane/View/Help
  menus and task/lane context menus from presentations.
- Build the prioritized StatusLine, dynamic Board item, clock item, focus hints
  and move/conflict/autosave transient hints.
- Install HelpViewer topics and About; expose all standard window commands and
  four built-in themes.
- Polish initial window geometry, min sizes, frame overlays, close/quit
  flushing, pointer shapes and accessibility-through-text cues.

**Acceptance.**

- A registry test proves unique command keys/chords, correct context scoping,
  enablement, check state and complete menu/status reachability.
- F10 opens the menu; mnemonics, arrows, Enter and Esc work; Alt+X/Board `q`
  quit through the vetoable close/flush protocol.
- Status items click correctly, narrow priority layout is deterministic, and
  clock/date disappear before action items become unusable.
- Window move/resize/zoom/minimize/cycle/list/tile/cascade are scripted with the
  Board and at least two Note windows.

### TODO-WP-07 — Live reload, optimistic rebase and conflict UX

**Purpose.** Match multi-instance awareness without the reference product's
silent last-writer-wins risk.

**Work.**

- Start/cancel the repository poll timer with TodoApp lifetime.
- Reload clean external revisions while preserving stable selection/window
  targets that still exist.
- Rebase one id-addressed non-overlapping local operation; present typed
  conflict choices for overlapping task/note/lane/board edits.
- Define removed-board and removed-open-note behavior.

**Acceptance.**

- Two controllers over one MemoryTodoRepository deterministically exercise
  remote add/edit/move/delete, non-overlapping rebase and every overlapping
  conflict without threads or sleeps.
- ManualClock scripts prove one-second polling, no redundant redraw on an
  unchanged revision, timer cancellation and no callback after destruction.
- Conflict dialog choices preserve the selected version exactly and never
  dismiss unsaved local note text.
- A real two-process smoke proves one instance displays the other's committed
  task within the documented polling interval.

### TODO-WP-08 — Executable, documentation and release-grade evidence

**Purpose.** Ship a runnable teaching artifact with the same evidence standard
as the library it demonstrates.

**Work.**

- Add the portable `todo_app` object-graph target, POSIX interactive
  `ckvision_todo`, install rule, tests and example-hygiene coverage.
- Write `docs/todo-example.md` with build/run, guided tour, architecture,
  persistence, all commands, extension-widget explanation and source-backed
  snippets.
- Add deterministic screenshot capture and manifest entries.
- Add focused benchmark cases for codec/load, 1,000-task refresh, steady board
  render, lane movement and unchanged revision polling.

**Acceptance.**

- Generated figures cover guided board, task editor, move mode, lane menu,
  Board Manager, overlapping Note Editor, narrow/many-lane layout, conflict
  resolution and all four themes where state styling differs.
- `test_todo_smoke` performs a complete keyboard and mouse tour against fixed
  repository/time/identity services and ends with the exact expected workspace.
- Warning-clean builds, full unit/golden/script suite, codec fuzz corpus,
  ASan/UBSan and TSan where threads exist, and performance budgets pass on the
  required targets.
- Documentation generation, snippets, links, screenshot manifest, example
  hygiene, include-direction checks, `provenance_check`, and
  `provenance_checker_self_test` pass.
- `plans/example-coverage.md`, `docs/example-apps.md`, docs index, README tour
  and the appropriate roadmap/status maps are updated in the same landing.

## Cross-package acceptance matrix

| Capability | Primary executable evidence | Visual/reference evidence |
|---|---|---|
| Task fields, ids, sorts, due/priority/color | `test_todo_model`, `test_todo_codec` | Task-dialog and all-theme board captures |
| Atomic persistence and daily backup | `test_todo_repository` | Persistence section in `docs/todo-example.md` |
| Archive/delete safety | repository + smoke failure-path cases | Archive/delete confirmation captures |
| Lane navigation and moving | `test_todo_board`, smoke keyboard scripts | Initial and move-mode captures |
| Pointer parity and drag/drop | board + smoke pointer scripts | Drag insertion capture |
| Dynamic lanes and responsive resize | board resize scripts | narrow/many-lane capture |
| Lane lifecycle and sorting | model + smoke dialog/menu scripts | Lane-menu capture |
| Named boards and remembered selection | model/repository/smoke CLI cases | Board Manager capture |
| Built-in notes | smoke editor/document cases | overlapping Note Editor capture |
| Menus/status/help/window management | registry/smoke public-path scripts | all-theme chrome and help captures |
| Live instance reload/conflict | two-controller and two-process cases | conflict capture and guide |
| Hostile persistence bytes | codec unit/fuzz targets | documented limits/error behavior |

## Explicit divergences and exclusions

- UI terminology is **Board**, not Mode; the CLI still accepts the familiar
  compact board argument.
- F5/F6/F10 remain ckVision's Zoom/Next Window/Menu keys. Archive/Move use
  F8/F9, and F10 is not an Exit key.
- Long notes use ckVision TextEditor; the app does not suspend into an external
  editor.
- Dates display in an explicit ISO policy in deterministic UI, not an inferred
  process locale.
- Board merge/removal is complete even though it was unreachable in the
  audited product.
- Pointer drag/drop, contexts, themes and standard window management are added
  because they demonstrate ckVision and make feature parity mouse-complete.
- Network release checking, self-update, reference-format import, archive
  browsing/restoration, reminders, recurrence, tags, search across all boards,
  collaboration servers and cloud sync are outside this plan.

These exclusions do not weaken the requested TODO capability set. They prevent
an example application from becoming a package manager, network client or
general task service, while the target remains a fully featured local Kanban
application and a comprehensive ckVision demonstration.
