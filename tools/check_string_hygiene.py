#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Enforce ckVision 2.0 UI string hygiene (UiStrings centralization).

Checks:
1. Standard dialogs (message_box, file_dialog, directory_picker,
   window_list_dialog, terminal_report_dialog, help_viewer, date_time_dialog,
   theme_editor) do not construct Window/Button/Label widgets with raw
   hardcoded English UI literals instead of reading `app.strings()`.
2. Deleted pre-2.0 per-widget string structs (`StandardStrings`,
   `DateTimeLabels`, `PropertyInspectorMessages`, `WizardLabels`) do not
   reappear in include/, src/, examples/, or tests/.
"""

import argparse
import re
import sys
import tempfile
from pathlib import Path

DIALOG_SOURCES = (
    "src/widgets/message_box.cpp",
    "src/widgets/file_dialog.cpp",
    "src/widgets/directory_picker.cpp",
    "src/widgets/window_list_dialog.cpp",
    "src/widgets/terminal_report_dialog.cpp",
    "src/widgets/help_viewer.cpp",
    "src/widgets/date_time_dialog.cpp",
    "src/widgets/theme_editor.cpp",
)

RAW_UI_LITERAL_RE = re.compile(r'make_unique<(?:Window|Button|Label)>\("([^"]+)"')
LEGACY_STRINGS_RE = re.compile(
    r'\b(?:StandardStrings|DateTimeLabels|PropertyInspectorMessages|WizardLabels)\b'
)


def check_root(root: Path) -> int:
    violations = 0
    for rel in DIALOG_SOURCES:
        path = root / rel
        if not path.exists():
            continue
        for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            match = RAW_UI_LITERAL_RE.search(line)
            if match:
                print(
                    f'{rel}:{lineno}: hardcoded dialog UI literal "{match.group(1)}"; '
                    f"read from UiStrings (app.strings()) instead",
                    file=sys.stderr,
                )
                violations += 1

    for folder in ("include", "src", "examples", "tests"):
        base = root / folder
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix not in {".hpp", ".h", ".cpp"}:
                continue
            rel = path.relative_to(root)
            for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
                match = LEGACY_STRINGS_RE.search(line)
                if match:
                    print(
                        f"{rel}:{lineno}: deleted pre-2.0 string type {match.group(0)}; "
                        f"use ckv::ui::UiStrings",
                        file=sys.stderr,
                    )
                    violations += 1
    return violations


def run_self_test() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "src/widgets").mkdir(parents=True)
        (root / "src/widgets/message_box.cpp").write_text(
            'auto b = std::make_unique<Button>("Cancel");\n', encoding="utf-8"
        )
        (root / "include/cvision/ui").mkdir(parents=True)
        (root / "include/cvision/ui/sample.hpp").write_text(
            "struct StandardStrings {};\n", encoding="utf-8"
        )
        if check_root(root) != 2:
            print("string-hygiene self-test failed: expected 2 violations", file=sys.stderr)
            return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return run_self_test()
    return 1 if check_root(args.root) else 0


if __name__ == "__main__":
    raise SystemExit(main())
