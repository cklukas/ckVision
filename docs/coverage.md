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

| Public header | Client guide | Compiled example | Screenshot | Test family |
|---|---|---|---|---|
| `include/cvision/widgets/application_shell.hpp` | [Hello](tutorial-hello.md) | hello | hello-initial | test_hello_golden |
| `include/cvision/widgets/button.hpp` | [Gallery](widget-gallery.md#button) | forms | forms-initial | test_widgets, test_forms_smoke |
| `include/cvision/widgets/canvas.hpp` | [Graphics](graphics.md) | graphics | graphics-sixel-canvas | test_graphics_smoke |
| `include/cvision/widgets/combo_box.hpp` | [Gallery](widget-gallery.md#combobox) | forms/workbench | forms-initial | test_combo_box |
| `include/cvision/widgets/command_presentation.hpp` | [Commands](dialogs-and-commands.md) | hello | hello-menu-open | test_command |
| `include/cvision/widgets/common_components.hpp` | [Gallery](widget-gallery.md) | forms/workbench | forms-wizard-ready/workbench-help | test_common_components |
| `include/cvision/widgets/desktop.hpp` | [Object model](object-model.md) | gallery | gallery-initial | test_desktop |
| `include/cvision/widgets/dialog.hpp` | [Dialogs](dialogs-and-commands.md) | forms | forms-invalid-dialog | test_dialog |
| `include/cvision/widgets/dialog_presentation.hpp` | [Dialogs](dialogs-and-commands.md) | forms | forms-invalid-dialog | test_dialog_presentation |
| `include/cvision/widgets/directory_picker.hpp` | [Platform services](platform-services.md) | forms | forms-info-message | test_directory_picker |
| `include/cvision/widgets/editor_document.hpp` | [Editor](editor.md) | editor | editor-initial/editor-search/editor-close-confirm | test_editor_document |
| `include/cvision/widgets/editor_search.hpp` | [Editor](editor.md) | editor | editor-search | test_editor_search |
| `include/cvision/widgets/editor_window.hpp` | [Editor](editor.md) | editor | editor-initial/editor-close-confirm | test_editor_window |
| `include/cvision/widgets/file_editor_controller.hpp` | [Editor](editor.md) | editor | editor-initial/editor-close-confirm | test_file_editor_controller |
| `include/cvision/widgets/file_dialog.hpp` | [Platform services](platform-services.md) | filebrowser | filebrowser-initial | test_file_dialog |
| `include/cvision/widgets/flow_view.hpp` | [Flow content](flow-view.md) | workbench | workbench-text | test_flow_view, test_workbench_smoke |
| `include/cvision/widgets/help_viewer.hpp` | [Dialogs](dialogs-and-commands.md) | forms | forms-initial | test_help_viewer |
| `include/cvision/widgets/image_view.hpp` | [Graphics](graphics.md) | graphics | graphics-sixel-image/graphics-no-graphics-image | test_graphics_smoke |
| `include/cvision/widgets/input_line.hpp` | [Gallery](widget-gallery.md#inputline) | forms | forms-initial | test_widgets |
| `include/cvision/widgets/label.hpp` | [Gallery](widget-gallery.md#label) | layouts | layouts-initial | test_widgets |
| `include/cvision/widgets/list_view.hpp` | [Data views](data-views.md) | filebrowser | filebrowser-initial | test_list_view |
| `include/cvision/widgets/memo.hpp` | [Gallery](widget-gallery.md#memo) | workbench | workbench-text | test_memo |
| `include/cvision/widgets/menu.hpp` | [Hello](tutorial-hello.md) | hello | hello-menu-open | test_menu |
| `include/cvision/widgets/message_box.hpp` | [Dialogs](dialogs-and-commands.md) | hello/forms | hello-greeting/forms-info-message | test_message_box |
| `include/cvision/widgets/mnemonic.hpp` | [Gallery](widget-gallery.md#mnemonictext) | forms | forms-initial | test_mnemonic |
| `include/cvision/widgets/mnemonic_internal.hpp` | internal mnemonic paint support | forms/hello | forms-initial/hello-menu-open | test_mnemonic, test_menu |
| `include/cvision/widgets/option_group.hpp` | [Gallery](widget-gallery.md#checkgroup) | forms | forms-initial | test_option_group |
| `include/cvision/widgets/paged_strip.hpp` | [Gallery](widget-gallery.md#pagedstrip) | none yet | none yet | test_paged_strip |
| `include/cvision/widgets/popup_list.hpp` | [Gallery](widget-gallery.md#popuplist) | forms | forms-initial | test_popup_list |
| `include/cvision/widgets/orientation.hpp` | [Gallery](widget-gallery.md#scrollbar) | layouts | layouts-initial | test_scrollbar |
| `include/cvision/widgets/progress.hpp` | [Gallery](widget-gallery.md#progress) | workbench | workbench-data | test_progress |
| `include/cvision/widgets/scroll_viewport.hpp` | [Gallery](widget-gallery.md#scrollviewport) | gallery | gallery-initial | test_scroll_viewport |
| `include/cvision/widgets/scrollbar.hpp` | [Gallery](widget-gallery.md#scrollbar) | gallery | gallery-initial | test_scrollbar |
| `include/cvision/widgets/splitter.hpp` | [Layout](layout-guide.md) | layouts/filebrowser | layouts-initial/filebrowser-initial | test_splitter |
| `include/cvision/widgets/standard_strings.hpp` | [Dialogs](dialogs-and-commands.md) | forms | forms-info-message | test_standard_strings |
| `include/cvision/widgets/static_text.hpp` | [Gallery](widget-gallery.md#statictext) | layouts | layouts-initial | test_widgets |
| `include/cvision/widgets/status_line.hpp` | [Hello](tutorial-hello.md) | hello | hello-initial | test_status_line |
| `include/cvision/widgets/tab_control.hpp` | [Gallery](widget-gallery.md#tabcontrol) | workbench/graphics | workbench-data/graphics-sixel-image | test_tab_control |
| `include/cvision/widgets/table.hpp` | [Data views](data-views.md) | workbench | workbench-data | test_table |
| `include/cvision/widgets/terminal_report_dialog.hpp` | [Gallery](widget-gallery.md#terminal-report-dialog) | spin | gallery-initial | test_terminal_report_dialog |
| `include/cvision/widgets/terminal_scrollbar.hpp` | [Embedded terminal](embedded-terminal.md) | ckvision_terminal | interactive session | test_terminal_scrollbar |
| `include/cvision/widgets/terminal_view.hpp` | [Embedded terminal](embedded-terminal.md) | ckvision_terminal | interactive session | test_terminal_view, test_terminal_app, terminal_redraw_contract |
| `include/cvision/widgets/text_layout.hpp` | [Gallery](widget-gallery.md#wrapmode) | editor | editor-initial | test_text_layout |
| `include/cvision/widgets/text_view.hpp` | [Gallery](widget-gallery.md#textview) | workbench | workbench-text | test_text_view |
| `include/cvision/widgets/text_editor.hpp` | [Editor](editor.md) | editor | editor-initial/editor-search | test_text_editor |
| `include/cvision/widgets/syntax_profile.hpp` | [Editor](editor.md) | editor | editor-initial/editor-json/editor-search | test_syntax_profile |
| `include/cvision/widgets/syntax_cache.hpp` | [Editor](editor.md) | editor | editor-initial/editor-search | test_syntax_cache |
| `include/cvision/widgets/tree_view.hpp` | [Gallery](widget-gallery.md#treeview) | filebrowser | filebrowser-initial | test_tree_view |
| `include/cvision/widgets/window.hpp` | [Object model](object-model.md) | gallery | gallery-initial | test_window |
| `include/cvision/widgets/window_list_dialog.hpp` | [Gallery](widget-gallery.md#window-list-dialog) | gallery | gallery-initial | test_window_list_dialog |
| `include/cvision/widgets/minimized_window_stub.hpp` | [Gallery](widget-gallery.md#minimizedwindowstub) | none yet | none yet | test_minimized_window_stub |
| `include/cvision/widgets/window_switcher_bar.hpp` | [Gallery](widget-gallery.md#windowswitcherbar) | none yet | none yet | test_window_switcher_bar |
