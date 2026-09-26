<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# WP-38 appearance matrix

WP-38 requires golden coverage that is "mechanically enumerable and complete
against the widget catalog/state/scheme matrix". This document defines that
matrix, how it is generated, and what the gate proves. The
[editor/terminal appearance audit](wp38-editor-terminal-appearance.md) remains
the record of the app-level editor and contained-terminal slices, which have
their own generators.

## Dimensions

| Dimension | Values | Source |
|---|---|---|
| Element | Every public view type in `include/cvision/widgets/`, every standard dialog, and the `Application` shell | `tools/docgen/appearance_*.cpp` |
| State | The states each element's traits require, plus element-specific states | Same files; required set below |
| Scheme | classic, dark, light, mono | `make_{classic,dark,light,mono}_theme` |
| Graphics profile | Sixel and no-graphics for raster-bearing elements | `headless_sixel_profile()`, `headless_no_graphics_profile()` |

Wide text and terminal shrink are states, not a separate axis. Every
text-bearing element has a `wide` state (CJK, an emoji presentation sequence,
and a combining mark) and a `narrow` state (less width than its text needs).
`Application/too-small` is the whole-shell response to a terminal below its
hard floor.

## Traits and required states

| Trait | Meaning | Required states | Required differences, in every scheme |
|---|---|---|---|
| (any) | — | `normal` | the four schemes of each state are pairwise different, unless the catalog declares the state fixed with a reason, in which case they must be identical |
| `focusable` | takes the keyboard | `focused` | `focused` differs from `normal` |
| `control` | sets a value of the reader's (D-076) | `focused`, `disabled` | `disabled` differs from `normal` |
| `text` | shows caller text | `wide`, `narrow` | — |
| `window` | window chrome | `active`, `inactive` | — |
| `raster` | draws pictures | `sixel`, `no-graphics`, and a symbolic scene dump for every state | `sixel` decodes pixels identical across schemes; `no-graphics` leaves the plane empty; the `sixel` scene records a raster and equals the `no-graphics` scene |

The difference rules are what make a state's name a claim about its picture.
A `disabled` state that renders exactly like `normal` fails the gate, as does a
`focused` state no reader could see. The rules found the defect D-076 repaired
before the matrix was committed.

## Outputs

`generate_appearance_matrix <dir>` writes:

- `catalog.txt`: the schemes, then one `element` line (name, header, traits),
  its `state` lines (name, screen size, graphics profile), and every `exempt`
  line (type, reason);
- `<Element>/<state>.<scheme>.dump`: the terminal's visible cells after the
  presenter, in the [golden format](../docs/golden-format.md);
- `<Element>/<state>.<scheme>.scene`: for every state of a raster element, the
  composed frame before the presenter, captured with
  `scene::capture_frame(app.compositor(), app.current_cursor())` so that it
  carries a `raster` record for each visible, occlusion-sliced picture (the
  symbolic oracle of the golden format's
  [raster records](../docs/golden-format.md#raster-records-versus-rendered-graphics-captures));
- `<Element>/<state>.rgba`: the decoded Sixel plane of a raster state, written
  once because the generator fails unless all four schemes decoded the same
  plane.

The fixtures are pinned under `tests/golden/appearance/`. To regenerate after
an intended visual change, run the generator into that directory, review the
diff, and commit it with the change that caused it.

## Gate

The `appearance_matrix` CTest (`tests/check_appearance_matrix.py`, labels
`golden;visual;symbolic-scene;portable;script`) regenerates the matrix into
build-owned scratch and fails unless:

1. the generated tree equals the pinned tree, file set and bytes;
2. every public view type (the transitive `View` subclasses in
   `include/cvision/widgets/`, the same enumeration the documentation checker
   uses) is an element or an exemption with a stated reason, and no type is
   both;
3. every element has every state its traits require, in all four schemes, with
   a decoded plane for each Sixel state;
4. the difference rules above hold;
5. every state of a raster element has a symbolic scene dump in all four
   schemes, no other element has one, and in every scheme the `sixel` scene
   records at least one raster and is byte-identical to the `no-graphics`
   scene. The same script composes the same scene under either profile; only
   the presenter decides whether pixels cover the fallback (D-080).

It prints the element, state, file, and exemption counts and a manifest
SHA-256 so hosts can be compared by one line.

## Current record — 2026-09-26

59 elements, 388 states × 4 schemes, 1,559 pinned files (1,553 cell dumps,
six decoded Sixel planes, and the catalog), no exemptions. One Too-small
state is declared fixed, with its reason. One archive of the source tree
passed strict Release suites with zero warnings: macOS ARM64 179/179, Linux
ARM64 179/179, Windows ARM64 166/166. `appearance_matrix` regenerated the same
manifest, SHA-256
`a7b48279b05e9b4b95cfa436c3df5466870fc2b8668d68dbfe17e9228bbd4619`, on all
three hosts.

The symbolic raster scenes then added 92 files: the 23 states of ImageView (4),
Canvas (4), FlowView (7) and TerminalView (8), in four schemes each. The tree
is now 1,651 pinned files (1,553 cell dumps, 92 symbolic scenes, six decoded
Sixel planes, and the catalog), manifest SHA-256
`fe005c36566b22211c56c875b3fe7babeef603310c4642c53dc90368b1a4e692` on macOS
ARM64. The three-host record for this manifest is taken with the change that
lands it.

## Findings

Building the matrix exposed the defects below. Each is a library fix with
its own regression test, landed before the goldens that show it are pinned;
none is recorded in the matrix as accepted behaviour.

| # | Finding | Where | Status |
|---|---|---|---|
| F1 | Disabled controls drew exactly like enabled ones, and a disabled button still fired on click | View/Application routing, every control | Fixed (D-076) |
| F2 | `ckv.input.focused` equalled `ckv.input.normal` in Classic, Dark, and Light; fields without a caret (pick-only ComboBox, KeyChordCapture, TimePicker, SpinBox) showed no focus | `src/ui/standard_roles.cpp` | Fixed |
| F3 | Progress drew its label in the track style, erasing the fill beneath it | `src/widgets/progress.cpp` | Fixed |
| F4 | ScrollViewport let content show in the corner where two bars meet | `src/widgets/scroll_viewport.cpp` | Fixed |
| F5 | TabControl parsed tab mnemonics but never drew their accent | `src/widgets/tab_control.cpp` | Fixed |
| F6 | CalendarView's `serial()` counted a leap year's leap day from January 1, so every leap year was drawn one weekday late | `src/widgets/common_components.cpp` | Fixed |
| F7 | CalendarView drew the "Wk" heading but no ISO week numbers | same | Fixed |
| F8 | TimePicker never marked its active field, and changing field did not repaint | same | Fixed |
| F9 | No focus cue: CalendarView, PropertyInspector, TreeView, Table (selection ignored focus); BigClockView, CommandPalette, Wizard, CellGrid, TextView, FlowView | several | Fixed |
| F10 | PropertyInspector: no editing mark, typed characters ignored, selection bar shorter than the row | `common_components.cpp` | Fixed |
| F11 | ToolBar printed raw `&` mnemonic markers | same | Fixed |
| F12 | Wizard labelled a blocked first page "Finish" | same | Fixed |
| F13 | SpinBox clipped a wide value with no mark, showing a wrong number | same | Fixed |
| F14 | BreadcrumbBar has a keyboard walk and a focused face but never takes focus | same | Fixed |
| F15 | SearchBox does not scroll a query longer than its field | same | Fixed |
| F16 | A narrow MenuBar lets its trailing accessory cover a title | `src/widgets/menu.cpp` | Fixed |
| F17 | TreeView's box-drawing connectors ignore sibling position and draw no ancestry lines | `src/widgets/tree_view.cpp` | Fixed |
| F18 | FlowView wraps inside words | `src/widgets/flow_view.cpp` | Fixed |
| F19 | A multi-select ListView draws its cursor row exactly like its selected rows | `src/widgets/list_view.cpp` | Fixed |
| F20 | Table in-cell editing shows no caret or editing mark | `src/widgets/table.cpp` | Fixed |
| F21 | A stale Scrollbar comment described a "dark-shade run" the code does not draw | `src/widgets/scrollbar.cpp` | Fixed |
| F22 | No focus cue: MinimizedWindowStub, NotificationCenter; NotificationCenter severities differ only by prefix | `minimized_window_stub.cpp`, `common_components.cpp` | Fixed |
| F23 | The too-small screen ignores the scheme (fixed white on red) and, below 20 columns, clips away the size it asks for | `src/ui/application.cpp` | Fixed |
| F24 | A highlighted unavailable dropdown row draws exactly like an unhighlighted one | `src/widgets/menu.cpp` | Fixed |
| F25 | A window title's padding overwrites a frame control's bracket at small widths | `src/widgets/window.cpp` | Fixed |
| F26 | DirectoryPicker at its default size gives its tree no rows | `src/widgets/directory_picker.cpp` | Fixed |
| F27 | HelpViewer at 30 columns leaves its topic pane zero width | `src/widgets/help_viewer.cpp` | Fixed |
| F28 | FileDialog on a 48-column terminal cannot show all its buttons | `src/widgets/file_dialog.cpp` | Accepted: below the 80×24 full-chrome minimum (ARCHITECTURE §5 sizing policy) chrome degrades; Enter and Escape still commit and cancel |
| F29 | A dropdown wider than the screen is clipped without its border or an elision mark | `src/widgets/menu.cpp` | Fixed |
| F30 | FrameText, Tooltip, and NotificationCenter cut text mid-cluster without the `…` window titles use | several | Fixed |
| F31 | Two gallery scenes misuse the API: a status item with literal `~` markers, a tooltip placed under the windows | `tools/docgen/widget_shots_chrome.cpp` | Fixed |
| F32 | The Windows ConPTY resize-storm case timed out once under a parallel suite load: it sampled the screen once after the storm, while ConPTY was still repainting | `tests/test_windows_terminal_subsession.cpp` | Fixed: the test waits for the settled screen |
| F33 | DirectoryPicker, once its tree had a size, opened one row tall; TreeView ignored an owner's preferred size | `directory_picker.cpp`, `tree_view.cpp` | Fixed |
| F34 | A tooltip over a dialog wore the dialog's own surface and could not be told from it | `ckv.tooltip` role, `common_components.cpp` | Fixed |
| F35 | POSIX PTY tests of abruptly dying children lost output under load: the child's exit was the last slave close, and the hang-up discarded unread output | `tests/test_posix_terminal.cpp` | Fixed: the parent holds the slave for the child's life |
