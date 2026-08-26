# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Synchronize source-backed C++ snippets embedded in client documentation."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

MARKER = re.compile(
    r'<!-- ckvision-snippet source="(?P<source>[^"]+)" '
    r'(?:lines="(?P<first>\d+)-(?P<last>\d+)"|region="(?P<region>[A-Za-z0-9_.-]+)") -->\n'
    r'```cpp\n(?P<code>.*?)```\n<!-- /ckvision-snippet -->',
    re.DOTALL,
)

# A named region is delimited in the C++ source itself, so it survives every
# edit that moves the code — which a `lines=` range does not, and the widget
# gallery cites well over a hundred of these.
REGION_BEGIN = "// ckvision-doc: "
REGION_END = "// ckvision-doc-end: "


def source_lines(root: pathlib.Path, source_name: str) -> list[str]:
    source = (root / source_name).resolve()
    if root not in source.parents or not source.is_file():
        raise ValueError(f"snippet source is outside the repository or missing: {source_name}")
    return source.read_text(encoding="utf-8").splitlines(keepends=True)


def rendered_snippet(root: pathlib.Path, source_name: str, first: int, last: int) -> str:
    lines = source_lines(root, source_name)
    if first < 1 or last < first or last > len(lines):
        raise ValueError(f"invalid line range {first}-{last} for {source_name} ({len(lines)} lines)")
    return "".join(lines[first - 1:last])


def dedented(lines: list[str]) -> str:
    """Removes the indentation the region carried purely from its enclosing scope."""
    indents = [len(line) - len(line.lstrip()) for line in lines if line.strip()]
    common = min(indents) if indents else 0
    return "".join(line[common:] if line.strip() else line.lstrip(" ") for line in lines)


def rendered_region(root: pathlib.Path, source_name: str, region: str) -> str:
    """Extracts one `// ckvision-doc: <region>` … `// ckvision-doc-end: <region>` block.

    The delimiters live in compiled source, so what a reader copies out of the
    documentation is exactly what the capture that produced the screenshot
    beside it executed.
    """
    lines = source_lines(root, source_name)
    begin = REGION_BEGIN + region
    end = REGION_END + region
    starts = [i for i, line in enumerate(lines) if line.strip() == begin]
    ends = [i for i, line in enumerate(lines) if line.strip() == end]
    if len(starts) != 1 or len(ends) != 1:
        raise ValueError(
            f"region {region!r} must be delimited exactly once in {source_name} "
            f"({len(starts)} begin, {len(ends)} end markers)")
    if ends[0] <= starts[0] + 1:
        raise ValueError(f"region {region!r} in {source_name} is empty")
    return dedented(lines[starts[0] + 1:ends[0]])


def process_document(root: pathlib.Path, document: pathlib.Path, write: bool) -> list[str]:
    original = document.read_text(encoding="utf-8")
    errors: list[str] = []

    def replace(match: re.Match[str]) -> str:
        source = match.group("source")
        region = match.group("region")
        if region is not None:
            selector = f'region="{region}"'
            origin = f"{source}:{region}"
        else:
            first = int(match.group("first"))
            last = int(match.group("last"))
            selector = f'lines="{first}-{last}"'
            origin = f"{source}:{first}-{last}"
        try:
            expected = (rendered_region(root, source, region) if region is not None
                        else rendered_snippet(root, source, first, last))
        except ValueError as error:
            errors.append(f"{document.relative_to(root)}: {error}")
            return match.group(0)
        if not write and match.group("code") != expected:
            errors.append(
                f"{document.relative_to(root)}: stale snippet from {origin}; "
                "a region= snippet only needs --write; for lines= "
                "FIRST check the range still points at the intended code, "
                "THEN run tools/docgen/extract_snippets.py --write. --write trusts "
                "the cited ranges: if the source gained or lost lines (a header "
                "sweep, a refactor), it rewrites the docs to match the stale range "
                "and this check goes green over wrong snippets — fix the ranges "
                "first (found the expensive way, 2026-08-20)"
            )
        return (
            f'<!-- ckvision-snippet source="{source}" {selector} -->\n'
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
