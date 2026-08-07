# ckVision — API Ergonomics and Integration Review #2

Reviewed 2026-08-07 against the full public surface (`include/cvision/`),
all three examples, the ui/widgets implementations, the test suite
(899 tests across 61 files at review time), and every planning document.
This file is the binding work inventory for ROADMAP milestones **M8**
(integration correctness), **M9** (API ergonomics), and **M10** (layout
and widget-model completion); DECISIONS.md **D-028** and **D-029** record
the two policy changes it introduces. Nothing in this file loosens an
existing decision — it schedules the already-decided-but-unbuilt pieces
and repairs seams between individually-tested components.

The one-sentence verdict: per-component engineering is strong
(layering, ownership, determinism, adversarial tests), but the library
currently fails its own VISION §Design-principles bar — *"a complete
windowed application with a menu bar, status line, and a dialog must fit
on one screen of readable code with sane defaults"* — and three
integration seams are broken end-to-end while their mechanisms are
individually green under test. `examples/hello` is 163 lines across
three files against the M5 exit criterion of an enforced 60.

Method note: every finding below was verified in the sources named, not
inferred from documentation. Line references are as of the review
commit and will drift; the named symbols will not.

## Status ledger

Updated as each work package lands (build + test evidence in the
landing commit). A row with no note is still open.

| WP | Status | Note |
|---|---|---|
| WP-1 | **Done** | `View::fills_root()`/`on_bounds_changed`; `tests/test_m8_resize_integration.cpp` |
| WP-2 | **Done** | Click-to-focus in `Application::dispatch`, applied after delivery (not before — see the MenuBar note below) |
| WP-3 | **Done** | `View::on_descendant_mouse_down`; `Desktop` override; `tests/test_m8_integration.cpp`, `tests/test_desktop.cpp` |
| WP-4 | **Done** | `Window::zoom_area_provider`/`fill`/`refresh_zoom_area`, `DesktopGrowPolicy`; `tests/test_window.cpp`, `tests/test_desktop.cpp` |
| WP-5 | **Done** | Unified `widgets::WindowHandle` (retired 5 duplicate per-widget Handle structs); `Desktop::show_dialog`; gallery migrated (F6 fixed); `tests/test_message_box.cpp` |
| WP-6 | **Done** (M8 complete) | Gallery on `dock_top`/`dock_bottom` (F7); redundant `set_focus_policy(TabStop)` removed from all 3 examples (E6); filebrowser window declares `KeepFilling`; `tests/test_gallery_smoke.cpp` resize regression |
| WP-7 | **Done** | Core `ui::Context`/`on_attached()` mechanism done. M4 widgets migrated: Label, StaticText, Button, InputLine — all with `set_role_override`; `materialize_dialog` and `make_message_box` dropped their now-unused `theme` parameter. Window migrated (4-role `set_role_override` for the dialog-chrome family). Found and fixed a real propagation gap while migrating Window (see note below) — 5 `Window(...)`-constructing dialog factories updated in the same commit. M6 batch landed against the real role names WP-8 interned: Scrollbar (`track`/`thumb`), ListView (`normal`/`selected`), TreeView (`normal`/`selected`), Table (`header`/`normal`/`selected`), Memo (`normal`/`focused`), ScrollViewport (no roles — never draws, children resolve independently), TextView (`text`). `make_file_dialog`, `make_window_list_dialog`, `make_directory_picker`, and `make_help_viewer` all dropped their now-unused `theme` parameter, with every call site updated. A second M6 batch landed CheckGroup/RadioGroup (`ckv.option.normal`/`focused`), ImageView (`ckv.image.fallback`), and Canvas (`ckv.canvas.fallback`) the same way; the gallery's Sixel demo window keeps its own `dialog_background` fallback via `set_role_override`. The final batch landed the M5 widgets that need `Application&`/`Desktop&` for behavior, not just a theme: DropdownMenu resolves `ckv.menu.dropdown.*` and reads `context().app` for command enablement (its `highlighted_ = step_selection(...)` initialization moved from the constructor into `on_attached()`, since it needs `context().app` and that's only valid once attached); MenuBar resolves `ckv.menu.bar.*` for its own bar, no longer holds the 3 dropdown roles at all (its dropdowns self-resolve those independently), reads `context().app`, and finds its owning Desktop with a parent-chain `dynamic_cast` walk at attach (matching real usage, where `Desktop::dock_top` always makes MenuBar a direct child of the Desktop it controls); StatusLine resolves `ckv.statusline.normal` and reads `context().app` for the focused view's help-context key and command execution. `show_context_menu` dropped its theme/3-role parameters, keeping `Application&`/`Desktop&` as genuine free-function parameters (not widget-constructor plumbing). One real bug caught by this batch's own test suite (not by inspection): `tests/test_menu.cpp`'s `clicking_the_bar_to_open_it...` test attached its `MenuBar` directly to `app.root()` rather than a `Desktop`, which used to work only because the old constructor took an unrelated explicit `Desktop&` reference; under the new parent-walk, `desktop_` stayed null and the test's own simulated click into `open_dropdown()` null-derefed it — fixed by giving that test its own `Desktop` in the tree, matching how `MenuBar` is actually meant to be placed. Verified across all three WP-7 batches: Release build clean, `cvision_tests` 938/938, `ctest` (both targets) green, ASan/UBSan build + ctest green — the ASan/UBSan pass is what would have caught the null-deref above even if the plain test binary's crash (SIGSEGV, exit 139) had gone unnoticed. Every M4/M5/M6 widget now resolves its own theme roles; WP-7 (D-028) is complete. Desktop stays theme-holding (background_role_ has no natural "attach" moment independent of construction — the one documented, deliberate exception). |
| WP-8 | **Done** | `StandardRoles` grows 13 fields (`list_normal/selected`, `table_header`, `memo_normal/focused`, `option_normal/focused`, `scrollbar_track/thumb`, `image_fallback`, `canvas_fallback`, `text_view_text`, `status_line_normal` — 22 → 35 total), interned + styled across all 4 built-in schemes with fallback values that exactly match what each M6 widget rendered while still borrowing foreign roles (zero visual change — a pure indirection split, verified by the unchanged goldens). `tests/test_standard_roles.cpp` extended (role count, namespace prefix, and the mono-scheme hue-safety sweep all now cover the new roles). Prerequisite satisfied; WP-7 resumes against these names next. |
| WP-9 | **Done** | `View::add<T>`/`make<T>` (header-only templates over `add_child`, the type-erased primitive); `Desktop::dock_top`/`dock_bottom`/`add_popup` given the same typed shape (private `*_impl` non-template methods do the real work; the public templates are thin `static_cast` wrappers — one exact-match overload beats any candidate needing unique_ptr's converting constructor, so a plain `unique_ptr<ui::View>` caller still works unchanged). `Desktop::add_window` was already typed (Window-specific), simplified internally to use `add<T>` instead of its own `static_cast`. Library-internal composition converted: `ScrollViewport`, `Table`, `Memo`, `ListView`, `TreeView`, `TextView`'s scrollbar construction now reads `scrollbar_ = make<Scrollbar>(...)`. All 3 examples converted; the one remaining `static_cast<widgets::` in `examples/` (`filebrowser_app.cpp`'s `frame_overlay()`) is a documented exception — `frame_overlay()` is a getter for state set earlier via `set_frame_overlay()`'s "replaces and returns the OLD overlay" contract, not an insertion call, so it doesn't fit `add<T>`'s "returns what you just gave it" shape; forcing that would mean changing `set_frame_overlay`'s return contract, which is out of this WP's scope. `add_item` (`ui::Row`/`Column`, used throughout the dialog factories) is a distinct insertion primitive with its own `LayoutSpec` parameter, not mentioned anywhere in WP-9's own text — left as a candidate for a future increment rather than silently folded in here. Verified: Release build clean, `cvision_tests` 938/938, `ctest` (both targets) green, ASan/UBSan build + ctest green. |
| WP-10 | Open | Blocked on WP-11's `KeyChord::parse` for the descriptor's `.chord = "Alt+G"` string field (now landed — see WP-11) and on WP-11's registry-rendered `MenuItem`/`StatusLineItem` for `.menu`/`.status` to mean anything (not yet landed). The two WPs are co-dependent in practice despite the separate numbering; implementing one without the other leaves an incoherent halfway state, so this waits for WP-11's render-from-registry half. |
| WP-11 | **In progress** (key-name table + parse/format landed; registry-rendered menus/status still open) | `ckv::key_name(Key)`/`key_from_name(string_view)` — a canonical spelling table for every named key (`include/cvision/core/key.hpp`, `src/core/key.cpp`); `Key::Escape` is named `"Esc"`, matching the spelling already used everywhere else in this codebase's own comments and test names, not the enumerator's own name. `KeyChord::parse(string_view)` (static) accepts any of `"Ctrl"`/`"Alt"`/`"Shift"`/`"Super"` as `"+"`-joined, case-insensitive, any-order modifier prefixes, followed by either a table name or a single printable character (which becomes a lowercased `Key::Char` chord, matching the library-wide convention). `ckv::format(const KeyChord&)` is the inverse, rendering modifiers in a fixed Ctrl/Alt/Shift/Super order regardless of bit-set order (so `format` ∘ `parse` round-trips byte-for-byte) and upper-casing a single-letter `Char` chord for display (`"Alt+G"`, not `"Alt+g"` — parse still accepts either case). 20 new tests in `tests/test_key_chord.cpp`: name-table round-trip for every named key, case-insensitive lookup, `CKV_ASSERT` coverage for `key_name`/`format` on `Key::Char`/`Key::None`, parse acceptance/rejection cases, and format/parse round-trips across modifier combinations. Verified: Release build clean, `cvision_tests` 958/958, `ctest` (both targets) green, ASan/UBSan build + ctest green. Remaining, unblocking WP-10: `MenuItem`/`StatusLineItem` gain a command-referencing mode so title/chord-hint/enablement render live from `CommandRegistry` instead of a hand-set label (dropdown right-aligned chord column, from the M5 catalog baseline); golden-test coverage for that column; a script test proving a runtime rebind re-renders without touching item declarations; a grep sweep confirming no hand-encoded chord strings remain in `examples/`. |
| WP-12…22 | Open | — |

---

## Part I — Integration findings (mechanisms built, wiring missing)

The same failure shape recurs: a mechanism exists and is unit-tested,
but the connection that would let a real terminal session reach it was
never made, and no end-to-end test notices.

**F1 — The resize chain stops at the root.**
`Application::dispatch(ResizeEvent)` calls `root_.set_bounds(...)`
(`src/ui/application.cpp`); root is a plain `View` whose default
`on_resized()` does nothing, and nothing propagates the new bounds to
the Desktop child. `Desktop::on_resized()` — which re-pins docked
chrome and re-clamps windows (`src/widgets/desktop.cpp`) — is reachable
only by calling `desktop->set_bounds()` directly, which only tests do.
There is also no sanctioned way for an application to *observe* a
resize: root is a concrete `View` owned by value, `on_resized` is a
virtual (not a settable hook), and the typed notification route is
deferred. The sole Application-level resize test
(`test_application.cpp`, `resize_event_updates_the_roots_bounds`)
asserts root's bounds and nothing else. Interactive consequence, all
three examples: on terminal grow, the status line strands mid-screen
with dead cells below it; on shrink, chrome and windows are clipped
away. `docs/example-apps.md`'s claim that the file browser's chrome
"reposition themselves automatically across a terminal resize" is
currently false interactively.

**F2 — No click-to-focus.**
`Application::dispatch` delivers mouse events to the hit view without
any focus change, and widgets cannot self-focus (no Application
reference, by design). Clicking an unfocused `InputLine` moves its
cursor (`InputLine::on_mouse`) while keystrokes continue to flow to the
previously focused view. VISION non-negotiable #7 names click-to-focus
explicitly.

**F3 — No click-to-activate/raise for windows.**
`Desktop::activate` is called from `add_window`, `remove_window`, and
the cycling/select-by-number methods — never from any mouse path
(Desktop has no `on_mouse` and never sees clicks that hit a
descendant). Clicking a background window neither raises nor activates
it; after `cascade()` a buried window is unreachable by mouse.

**F4 — The zoom glyph is dead chrome.**
`Window::on_mouse` calls `toggle_zoom(bounds())` — zooming the window
into its own current bounds, a no-op — because a Window cannot know the
Desktop's `content_area()`. Only tests pass a real rect. Every window
draws a zoom control that does nothing when clicked.

**F5 — No window tracks desktop growth.**
`Desktop::on_resized` only clamps (`Window::reposition_within`); a
zoomed window stays at its old size when the desktop grows, and a
window sized to fill `content_area()` (the file browser's) stays small.
There is no per-window grow policy at all.

**F6 — Two dialog-attach paths with silently different behavior.**
`examples/gallery` attaches message boxes via `Desktop::add_child`
rather than `add_window`: the box is never activated (renders the
inactive single-line frame while holding focus), sits outside window
cycling, and does not deactivate the window below. `test_message_box.cpp`
attaches to bare roots, so nothing catches it. The API bug is that both
paths look plausible.

**F7 — The examples teach two contradictory chrome idioms.**
`examples/filebrowser` uses `dock_top`/`dock_bottom`;
`examples/gallery` still hand-positions its menu bar and status line
with one-time `set_bounds` calls — the exact stale-after-resize pattern
docking was built to eliminate.

**F8 — Examples squat on the reserved CommandId range.**
D-013 reserves ids below 4096 for the library's standard commands;
every example defines `kCmdQuit = 1`, `kCmd… = 2, 3, 4`. Root cause:
the standard command set does not exist yet, so there is nothing to
reference and no example of correct id allocation.

**F9 — Status-line and menu labels duplicate registry truth.**
`{"Alt-X Exit", kCmdQuit}` hand-encodes chord text the registry already
knows, and the encodings have already drifted ("Alt-X" in hello,
"Alt+X" in gallery/filebrowser). ARCHITECTURE §5 decides that menus and
status lines render title, chord, and enablement live from the
registry; the blocker is the missing Key→display-name table
(scope-noted in `widgets/status_line.hpp`).

**F10 — Status claims have drifted from the binding inventories.**
README's "M0 through M6 complete" does not hold against ROADMAP's own
exit criteria and `plans/widgets.md`: missing are the modality stack
and blocking conveniences (D-021, M5 scope), window backing stores and
the composition-only move benchmark (M5 exit), nested submenus /
accelerator-hint column / checkable menu items (M5 catalog baseline),
combo box / tab control / progress / splitter (M6a), the enforced
60-line hello budget (M5 exit), the minimum-terminal-size degraded
state (ARCHITECTURE §5 sizing policy), per-window theme scope, `run()`,
the localization string table, and desktop snapshot/restore. Each
absence is individually scope-noted and defensible; the milestone
status and the catalog's "binding inventory" language must say so.
`plans/widgets.md` now carries the status table; README's status
section is corrected as part of this review's landing.

## Part II — Ergonomics findings

**E1 — Theme + role threading is the dominant noise source.**
Every widget constructor takes `const Theme&` plus one to five
`RoleId`s (`Button` four roles, `Window` four, `MenuBar` five plus
`Application&` plus `Desktop&`; `make_file_dialog` eight parameters).
Roughly 40% of every construction expression is theming plumbing that
callers fill with the same `StandardRoles` fields every time. D-007
(flat semantic tables, per-widget role sets) does not require this:
widgets know their own role *names* and can resolve defaults
themselves, keeping an explicit override for the rare case (D-028).

**E2 — `StandardRoles` stops at M5, so M6 widgets borrow foreign roles.**
No list/tree/table/scrollbar/status roles exist; the file browser
styles its TreeView with `input_normal`/`input_focused` and its
scrollbars with `dialog_frame`/`dialog_background`. Restyling inputs
restyles trees — exactly the coupling the role system exists to
prevent.

**E3 — Five identical setup lines open every application**
(intern standard roles, install theme, construct Desktop, keep raw
pointer, add to root) — in all three examples and every future app.

**E4 — Commands are declared in triplicate.**
`register_command(id, title, category, chord)` +
`set_command_handler(id, fn)` + `MenuItem{title-with-&, id, nullptr,
false}` (+ a status item re-encoding the chord as text). ARCHITECTURE
§5 already decides the fix — "a one-call convenience registers command
+ handler + chord + menu item together" — it was never built.

**E5 — The ownership dance.**
`make_unique` → `set_bounds` → `raw = static_cast<T*>(
parent->add_child(std::move(x)))` → configure through `raw`. Return
conventions are mixed — `add_child` returns the *new* child as
`View*`, `set_content`/`set_frame_overlay` return the *old* one — so
examples are littered with downcasts
(`static_cast<widgets::MenuBar*>(desktop_->dock_top(...))`).

**E6 — Redundant `set_focus_policy(TabStop)` everywhere.**
Button, InputLine, ListView, TreeView, Memo, Table, and both option
groups already default to `TabStop` in their constructors — yet all
three examples set it again per widget, and the conversion chapter's
showcase snippet reproduces the redundancy. The examples teach phantom
API.

**E7 — Message boxes take five steps** (descriptor,
`make_message_box` with six arguments, attach, focus, result callback)
for the most common dialog operation in existence. The D-021 "blocking
convenience for applications that own the loop" is decided and absent;
so is any one-call non-blocking form.

**E8 — Every application hand-rolls quit.**
A `quit_` flag, a `should_quit()` accessor, and a bespoke `main` loop
in each example — because `Application` has neither `run()` (decided,
D-021) nor a quit request.

**E9 — Tab traversal is not bound by default.**
Every application must register a command binding Tab to
`focus_next()`. The documented rationale ("an application that wants
Tab for something else must be free to do that") is satisfied by an
overridable default, not by the absence of one — a focused editor
already consumes Tab in the focus chain before the keymap is
consulted. D-029 supersedes the old stance.

**E10 — No size-hint-change propagation.**
`Label::set_text` changes the label's preferred width and nothing
tells its parent; `Window::relayout_frame_overlay()` exists solely as
the manual workaround, and its header comment documents the gap. Any
hint-driven container has the same latent staleness.

## Part III — Style comparison and the conversion chapter

Construct-by-construct against the same application (`examples/hello`
vs. the classic tutorial app in its published API shape):

| Construct | Classic framework | ckVision today | Assessment |
|---|---|---|---|
| App shell + chrome | ~20 dense lines, constructor-callback protocol | ~28 clearer lines | Structurally better here, slightly longer; acceptable |
| One menu item | one expression carrying label + command + chord | `MenuItem{...}` + separate `register_command` + `set_command_handler` | Terser there; registry design is right, needs the one-call form (E4) |
| One button | one `insert(new ...)` expression incl. geometry and command | five lines incl. role plumbing, redundant focus policy, downcast | Clearly noisier today; E1+E5+E6 close the gap to 1–2 lines |
| Dialog modality | blocking `execView`, Esc cancels, default button | non-modal window, per-button close lambdas, Esc inert | Ergonomics *and* behavior gap until the modality stack lands |
| Resize behavior | one-time rects, stale after resize | docking mechanism exists; chain to the terminal unwired (F1) | One fix from decisively better |
| Theming, testability, safety, Unicode, mouse model | palettes, globals, none | roles, headless goldens, sanitizers | ckVision decisively better |

The conclusion the user-facing docs should be able to state truthfully:
a modern library must read *better*, not busier. The noise is
concentrated in four mechanical places (role plumbing, triple command
declaration, the ownership dance, missing conveniences) — all fixable
without touching the architecture.

`docs/turbo-vision-conversion.md` — corrections owed (items 2–5 land
with WP-17's rewrite of hello; item 1's behavior fix arrives via
WP-15/WP-21, its call-out immediately; the chapter is otherwise strong
and stays):

1. "Observable behavior is identical" overclaims: the original dialog
   closes on Esc (and honors a default-button rule); the port's window
   wires no `cancel_request`, so Esc does nothing. Wire Esc in the
   example or call the deviation out; add an Esc row to the test table.
2. An uncalled-out off-by-one: by the chapter's own re-anchoring rule
   (interior origin = window origin + 1,+1), buttons at window-y
   2/4/6/8 map to interior y 1/3/5/7; the port uses 0/2/4/6 (the
   static text follows the rule). A chapter whose thesis is "every
   structural deviation called out" must not contain a silent one.
3. The static-text width: the original rect is 12 columns wide; the
   port uses 14 while claiming direct translation.
4. The showcased snippet reproduces the redundant
   `set_focus_policy(TabStop)` line (E6) — the elegance-showcase
   document inflates ckVision's own verbosity.
5. "Both sides are honestly about the same length" holds for the shell,
   not the dialog body (~8 vs ~25 lines). Rewrite from the post-M9 API,
   at which point the sentence becomes simply true.

---

## Part IV — Work packages

Grouped by milestone. Every package names its findings, its design
shape where one is non-obvious, and its acceptance criteria. "Script"
means a headless event-script test; "golden" means a pinned styled-cell
dump.

### M8 — Integration correctness (findings F1–F7)

**WP-1 — Close the resize chain.** Direct children of
`Application::root()` are resized to root's bounds whenever root's
bounds change — the fill-the-root policy every real root child
(Desktop, or an application's own full-screen surface) wants. Escape
hatch: a child that must not fill the root sets an explicit flag
(`View::set_fills_root(false)` on the child, or an equivalent
root-layout policy object) — the default is fill, because the stale
alternative is never what anyone wants. Additionally, `View` gains a
settable `std::function<void(Rect)> on_bounds_changed` observer
(style-matching `on_press`/`on_closed`) so applications and tests can
react to any view's geometry change without subclassing.
*Acceptance:* script — headless terminal resized larger and smaller
mid-run through `Application::step`; asserts the status line occupies
the new bottom row, the menu bar spans the new width, a window
previously outside the shrunk area is reachable, and no stale cells
remain (golden per step). The false sentence in
`docs/example-apps.md` becomes true and gains a test reference.

**WP-2 — Click-to-focus. Landed AFTER delivery, not before (design
correction found during implementation).** On `MouseAction::Down`
outside any active popup input-capture, `Application::dispatch` walks
from the hit view upward to the nearest `focusable()` view and calls
`set_focus(...)`. The original plan below said "before delivering the
event"; implementing it turned up a real conflict: `MenuBar::on_mouse`
opens its dropdown via `activate()`, which reads `app.focused()` to
remember "what was focused before" for `deactivate()`'s restore. If
click-to-focus ran first, `activate()` would read the click's own new
focus (the bar itself) as "the prior focus," and Esc would restore to
the bar instead of whatever was truly focused before the click. Landed
shape: click-to-focus runs AFTER `target->on_mouse(event)` returns,
using the SAME `target` resolved before delivery, and is skipped
entirely when a popup already held input capture at the start of the
event (popups manage their own focus). `set_focus` is idempotent, so a
widget that already focused itself during delivery (as `activate()`
does) is unaffected — this is confirmed by
`tests/test_menu.cpp`:`clicking_the_bar_to_open_it_still_restores_the_true_prior_focus_on_escape`.
No `focus_on_click()` opt-out predicate was added: no current widget
needs one (a widget that must not take click focus already achieves
that by not setting `FocusPolicy::TabStop`), and the review's own
engineering standard is to not add abstractions beyond what a concrete
caller requires — add the predicate if and when a real widget needs a
focusable-but-not-click-focusable state.
*Acceptance:* script — click an unfocused InputLine in another window:
focus moves, subsequent typing lands there; click a Label: focus
unchanged; click a non-focusable child inside a focusable container:
the container focuses (the leaf still receives the click); Move/Up
events never move focus, only Down.

**WP-3 — Click-to-activate/raise.** Mouse Down dispatch gains a
capture-phase notification: before delivery, ancestors are walked
root→target through a new `View::on_descendant_mouse_down(View&
target)` hook (default no-op). `Desktop` overrides it to find the
owned window containing `target`, then `activate()`s (raise +
active-frame styling + deactivation of the previous). Popups keep
priority via the existing input-capture path, which bypasses the walk.
*Acceptance:* script — two overlapping windows; click the buried one's
visible edge: it raises, gets the double-line active frame, prior
window deactivates; click a widget inside an inactive window: window
activates *and* the widget focuses (WP-2 composes).

**WP-4 — Zoom that works, and windows that track the desktop.**
`Window` gains `std::function<Rect()> zoom_area_provider`;
`Desktop::add_window` installs `[this]{ return content_area(); }`.
The mouse zoom control and any keyboard zoom command use it; the
standalone-Window fallback stays current-bounds. `Desktop::on_resized`
re-applies `content_area()` to windows reporting `zoomed()`, then
clamps the rest. New per-window `DesktopGrowPolicy { None,
KeepFilling, AnchorEdges }` (default `None`): `KeepFilling` re-fills
`content_area()` on desktop growth when the window previously filled
it (the file browser's window declares it); `AnchorEdges` keeps
right/bottom distances (the classic grow-with-desktop behavior).
*Acceptance:* scripts — clicking the zoom glyph of a desktop-owned
window fills the content area and restores on second click; a zoomed
window still fills after the desktop grows; a `KeepFilling` window
follows; a `None` window only ever gets clamped. Golden for the zoomed
frame.

**WP-5 — One sanctioned dialog-attach path.**
`Desktop::show_dialog(WindowHandle)` (name final at implementation):
attaches via `add_window` (activation included), focuses the handle's
`initial_focus`, and is the documented way to present `make_message_box`
/ `make_file_dialog` / picker / window-list / help-viewer handles.
The handle struct is shared by all of them. Gallery migrates (F6);
`test_message_box.cpp` attaches through a Desktop so active-frame
styling is asserted at least once.
*Acceptance:* golden — a message box over a desktop shows the active
double frame while the window below shows inactive; cycling includes
the box; script — Esc/result/focus-restore unchanged.

**WP-6 — Example hygiene for the seams.** Gallery moves to
`dock_top`/`dock_bottom` (F7); all examples drop redundant
`set_focus_policy` calls (E6); filebrowser's window declares
`KeepFilling`.
*Acceptance:* gallery smoke gains a resize script step (chrome follows);
a repo-wide grep in CI (or a reviewer checklist line) keeps
`set_focus_policy(ui::FocusPolicy::TabStop)` out of examples for
widgets that already default to it.

### M9 — API ergonomics: the 60-line hello (findings E1–E9, F8, F9)

**WP-7 — Widgets resolve their own theme roles (D-028).** Constructors
drop `const Theme&` and every `RoleId`. Mechanism: `Application`
installs a ui context on its root (propagated to every current and
future descendant exactly like the existing dirty-rect and detach
sinks) carrying `{Theme&, RoleRegistry&, Application&}`; a widget
resolves its standard roles lazily at attach/first paint by interning
its documented role names (idempotent by D-007's registration
contract). Per-widget override: `set_role(RoleSlot, RoleId)` for the
rare deviating instance (the gallery "gray dialog window" case). A new
`View::on_attached()` notification fires when the context arrives —
also the natural home for other deferred wiring. Per-window theme
override (ARCHITECTURE §5 "application default with per-window
override") slots in here: `Window::set_theme_override(Theme)`, context
walk stops at the nearest override, materialized at scope-change time.
Widgets needing `Application&` for behavior (MenuBar focus save/
restore, StatusLine click dispatch) take it from the context, dropping
those constructor parameters too; `MenuBar`'s `Desktop&` parameter
falls to a parent-walk at attach.

**Implementation finding (Window's migration): `propagate_context`
always overwrites, even with an invalid context — by design (a single
authoritative source, matching how `dirty_rect_sink_`/`detach_sink_`
already behave), but it means attaching an already-`set_context`'d
child to a parent that has NO context yet silently erases the child's
context.** This never bites real application code (Desktop always
attaches to `Application::root()` — which is context-bearing from
construction — before anything attaches to Desktop), but it did bite
`tests/test_desktop.cpp`'s ~50 standalone-`Desktop` fixtures: a
`make_window(f)` helper that called `set_context` on the window
directly had that context immediately stomped by the very next
`desktop.add_window(...)` call, since the test's `Desktop` itself was
never given one. UBSan caught it immediately (`reference binding to
null pointer of type 'const Theme'` in `Window::draw`) once a window
actually needed to paint. Fix: `desktop.set_context(f.ctx())` on the
handful of `Desktop` instances that exercise `paint_children` — the
same shape a real `Application` gives every `Desktop` for free — not a
change to the propagation mechanism itself, which is doing the right
thing. Recorded here because it will recur for every remaining M6
widget test fixture that attaches through a standalone `Desktop`.
*Acceptance:* `Button b{"Ok"};` compiles and renders classic-correct
under every built-in scheme (golden × 4 schemes); constructing,
attaching, then switching themes at runtime re-resolves (existing
invalidate-all path); a detached widget never crashes resolving (it
defers); every widget's reference doc lists its role names — checked by
the docgen pass.

**WP-8 — Complete `StandardRoles`** with list/tree/table/scrollbar/
status-line/help-viewer role sets (E2); the M6 widgets stop borrowing
`input_*`/`dialog_*`; the built-in schemes gain the new entries (they
are complete by construction per D-007, so this is additive).
Prerequisite for WP-7's defaults.
*Acceptance:* filebrowser restyles lists without touching inputs
(scripted theme mutation asserts styles diverge); goldens updated
deliberately.

**WP-9 — Typed insertion.** `template <class T> T*
View::add(std::unique_ptr<T>)` preserving the concrete type (plus the
same shape on `Desktop::dock_top`/`dock_bottom`/`add_window`/
`add_popup`), and an emplace form `template <class T, class... A> T*
View::make(A&&...)`. `add_child` remains the type-erased primitive.
Every `static_cast` in examples and library-internal composition
disappears.
*Acceptance:* zero `static_cast<widgets::` occurrences in `examples/`;
compile-time.

**WP-10 — One-call command registration (decided in ARCHITECTURE §5
Menus).** A designated-initializer descriptor:

```cpp
app.commands().add({.id = kCmdGreeting,
                    .title = "&Greeting...",   // mnemonic carried once
                    .category = "Hello",
                    .chord = "Alt+G",           // parsed, WP-11
                    .menu = "&Hello",           // menu path; creates the bar entry
                    .status = true,             // advertise on the status line
                    .handler = [this] { greeting_box(); }});
```

registering metadata + handler + menu item + status item in one place.
Menu structure stays authored (order/separators via an explicit
structure API or a `.menu` path with declaration order); every menu
item remains a command underneath. The old granular calls remain the
primitive layer for large applications with central tables.
*Acceptance:* hello and filebrowser declare each command exactly once;
script — chord fires, menu item fires, status item clicks, all through
the same id.

**WP-11 — Key-name table + chord literals; menus and status lines
render from the registry (F9).** `KeyChord::parse("Alt+G")` /
`format(KeyChord) -> "Alt+G"` with a canonical, documented spelling
table for every `Key`; `MenuItem`/`StatusLineItem` referencing a
command stop carrying label text — title (with mnemonic), chord hint,
and enablement render live from the registry (dropdown right-aligned
chord column included, from the M5 catalog baseline). Hand-labeled
items remain possible for the no-command case.
*Acceptance:* golden — dropdown shows right-aligned chords; script —
rebinding a chord at runtime re-renders menu and status line without
touching item declarations; grep — no hand-encoded chord strings in
examples.

**WP-12 — Standard command set (F8, D-013 materialized).**
`ckv::ui::std_commands`: `kQuit`, `kClose`, `kZoom`, `kNextWindow`,
`kPreviousWindow`, `kTile`, `kCascade`, `kWindowList`, `kMenu`,
`kHelp`, with reserved ids below 4096, registered (with default chords
where the classic grammar has them — F10 menu, F1 help; window cycling
per the documented scheme) by `Application` at construction under
D-029. Examples move their own ids above 4096 and reference the
standard set where applicable.
*Acceptance:* registry dump in the reference docs lists the set; a
CKV_ASSERT (debug) rejects application registration inside the
reserved range; examples grep-clean.

**WP-13 — Default keymap (D-029).** Tab/Shift-Tab bound to
`focus_next()`/`focus_previous()` out of the box; F10 → `kMenu` once a
MenuBar is attached; the window-management chords bound to the
standard commands. All defaults live in the ordinary rebindable keymap
— an application overrides or clears them like any binding; a focused
editor consuming Tab in the focus chain already preempts the keymap by
routing order.
*Acceptance:* script — Tab traverses in a fresh Application with no
app-side setup; a view consuming Tab still receives it; unbinding
works. The filebrowser deletes its Tab plumbing; its comment block
(and the matching `docs/example-apps.md` paragraph) is replaced by a
reference to D-029.

**WP-14 — `run()` and quit (E8, D-021's convenience).**
`Application::run()` — the documented loop (`step` until quit) with a
default frame deadline; `Application::request_quit()` /
`quit_requested()`; the quit sweep (close-request veto across windows,
ARCHITECTURE §5) hooks in here when the modality stack lands.
`main.cpp` of every example becomes: construct terminal/clock/app,
construct the app object, `app.run()`.
*Acceptance:* examples' `main.cpp` ≤ 10 lines each; `should_quit()`
flags deleted; embedding via bare `step()` unchanged and still tested.

**WP-15 — Modality stack + blocking conveniences (D-021 completion;
E7).** The decided design: a modal push scopes event routing
(accelerators included) to the modal subtree until completion via
callback; `exec_*` blocking wrappers for loop-owning applications
(`exec_message_box(app, desktop, descriptor) -> MessageBoxResult`,
same for dialogs/file dialogs) implemented over `step()` — no nested
native loop. Non-blocking one-liners ride WP-5's `show_dialog`.
*Acceptance:* scripts — input outside a modal window is scoped out
(clicks don't activate background windows; accelerators of background
scope don't fire); Esc/Enter grammar; focus restore; `exec_` form
returns the pressed result in a headless script; re-entrancy (post
during modal, close during modal) covered; the hello greeting becomes
a real modal dialog and its chapter's deviation note shrinks
accordingly.

**WP-16 — Size-hint-change propagation (E10).**
`View::size_hint_changed()` protected notifier → parent virtual
`on_child_size_hint_changed(View&)`; Row/Column/Desktop docks/Window
overlay re-layout on it; `Label::set_text`, `Button::set_text`,
`StaticText` wrapping call it. `Window::relayout_frame_overlay()` is
deleted (its call sites too), the filebrowser drops the manual call
and its explanatory comment.
*Acceptance:* script — growing the overlay label live re-right-aligns
it (the current manual call removed); Row with a text-grown child
re-distributes on the next layout pass; no relayout storms (one
relayout per batch, asserted by the existing zero-allocation/
counter instrumentation where applicable).

**WP-17 — The 60-line hello, enforced.** With WP-7…WP-15 landed,
rewrite `examples/hello` to the target shape (menu bar + status line +
modal greeting dialog, one command declaration each), and enforce the
M5 exit criterion that was never wired: a CI check that
`hello_app.cpp` + `hello_app.hpp` + `main.cpp` together stay ≤ 60
non-comment, non-blank lines.
*Acceptance:* the check runs in CI and fails on regression; the
conversion chapter's snippets regenerate from the new file; goldens
re-pinned deliberately.

### M10 — Layout and widget-model completion (findings E-layout, P2/P3)

**WP-18 — Anchors for fixed-geometry children.** The modern equivalent
of the classic per-view grow flags: `Anchors{left, top, right, bottom}`
(any combination; opposite edges anchored ⇒ stretch) interpreted for
children of a new `ui::AnchorPane` container — and honored by `Window`
content interiors via an `AnchorPane`-by-default content when the
caller wants free placement with resize behavior. A bottom-right
status label keeps its corner distance; an anchored-all-edges list
stretches with the dialog.
*Acceptance:* unit — the distribution math pure-function tested like
`distribute_main_axis`; script — resizing a window keeps a
bottom-right-anchored child in the corner and stretches an all-anchored
child; golden before/after.

**WP-19 — Finish D-006's container set.** `Grid` (row/column spans,
per-cell alignment), `Dock` (all four edges + center — also the
Desktop's left/right sidebar story), `Overlay` (z-stacked children,
the layered-composition primitive), per-child cross-axis alignment and
margins for Row/Column (retiring the documented v1 fill-only
limitation), and `Splitter` (from the M6a catalog; keyboard-adjustable
per the catalog's beyond-baseline column).
*Acceptance:* per-container unit + script suites in the established
layout-test style; the file browser's two panes gain a Splitter; docs
per container.

**WP-20 — Frame overlays, plural and positioned.**
`Window::add_frame_overlay(view, FrameSlot{Edge, Alignment, offset})`
replacing the single hard-wired bottom-right slot (`set_frame_overlay`
retires; its call sites migrate). Multiple overlays coexist (path
indicator bottom-left + "line:col" bottom-right); top-edge slots
compose with the title/controls without overlap (deterministic
precedence documented).
*Acceptance:* golden — two overlays on one window; script — overlay
repositions on window resize and on its own size-hint change (WP-16);
clamped gracefully on narrow frames.

**WP-21 — Dialog and window defaults; degraded minimum size.**
`wire_dialog_window` (and the modal dialog path) sets
`set_resizable(false)` unless the descriptor opts into resizable-with-
layout (the catalog's beyond-baseline row); dialog-hosting windows get
Esc-cancel semantics by default (fixes the conversion chapter's
behavior gap at the root). The ARCHITECTURE §5 sizing policy's missing
end state lands: below the minimum supported terminal size, degraded
chrome down to a hard floor, then a deterministic "terminal too small"
state instead of corrupted output.
*Acceptance:* scripts — Esc closes a descriptor-built dialog bypassing
validation (already specified) *without* explicit wiring; a fixed
dialog shows no resize affordance; shrink-storm script drives the
terminal below the floor and asserts the too-small state golden, then
recovery on re-grow.

**WP-22 — Tree/list model polish (the master-detail experience).**
`TreeNode` gains a user payload (`std::uint64_t id` plus `std::any
user_data` — non-template, ABI-friendly), killing the sidecar-map
pattern in the file browser; `TreeView::on_expand_request(TreeNode&)`
enables lazy population (with `TreeNode::children_known` so a
collapsed directory can show an expander before its children are
listed); selection/activation callbacks pass the node
(`std::function<void(TreeNode&)>`), ListView's pass the index; Left on
a collapsed node jumps to the parent (the parent-tracking pass the
header already sketches). The file browser drops `record_paths`/
`path_of_` and the depth-8 prebuild in favor of lazy expansion.
*Acceptance:* filebrowser smoke unchanged in observable behavior plus
a new lazy-expansion script (expanding a never-listed directory
populates it on demand through the injected filesystem); unit tests
for payload stability across expand/collapse; docs updated.

### M11 hand-off (unchanged release milestone, renumbered)

Backing stores + damage-scoped painting (the performance charter items
currently scope-noted in `ui/application.hpp` and
`widgets/window.hpp`), the localization string table, desktop
snapshot/restore, nested submenus / checkable items (menu catalog
remainder), combo box / tab control / progress (M6a catalog remainder)
— tracked in `plans/widgets.md`'s status table — land before or with
the release milestone as its entry gate. This review adds no new scope
there; it only makes the ledger visible.

---

## Sequencing and effort

Order within the program: WP-1…WP-6 first (user-visible breakage,
small, independent), then WP-7/WP-8/WP-9 (the single biggest elegance
jump), then WP-15 (modality — the biggest remaining behavior gap),
then WP-10…WP-14/WP-16/WP-17, then M10's packages. Coarse effort,
same planning basis as ROADMAP's figures: M8 ≈ 1–2 pw, M9 ≈ 2–3 pw,
M10 ≈ 2–3 pw.

## The target picture

What `examples/hello`'s dialog should look like once M9 lands — the
measuring stick for every ergonomics package (shorter than the classic
original, with none of its magic):

```cpp
void HelloApp::greeting_box() {
    auto window = std::make_unique<widgets::Window>("Hello, World!");
    window->set_bounds({25, 5, 30, 11});
    auto* content = window->set_content(std::make_unique<ui::AnchorPane>());
    content->make<widgets::StaticText>("How are you?")->set_bounds({2, 4, 12, 1});
    int y = 0;
    for (const char* label : {"Terrific", "Ok", "Lousy", "Cancel"}) {
        auto* b = content->make<widgets::Button>(label);
        b->set_bounds({15, y, 12, 2}); y += 2;
        b->on_press = [w = window.get()] { w->close(); };
    }
    desktop_->show_dialog(std::move(window));  // attach + activate + initial focus + Esc closes
}
```
