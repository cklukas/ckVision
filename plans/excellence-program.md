<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# ckVision 2.0 Engineering, UX & Visual Excellence Program (WP-85…WP-96)

Status: Authoritative post-`v1.4.0` clean-architecture execution plan established
2026-11-15 under the owner's clean-design directive: **one uncompromising,
zero-cruft library targeting `v2.0.0` directly on `main` — no legacy fallbacks,
no backward-compatibility shims, no multi-ABI `#ifdef` forks — paired with a
complete, mechanical client upgrade guide (`docs/migration-2.0.md`)**.

## 1. Charter: The 30-Year Terminal UI Standard

ckVision's mission is to surpass both classic 1990s CUA desktop-in-a-terminal
usability and modern TUI toolkits, establishing the definitive C++ terminal UI
library for 2026 and the next three decades:

1. **Zero-compromise end-user usability**: Instant keyboard fluency (`F10` menu,
   `Alt+<mnemonic>`, `Alt+1..9` window select, `Alt+0` window list, `Ctrl+F5`
   keyboard move/resize, `F12` + `Ctrl+Shift+P` universal command palette,
   `Alt+-` window control menu), complete spatial discoverability (live command
   enablement, automatic status-bar hints explaining both active commands and
   *why* disabled commands are unavailable), effortless mouse affordances
   (double-click title bar to zoom, right-click window/list context menus,
   accurate pixel-aware dragging), and WCAG-verified contrast across every
   built-in theme.
2. **Best-in-class terminal visual craft**: Crisp single/double/mixed box
   junctions, surface-aware 3D control shadows, adaptive Bayer-dithered Sixel
   rasters with smooth downscale/upscale filtering, direct 32-bit RGBA Kitty
   Graphics where supported, and publication-grade SVG/PNG snapshots.
3. **Sub-millisecond `O(damage)` & `O(line)` engineering**: 32-byte zero-heap
   `Cell`s, `O(line)` incremental document editing, `O(damage)` delta-SGR frame
   emission, `O(1)` `Image` identity caching, and hardware `DECSTBM` scroll
   regions over high-latency SSH links.
4. **Pure, 1-screen C++20 SDK ergonomics**: Zero mandatory dependencies, zero
   legacy shims or duplicate overloads, compile-time prevention of layout
   container misuse, `Application`-owned theme/roles/localization so dialog and
   widget calls stay concise, and a crystal-clear `docs/migration-2.0.md` guide
   for updating client applications.

---

## 2. Settled Architectural & UX Decisions (D-152…D-158)

All open design questions are settled in favor of architectural purity and
recorded in `DECISIONS.md` as the opening step of `WP-85`:

- **D-152 — Single-Version Architectural Purity & `docs/migration-2.0.md` (Supersedes D-137's 1.x Lock)**:
  ckVision maintains one clean target (`v2.0.0`) on `main` without backward-
  compatibility shims, umbrella-header stubs, duplicate legacy overloads, or
  conditional ABI flags. Whenever a design improvement breaks an existing source
  or binary contract — shrinking `Cell` to 32 bytes, deleting
  `common_components.hpp` in favor of focused per-widget headers, hiding
  `View::add_child` on layout containers, removing `<windows.h>` from public
  headers, or simplifying dialog factories — all internal callers, examples,
  tests, and documentation update in the same commit, and `docs/migration-2.0.md`
  records the exact Before -> After client change.
- **D-153 — Explicit Color Roles, Zero `Attr::Reverse` Pre-Swap & Automated WCAG 2.1 Gates**:
  Built-in themes (`Classic`, `Dark`, `Light`, `Mono`, `High Contrast`) express
  focused, selected, active, and invalid states through explicit `(fg, bg)`
  colors without setting `Attr::Reverse` on pre-swapped pairs. Automated CTest
  gates enforce WCAG 2.1 contrast minimums (`>= 4.5:1` for body, selection, and
  syntax tokens; `>= 7.0:1` in `High Contrast`; `>= 3.0:1` for gutters,
  comments, and inactive chrome), zero chromatic RGB channels in `Mono`, zero
  24-bit RGB leaks in `Classic`, and zero `Attr::Dim` in `High Contrast`.
- **D-154 — Adaptive Bayer-Dithered Sixel, Filtered Scaling & Probed Kitty Graphics**:
  Raster placement defaults to `ImageSampling::Auto` (integer fixed-point
  box-averaging on downscale, fixed-point bilinear interpolation on upscale;
  `ImageSampling::Nearest` opt-in for pixel art), and `Painter::draw_image`
  replaces `std::function` directly with lightweight `FunctionRef`.
  `SixelEncoder` deduplicates in `0..100` percent space, applies deterministic
  adaptive quantization up to `min(max_colors, 256)` with coordinate-anchored
  4x4 Bayer ordered dithering (seam-free across occluded slices), and composites
  RGBA alpha over resolved cell backgrounds (`P2=1` transparent holes for
  `a == 0`). `Capabilities::kitty_graphics` is probed and implemented as the
  primary 32-bit RGBA raster path (`\x1B_Ga=T,f=32,...`), degrading
  automatically to upgraded Sixel and then to declared textual fallbacks.
- **D-155 — Desktop-in-a-Terminal UX Standard**:
  `Desktop` installs live `set_enabled_predicate`s alongside its default window
  command handlers; `standard.window_list` binds `Alt+0` by default;
  `CommandRegistry` supports secondary chords so `standard.command_palette`
  responds to both `Ctrl+Shift+P` (kitty protocol) and portable `F12` (non-kitty
  and SSH terminals); `StatusLineItem` drops its redundant legacy `command`
  field in favor of `CommandPresentation`; `ApplicationShell` automatically
  connects `MenuBar::on_highlight_changed` to `StatusLine::set_transient_hint`
  (rendering contextual help and `disabled_reason` explanations) and pairs with
  a shipped `make_standard_window_menu()` helper; `Window` restricts chrome
  clicks/drags to `MouseButton::Left`, toggles `Zoom` on title-bar double-click,
  and offers a Window Control Menu on `Alt+-` and title-bar right-click;
  `CommandPalette` uses multi-token + subsequence ranking across category and
  title with right-aligned chords.
- **D-156 — 32-Byte Inline `Cell`, `O(damage)` Presentation, `DECSTBM` & `O(line)` Editing**:
  `Cell` shrinks directly from 48/56 bytes to **32 bytes** via 15-byte inline
  UTF-8 storage plus compact interning for rare `> 15B` ZWJ sequences,
  eliminating per-cell heap allocations. `TextEditor` edits and caret motion run
  in `O(line)` without materializing full-document strings; `Presenter` diffs
  only `Compositor`-damaged regions using allocation-free delta-SGR, `O(1)`
  monotonic `Image` generation IDs, and a byte-bounded LRU Sixel slice cache;
  unoccluded full-width vertical scrolls emit `DECSTBM` + `CSI S`/`CSI T`.
- **D-157 — Cross-Layer Wide-Glyph & Zero-Width Neutralization Invariants**:
  Overwriting or clipping half of a 2-column glyph in `Surface`, `Compositor`,
  `Presenter`, or `TerminalEmulator` atomically clears the partner cell to `' '`
  with the overwritten cell's background; Unicode `Cf` format/BiDi controls and
  `U+00AD` are neutralized in `sanitize_display_text`; UTS #51
  `Emoji_Modifier_Base` skin-tone sequences and `VS15`/`VS16` selectors resolve
  deterministically.
- **D-158 — `Application`-Owned `UiStrings` & Clean Dialog/Container Ergonomics**:
  The four fragmented label structs (`StandardStrings`, `DateTimeLabels`,
  `PropertyInspectorMessages`, `WizardLabels`) and all ~80 remaining hardcoded
  framework literals collapse directly into one unified `UiStrings` owned by
  `ui::Application` (`app.strings()`, alongside `app.theme()` and cached
  `app.standard_roles()`). Standard dialog factories drop their redundant
  `roles` and `strings` parameters and unify on one clean parameter order:
  `(app, desktop, descriptor_or_options, ...)`. Layout containers (`Row`,
  `Column`, `Grid`, `Dock`, `AnchorPane`, `Overlay`) make raw `View::add_child`
  `protected` so bypassing layout is a compile-time error.

---

## 3. Master Work-Package Queue (WP-85…WP-96)

| WP | Wave | Package | Est. | Prerequisites | Primary Layers |
|---|---|---|---:|---|---|
| **WP-85** | I. Foundation & Safety | Authority (`D-152…D-158`), `migration-2.0.md` scaffold, fuzzer/CI & build hygiene | 1 pw | WP-84 | `DECISIONS.md`, `fuzz`, `tests`, `cmake`, `ci` |
| **WP-86** | I. Foundation & Safety | 32-byte inline `Cell`, checked 64-bit geometry & Unicode/wide-glyph integrity | 2 pw | WP-85 | `core`, `scene`, `term` |
| **WP-87** | I. Foundation & Safety | POSIX/Windows session safety, `<windows.h>` removal & `InputDecoder` completeness | 1.5 pw | WP-85 | `core`, `term` |
| **WP-88** | I. Foundation & Safety | Lifetime safety, focus visibility, `TextEditor` scroll/wrap & `Desktop` invariants | 1.5 pw | WP-86, WP-87 | `scene`, `ui`, `widgets` |
| **WP-89** | II. Visual Craft | Theme overhaul, automated WCAG 2.1 gates & surface-aware chrome | 1.5 pw | WP-88 | `scene`, `ui`, `widgets`, `examples` |
| **WP-90** | II. Visual Craft | Adaptive Bayer-dithered Sixel, `FunctionRef` smooth scaling & Kitty Graphics | 2.5 pw | WP-88 | `core`, `scene`, `term`, `widgets` |
| **WP-91** | II. Visual Craft | High-fidelity SVG/PNG DEFLATE snapshot engine & golden refresh | 1 pw | WP-89, WP-90 | `scene`, `tools/docgen`, `tests/golden`, `docs` |
| **WP-92** | III. UX & Speed | Desktop-in-a-terminal UX mastery (menus, windows, `CommandPalette`) | 2 pw | WP-91 | `ui`, `widgets`, `examples` |
| **WP-93** | III. UX & Speed | `O(line)` incremental `TextEditor` & syntax lexer state machines | 2 pw | WP-88 | `widgets`, `bench` |
| **WP-94** | III. UX & Speed | `O(damage)` `Presenter`, delta-SGR, `DECSTBM` scroll-intent & Sixel LRU | 2 pw | WP-86, WP-90 | `core`, `scene`, `term`, `bench` |
| **WP-95** | IV. Pure SDK & Closure | Data views, unified `ListItem`, `FileDialog`, `Table` & `TerminalView` UX | 2 pw | WP-92 | `core`, `widgets` |
| **WP-96** | IV. Pure SDK & Closure | Pure modular SDK headers, `app.strings()`, compile-safe layouts & `2.0` docs closure | 2.5 pw | WP-93, WP-94, WP-95 | `include`, `src`, `examples`, `docs` |

---

## 4. Detailed Work-Package Specifications & Exit Criteria

### Wave I — Core Foundation, Safety & Protocol Integrity

#### WP-85 — Authority (`D-152…D-158`), `migration-2.0.md` scaffold, fuzzer/CI & build hygiene
- **Scope & Changes**:
  1. Record `D-152…D-158` in `DECISIONS.md` (explicitly superseding `D-137`'s 1.x compatibility lock), create `docs/migration-2.0.md` with structured sections updated by every breaking WP, and register `WP-85…WP-96` in `plans/corrective-assurance.md` and `plans/00-status.md`.
  2. Fix `decode_seed_escapes()` in `fuzz/fuzz_common.hpp` to decode `\\` -> `\` and add escaped-backslash regression seeds; run `-runs=64` deterministic mutation smoke in `fuzz/CMakeLists.txt`.
  3. Register `add_test(NAME native_system_log_contract COMMAND native_system_log_contract)` in `tests/CMakeLists.txt` and move `TMPDIR`/`TMP`/`TEMP` injection below the final `add_test()`.
  4. Deduplicate `.github/workflows/ci.yml` so tests run once per job and `pty` `--repeat until-pass:2` is effective; add `generate_completion_goldens`, `generate_path_dialog_goldens`, and `generate_logmonitor_goldens` to `tests/check_generated_goldens.py`; return skip code `77` when `tmux` is absent in `tools/vtconform/conform.py`.
  5. Add `CVISION_API` symbol-visibility macros, `POSITION_INDEPENDENT_CODE ON`, `SOVERSION 2`, `$<BUILD_INTERFACE:...>` sanitizer flag scoping, and `-DBUILD_SHARED_LIBS=ON` CI verification; guard `install()`/CPack under `PROJECT_IS_TOP_LEVEL`; use `GNUInstallDirs` in `tests/install_package_smoke.cmake`; remove stale `openssh-client(s)` from `cmake/Distribution.cmake`; set `_O_BINARY` on `stdin` in `tools/vtconform/dump_emulator.cpp` on Windows; replace `/tmp` fallbacks in `tools/publish/make-public.sh` with mandatory `TMPDIR`.
- **Acceptance Exit**:
  - All CTest suites including `native_system_log_contract`, `provenance_check`, `check_license_headers`, `check_generated_goldens`, `install_package_smoke`, shared-library build, and all 13 fuzzers pass cleanly on macOS, Linux, and Windows.

#### WP-86 — 32-byte inline `Cell`, checked 64-bit geometry & Unicode/wide-glyph integrity
- **Scope & Changes**:
  1. **Direct 32-byte `Cell` (`include/cvision/core/cell.hpp`, `src/core/`)**: Replace `std::string grapheme_` directly with 15-byte inline UTF-8 storage (`char bytes_[15]` + `std::uint8_t len_and_flags_`) plus a compact interned table for rare `> 15B` multi-ZWJ sequences. Enforce `static_assert(sizeof(Cell) == 32)`, eliminating per-cell heap allocations across `Surface`, `Compositor`, `Presenter`, and `TerminalEmulator`. Document any `Cell` API refinements in `docs/migration-2.0.md`.
  2. **Checked 64-bit geometry & `Image` identity (`include/cvision/core/geometry.hpp`, `include/cvision/core/image.hpp`)**: Fix signed 32-bit overflow UB in `Rect::intersected`/`right()`/`bottom()` and `Image` stride validation; stamp `Image` with a process-monotonic `std::uint64_t` content ID for `O(1)` caching; cast `Presenter` raster UV math to `std::int64_t` before multiplication.
  3. **Wide-glyph atomicity & Unicode display integrity (`src/core/text.cpp`, `src/scene/surface.cpp`, `src/scene/painter.cpp`, `src/scene/compositor.cpp`, `src/term/presenter.cpp`, `src/term/terminal_emulator.cpp`)**: Atomically clear orphaned 2-column partner cells to `' '` on overwrite/clip across `Surface`, `Compositor`, `Presenter`, and `TerminalEmulator` (adding `wrap_pending` at right margin `width - 1`); neutralize `Cf` zero-width/BiDi controls and `U+00AD` in `sanitize_display_text` and guard `Painter::draw_text` against `w == 0` cell overwrites; fix UTS #51 `Emoji_Modifier_Base` + `U+1F3FB..U+1F3FF` (`2` cols) and `VS15`/`VS16` widths.
- **Acceptance Exit**:
  - `sizeof(Cell) == 32` verified on macOS, Linux, and Windows; zero cell-copy heap allocations in benchmarks; ASan/UBSan overflow, wide-glyph occlusion, and Unicode width/hostile-text suites green.

#### WP-87 — POSIX/Windows session safety, `<windows.h>` removal & `InputDecoder` completeness
- **Scope & Changes**:
  1. **Raster resize OOB fix (`src/term/terminal_emulator.cpp`)**: Rebuild/resize `raster_coverage_[i].live_cells` whenever `raster.cell_extent` changes in `TerminalEmulator::resize()`, and guard bounds in `damage_rasters()`.
  2. **POSIX & Windows session hardening (`src/term/posix_terminal.cpp`, `src/term/posix_terminal_subsession.cpp`, `src/term/windows_terminal.cpp`, `src/core/log_mailbox.cpp`, `src/term/log_file_service.cpp`)**: Set `FD_CLOEXEC` on self-pipes and `forkpty` `master_fd_`; close inherited FDs above `STDERR_FILENO` in child before `execve`; wrap `PosixTerminal` constructor `emit(session_enter)` in `try/catch` restoring termios, pipes, and `g_sessions[session_slot_]`; preserve `XTWINOPS 16` `cell_pixels`; consume `UnicodeChar == L'\0'` modifier records via `ReadConsoleInputW` before `ReadConsoleW` in `WindowsTerminal::poll()`; call `decoder_.set_pixel_reports_possible(false)` in `WindowsTerminal::finish_probes()`; fix `LogMailbox::cancel()` duplicate `End` and `NativeLogFileService` `Dead` -> `Retiring` TOCTOU + relative-path watching.
  3. **Remove `<windows.h>` from public `term` headers (`include/cvision/term/windows_*.hpp`)**: Replace Win32 SDK includes and `HANDLE`/`DWORD` leaks directly with clean C++ types / Pimpl handles; record header updates in `docs/migration-2.0.md`.
  4. **Complete `InputDecoder` & `KeyChord` (`src/term/input_decoder.cpp`, `src/core/key.cpp`)**: Decode modified/kitty `F1..F4` (`CSI ... P/Q/R/S`) and `Ctrl`/release on `CSI Z`; replace quadratic `pending_.erase` with `read_offset_` cursor compaction and cap `pending_`/`paste_accum_`; fix `KeyChord::parse` `"+"` / `"Ctrl++"` round-trip and reject raw control bytes.
- **Acceptance Exit**:
  - Zero `<windows.h>` macros (`min`, `max`, `near`, `far`) leaked into Windows consumers; ASan/TSan/PTY FD-leak, exception-unwind, Windows modifier-key, and `F1..F12` round-trip tests green.

#### WP-88 — Lifetime safety, focus visibility, `TextEditor` scroll/wrap & `Desktop` invariants
- **Scope & Changes**:
  1. **Popup/ComboBox UAF & Scrollbar math (`src/widgets/popup_list.cpp`, `src/widgets/combo_box.cpp`, `src/widgets/scrollbar.cpp`)**: Replace raw `View*` captures in `show_popup_list()` with `lifetime_token()` weak handles; implement `ComboBox::~ComboBox()`, `on_detaching()`, and `on_effective_visibility_changed()` to close open dropdowns; promote `Scrollbar::on_mouse` math to `std::int64_t` and fix the `span.length == track_halves - 1` dead-drag off-by-one.
  2. **`TextEditor` & `Memo` viewport repairs (`src/widgets/text_editor.cpp`, `src/widgets/memo.cpp`)**: Wire `v_scrollbar_->on_position_changed` and sync thumb position on wheel/key scroll; call `relayout_scrollbars()` on document edits; unify wrapped `cursor_state()` and `position_for_screen_cell()` on `viewport_width_`; paint bottom-left gutter and bottom-right corner cells cleanly.
  3. **Focus visibility & `Desktop`/layout invariants (`include/cvision/ui/view.hpp`, `src/ui/application.cpp`, `src/widgets/tab_control.cpp`, `src/widgets/desktop.cpp`, `src/widgets/window.cpp`, `src/scene/compositor.cpp`, `include/cvision/ui/layout.hpp`, `src/ui/anchor_pane.cpp`)**: Require `visible_in_tree()` in `View::focusable()` and evacuate/restore per-page focus on `TabControl` switches; filter `Desktop::tile*` by `tiling_participant()`, clear `Window::zoomed_` on tile/cascade/manual resize, preserve `popups_` above cascaded windows and activate top window, detect pure z-order swaps in `Compositor::compute_damage`, propagate `size_hint_changed()` through `Row`/`Column`, and fix initial `{0,0}` sizing in `AnchorPane`.
- **Acceptance Exit**:
  - ASan UAF regression tests for `PopupList`/`ComboBox`; headless interactive suites for `TextEditor` scrollbar drag/auto-appear/wrapped clicks, `TabControl` hidden-page focus isolation, and `Desktop` tile/cascade invariants green.

---

### Wave II — Visual Craft, Theme Accessibility & Next-Gen Graphics

#### WP-89 — Theme overhaul, automated WCAG 2.1 gates & surface-aware chrome
- **Scope & Changes**:
  1. **Theme & contrast overhaul (`src/ui/standard_roles.cpp`, `src/widgets/`)**: Eliminate `Attr::Reverse` from explicit color roles in `Mono`, `Dark`, and `Light`; clear inherited `Attr::Reverse` in `Progress` painting; overhaul `set_editor_roles` across all 5 themes (`Classic`, `Dark`, `Light`, `Mono`, `High Contrast`) to eliminate low-contrast pairs, pink RGB leaks in `Mono`/`Classic`, and `Attr::Dim` in `High Contrast`; add `High Contrast` to `examples/gallery`; enforce WCAG 2.1 contrast and palette purity in `tests/test_theme_contrast.cpp`.
  2. **Surface-aware shadows, carets & mixed junctions (`src/widgets/button.cpp`, `src/widgets/input_line.cpp`, `src/widgets/memo.cpp`, `src/scene/box_drawing.cpp`, `src/widgets/table.cpp`)**: Sample underlying `Surface` cell `bg` when painting `Button` 3D half-block shadows; add `ckv.input.selection` and implement `InputLine::cursor_state()` + `Memo::cursor_state()`; add single-to-double mixed junctions (`╞`, `╡`, `╤`, `╧`, `╪`) to `box_drawing.cpp`; render `" ▲"` / `" ▼"` sort chevrons (with ASCII fallback) in `Table`.
- **Acceptance Exit**:
  - `test_theme_contrast`, `check_appearance_matrix`, and `check_classic_look` 100% green across all 5 built-in themes.

#### WP-90 — Adaptive Bayer-dithered Sixel, `FunctionRef` smooth scaling & Kitty Graphics
- **Scope & Changes**:
  1. **Upgraded `SixelEncoder` (`src/term/sixel_encoder.cpp`)**: Deduplicate palette entries in `0..100` percent space; implement deterministic adaptive color quantization up to `min(max_colors, 256)` paired with stateless `(anchor_px_x, anchor_px_y)` 4x4 Bayer ordered dithering; composite RGBA alpha over resolved cell backgrounds and emit transparent holes (`P2=1`) for `a == 0`.
  2. **Filtered `ImageSampling` & `FunctionRef` (`include/cvision/core/function_ref.hpp`, `include/cvision/core/image.hpp`, `include/cvision/scene/painter.hpp`, `include/cvision/widgets/image_view.hpp`, `src/term/presenter.cpp`)**: Add `ImageSampling::{Auto, Nearest, Smooth}` (default `Auto`: integer fixed-point box-averaging downscale, bilinear upscale); replace `std::function` in `Painter::draw_image` directly with `FunctionRef` (removing `<functional>` from `painter.hpp`) and record in `docs/migration-2.0.md`.
  3. **Kitty Graphics Protocol (`include/cvision/term/capabilities.hpp`, `src/term/posix_terminal.cpp`, `src/term/windows_terminal.cpp`, `src/term/presenter.cpp`, `src/term/terminal_emulator.cpp`, `src/term/virtual_display.cpp`)**: Probe Kitty Graphics support before `DA1`, emit cropped/occluded/shadow-dimmed 32-bit RGBA placements (`\x1B_Ga=T,f=32,...`) with deterministic cleanup, decode Kitty placements in `VirtualDisplay`, and retain seamless Sixel -> textual fallback degradation.
- **Acceptance Exit**:
  - Smooth, banding-free gradients and seam-free occluded Sixel & Kitty RGBA decoded-plane goldens; headless round-trips green across `KittyGraphics`, `Sixel`, and `NoGraphics` profiles.

#### WP-91 — High-fidelity SVG/PNG DEFLATE snapshot engine & golden refresh
- **Scope & Changes**:
  1. **SVG & PNG fidelity (`src/scene/svg_snapshot.cpp`)**: Render `Attr::Dim` (`opacity="0.55"`), `Attr::Italic` (`font-style="italic"`), `Attr::Strike` (`text-decoration="line-through"`), OSC 8 `<a href="...">` anchors, optional `CursorState`, XML/CSS escaping on `options.font_family`, horizontal `<text>` run coalescing, and dependency-free fixed-Huffman DEFLATE PNG compression.
  2. **Single-pass golden & screenshot refresh (`tools/docgen/`, `tests/golden/`, `docs/generated/screenshots/`)**: Regenerate all affected scene/SVG/RGBA goldens and published documentation screenshots in one audited pass; delete obsolete `.pdf` files from `docs/generated/screenshots/`.
- **Acceptance Exit**:
  - `check_generated_goldens` green on macOS, Linux, and Windows; `docs/generated/screenshots/` shrunk by `>= 60%` with accurate dim/italic/strike rendering.

---

### Wave III — Desktop-in-a-Terminal UX Mastery & `O(damage)` Performance

#### WP-92 — Desktop-in-a-terminal UX mastery (menus, windows, `CommandPalette`)
- **Scope & Changes**:
  1. **Clean Command & StatusLine contracts (`include/cvision/ui/command.hpp`, `src/ui/command.cpp`, `include/cvision/widgets/status_line.hpp`, `src/widgets/desktop.cpp`)**: Delete the redundant legacy `StatusLineItem::command` member in favor of `CommandPresentation` (logged in `docs/migration-2.0.md`); install live `set_enabled_predicate`s on all `Desktop` standard window commands and expose active desktop verbs in `CommandPalette`; bind `Alt+0` to `standard.window_list`; add secondary-chord support in `CommandRegistry` so `standard.command_palette` answers both `Ctrl+Shift+P` and portable `F12`.
  2. **Zero-boilerplate `ApplicationShell` & Window Menu (`include/cvision/widgets/application_shell.hpp`, `src/widgets/application_shell.cpp`, `include/cvision/widgets/menu.hpp`)**: Auto-wire `menu_bar_->on_highlight_changed` -> `status_line_->set_transient_hint` (formatting command hints and `disabled_reason` explanations automatically) in `ApplicationShell`; ship `make_standard_window_menu(CommandRegistry&)`.
  3. **Window mouse & control-menu polish (`include/cvision/widgets/window.hpp`, `src/widgets/window.cpp`)**: Gate `Window` chrome clicks and move/resize drags to `MouseButton::Left`; toggle `Zoom` on left double-click on resizable title bars; add a Window Control Menu (`Alt+-` and title-bar right-click: Restore/Zoom, Minimize, Size/Move, Close).
  4. **2026-grade `CommandPalette` (`src/widgets/common_components.cpp`)**: Multi-token + subsequence scoring across `category` and `title` with per-instance MRU boost, dimmed category prefixes, right-aligned chord column, `PageUp`/`PageDown`/`Home`/`End`/`Ctrl+U`/`Ctrl+W`, scrollbar mouse drag, and paste newline stripping.
- **Acceptance Exit**:
  - End-to-end UX tests verifying `Alt+0`, `F12` on legacy terminals, greyed window actions on an empty desktop, automatic disabled-reason hints in `StatusLine`, title-bar double-click zoom, `Alt+-` / right-click Window Control Menu, and ranked `CommandPalette` queries.

#### WP-93 — `O(line)` incremental `TextEditor` & syntax lexer state machines
- **Scope & Changes**:
  1. **Incremental `TextEditor` (`src/widgets/text_editor.cpp`, `src/widgets/dialog.cpp`)**: Eliminate `document_->text()` full-file copies on edits, arrow keys, `Backspace`, and `on_resized()`, updating lines incrementally from `first_dirty_line`; bound `previous_grapheme`/`previous_word` to `lines_[line].text`; binary-search `search_matches_` per row via `std::partition_point`; replace `wrap_text` vector allocation in `Dialog`'s `FieldDescriptionView::draw` with `next_text_row`.
  2. **Syntax lexers (`src/widgets/syntax_profile.cpp`)**: Fix `yaml_line` string-aware `#`/`:` handling and strict content detection; fix `bash_line` `${...#...}`, `$#`, `\"` escapes, and quoted keyword/heredoc suppression; add `.jsonc` (`//`, `/* */`) comment states in `json_line`.
- **Acceptance Exit**:
  - Benchmark gate proving `< 50 us` edit/navigation latency on a 100,000-line / 5 MiB `EditorDocument` with active search matches; syntax unit & fuzz suites green.

#### WP-94 — `O(damage)` `Presenter`, delta-SGR, `DECSTBM` scroll-intent & Sixel LRU
- **Scope & Changes**:
  1. **Damage-scoped `Presenter` & delta-SGR (`src/term/presenter.cpp`)**: Restrict `build_presentation` cell inspection/copy to `Compositor` damage spans; pre-reserve synchronized-output framing without `out.insert(0, ...)` `memmove`; share `LinkTable` by immutable generation/pointer; emit allocation-free SGR deltas via `std::to_chars`, degrade underline colors per `ColorDepth`, and narrow width-unsafe gating to ambiguous/emoji codepoints.
  2. **Sixel slice LRU & `DECSTBM` hardware scrolling (`src/term/presenter.cpp`, `src/scene/compositor.cpp`, `src/widgets/scroll_viewport.cpp`, `src/widgets/text_editor.cpp`, `src/widgets/log_view.cpp`)**: Key Sixel cache entries on `O(1)` `Image` generation IDs inside a bounded (8 MiB) LRU slice cache; emit `DECSTBM` + `CSI S`/`CSI T` for unoccluded full-width vertical scrolls and paint only newly exposed rows.
- **Acceptance Exit**:
  - Benchmark gates proving `< 256` bytes and zero heap allocations for a 1-cell cursor blink, `>= 85%` byte reduction on 1-row `TextEditor`/`LogView` scrolls on 200x60 grids, and byte-exact `VirtualDisplay` round-trip verification.

---

### Wave IV — Pure SDK Architecture, Unified Localization & `2.0` Closure

#### WP-95 — Data views, unified `ListItem`, `FileDialog`, `Table` & `TerminalView` UX
- **Scope & Changes**:
  1. **Clean `ListItem` & data-view mouse model (`include/cvision/widgets/list_view.hpp`, `src/widgets/list_view.cpp`, `src/widgets/tree_view.cpp`)**: Replace the error-prone dual `ListItem::text` + `borrowed_text` split directly with a single unified string/borrowed representation so `caption()` is the sole text path everywhere (recorded in `docs/migration-2.0.md`); restrict click-selection mutation in `ListView` and `TreeView` to `MouseButton::Left` while preserving multi-selection on `MouseButton::Right` context-menu clicks.
  2. **Clean `DirectoryPicker` & `FileDialog` (`include/cvision/widgets/directory_picker.hpp`, `src/widgets/file_dialog.cpp`, `src/core/filesystem.cpp`)**: Remove the legacy eager-tree `FileSystem`-only `DirectoryPicker` overloads and unify `make_directory_picker` callback type on `DirectoryPickerResult` (recorded in `docs/migration-2.0.md`); preserve user-typed filenames in `FileDialog` across filter/hidden toggles; fix Windows drive-root `".."` and `MemoryFileSystem` `"C:"` normalization.
  3. **`Table` & `TerminalView` (`include/cvision/widgets/table.hpp`, `src/widgets/table.cpp`, `src/widgets/terminal_view.cpp`)**: Add non-zero preferred size hints, natural numeric/typed column sorting, single-grapheme forward `Delete` in inline cell editing, clean commit on focus loss (`Tab`), and header-divider drag column resizing in `Table`; add transient diagnostic banner dismissal/auto-clear and scrollback-anchored text selection in `TerminalView`.
- **Acceptance Exit**:
  - Unit and golden suites covering all data-view, path-dialog, table-edit/sort/resize, and terminal-selection workflows across POSIX and Windows fixtures.

#### WP-96 — Pure modular SDK headers, `app.strings()`, compile-safe layouts & `2.0` docs closure
- **Scope & Changes**:
  1. **Delete `common_components.hpp` & hide internal headers (`include/cvision/`, `src/`, `CMakeLists.txt`)**: Delete `common_components.hpp` outright (no umbrella shim) and replace it with 9 focused headers (`date_time.hpp`, `spin_box.hpp`, `slider.hpp`, `search_box.hpp`, `tool_bar.hpp`, `command_palette.hpp`, `property_inspector.hpp`, `wizard.hpp`, `tooltip.hpp`); relocate all 11 `*internal*.hpp`, `layout_metrics.hpp`, and `generated_unicode_15_1.hpp` files from `include/cvision/` into `src/`.
  2. **Compile-safe layout containers & 1-screen ergonomics (`include/cvision/ui/`, `include/cvision/widgets/`, `examples/`)**: Make raw `View::add_child`/`add`/`make` `protected` on `Row`, `Column`, `Grid`, `Dock`, `AnchorPane`, and `Overlay` so bypassing layout is a compile-time error; provide bounds-free default constructors (`Desktop()`, `Splitter(first, second, orientation)`, etc.) and `Window::set_detach_on_close(bool)`; update all `examples/` to clean 2.0 idioms (`ApplicationShell`, `make_standard_window_menu()`, `set_detach_on_close(true)`, `EditorWindow`).
  3. **`Application`-owned `UiStrings` & simplified dialog factories (`include/cvision/ui/application.hpp`, `include/cvision/widgets/`)**: Collapse `StandardStrings`, `DateTimeLabels`, `PropertyInspectorMessages`, `WizardLabels`, and all ~80 remaining hardcoded UI literals into one unified `UiStrings` owned on `Application` (`app.strings()`, alongside cached `app.standard_roles()`), **deleting the redundant `roles` and `strings` parameters from every standard dialog factory** and unifying all modal/modeless factories onto `(app, desktop, options_or_descriptor, ...)`! Enforce string hygiene with a CTest check rejecting hardcoded UI literals in `src/ui` and `src/widgets`.
  4. **Authority, Documentation & `docs/migration-2.0.md` Closure**: Bump `CMakeLists.txt` version to `2.0.0`; reconcile `ARCHITECTURE.md`, `ROADMAP.md`, `DECISIONS.md`, `AGENTS.md`, `README.md`, and `docs/*.md` (fixing broken tables, headings, standalone `getting-started.md` snippet, widget ordering, and host-path hygiene); finalize `docs/migration-2.0.md` and `CHANGES.md` with complete, mechanical Before -> After instructions for upgrading client applications (such as `ckUtilities`).
- **Acceptance Exit**:
  - Installed-SDK consumer tests, non-English `app.set_strings(...)` smoke test, standalone documentation snippet compilation, `provenance_check`, `check_license_headers`, `check_example_hygiene`, and full three-platform Release + Sanitizer + Fuzz matrices 100% green.
