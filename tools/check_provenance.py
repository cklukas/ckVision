#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Reject prohibited claim patterns under the decision log D-012.

This deliberately narrow lexical guard supplements mandatory human review; it
cannot establish the history of arbitrary text or behavior.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


RULES: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("source comparison", re.compile(
        r"\b(?:checked|compare[ds]?|comparison|matched|matching)\b.{0,120}"
        r"\b(?:against|with)\b.{0,120}\b(?:source|implementation|tutorial|project)\b",
        re.IGNORECASE | re.DOTALL)),
    ("fidelity claim", re.compile(
        r"\b(?:faithful|line[- ]by[- ]line|direct)\s+"
        r"(?:port|translation|conversion|reproduction|copy)\b",
        re.IGNORECASE)),
    ("external-source claim", re.compile(
        r"\b(?:Turbo[ -]?Vision|another|external|private)\b.{0,120}"
        r"\b(?:source|tutorial|implementation)\b.{0,120}"
        r"\b(?:identical|same|faithful|copied|transcribed|translated|matched)\b",
        re.IGNORECASE | re.DOTALL)),
    ("verbatim transcription", re.compile(
        r"\b(?:copied|transcribed|reproduced)\s+(?:verbatim|from)\b",
        re.IGNORECASE)),
    ("source-fidelity claim", re.compile(r"\b(?:exact\s+)?source\s+fidelity\b", re.IGNORECASE)),
)

TEXT_SUFFIXES = {
    ".cc", ".cmake", ".cpp", ".h", ".hpp", ".md", ".py", ".sh", ".txt", ".yml", ".yaml",
}
TEXT_NAMES = {"AGENTS.md", "CMakeLists.txt"}
SELF_PATH = Path("tools/check_provenance.py")


def violations(text: str) -> list[str]:
    findings: list[str] = []
    for name, pattern in RULES:
        for match in pattern.finditer(text):
            line = text.rfind("\n", 0, match.start()) + 1
            line_end = text.find("\n", match.end())
            context = text[line:len(text) if line_end < 0 else line_end].lower()
            if any(marker in context for marker in ("no ", "never", "do not", "must not", "reject", "prohibited")):
                continue
            findings.append(name)
            break
    return findings


def repository_files(root: Path) -> list[Path]:
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-co", "--exclude-standard"],
        check=True, capture_output=True, text=True)
    files: list[Path] = []
    for relative in result.stdout.splitlines():
        if Path(relative) == SELF_PATH:
            continue  # The separate self-test validates this checker's own rules and fixtures.
        path = root / relative
        if not path.is_file():
            continue
        if path.name in TEXT_NAMES or path.suffix.lower() in TEXT_SUFFIXES:
            files.append(path)
    return files


def check_repository(root: Path) -> int:
    failures = 0
    for path in repository_files(root):
        found = violations(path.read_text(encoding="utf-8"))
        if not found:
            continue
        relative = path.relative_to(root)
        print(f"provenance-check: FAIL: {relative}: {', '.join(found)}", file=sys.stderr)
        failures += 1
    if failures:
        return 1
    print("provenance-check: OK")
    return 0


def self_test() -> int:
    rejected = (
        "The behavior was " + "".join(("check", "ed")) + " against another implementation's source.",
        "This is a " + "".join(("faith", "ful")) + " port of a private tutorial.",
        "The wording was " + "".join(("cop", "ied")) + " verbatim from an external project.",
        "Exact " + "".join(("sou", "rce")) + " fidelity is the goal.",
    )
    accepted = (
        "No external source was consulted; behavior follows VISION.md and its goldens.",
        "This example is independently specified and has a ckVision-owned visual contract.",
    )
    for sample in rejected:
        if not violations(sample):
            print(f"provenance-check self-test: failed to reject {sample!r}", file=sys.stderr)
            return 1
    for sample in accepted:
        if violations(sample):
            print(f"provenance-check self-test: rejected clean text {sample!r}", file=sys.stderr)
            return 1
    print("provenance-check self-test: OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    root = args.root if args.root is not None else Path(__file__).resolve().parent.parent
    return check_repository(root.resolve())


if __name__ == "__main__":
    sys.exit(main())
