# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Synchronize the widget gallery's value-type field tables with the headers.

A descriptor's data members ARE its configuration, so the gallery lists
them — and a list of fields is exactly the kind of documentation that goes
quietly wrong when a field is renamed, defaulted differently, or removed.
This is the same contract `extract_snippets.py` gives the C++ samples: the
table is generated from the declaration, and the check fails when the two
disagree, in either direction. A new field is as much a documentation bug
as a departed one.

Only plain aggregates are covered. A class with methods is documented by a
hand-written "Configure" table instead, because what its methods MEAN is
not derivable from their signatures.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

BLOCK = re.compile(
    r'<!-- ckvision-fields type="(?P<type>\w+)" -->\n(?P<table>.*?)\n<!-- /ckvision-fields -->',
    re.DOTALL)

MEMBER = re.compile(
    r"^\s{4}(?P<type>[A-Za-z_][\w:<>,\s\*&\[\]]*?)\s+(?P<name>[a-z_][\w]*)"
    r"\s*(?:=\s*(?P<default>[^;]+?)|(?P<braced>\{[^;]*\}))?\s*;"
    r"\s*(?://\s*(?P<comment>.*))?$")

# Lines that look like a member declaration but are not one.
NOT_A_MEMBER = {"friend", "return", "using", "static"}


def headers(root: pathlib.Path) -> list[pathlib.Path]:
    return sorted((root / "include/cvision/widgets").glob("*.hpp"))


def struct_fields(root: pathlib.Path, type_name: str) -> list[tuple[str, str, str]]:
    """(name, type, default) for each data member of `struct <type_name>`."""
    for header in headers(root):
        lines = header.read_text(encoding="utf-8").splitlines()
        for index, line in enumerate(lines):
            if not re.match(rf"^struct {re.escape(type_name)}\b(?!;)", line):
                continue
            if line.rstrip().endswith(";"):  # a forward declaration
                continue
            fields: list[tuple[str, str, str]] = []
            depth = 0
            for raw in lines[index:]:
                depth += raw.count("{") - raw.count("}")
                match = MEMBER.match(raw)
                if match and match.group("type").split()[0] not in NOT_A_MEMBER:
                    braced = (match.group("braced") or "").strip()
                    fields.append((match.group("name"), match.group("type").strip(),
                                   (match.group("default") or braced).strip()))
                if depth == 0 and raw.rstrip().endswith("};"):
                    break
            return fields
    return []


def rendered_table(fields: list[tuple[str, str, str]]) -> str:
    rows = ["| Field | Type | Default |", "|---|---|---|"]
    for name, type_text, default in fields:
        rows.append(f"| `{name}` | `{type_text}` | {f'`{default}`' if default else '—'} |")
    return "\n".join(rows)


def process(root: pathlib.Path, document: pathlib.Path, write: bool) -> list[str]:
    original = document.read_text(encoding="utf-8")
    errors: list[str] = []

    def replace(match: re.Match[str]) -> str:
        type_name = match.group("type")
        fields = struct_fields(root, type_name)
        if not fields:
            errors.append(
                f"{document.relative_to(root)}: no public struct {type_name} with data members "
                "under include/cvision/widgets — was it renamed, or turned into a class?")
            return match.group(0)
        expected = rendered_table(fields)
        if not write and match.group("table").strip() != expected:
            errors.append(
                f"{document.relative_to(root)}: the field table for {type_name} no longer "
                "matches its declaration; run tools/docgen/sync_field_tables.py --write")
        return (f'<!-- ckvision-fields type="{type_name}" -->\n{expected}\n'
                "<!-- /ckvision-fields -->")

    updated, count = BLOCK.subn(replace, original)
    if count == 0 and "ckvision-fields" in original:
        errors.append(f"{document.relative_to(root)}: malformed ckvision-fields marker")
    if write and updated != original:
        document.write_text(updated, encoding="utf-8")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--write", action="store_true",
                        help="rewrite each table from its header declaration")
    arguments = parser.parse_args()
    root = arguments.root.resolve()
    errors: list[str] = []
    tables = 0
    for document in sorted((root / "docs").glob("*.md")):
        tables += document.read_text(encoding="utf-8").count('<!-- ckvision-fields type=')
        errors.extend(process(root, document, arguments.write))
    if tables == 0:
        errors.append("no generated field tables found under docs/")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"validated {tables} header-backed field tables")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
