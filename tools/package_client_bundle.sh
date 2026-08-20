#!/usr/bin/env bash
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Build a self-contained local client handoff: the installable SDK, runnable
# examples, source documentation, and fresh visual documentation captures.
# The destination must not exist, so this command never overwrites a prior
# handoff artifact.
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <build-directory> <bundle-directory>" >&2
    exit 2
fi

BUILD_DIR="$1"
BUNDLE_DIR="$2"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "error: '$BUILD_DIR' is not a configured ckVision build directory" >&2
    exit 2
fi
if [[ -e "$BUNDLE_DIR" || -e "${BUNDLE_DIR}.tar.gz" ]]; then
    echo "error: bundle destination or archive already exists: '$BUNDLE_DIR'" >&2
    exit 2
fi

cmake --install "$BUILD_DIR" --prefix "$BUNDLE_DIR/sdk"
cmake --build "$BUILD_DIR" --target \
    capture_widget_gallery_screenshots capture_gallery_screenshots \
    capture_filebrowser_screenshots capture_hello_screenshots \
    capture_layouts_screenshots capture_forms_screenshots \
    capture_workbench_screenshots capture_graphics_screenshots \
    capture_editor_screenshots capture_terminal_screenshots

cmake -E copy_directory "$REPO_ROOT/docs" "$BUNDLE_DIR/docs/source"
SCREENSHOTS_DIR="$BUNDLE_DIR/docs/generated/screenshots"
mkdir -p "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_widget_gallery_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_gallery_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_filebrowser_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_hello_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_layouts_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_forms_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_workbench_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_graphics_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_editor_screenshots" "$SCREENSHOTS_DIR"
"$BUILD_DIR/tools/docgen/capture_terminal_screenshots" "$SCREENSHOTS_DIR"

PARENT_DIR="$(dirname "$BUNDLE_DIR")"
BUNDLE_NAME="$(basename "$BUNDLE_DIR")"
(cd "$PARENT_DIR" && cmake -E tar cfz "${BUNDLE_NAME}.tar.gz" --format=gnutar "$BUNDLE_NAME")
echo "wrote $BUNDLE_DIR and ${BUNDLE_DIR}.tar.gz"
