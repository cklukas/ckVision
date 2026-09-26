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
| A1 | `CalendarView::date_at_cell` hard-codes the row range and ignores `grid_top()`, so a `CalendarDropdown`'s first week cannot be clicked | `src/widgets/common_components.cpp` | Fixed: rows are counted from `grid_top()` |
| A2 | Backspace in `SearchBox` and `CommandPalette` removes one byte, splitting UTF-8 | same | Fixed: both remove the last grapheme cluster |
| A3 | `PropertyInspector::set_items` leaves editing on; Backspace ignores `editable` | same | Fixed: `set_items` ends the edit; Backspace and typing need an editable row |
| A4 | Every `DatePicker` value becomes the calendar seed, so "today" follows the last value | same | Fixed: `set_value` leaves the seed alone |
| A5 | Possible int overflow in SpinBox/Slider stepping and `Slider::value_from_x` | same | Fixed: stepping and the value↔column mapping are computed in 64 bits |
| A6 | `EditorDocument::commit_edits` sorts edits unstably by begin byte, so edits at one position apply in unspecified order | `src/widgets/editor_document.cpp` | Fixed: a stable document-order sort; insertions at one position land in queue order, ahead of a replacement that starts there |
| A7 | A leading BOM is stripped from every edit's text, not only from `set_text` | same | Fixed: only `set_text` records a BOM; an edit keeps U+FEFF as text |
| A8 | Double-clicking a word's first character also selects the previous word | `src/widgets/text_editor.cpp` | Fixed: double-click selects from the word start |
| A9 | Read-only does not block undo/redo | same | Fixed: a read-only or disabled editor refuses undo and redo |
| A10 | Replace-current can use a stale match index; replacements bypass the edit handler and enabled check; replace-all on a read-only editor returns Ok | same | Fixed: the current match is a revision-bound range; both replace verbs go through the edit handler as one request carrying all its edits; `replace_all_search_matches` returns `bool` |
| A11 | `clamp_cursor` can jump the caret to the end of the document | same | Fixed: the caret snaps to the start of the grapheme cluster it landed in |
| A12 | `TextEditor` keeps a `LanguageProfile*` a later `register_profile` can leave dangling | same | Fixed: the editor keeps its own copy of the profile |
| A13 | `mark_clean` sends no status notification | same | Fixed: `EditorDocument::subscribe_state` reports clean, newline and BOM changes, and the editor republishes its status |
| A14 | `EditorWindow::request_close(Discard)` still leaves `close()` vetoed | `src/widgets/editor_window.cpp` | Fixed: a close is allowed while the document stays at the revision Discard answered |
| A15 | The syntax cache's fixed-point shortcut leaves stale or unlexed lines when one update carries several non-adjacent edits (confirmed by a probe) | `src/widgets/syntax_cache.cpp` | Fixed: every changed line is relexed before a fixed point is accepted, and an interrupted pass remembers how far it must go |
| A16 | A styled Table cell or ListView row replaces the cursor and selection highlight | `table.cpp`, `list_view.cpp` | Fixed: the cursor and selection are drawn over a styled cell or row by the shared D-067 highlight rule |
| A17 | A failed Enter commit in Table is left unconsumed | `src/widgets/table.cpp` | Fixed: Enter while editing is always consumed |
| A18 | `Table::set_columns` does not clamp `sort_column_`, which can index out of range | same | Fixed: a sort column the new columns lack is dropped |
| A19 | Any mouse button activates Table, Button, KeyChordCapture, and Scrollbar | several | Fixed: only the primary button activates them |
| A20 | `Table::cancel_edit` does not repaint | `src/widgets/table.cpp` | Fixed: repaints when an edit was open |
| A21 | `Table::sort_by` reports only when a cell is selected | same | Fixed: `on_sort_requested` takes `std::optional<TableCellRef>` and fires on every model sort |
| A22 | `ComboBox::set_items` leaves the editor text stale; `open_dropdown` selects index 0 without changing the text | `src/widgets/combo_box.cpp` | Fixed: `set_items` updates the editor; opening the list moves only its cursor |
| A23 | FileDialog and HelpViewer kept the caller's `strings` reference past the call, so a temporary dangled | `file_dialog.cpp`, `help_viewer.cpp` | Fixed: both copy the labels they keep; the contract now requires `strings` only for the call |
| A24 | TabControl always consumes Tab and Shift+Tab, so focus cannot leave it, and Shift+Tab steps forward | `src/widgets/tab_control.cpp` | Fixed: Tab and Shift+Tab are left to focus traversal; Left/Right and Alt+mnemonic switch tabs |
| A25 | `ScrollViewport::set_content` does not reset the scrollbar positions | `src/widgets/scroll_viewport.cpp` | Fixed: both scrollbars return to the origin |
| A26 | `Button::set_default`, the inline `set_role_override` setters, and `StaticText::set_preformatted` neither repaint nor report a size change | headers | Fixed: each setter repaints when its value changes; `set_preformatted` also reports a size change |
| A27 | TextView link hit-testing ignores horizontal scroll | `src/widgets/text_view.cpp` | Fixed: hit-testing adds the horizontal scroll and ignores clicks past the viewport |
| A28 | Window horizontal size hints ignore asymmetric content margins | `src/widgets/window.cpp` | Fixed: hints budget both margins; `left_content_margin()`/`right_content_margin()` replace the unused symmetric accessors |
| A29 | Memo Backspace/Delete on an empty selection records an empty undo step | `src/widgets/memo.cpp` | Fixed: an empty selection is dropped before the edit, so it records one undo step |
| A30 | Row/Column/Grid/Dock/Overlay/AnchorPane keep stale `specs_` after `remove_child` (Dock can then assert) and store `specs_[nullptr]` when attachment self-detaches | `src/ui/layout.cpp`, `src/ui/dock.cpp` | Fixed: every container forgets a child's spec on `remove_child` by any route and never records one for a child that detached itself; the duplicate `remove_item` is removed (D-079) |
| A31 | Row/Column shadow checks count hidden children; only Row/Column skip hidden children in layout; no container reads `SizeHint::max` | `src/ui/` layout containers | Fixed (D-079): Row and Column cap an Expanding child's share of spare space at its max; hidden children take no space and no longer decide the shadow row |
| A32 | `base64::decode` accepts non-zero padding bits | `src/core/base64.cpp` | Fixed: rejected per RFC 4648 §3.5 |
| A33 | `MemoryFileSystem::create_directories` checks only the last component | `src/core/filesystem.cpp` | Fixed: every component along the path is checked |
| A34 | `join("/", …)` skips normalisation, contradicting "never has //" | same | Fixed: `join` always normalises |
| A35 | `PosixFileSystem::create_directories("a/b")` resolves to `/a/b` | `src/term/posix_filesystem.cpp` | Fixed: a relative path resolves against the working directory |
| A36 | An overlong BEL-terminated OSC or CSI swallows child output until an `ESC \` arrives | `src/term/terminal_emulator.cpp` | Fixed: a dropped CSI ends at its final byte, an OSC at BEL or ST, a DCS at ST only |
| A37 | `damage_.rasters` is not set on raster erase or scroll | same | Fixed: set on every change to the picture list |
| A38 | `RecordingTerminal` does not forward `frame_acknowledgements()` or the `poll` overload that takes wait handles | `src/term/record_replay_terminal.cpp` | Fixed: forwarded, and each recorded poll stores the acknowledgements replay reports back |
| A39 | A second `CK_EXPECT_ABORT` in one test case cannot fail | `include/cvision/testing/cktest.hpp` | Fixed: each expectation runs in its own child; `tests/cktest_expect_abort_contract.cpp` proves a second one can fail |
| A40 | The title sanitiser may not strip a raw C1 ST (0x9C); unverified | `src/term/presenter.cpp` | Confirmed and fixed: the sanitiser decodes UTF-8 and drops every C0/C1 control and DEL |
| A41a | Dead public API: `TimerEvent`, which nothing produces or consumes and no document names | `core/event.hpp` | Fixed: removed |
| A41b | `ckv::Error`/`ErrorCode` are returned by nothing, while D-011 and ARCHITECTURE §11 say environmental failures surface as `ckv::Error`; in fact backends throw `std::runtime_error` and file operations return their own status enums | `core/error.hpp`, D-011 | Fixed (D-078): `ckv::Error` withdrawn; D-011 and ARCHITECTURE §11 now state the error model the code implements |
| A41c | `RasterRegion::fallback_active` is always true outside tests; the Presenter never reads it, yet every golden raster line records it | `scene/surface.hpp`, golden format | Fixed (D-080): removed from the scene, the frame view, and the golden `raster` line; the golden parser rejects the old token |
| A41d | `PixelSize` is unused because pixel extents are typed as cell-space `Size` across the API (`Capabilities::cell_pixels`, Sixel geometry, image and canvas pixel sizes; 24 files), contrary to ARCHITECTURE §2 and D-018: "distinct types — the two coordinate spaces never mix silently" | `core/geometry.hpp`, `term/capabilities.hpp`, widgets | Open: retype every pixel extent as `PixelSize` |
| A42 | `graphics_log.cpp` keeps process-global mutable state with no DECISIONS entry (AGENTS.md sanctions only the D-024 session registry), and reads, writes and sets environment variables — a D-039 violation | `src/term/graphics_log.cpp` | Fixed (D-077): an injected `GraphicsTrace`, `term::FileTraceSink`, and the examples' own switch |
| A43 | The TextEvent "IME/dead-key" wording is kept, although the decoder produces TextEvents only for paste | `include/cvision/core/event.hpp` | Fixed: the event comment, docs/input-decoder.md and ARCHITECTURE §4 now say composed text arrives as unmodified `Key::Char` events that editable controls insert, and only paste is decoded as a `TextEvent` |
| A44 | `PosixTerminal` read `CKVISION_OUTPUT_CAPTURE` itself, never closed the file, and missed titles, bells and clipboard writes despite claiming every byte | `src/term/posix_terminal.cpp` | Fixed (D-077): `set_output_capture` receives every byte sent after it is set |
| A45 | `PosixTerminal` dropped output silently once the terminal was gone, while the Windows backend threw, so a POSIX program kept painting into nothing | `src/term/posix_terminal.cpp` | Fixed (D-078): a failed write ends the session with `std::system_error`; restoring stays best-effort; the examples report the failure after restoring the terminal |
| A46 | Undo and redo in `TextEditor` moved the caret to the end of the document, because undo reported a whole-document change | `src/widgets/editor_document.cpp`, `text_editor.cpp` | Fixed: undo and redo report the span they change and restore the selection; `undo()`/`redo()` return `std::optional<DocumentChange>` |
| A47 | Suspected: a budgeted syntax-cache pass interrupted by an insertion before its resume point could hide a stale line | `src/widgets/syntax_cache.cpp` | Closed, not a defect: the cache keys its resume and settle lines by position and re-checks every line whose text changed, so an insertion cannot hide a stale line; a 4,000-step randomized test comparing each completed pass with a full lexing pins it |
| A48 | `CalendarView` asked for 8 rows even with its title off | `src/widgets/common_components.cpp` | Fixed: the height hint follows what the calendar draws |
| A49 | `Table::set_columns` did not check the column count against rows supplied by `set_rows` | `src/widgets/table.cpp` | Fixed: columns that do not fit materialized rows are a contract violation, as in `set_rows` |
| A50 | With ISO week numbers on, `CalendarView`'s minimum width stayed 21 although the grid needs 23 | `src/widgets/common_components.cpp` | Fixed: the width hint counts the week-number column |
