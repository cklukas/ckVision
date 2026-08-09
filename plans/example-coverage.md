Copyright (c) 2026 C. Klukas. All rights reserved.

# WP-36A example coverage matrix

This matrix is the authoritative WP-36A mapping from public client surface to
example, executable evidence, visual/golden evidence, and documentation. WP-38
consumes this file for the final release-gate traceability pass.

Rows marked `WP-38 visual` have executable public-path coverage now, but their
complete state × theme × visual-capture grid is intentionally part of WP-38's
release-gate matrix.

| Public surface / capability | Example | Scripted interaction | Golden / visual artifact | Reference documentation |
|---|---|---|---|---|
| Minimal application shell, menu bar, status line, info dialog, quit command | `examples/hello` | `test_hello_golden.cpp` | `tests/golden/hello*.dump`; `hello_visual_capture` | `docs/hello-example.md`; `docs/example-apps.md` |
| Menu/status command presentation, F10, Esc focus restore, standard Quit | `examples/gallery`, `examples/layouts`, `examples/workbench` | `test_gallery_smoke.cpp`; `test_layouts_smoke.cpp`; `test_workbench_smoke.cpp` | `gallery_visual_capture`; WP-38 visual for layouts/workbench | `docs/example-apps.md`; `docs/standard-commands.md` |
| Desktop windows, activation, tile/cascade, frame shadows | `examples/gallery`; `examples/filebrowser` | `test_gallery_smoke.cpp`; `test_filebrowser_smoke.cpp`; `test_desktop.cpp` | `gallery_visual_capture`; `tests/golden/window_*.dump` | `docs/example-apps.md`; `plans/widgets.md` |
| Row, Column, alignment, margins, height-for-width | `examples/layouts` | `test_layouts_smoke.cpp` | WP-38 visual | `docs/example-apps.md`; `plans/widgets.md` |
| Grid | `examples/layouts` | `test_layouts_smoke.cpp`; `test_grid.cpp` | WP-38 visual | `docs/example-apps.md`; `plans/widgets.md` |
| Dock and docked chrome | `examples/layouts`; `examples/filebrowser` | `test_layouts_smoke.cpp`; `test_filebrowser_smoke.cpp`; `test_dock.cpp` | `filebrowser_visual_capture`; WP-38 visual for layouts | `docs/example-apps.md` |
| AnchorPane and terminal resize recovery | `examples/layouts` | `test_layouts_smoke.cpp`; `test_anchor_pane.cpp`; `test_wp36.cpp` | `tests/golden/root_resize_*.dump`; WP-38 visual | `docs/example-apps.md` |
| Overlay and frame overlays | `examples/layouts`; `examples/filebrowser` | `test_layouts_smoke.cpp`; `test_filebrowser_smoke.cpp`; `test_overlay.cpp` | `filebrowser_visual_capture`; WP-38 visual for layouts | `docs/example-apps.md` |
| Splitter keyboard adjustment | `examples/layouts`; `examples/filebrowser` | `test_layouts_smoke.cpp`; `test_filebrowser_smoke.cpp`; `test_splitter.cpp` | WP-38 visual | `docs/example-apps.md` |
| Label, StaticText, Button, InputLine | `examples/gallery`; `examples/forms`; `examples/workbench` | `test_gallery_smoke.cpp`; `test_forms_smoke.cpp`; `test_workbench_smoke.cpp` | `gallery_visual_capture`; WP-38 visual for forms/workbench | `docs/example-apps.md`; `plans/widgets.md` |
| Option groups and ComboBox | `examples/forms`; `examples/workbench` | `test_forms_smoke.cpp`; `test_workbench_smoke.cpp`; `test_option_group.cpp`; `test_combo_box.cpp` | WP-38 visual | `docs/example-apps.md`; `plans/widgets.md` |
| Descriptor dialogs, validation veto, default/cancel, focus restore | `examples/forms` | `test_forms_smoke.cpp`; `test_dialog.cpp`; `test_dialog_presentation.cpp` | WP-38 visual | `docs/example-apps.md`; `plans/widgets.md` |
| Message boxes and localized StandardStrings | `examples/hello`; `examples/gallery`; `examples/forms` | `test_hello_golden.cpp`; `test_gallery_smoke.cpp`; `test_forms_smoke.cpp`; `test_standard_strings.cpp` | `hello_visual_capture`; `gallery_visual_capture`; WP-38 visual for forms | `docs/example-apps.md` |
| Help viewer and help/status context hints | `examples/forms`; `examples/workbench` | `test_forms_smoke.cpp`; `test_help_viewer.cpp`; `test_status_line.cpp` | WP-38 visual | `docs/example-apps.md`; `plans/widgets.md` |
| File dialog, directory picker, injected filesystem | `examples/filebrowser` plus standard dialog suites | `test_filebrowser_smoke.cpp`; `test_file_dialog.cpp`; `test_directory_picker.cpp`; `test_filesystem.cpp` | `filebrowser_visual_capture`; WP-38 visual for dialog states | `docs/example-apps.md`; `plans/widgets.md` |
| Window list dialog | `examples/forms` standard-dialog coverage row | `test_window_list_dialog.cpp`; `test_forms_smoke.cpp` covers help/message/dialog pathways | WP-38 visual | `docs/example-apps.md`; `plans/widgets.md` |
| Memo, clipboard, undo, word wrap | `examples/workbench` | `test_workbench_smoke.cpp`; `test_memo.cpp` | WP-38 visual | `docs/example-apps.md`; `plans/widgets.md` |
| TextView links and OSC 8 export | `examples/workbench` | `test_workbench_smoke.cpp`; `test_text_view.cpp` | WP-38 visual | `docs/example-apps.md`; `plans/widgets.md` |
| ListView, TreeView, Table, TabControl, Progress | `examples/workbench`; `examples/filebrowser` | `test_workbench_smoke.cpp`; `test_filebrowser_smoke.cpp`; `test_table.cpp`; `test_tree_view.cpp`; `test_tab_control.cpp`; `test_progress.cpp` | `filebrowser_visual_capture`; WP-38 visual for workbench | `docs/example-apps.md`; `plans/widgets.md` |
| ImageView Sixel/fallback/degradation | `examples/gallery`; `examples/graphics` | `test_gallery_smoke.cpp`; `test_graphics_smoke.cpp`; `test_image_view.cpp` | `gallery_visual_capture`; `tests/golden/gallery_*.visual`; WP-38 visual for graphics | `docs/example-apps.md`; `docs/example-apps.md#graphics-examplesgraphics` |
| Canvas metric-derived raster and fallback | `examples/graphics` | `test_graphics_smoke.cpp`; `test_canvas.cpp` | WP-38 visual | `docs/example-apps.md`; `plans/widgets.md` |
| Cell and pixel mouse forwarding for graphics | `examples/graphics` | `test_graphics_smoke.cpp`; `test_canvas.cpp`; `test_image_view.cpp` | WP-38 visual | `docs/example-apps.md`; `plans/widgets.md` |
| Public example hygiene: public headers, typed insertion, no duplicate commands/chords | all examples | `example_hygiene` | not visual | `docs/example-apps.md` |
