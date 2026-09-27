#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Enforce ckVision include discipline (the architecture section 1).

Four checks over include/cvision and src:
1. Layer direction: '#include [<"]cvision/<layer>/...' must obey the
   allowed downward edges.
2. Include form: every quoted include in library code must use the full
   'cvision/<layer>/...' spelling — no relative includes, no '..'.
3. Include cycles: the project-include graph must be acyclic, including
   within a single layer.
4. Platform purity: core, scene, ui and widgets include nothing but the C++
   standard library, and not the parts of it that reach the environment,
   the clock or the locale (the engineering standard determinism rule, D-039). Platform
   headers belong to term, the one layer that meets the operating system,
   and to the standalone test harness, which starts child processes.

Exits non-zero on any violation.
"""

import argparse
import re
import sys
import tempfile
from pathlib import Path

ALLOWED = {
    "core": {"core"},
    "scene": {"scene", "core"},
    "term": {"term", "core"},
    "ui": {"ui", "core", "scene", "term"},
    "widgets": {"widgets", "ui", "scene", "core"},
    # The installed test harness (include/cvision/testing) is standalone by
    # construction: it depends on the standard library only, never on the
    # framework it is used to test. It therefore sits beside every other
    # layer and may include nothing from them — and, equally, nothing may
    # include it, so no framework code can ever acquire a test-only
    # dependency.
    "testing": {"testing"},
}

# The C++20 standard library headers, including the C compatibility headers.
STANDARD_HEADERS = {
    "algorithm", "any", "array", "atomic", "barrier", "bit", "bitset", "cassert", "cctype",
    "cerrno", "cfenv", "cfloat", "charconv", "chrono", "cinttypes", "climits", "clocale",
    "cmath", "codecvt", "compare", "complex", "concepts", "condition_variable", "coroutine",
    "csetjmp", "csignal", "cstdarg", "cstddef", "cstdint", "cstdio", "cstdlib", "cstring",
    "ctime", "cuchar", "cwchar", "cwctype", "deque", "exception", "execution", "filesystem",
    "format", "forward_list", "fstream", "functional", "future", "initializer_list",
    "iomanip", "ios", "iosfwd", "iostream", "istream", "iterator", "latch", "limits", "list",
    "locale", "map", "memory", "memory_resource", "mutex", "new", "numbers", "numeric",
    "optional", "ostream", "queue", "random", "ranges", "ratio", "regex", "scoped_allocator",
    "semaphore", "set", "shared_mutex", "source_location", "span", "sstream", "stack",
    "stdexcept", "stop_token", "streambuf", "string", "string_view", "syncstream",
    "system_error", "thread", "tuple", "type_traits", "typeindex", "typeinfo",
    "unordered_map", "unordered_set", "utility", "valarray", "variant", "vector", "version",
}
# Standard headers whose purpose is to read the clock, the locale, files, streams,
# signals, or a non-deterministic source; a pure layer gets these through injected
# interfaces (Clock, FileSystem) instead. <cctype> and <cwctype> classify and fold case
# by the process locale; cvision/core/ascii.hpp is the locale-free replacement.
IMPURE_STANDARD_HEADERS = {
    "cctype", "chrono", "clocale", "codecvt", "csignal", "ctime", "cwctype", "filesystem",
    "fstream", "iomanip",
    "iostream", "istream", "locale", "ostream", "random", "sstream", "streambuf",
    "syncstream",
}
PURE_LAYERS = {"core", "scene", "ui", "widgets"}

ANGLE_RE = re.compile(r'#\s*include\s+<([^>]+)>')
CVISION_RE = re.compile(r'#\s*include\s+[<"](cvision/([a-z]+)/[^">]+)[">]')
QUOTED_RE = re.compile(r'#\s*include\s+"([^"]+)"')


def layer_of(relative: Path):
    parts = relative.parts
    for anchor in ("cvision", "src"):
        if anchor in parts:
            i = parts.index(anchor)
            if i + 1 < len(parts) and parts[i + 1] in ALLOWED:
                return parts[i + 1]
    return None


def check_root(root: Path) -> int:
    include_root = root / "include"
    violations = 0
    graph = {}  # node id -> list of (lineno, target node id)

    files = []
    for base in (include_root / "cvision", root / "src"):
        if base.exists():
            files.extend(
                p for p in sorted(base.rglob("*")) if p.suffix in {".hpp", ".h", ".cpp"})

    for path in files:
        relative = path.relative_to(root)
        layer = layer_of(relative)
        if layer is None:
            print(f"layer-check: cannot determine layer of {relative}", file=sys.stderr)
            violations += 1
            continue
        node = str(relative)
        graph.setdefault(node, [])
        for lineno, line in enumerate(
                path.read_text(encoding="utf-8").splitlines(), start=1):
            quoted = QUOTED_RE.search(line)
            cvision = CVISION_RE.search(line)
            angle = ANGLE_RE.search(line)
            if layer in PURE_LAYERS and angle and not angle.group(1).startswith("cvision/"):
                header = angle.group(1)
                if header not in STANDARD_HEADERS:
                    print(f"{relative}:{lineno}: layer '{layer}' may include only the C++ "
                          f"standard library (got <{header}>)", file=sys.stderr)
                    violations += 1
                elif header in IMPURE_STANDARD_HEADERS:
                    print(f"{relative}:{lineno}: layer '{layer}' may not include <{header}>: "
                          f"clock, locale, file and stream access enter through injected "
                          f"interfaces", file=sys.stderr)
                    violations += 1
            if quoted and not quoted.group(1).startswith("cvision/"):
                print(
                    f"{relative}:{lineno}: quoted includes must use the full "
                    f"'cvision/<layer>/...' form (got \"{quoted.group(1)}\")",
                    file=sys.stderr)
                violations += 1
                continue
            if not cvision:
                continue
            target_path, target_layer = cvision.group(1), cvision.group(2)
            if target_layer not in ALLOWED[layer]:
                print(
                    f"{relative}:{lineno}: layer '{layer}' may not include "
                    f"cvision/{target_layer}/",
                    file=sys.stderr)
                violations += 1
            target_file = include_root / target_path
            if not target_file.exists():
                print(f"{relative}:{lineno}: include target does not exist: "
                      f"{target_path}",
                      file=sys.stderr)
                violations += 1
                continue
            graph[node].append((lineno, str(target_file.relative_to(root))))

    # Cycle detection: iterative DFS with white/gray/black coloring.
    WHITE, GRAY, BLACK = 0, 1, 2
    color = {node: WHITE for node in graph}
    for start in sorted(graph):
        if color[start] != WHITE:
            continue
        stack = [(start, iter(graph[start]))]
        path_stack = [start]
        color[start] = GRAY
        while stack:
            node, edges = stack[-1]
            advanced = False
            for _lineno, target in edges:
                if target not in graph:
                    continue
                if color[target] == GRAY:
                    cycle = path_stack[path_stack.index(target):] + [target]
                    print("layer-check: include cycle: " + " -> ".join(cycle),
                          file=sys.stderr)
                    violations += 1
                elif color[target] == WHITE:
                    color[target] = GRAY
                    stack.append((target, iter(graph[target])))
                    path_stack.append(target)
                    advanced = True
                    break
            if not advanced:
                color[node] = BLACK
                stack.pop()
                path_stack.pop()

    if violations:
        return violations
    print(f"layer-check: OK ({len(graph)} files)")
    return 0


def self_test() -> int:
    # Prove every forbidden edge is rejected, rather than merely relying on
    # the current repository not to contain one.  The fixtures are generated
    # outside the worktree so the checker never needs an exemption for its
    # own intentionally-invalid sources.
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        source_root = root / "include" / "cvision"
        expected_violations = 0
        for source, allowed_targets in ALLOWED.items():
            source_dir = source_root / source
            source_dir.mkdir(parents=True, exist_ok=True)
            for target in ALLOWED:
                target_file = source_root / target / "target.hpp"
                target_file.parent.mkdir(parents=True, exist_ok=True)
                target_file.write_text("// fixture\n", encoding="utf-8")
                if target not in allowed_targets:
                    fixture = source_dir / f"forbidden_{target}.hpp"
                    fixture.write_text(
                        f'#include "cvision/{target}/target.hpp"\n', encoding="utf-8")
                    expected_violations += 1

        # Platform purity: a platform header, and an impure standard header, in
        # each pure layer; a pure standard header and a platform header in term
        # are accepted.
        for layer in sorted(PURE_LAYERS):
            directory = source_root / layer
            directory.mkdir(parents=True, exist_ok=True)
            (directory / "platform.hpp").write_text("#include <unistd.h>\n", encoding="utf-8")
            (directory / "impure.hpp").write_text("#include <chrono>\n", encoding="utf-8")
            (directory / "pure.hpp").write_text("#include <vector>\n", encoding="utf-8")
            expected_violations += 2
        (source_root / "term" / "platform.hpp").write_text(
            "#include <windows.h>\n#include <chrono>\n", encoding="utf-8")

        actual_violations = check_root(root)
        if actual_violations != expected_violations:
            print(
                "layer-check self-test: expected "
                f"{expected_violations} failures, got {actual_violations}",
                file=sys.stderr)
            return 1

    print("layer-check self-test: OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true",
                        help="verify every forbidden layer edge is rejected")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    return 1 if check_root(Path(__file__).resolve().parent.parent) else 0


if __name__ == "__main__":
    sys.exit(main())
