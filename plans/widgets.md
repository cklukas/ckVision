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
| Dialog | Explicit modal and modeless presentation, buttons that state their role (accept / dismiss / neutral — D-050), default button (Enter), Esc-cancel, accept-time validation with veto (a failing control blocks accept and takes focus), correct initial focus by construction, focus restore on close | Typed non-blocking completion handle; declarative descriptor construction (labels auto-bind mnemonics to controls, including multi-line Memo fields); resizable dialogs with layout |
| Message boxes | Info/warning/error/confirm with standard button sets | Localizable via string table; theme-role styled severity accents |

## Menu system (M5)

| Element | Baseline | Beyond baseline |
|---|---|---|
| Menu bar | F10/Alt activation, mnemonic letters, arrow navigation, drop-down on Enter/Down/hover-while-open, disabled items | Overflow behavior on narrow terminals; live structure — runtime add/remove of items and whole top-level menus, safe while a dropdown is open |
| Drop-down menu | **Shadow**, separators, nested submenus, right-aligned accelerator hints, checkable items, disabled items, mnemonic jump, Esc closes one level | Commands-driven: items render chord/enablement live from the command registry and may carry an explicit, surface-specific display label/mnemonic without duplicating the command |
| Context menu | Positional pop-up with same feature set | Keyboard invocation (Menu key / Shift-F10) at focus location |
| Status line | Context-sensitive hints bound to focus, clickable items, command chords displayed | Command presentation labels independent of menu wording; priority-based item layout on narrow widths |

Menu behavior contract (excerpt, pinned by event scripts): bar activation
never steals unsaved state; navigation wraps; arrows stop on disabled
items, which stay visible, do nothing on Enter, and show their reason on the
status line (D-083); accelerators fire without opening menus; every menu
action is reachable by keyboard alone and by mouse alone.

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
| Input line | Editing keys, selection, clipboard, overwrite/insert, scrolling content, validators, input masks, password echo, history cycling | Grapheme-correct editing (never splits clusters); undo within field; shared Ctrl/C/V/X, Ctrl+Insert/Shift+Insert, Ctrl word navigation/deletion, and Shift selection semantics |
| Key chord capture | Focused, command-safe capture of one normalized `KeyChord`; Enter/Space starts capture, Escape abandons it, Backspace/Delete clears it | Application-owned conflict policy and persistence; the widget only captures a typed input value |
| Memo | Multi-line editing, scrolling, selection, clipboard, bounded undo; descriptor-form field with accept-time validation | Word wrap toggle; grapheme-correct navigation; shared Ctrl/C/V/X, Ctrl+Insert/Shift+Insert, Ctrl word/document navigation/deletion, and Shift selection semantics |
| Check group | Multi-select, arrow navigation, mnemonics | Tri-state option |
| Radio group | Exclusive select, arrow navigation, mnemonics | — |
| List view | Provider-backed scrolling, stable single/multi selection, keyboard search, injected-Clock double-click activate | Application-defined row presentation and asynchronous cache policy; provider contract in D-043 |
| Combo box | — (the classics lacked a true one) | Editable and pick-only variants, history integration, dropdown with shadow |
| Scrollbar | Proportional thumb, arrows, paging, drag | Auto-hide policy option |
| Scroll viewport | Scrolls any content view, keyboard and wheel | Semantic cell-line scrolling; scroll-region byte-diff optimization is renderer/performance scope per D-041 |
| Tab control | — | Top tabs with mnemonics, keyboard cycling, per-tab pages, overflow |
| Table | — | Provider-backed rows, typed editable cells, stable row/cell selection, headers, resize, sort requests, and cell styling; application-defined range/formula/database semantics per D-043 |
| Tree / outline | Expand/collapse, keyboard navigation, stable-ID provider model | Connector-line styles; compact `TreeNode` values remain available |
| Progress | — | Determinate/indeterminate, label slot |
| Splitter | — | Keyboard-adjustable split panes |
| Text view | Read-only styled text, scrolling | Links (activatable; rendered as OSC 8 hyperlinks where supported), simple inline styles; the building block for help browsers and about boxes |
| Flow view | Read-only wrapped styled content, scrolling, keyboard/mouse link activation | Inline raster atoms with mandatory fallback and ordinary scene composition; semantic document/chat models remain client-owned (D-043) |
| Text editor | Revisioned UTF-8 document editing, selections, undo/redo, scrolling, gutter, and line/column indicator | Search/replace, deterministic profile detection, incremental syntax styling, and composable file-editor workflow; WP-39…WP-44 |
| Terminal view | Private child PTY/ConPTY session, UTF-8 terminal cells, focus/input/resize, scrollback, and process lifecycle | Alternate buffer, mouse/paste/query policy, nested ckVision applications, and decoded Sixel re-composited with no spill; WP-45…WP-50 |
| Help viewer | Context topics (F1 from any view via help-context keys), activatable cross-links, back navigation | Index/keyword lookup; content via an injected provider — the library defines no help file format |
| Image view | — | Shows an `Image` scaled to its cell box; mandatory fallback content; dual-space (cell + pixel) mouse events where the terminal reports pixels |
| Canvas | — | Application draws RGBA at the widget's current pixel size (cell box × cell metrics, redraw on resize); mandatory fallback painter; dual-space mouse events |

## Common modern components (WP-36B)

These are not replacements for the classic M5/M6 catalog; they are the
additional components a practical client library should provide so common
applications do not hand-roll inconsistent patterns. They are 0.1-planned
unless explicitly marked post-0.1 below.

| Component | Baseline | Beyond baseline / notes |
|---|---|---|
| Calendar view | Month grid, keyboard/mouse day selection, range/disabled-date support, current-day styling, caller-supplied labels | Optional ISO week row; no library locale/timezone/clock reads |
| Date picker | Input/dropdown or dialog-backed date selection over Calendar view | Formatting/parsing policy supplied by options or caller callbacks |
| Time picker | Hour/minute/second selection, 12/24-hour display option | No locale/global state; optional seconds field |
| Spin box / numeric stepper | Integer value with min/max/step, arrows/buttons/wheel, validation feedback | Optional editable text field |
| Slider | Horizontal bounded numeric value selection with keyboard/mouse operation | Optional tick labels |
| Search box / filter bar | InputLine plus clear/status affordance for filtering list/table/tree/text content | Debounce, if any, must use injected clock/timer; no hidden wall-clock reads |
| Command palette | Searchable command popup over `ui::CommandRegistry` | Honors command contexts, enablement, labels/chords, retraction, and rebinding live |
| Tool bar / action bar | Compact command-button row, dockable top/bottom, keyboard/mouse complete | Overflow policy on narrow widths; shares CommandPresentation with menu/status |
| Breadcrumb bar | Segment navigation with overflow elision and keyboard focus | Caller supplies path semantics/separators; no filesystem access in widget |
| Property inspector | Two-column name/value editor with typed cell editors and validation | Integrates text/bool/enum/numeric/date-time editors as they exist |
| Wizard / assistant | Multi-page workflow shell with Back/Next/Finish/Cancel and per-page validation | Progress/step indicator and typed non-blocking completion |
| Notification center / toast | Severity-styled transient/persistent messages with keyboard dismissal | Timing, if any, must be injected; no hidden wall-clock reads |
| Tooltip / popover | Focus/pointer-triggered explanatory popup with deterministic placement | Keyboard-accessible invocation; same popup dismissal discipline as menus |

**Implementation status.** WP-36B implements the 0.1 common-component set in
`include/cvision/widgets/common_components.hpp` and
`src/widgets/common_components.cpp`, with focused coverage in
`tests/test_common_components.cpp` and example integration in `examples/forms`
and `examples/workbench`. WP-38 owns the exhaustive state/scheme/reference
visual matrix before release acceptance.

Post-0.1 candidates unless promoted by a recorded decision: tree-table /
outline-table with columns, chart widgets beyond `Progress`/`Canvas`, and
dockable/floating tool-palette systems.

## Standard dialogs (M6)

| Dialog | Baseline | Beyond baseline |
|---|---|---|
| File open/save | Path input with completion, file list, directory navigation, filters | Recent locations via the history registry, hidden-file toggle, cross-platform path semantics; enumerates through the injected filesystem (headless-testable) |
| Directory picker | Tree-based selection | — |
| Window list | Select/activate/close from list | Type-ahead filter |
| Theme editor | — | Enumerates theme roles from the flat tables, edits styles with live preview, produces a theme value the application persists (M11) |
| Date/time picker dialogs | Calendar/time selection using the WP-36B components | Localizable labels supplied through explicit string/options tables; no locale reads |

## Explicitly not widgets

- A network/remote terminal client, terminal multiplexer, or raw child-output
  pass-through: out of scope for 0.1. `TerminalView` is a private, contained
  local PTY/ConPTY terminal session (D-042).

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
for the row-by-row traceability matrices below (WP-34C's, and the
[WP-38 catalog traceability matrix](#wp-38-catalog-traceability-matrix) that
covers every catalog row). Every item below remains open until a matrix
carries literal evidence. A prior code landing does not
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
| Tree connector styles, complete lazy/navigation integration, and stable-ID provider model | Tree beyond-baseline | WP-34C, WP-36; provider promoted by D-043 from real large-tree clients |
| Progress complete behavior | Progress beyond-baseline | WP-34B |
| Splitter keyboard adjustment and integrated resize/golden behavior | Splitter beyond-baseline | WP-34C, WP-36 |
| TextView activatable links, OSC 8 export, and inline styles | Text view beyond-baseline | WP-34C |
| Editor document, TextEditor interaction, search/replace, standard JSON/YAML/Bash profiles, profile extension, and injected file-editor workflow | Text editor | WP-39…WP-44, WP-38 |
| Private terminal emulator, process/PTY lifecycle, input/resize, nested ckVision session, child Sixel raster containment, and Windows ConPTY matrix | Terminal view | WP-45…WP-50, WP-37, WP-38 |
| Help cross-links/back/index/provider behavior | Help viewer baseline/beyond-baseline | WP-34C |
| ImageView scaling, fallback, occlusion/scroll, dual-space pointer matrix, and paired decoded-Sixel/no-graphics visual captures | Image view beyond-baseline; raster acceptance | WP-32A, WP-34C, WP-38 |
| Canvas automatically derives pixel size from cell metrics, redraws on metric/size change, and passes paired decoded-Sixel/no-graphics visual captures | Canvas beyond-baseline | WP-32A, WP-34C |
| File open/save completion, filters, recent locations, hidden toggle, injected filesystem, and cross-platform paths | File dialog baseline/beyond-baseline | WP-28, WP-34C |
| Directory picker and Window list full keyboard/mouse/filter/close behavior | Standard dialogs | WP-34A, WP-34C |
| Desktop typed snapshot/restore | Desktop beyond-baseline | WP-34A |
| Comprehensive client-facing examples cover every widget, layout, dialog, graphics/degradation path, and command/theme capability through public API | Example suite / client usability | WP-36A, WP-38 |
| Calendar, date/time picker, numeric stepper, slider, search/filter, command palette, toolbar, breadcrumb, property inspector, wizard, notifications, and tooltip/popover | Common modern components | WP-36B, WP-38 |
| Every widget × applicable active/inactive/focused/disabled state × Classic/Dark/Light/Mono, plus scripts and reference docs | Cross-cutting acceptance; M6 exit | WP-38: state × scheme goldens Done by the [appearance matrix](wp38-appearance-matrix.md); reference docs Done by the `doc_api_comments` and `doc_widget_coverage` gates; interaction scripts Done by the `interaction_script_coverage` gate over the [interaction-script map](../docs/coverage.md#interaction-scripts). Nothing in this row remains open |

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
| Tree expand/collapse, keyboard/mouse navigation, lazy population, connector styles, provider refresh | `widgets::TreeView`, `TreeNode::children_known`, `TreeView::on_expand_request`, `TreeModel`, `TreeConnectorStyle` | `suite_test_tree_view` | Implemented locally |
| Splitter keyboard-adjustable panes | `widgets::Splitter::on_key`, `set_split_position`, size-hint clamp; integrated resize acceptance is covered by WP-36 shrink/filebrowser scripts | `suite_test_splitter`, `suite_test_wp36`, `suite_test_filebrowser_smoke` | Implemented locally |
| TextView links, OSC 8, inline styles | `widgets::TextView::set_spans`, `TextSpan`, `on_link_activate`, `osc8_text` | `suite_test_text_view` | Implemented locally; OSC 8 is an export string only, so the WP-38 matrix below records the link row as Partial |
| Help viewer cross-links, back, index/keyword lookup, injected provider | `HelpProvider`, `MemoryHelpProvider::index/search`, `make_help_viewer`, `present_help_viewer` | `suite_test_help_viewer` | Implemented locally; cross-links are not activatable and Back is untested, so the WP-38 matrix below records both as Partial |
| ImageView fallback, raster anchor, dual-space mouse | `widgets::ImageView`, mandatory `Painter::draw_image` fallback, full `MouseEvent` forwarding | `suite_test_image_view`; final paired decoded/fallback captures remain WP-38 | Implemented locally; final visual matrix deferred to WP-38 |
| Canvas metric-derived pixel size, fallback, raster anchor, redraw on metric/size change, dual-space mouse | `widgets::Canvas::set_cell_metrics`, `pixel_size`, `on_resized`, mandatory fallback, full `MouseEvent` forwarding | `suite_test_canvas`; final paired decoded/fallback captures remain WP-38 | Implemented locally; final visual matrix deferred to WP-38 |
| File open/save completion, filters, recent locations, hidden toggle, injected filesystem, separator-portable paths | `FileDialogOptions`, `FileDialogFilter`, `CompletingInputLine`, `FileSystem::normalize_path/is_absolute_path`, `MemoryFileSystem` | `suite_test_file_dialog`, `suite_test_filesystem` | Implemented locally |
| Directory picker tree selection | `make_directory_picker`, `present_directory_picker` over injected `FileSystem` | `suite_test_directory_picker` | Implemented locally |
| Window list keyboard/mouse/filter/close behavior | `make_window_list_dialog`, type-ahead/filter and activation/close behavior | `suite_test_window_list_dialog` | Implemented locally; type-ahead selects rather than filters and no action closes a listed window, so the WP-38 matrix below records both as Partial |

## WP-38 catalog traceability matrix

Every catalog row above, split into the features its Baseline and Beyond
cells name, in the catalog's own words. Each feature links its
implementation, named test cases, and reference documentation. A test marked
**[script]** is an Application-level script: it reaches the view in a real
`Application` and sends input only through `Application::dispatch`,
`HeadlessTerminal::inject_event` or `Application::step`. The
[interaction-script map](../docs/coverage.md#interaction-scripts) names one
such script for every public view type, and the `interaction_script_coverage`
gate checks it. Documentation paths are under `docs/`. The appearance matrix
([wp38-appearance-matrix.md](wp38-appearance-matrix.md)) covers every view
type's state × scheme goldens, so it is not repeated per row.

States:

- **Implemented**: the catalog wording is realised, and a named test fails if
  it stops holding.
- **Partial**: only part of the catalog wording is realised. The note says
  which part is missing.
- **Missing**: nothing realises it.

Partial and Missing items are listed again under
[Open catalog items](#open-catalog-items-owner-decision). This matrix changes
no catalog scope. Implementing, re-scoping or amending an open item is an
owner decision.

### Desktop and window chrome (M5)

| Row | Feature (catalog wording) | Implementation | Tests | Docs | State |
|---|---|---|---|---|---|
| Desktop | Background fill | `Desktop::draw` (`ckv.desktop.background`) | `test_desktop.cpp` `a_plain_desktop_child_that_hides_or_moves_uncovers_the_desktop_background` [script]; `test_standard_roles.cpp` `desktop_background_is_visually_distinct_from_dialog_background_in_every_scheme` | `widget-gallery.md#desktop` | Implemented |
| Desktop | owns z-ordered windows | `Desktop::add_window`, `activate`, `windows` | `test_desktop.cpp` `a_newly_added_window_becomes_the_topmost_child`; `test_m8_integration.cpp` `clicking_deep_content_in_a_background_window_raises_and_activates_it_through_dispatch` [script] | `widget-gallery.md#desktop` | Implemented |
| Desktop | tile/cascade | `Desktop::tile`, `tile_horizontally`, `tile_vertically`, `cascade` | `test_desktop.cpp` `tile_divides_the_desktop_width_evenly_among_windows_with_no_overlap_or_gap`, `cascade_makes_the_last_window_topmost`, `ktile_default_arranges_every_window_into_the_content_area` | `widget-gallery.md#desktop` | Implemented |
| Desktop | window cycling | `Desktop::activate_next`, `activate_previous` (F6, Shift+F6) | `test_desktop.cpp` `activate_next_visits_every_window_exactly_once_before_repeating`; `test_desktop_dialog_golden.cpp` `desktop_dialog_script_pins_generic_attach_cycling_and_close_frames` [script] | `widget-gallery.md#desktop`, `standard-commands.md#the-set` | Implemented |
| Desktop | select-by-number | `Desktop::select_by_number` | `test_desktop.cpp` `select_by_number_activates_the_nth_window_in_insertion_order`, `naming_a_minimized_window_by_number_brings_it_back_and_activates_it` | none | Partial: API only; no key or command reaches it, although the header comment describes an Alt+1..Alt+9 convention |
| Desktop | window-list dialog | `make_window_list_dialog`, `present_window_list_dialog`; kWindowList default handler | `test_window_list_dialog.cpp` `activating_a_row_switches_the_desktops_active_window_and_closes_the_dialog` [script]; `test_desktop.cpp` `kwindowlist_default_puts_up_a_list_of_this_desktops_windows` | `widget-gallery.md#window-list-dialog` | Implemented |
| Desktop | Background drawing hook | none named; `Desktop::set_content` fills the content area under the windows | `test_desktop.cpp` `desktop_content_fills_the_content_area_immediately`, `a_window_floats_above_the_desktop_content` | none | Partial: no hook for the background itself; `set_content` is the nearest mechanism and is undocumented |
| Desktop | typed snapshot/restore of layout (geometry, z-order, zoom) | `Desktop::Snapshot`, `WindowSnapshot`, `snapshot`, `restore` | `test_desktop.cpp` `snapshot_restore_recovers_window_geometry_zoom_active_state_and_z_order`, `snapshot_restore_ignores_windows_that_have_left_the_desktop` | `widget-gallery.md#desktop` (names `restore` only) | Implemented |
| Window | Frame (active/inactive styles) | `Window::draw`, `frame_style` (`ckv.window.frame.active/inactive`) | `test_window.cpp` `an_active_window_draws_with_double_line_box_glyphs`, `an_inactive_window_draws_with_single_line_box_glyphs`; `test_frame_text.cpp` `a_scripted_frame_text_follows_its_window_through_a_click_that_deactivates_it` [script] | `widget-gallery.md#window` | Implemented |
| Window | title | `Window::set_title` | `test_window.cpp` `the_title_is_centered_with_a_padding_space_on_each_side_not_left_aligned`, `draw_truncates_a_title_longer_than_the_available_space` | `widget-gallery.md#window` | Implemented |
| Window | close control | `Window::close`, close control on release | `test_window.cpp` `the_close_control_acts_on_release_not_on_press`; `test_editor_window.cpp` `a_scripted_editor_window_marks_typed_changes_and_keeps_its_close_control_vetoed` [script] | `widget-gallery.md#window` | Implemented |
| Window | zoom/restore | `Window::toggle_zoom`, `zoomed` (F5) | `test_window.cpp` `toggle_zoom_grows_to_the_supplied_available_rect_and_toggling_again_restores`; `test_desktop.cpp` `terminal_mouse_zoom_and_restore_track_the_desktop_after_a_resize` | `widget-gallery.md#window` | Implemented |
| Window | move by title-bar drag or keyboard mode | title-bar drag; `Window::enter_move_mode` | `test_window.cpp` `dragging_the_title_bar_moves_the_window`, `move_mode_arrow_keys_translate_the_window`; `test_application_render.cpp` `dragging_a_window_recomposes_without_repainting_its_retained_content` [script] | `widget-gallery.md#window` (drag only) | Partial: keyboard move mode exists but no command or chord enters it |
| Window | resize by grip/edges or keyboard mode | corner grips; `Window::enter_resize_mode` | `test_window.cpp` `each_corner_resizes_and_leaves_the_opposite_one_where_it_was`, `resize_mode_arrow_keys_change_size_not_position` | `widget-gallery.md#window` | Partial: corners only (the header: plain edges "say nothing"); keyboard resize mode has no command or chord |
| Window | min/max size limits | `Window::set_min_size`, `set_max_size` | `test_window.cpp` `resizing_below_the_minimum_size_clamps_rather_than_shrinking_further`, `resizing_above_the_maximum_size_clamps` | `widget-gallery.md#window` | Implemented |
| Window | **shadow** | `Window::casts_shadow`, `Desktop::child_casts_shadow` | `test_desktop.cpp` `overlapping_window_shadows_dim_the_desktop_exactly_once`; `test_application_render.cpp` `window_and_dropdown_shadows_form_one_retained_binary_union` [script] | `performance.md#performance-verification` | Implemented |
| Window | vetoable close request on every close path | `Window::close_request`, `close` | `test_window.cpp` `a_close_request_returning_false_vetoes_the_close_and_on_closed_never_fires`; `test_message_box.cpp` `a_vetoed_presented_message_box_remains_modal_until_a_later_close_succeeds` [script] | `dialogs-and-commands.md#focus-and-close-policy` | Implemented (a host `request_quit` during `exec_modal` force-detaches by design, `desktop.hpp`) |
| Window | Backing store (drag never repaints content) | `Window::repaint_backing_if_needed`, `content_repaint_count`; `Desktop::paint_retained` | `test_application_render.cpp` `dragging_a_window_recomposes_without_repainting_its_retained_content` [script], `moving_a_window_recomposes_its_retained_backing_without_content_repaint` [script] | `performance.md#performance-verification` | Implemented |
| Window | double/single/rounded frame sets | fixed: double while active, single while inactive | none | none | Partial: no API chooses a frame set, and no rounded frame is drawn |
| Window | optional maximize-to-desktop | `Desktop::set_maximize_follows_active`, `DesktopGrowPolicy::KeepFilling` | `test_desktop.cpp` `an_enabled_desktop_opens_a_new_window_maximized_beside_a_maximized_one`, `a_keep_filling_window_tracks_desktop_growth_without_being_zoomed` | `widget-gallery.md#desktop` | Implemented |
| Dialog | Explicit modal and modeless presentation | `Desktop::present_modal`, `present_modeless`; `present_dialog` | `test_message_box.cpp` `present_message_box_scopes_input_immediately_and_completes_after_detach` [script]; `test_dialog_presentation.cpp` `standard_modal_presentations_size_and_center_an_unpositioned_window` [script] | `object-model.md#modal-versus-modeless-surfaces` | Implemented |
| Dialog | buttons that state their role (accept / dismiss / neutral — D-050) | `ButtonRole`, `ButtonDescriptor` | `test_dialog.cpp` `a_neutral_button_runs_its_handler_and_leaves_the_dialog_up`; `test_dialog_presentation.cpp` `pressing_the_accepting_button_accepts_with_the_typed_values` [script] | `dialogs-and-commands.md#descriptor-dialog-with-validation` | Implemented |
| Dialog | default button (Enter) | `Window::accept_request`, `MaterializedDialog::default_button` | `test_message_box.cpp` `the_ok_button_is_the_default_and_fires_via_enter_regardless_of_where_focus_is` [script]; `test_m4_integration.cpp` `a_two_field_dialog_driven_headlessly_end_to_end` [script] | `dialogs-and-commands.md#descriptor-dialog-with-validation` | Implemented |
| Dialog | Esc-cancel | `Window::cancel_request` | `test_message_box.cpp` `escape_on_an_ok_cancel_box_fires_cancel_not_ok` [script]; `test_wp36.cpp` `descriptor_dialog_validation_veto_and_escape_cancel_run_on_window_hooks` | `dialogs-and-commands.md#descriptor-dialog-with-validation` | Implemented |
| Dialog | accept-time validation with veto (a failing control blocks accept and takes focus) | `validate_dialog`, `FieldDescriptor::validate`, `DialogDescriptor::check` | `test_dialog.cpp` `validate_dialog_with_an_invalid_field_vetoes_marks_it_invalid_and_focuses_it`, `a_checked_form_stays_open_on_a_veto_with_the_field_marked_focused_and_the_reason_shown` [script] | `dialogs-and-commands.md#checking-the-whole-answer` | Implemented |
| Dialog | correct initial focus by construction | `MaterializedDialog::initial_focus` | `test_dialog.cpp` `initial_focus_is_the_first_input_when_the_dialog_has_fields`, `initial_focus_falls_back_to_the_first_button_when_the_dialog_has_no_fields` | `widget-gallery.md#materializeddialog` | Implemented |
| Dialog | focus restore on close | `DialogFocusRestore` | `test_message_box.cpp` `closing_the_box_restores_focus_to_the_view_that_invoked_it` [script]; `test_dialog_presentation.cpp` `standard_dialogs_never_dereference_a_destroyed_former_focus_on_close` [script] | `object-model.md#modal-versus-modeless-surfaces` | Implemented |
| Dialog | Typed non-blocking completion handle | `DialogPresentation<Result>`, `PendingDialogs` | `test_dialog_presentation.cpp` `descriptor_dialog_presentation_returns_typed_values_only_after_detachment` [script], `pending_dialogs_keep_each_presentation_until_it_answers_and_let_the_answer_ask_again` [script] | `widget-gallery.md#pendingdialogs` | Implemented |
| Dialog | declarative descriptor construction (labels auto-bind mnemonics to controls, including multi-line Memo fields) | `DialogDescriptor`, `FieldDescriptor`, `FieldKind::Memo`, `materialize_dialog` | `test_dialog.cpp` `a_labeled_fields_mnemonic_buddy_is_wired_to_its_own_input`, `a_memo_field_materializes_as_a_labeled_multiline_editor`; `test_dialog_presentation.cpp` `a_typed_form_mixes_every_field_kind_and_keeps_each_answer_at_its_own_index` [script] | `dialogs-and-commands.md#fields-that-are-not-text` | Implemented |
| Dialog | resizable dialogs with layout | `DialogDescriptor::resizable` | `test_dialog.cpp` `wire_dialog_window_honors_an_explicit_resizable_opt_in`, `a_presented_dialog_scrolls_when_the_terminal_shrinks_and_recovers_when_it_grows` [script] | `widget-gallery.md#dialogdescriptor` | Implemented (no test resizes an opted-in dialog by its grip and checks the reflow) |
| Message boxes | Info/warning/error/confirm | `MessageBoxKind` | `test_message_box.cpp` `message_box_kind_selects_the_matching_message_text_role` | `widget-gallery.md#messageboxdescriptor` | Implemented |
| Message boxes | standard button sets | `MessageBoxButtons`, `MessageBoxResult` | `test_message_box.cpp` `escape_on_a_yes_no_cancel_box_fires_cancel` [script], `exec_message_box_returns_the_pressed_result_in_a_headless_script` [script] | `dialogs-and-commands.md#standard-message-boxes-and-strings` | Implemented |
| Message boxes | Localizable via string table | `StandardStrings` passed to `make_message_box`/`present_message_box` | `test_standard_strings.cpp` `standard_dialog_factories_use_the_supplied_localized_string_table` | `dialogs-and-commands.md#standard-message-boxes-and-strings` | Implemented |
| Message boxes | theme-role styled severity accents | `ckv.message.{info,warning,error,confirm}.text` | `test_message_box.cpp` `message_box_kind_selects_the_matching_message_text_role` | none names the roles | Implemented (the accent is the message text role only) |

### Menu system (M5)

| Row | Feature (catalog wording) | Implementation | Tests | Docs | State |
|---|---|---|---|---|---|
| Menu bar | F10/Alt activation | `MenuBar::activate` (kMenu default handler), `install_menu_accelerators` | `test_menu.cpp` `the_whole_menu_system_is_operable_by_keyboard_alone` [script], `f10_restarts_top_level_mnemonic_navigation_after_a_previous_menu` [script], `alt_mnemonic_opens_its_menu_without_the_bar_being_active_first` [script] | `widget-gallery.md#menubar` | Implemented (a bare Alt tap is a standalone key and opens nothing, `key.hpp`) |
| Menu bar | mnemonic letters | `MenuBar::on_key` | `test_menu.cpp` `mnemonic_letter_opens_the_matching_top_level_menu` [script], `menu_mnemonics_use_the_shared_hotkey_accent_on_each_surface` | `widget-gallery.md#menubar` | Implemented |
| Menu bar | arrow navigation | `MenuBar::on_key` | `test_menu.cpp` `left_and_right_move_the_highlighted_menu_and_wrap` [script], `the_menu_bar_ends_are_its_first_and_last_menu` [script] | `widget-gallery.md#menubar` | Implemented |
| Menu bar | drop-down on Enter/Down/hover-while-open | `MenuBar::open_dropdown`, pointer navigation | `test_menu.cpp` `enter_opens_a_dropdown_as_a_desktop_popup` [script], `a_keyboard_opened_menu_selects_its_first_item_at_once` [script], `the_whole_menu_system_is_operable_by_mouse_alone` [script] | `widget-gallery.md#menubar` | Implemented |
| Menu bar | disabled items | inert, reachable rows | `test_menu.cpp` `clicking_a_disabled_item_does_not_activate_it`, `the_whole_menu_system_is_operable_by_keyboard_alone` [script] | `widget-gallery.md#menuitem` | Implemented |
| Menu bar | Overflow behavior on narrow terminals | `MenuBar::draw` clips at the bar's width; dropdowns are clamped and elided | `test_menu.cpp` `a_narrow_menu_bar_clips_visible_items_to_its_own_width`, `a_popup_wider_than_its_desktop_is_narrowed_to_it_and_elides_its_labels` | none | Partial: titles past the width are clipped (still walkable, not visible); no overflow affordance |
| Menu bar | live structure — runtime add/remove of items and whole top-level menus, safe while a dropdown is open | `MenuBar::set_menus` (whole replacement; closes an open dropdown) | `test_wp35.cpp` `menu_bar_replaces_live_structure_and_closes_an_open_dropdown`; `test_menu.cpp` `menu_accelerators_are_withdrawn_when_the_menus_are_replaced` | none | Implemented as whole-structure replacement |
| Drop-down menu | **Shadow** | `DropdownMenu::casts_shadow` | `test_application_render.cpp` `dropdown_menu_shadows_follow_the_retained_popup_layer` [script] | `performance.md#performance-verification` | Implemented |
| Drop-down menu | separators | `MenuItem::separator` | `test_menu.cpp` `navigation_skips_separators_but_stops_on_unavailable_rows`, `mouse_release_on_a_divider_cancels_the_pending_activation` | `widget-gallery.md#menuitem` | Implemented |
| Drop-down menu | nested submenus | `MenuItem::submenu`, `DropdownMenu::open_submenu` | `test_menu.cpp` `right_enters_the_submenu_and_the_arrows_then_move_inside_it` [script], `the_whole_menu_system_is_operable_by_mouse_alone` [script] | `widget-gallery.md#dropdownmenu` | Implemented |
| Drop-down menu | right-aligned accelerator hints | `DropdownMenu::item_chord_hint` | `test_menu.cpp` `an_item_referencing_a_command_with_a_bound_chord_shows_it_right_aligned`, `a_submenu_marker_and_a_chord_hint_end_at_the_same_column` | `widget-gallery.md#menuitem` | Implemented |
| Drop-down menu | checkable items | `MenuMark`, `MenuItem::with_mark`, `with_mark_provider` | `test_menu.cpp` `checkable_items_render_a_stable_check_column`, `the_whole_menu_system_is_operable_by_keyboard_alone` [script] | `widget-gallery.md#menumark` | Implemented |
| Drop-down menu | disabled items | `DropdownMenu::item_enabled` | `test_menu.cpp` `a_command_row_cannot_be_told_it_is_available_when_its_command_is_not`, `the_whole_menu_system_is_operable_by_mouse_alone` [script] | `widget-gallery.md#menuhighlight` | Implemented |
| Drop-down menu | mnemonic jump | `DropdownMenu::on_key` | `test_menu.cpp` `menu_bar_routes_item_mnemonics_to_its_open_dropdown` [script], `a_submenu_mnemonic_reaches_the_submenus_own_items` [script] | `widget-gallery.md#dropdownmenu` | Implemented |
| Drop-down menu | Esc closes one level | `DropdownMenu::on_key` (routes to the innermost open menu); `MenuBar::on_key` | `test_menu.cpp` `escape_inside_a_submenu_closes_only_the_submenu` [script], `right_arrow_opens_a_nested_submenu_and_escape_closes_one_level` [script], `the_whole_menu_system_is_operable_by_keyboard_alone` [script] | `widget-gallery.md#dropdownmenu` | Implemented for submenus; from a bar's top-level dropdown Esc leaves the menu system (documented in `MenuBar::on_key`) |
| Drop-down menu | Commands-driven: items render chord/enablement live from the command registry and may carry an explicit, surface-specific display label/mnemonic | `MenuItem::command(CommandPresentation)` | `test_menu.cpp` `a_runtime_rebind_changes_the_rendered_chord_hint_without_touching_the_item`; `test_wp35.cpp` `menu_and_status_present_one_command_with_surface_specific_labels_and_same_handler` | `dialogs-and-commands.md#command-presentation` | Implemented |
| Context menu | Positional pop-up with same feature set | `show_context_menu` (a `DropdownMenu`) | `test_menu.cpp` `a_context_menu_receives_dispatched_keys_and_restores_focus` [script], `the_whole_menu_system_is_operable_by_mouse_alone` [script], `home_and_end_act_on_the_submenu_the_reader_is_standing_in` [script] | `editor.md#context-menus` | Implemented |
| Context menu | Keyboard invocation (Menu key / Shift-F10) at focus location | `is_keyboard_context_menu_request`, `show_context_menu_for_focus` | `test_menu.cpp` `the_whole_menu_system_is_operable_by_keyboard_alone` [script], `show_context_menu_for_focus_opens_at_the_focused_views_cell` | `editor.md#context-menus` | Partial: Shift+F10 works through the application's own binding; the physical Menu key is not accepted (`menu.hpp`) |
| Status line | Context-sensitive hints bound to focus | `StatusLine::set_hint_provider`, `current_hint`, `set_context_items` | `test_status_line.cpp` `hint_updates_when_focus_moves_to_a_different_view`, `the_items_follow_the_focused_views_command_context` | `widget-gallery.md#statusline` | Implemented |
| Status line | clickable items | release-committed item press | `test_status_line.cpp` `a_status_item_acts_on_release_and_can_be_taken_back`, `a_scripted_status_line_runs_its_command_by_click_and_by_chord_in_an_application_shell` [script] | `widget-gallery.md#statusline` | Implemented |
| Status line | command chords displayed | `StatusLine` chord labels from the registry | `test_status_line.cpp` `an_item_without_a_stated_chord_still_asks_the_registry`, `command_chords_automatically_use_the_shared_hotkey_accent` | `widget-gallery.md#statusline` | Implemented |
| Status line | Command presentation labels independent of menu wording | `StatusLineItem(CommandPresentation)` | `test_wp35.cpp` `menu_and_status_present_one_command_with_surface_specific_labels_and_same_handler`; `test_status_line.cpp` `a_surface_stated_chord_replaces_the_registrys_and_keeps_its_accent` | `dialogs-and-commands.md#command-presentation` | Implemented |
| Status line | priority-based item layout on narrow widths | `StatusLineItem::priority`, `StatusLine::visible_items` | `test_status_line.cpp` `narrow_status_line_keeps_higher_priority_items_visible_and_clickable`, `equal_priority_status_items_keep_a_visible_prefix_of_the_final_item` | none | Implemented |
| Menu behavior contract | bar activation never steals unsaved state | `MenuBar::activate`/`deactivate` focus bookmark | `test_menu.cpp` `the_whole_menu_system_is_operable_by_keyboard_alone` [script] (the typed field keeps its text and focus), `choosing_an_item_hands_focus_back_before_the_command_runs` [script] | `widget-gallery.md#menubar` | Implemented |
| Menu behavior contract | navigation wraps | `DropdownMenu::step_selection`, `MenuBar::on_key` | `test_menu.cpp` `down_arrow_moves_to_the_next_item_and_wraps`, `left_and_right_move_the_highlighted_menu_and_wrap` [script] | `widget-gallery.md#dropdownmenu` | Implemented |
| Menu behavior contract | arrows stop on disabled items, which stay visible and show their reason (D-083) | `DropdownMenu::item_reachable` | `test_menu.cpp` `navigation_skips_separators_but_stops_on_unavailable_rows`, `a_dropdown_opens_on_its_first_row_even_when_that_row_is_unavailable` | `widget-gallery.md#dropdownmenu` | Implemented (D-083) |
| Menu behavior contract | accelerators fire without opening menus | command keymap fallback in `Application::dispatch` | `test_menu.cpp` `the_whole_menu_system_is_operable_by_keyboard_alone` [script] (Ctrl+S with no popup); `test_application.cpp` `a_key_unconsumed_by_the_focus_chain_falls_through_to_a_bound_command` [script] | `standard-commands.md#the-set` | Implemented |
| Menu behavior contract | every menu action is reachable by keyboard alone and by mouse alone | bar, dropdown, submenu and context-menu input paths | `test_menu.cpp` `the_whole_menu_system_is_operable_by_keyboard_alone` [script], `the_whole_menu_system_is_operable_by_mouse_alone` [script] | `widget-gallery.md#menubar` | Implemented |

### Controls (M4 minimal set; M6 completion)

| Row | Feature (catalog wording) | Implementation | Tests | Docs | State |
|---|---|---|---|---|---|
| Label | Mnemonic that focuses its buddy control | `Label::set_buddy`, `activate_label_mnemonic` | `test_widgets.cpp` `alt_mnemonic_on_a_window_focuses_the_labels_buddy` [script], `label_buddy_returns_null_after_the_buddy_is_destroyed` | `widget-gallery.md#label` | Implemented |
| Static text | Styled | `StaticText` (`ckv.static.text`), `set_role_override`, `set_emphasized_leading_lines` | `test_widgets.cpp` `appearance_setters_repaint_and_a_width_changing_one_reports_its_size` | `widget-gallery.md#statictext` | Implemented (no test asserts the drawn emphasis) |
| Static text | wrapped | `StaticText::wrap`, `height_for_width` | `test_widgets.cpp` `static_text_wraps_at_word_boundaries_when_width_is_tight`; `test_wp36.cpp` `wrapped_static_text_reallocates_height_through_a_hosted_dialog_content_tree` [script] | `widget-gallery.md#statictext` | Implemented |
| Static text | Alignment options | `StaticText::set_alignment` | `test_widgets.cpp` `static_text_center_alignment_offsets_each_wrapped_line`, `static_text_end_alignment_offsets_each_wrapped_line` | `widget-gallery.md#statictext` | Implemented |
| Button | Default/normal | `Button::set_default` | `test_widgets.cpp` `a_default_buttons_face_uses_the_default_style_when_unfocused`, `a_focused_buttons_face_uses_the_focused_style` | `widget-gallery.md#button` | Implemented |
| Button | mnemonic | `Button::activate_mnemonic` | `test_widgets.cpp` `button_mnemonics_use_the_dialog_mnemonic_accent`; `test_disabled_state.cpp` `a_disabled_button_ignores_its_mnemonic` | `widget-gallery.md#button` | Implemented |
| Button | pressed feedback | `Button::pressed`, `on_key_release` | `test_widgets.cpp` `a_pressed_button_shifts_its_face_right_and_loses_its_shadow`, `a_held_enter_keeps_the_button_down_and_acts_only_on_release`; `test_disabled_state.cpp` `a_press_on_a_disabled_button_never_fires_it_and_reaches_its_enabled_container` [script] | `widget-gallery.md#button` | Implemented |
| Button | Repeat-on-hold option | none; a legacy terminal's keyboard auto-repeat re-fires `on_press` | `test_widgets.cpp` `button_fires_on_press_on_enter_key_repeat_but_not_release` (the legacy re-fire only) | none | Missing: no option and no pointer repeat |
| Input line | Editing keys | `InputLine::on_key` | `test_widgets.cpp` `typed_text_inserts_at_the_cursor`, `backspace_removes_the_grapheme_before_the_cursor`, `a_scripted_input_line_edits_selects_copies_recalls_and_undoes_through_dispatched_keys` [script] | `widget-gallery.md#inputline` | Implemented |
| Input line | selection | `InputLine::has_selection`, `selection_range`, pointer drag | `test_widgets.cpp` `shift_left_extends_a_selection_and_plain_left_collapses_it`, `mouse_drag_extends_the_input_line_selection`; `test_m4_integration.cpp` `a_two_field_dialog_driven_headlessly_end_to_end` [script] | `widget-gallery.md#inputline` | Implemented |
| Input line | clipboard | `copy_selection_to_clipboard`, `cut_selection_to_clipboard`, `paste_from_clipboard` | `test_widgets.cpp` `input_line_copy_cut_and_paste_use_the_application_clipboard`, `a_scripted_input_line_edits_selects_copies_recalls_and_undoes_through_dispatched_keys` [script] | `widget-gallery.md#inputline` | Implemented |
| Input line | overwrite/insert | `InputLine::overwrite_mode` (Insert toggles) | `test_widgets.cpp` `overwrite_mode_replaces_the_grapheme_under_the_cursor_instead_of_inserting`, `a_scripted_input_line_edits_selects_copies_recalls_and_undoes_through_dispatched_keys` [script] | `widget-gallery.md#inputline` | Implemented |
| Input line | scrolling content | horizontal scroll to the cursor | `test_widgets.cpp` `long_text_scrolls_so_the_cursor_stays_visible`, `an_input_line_exactly_as_wide_as_its_text_shows_all_of_it` | `widget-gallery.md#inputline` | Implemented |
| Input line | validators | `InputLine::set_validator`, `set_valid` | `test_widgets.cpp` `validator_runs_automatically_after_every_edit`, `external_set_valid_is_overridden_by_the_next_edit_when_a_validator_is_installed` | `widget-gallery.md#inputline` | Implemented |
| Input line | input masks | `InputLine::set_mask` | `test_widgets.cpp` `mask_prefills_literals_and_placeholders`, `typing_into_a_mask_only_accepts_matching_characters_and_auto_advances` | `widget-gallery.md#inputline` | Implemented |
| Input line | password echo | `InputLine::set_password_echo` | `test_widgets.cpp` `password_echo_hides_content_but_editing_still_affects_the_real_text` | `widget-gallery.md#inputline` | Implemented |
| Input line | history cycling | `InputLine::set_history`, `commit_to_history` | `test_widgets.cpp` `history_up_cycles_to_the_most_recent_entry_and_down_restores_the_live_text`, `a_scripted_input_line_edits_selects_copies_recalls_and_undoes_through_dispatched_keys` [script] | `widget-gallery.md#inputline` | Implemented |
| Input line | Grapheme-correct editing (never splits clusters) | grapheme-indexed cursor | `test_widgets.cpp` `grapheme_correct_editing_never_splits_a_multi_byte_cluster` | `widget-gallery.md#inputline` | Implemented (tested with a precomposed cluster only) |
| Input line | undo within field | `InputLine::undo` (Ctrl+Z, bounded) | `test_widgets.cpp` `input_line_undo_restores_the_previous_text_and_selection_state`, `a_scripted_input_line_edits_selects_copies_recalls_and_undoes_through_dispatched_keys` [script] | `widget-gallery.md#inputline` | Implemented |
| Input line | shared Ctrl/C/V/X, Ctrl+Insert/Shift+Insert, Ctrl word navigation/deletion, and Shift selection semantics | `InputLine::on_key` | `test_widgets.cpp` `input_line_uses_word_navigation_and_legacy_insert_clipboard_bindings`, `a_scripted_input_line_edits_selects_copies_recalls_and_undoes_through_dispatched_keys` [script] (Ctrl+Backspace, Ctrl+Left, Shift+End, Ctrl+C) | `widget-gallery.md#inputline` | Implemented |
| Key chord capture | Focused, command-safe capture of one normalized `KeyChord` | `KeyChordCapture` | `test_key_chord_capture.cpp` `a_scripted_chord_capture_shadows_the_bound_command_only_while_it_captures` [script], `key_chord_capture_consumes_the_captured_command_chord` | `widget-gallery.md#keychordcapture` | Implemented |
| Key chord capture | Enter/Space starts capture | `KeyChordCapture::on_key` | `test_key_chord_capture.cpp` `a_scripted_chord_capture_shadows_the_bound_command_only_while_it_captures` [script] | `widget-gallery.md#keychordcapture` | Implemented |
| Key chord capture | Escape abandons it | `KeyChordCapture::cancel_capture` | `test_key_chord_capture.cpp` `key_chord_capture_esc_cancels_capture_without_changing_the_binding`, `a_scripted_chord_capture_shadows_the_bound_command_only_while_it_captures` [script] | `widget-gallery.md#keychordcapture` | Implemented |
| Key chord capture | Backspace/Delete clears it | `KeyChordCapture::clear` | `test_key_chord_capture.cpp` `key_chord_capture_clear_is_explicit_and_typed`, `a_scripted_chord_capture_shadows_the_bound_command_only_while_it_captures` [script] (Delete) | `widget-gallery.md#keychordcapture` | Implemented |
| Key chord capture | Application-owned conflict policy and persistence; the widget only captures a typed input value | `KeyChordCapture::on_chord_changed` | `test_key_chord_capture.cpp` `key_chord_capture_clear_is_explicit_and_typed` | `widget-gallery.md#keychordcapture` | Implemented |
| Memo | Multi-line editing | `Memo` | `test_memo.cpp` `enter_splits_the_current_line_into_two`, `a_scripted_memo_edits_scrolls_copies_and_undoes_through_dispatched_keys` [script] | `widget-gallery.md#memo` | Implemented |
| Memo | scrolling | embedded scrollbars, `left_column` | `test_memo.cpp` `a_scripted_memo_edits_scrolls_copies_and_undoes_through_dispatched_keys` [script] (vertical), `the_memo_cursor_stays_on_screen_horizontally_as_it_walks_a_long_line` | `widget-gallery.md#memo` | Implemented |
| Memo | selection | Shift selection, pointer drag | `test_memo.cpp` `shift_right_extends_selection_and_typing_over_it_replaces_the_selection`, `mouse_drag_extends_the_memo_selection` | `widget-gallery.md#memo` | Implemented |
| Memo | clipboard | Ctrl+C/X/V | `test_memo.cpp` `copy_cut_and_paste_use_the_application_clipboard`, `a_scripted_memo_edits_scrolls_copies_and_undoes_through_dispatched_keys` [script] | `widget-gallery.md#memo` | Implemented |
| Memo | bounded undo | `Memo::undo` | `test_memo.cpp` `undo_restores_the_previous_memo_text_cursor_and_selection`, `a_scripted_memo_edits_scrolls_copies_and_undoes_through_dispatched_keys` [script] | `widget-gallery.md#memo` | Implemented |
| Memo | descriptor-form field with accept-time validation | `FieldKind::Memo`, `validate_dialog` | `test_dialog.cpp` `a_memo_field_materializes_as_a_labeled_multiline_editor`, `a_memo_field_participates_in_dialog_validation_and_recovers_after_editing` | `dialogs-and-commands.md#fields-that-are-not-text` | Implemented |
| Memo | Word wrap toggle | `Memo::set_wrap_mode` | `test_memo.cpp` `a_memo_offers_all_three_wrap_modes_and_word_wraps_by_default`, `wrapping_draws_soft_rows_without_changing_the_document_text` | `widget-gallery.md#memo` | Implemented |
| Memo | grapheme-correct navigation | grapheme-indexed cursor | `test_memo.cpp` `a_grapheme_correct_backspace_removes_a_whole_multi_byte_cluster` | `widget-gallery.md#memo` | Implemented (tested with a precomposed cluster only) |
| Memo | shared Ctrl/C/V/X, Ctrl+Insert/Shift+Insert, Ctrl word/document navigation/deletion, and Shift selection semantics | `Memo::on_key` | `test_memo.cpp` `memo_uses_word_and_document_navigation_with_legacy_insert_clipboard_bindings`, `a_scripted_memo_edits_scrolls_copies_and_undoes_through_dispatched_keys` [script] (Ctrl+Home, Ctrl+Backspace) | `widget-gallery.md#memo` | Implemented |
| Check group | Multi-select | `CheckGroup` | `test_option_group.cpp` `multiple_items_can_be_checked_independently`, `space_toggles_the_current_item` | `widget-gallery.md#checkgroup` | Implemented |
| Check group | arrow navigation | `CheckGroup::on_key` | `test_option_group.cpp` `down_arrow_moves_the_cursor_and_toggling_affects_the_new_position`, `left_and_right_step_through_a_row_and_up_and_down_are_not_its_own` | `widget-gallery.md#checkgroup` | Implemented |
| Check group | mnemonics | item and caption mnemonics | `test_option_group.cpp` `alt_mnemonic_key_toggles_the_matching_check_item_but_ctrl_does_not`, `alt_and_the_caption_letter_focus_the_group_from_anywhere_in_its_window` [script] | `widget-gallery.md#checkgroup` | Implemented |
| Check group | Tri-state option | `CheckState::Mixed`, `set_tristate` | `test_option_group.cpp` `tristate_check_group_cycles_through_checked_mixed_and_unchecked`, `on_state_changed_reports_mixed_without_losing_the_existing_bool_callback` | `widget-gallery.md#checkgroup` | Implemented |
| Radio group | Exclusive select | `RadioGroup` | `test_option_group.cpp` `selecting_one_item_deselects_the_previous_one`, `clicking_a_row_selects_that_item_exclusively` | `widget-gallery.md#radiogroup` | Implemented |
| Radio group | arrow navigation | `RadioGroup::on_key` | `test_option_group.cpp` `arrow_navigation_also_changes_selection`, `up_and_down_move_within_a_column_and_wrap_there` | `widget-gallery.md#radiogroup` | Implemented |
| Radio group | mnemonics | item and caption mnemonics | `test_option_group.cpp` `alt_mnemonic_key_selects_the_matching_radio_item_but_ctrl_does_not`, `alt_and_the_caption_letter_focus_the_group_from_anywhere_in_its_window` [script] | `widget-gallery.md#radiogroup` | Implemented |
| List view | Provider-backed scrolling | `ListModel`, `ListView::set_model` | `test_list_view.cpp` `provider_backed_list_queries_only_the_visible_slice`, `a_scripted_list_view_searches_walks_and_activates_by_key_and_by_timed_double_click` [script] | `data-views.md#list-providers` | Implemented |
| List view | stable single/multi selection | `ListView::selected_ids`, `set_selected_id` | `test_list_view.cpp` `provider_backed_list_preserves_cursor_and_selection_identity_across_reorder`, `multi_select_space_toggles_independently` | `data-views.md#list-providers` | Implemented |
| List view | keyboard search | type-ahead, `ListModel::find_prefix` | `test_list_view.cpp` `letters_typed_together_form_one_prefix_and_the_same_letter_again_steps_on`, `a_scripted_list_view_searches_walks_and_activates_by_key_and_by_timed_double_click` [script] | `widget-gallery.md#listview` | Implemented |
| List view | injected-Clock double-click activate | double-click interval on the application's Clock | `test_list_view.cpp` `a_second_click_after_the_clock_threshold_only_moves_the_cursor`, `a_scripted_list_view_searches_walks_and_activates_by_key_and_by_timed_double_click` [script] | none (header only) | Implemented |
| List view | Application-defined row presentation and asynchronous cache policy; provider contract in D-043 | `ListItem::style`; D-043 synchronous provider with `model_changed` | `test_list_view.cpp` `a_styled_row_still_shows_the_cursor_and_the_selection`, `provider_backed_list_preserves_cursor_and_selection_identity_across_reorder` | `data-views.md#ownership-and-threading` | Implemented |
| Combo box | Editable and pick-only variants | `ComboBoxMode` | `test_combo_box.cpp` `a_combo_drops_a_popup_list_and_takes_the_row_chosen_from_it` [script], `editable_combo_uses_the_standard_text_editing_keymap` | `widget-gallery.md#combobox` | Implemented |
| Combo box | history integration | `ComboBox::set_history`, `commit_to_history` | `test_combo_box.cpp` `editable_combo_accepts_text_and_uses_history_registry` | none | Implemented |
| Combo box | dropdown with shadow | `ComboBox::open_dropdown` over `show_popup_list` | `test_combo_box.cpp` `a_combo_drops_a_popup_list_and_takes_the_row_chosen_from_it` [script], `escape_closes_a_combos_list_without_changing_what_it_holds` [script] | `widget-gallery.md#popuplist` | Partial: the dropdown is a real popup, but `PopupList` casts no shadow (`View::casts_shadow` default) |
| History registry (paragraph) | Input line and Combo box histories live in the application-scoped history registry — shared by key across widgets and dialogs, never per-widget state | `Application::history`, `HistoryRegistry`; widgets borrow any registry pointer | `test_widgets.cpp` `a_scripted_input_line_edits_selects_copies_recalls_and_undoes_through_dispatched_keys` [script] (on `Application::history`); `test_history.cpp` `distinct_keys_have_independent_lists` | none | Partial: widgets take any borrowed registry rather than the application's; descriptor dialogs cannot name a history key |
| Scrollbar | Proportional thumb | `Scrollbar::thumb_length` | `test_scrollbar.cpp` `the_thumb_length_reports_how_much_of_the_content_is_visible`, `a_scripted_scrollbar_steps_pages_and_drags_through_dispatched_input` [script] | `widget-gallery.md#scrollbar` | Implemented |
| Scrollbar | arrows | arrow cells, arrow keys | `test_scrollbar.cpp` `clicking_the_end_arrow_increments_by_one`, `a_scripted_scrollbar_steps_pages_and_drags_through_dispatched_input` [script] | `widget-gallery.md#scrollbar` | Implemented |
| Scrollbar | paging | PageUp/PageDown, trough clicks | `test_scrollbar.cpp` `clicking_the_track_below_the_thumb_pages_toward_the_end`, `a_scripted_scrollbar_steps_pages_and_drags_through_dispatched_input` [script] | `widget-gallery.md#scrollbar` | Implemented |
| Scrollbar | drag | thumb drag | `test_scrollbar.cpp` `dragging_the_thumb_moves_position_proportionally`, `a_scripted_scrollbar_steps_pages_and_drags_through_dispatched_input` [script] | `widget-gallery.md#scrollbar` | Implemented |
| Scrollbar | Auto-hide policy option | `ScrollbarPolicy`, `Scrollbar::set_policy` | `test_scrollbar.cpp` `the_scrollbar_policy_decides_visibility_from_whether_content_fits`; `test_scroll_viewport.cpp` `vertical_auto_hide_reclaims_horizontal_content_space_when_only_horizontal_scroll_is_unneeded` | `widget-gallery.md#scrollviewport` | Implemented |
| Scroll viewport | Scrolls any content view | `ScrollViewport::set_content`, `set_scroll` | `test_scroll_viewport.cpp` `content_larger_than_the_viewport_positions_at_a_negative_offset_when_scrolled`, `content_that_grows_after_layout_re_measures_the_viewport` | `widget-gallery.md#scrollviewport` | Implemented |
| Scroll viewport | keyboard and wheel | `ScrollViewport::on_key`, `on_mouse`; wheel bubbling in `Application::dispatch` | `test_scroll_viewport.cpp` `page_down_scrolls_by_the_viewports_visible_height`, `application_routes_wheel_events_over_descendant_content_to_the_scroll_viewport` [script] | `widget-gallery.md#scrollviewport` | Implemented |
| Scroll viewport | Semantic cell-line scrolling | cell-unit `set_scroll`, `ensure_visible` (D-041) | `test_scroll_viewport.cpp` `ensure_visible_scrolls_the_least_it_can_and_only_when_it_must`, `set_scroll_clamps_to_the_maximum_reachable_offset` | `widget-gallery.md#scrollviewport` | Implemented |
| Scroll viewport | scroll-region byte-diff optimization is renderer/performance scope per D-041 | not widget scope (D-041) | — | — | Not applicable |
| Tab control | Top tabs | row-0 caption strip | `test_tab_control.cpp` `tab_control_draws_tab_labels`, `a_scripted_tab_control_switches_pages_by_arrows_mnemonics_and_clicks` [script] | `widget-gallery.md#tabcontrol` | Implemented |
| Tab control | with mnemonics | `TabControl::Tab::mnemonic`, Alt+letter | `test_tab_control.cpp` `tab_control_keyboard_cycles_and_alt_mnemonics_activate_tabs`, `a_scripted_tab_control_switches_pages_by_arrows_mnemonics_and_clicks` [script] | `widget-gallery.md#tabcontrol` | Implemented |
| Tab control | keyboard cycling | Left/Right, wrapping | `test_tab_control.cpp` `tab_and_shift_tab_are_left_for_focus_traversal`, `a_scripted_tab_control_switches_pages_by_arrows_mnemonics_and_clicks` [script] | `widget-gallery.md#tabcontrol` | Implemented |
| Tab control | per-tab pages | `TabControl::add_tab`, `active_page` | `test_tab_control.cpp` `tab_control_owns_pages_and_shows_only_the_active_page`, `tab_control_resizes_active_page_below_the_tab_row` | `widget-gallery.md#tabcontrol` | Implemented |
| Tab control | overflow | `TabControl::draw` clips at the right edge | none | none | Partial: captions past the right edge are clipped (`tab_control.hpp`); the active caption is not kept visible and nothing marks the overflow |
| Table | Provider-backed rows | `TableModel`, `Table::set_model` | `test_table.cpp` `provider_backed_table_queries_only_visible_cells` | `data-views.md#table-providers` | Implemented |
| Table | typed editable cells | `TableCellType`, `begin_edit`/`commit_edit`, `TableModel::commit` | `test_table.cpp` `provider_backed_table_commits_a_typed_edit_to_its_model`, `a_scripted_table_sorts_on_a_header_click_and_edits_a_typed_cell_from_the_keyboard` [script] | `widget-gallery.md#table` | Implemented |
| Table | stable row/cell selection | `TableRowId`, `TableCellRef`, `selected_cell` | `test_table.cpp` `provider_backed_table_preserves_the_selected_cell_through_reorder`, `sorting_preserves_selection_identity_not_just_the_display_index` | `widget-gallery.md#tablecellref` | Implemented |
| Table | headers | `TableColumn::title`, sort glyph | `test_table.cpp` `clicking_a_header_sorts_ascending_and_clicking_again_reverses_it`, `a_scripted_table_sorts_on_a_header_click_and_edits_a_typed_cell_from_the_keyboard` [script] | `widget-gallery.md#tablecolumn` | Implemented |
| Table | resize | header-boundary drag, `TableColumn::min_width` | `test_table.cpp` `dragging_a_column_boundary_resizes_that_column`, `resizing_below_the_columns_minimum_width_clamps` | none (header only) | Implemented |
| Table | sort requests | `Table::sort_by`, `TableModel::request_sort` | `test_table.cpp` `a_model_sort_is_reported_whether_or_not_a_cell_is_selected`, `sort_by_reorders_display_rows_without_touching_underlying_storage` | `data-views.md#table-providers` | Implemented |
| Table | cell styling; application-defined range/formula/database semantics per D-043 | `TableCell::style`, `set_cell_style_hook` | `test_table.cpp` `a_style_hook_colours_the_cell_and_the_cursor_is_drawn_over_it`, `cell_style_hook_receives_the_underlying_row_index_not_the_display_index` | `widget-gallery.md#table` | Implemented |
| Tree / outline | Expand/collapse | `TreeView` | `test_tree_view.cpp` `right_arrow_expands_a_collapsed_node_with_children_without_moving_the_cursor`, `a_scripted_tree_view_expands_walks_and_activates_through_dispatched_input` [script] | `widget-gallery.md#treeview` | Implemented |
| Tree / outline | keyboard navigation | `TreeView::on_key` | `test_tree_view.cpp` `home_end_and_paging_move_through_the_visible_rows`, `a_scripted_tree_view_expands_walks_and_activates_through_dispatched_input` [script] | `widget-gallery.md#treeview` | Implemented |
| Tree / outline | stable-ID provider model | `TreeModel` | `test_tree_view.cpp` `provider_tree_reveals_stable_id_and_preserves_it_through_a_refresh`, `provider_tree_requests_unknown_children_once_and_keeps_expansion_in_the_view` | `data-views.md#tree-providers` | Implemented |
| Tree / outline | Connector-line styles | `TreeConnectorStyle`, `set_connector_style` | `test_tree_view.cpp` `ascii_connector_style_draws_printable_branch_prefixes`, `outline_connector_style_draws_junctioned_group_and_leaf_branches` | `widget-gallery.md#treeview` | Implemented |
| Tree / outline | compact `TreeNode` values remain available | `TreeView::set_roots` | `test_tree_view.cpp` `setting_roots_selects_the_first_top_level_node` | `widget-gallery.md#treenode` | Implemented |
| Progress | Determinate/indeterminate | `Progress::set_fraction`, `set_indeterminate`, `set_pulse` | `test_progress.cpp` `progress_supports_indeterminate_pulse_state`, `a_scripted_progress_bar_shows_each_state_its_host_sets_between_steps` [script] | `widget-gallery.md#progress` | Implemented |
| Progress | label slot | `Progress::set_label` | `test_progress.cpp` `progress_draws_the_label_slot`, `a_scripted_progress_bar_shows_each_state_its_host_sets_between_steps` [script] | `widget-gallery.md#progress` | Implemented |
| Splitter | Keyboard-adjustable split panes | `Splitter::on_key`, `set_split_position`, divider drag | `test_splitter.cpp` `left_and_right_move_a_horizontal_splitters_split_position_by_one`, `a_scripted_splitter_moves_its_divider_by_keys_and_by_a_dispatched_drag` [script] | `widget-gallery.md#splitter` | Implemented |
| Text view | Read-only styled text | `TextView::set_spans` | `test_text_view.cpp` `draw_applies_inline_and_link_styles` | `widget-gallery.md#textview` | Implemented |
| Text view | scrolling | `TextView::on_key`, wheel | `test_text_view.cpp` `page_down_scrolls_by_the_visible_height`, `a_scripted_text_view_scrolls_and_follows_links_through_dispatched_input` [script] | `widget-gallery.md#textview` | Implemented |
| Text view | Links (activatable; rendered as OSC 8 hyperlinks where supported) | `TextSpan::link_target`, `on_link_activate`, `osc8_text` | `test_text_view.cpp` `a_scripted_text_view_scrolls_and_follows_links_through_dispatched_input` [script], `osc8_text_wraps_link_spans_and_strips_control_bytes` | `widget-gallery.md#textview` | Partial: links activate by key and click; OSC 8 exists only as the `osc8_text()` export string, and the presenter never emits OSC 8 |
| Text view | simple inline styles | `TextSpan::attrs` | `test_text_view.cpp` `draw_applies_inline_and_link_styles` | `widget-gallery.md#textview` | Implemented |
| Flow view | Read-only wrapped styled content | `FlowView`, `FlowDocument` | `test_flow_view.cpp` `flow_view_wraps_styled_text_without_splitting_a_row` | `flow-view.md` | Implemented |
| Flow view | scrolling | `FlowView::on_key`, wheel | `test_flow_view.cpp` `flow_view_scrolls_wrapped_display_rows`, `a_scripted_flow_view_scrolls_and_activates_links_by_key_by_click_and_by_wheel` [script] | `flow-view.md` | Implemented |
| Flow view | keyboard/mouse link activation | `FlowView::on_link_activate` | `test_flow_view.cpp` `flow_view_exposes_link_navigation_and_activation`, `a_scripted_flow_view_scrolls_and_activates_links_by_key_by_click_and_by_wheel` [script] | `flow-view.md` | Implemented |
| Flow view | Inline raster atoms with mandatory fallback and ordinary scene composition | `FlowImage::fallback` | `test_flow_view.cpp` `flow_view_places_an_inline_image_through_the_scene_raster_path`, `flow_view_raster_uses_sixel_when_available_and_its_text_fallback_when_not` | `flow-view.md#inline-raster-atoms` | Implemented |
| Flow view | semantic document/chat models remain client-owned (D-043) | `FlowView::set_document`, `replace_block` | `test_flow_view.cpp` `flow_view_replaces_one_block_without_rebuilding_the_document_value` | `flow-view.md#updating-one-block` | Implemented |
| Text editor | Revisioned UTF-8 document editing | `EditorDocument` | `test_editor_document.cpp` `editor_document_positions_are_revision_bound_and_grapheme_safe`; `test_text_editor.cpp` `text_editor_edits_shared_document_and_reports_status` | `editor.md#positions-and-edits` | Implemented |
| Text editor | selections | `TextEditor` selection | `test_text_editor.cpp` `text_editor_extends_and_collapses_selection_for_cursor_home_end_and_document_navigation`; `test_editor_window.cpp` `a_scripted_editor_window_marks_typed_changes_and_keeps_its_close_control_vetoed` [script] | `editor.md` | Implemented |
| Text editor | undo/redo | `EditorDocument::undo`, `redo` | `test_editor_document.cpp` `editor_document_undo_and_redo_restore_persistent_roots`; `test_text_editor.cpp` `text_editor_undo_and_redo_restore_the_selection_where_the_change_was` | `editor.md#commands-and-key-bindings` | Implemented |
| Text editor | scrolling | `TextEditor` viewport | `test_text_editor.cpp` `text_editor_paging_moves_the_caret_and_shift_paging_selects`, `text_editor_drag_selection_auto_scrolls_when_the_pointer_leaves_its_viewport` | `editor.md` | Implemented |
| Text editor | gutter | `TextEditor::set_show_line_numbers` | covered only through the editor example's pinned frames | `editor.md` | Implemented (no direct test) |
| Text editor | line/column indicator | `EditorStatus`, `EditorStatusModel` | `test_text_editor.cpp` `editor_status_model_tracks_an_editor_without_assuming_window_chrome` | `editor.md` | Implemented |
| Text editor | Search/replace | `EditorSearch` | `test_editor_search.cpp` `editor_search_replace_all_is_one_undoable_transaction`; `test_text_editor.cpp` `text_editor_replace_current_and_replace_all_keep_the_document_transactional` | `editor.md#search-and-files` | Implemented |
| Text editor | deterministic profile detection | `SyntaxProfileRegistry` | `test_syntax_profile.cpp` `standard_syntax_profiles_detect_json_yaml_and_bash_deterministically`; `test_text_editor.cpp` `text_editor_uses_file_name_to_select_a_standard_profile` | `editor.md#profiles-and-highlighting` | Implemented |
| Text editor | incremental syntax styling | `SyntaxCache` | `test_syntax_cache.cpp` `syntax_cache_relexes_to_a_fixed_point_without_rehighlighting_an_unchanged_suffix` | `editor.md#profiles-and-highlighting` | Implemented |
| Text editor | composable file-editor workflow | `FileEditorController`, `EditorWindow` | `test_file_editor_controller.cpp` `file_editor_controller_refuses_silent_external_overwrite`; `test_editor_window.cpp` `a_scripted_editor_window_marks_typed_changes_and_keeps_its_close_control_vetoed` [script] | `editor.md#search-and-files` | Implemented |
| Terminal view | Private child PTY/ConPTY session | `TerminalView`, POSIX and Windows subsessions | `test_posix_terminal_subsession.cpp` `posix_terminal_subsessions_are_isolated`; `test_windows_terminal_subsession.cpp` `windows_conpty_input_and_two_sessions_remain_isolated` | `embedded-terminal.md#launching-a-contained-child` | Implemented |
| Terminal view | UTF-8 terminal cells | `TerminalEmulator` | `test_terminal_emulator.cpp` `terminal_emulator_preserves_split_utf8_codepoints_across_child_reads` | `embedded-terminal.md` | Implemented |
| Terminal view | focus/input/resize | `TerminalView` input encoding and sizing | `test_terminal_view.cpp` `terminal_view_encodes_control_meta_and_modified_navigation_for_the_private_child`; `test_posix_terminal_subsession.cpp` `posix_terminal_subsession_propagates_content_resize_to_child` | `embedded-terminal.md` | Implemented |
| Terminal view | scrollback | primary-buffer history | `test_terminal_emulator.cpp` `terminal_emulator_retains_bounded_primary_scrollback`; `test_terminal_view.cpp` `a_wheel_on_the_primary_buffer_walks_the_terminals_own_history` | `embedded-terminal.md#scrolling-a-full-screen-child-with-the-wheel` | Implemented |
| Terminal view | process lifecycle | subsession exit and termination | `test_posix_terminal_subsession.cpp` `posix_terminal_subsession_observes_child_exit`, `posix_terminal_subsession_honours_bounded_termination_after_grace` | `embedded-terminal.md#launching-a-contained-child` | Implemented |
| Terminal view | Alternate buffer | `TerminalEmulator` alternate screen | `test_terminal_emulator.cpp` `terminal_emulator_has_isolated_alternate_buffer` | `embedded-terminal.md` | Implemented |
| Terminal view | mouse/paste/query policy | `TerminalQueryPolicy`, mouse and paste forwarding | `test_terminal_view.cpp` `terminal_view_wraps_paste_only_when_the_child_requests_bracketed_paste`; `test_terminal_emulator.cpp` `terminal_emulator_consumes_nested_capability_queries_at_the_private_boundary` | `embedded-terminal.md#containment-and-policies` | Implemented |
| Terminal view | nested ckVision applications | subsession containment | `test_posix_terminal_subsession.cpp` `posix_terminal_subsession_confines_a_separately_launched_ckvision_application` | `embedded-terminal.md#nested-ckvision-applications` | Implemented |
| Terminal view | decoded Sixel re-composited with no spill | child raster clip and re-encode | `test_terminal_view.cpp` `terminal_view_reencodes_private_child_sixel_only_through_the_outer_presenter` [script]; `test_terminal_emulator.cpp` `terminal_emulator_crops_child_sixel_to_its_visible_cell_footprint` | `embedded-terminal.md#what-a-picture-is-once-it-is-on-screen` | Implemented |
| Help viewer | Context topics (F1 from any view via help-context keys) | `View::set_help_context_key`, `Application::set_help_provider` | `test_application.cpp` `f1_resolves_the_focused_views_help_context_key_and_hands_it_to_the_provider` [script], `f1_walks_ancestors_when_the_focused_view_has_no_help_context_key_of_its_own` [script] | `standard-commands.md#the-set` | Implemented |
| Help viewer | activatable cross-links | a "See also" line of plain text | `test_help_viewer.cpp` `a_topics_curated_cross_references_are_named_in_its_prose` [script] | `widget-gallery.md#memoryhelpprovider` | Partial: cross-references are named, not activatable (`help_viewer.hpp` keeps them as plain text by design) |
| Help viewer | back navigation | Back button over a history | none presses Back after navigating | none | Partial: implemented, no test (`back_with_empty_history_is_a_harmless_no_op` never presses Back) |
| Help viewer | Index/keyword lookup | `HelpProvider::index`, `search`; the SearchBox filter | `test_help_viewer.cpp` `memory_help_provider_keyword_search_matches_key_title_body_and_link_labels`, `enter_in_the_search_box_does_not_dismiss_the_help_window` [script] | `widget-gallery.md#helpprovider` | Implemented |
| Help viewer | content via an injected provider — the library defines no help file format | `HelpProvider`, `MemoryHelpProvider` | `test_help_viewer.cpp` `present_help_viewer_is_modeless_and_completes_on_detachment` [script] | `widget-gallery.md#helpprovider` | Implemented |
| Image view | Shows an `Image` scaled to its cell box | `ImageView::set_image`, `image_anchor` | `test_image_view.cpp` `draw_with_a_real_image_adds_a_raster_region_at_the_widget_cell_anchor`; `test_application.cpp` `a_frame_carrying_a_picture_is_not_followed_by_another_until_the_host_is_ready` [script] | `widget-gallery.md#imageview` | Implemented |
| Image view | mandatory fallback content | `Painter::draw_image` fallback | `test_image_view.cpp` `draw_with_a_real_image_calls_the_mandatory_fallback_and_does_not_crash`, `with_no_image_installed_draw_fills_the_fallback_style_without_crashing` | `graphics.md` | Implemented |
| Image view | dual-space (cell + pixel) mouse events where the terminal reports pixels | `ImageView::on_click` forwards the whole `MouseEvent` | `test_image_view.cpp` `on_click_receives_the_full_mouse_event_including_the_pixel_field_when_present`; `test_input_decoder.cpp` `sgr_pixel_mouse_reports_both_coordinate_spaces` | `graphics.md` | Implemented |
| Canvas | Application draws RGBA at the widget's current pixel size (cell box × cell metrics, redraw on resize) | `Canvas::set_cell_metrics`, `set_draw_callback` | `test_canvas.cpp` `resizing_a_metric_driven_canvas_reallocates_and_redraws_at_the_new_pixel_size`, `a_scripted_canvas_redraws_at_its_new_pixel_size_and_forwards_both_coordinate_spaces` [script] | `widget-gallery.md#canvas` | Implemented |
| Canvas | mandatory fallback painter | `Canvas::set_fallback_painter` | `test_canvas.cpp` `an_installed_fallback_painter_replaces_the_placeholder_name` | `widget-gallery.md#canvas` | Implemented |
| Canvas | dual-space mouse events | `Canvas::on_click` | `test_canvas.cpp` `a_scripted_canvas_redraws_at_its_new_pixel_size_and_forwards_both_coordinate_spaces` [script] | `graphics.md` | Implemented |

### Common modern components (WP-36B)

| Row | Feature (catalog wording) | Implementation | Tests | Docs | State |
|---|---|---|---|---|---|
| Calendar view | Month grid | `CalendarView::draw`, `set_month` | `test_common_components.cpp` `the_week_starts_where_the_reader_expects_it_to`, `a_scripted_calendar_view_moves_by_keys_and_selects_the_clicked_day` [script] | `widget-gallery.md#calendarview` | Implemented |
| Calendar view | keyboard/mouse day selection | `CalendarView::on_key`, `on_mouse`, `on_select` | `test_common_components.cpp` `calendar_selection_is_deterministic_range_aware_and_keyboard_driven`, `a_scripted_calendar_view_moves_by_keys_and_selects_the_clicked_day` [script] | `widget-gallery.md#calendarview` | Implemented |
| Calendar view | range/disabled-date support | `set_range`, `set_disabled_predicate` | `test_common_components.cpp` `calendar_selection_is_deterministic_range_aware_and_keyboard_driven` | `widget-gallery.md#calendarview` | Implemented |
| Calendar view | current-day styling | `set_today`, `set_today_provider` | `test_common_components.cpp` `a_calendar_marks_today_and_stops_when_told_there_is_no_today`, `a_calendar_asks_again_for_today_so_it_can_turn_over_at_midnight` | `widget-gallery.md#calendarview` | Implemented |
| Calendar view | caller-supplied labels | `CalendarView::set_labels` | none | `widget-gallery.md#calendarview` | Implemented (no test) |
| Calendar view | Optional ISO week row | `set_show_iso_week_numbers` (a "Wk" column) | `test_common_components.cpp` `iso_week_numbers_label_each_row_with_the_week_of_its_monday` | `widget-gallery.md#calendarview` | Implemented |
| Calendar view | no library locale/timezone/clock reads | host-injected today provider | `test_common_components.cpp` `a_calendar_asks_again_for_today_so_it_can_turn_over_at_midnight` | `widget-gallery.md#calendarview` | Implemented |
| Date picker | Input/dropdown or dialog-backed date selection over Calendar view | `DatePicker`, `CalendarDropdown`, `FieldKind::Date` | `test_common_components.cpp` `a_hosted_date_picker_opens_the_calendar_and_tracks_its_typed_selection` [script]; `test_dialog.cpp` `a_date_field_materializes_as_a_typed_optional_picker_with_its_own_buddy` | `widget-gallery.md#datepicker` | Implemented |
| Date picker | Formatting/parsing policy supplied by options or caller callbacks | fixed ISO format (`format_iso_date`) | `test_common_components.cpp` `date_values_have_a_strict_locale_free_iso_boundary` | `widget-gallery.md#datepicker` | Missing: no formatter or parser hook or option |
| Time picker | Hour/minute/second selection | `TimePicker::on_key`, `on_mouse` | `test_common_components.cpp` `a_focused_time_picker_marks_the_field_its_arrows_change`, `a_scripted_time_picker_steps_its_fields_and_shows_either_clock_face` [script] | `widget-gallery.md#timepicker` | Implemented |
| Time picker | 12/24-hour display option | `TimePicker::set_24_hour` | `test_common_components.cpp` `a_scripted_time_picker_steps_its_fields_and_shows_either_clock_face` [script] | `widget-gallery.md#timepicker` | Implemented |
| Time picker | No locale/global state | fixed English meridiem labels | `test_common_components.cpp` `typed_time_interchange_is_canonical_and_strict` | `widget-gallery.md#timepicker` | Implemented |
| Time picker | optional seconds field | `TimePicker::set_show_seconds` | `test_common_components.cpp` `hiding_the_seconds_while_they_are_the_active_field_hands_the_arrows_to_the_minutes` | `widget-gallery.md#timepicker` | Implemented |
| Spin box / numeric stepper | Integer value with min/max/step | `SpinBox::set_range`, `set_step`, `set_value` | `test_common_components.cpp` `spin_box_and_slider_arithmetic_holds_across_the_whole_int_range` | `widget-gallery.md#spinbox` | Implemented |
| Spin box / numeric stepper | arrows/buttons/wheel | `SpinBox::on_key`, `on_mouse` (half-field clicks, wheel) | `test_common_components.cpp` `a_scripted_spin_box_steps_by_keys_by_clicks_on_either_half_and_by_the_wheel` [script] | `widget-gallery.md#spinbox` | Implemented (wheel added by this change) |
| Spin box / numeric stepper | validation feedback | none: out-of-range values are clamped silently | none | none | Missing |
| Spin box / numeric stepper | Optional editable text field | none | none | none | Missing |
| Slider | Horizontal bounded numeric value selection with keyboard/mouse operation | `Slider` | `test_common_components.cpp` `spinbox_and_slider_clamp_and_respond_to_keyboard_and_mouse`, `a_scripted_slider_follows_keys_and_a_proportional_click` [script] | `widget-gallery.md#slider` | Implemented |
| Slider | Optional tick labels | none | none | none | Missing |
| Search box / filter bar | InputLine plus clear/status affordance | `SearchBox` (query, "[x]" clear, Escape) | `test_common_components.cpp` `search_box_and_breadcrumb_emit_client_callbacks`; `test_help_viewer.cpp` `enter_in_the_search_box_does_not_dismiss_the_help_window` [script] | `widget-gallery.md#searchbox` | Partial: clear affordance only; no status affordance |
| Search box / filter bar | for filtering list/table/tree/text content | `SearchBox::on_change`; the help viewer filters its index with it | `test_help_viewer.cpp` `searching_narrows_the_index_to_the_matching_topics` [script] | `widget-gallery.md#searchbox` | Implemented through the host's `on_change` |
| Search box / filter bar | Debounce, if any, must use injected clock/timer; no hidden wall-clock reads | no debounce | — | — | Implemented (no clock read) |
| Command palette | Searchable command popup over `ui::CommandRegistry` | `CommandPalette` | `test_common_components.cpp` `toolbar_and_command_palette_are_command_registry_surfaces`, `a_scripted_command_palette_filters_as_typed_and_runs_the_highlighted_command` [script] | `widget-gallery.md#commandpalette` | Partial: a searchable list the host presents and closes; no popup presentation of its own |
| Command palette | Honors command contexts, enablement, labels/chords, retraction, and rebinding live | `set_invocation_contexts`, registry queries on every draw | `test_common_components.cpp` `command_palette_lists_and_runs_what_the_place_it_was_opened_from_allows` | `widget-gallery.md#commandpalette` | Implemented (enablement, chords, retraction and rebinding have no dedicated test) |
| Tool bar / action bar | Compact command-button row | `ToolBar` | `test_common_components.cpp` `a_toolbar_button_shows_its_commands_title_without_the_mnemonic_marker`, `a_scripted_tool_bar_click_runs_its_command_only_while_it_is_enabled` [script] | `widget-gallery.md#toolbar` | Implemented |
| Tool bar / action bar | dockable top/bottom | generic `Desktop::dock_top`/`dock_bottom` only | none | `widget-gallery.md#desktop` | Partial: no tool-bar docking, and docking to an edge replaces that edge's view (the menu bar or status line) |
| Tool bar / action bar | keyboard/mouse complete | mouse only; not a focus stop | `test_common_components.cpp` `a_scripted_tool_bar_click_runs_its_command_only_while_it_is_enabled` [script] | `widget-gallery.md#toolbar` | Partial: no keyboard operation |
| Tool bar / action bar | Overflow policy on narrow widths | buttons past the width are clipped or left out | none | none | Missing |
| Tool bar / action bar | shares CommandPresentation with menu/status | registry title and enablement | `test_common_components.cpp` `toolbar_and_command_palette_are_command_registry_surfaces` | `widget-gallery.md#toolbar` | Partial: takes `CommandId`s, not `CommandPresentation` |
| Breadcrumb bar | Segment navigation | `BreadcrumbBar` | `test_common_components.cpp` `search_box_and_breadcrumb_emit_client_callbacks`, `a_scripted_breadcrumb_bar_walks_and_activates_its_segments` [script] | `widget-gallery.md#breadcrumbbar` | Implemented |
| Breadcrumb bar | overflow elision | clipped at the width | none | none | Missing: no elision |
| Breadcrumb bar | keyboard focus | Tab stop, focused segment | `test_common_components.cpp` `a_breadcrumb_bar_is_a_tab_stop_from_construction` | `widget-gallery.md#breadcrumbbar` | Implemented |
| Breadcrumb bar | Caller supplies path semantics/separators; no filesystem access in widget | `set_separator`, plain strings | `test_common_components.cpp` `search_box_and_breadcrumb_emit_client_callbacks` | `widget-gallery.md#breadcrumbbar` | Implemented |
| Property inspector | Two-column name/value editor | `PropertyInspector` ("name: value" rows) | `test_common_components.cpp` `an_edited_property_takes_typed_characters_and_backspace_takes_a_whole_grapheme`, `a_scripted_property_inspector_edits_a_value_in_place` [script] | `widget-gallery.md#propertyinspector` | Partial: values are edited in place but not laid out in an aligned second column |
| Property inspector | with typed cell editors | none (`PropertyItem` holds text) | none | none | Missing |
| Property inspector | and validation | none | none | none | Missing |
| Property inspector | Integrates text/bool/enum/numeric/date-time editors as they exist | text only | none | none | Missing |
| Wizard / assistant | Multi-page workflow shell | `Wizard`, `WizardPage` | `test_common_components.cpp` `a_scripted_wizard_goes_forward_back_and_finishes_only_when_allowed` [script] | `widget-gallery.md#wizard` | Implemented |
| Wizard / assistant | with Back/Next/Finish/Cancel | Left/Right/Enter; Escape fires `on_cancel` | `test_common_components.cpp` `a_scripted_wizard_goes_forward_back_and_finishes_only_when_allowed` [script] | `widget-gallery.md#wizard` | Partial: no Cancel control is drawn and nothing is operable by mouse |
| Wizard / assistant | per-page validation | `WizardPage::can_continue` | `test_common_components.cpp` `a_wizard_page_that_holds_the_reader_back_offers_next_greyed_rather_than_finish` | `dialogs-and-commands.md#wizard-state-dependent-next` | Implemented |
| Wizard / assistant | Progress/step indicator | none | none | none | Missing |
| Wizard / assistant | typed non-blocking completion | `Wizard::on_finish` | `test_common_components.cpp` `a_scripted_wizard_goes_forward_back_and_finishes_only_when_allowed` [script] | `widget-gallery.md#wizard` | Partial: an untyped callback, not a typed completion |
| Notification center / toast | Severity-styled transient/persistent messages | `NotificationSeverity`, `Notification::persistent`, `set_auto_dismiss` | `test_common_components.cpp` `each_notifications_mark_wears_its_severitys_colour_over_the_centres_surface`, `a_scripted_notification_center_expires_toasts_and_dismisses_by_escape_and_click` [script] | `widget-gallery.md#notificationcenter` | Implemented |
| Notification center / toast | with keyboard dismissal | Escape dismisses the newest | `test_common_components.cpp` `escape_still_dismisses_the_most_recent_notification`, `a_scripted_notification_center_expires_toasts_and_dismisses_by_escape_and_click` [script] | `widget-gallery.md#notificationcenter` | Implemented |
| Notification center / toast | Timing, if any, must be injected; no hidden wall-clock reads | the Application's Clock and timer | `test_common_components.cpp` `a_toast_leaves_by_itself_once_its_time_is_up`, `a_scripted_notification_center_expires_toasts_and_dismisses_by_escape_and_click` [script] | `widget-gallery.md#notificationcenter` | Implemented |
| Tooltip / popover | Focus/pointer-triggered explanatory popup | `Tooltip::show_at`, `hide` (host-driven) | `test_common_components.cpp` `a_scripted_tooltip_appears_and_leaves_with_the_frames_its_host_steps` [script] | `widget-gallery.md#tooltip` | Partial: the host shows and hides it; nothing triggers it on focus or hover |
| Tooltip / popover | with deterministic placement | caller-given position | `test_common_components.cpp` `a_tooltip_given_less_room_than_its_text_elides_it_in_the_tooltip_role` | `widget-gallery.md#tooltip` | Partial: placed exactly where told; no anchor-relative or keep-inside policy |
| Tooltip / popover | Keyboard-accessible invocation | none | none | none | Missing |
| Tooltip / popover | same popup dismissal discipline as menus | none (host `hide` only) | none | none | Missing |

### Standard dialogs (M6)

| Row | Feature (catalog wording) | Implementation | Tests | Docs | State |
|---|---|---|---|---|---|
| File open/save | Path input with completion | Tab completion of the last segment | `test_file_dialog.cpp` `path_field_tab_completion_uses_the_filtered_injected_directory_listing`, `presented_file_dialog_accepts_a_terminal_typed_path_inside_its_modal_scope` [script] | `platform-services.md` | Implemented |
| File open/save | file list | directories first, then files | `test_file_dialog.cpp` `activating_a_file_entry_fills_the_path_field_and_a_second_enter_accepts_it` | `widget-gallery.md#filedialogoptions` | Implemented |
| File open/save | directory navigation | directory and ".." rows | `test_file_dialog.cpp` `activating_a_directory_entry_navigates_into_it_and_activating_dotdot_navigates_back` | `widget-gallery.md#filedialogoptions` | Implemented |
| File open/save | filters | `FileDialogFilter` | `test_file_dialog.cpp` `filters_limit_visible_files_and_the_filter_button_cycles_filters` | `widget-gallery.md#filedialogfilter` | Implemented |
| File open/save | Recent locations via the history registry | `FileDialogOptions::recent_locations` | `test_file_dialog.cpp` `recent_locations_are_listed_as_navigable_rows_and_accept_records_the_current_directory` | `widget-gallery.md#filedialogoptions` | Implemented |
| File open/save | hidden-file toggle | hidden toggle | `test_file_dialog.cpp` `hidden_toggle_reveals_and_hides_dot_entries_without_host_filesystem_access` | `widget-gallery.md#filedialogoptions` | Implemented |
| File open/save | cross-platform path semantics | `FileSystem::normalize_path`, `is_absolute_path` | `test_file_dialog.cpp` `relative_paths_with_backslashes_are_normalized_against_the_current_directory_on_accept`; `test_filesystem.cpp` `is_absolute_path_accepts_posix_root_and_drive_root_spellings` | `platform-services.md` | Implemented |
| File open/save | enumerates through the injected filesystem (headless-testable) | `MemoryFileSystem` | `test_file_dialog.cpp` `hidden_toggle_reveals_and_hides_dot_entries_without_host_filesystem_access` | `platform-services.md` | Implemented |
| Directory picker | Tree-based selection | `make_directory_picker`, `present_directory_picker` | `test_directory_picker.cpp` `navigating_into_a_subdirectory_and_accepting_returns_its_full_path`, `present_directory_picker_completes_after_its_modal_window_detaches` | `widget-gallery.md#directorypickerresult` | Implemented |
| Window list | Select/activate/close from list | `make_window_list_dialog` | `test_window_list_dialog.cpp` `activating_a_row_switches_the_desktops_active_window_and_closes_the_dialog` [script], `the_close_button_dismisses_without_changing_the_active_window` | `widget-gallery.md#window-list-dialog` | Partial: select and activate work; "close" closes the dialog, and no action closes the listed window |
| Window list | Type-ahead filter | the list's first-letter type-ahead | `test_window_list_dialog.cpp` `type_ahead_selects_a_matching_window_before_activation` | `widget-gallery.md#window-list-dialog` | Partial: type-ahead moves the selection; nothing filters the list |
| Theme editor | Enumerates theme roles from the flat tables, edits styles with live preview, produces a theme value the application persists (M11) | none | none | none | Missing |
| Date/time picker dialogs | Calendar/time selection using the WP-36B components | descriptor fields `FieldKind::Date`/`Time` in `present_dialog` | `test_dialog_presentation.cpp` `a_typed_form_mixes_every_field_kind_and_keeps_each_answer_at_its_own_index` [script] | `dialogs-and-commands.md#fields-that-are-not-text` | Partial: through the generic descriptor dialog; no dedicated date or time dialog |
| Date/time picker dialogs | Localizable labels supplied through explicit string/options tables; no locale reads | `CalendarView::set_labels`, `CalendarDropdown::set_month_labels` | none | none | Partial: no locale reads, but descriptor fields carry no label tables and the meridiem and "no date" texts are fixed English |

### Open catalog items (owner decision)

Each item below is quoted in the catalog's exact wording. This change does not
implement, re-scope or amend any of them.

| Catalog row | Catalog wording | State | What exists |
|---|---|---|---|
| Menu bar | "Overflow behavior on narrow terminals" | Partial | Clipping at the bar's width; dropdowns clamp and elide |
| Button | "Repeat-on-hold option" | Missing | A legacy terminal's keyboard auto-repeat only |
| Tab control | "overflow" (of "Top tabs with mnemonics, keyboard cycling, per-tab pages, overflow") | Partial | Captions past the right edge are clipped |
| Combo box | "dropdown with shadow" | Partial | `PopupList` casts no shadow; a one-line `casts_shadow()` override would change the pinned PopupList/ComboBox appearance goldens and widget screenshots |
| History registry | "Input line and Combo box histories live in the application-scoped history registry — shared by key across widgets and dialogs, never per-widget state" | Partial | Widgets borrow any registry; descriptor dialogs name no history key |
| Context menu | "Keyboard invocation (Menu key / Shift-F10) at focus location" | Partial | Shift+F10 through the application's binding; no Menu key |
| Desktop | "select-by-number" | Partial | API only; no key or command |
| Desktop | "Background drawing hook" | Partial | `Desktop::set_content` only |
| Window | "move by title-bar drag or keyboard mode" | Partial | Keyboard move mode has no command or chord |
| Window | "resize by grip/edges or keyboard mode" | Partial | Corners only; keyboard resize mode has no command or chord |
| Window | "double/single/rounded frame sets" | Partial | Fixed double/single; no rounded frame, no API |
| Text view | "Links (activatable; rendered as OSC 8 hyperlinks where supported)" | Partial | OSC 8 is an export string only; the presenter never emits it |
| Help viewer | "activatable cross-links" | Partial | "See also" names topics as plain text |
| Help viewer | "back navigation" | Partial | Implemented, untested |
| Date picker | "Formatting/parsing policy supplied by options or caller callbacks" | Missing | Fixed ISO format |
| Spin box / numeric stepper | "validation feedback" | Missing | Silent clamping |
| Spin box / numeric stepper | "Optional editable text field" | Missing | — |
| Slider | "Optional tick labels" | Missing | — |
| Search box / filter bar | "InputLine plus clear/status affordance" | Partial | Clear only, and not an InputLine |
| Command palette | "Searchable command popup over `ui::CommandRegistry`" | Partial | A list the host presents |
| Tool bar / action bar | "dockable top/bottom" | Partial | Generic desktop docking only |
| Tool bar / action bar | "keyboard/mouse complete" | Partial | Mouse only |
| Tool bar / action bar | "Overflow policy on narrow widths" | Missing | Clipped |
| Tool bar / action bar | "shares CommandPresentation with menu/status" | Partial | `CommandId`s only |
| Breadcrumb bar | "overflow elision" | Missing | Clipped |
| Property inspector | "Two-column name/value editor with typed cell editors and validation" | Partial / Missing | Text editing in "name: value" rows |
| Property inspector | "Integrates text/bool/enum/numeric/date-time editors as they exist" | Missing | Text only |
| Wizard / assistant | "Back/Next/Finish/Cancel" | Partial | Keyboard only; no drawn Cancel |
| Wizard / assistant | "Progress/step indicator and typed non-blocking completion" | Missing / Partial | Untyped `on_finish` callback |
| Tooltip / popover | "Focus/pointer-triggered explanatory popup with deterministic placement" | Partial | Host-driven `show_at`/`hide` |
| Tooltip / popover | "Keyboard-accessible invocation; same popup dismissal discipline as menus" | Missing | — |
| Window list | "Select/activate/close from list" | Partial | No action closes a listed window |
| Window list | "Type-ahead filter" | Partial | Type-ahead selects; does not filter |
| Theme editor | "Enumerates theme roles from the flat tables, edits styles with live preview, produces a theme value the application persists (M11)" | Missing | — |
| Date/time picker dialogs | "Calendar/time selection using the WP-36B components" | Partial | Descriptor-dialog fields only |
| Date/time picker dialogs | "Localizable labels supplied through explicit string/options tables; no locale reads" | Partial | Label setters exist but are unreachable from descriptor fields |
