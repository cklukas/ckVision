# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Enforce the client-documentation coverage, link, and screenshot contract."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


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


def markdown_links(text: str) -> list[str]:
    prose_only = re.sub(r'```.*?```', '', text, flags=re.DOTALL)
    return re.findall(r'!?\[[^\]]*\]\(([^)]+)\)', prose_only)


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
            for header in public_widget_headers(root):
                if header not in coverage_text:
                    errors.append(f"coverage.md omits public widget header {header}")
                if header not in api_text:
                    errors.append(f"api-index.md omits public widget header {header}")
            widget_names = re.findall(r'^(?:class|struct) (\w+)', "\n".join(
                (root / header).read_text(encoding="utf-8") for header in public_widget_headers(root)), re.MULTILINE)
            for name in widget_names:
                if f"## {name}" not in gallery_text and f"### {name}" not in gallery_text:
                    errors.append(f"widget-gallery.md omits section for public type {name}")
            if "<!-- ckvision-snippet " not in gallery_text:
                errors.append("widget-gallery.md has no source-backed code snippet")
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

    if arguments.screenshots:
        manifest = root / "tools/docgen/screenshot-manifest.txt"
        if not manifest.is_file():
            errors.append("missing tools/docgen/screenshot-manifest.txt")
        else:
            names = {line.strip() for line in manifest.read_text(encoding="utf-8").splitlines()
                     if line.strip() and not line.startswith("#")}
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

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("documentation coverage, links, and screenshot references are valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
