#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""WP-35 public-example hygiene gate.

The examples are the client-facing construction surface. They must not retain
the old plumbing/cast/duplicate-command patterns that the library may still use
internally for type-erased implementation seams.
"""

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
EXAMPLES = ROOT / "examples"

PATTERNS = [
    ("widget casts", re.compile(r"static_cast\s*<\s*widgets::")),
    ("Desktop constructor theme plumbing", re.compile(r"Desktop\s*\([^;\n]*theme\s*\(")),
    ("Desktop make_unique theme plumbing", re.compile(r"make_unique\s*<[^>]*Desktop[^;]*theme\s*\(")),
    ("bypassed standard-dialog attachment", re.compile(r"root\s*\(\)\.add_child\s*\([^;]*handle\.window")),
    ("redundant focus-policy defaults", re.compile(r"set_focus_policy\s*\(")),
]

# The field a command's identity is actually declared in. It was `.id` when
# this gate was written and is `.key` now, and the old spelling matched every
# `.id =` in an example -- including an ordinary struct field, which is how
# this rule came to reject correct code while no longer covering a single
# command in the tree. Matching `.key` restores what it was for: two examples
# claiming the same command identity.
COMMAND_KEY = re.compile(r"\.key\s*=\s*([^,\n}]+)")
COMMAND_CHORD = re.compile(r"\.chord\s*=\s*\"([^\"]+)\"")


def example_sources() -> list[Path]:
    return sorted(EXAMPLES.glob("*/*.cpp")) + sorted(EXAMPLES.glob("*/*.hpp"))


def main() -> int:
    failures: list[str] = []
    ids: dict[str, Path] = {}
    chords: dict[str, Path] = {}
    for path in example_sources():
        text = path.read_text(encoding="utf-8")
        rel = path.relative_to(ROOT)
        for label, pattern in PATTERNS:
            for match in pattern.finditer(text):
                line = text.count("\n", 0, match.start()) + 1
                failures.append(f"{rel}:{line}: forbidden {label}")
        for match in COMMAND_KEY.finditer(text):
            command_key = match.group(1).strip()
            previous = ids.setdefault(command_key, rel)
            if previous != rel:
                failures.append(f"{rel}: duplicate command key {command_key} also declared in {previous}")
        for match in COMMAND_CHORD.finditer(text):
            chord = match.group(1)
            previous = chords.setdefault(chord, rel)
            if previous != rel:
                failures.append(f"{rel}: duplicate command chord {chord} also declared in {previous}")

    if failures:
        print("example-hygiene: FAIL", file=sys.stderr)
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1
    print("example-hygiene: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
