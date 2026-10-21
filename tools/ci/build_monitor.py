#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Record source identity, duration and exact monitor output for a CI build."""
from __future__ import annotations
import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--record", type=Path, required=True)
    parser.add_argument("--jobs", default="4")
    parser.add_argument("--configuration", default="Release")
    args = parser.parse_args()
    source = args.source.resolve()
    if (source / ".git").exists():
        names = subprocess.check_output(["git", "-C", str(source), "ls-files", "--cached", "--others", "--exclude-standard", "-z"]).decode().split("\0")
        paths = [source / name for name in names if name and not name.startswith(".claude/") and (source / name).is_file()]
    else:
        paths = [p for p in source.rglob("*") if p.is_file() and not set(p.relative_to(source).parts) & {".git", "dist", "public", "__pycache__"}]
    inputs = {str(p.relative_to(source)): digest(p) for p in sorted(paths)}
    start = time.time()
    result = subprocess.run(["cmake", "--build", str(args.build), "--config", args.configuration, "--parallel", args.jobs])
    end = time.time()
    changed = [name for name, value in inputs.items() if not (source / name).is_file() or digest(source / name) != value]
    output = args.executable.resolve()
    record = {"start": start, "end": end, "elapsed_seconds": end - start, "configuration": args.configuration,
              "output": str(output), "exit": result.returncode, "source_assets_sha256": inputs,
              "changed_inputs": changed, "baseline": "First run has no assumed duration baseline; retain this receipt for comparable runs."}
    if output.is_file():
        record.update(output_mtime=output.stat().st_mtime, output_sha256=digest(output))
    args.record.parent.mkdir(parents=True, exist_ok=True)
    args.record.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    if result.returncode or changed or not output.is_file():
        raise SystemExit("build or source-identity verification failed")
    # A clean CI workspace must produce the executable during this run.
    if output.stat().st_mtime < start - 1:
        raise SystemExit("monitor output predates the clean CI build")

if __name__ == "__main__":
    main()
