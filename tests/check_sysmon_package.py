#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Exercise the actual CPack archive and checksum rejection path."""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path, required=True)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--scratch", type=Path, required=True)
parser.add_argument("--version", required=True)
parser.add_argument("--cpack", required=True)
args = parser.parse_args()
shutil.rmtree(args.scratch, ignore_errors=True)
args.scratch.mkdir(parents=True)
subprocess.run([args.cpack, "-G", "TGZ", "--config", str(args.build / "CPackConfig.cmake"),
                "-B", str(args.scratch / "packages")], check=True)
packages = list((args.scratch / "packages").glob("*.tar.gz"))
assert len(packages) == 1
command = [sys.executable, str(args.source / "packaging/check_package.py"), "--package", str(packages[0]),
           "--version", args.version, "--scratch", str(args.scratch / "verified")]
subprocess.run(command, check=True)
sidecar = packages[0].with_name(packages[0].name + ".sha256")
sidecar.write_text("0" * 64 + "  " + packages[0].name + "\n")
command[-1] = str(args.scratch / "rejected")
result = subprocess.run(command, capture_output=True, text=True)
assert result.returncode != 0 and "checksum mismatch" in result.stderr
assert not (args.scratch / "rejected").exists()
print("PASS native package payload, collector and corrupt-checksum rejection")
