# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Enforce the client-documentation coverage, link, and screenshot contract.

The link check (--links) also requires every test case a page cites to exist
as a CK_TEST in tests/; cited_test_cases() states which citations count. The
screenshot check (--screenshots) holds tools/docgen/screenshot-manifest.txt,
whose groups the <name>_visual_capture tests are registered from, to the
documentation's references and to docs/generated/screenshots/ in both
directions.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys


def published_screenshots(root: pathlib.Path) -> list[str]:
    """The stems of the SVG figures the source tree publishes.

    In a git checkout that is what git tracks or would track, so a figure a
    developer keeps locally under an ignore rule is not taken for a published
    one; in an extracted archive it is every SVG in the directory.
    """
    shots = root / "docs/generated/screenshots"
    if (root / ".git").exists():
        listed = subprocess.run(
            ["git", "ls-files", "-co", "--exclude-standard", "--", "docs/generated/screenshots/*.svg"],
            cwd=root, check=True, capture_output=True, text=True).stdout
        return [pathlib.PurePosixPath(line).stem for line in listed.splitlines() if line]
    return [path.stem for path in shots.glob("*.svg")]


def public_headers(root: pathlib.Path) -> list[str]:
    return [path.relative_to(root).as_posix() for path in sorted((root / "include/cvision").rglob("*.hpp"))]


def public_widget_headers(root: pathlib.Path) -> list[str]:
    return [path.relative_to(root).as_posix() for path in sorted((root / "include/cvision/widgets").glob("*.hpp"))]


def view_types(root: pathlib.Path) -> list[str]:
    """Every public widget type that ends up being drawn on the screen.

    Transitive, because EditorWindow is a Window and WindowSwitcherBar is a
    PagedStrip, and both draw as much as anything that says `: public View`
    outright.
    """
    declarations: list[tuple[str, str]] = []
    for header in public_widget_headers(root):
        for line in (root / header).read_text(encoding="utf-8").splitlines():
            match = re.match(r'^class (?:\[\[nodiscard\]\] )?(\w+)(?: final)? : ([^{]+)', line)
            if match:
                declarations.append((match.group(1), match.group(2)))
    views = {"View", "ui::View"}
    changed = True
    while changed:  # a base may be declared after its derived type
        changed = False
        for name, bases in declarations:
            if name in views:
                continue
            if any(base.strip().removeprefix("public ").strip() in views
                   for base in bases.split(",")):
                views.add(name)
                changed = True
    return sorted(views - {"View", "ui::View"})


def gallery_sections(text: str) -> dict[str, str]:
    """Section body per `## Name` / `### Name` heading in one page."""
    sections: dict[str, str] = {}
    current: str | None = None
    body: list[str] = []
    for line in text.splitlines():
        heading = re.match(r'^#{2,3} (.+)$', line)
        if heading:
            if current is not None:
                sections[current] = "\n".join(body)
            current = heading.group(1).strip()
            body = []
        else:
            body.append(line)
    if current is not None:
        sections[current] = "\n".join(body)
    return sections


def divergent_width_errors(root: pathlib.Path) -> list[str]:
    """The D-019 divergence table in text-width.md names exactly the suite's cases.

    tests/test_text_width_divergent.cpp is the machine-enumerable marker for
    the known terminal-divergent width classes, and the page promises to list
    the same set; a case added to one and not the other breaks that promise.
    """
    suite = root / "tests/test_text_width_divergent.cpp"
    page = root / "docs/text-width.md"
    if not suite.is_file() or not page.is_file():
        return ["tests/test_text_width_divergent.cpp and docs/text-width.md are required"]
    cases = re.findall(r"^CK_TEST\((\w+)\)", suite.read_text(encoding="utf-8"), re.MULTILINE)
    listed = re.findall(r"^\| `(\w+)` \|", page.read_text(encoding="utf-8"), re.MULTILINE)
    errors = [f"text-width.md does not list the D-019 divergent case {name}"
              for name in cases if name not in listed]
    errors += [f"text-width.md lists {name}, which is not a case of test_text_width_divergent.cpp"
               for name in listed if name not in cases]
    errors += [f"text-width.md lists the D-019 divergent case {name} more than once"
               for name in sorted(set(listed)) if listed.count(name) > 1]
    return errors


def markdown_links(text: str) -> list[str]:
    prose_only = re.sub(r'```.*?```', '', text, flags=re.DOTALL)
    return re.findall(r'!?\[[^\]]*\]\(([^)]+)\)', prose_only)


# A test case name as a page cites it: a backticked lowercase snake_case
# identifier. The shape alone is not enough to call something a test
# (`set_vertical_scrollbar_policy` has it too), so cited_test_cases() only
# looks where a page states that it is naming a test.
CITED_NAME = r'`([a-z][a-z0-9]*(?:_[a-z0-9]+)+)`'
TEST_SOURCE = r'`(?:[\w./-]*/)?test_\w+\.cpp`'
INLINE_CITATION = re.compile(TEST_SOURCE + r':?\s*(' + CITED_NAME + r'(?:\s*[,;]\s*' + CITED_NAME + r')*)')


def test_case_names(root: pathlib.Path) -> set[str]:
    """Every `CK_TEST(name)` defined in a C++ source under tests/."""
    names: set[str] = set()
    for source in sorted((root / "tests").rglob("*.cpp")):
        names.update(re.findall(r'\bCK_TEST\((\w+)\)', source.read_text(encoding="utf-8")))
    return names


def cited_test_cases(text: str) -> list[tuple[int, str]]:
    """The test case names a page cites, each once with its 1-based line number.

    Exactly two citation forms count, so an API name mentioned in prose is
    never taken for a test:

    * every backticked snake_case name in the cells of a Markdown table
      column whose header cell reads `Test` or `Tests`, as in the example
      pages' "What's verified" tables;
    * the backticked snake_case names that directly follow a backticked
      `test_*.cpp` source, after an optional colon, as a comma- or
      semicolon-separated list that may wrap onto following lines:
      `test_x.cpp`: `first`; `second`.

    File names never match, because a cited name may not contain a dot or a
    slash. Fenced code blocks are skipped.
    """
    # Blank out fenced blocks but keep their newlines, so line numbers hold.
    prose = re.sub(r'```.*?```', lambda block: "\n" * block.group(0).count("\n"), text, flags=re.DOTALL)
    cited: dict[tuple[int, str], None] = {}
    test_column: int | None = None
    lines = prose.splitlines()
    for number, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped.startswith("|"):
            test_column = None
            continue
        cells = [cell.strip() for cell in stripped.strip("|").split("|")]
        following = lines[number].strip() if number < len(lines) else ""
        if re.fullmatch(r'\|?(\s*:?-+:?\s*\|)+\s*:?-*:?\s*', following):
            # A header row: the next line is the delimiter row.
            test_column = next((index for index, cell in enumerate(cells)
                                if cell.lower() in ("test", "tests")), None)
        elif test_column is not None and test_column < len(cells):
            for name in re.findall(CITED_NAME, cells[test_column]):
                cited[(number, name)] = None
    for citation in INLINE_CITATION.finditer(prose):
        list_start = citation.start(1)
        for name in re.finditer(CITED_NAME, citation.group(1)):
            number = prose.count("\n", 0, list_start + name.start()) + 1
            cited[(number, name.group(1))] = None
    return list(cited)


def read_screenshot_manifest(manifest: pathlib.Path) -> tuple[dict[str, list[str]], list[str]]:
    """The manifest's groups, capture tool -> figure names, and its errors.

    The grammar is cmake/ScreenshotManifest.cmake's: `[capture_<tool>]` opens
    a group, each other non-comment line is a figure name inside one.
    """
    groups: dict[str, list[str]] = {}
    errors: list[str] = []
    current = None
    seen: set[str] = set()
    for number, raw in enumerate(manifest.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        group = re.fullmatch(r"\[(capture_[a-z_]+)\]", line)
        if group:
            current = group.group(1)
            if current in groups:
                errors.append(f"screenshot manifest:{number}: [{current}] appears twice")
            groups.setdefault(current, [])
            continue
        if not re.fullmatch(r"[a-z0-9-]+", line):
            errors.append(f"screenshot manifest:{number}: '{line}' is neither a group nor a figure name")
        elif current is None:
            errors.append(f"screenshot manifest:{number}: '{line}' is not under a [capture_<tool>] group")
        elif line in seen:
            errors.append(f"screenshot manifest:{number}: '{line}' is listed twice")
        else:
            seen.add(line)
            groups[current].append(line)
    for tool, figures in groups.items():
        if not figures:
            errors.append(f"screenshot manifest: [{tool}] names no figure")
    return groups, errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--coverage", action="store_true")
    parser.add_argument("--links", action="store_true")
    parser.add_argument("--screenshots", action="store_true")
    arguments = parser.parse_args()
    if not any((arguments.coverage, arguments.links, arguments.screenshots)):
        arguments.coverage = arguments.links = arguments.screenshots = True
    root = arguments.root.resolve()
    docs = root / "docs"
    errors: list[str] = []
    pages = [root / "README.md", *sorted(docs.glob("*.md"))]
    page_text = {page: page.read_text(encoding="utf-8") for page in pages}

    if arguments.coverage:
        coverage = docs / "coverage.md"
        gallery = docs / "widget-gallery.md"
        api_index = docs / "api-index.md"
        if not coverage.is_file() or not gallery.is_file() or not api_index.is_file():
            errors.append("coverage, widget gallery, and API index pages are required")
        else:
            coverage_text = coverage.read_text(encoding="utf-8")
            gallery_text = gallery.read_text(encoding="utf-8")
            api_text = api_index.read_text(encoding="utf-8")
            listed_headers = re.findall(r"^\| `(include/cvision/[^`]+\.hpp)` \|", api_text, re.MULTILINE)
            installed_headers = set(public_headers(root))
            for header in sorted(installed_headers):
                count = listed_headers.count(header)
                if count != 1:
                    errors.append(f"api-index.md must list installed header {header} exactly once (found {count})")
            for header in sorted(set(listed_headers) - installed_headers):
                errors.append(f"api-index.md lists absent header {header}")
            for header in public_widget_headers(root):
                if header not in coverage_text:
                    errors.append(f"coverage.md omits public widget header {header}")
            widget_names = re.findall(r'^(?:class|struct) (\w+)', "\n".join(
                (root / header).read_text(encoding="utf-8") for header in public_widget_headers(root)), re.MULTILINE)
            for name in widget_names:
                if f"## {name}" not in gallery_text and f"### {name}" not in gallery_text:
                    errors.append(f"widget-gallery.md omits section for public type {name}")
            if "<!-- ckvision-snippet " not in gallery_text:
                errors.append("widget-gallery.md has no source-backed code snippet")
            errors.extend(divergent_width_errors(root))
            # A widget that draws must be shown drawing. The gallery
            # promises "a picture of it running" for every such type, and a
            # promise nothing checks is how this page came to list fifty
            # widgets with no figure between them.
            sections = gallery_sections(gallery_text)
            for name in view_types(root):
                body = sections.get(name)
                if body is None:
                    continue  # the missing-section error above already covers it
                if "generated/screenshots/" not in body:
                    errors.append(
                        f"widget-gallery.md section for the view type {name} has no figure "
                        "(add a scene to tools/docgen/widget_shots_*.cpp and reference its "
                        "capture)")
                if "<!-- ckvision-snippet " not in body:
                    errors.append(
                        f"widget-gallery.md section for the view type {name} has no compiled "
                        "usage sample")

    if arguments.links:
        for page, text in page_text.items():
            for target in markdown_links(text):
                if "://" in target or target.startswith("#") or target.startswith("mailto:"):
                    continue
                target_path = target.split("#", 1)[0]
                if not target_path or target_path.startswith("generated/screenshots/"):
                    continue
                if not (page.parent / target_path).resolve().is_file():
                    errors.append(f"{page.relative_to(root)} has missing local link: {target}")
        # A cited test that was renamed or deleted leaves a page claiming
        # evidence that no longer runs. Every name cited as a test (see
        # cited_test_cases for the exact rule) must be a CK_TEST in tests/.
        defined_tests = test_case_names(root)
        for page in sorted(docs.glob("*.md")):
            for number, name in cited_test_cases(page_text[page]):
                if name not in defined_tests:
                    errors.append(f"{page.relative_to(root)}:{number} cites test case {name}, "
                                  "which no CK_TEST in tests/ defines")

    if arguments.screenshots:
        manifest = root / "tools/docgen/screenshot-manifest.txt"
        if not manifest.is_file():
            errors.append("missing tools/docgen/screenshot-manifest.txt")
        else:
            groups, manifest_errors = read_screenshot_manifest(manifest)
            errors.extend(manifest_errors)
            names = {name for figures in groups.values() for name in figures}
            for tool in groups:
                if not (root / "tools/docgen" / f"{tool}.cpp").is_file():
                    errors.append(f"screenshot manifest group [{tool}] names no tool in tools/docgen/")
            referenced = set()
            for page, text in page_text.items():
                referenced.update(re.findall(r'generated/screenshots/([A-Za-z0-9-]+)\.svg', text))
            missing = sorted(referenced - names)
            if missing:
                errors.append("documentation references screenshots absent from manifest: " + ", ".join(missing))
            unused = sorted(names - referenced)
            if unused:
                errors.append("screenshot manifest has no documentation reference: " + ", ".join(unused))
            # --links skips generated/screenshots/ on purpose, so nothing above
            # ever asks whether the file behind a reference is actually there.
            # A clone renders these pages; an absent capture is a broken image
            # on a published page, not a local inconvenience.
            shots = root / "docs/generated/screenshots"
            absent = sorted(n for n in names if not (shots / f"{n}.svg").is_file())
            if absent:
                errors.append(
                    "manifest screenshots absent from docs/generated/screenshots: "
                    + ", ".join(absent)
                    + " (regenerate with tools/docgen/generate_docs.sh)")
            # And the other way: a published figure the manifest does not name
            # belongs to no capture tool, so no <name>_visual_capture test
            # regenerates it and it goes stale unnoticed.
            unlisted = sorted(stem for stem in published_screenshots(root) if stem not in names)
            if unlisted:
                errors.append(
                    "docs/generated/screenshots holds figures the screenshot manifest does not name, "
                    "so no freshness test covers them: " + ", ".join(unlisted))

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("documentation coverage, links, and screenshot references are valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
