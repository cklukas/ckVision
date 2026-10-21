#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Normalize CPack tar payloads without carrying the build host's macOS xattrs."""
import gzip
import sys
import tarfile
from pathlib import Path

package = Path(sys.argv[1])
normalized = package.with_name(package.name + ".normalized")
try:
    with tarfile.open(package) as source, normalized.open("wb") as raw:
        with gzip.GzipFile(fileobj=raw, mode="wb", filename="", mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as output:
                for member in source:
                    if Path(member.name).name.startswith("._"):
                        stream = source.extractfile(member)
                        if stream is None or stream.read(4) != b"\x00\x05\x16\x07":
                            raise ValueError("unexpected non-AppleDouble metadata member")
                        continue
                    member.uid = member.gid = 0
                    member.uname = member.gname = ""
                    member.pax_headers = {}
                    output.addfile(member, source.extractfile(member) if member.isfile() else None)
    normalized.replace(package)
finally:
    normalized.unlink(missing_ok=True)
