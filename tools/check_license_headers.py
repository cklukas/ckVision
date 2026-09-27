#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Require the copyright and licence lines at the top of every source, build and tooling file.

the engineering standard: every source, build, and tooling file begins with the copyright line
`Copyright (c) 2026 C. Klukas. All rights reserved.` in the file's comment syntax,
followed by `SPDX-License-Identifier: MIT` on the next line. A `#!` line may come
first. Data files (fuzz corpora, goldens, generated screenshots) are not source and
are not checked.

  check_license_headers.py --root <repo>   check every tracked and untracked,
                                           non-ignored file of a checked kind
  check_license_headers.py --self-test     prove the rule accepts and rejects
                                           what it should
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys

COPYRIGHT = "Copyright (c) 2026 C. Klukas. All rights reserved."
SPDX = "SPDX-License-Identifier: MIT"

# Comment leader per kind of file.
SLASH = "//"
HASH = "#"
SUFFIXES = {
    ".cpp": SLASH, ".hpp": SLASH, ".h": SLASH, ".c": SLASH, ".inc": SLASH,
    ".py": HASH, ".cmake": HASH, ".sh": HASH, ".ps1": HASH, ".yml": HASH, ".yaml": HASH,
}
NAMES = {
    "CMakeLists.txt": HASH, ".gitignore": HASH, ".gitattributes": HASH, ".clang-format": HASH,
}
# Paths that hold data rather than source, whatever their suffix.
DATA_PREFIXES = ("fuzz/corpus/", "tests/golden/", "tests/fixtures/", "docs/generated/")


def leader_for(path: str) -> str | None:
    if path.startswith(DATA_PREFIXES):
        return None
    name = path.rsplit("/", 1)[-1]
    if name in NAMES:
        return NAMES[name]
    suffix = "." + name.rsplit(".", 1)[-1] if "." in name else ""
    return SUFFIXES.get(suffix)


def header_problem(text: str, leader: str) -> str | None:
    """Why `text` lacks the required header, or None when it has it."""
    lines = text.splitlines()
    if lines and lines[0].startswith("#!"):
        lines = lines[1:]
    if not lines or lines[0].strip() != f"{leader} {COPYRIGHT}":
        return "first line is not the copyright line"
    if len(lines) < 2 or lines[1].strip() != f"{leader} {SPDX}":
        return "the SPDX licence line does not follow the copyright line"
    return None


# Directories a source tree without git metadata (an extracted archive) may
# hold that are build output or tool caches rather than source.
UNTRACKED_DIRECTORIES = {".git", ".cache", "__pycache__", "Testing", ".claude"}


def repository_files(root: pathlib.Path) -> list[str]:
    """The source tree's files: what git tracks or would track, or, in an
    extracted archive with no git metadata, every file outside build output."""
    if (root / ".git").exists():
        output = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=root, check=True,
                                capture_output=True, text=True).stdout
        return sorted(line for line in output.splitlines() if line)
    files = []
    for path in root.rglob("*"):
        parts = path.relative_to(root).parts
        if any(part in UNTRACKED_DIRECTORIES or part.startswith("build") for part in parts[:-1]):
            continue
        if path.is_file():
            files.append(path.relative_to(root).as_posix())
    return sorted(files)


def check(root: pathlib.Path) -> int:
    checked = 0
    problems = 0
    for path in repository_files(root):
        leader = leader_for(path)
        full = root / path
        if leader is None or not full.is_file():
            continue
        checked += 1
        problem = header_problem(full.read_text(encoding="utf-8", errors="replace"), leader)
        if problem:
            problems += 1
            print(f"{path}: {problem}")
    print(f"license-headers: {checked} files checked, {problems} without the header")
    return 1 if problems else 0


SELF_TEST_CASES: list[tuple[str, str, bool]] = [
    ("src/a.cpp", f"// {COPYRIGHT}\n// {SPDX}\n#include <x>\n", True),
    ("tools/a.py", f"#!/usr/bin/env python3\n# {COPYRIGHT}\n# {SPDX}\n", True),
    ("CMakeLists.txt", f"# {COPYRIGHT}\n# {SPDX}\n", True),
    ("src/b.cpp", f"// {COPYRIGHT}\n#include <x>\n", False),
    ("src/c.cpp", f"// {SPDX}\n// {COPYRIGHT}\n", False),
    ("tools/b.py", "print('no header')\n", False),
    ("src/d.hpp", f"# {COPYRIGHT}\n# {SPDX}\n", False),
]
SELF_TEST_SKIPPED = ["fuzz/corpus/syntax_profile/bash.sh", "tests/golden/a.dump", "docs/index.md"]


def self_test() -> int:
    failures = 0
    for path, text, accepted in SELF_TEST_CASES:
        leader = leader_for(path)
        ok = leader is not None and header_problem(text, leader) is None
        if ok != accepted:
            failures += 1
            print(f"FAIL {path}: {'accepted' if ok else 'rejected'}, expected {'accepted' if accepted else 'rejected'}")
    for path in SELF_TEST_SKIPPED:
        if leader_for(path) is not None:
            failures += 1
            print(f"FAIL {path}: checked, expected to be skipped as data")
    total = len(SELF_TEST_CASES) + len(SELF_TEST_SKIPPED)
    print(f"{total - failures}/{total} self-test cases pass")
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=pathlib.Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if args.root is None:
        parser.error("--root is required unless --self-test is given")
    return check(args.root.resolve())


if __name__ == "__main__":
    sys.exit(main())
