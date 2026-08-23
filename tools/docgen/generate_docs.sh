#!/usr/bin/env bash
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Regenerates everything under docs/generated/: the example-app
# screenshots (SVG, from the real virtual-terminal render — see
# frame_svg.hpp) and the HTML/PDF renders of the Markdown documentation
# pages, via CK Office Write's `ckwrite` CLI. Nothing under
# docs/generated/ is committed (see .gitignore) — this script is the
# single source of truth for producing it.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${CKVISION_BUILD_DIR:-$REPO_ROOT/build}"
DOCS_DIR="$REPO_ROOT/docs"
GEN_DIR="$DOCS_DIR/generated"
SCREENSHOTS_DIR="$GEN_DIR/screenshots"
PYTHON_BIN="${PYTHON_BIN:-python3}"

# ckwrite lives in the separate cworks monorepo (CK Office Write), not
# in this repo — point CKWRITE_BIN at its built binary if it's not at
# this default location.
CKWRITE_BIN="${CKWRITE_BIN:-$HOME/git/cworks_dir/cworks/build/bin/ckwrite}"

echo "==> Synchronizing source-backed documentation snippets"
"$PYTHON_BIN" "$REPO_ROOT/tools/docgen/extract_snippets.py" --root "$REPO_ROOT" --write
"$PYTHON_BIN" "$REPO_ROOT/tools/docgen/extract_snippets.py" --root "$REPO_ROOT"
"$PYTHON_BIN" "$REPO_ROOT/tools/docgen/check_docs.py" --root "$REPO_ROOT"

echo "==> Building ckVision (screenshot capture tools)"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD_DIR" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" \
    --target capture_widget_gallery_screenshots capture_gallery_screenshots capture_filebrowser_screenshots capture_hello_screenshots \
        capture_layouts_screenshots capture_forms_screenshots capture_workbench_screenshots \
        capture_graphics_screenshots capture_spin_screenshots capture_editor_screenshots capture_terminal_screenshots

echo "==> Capturing example-app screenshots"
mkdir -p "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_widget_gallery_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_gallery_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_filebrowser_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_hello_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_editor_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_terminal_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_layouts_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_forms_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_workbench_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_graphics_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_spin_screenshots" "$SCREENSHOTS_DIR"

# ckwrite's LaTeX/PDF path requires a PDF companion for every embedded
# SVG (no TeX engine reads SVG directly) — the HTML path uses the SVGs
# as-is. rsvg-convert is doc-tooling-only (never a cvision dependency,
# consistent with ckVision's own zero-dependency rule for the library
# itself); if it's absent, PDF rendering below degrades to a clear
# warning per file rather than a hard failure.
if command -v rsvg-convert >/dev/null 2>&1; then
    screenshot_names=(
        widget-navigation
        gallery-initial
        gallery-typed-name
        gallery-menu-open
        gallery-no-graphics
        filebrowser-initial
        filebrowser-src-selected
        filebrowser-include-selected
        hello-initial
        hello-greeting
        hello-menu-open
        editor-initial
        editor-search
        editor-close-confirm
        terminal-initial
        terminal-initial-dark
        terminal-initial-light
        terminal-initial-mono
        terminal-menu
        terminal-full-screen
        terminal-nested
        terminal-sixel
        terminal-no-graphics
        layouts-initial
        layouts-wide
        layouts-narrow
        layouts-too-small
        layouts-recovered
        forms-initial
        forms-invalid-dialog
        forms-info-message
        forms-wizard-ready
        workbench-text
        workbench-data
        workbench-help
        graphics-sixel-image
        graphics-sixel-canvas
        graphics-no-graphics-image
        graphics-no-graphics-canvas
        spin-initial
        spin-menu
        spin-desktop
        spin-no-graphics
    )
    for name in "${screenshot_names[@]}"; do
        svg="$SCREENSHOTS_DIR/$name.svg"
        echo "    converting $name.svg -> PDF"
        rsvg-convert --format=pdf -o "${svg%.svg}.pdf" "$svg"
    done
else
    echo "warning: rsvg-convert not found (brew install librsvg) — PDF export of pages" >&2
    echo "         with embedded screenshots will fail; HTML export is unaffected." >&2
fi

if [[ ! -x "$CKWRITE_BIN" ]]; then
    echo "warning: ckwrite not found at $CKWRITE_BIN" >&2
    echo "         set CKWRITE_BIN to its build/bin/ckwrite path to also render HTML/PDF." >&2
    echo "         screenshots were still generated under $SCREENSHOTS_DIR." >&2
    exit 0
fi

echo "==> Rendering docs/*.md -> HTML via ckwrite"
mkdir -p "$GEN_DIR/html"
for md in "$DOCS_DIR"/*.md; do
    name="$(basename "$md" .md)"
    "$CKWRITE_BIN" export "$md" --to html -o "$GEN_DIR/html/$name.html" --copy-assets
done

echo "==> Rendering docs/*.md -> LaTeX -> PDF via ckwrite + pdflatex"
mkdir -p "$GEN_DIR/pdf"
for md in "$DOCS_DIR"/*.md; do
    name="$(basename "$md" .md)"
    "$CKWRITE_BIN" export "$md" --to latex -o "$GEN_DIR/pdf/$name.tex" --copy-assets
    if command -v pdflatex >/dev/null 2>&1; then
        # One page's LaTeX content (e.g. a code block with raw box-
        # drawing glyphs pdflatex's default font can't set) must not
        # abort the whole run — pdflatex often still emits a usable
        # PDF alongside a nonzero exit in -interaction=nonstopmode, so
        # this warns and moves on rather than losing every OTHER page.
        if (cd "$GEN_DIR/pdf" && pdflatex -interaction=nonstopmode -output-directory="$GEN_DIR/pdf" "$name.tex" \
                >"$GEN_DIR/pdf/$name.pdflatex.log" 2>&1); then
            echo "    wrote $GEN_DIR/pdf/$name.pdf"
        else
            echo "    warning: pdflatex reported errors for $name.tex — see $GEN_DIR/pdf/$name.pdflatex.log" >&2
            echo "             ($GEN_DIR/pdf/$name.pdf may still exist and be usable)" >&2
        fi
    else
        echo "    warning: pdflatex not found; wrote LaTeX only ($GEN_DIR/pdf/$name.tex)" >&2
    fi
done

echo "==> Done. See $GEN_DIR/"
