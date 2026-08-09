# ckVision — Widget and Chrome Catalog

The binding inventory for ROADMAP M5/M6. "Baseline" is the classic
desktop-framework capability floor — everything a first-class 1990s
windowed TUI offered. "Beyond" is where ckVision must exceed it. Every
widget: keyboard-complete, mouse-complete, theme-role documented, golden
appearance tests, scripted interaction tests, reference docs.

Current implementation status is owned by WP-34A…WP-34C and WP-38 in
[corrective-assurance.md](corrective-assurance.md). The catalog rows stay
aspirationally complete. A feature is complete only when the final traceability
matrix links its implementation, interaction script, appearance goldens, and
reference documentation. The earlier narrative ledger was incomplete and is
superseded below.

## Desktop and window chrome (M5)

| Element | Baseline | Beyond baseline |
|---|---|---|
| Desktop | Background fill, owns z-ordered windows, tile/cascade, window cycling, select-by-number, window-list dialog | Background drawing hook; typed snapshot/restore of layout (geometry, z-order, zoom) |
| Window | Frame (active/inactive styles), title, close control, zoom/restore, move by title-bar drag or keyboard mode, resize by grip/edges or keyboard mode, min/max size limits, **shadow**, vetoable close request on every close path | Backing store (drag never repaints content); double/single/rounded frame sets; optional maximize-to-desktop |
| Dialog | Explicit modal and modeless presentation, default button (Enter), Esc-cancel, accept-time validation with veto (a failing control blocks accept and takes focus), correct initial focus by construction, focus restore on close | Typed non-blocking completion handle; declarative descriptor construction (labels auto-bind mnemonics to controls); resizable dialogs with layout |
| Message boxes | Info/warning/error/confirm with standard button sets | Localizable via string table; theme-role styled severity accents |

## Menu system (M5)

| Element | Baseline | Beyond baseline |
|---|---|---|
| Menu bar | F10/Alt activation, mnemonic letters, arrow navigation, drop-down on Enter/Down/hover-while-open, disabled items | Overflow behavior on narrow terminals; live structure — runtime add/remove of items and whole top-level menus, safe while a dropdown is open |
| Drop-down menu | **Shadow**, separators, nested submenus, right-aligned accelerator hints, checkable items, disabled items, mnemonic jump, Esc closes one level | Commands-driven: items render chord/enablement live from the command registry and may carry an explicit, surface-specific display label/mnemonic without duplicating the command |
| Context menu | Positional pop-up with same feature set | Keyboard invocation (Menu key / Shift-F10) at focus location |
| Status line | Context-sensitive hints bound to focus, clickable items, command chords displayed | Command presentation labels independent of menu wording; priority-based item layout on narrow widths |

Menu behavior contract (excerpt, pinned by event scripts): bar activation
never steals unsaved state; navigation wraps; disabled items are skipped
by arrows but visible; accelerators fire without opening menus; every
menu action is reachable by keyboard alone and by mouse alone.

## Controls (M4 minimal set; M6 completion)

The M4 minimal set is Label, Static text, Button, and Input line (core
editing); everything else lands in M6. Input line and Combo box histories
live in the application-scoped history registry — shared by key across
widgets and dialogs, never per-widget state.

| Widget | Baseline | Beyond baseline |
|---|---|---|
| Label | Mnemonic that focuses its buddy control | — |
| Static text | Styled, wrapped | Alignment options |
| Button | Default/normal, mnemonic, pressed feedback | Repeat-on-hold option |
| Input line | Editing keys, selection, clipboard, overwrite/insert, scrolling content, validators, input masks, password echo, history cycling | Grapheme-correct editing (never splits clusters); undo within field |
| Memo | Multi-line editing, scrolling, selection, clipboard, bounded undo | Word wrap toggle; grapheme-correct navigation |
| Check group | Multi-select, arrow navigation, mnemonics | Tri-state option |
| Radio group | Exclusive select, arrow navigation, mnemonics | — |
| List view | Scrolling, single/multi select, keyboard search, injected-Clock double-click activate | Post-0.1 per D-041: virtualized model interface and columned modes |
| Combo box | — (the classics lacked a true one) | Editable and pick-only variants, history integration, dropdown with shadow |
| Scrollbar | Proportional thumb, arrows, paging, drag | Auto-hide policy option |
| Scroll viewport | Scrolls any content view, keyboard and wheel | Semantic cell-line scrolling; scroll-region byte-diff optimization is renderer/performance scope per D-041 |
| Tab control | — | Top tabs with mnemonics, keyboard cycling, per-tab pages, overflow |
| Table | — | Columns with headers, resize, sort indicators, cell styling hook; virtualized rows are post-0.1 per D-041 |
| Tree / outline | Expand/collapse, keyboard navigation | Connector-line styles; virtualized model is post-0.1 per D-041 |
| Progress | — | Determinate/indeterminate, label slot |
| Splitter | — | Keyboard-adjustable split panes |
| Text view | Read-only styled text, scrolling | Links (activatable; rendered as OSC 8 hyperlinks where supported), simple inline styles; the building block for help browsers and about boxes |
| Help viewer | Context topics (F1 from any view via help-context keys), activatable cross-links, back navigation | Index/keyword lookup; content via an injected provider — the library defines no help file format |
| Image view | — | Shows an `Image` scaled to its cell box; mandatory fallback content; dual-space (cell + pixel) mouse events where the terminal reports pixels |
| Canvas | — | Application draws RGBA at the widget's current pixel size (cell box × cell metrics, redraw on resize); mandatory fallback painter; dual-space mouse events |

## Standard dialogs (M6)

| Dialog | Baseline | Beyond baseline |
|---|---|---|
| File open/save | Path input with completion, file list, directory navigation, filters | Recent locations via the history registry, hidden-file toggle, cross-platform path semantics; enumerates through the injected filesystem (headless-testable) |
| Directory picker | Tree-based selection | — |
| Window list | Select/activate/close from list | Type-ahead filter |
| Theme editor | — | Enumerates theme roles from the flat tables, edits styles with live preview, produces a theme value the application persists (M11) |

## Explicitly not widgets

- A code/document **editor**: applications own their editors; the library
  provides Memo (forms) and Text view (display) only.
- A terminal emulator widget: out of scope for 0.1; revisit on demand.

## Cross-cutting acceptance (all widgets)

- Operable entirely by keyboard and entirely by mouse.
- Correct under every built-in scheme including Mono/16-color.
- Correct with wide/combining/emoji graphemes in content and labels.
- Correct under terminal shrink to and below the minimum supported size
  (deterministic clamping, never corrupted output).
- Golden-tested in active, inactive, focused, disabled states.
- Raster-bearing widgets additionally: correct occlusion and scrolling,
  symbolic raster-scene goldens, decoded pixel-plane visual goldens under a
  fixed-metrics Sixel profile, fallback rendering with zero raster pixels under
  a no-graphics profile, runtime switching between those profiles without
  stale pixels, and dual-space mouse verified under a pixel-mouse profile.
- Documented behavior contract: keys, mouse, notifications, theme roles.

## Current corrective ledger

M5/M6 acceptance is reopened. This is a finding inventory, not a substitute
for WP-34C's required row-by-row traceability matrix. Every item below remains
open until that matrix carries literal evidence. A prior code landing does not
remove an item when its integrated behavior, full state/scheme appearance, or
reference acceptance is missing.

| Open or unproven item | Catalog row / contract | Corrective owner |
|---|---|---|
| Desktop attachment, activation, cycling, direct-dialog attachment, focus, modal detachment, and zoom-target lifetime are coherent through every public attach/detach path; broader modal routing remains | Desktop; Window; Dialog | WP-27 |
| Paint-scope junction ownership and binary/non-cumulative shadow coverage across overlapping z-levels; window backing store, damage-scoped painting, composition-only move, resize-confined repaint, enforceable performance counters | Window beyond-baseline; D-010/D-036/D-037 | WP-31 |
| Default button, Esc, validation veto, focus restore, nested modality, explicit modal/modeless presentation, typed non-blocking completion, and complete blocking standard-dialog conveniences | Dialog baseline / beyond-baseline | WP-27, WP-36 |
| Message-box severity accents and localizable standard strings | Message boxes beyond-baseline | WP-34A |
| Live menu/status structure, safe mutation while open, status hints/enablement, command retraction, surface-specific command presentation, and declarative application chrome composition | Menu bar beyond-baseline; Status line baseline | WP-35 |
| Nested/checkable dropdowns and keyboard context-menu invocation | Drop-down / context menu baseline | WP-34A |
| Menu overflow and status priority layout on narrow terminals | Menu bar / Status line beyond-baseline | WP-34A or a new settled re-scope decision |
| Label mnemonic activates/focuses its buddy | Label baseline | WP-34B |
| StaticText alignment and visible height-for-width layout | Static text beyond-baseline; D-006 | WP-34B, WP-36 |
| Button repeat-on-hold | Button beyond-baseline | WP-34B |
| InputLine clipboard cut/copy/paste, pointer drag selection, history cycling, complete undo, and grapheme correctness | Input line baseline/beyond-baseline | WP-30, WP-34B; dropdown history surface moved to ComboBox by D-041 |
| Memo clipboard, bounded undo, word wrap, and grapheme navigation | Memo baseline/beyond-baseline | WP-30, WP-34B |
| Check/Radio mnemonic completeness and CheckGroup tri-state | Option groups baseline/beyond-baseline | WP-34B |
| ListView Clock-based double-click semantics; materialized search/selection behavior | List view baseline/beyond-baseline | WP-34B; virtualized/columned model moved post-0.1 by D-041 |
| ComboBox editable/pick-only/history/shadow behavior | Combo box beyond-baseline | WP-34B |
| Scrollbar auto-hide and viewport wheel delivery over descendant content; semantic cell-line scrolling | Scrollbar / Scroll viewport | WP-31, WP-34B; scroll-region byte-diff optimization moved out of widget catalog by D-041 |
| TabControl complete behavior | Tab control beyond-baseline | WP-34B |
| Table resize/sort/styled-cell model | Table beyond-baseline | WP-34C; virtualized rows moved post-0.1 by D-041 |
| Tree connector styles and complete lazy/navigation integration | Tree beyond-baseline | WP-34C, WP-36; virtualized model moved post-0.1 by D-041 |
| Progress complete behavior | Progress beyond-baseline | WP-34B |
| Splitter keyboard adjustment and integrated resize/golden behavior | Splitter beyond-baseline | WP-34C, WP-36 |
| TextView activatable links, OSC 8 export, and inline styles | Text view beyond-baseline | WP-34C |
| Help cross-links/back/index/provider behavior | Help viewer baseline/beyond-baseline | WP-34C |
| ImageView scaling, fallback, occlusion/scroll, dual-space pointer matrix, and paired decoded-Sixel/no-graphics visual captures | Image view beyond-baseline; raster acceptance | WP-32A, WP-34C, WP-38 |
| Canvas automatically derives pixel size from cell metrics, redraws on metric/size change, and passes paired decoded-Sixel/no-graphics visual captures | Canvas beyond-baseline | WP-32A, WP-34C |
| File open/save completion, filters, recent locations, hidden toggle, injected filesystem, and cross-platform paths | File dialog baseline/beyond-baseline | WP-28, WP-34C |
| Directory picker and Window list full keyboard/mouse/filter/close behavior | Standard dialogs | WP-34A, WP-34C |
| Desktop typed snapshot/restore | Desktop beyond-baseline | WP-34A |
| Every widget × applicable active/inactive/focused/disabled state × Classic/Dark/Light/Mono, plus scripts and reference docs | Cross-cutting acceptance; M6 exit | WP-38 |

The catalog currently says M6 implements both baseline and beyond-baseline
columns. Therefore “post-0.1” is not a valid state by ledger annotation alone.
WP-34A…WP-34C must either implement such an item for 0.1 or record a settled decision
that changes ROADMAP/catalog scope before code or status is changed.

## WP-34C traceability matrix

This matrix is the WP-34C replacement for narrative catalog assertions for the
data, media, help, file, and remaining standard-dialog rows. WP-38 still owns
the exhaustive cross-scheme visual/reference promotion gate.

| Catalog item | 0.1 implementation / scope authority | Executable evidence | State |
|---|---|---|---|
| Table headers, resize, sort indicators, styled-cell hook | `widgets::Table`, `TableColumn`, `Table::sort_by`, `Table::set_cell_style_hook`; virtualized rows and cell editor are post-0.1 by D-041 | `suite_test_table` | Implemented locally |
| Tree expand/collapse, keyboard/mouse navigation, lazy population, connector styles | `widgets::TreeView`, `TreeNode::children_known`, `TreeView::on_expand_request`, `TreeConnectorStyle`; virtualized provider is post-0.1 by D-041 | `suite_test_tree_view` | Implemented locally |
| Splitter keyboard-adjustable panes | `widgets::Splitter::on_key`, `set_split_position`, size-hint clamp; integrated resize golden remains WP-36 | `suite_test_splitter` | Implemented locally; integrated golden deferred to WP-36 |
| TextView links, OSC 8, inline styles | `widgets::TextView::set_spans`, `TextSpan`, `on_link_activate`, `osc8_text` | `suite_test_text_view` | Implemented locally |
| Help viewer cross-links, back, index/keyword lookup, injected provider | `HelpProvider`, `MemoryHelpProvider::index/search`, `make_help_viewer`, `present_help_viewer` | `suite_test_help_viewer` | Implemented locally |
| ImageView fallback, raster anchor, dual-space mouse | `widgets::ImageView`, mandatory `Painter::draw_image` fallback, full `MouseEvent` forwarding | `suite_test_image_view`; final paired decoded/fallback captures remain WP-38 | Implemented locally; final visual matrix deferred to WP-38 |
| Canvas metric-derived pixel size, fallback, raster anchor, redraw on metric/size change, dual-space mouse | `widgets::Canvas::set_cell_metrics`, `pixel_size`, `on_resized`, mandatory fallback, full `MouseEvent` forwarding | `suite_test_canvas`; final paired decoded/fallback captures remain WP-38 | Implemented locally; final visual matrix deferred to WP-38 |
| File open/save completion, filters, recent locations, hidden toggle, injected filesystem, separator-portable paths | `FileDialogOptions`, `FileDialogFilter`, `CompletingInputLine`, `FileSystem::normalize_path/is_absolute_path`, `MemoryFileSystem` | `suite_test_file_dialog`, `suite_test_filesystem` | Implemented locally |
| Directory picker tree selection | `make_directory_picker`, `present_directory_picker` over injected `FileSystem` | `suite_test_directory_picker` | Implemented locally |
| Window list keyboard/mouse/filter/close behavior | `make_window_list_dialog`, type-ahead/filter and activation/close behavior | `suite_test_window_list_dialog` | Implemented locally |
