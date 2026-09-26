#!/usr/bin/env bash
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Regenerates everything under docs/generated/: the example-app
# screenshots and the per-widget gallery figures (SVG, from the real
# virtual-terminal render — see frame_svg.hpp) and the HTML/PDF renders
# of the Markdown documentation pages, via CK Office Write's `ckwrite`
# CLI. The SVGs are tracked and published, and regenerated in place; the
# HTML, PDF and TeX under generated/ are not (see .gitignore). Either
# way this script is the single source of truth for producing them.
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
"$PYTHON_BIN" "$REPO_ROOT/tools/docgen/sync_field_tables.py" --root "$REPO_ROOT" --write
"$PYTHON_BIN" "$REPO_ROOT/tools/docgen/sync_field_tables.py" --root "$REPO_ROOT"
"$PYTHON_BIN" "$REPO_ROOT/tools/docgen/check_docs.py" --root "$REPO_ROOT"

# The capture tools and the figures each draws are the screenshot manifest's
# groups, the same reading the <name>_visual_capture tests are registered
# from, so a tool added there is built, run and gated without a second list.
capture_tools=()
screenshot_names=()
while IFS= read -r line; do
    [[ -z "$line" || "$line" == \#* ]] && continue
    if [[ "$line" =~ ^\[(capture_[a-z_]+)\]$ ]]; then
        capture_tools+=("${BASH_REMATCH[1]}")
    else
        screenshot_names+=("$line")
    fi
done < "$REPO_ROOT/tools/docgen/screenshot-manifest.txt"

echo "==> Building ckVision (screenshot capture tools)"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD_DIR" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" \
    --target "${capture_tools[@]}"

echo "==> Capturing example-app screenshots"
mkdir -p "$SCREENSHOTS_DIR"
for tool in "${capture_tools[@]}"; do
    "$BUILD_DIR/tools/docgen/$tool" "$SCREENSHOTS_DIR"
done

# ckwrite's LaTeX/PDF path requires a PDF companion for every embedded
# SVG (no TeX engine reads SVG directly) — the HTML path uses the SVGs
# as-is. rsvg-convert is doc-tooling-only (never a cvision dependency,
# consistent with ckVision's own zero-dependency rule for the library
# itself); if it's absent, PDF rendering below degrades to a clear
# warning per file rather than a hard failure.
if command -v rsvg-convert >/dev/null 2>&1; then
    # The names come from the manifest read above rather than a second copy
    # of them here: the two lists drifted apart the moment the gallery gained
    # fifty figures, and check_docs.py only polices the manifest.
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
