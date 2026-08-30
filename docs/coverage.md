---
title: ckVision Documentation Coverage
author: C. Klukas
date: 2026-08-09
format: report
description: Machine-checked traceability from every public widget header to client documentation and evidence.
---

# Documentation coverage

This matrix is deliberately checked by `doc_widget_coverage`. It maps every
public widget header to the client guide, a real example, generated screenshot,
and executable test family. Screenshots are generated, not committed artwork.

Most Screenshot entries now name a `widget-*` figure: a focused cut-out of the
widget, written by `tools/docgen/capture_widget_shots` from a scene in
`tools/docgen/widget_shots_*.cpp`. Those scenes are compiled examples in their
own right — the [widget gallery](widget-gallery.md) quotes each one as that
widget's usage sample — which is why they appear in the Compiled example
column as `widget_shots`.

| Public header | Client guide | Compiled example | Screenshot | Test family |
|---|---|---|---|---|
| `include/cvision/widgets/application_shell.hpp` | [Hello](tutorial-hello.md) | hello | hello-initial | test_hello_golden |
| `include/cvision/widgets/button.hpp` | [Gallery](widget-gallery.md#button) | forms/widget_shots | widget-button | test_widgets, test_forms_smoke |
| `include/cvision/widgets/canvas.hpp` | [Graphics](graphics.md) | graphics/widget_shots | widget-canvas/widget-canvas-no-graphics | test_graphics_smoke |
| `include/cvision/widgets/combo_box.hpp` | [Gallery](widget-gallery.md#combobox) | forms/workbench/widget_shots | widget-combobox/widget-combobox-open | test_combo_box |
| `include/cvision/widgets/command_presentation.hpp` | [Commands](dialogs-and-commands.md) | hello/widget_shots | widget-toolbar/hello-menu-open | test_command |
| `include/cvision/widgets/common_components.hpp` | [Gallery](widget-gallery.md) | forms/workbench/widget_shots | widget-calendarview/widget-clockview/widget-wizard/widget-notificationcenter | test_common_components |
| `include/cvision/widgets/desktop.hpp` | [Object model](object-model.md) | gallery/widget_shots | widget-desktop | test_desktop |
| `include/cvision/widgets/dialog.hpp` | [Dialogs](dialogs-and-commands.md) | forms/widget_shots | widget-dialogdescriptor | test_dialog |
| `include/cvision/widgets/dialog_presentation.hpp` | [Dialogs](dialogs-and-commands.md) | forms/widget_shots | widget-messagebox | test_dialog_presentation |
| `include/cvision/widgets/directory_picker.hpp` | [Platform services](platform-services.md) | forms/widget_shots | widget-directorypicker | test_directory_picker |
| `include/cvision/widgets/editor_document.hpp` | [Editor](editor.md) | editor/widget_shots | widget-texteditor/editor-initial | test_editor_document |
| `include/cvision/widgets/editor_search.hpp` | [Editor](editor.md) | editor/widget_shots | widget-texteditor/editor-search | test_editor_search |
| `include/cvision/widgets/editor_window.hpp` | [Editor](editor.md) | editor/widget_shots | widget-editorwindow/editor-close-confirm | test_editor_window |
| `include/cvision/widgets/file_editor_controller.hpp` | [Editor](editor.md) | editor | editor-initial/editor-close-confirm | test_file_editor_controller |
| `include/cvision/widgets/file_dialog.hpp` | [Platform services](platform-services.md) | filebrowser/widget_shots | widget-filedialog | test_file_dialog |
| `include/cvision/widgets/flow_view.hpp` | [Flow content](flow-view.md) | workbench/widget_shots | widget-flowview | test_flow_view, test_workbench_smoke |
| `include/cvision/widgets/help_viewer.hpp` | [Dialogs](dialogs-and-commands.md) | forms/widget_shots | widget-helpviewer | test_help_viewer |
| `include/cvision/widgets/image_view.hpp` | [Graphics](graphics.md) | graphics/widget_shots | widget-imageview/graphics-no-graphics-image | test_graphics_smoke |
| `include/cvision/widgets/input_line.hpp` | [Gallery](widget-gallery.md#inputline) | forms/widget_shots | widget-inputline | test_widgets |
| `include/cvision/widgets/label.hpp` | [Gallery](widget-gallery.md#label) | layouts/widget_shots | widget-label | test_widgets |
| `include/cvision/widgets/list_view.hpp` | [Data views](data-views.md) | filebrowser/widget_shots | widget-listview | test_list_view |
| `include/cvision/widgets/memo.hpp` | [Gallery](widget-gallery.md#memo) | workbench/widget_shots | widget-memo | test_memo |
| `include/cvision/widgets/menu.hpp` | [Hello](tutorial-hello.md) | hello/widget_shots | widget-menubar/widget-dropdownmenu | test_menu |
| `include/cvision/widgets/message_box.hpp` | [Dialogs](dialogs-and-commands.md) | hello/forms/widget_shots | widget-messagebox | test_message_box |
| `include/cvision/widgets/mnemonic.hpp` | [Gallery](widget-gallery.md#mnemonictext) | forms/widget_shots | widget-label/widget-checkgroup/widget-radiogroup | test_mnemonic |
| `include/cvision/widgets/mnemonic_internal.hpp` | internal mnemonic paint support | forms/hello/widget_shots | widget-dropdownmenu/widget-checkgroup/widget-radiogroup | test_mnemonic, test_menu |
| `include/cvision/widgets/option_group.hpp` | [Gallery](widget-gallery.md#checkgroup) | forms/widget_shots | widget-checkgroup/widget-radiogroup | test_option_group |
| `include/cvision/widgets/paged_strip.hpp` | [Gallery](widget-gallery.md#pagedstrip) | widget_shots | widget-pagedstrip | test_paged_strip |
| `include/cvision/widgets/popup_list.hpp` | [Gallery](widget-gallery.md#popuplist) | forms/widget_shots | widget-popuplist | test_popup_list |
| `include/cvision/widgets/orientation.hpp` | [Gallery](widget-gallery.md#scrollbar) | layouts/widget_shots | widget-scrollbar | test_scrollbar |
| `include/cvision/widgets/progress.hpp` | [Gallery](widget-gallery.md#progress) | workbench/widget_shots | widget-progress | test_progress |
| `include/cvision/widgets/scroll_viewport.hpp` | [Gallery](widget-gallery.md#scrollviewport) | gallery/widget_shots | widget-scrollviewport | test_scroll_viewport |
| `include/cvision/widgets/scrollbar.hpp` | [Gallery](widget-gallery.md#scrollbar) | gallery/widget_shots | widget-scrollbar | test_scrollbar |
| `include/cvision/widgets/splitter.hpp` | [Layout](layout-guide.md) | layouts/filebrowser/widget_shots | widget-splitter | test_splitter |
| `include/cvision/widgets/standard_strings.hpp` | [Dialogs](dialogs-and-commands.md) | forms/widget_shots | widget-filedialog/widget-helpviewer | test_standard_strings |
| `include/cvision/widgets/static_text.hpp` | [Gallery](widget-gallery.md#statictext) | layouts/widget_shots | widget-statictext | test_widgets |
| `include/cvision/widgets/status_line.hpp` | [Hello](tutorial-hello.md) | hello/widget_shots | widget-statusline | test_status_line |
| `include/cvision/widgets/tab_control.hpp` | [Gallery](widget-gallery.md#tabcontrol) | workbench/graphics/widget_shots | widget-tabcontrol | test_tab_control |
| `include/cvision/widgets/table.hpp` | [Data views](data-views.md) | workbench/widget_shots | widget-table/widget-table-editing | test_table |
| `include/cvision/widgets/terminal_report_dialog.hpp` | [Gallery](widget-gallery.md#terminal-report-dialog) | spin/widget_shots | widget-terminalreportdialog | test_terminal_report_dialog |
| `include/cvision/widgets/terminal_scrollbar.hpp` | [Embedded terminal](embedded-terminal.md) | ckvision_terminal | interactive session | test_terminal_scrollbar |
| `include/cvision/widgets/terminal_view.hpp` | [Embedded terminal](embedded-terminal.md) | ckvision_terminal | terminal-initial | test_terminal_view, test_terminal_app, terminal_redraw_contract |
| `include/cvision/widgets/text_layout.hpp` | [Gallery](widget-gallery.md#wrapmode) | editor/widget_shots | widget-textview/widget-memo | test_text_layout |
| `include/cvision/widgets/text_view.hpp` | [Gallery](widget-gallery.md#textview) | workbench/widget_shots | widget-textview | test_text_view |
| `include/cvision/widgets/text_editor.hpp` | [Editor](editor.md); [TODO](todo-example.md#full-note-editor) | editor/todo/widget_shots | widget-texteditor/todo-note-editor | test_text_editor, test_todo_smoke |
| `include/cvision/widgets/syntax_profile.hpp` | [Editor](editor.md) | editor/widget_shots | widget-texteditor/editor-json | test_syntax_profile |
| `include/cvision/widgets/syntax_cache.hpp` | [Editor](editor.md) | editor/widget_shots | widget-texteditor | test_syntax_cache |
| `include/cvision/widgets/tree_view.hpp` | [Gallery](widget-gallery.md#treeview) | filebrowser/widget_shots | widget-treeview | test_tree_view |
| `include/cvision/widgets/window.hpp` | [Object model](object-model.md) | gallery/widget_shots | widget-window | test_window |
| `include/cvision/widgets/window_list_dialog.hpp` | [Gallery](widget-gallery.md#window-list-dialog) | gallery/widget_shots | widget-windowlistdialog | test_window_list_dialog |
| `include/cvision/widgets/minimized_window_stub.hpp` | [Gallery](widget-gallery.md#minimizedwindowstub) | widget_shots | widget-minimizedwindowstub | test_minimized_window_stub |
| `include/cvision/widgets/window_switcher_bar.hpp` | [Gallery](widget-gallery.md#windowswitcherbar) | widget_shots | widget-windowswitcherbar | test_window_switcher_bar |
