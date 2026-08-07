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
| WP-10 | **In progress** (metadata+chord+handler half landed; `.menu`/`.status` auto-population deliberately deferred) | `ui::CommandDescriptor{.id, .title, .category, .chord, .handler}` + `CommandRegistry::add(descriptor)` — one designated-initializer call replaces the `register_command(...)` + `set_command_handler(...)` pair (three, counting hand-building a `KeyChord`, now just a `KeyChord::parse`-able string). `.handler` needed somewhere to land without giving `CommandRegistry` (in `ui::`) a dependency on `Application`, so handler storage/dispatch moved INTO `CommandRegistry` itself (`set_handler`/`execute`, both self-contained — no `Application` needed, consistent with `CommandRegistry` already being usable standalone in every test in `tests/test_command.cpp`); `Application::set_command_handler`/`execute_command` are now one-line forwards, kept because callers already have an `Application&` in scope everywhere dispatch happens — not deleted, since they're a legitimate convenience, not dead API. All 3 examples converted: every `register_command`+`set_command_handler` pair becomes one `commands.add({...})` call. Deliberately NOT implemented: `.menu`/`.status` fields that would auto-create/populate a `MenuBarItem`/`StatusLineItem` from the descriptor, per WP-10's own illustrative snippet. Root-cause reason, not a shortcut: `CommandRegistry` sits in `ui::`, below `widgets::` in this project's own dependency direction (AGENTS.md — "domain engines and models at the bottom, frontend-neutral application logic above them"); it cannot reference `MenuBar`/`StatusLine` without inverting that. Building menu/status structure from command metadata also needs registration-ORDER tracking `CommandRegistry` doesn't keep (`commands_` is an `unordered_map`), and a real design for separators/nesting the plan's own text leaves genuinely open ("via an explicit structure API or a `.menu` path with declaration order"). This is real, separate design work for a `widgets::`-layer mechanism that READS `CommandRegistry`, not something to bolt on as a `CommandDescriptor` field a caller could set and have silently do nothing — deferred with reasoning recorded here rather than rushed. Because of this, the acceptance criterion ("hello and filebrowser declare each command exactly once") is not yet fully met — examples still separately construct `MenuItem`/`StatusLineItem` referencing each command's id, alongside the one `add()` call for its metadata/chord/handler. New coverage: `tests/test_command.cpp` gains 9 tests (`execute`/`set_handler` behavior including disabled-command and handler-before-registration cases, `add()`'s metadata/chord-parsing/handler-wiring, empty-chord and unparseable-chord cases). Verified: Release build clean, `cvision_tests` 976/976, `ctest` (both targets) green, ASan/UBSan build + ctest green. |
| WP-11 | **Done** | Part 1 (key-name table + `KeyChord::parse`/`format`) landed first — see the commit history for that half. Part 2 (this batch): `CommandRegistry::chord_for_command(id)` — the reverse of `command_for_key`, "the first chord currently bound to `id`" (a command may have more than one; this is display's one representative pick), reflecting runtime rebinds, not just whatever `default_chord` it was registered with. `MenuItem`/`StatusLineItem` referencing a command now stop carrying their own label text: `DropdownMenu::item_source_text`/`item_chord_hint` pull the title and chord live from `context().app->commands()` for sizing, mnemonic navigation, and drawing (right-aligned chord column, e.g. `"Ctrl+S"` flush against the dropdown's right edge); `StatusLine::effective_label` composes `"{chord} {title}"` (e.g. `"Alt+X Quit"`), stripping any `&` mnemonic marker from the title via `parse_mnemonic` since the status line never navigates by letter. A hand-labeled item (`command == kInvalidCommand`) is unaffected — labels remain fully supported for callback-only items and separators. All 3 examples updated to stop hand-duplicating command titles/chords in menu and status labels (`gallery_app.cpp`'s `kCmdActivateMenu` title shortened `"Activate Menu Bar"` → `"Menu"` so the SAME title reads naturally in both the status-line hint and any future menu use; `hello_app.cpp` gained the F10-activates-the-menu wiring gallery already had, since removing its hand-typed `"F10 Menu"` hint meant giving it a real command to reference — the status line's claim is now actually true, not just cosmetic). `tests/golden/hello_{initial,greeting}.dump` regenerated via the documented `generate_hello_golden` procedure — the only diff is `"Alt-X Exit"` → `"Alt+X Exit"` (the hand-typed hyphen was already inconsistent with this codebase's own `"+"` convention used everywhere else; composing it from `format()` fixes that inconsistency as a side effect). New coverage: `tests/test_command.cpp` (`chord_for_command`, 4 tests), `tests/test_menu.cpp` (registry-rendered title/chord rendering including the right-aligned-column golden-style Surface assertion, a live-rebind-changes-the-render test, and mnemonic-navigation-uses-the-title-not-the-label, 5 tests). Grep-verified: zero hand-encoded chord strings (`"Alt+"`, `"Ctrl+"`, `"F10 "`, `"F1 "`) remain in `examples/`. Verified: Release build clean, `cvision_tests` 967/967, `ctest` (both targets) green, ASan/UBSan build + ctest green. |
| WP-12 | **Done** | `ckv::ui::std_commands` — 10 reserved `CommandId`s (`kQuit`, `kClose`, `kZoom`, `kNextWindow`, `kPreviousWindow`, `kTile`, `kCascade`, `kWindowList`, `kMenu`, `kHelp`) below `kFirstApplicationCommandId`, populated by a new `CommandRegistry::register_standard_commands()` that `Application`'s constructor calls once. Titles carry a conventional `&`-mnemonic except `kMenu`/`kHelp` (invoked by chord directly, not usually a menu item themselves); default chords bound where there's a strong public convention (F1/F10/F6/Shift+F6/F5/Alt+F3/Alt+X) — this project's own authorship per its provenance rule, no prior source consulted; `kTile`/`kCascade`/`kWindowList` get none. `register_command`/`add` now `CKV_ASSERT(id >= kFirstApplicationCommandId)` — the reserved range can only be populated by `register_standard_commands()` itself (a private `register_command_internal` is the shared unchecked core both paths call). New `CommandRegistry::all()` returns every registered command sorted by id, for anything that enumerates the registry. Found and fixed a real shadowing bug while wiring this in: `Application::dispatch`'s KeyEvent case checked `command_for_key` before its old F1-specific `help_provider_` fallback — once F1 became `kHelp`'s live default chord, that generic check always fired first and the fallback became unreachable. Fixed by giving `kHelp` an always-installed default handler (the exact old fallback logic, moved into the constructor) and deleting the now-redundant special case from `dispatch()` — F1 flows through the same generic path as every other standard chord now. This is an intentional, documented contract change: `dispatch()` reports F1 as "handled" once unconsumed by the focus chain (the command ran), regardless of whether the provider itself found anything to show — that visible-effect nuance is observable through the provider callback, not the return value, matching every other standard chord. 5 of `test_application.cpp`'s F1 tests updated to the new contract (2 renamed and re-asserted true, 1 already correct, 2 unaffected). All 3 examples rewired: `gallery_app.cpp` needs zero application-space command ids of its own anymore (all 4 of its commands are exactly the standard ones); `filebrowser_app.cpp` drops its duplicate Quit; `hello_app.cpp` keeps its own Quit/Greeting (Quit's title is pinned to "Exit," matching Turbo Vision's own `hello.cpp` tutorial, not the framework's generic "Quit" wording — a deliberate, documented reason NOT to reference the standard command, not a gap) but adopts `kMenu` for its F10 status hint. New `docs/standard-commands.md` (referenced from `command.hpp`'s own file comment) documents the set, the mnemonic/chord design reasoning, and the "keep your own command" escape hatch, with its table generated via a new `tools/docgen/generate_standard_commands_table.cpp` (regeneration is a manual, reviewed act, exactly like a golden-fixture update — never automatic). New coverage: 8 tests in `test_command.cpp` (reserved-range `CKV_ASSERT` for both `register_command` and `add`, boundary-exact allowed case, `register_standard_commands` populates every id with a title, binds the documented default chords, leaves Tile/Cascade/WindowList unbound, `all()` empty-registry and sorted-by-id cases). Verified: Release build clean, `cvision_tests` 984/984, `ctest` (both targets) green, ASan/UBSan build + ctest green. |
| WP-13 | **In progress** (Tab/Shift-Tab default traversal + `kMenu`'s MenuBar-attachment auto-wiring landed; window-management chords' Desktop-side auto-wiring deliberately deferred) | Two new `std_commands` ids, `kFocusNext`/`kFocusPrevious`, bound to Tab/Shift-Tab by default with handlers `Application`'s constructor installs unconditionally (`focus_next()`/`focus_previous()` — Application itself outlives everything that could invoke them, so there's no lifetime concern to guard, unlike `kMenu` below). `MenuBar::on_attached()` now installs itself as `kMenu`'s default handler (calling `activate()`) the moment it attaches — but ONLY if `CommandRegistry::has_handler(kMenu)` is still false, so an application that calls `set_handler(kMenu, ...)` itself before attaching a `MenuBar` is never silently overridden; the destructor clears the handler again if THIS instance was the one that installed it (tracked via a new `installed_default_menu_handler_` bool — a SECOND `MenuBar` that finds `kMenu` already claimed never installs, and correspondingly never clears it on its own destruction, verified by a dedicated test). New `CommandRegistry::has_handler(id)` is the guard this pattern needs. Found and fixed a real, more serious bug while wiring MenuBar's destructor cleanup — not a MenuBar-specific one: `Application`'s member declaration order had `root_` declared BEFORE `commands_`/`roles_`/`theme_`, meaning those three were already DESTROYED (C++ destroys members in reverse declaration order) by the time `root_`'s own teardown cascaded into destroying its children — ANY widget destructor touching `context().theme`/`context().roles`/`context().app->commands()` during `Application` teardown was already a latent dangling-reference bug, just never exercised until this handler-clearing code tried it (caught immediately, as a SIGSEGV, by the very first test exercising it — confirmed as a real use-after-destroy by ASan going clean only after the fix). Fixed at the root cause: reordered `Application`'s members so `roles_`/`theme_`/`commands_` are declared (and therefore destroyed last) before `root_`. Deliberately NOT implemented: the "window-management chords bound to the standard commands" half of D-029's own text — `kClose`/`kZoom`/`kNextWindow`/`kPreviousWindow`/`kTile`/`kCascade` auto-wired to real `Desktop` operations once one is attached, mirroring `kMenu`'s pattern. Deferred for a real sequencing reason, not effort alone: `kClose`'s eventual default handler needs the close-request veto sweep across windows (ARCHITECTURE.md §5), which is explicitly WP-15's own scope ("hooks in here when the modality stack lands" — WP-15's own text) — wiring it now would mean reworking it once WP-15 lands; better to land both together. `examples/filebrowser`'s manual Tab-to-`focus_next` command (and its explanatory comment predicting exactly this) is deleted per the acceptance criterion, along with the matching `docs/example-apps.md` paragraph, both replaced by references to D-029. New coverage: `tests/test_command.cpp` (+3 `has_handler` tests, existing standard-set tests extended to cover `kFocusNext`/`kFocusPrevious`), `tests/test_application.cpp` (+4: Tab/Shift-Tab traverse with no app-side setup, a consuming view still receives Tab before the default fires, unbinding removes the default), `tests/test_menu.cpp` (+4: attaching installs the default, a pre-existing handler is never overridden, destroying the bar clears its own installed handler, a second bar that never installed doesn't clear the first bar's handler on its own destruction). Verified: Release build clean, `cvision_tests` 995/995, `ctest` (both targets) green, ASan/UBSan build + ctest green — the ASan pass is what confirms the member-reorder fix actually closed the dangling-reference bug, not just silenced its one observed crash. |
| WP-14 | **Done** | `Application::run()` — steps repeatedly (`step(now + kDefaultFrameIntervalNanos)`, the same 200ms every example's hand-rolled loop already used) until `request_quit()`/`quit_requested()` (backed by `std::atomic<bool>`, relaxed ordering) reports true. Installs its own `SIGINT` handler for the duration via a file-static `std::atomic<Application*> g_run_instance` (a signal handler cannot capture `this`; the atomic holds whichever `Application` is currently inside its own `run()`, correctly restoring the previous instance/handler on return, including the nested/sequential-`run()`-calls case) and restores whatever handler was previously installed before returning — so Ctrl+C requests a clean quit and `main()` returns normally, letting `Terminal`'s destructor restore the terminal's original mode, rather than the process dying mid-raw-mode under `SIGINT`'s default disposition. `step()`/`wake()` are completely untouched and remain signal-agnostic — `run()` is a separate, optional entry point layered on top, not a replacement; an application embedding ckVision in its own event loop keeps calling `step()`/`wake()` directly with no signal-handling side effect at all. `quit_requested_` is `std::atomic<bool>` rather than a plain `bool`: `handle_run_sigint` calls `request_quit()` on the signal-handler stack, and reading/writing a non-atomic, non-`volatile sig_atomic_t` object there concurrently with the rest of the program is undefined behavior per the C++ standard even though it happens to work on every real target — this project's own ASan/UBSan discipline is the reason to close that gap rather than leave a technically-UB-but-works-in-practice flag. All three examples' `should_quit()`/`quit_` duplication deleted; each Quit-command handler now calls `app_.request_quit()` directly. Every `main.cpp` rewritten to construct terminal/clock/app/app-object then `app.run(); return 0;` — `hello`/`gallery` are 6 body lines, `filebrowser` (which also parses an optional root-path argv) is 7, all comfortably under the ≤10-line acceptance bar; `gallery/main.cpp` additionally dropped a batch of now-unused widget includes it no longer needs now that `main()` no longer touches widget types directly. New coverage: `tests/test_application.cpp` (+3: `quit_requested()` false by default, `request_quit()` sets it, `run()` returns immediately without stepping if quit was already requested before the call — `run()`'s actual loop/signal-delivery behavior beyond the immediate-return case is deliberately not unit-tested, since a real SIGINT can't be delivered to a test process without an unacceptably flaky test; verified instead via the three examples' own manual runs). `tests/test_filebrowser_smoke.cpp`, `tests/test_gallery_smoke.cpp`, `tests/test_hello_golden.cpp` updated from the deleted `X.should_quit()` to `f.app.quit_requested()`/`app.quit_requested()`. Verified: Release build clean, `cvision_tests` 998/998, `ctest` (both targets) green, ASan/UBSan build + ctest green. |
| WP-15 | **Done** (modal-stack mechanism, `kClose`/`kQuit` default handlers, and `exec_window`/`exec_message_box` all landed; `exec_dialog`/`exec_file_dialog` deliberately deferred — no concrete signature to validate against yet) | `Application::push_modal(View&)`/`pop_modal()`/`is_modal()` — a `std::vector<View*>` stack, `ui::`-layer (operates on `View&`, never `Window`/`Desktop`, preserving the dependency direction). While non-empty: `topmost_view_at()` hit-tests starting AT the innermost modal instead of `root_` (a click outside its bounds matches nothing, never falls through to whatever's underneath); `focusable_views()` walks that same subtree instead of the whole tree (Tab/Shift-Tab stay confined); `dispatch()`'s `KeyEvent` branch stops its focus-chain `on_key` walk once it reaches the modal root itself (never continuing to whatever it's nested inside) AND skips the global accelerator-keymap fallback entirely (a background chord, including `kMenu`/`kQuit`, simply cannot fire). `pop_modal()` takes no argument and never dereferences the popped `View*` — by the time an `exec_*` wrapper is ready to pop, the modal's own `Window` may already be destroyed (self-detach runs to completion inside the very `step()` call that produced the result), so popping is by stack position, never identity. Nested pushes scope correctly to the innermost modal (`nested_modals_scope_to_the_innermost_and_popping_restores_each_level`), proactively covering VISION.md's "nested modality" even though the WP's own acceptance text only describes a single level. New shared primitive `Application::run_until(std::function<bool()> done)`: steps at `run()`'s own cadence until `done()` or `quit_requested()`, returning `done()`'s final value so a caller can tell "completed" from "interrupted by a quit request" — `run()` itself is now `run_until([]{return false;})`, and every `exec_*` wrapper below is built on it too, so a SIGINT arriving while one is blocked (e.g. a modal `exec_message_box` mid-dispatch inside an outer `run()`) unwinds cleanly instead of hanging forever waiting for terminal input that will never come (`exec_window_returns_promptly_if_quit_is_requested_before_the_window_closes` is the regression guard for this). `Desktop::on_attached()` installs `kClose`/`kQuit`'s default handlers — guarded by `has_handler`, cleared by a new `~Desktop()` only if THIS instance installed them, the exact pattern `kMenu`'s own WP-13 default established, now with `app_`/two `installed_default_*_handler_` bools mirroring `MenuBar`'s own fields. `kClose` closes the active window (vetoable, exactly what its own close-control click already does); `kQuit` sweeps every window front-to-back through that same `Window::close()` protocol and calls `request_quit()` only if none veto (ARCHITECTURE.md §5 "application quit sweeps all windows through the same protocol" — the half WP-14 explicitly deferred here). `Desktop::exec_window(Application&, WindowHandle)` — the blocking sibling of `show_dialog`: attaches+focuses via `show_dialog`, `push_modal`s the attached window, `run_until`s "the window is no longer in `windows()`" (not "a result was captured" — this is what makes it generalize to a future `exec_dialog`/`exec_file_dialog` with no result payload at all), then `pop_modal`s. `widgets::exec_message_box(Application&, Desktop&, StandardRoles&, MessageBoxDescriptor&)` — captures a local `std::optional<MessageBoxResult>` via the same `on_result` callback `make_message_box` already accepts, calls `exec_window`, returns `result.value_or(escape_result(...))` (the quit-interrupted fallback reuses the exact "Esc" mapping every button/Esc path already produces). Deliberately NOT implemented: `exec_dialog` (for `DialogDescriptor`/`materialize_dialog`) and `exec_file_dialog` — the plan's own text only commits to one concrete signature (`exec_message_box`) plus "same for dialogs/file dialogs," and neither has an existing caller or validated shape to test against; `exec_window` is the generalized primitive both would compose over once one does, mirroring WP-10's own "don't bolt on an untested shape" precedent. `examples/hello`'s `greeting_box()` becomes a REAL modal — `push_modal`/`show_dialog`/`pop_modal`-from-`on_closed`, deliberately NOT `exec_window`: it has no result to consume, and D-021 itself decouples modality (loop state) from blocking (an opt-in convenience), so reaching for a blocking call here would only complicate its own test suite for no benefit — `cancel_request` is wired for the first time too, closing a real, previously-flagged gap (Esc used to do nothing). `docs/turbo-vision-conversion.md`'s "one deliberate deviation" section is rewritten: observable behavior is now fully identical to the original (all four buttons AND Esc dismiss it, nothing outside it responds while it's open), the only remaining difference is non-observable (blocking vs. non-blocking return) — this plan's own finding #1 about the overclaim is marked fixed. `docs/standard-commands.md` gains a `kClose`/`kQuit` default-handler bullet matching `kHelp`'s own. Found and fixed one real testing-infrastructure bug while writing coverage, not a product bug: a `Desktop` built with a STANDALONE `Theme`/`RoleRegistry` (as most of `test_desktop.cpp`'s existing fixture already did) crashes on first paint once actually attached under a real `Application` and exercised through `step()` — `context()` propagates from the ATTACHING PARENT's own theme/roles, not from whatever the widget was constructed with, so a `Window`'s resolved role ids (real indices into the Fixture's registry) get resolved against the Application's own, disconnected, empty registry instead, tripping `RoleRegistry::fallback`'s bounds `CKV_ASSERT`. Existing tests never hit this because they never both attach AND actually paint before their own self-detach removes the widget from the tree first; `exec_window`'s pump is the first thing in this codebase to reliably do both. Fixed by giving every new fixture that attaches for real (`test_desktop.cpp`'s `AttachedFixture`, `test_message_box.cpp`'s three `exec_message_box` tests) the same `app.theme() = make_classic_theme(app.roles(), roles)` dance every real example's own constructor already does — not a workaround, the actually-correct setup the standalone-`Desktop` tests were never exercising. New coverage: `tests/test_application.cpp` (+8: focus/mouse/accelerator/on_key-ancestor-walk scoping, nested push/pop, `pop_modal` on an empty stack aborts via the house fork+`SIGABRT` idiom, `run_until` stops-on-predicate and stops-on-quit), `tests/test_desktop.cpp` (+12: `kClose`/`kQuit` guarded install/no-override/destructor-cleanup, `kClose` closes-active/no-op-when-none-active, `kQuit` sweeps-and-requests-quit/stops-at-first-veto, `exec_window` blocks-until-closed, runs-posted-work-during-the-pump ["post during modal"], is-not-disrupted-by-an-unrelated-window's-close ["close during modal"], returns-promptly-on-quit), `tests/test_message_box.cpp` (+3: `exec_message_box` returns the pressed result / the Esc result / restores focus), `tests/test_hello_golden.cpp` (+1: background accelerators scoped out while the greeting is open, restored once Esc closes it — the existing golden-dump and button-dismissal tests needed zero changes, since the non-blocking-modal design keeps `dispatch()` fully synchronous). Verified: Release build clean, `cvision_tests` 1022/1022, `ctest` (both targets) green, ASan/UBSan build + ctest green. |
| WP-16 | **Done** | `View::size_hint_changed()` (new protected notifier) → `View::on_child_size_hint_changed(View&)` (new public virtual, default no-op — same shape as `on_descendant_mouse_down`) — a single hop to the immediate parent only, synchronous, never deferred through `Application::post()`; recorded as a fresh decision, `DECISIONS.md` D-030, since nothing existing constrained the contract (checked first — no prior D-number or ARCHITECTURE.md text specified stop-at-first-ancestor vs. continue-to-root, or sync vs. deferred). `Row`/`Column::on_child_size_hint_changed` just calls `relayout()` — their own aggregate hints are never cached (computed fresh from `children()` every call), so there is nothing to invalidate beyond re-running the existing pass. `Window::on_child_size_hint_changed` re-derives `frame_overlay_`'s bounds only if `&child == frame_overlay_` — `content_rect()` depends solely on `bounds()`, never on `content_`'s hint, so a content-child notification is correctly a no-op (verified by a dedicated test, not just inferred). `Desktop::on_child_size_hint_changed` re-runs `on_resized()` wholesale (not a smaller slice) if the child is `top_dock_`/`bottom_dock_` — a docked view's height growing shrinks `content_area()`, which must re-clamp every owned window via `reposition_within`, not just reposition the dock itself. `Label::set_text`/`Button::set_text`/`StaticText::set_text` all call `size_hint_changed()` after their existing `invalidate()`; `StaticText`'s case is honestly flagged as not yet visibly consequential — neither `Row`/`Column` nor `Desktop` currently call `height_for_width()` at all (only tests do), a separate, pre-existing gap this WP does not attempt to close, so wrapping-driven height changes propagate the notification but have no container today that acts on it. `Window::relayout_frame_overlay()` is deleted — declaration, definition, and its doc comment describing the very gap this WP closes — along with its only two call sites: `examples/filebrowser/filebrowser_app.cpp`'s manual call (and the three-line comment explaining why it was needed) is removed, leaving just the `set_text()` call the acceptance criterion asks for; `tests/test_window.cpp`'s two `relayout_frame_overlay_*` tests are replaced (not merely deleted) with tests proving the same two guarantees — changed-width overlay repositioning, no-overlay-set safety — now hold automatically. `ARCHITECTURE.md`'s "Layout and dialog construction" section gains a short paragraph describing the mechanism, citing D-030. New coverage: `tests/test_view.cpp` (+3: no-op before attachment, notifies only the immediate parent — not a grandparent — proving the single-hop contract directly, the default override does nothing), `tests/test_layout.cpp` (+2: a `Row` re-distributes when a `Minimum`-policy child grows without the `Row` itself resizing — the acceptance criterion's literal scenario — and a single hint change triggers exactly one relayout pass on a spy sibling, the "no relayout storms" criterion), `tests/test_desktop.cpp` (+2: a docked view's own height change repositions it without a desktop resize, and separately re-clamps an owned window against the now-shrunk content area — the "full `on_resized()` treatment" claim, not just the dock's own two lines), `tests/test_widgets.cpp` (+3: `Label`/`Button`/`StaticText`'s `set_text` each notify a spy parent exactly once, and not at all before attachment), `tests/test_window.cpp` (net +1: 2 rewritten/replaced, 1 new — a real `widgets::Label` overlay growing stays right-aligned, matching the acceptance criterion's literal wording, not just the `FixedWidthView` test-helper proof). Verified: Release build clean, `cvision_tests` 1033/1033, `ctest` (both targets) green, ASan/UBSan build + ctest green. |
| WP-17 | **Done** (line budget is 74/75, not the plan's original ≤60 — a deliberate, owner-approved trade-off, not a silently loosened gate) | The M5 exit criterion is wired for real: `tools/check_hello_line_budget.py` counts non-comment, non-blank lines across `hello_app.hpp`+`hello_app.cpp`+`main.cpp`, registered as a new `ctest` (`hello_line_budget`, mirroring `layer_check`'s exact pattern — required `Python3`, never silently absent) — CI already runs `ctest` for every OS/config in the matrix, so no `ci.yml` change was needed. The plan's own aspirational ≤60 turned out unreachable while keeping Turbo Vision's own `hello.cpp` tutorial faithfully — its distinctive four-button "Terrific"/"Ok"/"Lousy"/"Cancel" greeting and "Exit" (not generic "Quit") status-line wording — even after real compaction (aggregate/designated initializers for `MenuBarItem`/`MenuItem`/`StatusLineItem`; `Desktop::exec_window` replacing hand-rolled `push_modal`/`pop_modal`/`on_closed` bookkeeping; removing `hello`'s own `kMenu` handler wiring, now fully redundant with `MenuBar`'s own WP-13 default; adding dialog children directly to `Window` instead of through a `content()` wrapper, since this dialog's bounds never change so `content_rect()`'s auto-reposition has nothing to do). Presented the honest trade-off to the owner (60-line budget vs. the tutorial's distinctive dialog): keep full fidelity and set the enforced budget to the real, re-measured number (74, two lines of headroom) rather than cut the greeting to a generic single-button message box or silently disable the gate — the owner's call, recorded here rather than made unilaterally, since it changes what the flagship example — and its accompanying conversion-chapter documentation — actually teaches. `greeting_box()` now calls `Desktop::exec_window` (M9/WP-15) instead of the bare `push_modal`/`show_dialog` WP-15 landed it with — this makes it BLOCK its caller exactly like the original's `execView()`, closing what was WP-15's own last documented non-observable difference entirely (not just scoping input, actually blocking too), while still needing zero test-suite changes beyond `tests/test_hello_golden.cpp` itself (see below). Two more `docs/turbo-vision-conversion.md` corrections this WP owed land here too (its own "corrections owed" list, items 2–3): the static-text width is corrected to 12 columns (was 14 — the chapter's own "direct translation" claim is now actually true, "How are you?" being exactly 12 characters); the button-position "off-by-one" clarity issue (raw Rect literals looked inconsistent with the chapter's own stated re-anchoring rule, even though the FINAL rendered position was always correct via `content_rect()`'s hidden +1 offset) is resolved by construction once dialog children stop going through that hidden indirection — both snippets in the doc are regenerated from the actual current file rather than left stale (they predated WP-10/WP-11/WP-12's own command-registry API entirely). The "one remaining structural difference" section is rewritten as "no remaining difference" (renamed header: "Blocking without a nested native loop") since `exec_window` closes the blocking gap; the dialog-body length claim is corrected to the true, honestly-measured 10-vs-19 lines (not "same length" — a real, disclosed ergonomics gap: `TButton`'s constructor takes a command Turbo Vision's own dialog machinery already knows how to close on, ckVision's `Button` has no equivalent, so each button needs its own explicit `on_press`; flagged as a real candidate for a future "buttons that close their dialog by default" convenience, not swept under the rug); the geometry paragraph stops asserting a specific TV-to-ckVision coordinate formula it can't verify without consulting Turbo Vision source (forbidden by this project's own provenance rule) — states only what's directly checkable against the pinned golden dump. A stray doubled "ckVision ckVision Hello" typo in `main.cpp`'s header comment is fixed in passing. Found and fixed while rewriting: `hello`'s own explicit `commands.set_handler(kMenu, ...)` (and the `menu_` member it needed) was fully redundant with `MenuBar::on_attached()`'s own default-install (WP-13) — removed from both `hello_app.hpp` and `hello_app.cpp`, a genuine simplification independent of the line-budget pressure that motivated finding it (the SAME redundancy exists in `examples/gallery/gallery_app.cpp`, flagged separately since it's out of this WP's own scope). New/rewritten coverage in `tests/test_hello_golden.cpp` (net line count unchanged, 6 tests before and after): every test that now needs to inspect or interact with the dialog *while it's open* queues a closure via `Application::post()` before dispatching Alt+G, since `exec_window` means `dispatch()` itself no longer returns until the dialog closes — the golden-frame test specifically needs a SECOND level of `post()` indirection (queue-a-closure-that-queues-a-closure) so its capture runs on the step() call AFTER the one that painted the dialog, not the same one (`step()` drains posted work before it paints, so a single-level closure would see the PRIOR frame). Confirmed via a hang (`SIGALRM` under a 20s test-harness timeout) that the FIRST, naive rewrite attempt — reusing the old direct-`dispatch()`-then-inspect shape unchanged — genuinely breaks with a blocking `greeting_box()`, not just a theoretical concern. The rendered output is BYTE-IDENTICAL to before this whole rewrite for both the button-position and static-text-width fixes (`hello_greeting_dialog_frame_matches_the_pinned_golden_dump` passes unchanged, no golden regeneration needed) — StaticText's own default role happens to resolve identically to the dialog's own background fill in the Classic theme, and the button/text Rect math nets out to the exact same absolute window-local cells whether reached via the old content-wrapper indirection or the new direct-to-window placement. Verified: Release build clean, `cvision_tests` 1033/1033, `ctest` (all three targets, including the new `hello_line_budget`) green, ASan/UBSan build + ctest green. |
| WP-18 | **Done** | New `include/cvision/ui/anchor_pane.hpp`/`src/ui/anchor_pane.cpp`, mirroring `Row`/`Column`'s own shape and file layout (`ui::` layer, `add_item`/`remove_item`, a private `specs_` map). `ui::Anchors{left, top, right, bottom}` (all `false` by default). The pure distribution function — `apply_anchors(Rect current, Anchors anchors, int delta_width, int delta_height) -> Rect`, unit-testable in isolation exactly like `distribute_main_axis` — is INCREMENTAL by design: it takes the child's CURRENT bounds and the pane's size DELTA since the last pass, not an "original bounds" recorded once and an absolute old/new pane size, so repeated resizes compose correctly without `AnchorPane` needing to remember anything per-child beyond its `Anchors` (`AnchorPane` itself tracks exactly one extra piece of state, `last_size_`, shared across every child). Per axis: both opposite edges anchored stretches the child by the delta; only the far edge (right/bottom) anchored shifts the child by the delta, size unchanged; the near edge only, or neither, leaves the child's bounds on that axis completely untouched — deliberately the SAME resulting geometry as "anchored near-edge-only," since a plain Rect placement with no `Anchors` specified already behaves like it was anchored top-left, needing no special case. `AnchorPane::on_resized()` walks `children()` unconditionally (no dirty-guard, matching `Row`/`Column::relayout()`'s own unconditional-loop style — `View::set_bounds`'s own no-op guard for identical bounds already makes a zero-delta resize cheap per child). `Window::content_pane()` is the opt-in integration point named in the plan's own text ("honored by Window content interiors... when the caller wants free placement with resize behavior"): lazily installs a fresh `AnchorPane` as the window's content the first time it's called (via the EXISTING `set_content`, so it inherits that path's own `content_rect()` sizing for free — no new positioning logic needed in `Window` itself), and returns the SAME pane on every later call; `CKV_ASSERT`s if `content()` is already set to something `content_pane()` didn't itself install (the two ways of claiming the one content slot, `set_content()` and `content_pane()`, are not composable — a caller picks one). New coverage: `tests/test_anchor_pane.cpp` (17 tests — 9 for `apply_anchors` covering every anchor combination on both axes plus a shrinking-delta case; 8 for `AnchorPane` itself: a child keeps its own given bounds on insertion, a bottom-right-anchored child keeps its corner distance across a resize — the acceptance criterion's literal scenario — an all-anchored child stretches to fill, a no-anchors child is completely unaffected by a resize, repeated resizes compose correctly, `remove_item` returns ownership/stops tracking/doesn't crash a later resize, `remove_item` for an unowned view returns null, and a child added AFTER one resize is correctly unaffected by that EARLIER resize's own delta on the NEXT one — the specific case that would catch a bug in tracking "original bounds per child" instead of the incremental delta design actually used), `tests/test_window.cpp` (+4: `content_pane()` lazily creates a correctly-sized pane, returns the same pane on repeated calls, an anchored child added through it keeps its corner on a real window resize end-to-end, and calling it on a window whose content is something else `CKV_ASSERT`s — via the house fork+`SIGABRT` idiom). The plan's own "golden before/after" acceptance wording is satisfied by these Rect-equality before/after assertions rather than a new byte-exact `Surface` dump fixture — consistent with `Row`/`Column`'s own precedent, which pins bare layout-container positioning the same way; this codebase's golden dumps are reserved for whole-application regression tests (`hello`/`gallery`/`filebrowser`) and widget-visual-fidelity tests, a distinction `AnchorPane`, a layout primitive with no rendering of its own, does not need. No example was wired to use `AnchorPane`/`content_pane()` — the plan's own acceptance text for this WP doesn't ask for one (unlike WP-19's explicit "the file browser's two panes gain a Splitter"). Verified: Release build clean, `cvision_tests` 1054/1054, `ctest` (all three targets) green, ASan/UBSan build + ctest green. |
| WP-19 | **Done** | Five deliverables, each its own build+test+ASan-verified commit. **(1) Row/Column cross-axis alignment and margins**, retiring the documented v1 fill-only limitation: `LayoutSpec` gains `Alignment alignment` (`Fill`/`Start`/`Center`/`End`, defaulting to `Fill` — every existing Row/Column call site unaffected) and `margin_before`/`margin_after`; a new pure function `align_cross_axis(available, preferred, alignment, margin_before, margin_after) -> (offset, extent)` (`layout.hpp`/`.cpp`) is the cross-axis counterpart to `distribute_main_axis`, declared explicitly reusable by Grid's own per-cell alignment. `Row`/`Column::relayout()` call it per child; `distribute_main_axis` itself is untouched. **(2) `ui::Grid`** (row/column spans, per-cell alignment): a deliberate v1-scoped UNIFORM sizing model — `rows`/`columns` fixed at construction, each row/column sharing the container's extent on its axis evenly (content-driven track sizing is real future scope, not implemented) — mirroring Row/Column's own former "v1 scope" precedent. A child may span multiple consecutive rows/columns; placement within its spanned cell reuses `align_cross_axis` directly (no duplicate alignment math). One new pure function, `distribute_grid_cells(available, count, spacing) -> [(offset, extent)]`, the grid analogue of `distribute_main_axis`. Does not aggregate child hints into its own (like `AnchorPane`, a placement container, not a flow container). **(3) `ui::Dock`** (edges + center — the classic border layout, and the general "sidebar next to main content" primitive): a STANDALONE `ui::` container, deliberately NOT restructuring `Desktop`'s own existing `top_dock_`/`bottom_dock_` (narrower, already-shipped, left untouched). At most one child per edge (including Center) — several views on one edge nest a Row/Column there, same answer Row/Column give beyond their own scope. Reservation order is fixed (Top, Bottom, Left, Right, Center last) so Left/Right only ever span the vertical gap Top/Bottom left, never the full height. New pure function `compute_dock_layout(available, DockEdgeExtents) -> DockRects`, clamped so no edge overflows or goes negative. **(4) `ui::Overlay`** (z-stacked children): z-order is exactly `View::children()`'s own list order (no separate z-index concept) — a later layer is added later, an existing one restacks via the already-public `View::raise_to_front()`. Each child is `Fill` (resized to the overlay's own full extent on insertion and every resize) or `Manual` (keeps whatever bounds it was given, untouched by the overlay's own resize). No new pure function (Fill placement is exactly the container's own bounds) and deliberately does not override `on_child_size_hint_changed` (neither mode depends on a child's hint) — a real design difference from Row/Column/Grid/Dock, documented as such, not an oversight. **(5) `widgets::Splitter`** (M6a catalog: "keyboard-adjustable split panes"): unlike the four `ui::`-layer primitives above, Splitter is a genuine focusable, drawing, key-handling widget, so it lives in `widgets::`. Owns exactly two panes (`first()`/`second()`) separated by a one-cell divider bar it draws (`painter.vline`/`hline`) and holds keyboard focus on (new `ckv.splitter.normal`/`ckv.splitter.focused` `StandardRoles`, 35 → 37 total). `Orientation` (`Horizontal`/`Vertical`) is hoisted out of `Scrollbar` into a new shared `widgets/orientation.hpp` rather than redeclared, since both widgets now need it. `split_position()` is the first pane's own main-axis extent in cells (not counting the divider) — an ABSOLUTE cell count, not a proportional ratio, is deliberately what persists across a later resize (the standard splitter UX: the pane the user explicitly sized keeps that size, the OTHER pane grows/shrinks with the window); the constructor's own default (half of the initial bounds) is what gives a freshly built Splitter its exact 50/50 starting split. Left/Right (Horizontal) or Up/Down (Vertical) adjust by one cell while focused, clamped to leave both panes at least their own size hint's min. Mouse-drag resizing is deliberately out of v1 scope — the catalog's own spec says "keyboard-adjustable," matching Grid's own precedent of implementing exactly the documented baseline. `Window::content_rect()` made public (was private) so a caller can size a content view correctly BEFORE constructing it, when the view's own constructor needs a real `Rect` up front, exactly Splitter's (and Grid's) own shape — the root-cause fix for the file-browser integration below, not a workaround. `examples/filebrowser/filebrowser_app.cpp`'s two-pane `ui::Row` (`LayoutSpec{Expanding,1}` on both, an exact 50/50 split) is replaced by a `widgets::Splitter(window->content_rect(), std::move(tree), std::move(file_list))` — same starting 50/50 ratio (Splitter's own default), Left/Right now lets the user adjust it; a new `splitter()` accessor exposes it for testing, the build_window()-time comment documenting the app's Tab-traversal behavior is updated to mention the Splitter now shares the cycle. New coverage: `tests/test_layout.cpp` (+16: 8 pure-function, 8 Row/Column integration, including a regression guard proving default/no-alignment output is byte-identical to pre-WP-19 behavior), `tests/test_grid.cpp` (new, 16 tests: 5 pure-function, 10 Grid integration, 1 fork+`SIGABRT` span-exceeds-bounds contract test), `tests/test_dock.cpp` (new, 17 tests: 7 pure-function including the Left/Right-never-touch-Top/Bottom ordering property, 10 Dock integration, 1 fork+`SIGABRT` one-child-per-edge contract test), `tests/test_overlay.cpp` (new, 8 tests), `tests/test_splitter.cpp` (new, 19 tests: construction/default-split, keyboard adjustment both orientations, clamping, absolute-position-preserved-across-resize, aggregate size hints, drawing), `tests/test_standard_roles.cpp` (role count 35→37 across all four enumeration/coverage tests), `tests/test_filebrowser_smoke.cpp` (+3: the panes start at an even 50/50 split, the Splitter's `first()`/`second()` are exactly the tree/file-list panes, adjusting the Splitter resizes both panes and keeps them contiguous). Verified: Release build clean (zero warnings throughout all five sub-commits), `cvision_tests` 1133/1133, `ctest` (all three targets, including `layer_check`) green, ASan/UBSan build + ctest green. |
| WP-20 | **Done** | `Window::add_frame_overlay(view, FrameSlot{edge, alignment, offset})` replaces the single hard-wired bottom-right `set_frame_overlay`/`frame_overlay()` (retired — all 3 call sites migrated: the implementation, `examples/filebrowser`, `tests/test_window.cpp`). New `enum class Edge { Top, Bottom }` — deliberately just these two, not `ui::Dock`'s full four-edges-plus-center set: a window's side borders are one cell wide, no room for meaningful overlay content. `FrameSlot::alignment` reuses `ui::Alignment` (`Start`/`Center`/`End`/`Fill`) rather than inventing a new enum — the same values Row/Column/Grid already position children with; `frame_overlay_rect` positions an overlay within its edge's own row by calling `align_cross_axis` directly, the exact function Row/Column/Grid already use for cross-axis placement. `offset` nudges the aligned result left/right, clamped back into the row's own available span. At most one overlay per (edge, alignment) pair — `CKV_ASSERT`s on a second, mirroring `ui::Dock`'s own "at most one child per edge" contract; several overlays at the same position nest in a Row the caller builds. `Edge::Top` further restricts alignment to `Start`/`End` only (`CKV_ASSERT`s against `Center`/`Fill`) since the title already owns Top's center — this is the "deterministic precedence" the plan's own acceptance text asks for, made an explicit contract violation rather than an implicit "whoever paints last wins" stacking rule; recorded as DECISIONS.md D-031. Top's own margins track whichever controls `draw()` actually draws at the window's current width (`point_in_close_control`/`point_in_zoom_control`'s own gates — an overlay never sits under a control that isn't there); Bottom keeps the original single-overlay design's 2-cell-from-each-corner margin on both sides (visual symmetry with Top, and clearance for the resize grip's own corner cell). Storage is `std::unordered_map<View*, FrameSlot>` (mirroring Grid/Dock/Overlay's own `specs_` pattern) instead of the old single raw pointer; `on_child_size_hint_changed` now looks up which specific overlay changed and repositions only that one, not a blanket relayout of every overlay (verified by a dedicated test). `examples/filebrowser`'s path-label overlay migrates to `add_frame_overlay(std::move(path_label), widgets::FrameSlot{})` — the default slot is Bottom/End, so its own observable position is completely unchanged. The plan's own "golden — two overlays on one window" acceptance wording is satisfied by Rect-equality/non-overlap assertions rather than a new byte-exact `Surface` dump fixture, consistent with WP-18's own precedent for this same class of acceptance text. New coverage: `tests/test_window.cpp`'s frame-overlay section rewritten and extended (18 tests, net +9: the pre-existing single-overlay scenarios — default-slot positioning, resize repositioning, narrow-window clamping, content/overlay coexistence, live size-hint-driven repositioning — all re-verified against the new API, plus new coverage for two overlays coexisting without overlap [the literal path-indicator/line:col acceptance scenario], Top/Start and Top/End margins against the close/zoom controls, a narrow-window Top case with neither control drawn, `offset` nudging and its own clamping, the one-overlay-per-slot and Top-Center/Fill contract violations via the house fork+`SIGABRT` idiom, and one overlay's hint change never repositioning a different overlay). Docs updated: README.md's first-review summary, `docs/example-apps.md`'s File Browser section (both the `add_frame_overlay` rename and the WP-19 Splitter migration it had not yet caught up to), DECISIONS.md gains D-031. Verified: Release build clean, `cvision_tests` 1141/1141, `ctest` (all three targets) green, ASan/UBSan build + ctest green. |

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

1. **Fixed by WP-15.** "Observable behavior is identical" overclaimed:
   the original dialog closes on Esc (and honors a default-button
   rule); the port's window wired no `cancel_request`, so Esc did
   nothing. `greeting_box()` now pushes a real modal scope
   (`Application::push_modal`) and wires `cancel_request`; the chapter's
   text and test table (`while_the_greeting_dialog_is_open_background_
   accelerators_are_scoped_out` in `tests/test_hello_golden.cpp`) both
   reflect it.
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
