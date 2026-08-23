# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Report CTest category counts for local and CI verification evidence."""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


CATEGORIES = (
    "unit",
    "script",
    "golden",
    "raw-byte",
    "symbolic-scene",
    "visual",
    "fuzz-corpus",
    "benchmark",
    "platform",
    "pty",
)


def labels_for(test):
    for property_ in test.get("properties", []):
        if property_.get("name") == "LABELS":
            value = property_.get("value", [])
            return set(value if isinstance(value, list) else [value])
    return set()


def inventory(test_dir):
    completed = subprocess.run(
        ["ctest", "--test-dir", str(test_dir), "--show-only=json-v1"],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        sys.stderr.write(completed.stderr)
        raise RuntimeError("CTest could not enumerate the configured tests")
    tests = json.loads(completed.stdout).get("tests", [])
    return len(tests), {category: sum(category in labels_for(test) for test in tests) for category in CATEGORIES}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test-dir", required=True, type=Path)
    parser.add_argument("--sanitizer", metavar="NAME")
    args = parser.parse_args()

    total, counts = inventory(args.test_dir)
    lines = ["ckVision CTest inventory", f"total: {total}"]
    lines.extend(f"{category}: {counts[category]}" for category in CATEGORIES)
    sanitizer = args.sanitizer or os.environ.get("CKVISION_SANITIZER")
    if sanitizer:
        lines.append(f"sanitizer: {sanitizer}")
    report = "\n".join(lines)
    print(report)

    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with Path(summary).open("a", encoding="utf-8") as handle:
            handle.write("## ckVision test inventory\n\n```text\n")
            handle.write(report)
            handle.write("\n```\n")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, json.JSONDecodeError) as error:
        print(f"test inventory failed: {error}", file=sys.stderr)
        sys.exit(1)
