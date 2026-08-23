#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Enforce the 60-line Hello example gate from the roadmap M9.

The count covers non-comment, non-blank physical lines in the three
Hello application files. It exits non-zero on a regression.
"""

import sys
from pathlib import Path

BUDGET = 60
FILES = ["hello_app.hpp", "hello_app.cpp", "main.cpp"]


def code_lines(path: Path) -> int:
    count = 0
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue
        count += 1
    return count


def main() -> int:
    root = Path(__file__).resolve().parent.parent / "examples" / "hello"
    total = 0
    for name in FILES:
        n = code_lines(root / name)
        print(f"hello-line-budget: {name}: {n} lines")
        total += n
    print(f"hello-line-budget: total {total} / {BUDGET}")
    if total > BUDGET:
        print(f"hello-line-budget: FAIL -- {total} exceeds the {BUDGET}-line budget",
              file=sys.stderr)
        return 1
    print("hello-line-budget: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
