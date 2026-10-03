---
title: ckVision Client API Index
author: C. Klukas
date: 2026-08-09
format: report
description: A curated map from client tasks to public ckVision headers, guides, examples, and tests.
---

# Client API index

This is an index, not a replacement for the learning path:
[getting started](getting-started.md) -> [Hello](tutorial-hello.md) ->
[object model](object-model.md) → the focused guides. Every public widget
header has an entry below and a corresponding row in [coverage](coverage.md).
Every installed header appears once in the tables, including headers for testing
and implementation support; their descriptions identify the intended entry point.

## Application and layout

| Header | Primary types | Start here |
|---|---|---|
| `include/cvision/ui/application.hpp` | `Application` | [Getting started](getting-started.md) |
| `include/cvision/ui/view.hpp` | `View` | [Object model](object-model.md) |
| `include/cvision/ui/animation.hpp` | `Animation` | [Widget gallery](widget-gallery.md#animation) |
| `include/cvision/ui/layout.hpp` | `Row`, `Column`, layout specifications | [Layout guide](layout-guide.md) |
| `include/cvision/ui/grid.hpp` | `Grid` | [Layout guide](layout-guide.md) |
| `include/cvision/ui/dock.hpp` | `Dock` | [Layout guide](layout-guide.md) |
| `include/cvision/ui/anchor_pane.hpp` | `AnchorPane` | [Layout guide](layout-guide.md) |
| `include/cvision/ui/overlay.hpp` | `Overlay` | [Layout guide](layout-guide.md) |
| `include/cvision/ui/command.hpp` | command registry and ids | [Dialogs and commands](dialogs-and-commands.md) |
| `include/cvision/ui/theme.hpp` | `Theme` | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/core/shadow_style.hpp` | `ShadowStyle`: how a theme's shadow darkens cells and pictures | [Themes and rendering](themes-and-rendering.md#shadows-one-binary-union-d-037) |
| `include/cvision/ui/theme_format.hpp` | `serialize_theme`, `parse_theme`: the saved text of a theme | [Themes and rendering](themes-and-rendering.md#saving-and-loading-a-theme) |

## Embedded terminal

| Header | Primary types | Start here |
|---|---|---|
| `include/cvision/core/terminal_subsession.hpp` | deterministic child-session snapshot contract, capability profile and policies | [Embedded terminal](embedded-terminal.md) |
| `include/cvision/term/terminal_subsession.hpp` | launch specification and platform adapter seam | [Embedded terminal](embedded-terminal.md) |
| `include/cvision/core/palette.hpp` | what a palette index names, and colour resolution | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/core/base64.hpp` | the encoding `OSC 52` carries clipboard text in | [Embedded terminal](embedded-terminal.md) |

## Chrome and interaction

| Header | Primary types | Guide/example |
|---|---|---|
| `include/cvision/widgets/application_shell.hpp` | `ApplicationShell`, `ApplicationShellOptions` | [Hello](tutorial-hello.md) |
| `include/cvision/widgets/desktop.hpp` | `Desktop`, `DockEdge` | [Object model](object-model.md) |
| `include/cvision/widgets/window.hpp` | `Window`, `FrameSlot`, `FrameLines`, `WindowHandle` | [Object model](object-model.md) |
| `include/cvision/widgets/menu.hpp` | `MenuBar`, `DropdownMenu`, menu items | [Hello](tutorial-hello.md) |
| `include/cvision/widgets/status_line.hpp` | `StatusLine`, `StatusLineItem` | [Hello](tutorial-hello.md) |
| `include/cvision/widgets/command_presentation.hpp` | `CommandPresentation` | [Dialogs and commands](dialogs-and-commands.md) |
| `include/cvision/widgets/mnemonic.hpp` | `MnemonicText` | [Widget gallery](widget-gallery.md) |
| `include/cvision/widgets/mnemonic_internal.hpp` | shared mnemonic drawing helpers (internal) | `mnemonic.hpp` is the client entry point |
| `include/cvision/widgets/orientation.hpp` | `Orientation` | [Widget gallery](widget-gallery.md#scrollbar) |

## Controls and data views

| Header | Primary types | Guide/example |
|---|---|---|
| `include/cvision/widgets/frame_text.hpp` | `FrameText` | [Widget gallery](widget-gallery.md#frametext) |
| `include/cvision/widgets/label.hpp` | `Label` | [Widget gallery](widget-gallery.md#label) |
| `include/cvision/widgets/static_text.hpp` | `StaticText` | [Widget gallery](widget-gallery.md#statictext) |
| `include/cvision/widgets/button.hpp` | `Button` | [Widget gallery](widget-gallery.md#button) |
| `include/cvision/widgets/input_presentation.hpp` | `InputPresentation`, field geometry | [Widget gallery](widget-gallery.md#inputline) |
| `include/cvision/widgets/input_presentation_internal.hpp` | shared field surface drawing (internal) | `input_presentation.hpp` is the client entry point |
| `include/cvision/widgets/input_line.hpp` | `InputLine` | [Widget gallery](widget-gallery.md#inputline) |
| `include/cvision/widgets/key_chord_capture.hpp` | `KeyChordCapture` | [Widget gallery](widget-gallery.md#keychordcapture) |
| `include/cvision/widgets/memo.hpp` | `Memo`, `MemoPosition` | [Widget gallery](widget-gallery.md#memo) |
| `include/cvision/widgets/editor_document.hpp` | `EditorDocument`, positions, ranges, transactions | [Editor](editor.md) |
| `include/cvision/widgets/text_editor.hpp` | `TextEditor`, `EditorStatus`, `EditorStatusModel` | [Editor](editor.md) |
| `include/cvision/widgets/syntax_profile.hpp` | profile registry and syntax spans | [Editor](editor.md) |
| `include/cvision/widgets/syntax_cache.hpp` | incremental lexical-state cache | [Editor](editor.md) |
| `include/cvision/widgets/editor_search.hpp` | literal search and atomic replace-all | [Editor](editor.md) |
| `include/cvision/widgets/editor_window.hpp` | optional editor/controller/window composition | [Editor](editor.md) |
| `include/cvision/widgets/terminal_report_dialog.hpp` | the terminal capability report | [Widget gallery](widget-gallery.md#terminal-report-dialog) |
| `include/cvision/widgets/terminal_scrollbar.hpp` | the frame-mounted scrollbar bound to a terminal's scrollback | [Embedded terminal](embedded-terminal.md) |
| `include/cvision/widgets/terminal_view.hpp` | `TerminalView` | [Embedded terminal](embedded-terminal.md) |
| `include/cvision/widgets/popup_list.hpp` | the floating list a control drops when its choices are data | [Widget gallery](widget-gallery.md#popuplist) |
| `include/cvision/widgets/text_layout.hpp` | `WrapMode`, `WrapOptions`, `WrapSegment`, `ScrollGeometry` | [Widget gallery](widget-gallery.md#wrapmode) |
| `include/cvision/widgets/text_view.hpp` | `TextView`, `TextSpan` | [Widget gallery](widget-gallery.md#textview) |
| `include/cvision/widgets/flow_view.hpp` | `FlowView`, `FlowDocument` | [Flow content](flow-view.md) |
| `include/cvision/widgets/option_group.hpp` | `CheckGroup`, `RadioGroup` | [Widget gallery](widget-gallery.md#checkgroup) |
| `include/cvision/widgets/combo_box.hpp` | `ComboBox` | [Widget gallery](widget-gallery.md#combobox) |
| `include/cvision/widgets/list_view.hpp` | `ListView`, `ListModel`, `ListItem` | [Data views](data-views.md) |
| `include/cvision/widgets/tree_view.hpp` | `TreeView`, `TreeNode`, `TreeModel`, `TreeItem` | [Data views](data-views.md#tree-providers) |
| `include/cvision/widgets/table.hpp` | `Table`, `TableModel`, `TableCell` | [Data views](data-views.md) |
| `include/cvision/widgets/cell_grid.hpp` | `CellGrid`, `CellGridModel`, `GridCell`, `GridFrame`, `MaterializedCellGridModel` | [Data views](data-views.md#cell-grid-providers) |
| `include/cvision/widgets/tab_control.hpp` | `TabControl` | [Widget gallery](widget-gallery.md#tabcontrol) |
| `include/cvision/widgets/big_clock.hpp` | `BigClockView`, `BigClockContent` | [Widget gallery](widget-gallery.md#bigclockview) |
| `include/cvision/widgets/progress.hpp` | `Progress` | [Widget gallery](widget-gallery.md#progress) |
| `include/cvision/widgets/scrollbar.hpp` | `Scrollbar` | [Widget gallery](widget-gallery.md#scrollbar) |
| `include/cvision/widgets/scroll_viewport.hpp` | `ScrollViewport` | [Widget gallery](widget-gallery.md#scrollviewport) |
| `include/cvision/widgets/splitter.hpp` | `Splitter` | [Layout guide](layout-guide.md) |

## Dialogs and client services

| Header | Primary types | Guide/example |
|---|---|---|
| `include/cvision/widgets/dialog.hpp` | descriptor dialogs and results | [Dialogs and commands](dialogs-and-commands.md) |
| `include/cvision/widgets/dialog_presentation.hpp` | typed dialog presentation | [Dialogs and commands](dialogs-and-commands.md) |
| `include/cvision/widgets/message_box.hpp` | message boxes | [Hello](tutorial-hello.md) |
| `include/cvision/widgets/file_dialog.hpp` | open/save dialog | [Platform services](platform-services.md) |
| `include/cvision/widgets/directory_picker.hpp` | directory picker | [Platform services](platform-services.md) |
| `include/cvision/widgets/file_editor_controller.hpp` | injected editor file lifecycle | [Editor](editor.md) |
| `include/cvision/widgets/help_viewer.hpp` | help viewer/provider | [Dialogs and commands](dialogs-and-commands.md) |
| `include/cvision/widgets/date_time_dialog.hpp` | date and time dialogs | [Dialogs and commands](dialogs-and-commands.md#date-and-time-dialogs) |
| `include/cvision/widgets/window_list_dialog.hpp` | window chooser: filter, switch to, close | [Widget gallery](widget-gallery.md#window-list-dialog) |
| `include/cvision/widgets/theme_editor.hpp` | theme editor | [Themes and rendering](themes-and-rendering.md#the-theme-editor) |
| `include/cvision/widgets/paged_strip.hpp` | `PagedStrip` | [Widget gallery](widget-gallery.md#pagedstrip) |
| `include/cvision/widgets/minimized_window_stub.hpp` | `MinimizedWindowStub` | [Widget gallery](widget-gallery.md#minimizedwindowstub) |
| `include/cvision/widgets/window_switcher_bar.hpp` | `WindowSwitcherBar`, `WindowSwitcherTarget` | [Widget gallery](widget-gallery.md#windowswitcherbar) |
| `include/cvision/widgets/standard_strings.hpp` | `StandardStrings` | [Dialogs and commands](dialogs-and-commands.md) |

## Graphics and common components

| Header | Primary types | Guide/example |
|---|---|---|
| `include/cvision/widgets/image_view.hpp` | `ImageView` | [Graphics](graphics.md) |
| `include/cvision/widgets/canvas.hpp` | `Canvas`, `fit_image_cells`, `kAssumedCellPixels` | [Graphics](graphics.md) |
| `include/cvision/widgets/common_components.hpp` | calendar/date/time/spin/slider/search/toolbar/palette/breadcrumb/property/wizard/notification/tooltip | [Widget gallery](widget-gallery.md) |

The public terminal hosts live under `include/cvision/term/`; use
`headless_terminal.hpp` in tests and the platform backend header appropriate to
your host. [Platform services](platform-services.md) covers the boundary.

## Core values, text, and services

| Header | Primary contract | Start here |
|---|---|---|
| `include/cvision/core/ascii.hpp` | locale-free ASCII classification and case folding | [Text width](text-width.md#sanitization) |
| `include/cvision/core/assert.hpp` | always-on `CKV_ASSERT` contract checks | [Host diagnostics](terminal-host-integration.md#diagnostics) |
| `include/cvision/core/cell.hpp` | `Cell` and grapheme/style/link storage | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/core/clipboard.hpp` | `ClipboardWriter`, `MemoryClipboardWriter` | [Platform services](platform-services.md) |
| `include/cvision/core/clock.hpp` | `Clock`, `ManualClock` | [Platform services](platform-services.md) |
| `include/cvision/core/color.hpp` | `Color` and colour kinds | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/core/cursor.hpp` | `CursorState`, `CursorShape` | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/core/diagnostics.hpp` | `DiagnosticsSink`, `BufferedDiagnostics` | [Host diagnostics](terminal-host-integration.md#diagnostics) |
| `include/cvision/core/event.hpp` | key, text, mouse, resize, focus, and paste events | [Input decoder](input-decoder.md) |
| `include/cvision/core/filesystem.hpp` | injected `FileSystem` and file result contracts | [Platform services](platform-services.md) |
| `include/cvision/core/frame_view.hpp` | `FrameView`, `RasterSlice` | [Graphics](graphics.md) |
| `include/cvision/core/generated_unicode_15_1.hpp` | generated Unicode tables; implementation support, not the client text entry point | [Text width](text-width.md) |
| `include/cvision/core/geometry.hpp` | `Point`, `Size`, `Rect`, `PixelPoint`, `PixelSize` | [Object model](object-model.md) |
| `include/cvision/core/golden.hpp` | symbolic scene `golden::Document` and records | [Golden format](golden-format.md) |
| `include/cvision/core/hyperlink.hpp` | `LinkTable`, `LinkId`, `is_valid_hyperlink_target` — hyperlink targets carried by cells | [Terminal host integration](terminal-host-integration.md#hyperlinks-osc-8) |
| `include/cvision/core/image.hpp` | `Image`, `Rgba` | [Graphics](graphics.md) |
| `include/cvision/core/key.hpp` | `Key`, `Modifier`, `KeyChord` | [Input decoder](input-decoder.md) |
| `include/cvision/core/pointer_shape.hpp` | `PointerShape` vocabulary | [Terminal capability profiles](terminal-profiles.md) |
| `include/cvision/core/style.hpp` | `Style`, attributes, underline shape | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/core/text.hpp` | deterministic text and grapheme helpers | [Text width](text-width.md) |
| `include/cvision/core/utf8.hpp` | UTF-8 validation and conversion helpers | [Text width](text-width.md) |
| `include/cvision/core/version.hpp` | `Version` and library version | [Getting started](getting-started.md) |

## Scene composition and capture

| Header | Primary contract | Start here |
|---|---|---|
| `include/cvision/scene/box_drawing.hpp` | box junction and line-style resolution | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/scene/compositor.hpp` | `Compositor`, layers, shadows, raster visibility | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/scene/cursor.hpp` | scene cursor placement helpers | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/scene/golden_capture.hpp` | scene and frame capture into golden documents | [Golden format](golden-format.md) |
| `include/cvision/scene/painter.hpp` | `Painter` drawing API | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/scene/rect_ops.hpp` | rectangle set operations for damage and clipping | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/scene/surface.hpp` | `Surface`, damage spans, raster regions | [Themes and rendering](themes-and-rendering.md) |

## Terminal hosts, protocols, and diagnostics

| Header | Primary contract | Start here |
|---|---|---|
| `include/cvision/term/capabilities.hpp` | `Capabilities`, terminal profiles, colour and input protocol enums, the cell/pixel conversions `cells_to_pixels` and `cell_pixels_from_area` | [Terminal capability profiles](terminal-profiles.md) |
| `include/cvision/term/capability_report.hpp` | structured capability report entries | [Terminal host integration](terminal-host-integration.md) |
| `include/cvision/term/file_trace_sink.hpp` | `FileTraceSink`, the live file or stderr sink for a graphics trace | [Graphics](graphics.md) |
| `include/cvision/term/headless_terminal.hpp` | deterministic `HeadlessTerminal` test host | [Terminal host integration](terminal-host-integration.md) |
| `include/cvision/term/input_decoder.hpp` | `InputDecoder` and capability-update policy | [Input decoder](input-decoder.md) |
| `include/cvision/term/osc_sequences.hpp` | the window-title (OSC 0) and clipboard-export (OSC 52) sequences every VT backend sends | [Terminal host integration](terminal-host-integration.md#osc-emission-safety) |
| `include/cvision/term/pointer_shape_names.hpp` | pointer-shape protocol names and support map | [Terminal capability profiles](terminal-profiles.md) |
| `include/cvision/term/posix_clock.hpp` | `PosixClock` | [Platform services](platform-services.md) |
| `include/cvision/term/posix_filesystem.hpp` | `PosixFileSystem` | [Platform services](platform-services.md) |
| `include/cvision/term/posix_terminal.hpp` | POSIX `PosixTerminal` session backend | [Terminal host integration](terminal-host-integration.md) |
| `include/cvision/term/posix_terminal_subsession.hpp` | private POSIX child PTY adapter | [Embedded terminal](embedded-terminal.md) |
| `include/cvision/term/presenter.hpp` | `Presenter` cell and raster VT encoder | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/term/record_replay_terminal.hpp` | deterministic terminal I/O recording and replay | [Terminal host integration](terminal-host-integration.md) |
| `include/cvision/term/sixel_decoder.hpp` | bounded Sixel image decoder | [Graphics](graphics.md) |
| `include/cvision/term/sixel_encoder.hpp` | Sixel image encoder | [Graphics](graphics.md) |
| `include/cvision/term/terminal.hpp` | `Terminal`, capability events, and wait handles | [Terminal host integration](terminal-host-integration.md) |
| `include/cvision/term/terminal_clipboard.hpp` | `TerminalClipboardWriter` | [Platform services](platform-services.md) |
| `include/cvision/term/windows_clipboard.hpp` | `WindowsClipboardWriter`: instance-owned native Unicode export | [Platform services](platform-services.md) |
| `include/cvision/term/terminal_emulator.hpp` | `TerminalEmulator` for contained child output | [Embedded terminal](embedded-terminal.md) |
| `include/cvision/term/virtual_display.hpp` | `VirtualDisplay` for deterministic VT presentation | [Graphics](graphics.md) |
| `include/cvision/term/windows_clock.hpp` | `WindowsClock` | [Platform services](platform-services.md) |
| `include/cvision/term/windows_terminal.hpp` | Windows `WindowsTerminal` session backend | [Terminal host integration](terminal-host-integration.md) |
| `include/cvision/term/windows_terminal_subsession.hpp` | private Windows ConPTY child adapter | [Embedded terminal](embedded-terminal.md) |

## UI support and testing

| Header | Primary contract | Start here |
|---|---|---|
| `include/cvision/ui/context.hpp` | `Context` for application-owned services | [Object model](object-model.md) |
| `include/cvision/ui/history.hpp` | `HistoryRegistry` for scoped interaction history | [Object model](object-model.md) |
| `include/cvision/ui/layout_metrics.hpp` | layout measurement helpers | [Layout guide](layout-guide.md) |
| `include/cvision/ui/standard_roles.hpp` | `StandardRoles` for theme role lookup | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/testing/cktest.hpp` | repository test harness; testing support, not an application dependency | [Contributing](https://github.com/cklukas/ckVision/blob/main/CONTRIBUTING.md) |
