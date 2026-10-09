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
if [[ ${#capture_tools[@]} -eq 0 ]]; then
    echo "error: screenshot manifest names no capture tools" >&2
    exit 2
fi

cmake --install "$BUILD_DIR" --prefix "$BUNDLE_DIR/sdk"
cmake --build "$BUILD_DIR" --target "${capture_tools[@]}"

cmake -E copy_directory "$REPO_ROOT/docs" "$BUNDLE_DIR/docs/source"
SCREENSHOTS_DIR="$BUNDLE_DIR/docs/generated/screenshots"
# Remove the published copies copied with the source documentation: every
# bundled SVG must now be produced by a capture tool from this build.
rm -rf "$SCREENSHOTS_DIR"
mkdir -p "$SCREENSHOTS_DIR"
for tool in "${capture_tools[@]}"; do
    "$BUILD_DIR/tools/docgen/$tool" "$SCREENSHOTS_DIR"
done
for name in "${screenshot_names[@]}"; do
    if [[ ! -s "$SCREENSHOTS_DIR/$name.svg" ]]; then
        echo "error: capture tools did not produce $name.svg" >&2
        exit 1
    fi
done

PARENT_DIR="$(dirname "$BUNDLE_DIR")"
BUNDLE_NAME="$(basename "$BUNDLE_DIR")"
(cd "$PARENT_DIR" && cmake -E tar cfz "${BUNDLE_NAME}.tar.gz" --format=gnutar "$BUNDLE_NAME")
echo "wrote $BUNDLE_DIR and ${BUNDLE_DIR}.tar.gz"
