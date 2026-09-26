<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# WP-38 API reference review findings

WP-38 requires reference documentation for every public API. Writing it
meant reading the implementation behind every public declaration in the 124
installed headers. Each comment states the code's actual behaviour. The
review raised the suspected defects below. They were not fixed in the
documentation change; each is either fixed with a regression test or closed
with a reason before WP-38 can close. Line numbers refer to the tree at the
documentation commit.

## Documentation coverage

A libclang-based scan counts a public declaration as documented when a `//`
comment sits immediately above it, or above a run of consecutive related
declarations with no blank line between them. An override is documented by
its base unless it narrows the base contract.

| Layer | Undocumented before | After |
|---|---:|---:|
| core | 337 | 0 |
| scene | 41 | 0 |
| term | 122 | 0 |
| testing | 21 | 0 |
| ui | 180 | 0 |
| widgets | 905 | 0 |
| **Total** | **1,606** | **0** |

Of the 516 public overrides, the 134 that narrow their base contract are
documented; the other 209 without a comment of their own keep it. The
change was verified comment-only. Stripping blank lines and comments leaves
every header byte-identical, apart from three trailing comments that were
corrected. About 35 existing comments were found wrong and corrected. A
mechanical gate that keeps coverage at zero is WP-38 work still Open.

## Suspected defects

| # | Finding | Where | Status |
|---|---|---|---|
| A1 | `CalendarView::date_at_cell` hard-codes the row range and ignores `grid_top()`, so a `CalendarDropdown`'s first week cannot be clicked | `src/widgets/common_components.cpp` | Open |
| A2 | Backspace in `SearchBox` and `CommandPalette` removes one byte, splitting UTF-8 | same | Open |
| A3 | `PropertyInspector::set_items` leaves editing on; Backspace ignores `editable` | same | Open |
| A4 | Every `DatePicker` value becomes the calendar seed, so "today" follows the last value | same | Open |
| A5 | Possible int overflow in SpinBox/Slider stepping and `Slider::value_from_x` | same | Open |
| A6 | `EditorDocument::commit_edits` sorts edits unstably by begin byte, so edits at one position apply in unspecified order | `src/widgets/editor_document.cpp` | Open |
| A7 | A leading BOM is stripped from every edit's text, not only from `set_text` | same | Open |
| A8 | Double-clicking a word's first character also selects the previous word | `src/widgets/text_editor.cpp` | Open |
| A9 | Read-only does not block undo/redo | same | Open |
| A10 | Replace-current can use a stale match index; replacements bypass the edit handler and enabled check; replace-all on a read-only editor returns Ok | same | Open |
| A11 | `clamp_cursor` can jump the caret to the end of the document | same | Open |
| A12 | `TextEditor` keeps a `LanguageProfile*` a later `register_profile` can leave dangling | same | Open |
| A13 | `mark_clean` sends no status notification | same | Open |
| A14 | `EditorWindow::request_close(Discard)` still leaves `close()` vetoed | `src/widgets/editor_window.cpp` | Open |
| A15 | The syntax cache's fixed-point shortcut leaves stale or unlexed lines when one update carries several non-adjacent edits (confirmed by a probe) | `src/widgets/syntax_cache.cpp` | Open |
| A16 | A styled Table cell or ListView row replaces the cursor and selection highlight | `table.cpp`, `list_view.cpp` | Open |
| A17 | A failed Enter commit in Table is left unconsumed | `src/widgets/table.cpp` | Open |
| A18 | `Table::set_columns` does not clamp `sort_column_`, which can index out of range | same | Open |
| A19 | Any mouse button activates Table, Button, KeyChordCapture, and Scrollbar | several | Open |
| A20 | `Table::cancel_edit` does not repaint | `src/widgets/table.cpp` | Open |
| A21 | `Table::sort_by` reports only when a cell is selected | same | Open |
| A22 | `ComboBox::set_items` leaves the editor text stale; `open_dropdown` selects index 0 without changing the text | `src/widgets/combo_box.cpp` | Open |
| A23 | FileDialog and HelpViewer keep the caller's `strings` reference past the call (now documented as a lifetime contract; decide whether to copy) | `file_dialog.cpp`, `help_viewer.cpp` | Open |
| A24 | TabControl always consumes Tab and Shift+Tab, so focus cannot leave it, and Shift+Tab steps forward | `src/widgets/tab_control.cpp` | Open |
| A25 | `ScrollViewport::set_content` does not reset the scrollbar positions | `src/widgets/scroll_viewport.cpp` | Open |
| A26 | `Button::set_default`, the inline `set_role_override` setters, and `StaticText::set_preformatted` neither repaint nor report a size change | headers | Open |
| A27 | TextView link hit-testing ignores horizontal scroll | `src/widgets/text_view.cpp` | Open |
| A28 | Window horizontal size hints ignore asymmetric content margins | `src/widgets/window.cpp` | Open |
| A29 | Memo Backspace/Delete on an empty selection records an empty undo step | `src/widgets/memo.cpp` | Open |
| A30 | Row/Column/Grid/Dock/Overlay/AnchorPane keep stale `specs_` after `remove_child` (Dock can then assert) and store `specs_[nullptr]` when attachment self-detaches | `src/ui/layout.cpp`, `src/ui/dock.cpp` | Open |
| A31 | Row/Column shadow checks count hidden children; only Row/Column skip hidden children in layout; no container reads `SizeHint::max` | `src/ui/` layout containers | Open |
| A32 | `base64::decode` accepts non-zero padding bits | `src/core/base64.cpp` | Open |
| A33 | `MemoryFileSystem::create_directories` checks only the last component | `src/core/filesystem.cpp` | Open |
| A34 | `join("/", …)` skips normalisation, contradicting "never has //" | same | Open |
| A35 | `PosixFileSystem::create_directories("a/b")` resolves to `/a/b` | `src/term/posix_filesystem.cpp` | Open |
| A36 | An overlong BEL-terminated OSC or CSI swallows child output until an `ESC \` arrives | `src/term/terminal_emulator.cpp` | Open |
| A37 | `damage_.rasters` is not set on raster erase or scroll | same | Open |
| A38 | `RecordingTerminal` does not forward `frame_acknowledgements()` or the `poll` overload that takes wait handles | `src/term/record_replay_terminal.cpp` | Open |
| A39 | A second `CK_EXPECT_ABORT` in one test case cannot fail | `include/cvision/testing/cktest.hpp` | Open |
| A40 | The title sanitiser may not strip a raw C1 ST (0x9C); unverified | `src/term/presenter.cpp` | Open |
| A41 | Dead or unused public API: `TimerEvent`, `PixelSize`, `ckv::Error`/`ErrorCode` (nothing returns them), `RasterRegion::fallback_active` (nothing clears or reads it) | several | Open |
| A42 | `graphics_log.cpp` keeps process-global mutable state with no DECISIONS entry (AGENTS.md sanctions only the D-024 session registry) | `src/term/graphics_log.cpp` | Open |
| A43 | The TextEvent "IME/dead-key" wording is kept, although the decoder produces TextEvents only for paste | `include/cvision/core/event.hpp` | Open |
