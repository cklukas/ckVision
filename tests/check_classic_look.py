#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Hold the pinned goldens to the classic-look specification.

the internal plans specifies the Classic scheme's visual baseline and ends
in a numbered checklist. Every checklist item carries one or more
``classic-check`` blocks naming golden dumps (docs/golden-format.md) and the
cells that show the item; this gate reads those blocks out of the
specification itself and verifies each named cell's glyph and style, so the
prose, the cited cells and the check cannot drift apart. Styles are named in
the specification's style legend, whose rows state each style exactly as a
dump's style line spells it.

The specification's palette table is the output of
tools/docgen/generate_classic_palette_table; given ``--palette-tool``, the
gate runs it and fails when the embedded table differs. The colour legend
must name exactly the colours that table uses.

Check-line grammar (one per line inside a ```classic-check fence):

    <golden> row <r> col <c> "<text>" <styles>
    <golden> rows <r0>-<r1> cols <c0>-<c1> ["<glyph>"] <style>

<golden> is relative to tests/golden/. Rows and columns are 0-based cells.
<styles> is one legend name for every cell, or a comma-separated list with
one name per cell; ``name*N`` repeats a name N times. The second form checks
every cell of an inclusive rectangle for one style and, if given, one glyph.
A cited row must hold single-width cells only, so a code point is a column.

``--self-test`` proves the gate rejects a mutated glyph, a mutated style, a
mutated style line, a malformed or empty checklist, a wide row, a drifted
palette and a stale colour legend, and that each item of the real
specification fails when the first cell it cites is mutated.
"""

from __future__ import annotations

import argparse
import dataclasses
import pathlib
import re
import subprocess
import sys
import unicodedata

ROOT = pathlib.Path(__file__).resolve().parent.parent

MARKER = "<!-- classic-look:{name}:{edge} -->"
STYLE_ATTRS = ("bold", "dim", "italic", "underline", "reverse", "strike")


class SpecError(Exception):
    """The specification or a golden cannot be read as this gate requires."""


# --- golden dumps -----------------------------------------------------------


@dataclasses.dataclass
class Golden:
    cols: int
    rows: int
    styles: list[str]
    grid: list[str]
    stylemap: list[str]

    def row_cells(self, row: int, name: str) -> str:
        if not 0 <= row < self.rows:
            raise SpecError(f"{name}: row {row} is outside the {self.cols}x{self.rows} frame")
        text = self.grid[row]
        single = len(text) == self.cols and not any(
            unicodedata.east_asian_width(ch) in ("W", "F") or unicodedata.combining(ch) for ch in text)
        if not single:
            raise SpecError(f"{name}: row {row} holds wide or combining content; cite a row of "
                            "single-width cells")
        return text

    def cell(self, row: int, col: int, name: str) -> tuple[str, str]:
        text = self.row_cells(row, name)
        if not 0 <= col < self.cols:
            raise SpecError(f"{name}: column {col} is outside the {self.cols}x{self.rows} frame")
        return text[col], self.styles[style_index(self.stylemap[row][col])]


def style_index(ch: str) -> int:
    if ch.isdigit():
        return ord(ch) - ord("0")
    if "A" <= ch <= "Z":
        return 10 + ord(ch) - ord("A")
    if "a" <= ch <= "z":
        return 36 + ord(ch) - ord("a")
    raise SpecError(f"stylemap character {ch!r} is not in the 0-9A-Za-z alphabet")


def parse_golden(text: str, name: str) -> Golden:
    lines = text.split("\n")
    if not text.endswith("\n"):
        raise SpecError(f"{name}: a golden dump ends with a newline")
    lines.pop()
    at = 0

    def take(prefix: str) -> str:
        nonlocal at
        if at >= len(lines) or not lines[at].startswith(prefix):
            raise SpecError(f"{name}: line {at + 1} should start with {prefix!r}")
        at += 1
        return lines[at - 1]

    if take("ckvision-golden ") != "ckvision-golden 1":
        raise SpecError(f"{name}: only golden format version 1 is understood")
    frame = take("frame ").split()
    cols, rows = int(frame[1]), int(frame[2])
    take("cursor ")
    count = int(take("styles ").split()[1])
    styles = []
    for index in range(count):
        head, _, body = take(f"{index} ").partition(" ")
        styles.append(body)
    take("grid")
    grid = [take("|")[1:-1] for _ in range(rows)]
    take("stylemap")
    stylemap = [take("|")[1:-1] for _ in range(rows)]
    for number, line in enumerate(stylemap):
        if len(line) != cols:
            raise SpecError(f"{name}: stylemap row {number} is not {cols} cells wide")
        for ch in line:
            if style_index(ch) >= count:
                raise SpecError(f"{name}: stylemap row {number} names undeclared style {ch!r}")
    while at < len(lines) and lines[at].startswith("raster "):
        at += 1
    take("end")
    if at != len(lines):
        raise SpecError(f"{name}: text follows 'end'")
    return Golden(cols, rows, styles, grid, stylemap)


# --- the specification ------------------------------------------------------


@dataclasses.dataclass
class Check:
    item: int
    line: str
    golden: str
    row0: int
    row1: int
    col0: int
    col1: int
    text: str | None     # the exact cells of a run, or the glyph every cell of an area holds
    styles: list[str]    # one legend name per cell of a run, or the one name of an area
    area: bool


@dataclasses.dataclass
class Spec:
    legend: dict[str, str]
    colours: set[str]
    palette: str
    items: dict[int, list[Check]]


def section(text: str, name: str) -> str:
    begin, end = MARKER.format(name=name, edge="begin"), MARKER.format(name=name, edge="end")
    if text.count(begin) != 1 or text.count(end) != 1:
        raise SpecError(f"the specification needs exactly one {begin} ... {end} section")
    body = text.split(begin, 1)[1].split(end, 1)[0]
    return body.strip("\n") + "\n"


def table_rows(body: str) -> list[list[str]]:
    rows = []
    for line in body.splitlines():
        line = line.strip()
        if not line.startswith("|") or re.fullmatch(r"\|(\s*:?-+:?\s*\|)+", line):
            continue
        rows.append([cell.strip().strip("`") for cell in line.strip("|").split("|")])
    return rows[1:]  # drop the header row


def parse_legend(body: str) -> dict[str, str]:
    legend: dict[str, str] = {}
    for cells in table_rows(body):
        if len(cells) < 4:
            raise SpecError(f"style legend row {cells} needs name, foreground, background, attributes")
        name, fg, bg, attrs = cells[:4]
        if not re.fullmatch(r"[a-z][a-z0-9-]*", name) or name == "any":
            raise SpecError(f"style legend name {name!r} is not a lowercase-hyphenated identifier")
        for colour in (fg, bg):
            if not re.fullmatch(r"#[0-9A-F]{6}|default|@\d{1,3}", colour):
                raise SpecError(f"style {name}: {colour!r} is not a golden-format colour")
        if attrs != "-" and any(a not in STYLE_ATTRS for a in attrs.split(",")):
            raise SpecError(f"style {name}: {attrs!r} is not a golden-format attribute list")
        if name in legend:
            raise SpecError(f"style legend names {name!r} twice")
        legend[name] = f"fg {fg} bg {bg} attrs {attrs}"
    if not legend:
        raise SpecError("the style legend is empty")
    return legend


def parse_colours(body: str) -> set[str]:
    colours = set()
    for cells in table_rows(body):
        found = [cell for cell in cells if re.fullmatch(r"#[0-9A-F]{6}", cell)]
        if len(found) != 1:
            raise SpecError(f"colour legend row {cells} needs exactly one #RRGGBB cell")
        colours.add(found[0])
    return colours


CHECK_RUN = re.compile(r'(\S+) row (\d+) col (\d+) "((?:[^"\\]|\\.)+)" (\S+)')
CHECK_AREA = re.compile(r'(\S+) rows (\d+)-(\d+) cols (\d+)-(\d+)(?: "((?:[^"\\]|\\.))")? (\S+)')


def expand_styles(spec: str) -> list[str]:
    names = []
    for part in spec.split(","):
        name, _, repeat = part.partition("*")
        if repeat and not repeat.isdigit():
            raise SpecError(f"style repetition {part!r} is not name*N")
        names.extend([name] * (int(repeat) if repeat else 1))
    return names


def unescape(text: str) -> str:
    return re.sub(r"\\(.)", r"\1", text)


def parse_check(item: int, line: str) -> Check:
    if match := CHECK_RUN.fullmatch(line):
        golden, row, col, text, styles = match.groups()
        text = unescape(text)
        names = expand_styles(styles)
        if len(names) == 1:
            names = names * len(text)
        if len(names) != len(text):
            raise SpecError(f"item {item}: {len(text)} cells but {len(names)} styles in: {line}")
        return Check(item, line, golden, int(row), int(row), int(col), int(col) + len(text) - 1,
                     text, names, False)
    if match := CHECK_AREA.fullmatch(line):
        golden, r0, r1, c0, c1, glyph, style = match.groups()
        if int(r0) > int(r1) or int(c0) > int(c1):
            raise SpecError(f"item {item}: empty rectangle in: {line}")
        return Check(item, line, golden, int(r0), int(r1), int(c0), int(c1),
                     unescape(glyph) if glyph else None, [style], True)
    raise SpecError(f"item {item}: malformed check line: {line}")


def parse_checklist(body: str) -> dict[int, list[Check]]:
    items: dict[int, list[Check]] = {}
    current: int | None = None
    fenced = False
    for raw in body.splitlines():
        line = raw.strip()
        if fenced:
            if line.startswith("```"):
                fenced = False
            elif line and not line.startswith("#"):
                items[current].append(parse_check(current, line))
            continue
        if line.startswith("```classic-check"):
            if current is None:
                raise SpecError("a classic-check block precedes the first checklist item")
            fenced = True
            continue
        if line.startswith("```"):
            raise SpecError(f"only classic-check blocks belong in the checklist: {line}")
        if match := re.match(r"(\d+)\. ", raw):
            current = int(match.group(1))
            if current in items or current != len(items) + 1:
                raise SpecError(f"checklist item {current} is out of sequence (expected {len(items) + 1})")
            items[current] = []
    if fenced:
        raise SpecError("an unterminated classic-check block ends the checklist")
    if not items:
        raise SpecError("the checklist has no items")
    return items


def parse_spec(text: str) -> Spec:
    spec = Spec(parse_legend(section(text, "styles")), parse_colours(section(text, "colours")),
                section(text, "palette"), parse_checklist(section(text, "checklist")))
    used = {name for checks in spec.items.values() for check in checks for name in check.styles}
    for number, checks in spec.items.items():
        if not any(check.text is not None and any(name != "any" for name in check.styles) for check in checks):
            raise SpecError(f"checklist item {number} names no glyph with a checked style")
    unknown = sorted(used - set(spec.legend) - {"any"})
    if unknown:
        raise SpecError("checks name styles absent from the legend: " + ", ".join(unknown))
    unused = sorted(set(spec.legend) - used)
    if unused:
        raise SpecError("the style legend defines styles no check uses: " + ", ".join(unused))
    return spec


# --- verification -----------------------------------------------------------


def verify_check(check: Check, golden: Golden, legend: dict[str, str]) -> list[str]:
    failures = []
    for row in range(check.row0, check.row1 + 1):
        for col in range(check.col0, check.col1 + 1):
            glyph, style = golden.cell(row, col, check.golden)
            offset = col - check.col0
            want_glyph = check.text if check.area else check.text[offset]
            want_name = check.styles[0] if check.area else check.styles[offset]
            if want_glyph is not None and glyph != want_glyph:
                failures.append(f"row {row} col {col}: glyph {glyph!r}, specified {want_glyph!r}")
            if want_name != "any" and style != legend[want_name]:
                failures.append(f"row {row} col {col}: style '{style}', specified {want_name} "
                                f"'{legend[want_name]}'")
    return failures


def verify(spec: Spec, goldens: dict[str, Golden]) -> list[str]:
    failures = []
    for number, checks in spec.items.items():
        for check in checks:
            if check.golden not in goldens:
                raise SpecError(f"item {number}: no golden {check.golden}")
            for failure in verify_check(check, goldens[check.golden], spec.legend):
                failures.append(f"item {number}: {check.golden}: {failure}")
    return failures


def palette_colours(palette: str) -> set[str]:
    return set(re.findall(r"#[0-9A-F]{6}", palette))


def verify_palette(spec: Spec, tool_output: str | None) -> list[str]:
    failures = []
    if tool_output is not None and tool_output.strip("\n") + "\n" != spec.palette:
        failures.append("the palette table differs from generate_classic_palette_table's output; "
                        "regenerate it into the specification")
    used = palette_colours(spec.palette)
    if spec.colours != used:
        missing = sorted(used - spec.colours)
        extra = sorted(spec.colours - used)
        failures.append("the colour legend does not name exactly the palette's colours"
                        + (f"; missing {', '.join(missing)}" if missing else "")
                        + (f"; unused {', '.join(extra)}" if extra else ""))
    return failures


def load_goldens(spec: Spec, golden_root: pathlib.Path) -> dict[str, Golden]:
    goldens = {}
    for name in sorted({check.golden for checks in spec.items.values() for check in checks}):
        path = golden_root / name
        if not path.is_file():
            raise SpecError(f"no golden {name} under {golden_root}")
        goldens[name] = parse_golden(path.read_text(encoding="utf-8"), name)
    return goldens


def run_palette_tool(tool: pathlib.Path) -> str:
    result = subprocess.run([str(tool)], check=False, capture_output=True, text=True, encoding="utf-8")
    if result.returncode != 0:
        raise SpecError(f"{tool} exited with {result.returncode}: {result.stderr.strip()}")
    return result.stdout


def check(spec_path: pathlib.Path, golden_root: pathlib.Path, tool: pathlib.Path | None) -> int:
    try:
        spec = parse_spec(spec_path.read_text(encoding="utf-8"))
        goldens = load_goldens(spec, golden_root)
        failures = verify(spec, goldens) + verify_palette(spec, run_palette_tool(tool) if tool else None)
    except SpecError as error:
        print(f"classic-look: ERROR: {error}", file=sys.stderr)
        return 1
    for failure in failures:
        print(f"classic-look: FAIL: {failure}", file=sys.stderr)
    if failures:
        return 1
    cells = sum((c.row1 - c.row0 + 1) * (c.col1 - c.col0 + 1) for cs in spec.items.values() for c in cs)
    print(f"classic-look: OK: {len(spec.items)} checklist items, {cells} cells in {len(goldens)} goldens"
          + (", palette table current" if tool else ", palette table not regenerated (no --palette-tool)"))
    return 0


# --- self-test --------------------------------------------------------------

FIXTURE_GOLDEN = """ckvision-golden 1
frame 6 3
cursor hidden
styles 2
0 fg #0000AA bg #C8C8C8 attrs -
1 fg #FFFFFF bg #0000AA attrs bold
grid
|░╔══╗░|
|░║  ║░|
|░╚══╝░|
stylemap
|011110|
|011110|
|011110|
end
"""

FIXTURE_PALETTE = """| Family | Role | Foreground | Background | Attributes |
|---|---|---|---|---|
| desktop | `ckv.desktop.background` | `#0000AA` | `#C8C8C8` | `-` |
| window | `ckv.window.frame.active` | `#FFFFFF` | `#0000AA` | `bold` |
"""

FIXTURE_SPEC = """# Fixture

<!-- classic-look:colours:begin -->
| Name | RGB |
|---|---|
| blue | `#0000AA` |
| light gray | `#C8C8C8` |
| white | `#FFFFFF` |
<!-- classic-look:colours:end -->

<!-- classic-look:styles:begin -->
| Style | Foreground | Background | Attributes | Meaning |
|---|---|---|---|---|
| `desktop` | `#0000AA` | `#C8C8C8` | `-` | the desktop |
| `frame` | `#FFFFFF` | `#0000AA` | `bold` | an active frame |
<!-- classic-look:styles:end -->

<!-- classic-look:palette:begin -->
{palette}<!-- classic-look:palette:end -->

<!-- classic-look:checklist:begin -->
1. The frame is double-line.

   ```classic-check
   fixture.dump row 0 col 1 "╔══╗" frame
   fixture.dump row 2 col 0 "░╚══╝░" desktop,frame*4,desktop
   ```

2. The desktop shows its pattern.

   ```classic-check
   fixture.dump rows 0-2 cols 0-0 "░" desktop
   ```
<!-- classic-look:checklist:end -->
"""


def fixture_failures(spec_text: str, golden_text: str, tool_output: str | None = FIXTURE_PALETTE) -> list[str]:
    spec = parse_spec(spec_text)
    goldens = {"fixture.dump": parse_golden(golden_text, "fixture.dump")}
    return verify(spec, goldens) + verify_palette(spec, tool_output)


def expect(condition: bool, message: str) -> bool:
    if not condition:
        print(f"classic-look self-test: {message}", file=sys.stderr)
    return condition


def expect_rejected(spec_text: str, golden_text: str, what: str, tool_output: str | None = FIXTURE_PALETTE) -> bool:
    try:
        return expect(bool(fixture_failures(spec_text, golden_text, tool_output)), f"accepted {what}")
    except SpecError:
        return True


def mutated_cell(golden: Golden, row: int, col: int, glyph: str | None, style: str | None) -> Golden:
    copy = Golden(golden.cols, golden.rows, list(golden.styles), list(golden.grid), list(golden.stylemap))
    if glyph is not None:
        text = copy.grid[row]
        copy.grid[row] = text[:col] + glyph + text[col + 1:]
    if style is not None:
        copy.styles.append(style)
        marks = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
        line = copy.stylemap[row]
        copy.stylemap[row] = line[:col] + marks[len(copy.styles) - 1] + line[col + 1:]
    return copy


def real_items_detect_mutation(spec_path: pathlib.Path, golden_root: pathlib.Path) -> bool:
    """Every item of the real specification fails once its first cited cell changes."""
    spec = parse_spec(spec_path.read_text(encoding="utf-8"))
    goldens = load_goldens(spec, golden_root)
    ok = expect(not verify(spec, goldens), "the real specification does not pass on the pinned goldens")
    for number, checks in spec.items.items():
        first = next(c for c in checks if c.text is not None and any(n != "any" for n in c.styles))
        offset = 0 if first.area else next(i for i, n in enumerate(first.styles) if n != "any")
        row, col = first.row0, first.col0 + offset
        glyph, _ = goldens[first.golden].cell(row, col, first.golden)
        for label, mutation in (("glyph", ("¤" if glyph != "¤" else "#", None)),
                                ("style", (None, "fg #123456 bg #654321 attrs strike"))):
            trial = dict(goldens)
            trial[first.golden] = mutated_cell(goldens[first.golden], row, col, *mutation)
            flagged = any(f.startswith(f"item {number}:") for f in verify(spec, trial))
            ok = expect(flagged, f"item {number} does not notice a mutated {label} at "
                                 f"{first.golden} row {row} col {col}") and ok
    return ok


def self_test(spec_path: pathlib.Path, golden_root: pathlib.Path) -> int:
    spec = FIXTURE_SPEC.format(palette=FIXTURE_PALETTE)
    ok = expect(not fixture_failures(spec, FIXTURE_GOLDEN), "the fixture does not pass as written")
    ok = expect_rejected(spec, FIXTURE_GOLDEN.replace("|░╔══╗░|", "|░┌══╗░|"), "a mutated glyph") and ok
    ok = expect_rejected(spec, FIXTURE_GOLDEN.replace("|011110|\n|011110|\n|011110|",
                                                      "|011110|\n|011110|\n|010110|"),
                         "a mutated stylemap cell") and ok
    ok = expect_rejected(spec, FIXTURE_GOLDEN.replace("1 fg #FFFFFF bg #0000AA attrs bold",
                                                      "1 fg #FFFFFF bg #0000AA attrs -"),
                         "a mutated style line") and ok
    ok = expect_rejected(spec.replace('row 0 col 1 "╔══╗" frame', 'row 0 col 1 "╔══╗" frame,desktop'),
                         FIXTURE_GOLDEN, "a check whose style count disagrees with its cells") and ok
    ok = expect_rejected(spec.replace("2. The desktop", "3. The desktop"), FIXTURE_GOLDEN,
                         "a checklist that skips a number") and ok
    ok = expect_rejected(spec.replace('   fixture.dump rows 0-2 cols 0-0 "░" desktop\n', ""), FIXTURE_GOLDEN,
                         "a checklist item without a check") and ok
    ok = expect_rejected(spec.replace("fixture.dump row 0 col 1", "fixture.dump row 0 column 1"),
                         FIXTURE_GOLDEN, "a malformed check line") and ok
    ok = expect_rejected(spec, FIXTURE_GOLDEN.replace("|░║  ║░|", "|░║表║░|").replace(
        "fixture.dump rows 0-2", "fixture.dump rows 1-1"), "a cited row of wide cells") and ok
    ok = expect_rejected(spec, FIXTURE_GOLDEN, "a palette table that drifted from the tool",
                         FIXTURE_PALETTE.replace("`bold`", "`-`")) and ok
    ok = expect_rejected(spec.replace("| white | `#FFFFFF` |\n", ""), FIXTURE_GOLDEN,
                         "a colour legend missing a palette colour") and ok
    ok = expect_rejected(spec.replace(
        "<!-- classic-look:styles:end -->",
        "| `unused` | `#000000` | `#000000` | `-` | never cited |\n<!-- classic-look:styles:end -->"),
        FIXTURE_GOLDEN, "a legend style no check uses") and ok
    if spec_path.is_file():
        try:
            ok = real_items_detect_mutation(spec_path, golden_root) and ok
        except SpecError as error:
            ok = expect(False, f"the real specification cannot be read: {error}")
    if not ok:
        return 1
    print("classic-look self-test: OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", type=pathlib.Path, default=ROOT)
    parser.add_argument("--palette-tool", type=pathlib.Path)
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()
    root = arguments.root.resolve()
    spec_path, golden_root = root / "plans" / "classic-look.md", root / "tests" / "golden"
    if arguments.self_test:
        return self_test(spec_path, golden_root)
    return check(spec_path, golden_root, arguments.palette_tool)


if __name__ == "__main__":
    sys.exit(main())
