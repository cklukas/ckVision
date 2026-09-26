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
| `include/cvision/widgets/big_clock.hpp` | [Gallery](widget-gallery.md#bigclockview) | widget_shots | widget-bigclockview | test_big_clock |
| `include/cvision/widgets/button.hpp` | [Gallery](widget-gallery.md#button) | forms/widget_shots | widget-button | test_widgets, test_forms_smoke |
| `include/cvision/widgets/canvas.hpp` | [Graphics](graphics.md) | graphics/widget_shots | widget-canvas/widget-canvas-no-graphics | test_graphics_smoke |
| `include/cvision/widgets/combo_box.hpp` | [Gallery](widget-gallery.md#combobox) | forms/workbench/widget_shots | widget-combobox/widget-combobox-open | test_combo_box |
| `include/cvision/widgets/command_presentation.hpp` | [Commands](dialogs-and-commands.md) | hello/widget_shots | widget-toolbar/hello-menu-open | test_command |
| `include/cvision/widgets/common_components.hpp` | [Gallery](widget-gallery.md) | forms/workbench/widget_shots | widget-calendarview/widget-clockview/widget-wizard/widget-notificationcenter/widget-tooltip | test_common_components, test_tooltip_controller |
| `include/cvision/widgets/desktop.hpp` | [Object model](object-model.md) | gallery/widget_shots | widget-desktop | test_desktop |
| `include/cvision/widgets/dialog.hpp` | [Dialogs](dialogs-and-commands.md) | forms/widget_shots | widget-dialogdescriptor | test_dialog |
| `include/cvision/widgets/dialog_presentation.hpp` | [Dialogs](dialogs-and-commands.md) | forms/widget_shots | widget-messagebox | test_dialog_presentation |
| `include/cvision/widgets/date_time_dialog.hpp` | [Dialogs](dialogs-and-commands.md#date-and-time-dialogs) | widget_shots | widget-datedialog/widget-timedialog | test_date_time_dialog |
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
| `include/cvision/widgets/key_chord_capture.hpp` | [Gallery](widget-gallery.md#keychordcapture) | workbench/widget_shots | widget-keychordcapture | test_key_chord_capture, test_workbench_smoke |
| `include/cvision/widgets/frame_text.hpp` | [Gallery](widget-gallery.md#frametext) | widget_shots, EditorWindow | widget-frametext | test_frame_text |
| `include/cvision/widgets/label.hpp` | [Gallery](widget-gallery.md#label) | layouts/widget_shots | widget-label | test_widgets |
| `include/cvision/widgets/list_view.hpp` | [Data views](data-views.md) | filebrowser/widget_shots | widget-listview | test_list_view |
| `include/cvision/widgets/memo.hpp` | [Gallery](widget-gallery.md#memo) | workbench/widget_shots | widget-memo | test_memo |
| `include/cvision/widgets/menu.hpp` | [Hello](tutorial-hello.md) | hello/widget_shots | widget-menubar/widget-menubar-overflow/widget-dropdownmenu | test_menu |
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
| `include/cvision/widgets/status_line.hpp` | [Hello](tutorial-hello.md) | hello/forms/widget_shots | widget-statusline | test_status_line, test_hello_golden, test_forms_smoke |
| `include/cvision/widgets/tab_control.hpp` | [Gallery](widget-gallery.md#tabcontrol) | workbench/graphics/widget_shots | widget-tabcontrol | test_tab_control |
| `include/cvision/widgets/table.hpp` | [Data views](data-views.md) | workbench/widget_shots | widget-table/widget-table-editing | test_table |
| `include/cvision/widgets/cell_grid.hpp` | [Data views](data-views.md#cell-grid-providers) | widget_shots | widget-cellgrid | test_cell_grid |
| `include/cvision/widgets/terminal_report_dialog.hpp` | [Gallery](widget-gallery.md#terminal-report-dialog) | spin/widget_shots | widget-terminalreportdialog | test_terminal_report_dialog |
| `include/cvision/widgets/terminal_scrollbar.hpp` | [Embedded terminal](embedded-terminal.md) | ckvision_terminal | interactive session | test_terminal_scrollbar |
| `include/cvision/widgets/terminal_view.hpp` | [Embedded terminal](embedded-terminal.md) | ckvision_terminal | terminal-initial | test_terminal_view, test_terminal_app, terminal_redraw_contract |
| `include/cvision/widgets/text_layout.hpp` | [Gallery](widget-gallery.md#wrapmode) | editor/widget_shots | widget-textview/widget-memo | test_text_layout |
| `include/cvision/widgets/text_view.hpp` | [Gallery](widget-gallery.md#textview) | workbench/widget_shots | widget-textview | test_text_view |
| `include/cvision/widgets/theme_editor.hpp` | [Themes](themes-and-rendering.md#the-theme-editor) | workbench/widget_shots | widget-themeeditor | test_theme_editor, test_workbench_smoke |
| `include/cvision/widgets/text_editor.hpp` | [Editor](editor.md); [TODO](todo-example.md#full-note-editor) | editor/todo/widget_shots | widget-texteditor/todo-note-editor | test_text_editor, test_todo_smoke |
| `include/cvision/widgets/syntax_profile.hpp` | [Editor](editor.md) | editor/widget_shots | widget-texteditor/editor-json | test_syntax_profile |
| `include/cvision/widgets/syntax_cache.hpp` | [Editor](editor.md) | editor/widget_shots | widget-texteditor | test_syntax_cache |
| `include/cvision/widgets/tree_view.hpp` | [Data views](data-views.md#tree-providers) | filebrowser/widget_shots | widget-treeview | test_tree_view |
| `include/cvision/widgets/window.hpp` | [Object model](object-model.md) | gallery/widget_shots | widget-window | test_window |
| `include/cvision/widgets/window_list_dialog.hpp` | [Gallery](widget-gallery.md#window-list-dialog) | gallery/widget_shots | widget-windowlistdialog | test_window_list_dialog |
| `include/cvision/widgets/minimized_window_stub.hpp` | [Gallery](widget-gallery.md#minimizedwindowstub) | widget_shots | widget-minimizedwindowstub | test_minimized_window_stub |
| `include/cvision/widgets/window_switcher_bar.hpp` | [Gallery](widget-gallery.md#windowswitcherbar) | widget_shots | widget-windowswitcherbar | test_window_switcher_bar |

## Interaction scripts

Every public view type is also driven by at least one Application-level
interaction script, and `interaction_script_coverage` checks this table
against the tests (`tools/docgen/check_interaction_scripts.py`, with a
`--self-test`). A script is a `CK_TEST` that reaches the view in a real
`Application` and sends it input the way a terminal does: through
`Application::dispatch`, `HeadlessTerminal::inject_event` or
`Application::step`. The gate requires, for each row:

- the view type is one the documentation-coverage gate enumerates, and every
  such type has a row;
- the named case exists in the named file under `tests/`;
- the case calls `dispatch`, `inject_event` or `step`, directly or through a
  helper the same test file defines;
- the case never calls a view's `on_key`, `on_key_release`, `on_mouse`,
  `on_text` or `on_paste` handler itself, directly or through such a helper;
- the case names the "Reached through" identifier: the type itself, or the
  factory, example or fixture that builds it.

| View type | Script test case | Reached through |
|---|---|---|
| `BigClockView` | `test_big_clock.cpp` `any_key_or_a_click_on_a_focused_face_asks_for_it_to_be_dismissed` | `BigClockView` |
| `BreadcrumbBar` | `test_common_components.cpp` `a_scripted_breadcrumb_bar_walks_and_activates_its_segments` | `BreadcrumbBar` |
| `Button` | `test_disabled_state.cpp` `a_press_on_a_disabled_button_never_fires_it_and_reaches_its_enabled_container` | `Button` |
| `CalendarDropdown` | `test_common_components.cpp` `a_hosted_date_picker_opens_the_calendar_and_tracks_its_typed_selection` | `CalendarDropdown` |
| `CalendarView` | `test_common_components.cpp` `a_scripted_calendar_view_moves_by_keys_and_selects_the_clicked_day` | `CalendarView` |
| `Canvas` | `test_canvas.cpp` `a_scripted_canvas_redraws_at_its_new_pixel_size_and_forwards_both_coordinate_spaces` | `Canvas` |
| `CellGrid` | `test_cell_grid.cpp` `a_scripted_cell_grid_is_walked_selected_and_activated_through_dispatched_input` | `CellGrid` |
| `CheckGroup` | `test_option_group.cpp` `alt_and_the_caption_letter_focus_the_group_from_anywhere_in_its_window` | `CheckGroup` |
| `ClockView` | `test_common_components.cpp` `a_clock_repaints_when_the_time_changes_and_not_on_every_tick` | `ClockView` |
| `ComboBox` | `test_combo_box.cpp` `a_combo_drops_a_popup_list_and_takes_the_row_chosen_from_it` | `ComboOnDesktop` |
| `CommandPalette` | `test_common_components.cpp` `a_scripted_command_palette_filters_as_typed_and_runs_the_highlighted_command` | `CommandPalette` |
| `DatePicker` | `test_common_components.cpp` `a_hosted_date_picker_opens_the_calendar_and_tracks_its_typed_selection` | `DatePicker` |
| `Desktop` | `test_m8_integration.cpp` `clicking_deep_content_in_a_background_window_raises_and_activates_it_through_dispatch` | `Desktop` |
| `DropdownMenu` | `test_menu.cpp` `the_whole_menu_system_is_operable_by_mouse_alone` | `MenuScript` |
| `EditorWindow` | `test_editor_window.cpp` `a_scripted_editor_window_marks_typed_changes_and_keeps_its_close_control_vetoed` | `EditorWindow` |
| `FlowView` | `test_flow_view.cpp` `a_scripted_flow_view_scrolls_and_activates_links_by_key_by_click_and_by_wheel` | `FlowView` |
| `FrameText` | `test_frame_text.cpp` `a_scripted_frame_text_follows_its_window_through_a_click_that_deactivates_it` | `FrameText` |
| `ImageView` | `test_application.cpp` `a_frame_carrying_a_picture_is_not_followed_by_another_until_the_host_is_ready` | `ImageView` |
| `InputLine` | `test_widgets.cpp` `a_scripted_input_line_edits_selects_copies_recalls_and_undoes_through_dispatched_keys` | `InputLine` |
| `KeyChordCapture` | `test_key_chord_capture.cpp` `a_scripted_chord_capture_shadows_the_bound_command_only_while_it_captures` | `KeyChordCapture` |
| `Label` | `test_widgets.cpp` `alt_mnemonic_on_a_window_focuses_the_labels_buddy` | `Label` |
| `ListView` | `test_list_view.cpp` `a_scripted_list_view_searches_walks_and_activates_by_key_and_by_timed_double_click` | `ListView` |
| `Memo` | `test_memo.cpp` `a_scripted_memo_edits_scrolls_copies_and_undoes_through_dispatched_keys` | `Memo` |
| `MenuBar` | `test_menu.cpp` `the_whole_menu_system_is_operable_by_keyboard_alone` | `MenuScript` |
| `MinimizedWindowStub` | `test_minimized_window_stub.cpp` `a_parked_window_can_be_got_back_without_a_mouse` | `MinimizedWindowStub` |
| `NotificationCenter` | `test_common_components.cpp` `a_scripted_notification_center_expires_toasts_and_dismisses_by_escape_and_click` | `NotificationCenter` |
| `PagedStrip` | `test_paged_strip.cpp` `a_scripted_paged_strip_pages_and_activates_items_through_dispatched_clicks` | `PagedStrip` |
| `PopupList` | `test_popup_list.cpp` `a_popup_list_hangs_under_its_anchor_and_is_as_wide_as_its_longest_item` | `PopupList` |
| `Progress` | `test_progress.cpp` `a_scripted_progress_bar_shows_each_state_its_host_sets_between_steps` | `Progress` |
| `PropertyInspector` | `test_common_components.cpp` `a_scripted_property_inspector_edits_a_value_in_place` | `PropertyInspector` |
| `RadioGroup` | `test_option_group.cpp` `alt_and_the_caption_letter_focus_the_group_from_anywhere_in_its_window` | `RadioGroup` |
| `ScrollViewport` | `test_scroll_viewport.cpp` `application_routes_wheel_events_over_descendant_content_to_the_scroll_viewport` | `ScrollViewport` |
| `Scrollbar` | `test_scrollbar.cpp` `a_scripted_scrollbar_steps_pages_and_drags_through_dispatched_input` | `Scrollbar` |
| `SearchBox` | `test_help_viewer.cpp` `enter_in_the_search_box_does_not_dismiss_the_help_window` | `SearchBox` |
| `Slider` | `test_common_components.cpp` `a_scripted_slider_follows_keys_and_a_proportional_click` | `Slider` |
| `SpinBox` | `test_common_components.cpp` `a_scripted_spin_box_steps_by_keys_by_clicks_on_either_half_and_by_the_wheel` | `SpinBox` |
| `Splitter` | `test_splitter.cpp` `a_scripted_splitter_moves_its_divider_by_keys_and_by_a_dispatched_drag` | `Splitter` |
| `StaticText` | `test_wp36.cpp` `wrapped_static_text_reallocates_height_through_a_hosted_dialog_content_tree` | `StaticText` |
| `StatusLine` | `test_status_line.cpp` `a_scripted_status_line_runs_its_command_by_click_and_by_chord_in_an_application_shell` | `ApplicationShell` |
| `TabControl` | `test_tab_control.cpp` `a_scripted_tab_control_switches_pages_by_arrows_mnemonics_and_clicks` | `TabControl` |
| `Table` | `test_table.cpp` `a_scripted_table_sorts_on_a_header_click_and_edits_a_typed_cell_from_the_keyboard` | `Table` |
| `TerminalView` | `test_terminal_view.cpp` `terminal_view_reencodes_private_child_sixel_only_through_the_outer_presenter` | `TerminalView` |
| `TextEditor` | `test_text_editor.cpp` `text_editor_paints_host_highlights_and_places_a_virtual_caret_past_the_text` | `TextEditor` |
| `TextView` | `test_text_view.cpp` `a_scripted_text_view_scrolls_and_follows_links_through_dispatched_input` | `TextView` |
| `TimePicker` | `test_common_components.cpp` `a_scripted_time_picker_steps_its_fields_and_shows_either_clock_face` | `TimePicker` |
| `ToolBar` | `test_common_components.cpp` `a_scripted_tool_bar_click_runs_its_command_only_while_it_is_enabled` | `ToolBar` |
| `Tooltip` | `test_common_components.cpp` `a_scripted_tooltip_appears_and_leaves_with_the_frames_its_host_steps` | `Tooltip` |
| `TreeView` | `test_tree_view.cpp` `a_scripted_tree_view_expands_walks_and_activates_through_dispatched_input` | `TreeView` |
| `Window` | `test_m8_integration.cpp` `clicking_deep_content_in_a_background_window_raises_and_activates_it_through_dispatch` | `Window` |
| `WindowSwitcherBar` | `test_window_switcher_bar.cpp` `a_lengthening_title_shows_at_once_but_its_button_waits_out_the_grow_delay` | `WindowSwitcherBar` |
| `Wizard` | `test_common_components.cpp` `a_scripted_wizard_goes_forward_back_and_finishes_only_when_allowed` | `Wizard` |
