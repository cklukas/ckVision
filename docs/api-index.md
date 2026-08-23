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

## Embedded terminal

| `include/cvision/core/terminal_subsession.hpp` | deterministic child-session snapshot contract, capability profile and policies | [Embedded terminal](embedded-terminal.md) |
| `include/cvision/term/terminal_subsession.hpp` | launch specification and platform adapter seam | [Embedded terminal](embedded-terminal.md) |
| `include/cvision/core/palette.hpp` | what a palette index names, and colour resolution | [Themes and rendering](themes-and-rendering.md) |
| `include/cvision/core/base64.hpp` | the encoding `OSC 52` carries clipboard text in | [Embedded terminal](embedded-terminal.md) |

## Chrome and interaction

| Header | Primary types | Guide/example |
|---|---|---|
| `include/cvision/widgets/application_shell.hpp` | `ApplicationShell`, `ApplicationShellOptions` | [Hello](tutorial-hello.md) |
| `include/cvision/widgets/desktop.hpp` | `Desktop` | [Object model](object-model.md) |
| `include/cvision/widgets/window.hpp` | `Window`, `FrameSlot`, `WindowHandle` | [Object model](object-model.md) |
| `include/cvision/widgets/menu.hpp` | `MenuBar`, `DropdownMenu`, menu items | [Hello](tutorial-hello.md) |
| `include/cvision/widgets/status_line.hpp` | `StatusLine`, `StatusLineItem` | [Hello](tutorial-hello.md) |
| `include/cvision/widgets/command_presentation.hpp` | `CommandPresentation` | [Dialogs and commands](dialogs-and-commands.md) |
| `include/cvision/widgets/mnemonic.hpp` | `MnemonicText` | [Widget gallery](widget-gallery.md) |
| `include/cvision/widgets/mnemonic_internal.hpp` | shared mnemonic drawing helpers (internal) | `mnemonic.hpp` is the client entry point |
| `include/cvision/widgets/orientation.hpp` | `Orientation` | [Widget gallery](widget-gallery.md#scrollbar) |

## Controls and data views

| Header | Primary types | Guide/example |
|---|---|---|
| `include/cvision/widgets/label.hpp` | `Label` | [Widget gallery](widget-gallery.md#label) |
| `include/cvision/widgets/static_text.hpp` | `StaticText` | [Widget gallery](widget-gallery.md#statictext) |
| `include/cvision/widgets/button.hpp` | `Button` | [Widget gallery](widget-gallery.md#button) |
| `include/cvision/widgets/input_line.hpp` | `InputLine` | [Widget gallery](widget-gallery.md#inputline) |
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
| `include/cvision/widgets/tree_view.hpp` | `TreeView`, `TreeNode` | [Widget gallery](widget-gallery.md#treeview) |
| `include/cvision/widgets/table.hpp` | `Table`, `TableModel`, `TableCell` | [Data views](data-views.md) |
| `include/cvision/widgets/tab_control.hpp` | `TabControl` | [Widget gallery](widget-gallery.md#tabcontrol) |
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
| `include/cvision/widgets/window_list_dialog.hpp` | window chooser | [Widget gallery](widget-gallery.md#window-list-dialog) |
| `include/cvision/widgets/paged_strip.hpp` | `PagedStrip` | [Widget gallery](widget-gallery.md#pagedstrip) |
| `include/cvision/widgets/minimized_window_stub.hpp` | `MinimizedWindowStub` | [Widget gallery](widget-gallery.md#minimizedwindowstub) |
| `include/cvision/widgets/window_switcher_bar.hpp` | `WindowSwitcherBar`, `WindowSwitcherTarget` | [Widget gallery](widget-gallery.md#windowswitcherbar) |
| `include/cvision/widgets/standard_strings.hpp` | `StandardStrings` | [Dialogs and commands](dialogs-and-commands.md) |

## Graphics and common components

| Header | Primary types | Guide/example |
|---|---|---|
| `include/cvision/widgets/image_view.hpp` | `ImageView` | [Graphics](graphics.md) |
| `include/cvision/widgets/canvas.hpp` | `Canvas` | [Graphics](graphics.md) |
| `include/cvision/widgets/common_components.hpp` | calendar/date/time/spin/slider/search/toolbar/palette/breadcrumb/property/wizard/notification/tooltip | [Widget gallery](widget-gallery.md) |

The public terminal hosts live under `include/cvision/term/`; use
`headless_terminal.hpp` in tests and the platform backend header appropriate to
your host. [Platform services](platform-services.md) covers the boundary.
