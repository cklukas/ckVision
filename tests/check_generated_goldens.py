# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Compare current generated visual dumps with the pinned bytes on this host."""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import uuid
from pathlib import Path


OUTPUTS = {
    "generate_hello_golden": (
        "hello_initial.dump",
        "hello_greeting.dump",
        "hello_file_menu.dump",
        "hello_rebound.dump",
        "hello_rebound_menu.dump",
    ),
    "generate_editor_goldens": (
        "editor_initial.dump",
        "editor_initial_dark.dump",
        "editor_initial_light.dump",
        "editor_initial_mono.dump",
        "editor_search.dump",
        "editor_search_dark.dump",
        "editor_search_light.dump",
        "editor_search_mono.dump",
        "editor_close_confirm.dump",
        "editor_close_confirm_dark.dump",
        "editor_close_confirm_light.dump",
        "editor_close_confirm_mono.dump",
        "editor_replace.dump",
        "editor_replace_dark.dump",
        "editor_replace_light.dump",
        "editor_replace_mono.dump",
        "editor_menu_file.dump",
        "editor_menu_submenu.dump",
        "editor_menu_escaped.dump",
        "editor_menu_view.dump",
        "editor_menu_no_line_numbers.dump",
        "editor_menu_json.dump",
        "editor_menu_context.dump",
        "editor_menu_context_selection.dump",
        "editor_menu_mouse_drag.dump",
        "editor_menu_mouse_submenu.dump",
        "editor_menu_mouse_dismissed.dump",
        "editor_menu_mouse_context.dump",
    ),
    "generate_forms_goldens": (
        "forms_focus_name.dump",
        "forms_focus_group.dump",
    ),
    "generate_echo_goldens": (
        "echo_initial.dump",
        "echo_scripted.dump",
    ),
    "generate_window_activation_goldens": (
        "window_activation_before.dump",
        "window_activation_after.dump",
    ),
    "generate_root_resize_goldens": (
        "root_resize_grow.dump",
        "root_resize_shrink.dump",
        "root_resize_regrow.dump",
    ),
    "generate_desktop_dialog_goldens": (
        "desktop_dialog_open.dump",
        "desktop_dialog_cycled.dump",
        "desktop_dialog_closed.dump",
    ),
    "generate_shrink_storm_goldens": (
        "shrink_storm_full.dump",
        "shrink_storm_degraded.dump",
        "shrink_storm_too_small.dump",
        "shrink_storm_recovered.dump",
    ),
    "generate_window_resize_goldens": (
        "anchored_dialog_before.dump",
        "anchored_dialog_after.dump",
        "frame_overlays_initial.dump",
        "frame_overlays_resized.dump",
        "frame_overlays_relabelled.dump",
    ),
    "generate_layouts_goldens": (
        "layouts_anchor_initial.dump",
        "layouts_row_initial.dump",
        "layouts_column_initial.dump",
        "layouts_grid_initial.dump",
        "layouts_dock_initial.dump",
        "layouts_overlay_initial.dump",
        "layouts_anchor_wide.dump",
        "layouts_row_wide.dump",
        "layouts_column_wide.dump",
        "layouts_grid_wide.dump",
        "layouts_dock_wide.dump",
        "layouts_overlay_wide.dump",
        "layouts_anchor_narrow.dump",
        "layouts_row_narrow.dump",
        "layouts_column_narrow.dump",
        "layouts_grid_narrow.dump",
        "layouts_dock_narrow.dump",
        "layouts_overlay_narrow.dump",
        "layouts_keyboard.dump",
    ),
    "generate_filebrowser_goldens": (
        "filebrowser_initial.dump",
        "filebrowser_expanded.dump",
        "filebrowser_splitter_moved.dump",
    ),
    "generate_window_z_order_golden": ("window_z_order_junction.dump",),
    "generate_clip_sweep_goldens": (
        "clip_sweep_label.dump",
        "clip_sweep_static_text.dump",
        "clip_sweep_button.dump",
        "clip_sweep_input_line.dump",
        "clip_sweep_list_view.dump",
        "clip_sweep_table.dump",
        "clip_sweep_menu.dump",
        "clip_sweep_status_line.dump",
        "clip_sweep_text_view.dump",
        "clip_sweep_tab_control.dump",
    ),
    "generate_hostile_content_goldens": (
        "paste_recovery.dump",
        "hostile_display_text.dump",
    ),
    "generate_event_script_goldens": (
        "rootdialog_initial.dump",
        "rootdialog_veto.dump",
        "rootdialog_filled.dump",
        "rootdialog_accepted.dump",
        "rootdialog_cancelled.dump",
        "close_veto_initial.dump",
        "close_veto_refused.dump",
        "close_veto_review_open.dump",
        "close_veto_review_refused.dump",
        "close_veto_quit_refused.dump",
        "menu_initial.dump",
        "menu_submenu.dump",
        "menu_keyboard_escaped.dump",
        "menu_keyboard_disabled.dump",
        "menu_keyboard_context.dump",
        "menu_mouse_drag.dump",
        "menu_mouse_context.dump",
        "menu_mouse_dismissed.dump",
        "text_view_hyperlinks_initial.dump",
        "text_view_hyperlinks_initial_presented.dump",
        "text_view_hyperlinks_scrolled.dump",
        "text_view_hyperlinks_scrolled_presented.dump",
    ),
    "generate_theme_override_goldens": (
        "theme_override_classic.dump",
        "theme_override_dark.dump",
        "theme_override_light.dump",
        "theme_override_mono.dump",
    ),
    "generate_terminal_plane_goldens": (
        "terminal_initial.rgba",
        "terminal_initial.dump",
        "terminal_initial_dark.rgba",
        "terminal_initial_dark.dump",
        "terminal_initial_light.rgba",
        "terminal_initial_light.dump",
        "terminal_initial_mono.rgba",
        "terminal_initial_mono.dump",
        "terminal_full_screen.rgba",
        "terminal_full_screen.dump",
        "terminal_full_screen_dark.rgba",
        "terminal_full_screen_dark.dump",
        "terminal_full_screen_light.rgba",
        "terminal_full_screen_light.dump",
        "terminal_full_screen_mono.rgba",
        "terminal_full_screen_mono.dump",
        "terminal_nested.rgba",
        "terminal_nested.dump",
        "terminal_nested_dark.rgba",
        "terminal_nested_dark.dump",
        "terminal_nested_light.rgba",
        "terminal_nested_light.dump",
        "terminal_nested_mono.rgba",
        "terminal_nested_mono.dump",
        "terminal_nested_sixel.rgba",
        "terminal_nested_sixel.dump",
        "terminal_nested_sixel_dark.rgba",
        "terminal_nested_sixel_dark.dump",
        "terminal_nested_sixel_light.rgba",
        "terminal_nested_sixel_light.dump",
        "terminal_nested_sixel_mono.rgba",
        "terminal_nested_sixel_mono.dump",
        "protocol_sixel.rgba",
        "protocol_sixel.dump",
        "embedded_child_sixel.rgba",
        "embedded_child_sixel.dump",
        "presenter_occlusion.rgba",
        "presenter_occlusion.dump",
        "presenter_no_graphics.rgba",
        "presenter_no_graphics.dump",
        "presenter_move.rgba",
        "presenter_move.dump",
        "gallery_sixel.rgba",
        "gallery_sixel.dump",
        "gallery_no_graphics.dump",
        "contained_initial.rgba",
        "contained_initial.dump",
        "contained_initial_dark.rgba",
        "contained_initial_dark.dump",
        "contained_initial_light.rgba",
        "contained_initial_light.dump",
        "contained_initial_mono.rgba",
        "contained_initial_mono.dump",
        "contained_move.rgba",
        "contained_move.dump",
        "contained_resize.rgba",
        "contained_resize.dump",
        "contained_shadow.rgba",
        "contained_shadow.dump",
        "contained_shadow_union.rgba",
        "contained_shadow_union.dump",
        "contained_unshadow.rgba",
        "contained_unshadow.dump",
        "contained_overlap.rgba",
        "contained_overlap.dump",
        "contained_clear.rgba",
        "contained_clear.dump",
        "contained_repaint.rgba",
        "contained_repaint.dump",
        "contained_close.rgba",
        "contained_close.dump",
        "contained_storm_initial.rgba",
        "contained_storm_initial.dump",
        "contained_storm_shrink.rgba",
        "contained_storm_shrink.dump",
        "contained_storm_move.rgba",
        "contained_storm_move.dump",
        "contained_storm_restore.rgba",
        "contained_storm_restore.dump",
        "contained_storm_burst.rgba",
        "contained_storm_burst.dump",
        "contained_no_graphics.rgba",
        "contained_no_graphics.dump",
        "contained_no_graphics_dark.rgba",
        "contained_no_graphics_dark.dump",
        "contained_no_graphics_light.rgba",
        "contained_no_graphics_light.dump",
        "contained_no_graphics_mono.rgba",
        "contained_no_graphics_mono.dump",
        "contained_pair_initial.rgba",
        "contained_pair_initial.dump",
        "contained_pair_peer_closed.rgba",
        "contained_pair_peer_closed.dump",
        "contained_pair_reopened.rgba",
        "contained_pair_reopened.dump",
        "contained_text_over_raster.rgba",
        "contained_text_over_raster.dump",
    ),
    "generate_raster_scroll_goldens": (
        "raster_scroll_initial.scene",
        "raster_scroll_initial.dump",
        "raster_scroll_initial.rgba",
        "raster_scroll_initial_no_graphics.dump",
        "raster_scroll_viewport_1.scene",
        "raster_scroll_viewport_1.dump",
        "raster_scroll_viewport_1.rgba",
        "raster_scroll_viewport_1_no_graphics.dump",
        "raster_scroll_viewport_4.scene",
        "raster_scroll_viewport_4.dump",
        "raster_scroll_viewport_4.rgba",
        "raster_scroll_viewport_4_no_graphics.dump",
        "raster_scroll_flow_1.scene",
        "raster_scroll_flow_1.dump",
        "raster_scroll_flow_1.rgba",
        "raster_scroll_flow_1_no_graphics.dump",
        "raster_scroll_flow_4.scene",
        "raster_scroll_flow_4.dump",
        "raster_scroll_flow_4.rgba",
        "raster_scroll_flow_4_no_graphics.dump",
    ),
    "generate_gallery_goldens": (
        "gallery_picture_initial.scene",
        "gallery_picture_initial.dump",
        "gallery_picture_initial.rgba",
        "gallery_picture_initial_no_graphics.dump",
        "gallery_picture_scrolled_1.scene",
        "gallery_picture_scrolled_1.dump",
        "gallery_picture_scrolled_1.rgba",
        "gallery_picture_scrolled_1_no_graphics.dump",
        "gallery_picture_scrolled_page.scene",
        "gallery_picture_scrolled_page.dump",
        "gallery_picture_scrolled_page.rgba",
        "gallery_picture_scrolled_page_no_graphics.dump",
        "gallery_picture_menu.scene",
        "gallery_picture_menu.dump",
        "gallery_picture_menu.rgba",
        "gallery_picture_menu_no_graphics.dump",
        "gallery_scheme_dark.scene",
        "gallery_scheme_dark.dump",
        "gallery_scheme_dark.rgba",
        "gallery_scheme_dark_no_graphics.dump",
        "gallery_scheme_light.scene",
        "gallery_scheme_light.dump",
        "gallery_scheme_light.rgba",
        "gallery_scheme_light_no_graphics.dump",
        "gallery_scheme_mono.scene",
        "gallery_scheme_mono.dump",
        "gallery_scheme_mono.rgba",
        "gallery_scheme_mono_no_graphics.dump",
        "gallery_scheme_classic.scene",
        "gallery_scheme_classic.dump",
        "gallery_scheme_classic.rgba",
        "gallery_scheme_classic_no_graphics.dump",
    ),
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--scratch", type=Path, required=True)
    parser.add_argument("--generator", action="append", required=True, metavar="NAME=PATH")
    args = parser.parse_args()

    generators = {}
    for entry in args.generator:
        name, separator, path = entry.partition("=")
        if not separator or name not in OUTPUTS or name in generators:
            parser.error(f"invalid generator: {entry}")
        generators[name] = Path(path)
    if set(generators) != set(OUTPUTS):
        parser.error("all golden generators must be supplied")

    scratch = args.scratch.resolve()
    scratch.mkdir(parents=True, exist_ok=True)
    session = scratch / uuid.uuid4().hex
    session.mkdir()
    manifest = hashlib.sha256()
    failures = []
    for name, executable in generators.items():
        output = session / name
        output.mkdir()
        environment = os.environ.copy()
        environment.update(TMPDIR=str(output), TMP=str(output), TEMP=str(output))
        completed = subprocess.run(
            [str(executable), str(output)],
            capture_output=True,
            text=True,
            check=False,
            env=environment,
        )
        if completed.returncode:
            failures.append(f"{name}: exit {completed.returncode}: {completed.stderr.strip()}")
            continue
        expected_names = set(OUTPUTS[name])
        actual_names = {file.name for file in output.iterdir() if file.is_file()}
        if actual_names != expected_names:
            failures.append(f"{name}: expected {sorted(expected_names)}, got {sorted(actual_names)}")
            continue
        for filename in OUTPUTS[name]:
            actual = (output / filename).read_bytes()
            pinned = (args.fixtures / filename).read_bytes()
            if actual != pinned:
                failures.append(
                    f"{filename}: generated {len(actual)} bytes, pinned {len(pinned)} bytes; "
                    f"SHA-256 {hashlib.sha256(actual).hexdigest()} != "
                    f"{hashlib.sha256(pinned).hexdigest()}"
                )
            manifest.update(filename.encode("utf-8") + b"\0")
            manifest.update(len(actual).to_bytes(8, "little"))
            manifest.update(actual)
    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        print(f"generated files retained at {session}", file=sys.stderr)
        return 1
    shutil.rmtree(session)
    count = sum(len(files) for files in OUTPUTS.values())
    print(f"generated golden bytes: {count}/{count} match; manifest SHA-256 {manifest.hexdigest()}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except OSError as error:
        print(f"generated golden check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
