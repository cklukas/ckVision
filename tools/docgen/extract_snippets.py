# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Synchronize source-backed C++ snippets embedded in client documentation."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

MARKER = re.compile(
    r'<!-- ckvision-snippet source="(?P<source>[^"]+)" lines="(?P<first>\d+)-(?P<last>\d+)" -->\n'
    r'```cpp\n(?P<code>.*?)```\n<!-- /ckvision-snippet -->',
    re.DOTALL,
)


def rendered_snippet(root: pathlib.Path, source_name: str, first: int, last: int) -> str:
    source = (root / source_name).resolve()
    if root not in source.parents or not source.is_file():
        raise ValueError(f"snippet source is outside the repository or missing: {source_name}")
    lines = source.read_text(encoding="utf-8").splitlines(keepends=True)
    if first < 1 or last < first or last > len(lines):
        raise ValueError(f"invalid line range {first}-{last} for {source_name} ({len(lines)} lines)")
    return "".join(lines[first - 1:last])


def process_document(root: pathlib.Path, document: pathlib.Path, write: bool) -> list[str]:
    original = document.read_text(encoding="utf-8")
    errors: list[str] = []

    def replace(match: re.Match[str]) -> str:
        source = match.group("source")
        first = int(match.group("first"))
        last = int(match.group("last"))
        try:
            expected = rendered_snippet(root, source, first, last)
        except ValueError as error:
            errors.append(f"{document.relative_to(root)}: {error}")
            return match.group(0)
        if not write and match.group("code") != expected:
            errors.append(
                f"{document.relative_to(root)}: stale snippet from {source}:{first}-{last}; "
                "FIRST check the lines= range still points at the intended code, "
                "THEN run tools/docgen/extract_snippets.py --write. --write trusts "
                "the cited ranges: if the source gained or lost lines (a header "
                "sweep, a refactor), it rewrites the docs to match the stale range "
                "and this check goes green over wrong snippets — fix the ranges "
                "first (found the expensive way, 2026-08-20)"
            )
        return (
            f'<!-- ckvision-snippet source="{source}" lines="{first}-{last}" -->\n'
            f"```cpp\n{expected}```\n<!-- /ckvision-snippet -->"
        )

    updated, count = MARKER.subn(replace, original)
    if count == 0 and "ckvision-snippet" in original:
        errors.append(f"{document.relative_to(root)}: malformed ckvision snippet marker")
    if write and updated != original:
        document.write_text(updated, encoding="utf-8")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--write", action="store_true", help="replace snippet bodies from their source ranges. Trusts the "
                    "cited lines= ranges: after a source gained or lost lines, fix "
                    "the ranges FIRST or this rewrites the docs to match the stale "
                    "range and the check passes over wrong snippets")
    arguments = parser.parse_args()
    root = arguments.root.resolve()
    documents = sorted((root / "docs").glob("*.md"))
    errors: list[str] = []
    snippet_count = 0
    for document in documents:
        snippet_count += document.read_text(encoding="utf-8").count("<!-- ckvision-snippet ")
        errors.extend(process_document(root, document, arguments.write))
    if snippet_count == 0:
        errors.append("no source-backed snippets found under docs/")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"validated {snippet_count} source-backed documentation snippets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
