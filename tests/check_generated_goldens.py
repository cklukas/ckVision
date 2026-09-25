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
    "generate_hello_golden": ("hello_initial.dump", "hello_greeting.dump"),
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
    ),
    "generate_window_activation_goldens": (
        "window_activation_before.dump",
        "window_activation_after.dump",
    ),
    "generate_root_resize_goldens": (
        "root_resize_grow.dump",
        "root_resize_shrink.dump",
    ),
    "generate_desktop_dialog_goldens": (
        "desktop_dialog_open.dump",
        "desktop_dialog_cycled.dump",
        "desktop_dialog_closed.dump",
    ),
    "generate_window_z_order_golden": ("window_z_order_junction.dump",),
    "generate_terminal_plane_goldens": (
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
