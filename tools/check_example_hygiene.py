#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""WP-35 public-example hygiene gate.

The examples are the client-facing construction surface. They must not retain
the old plumbing/cast/duplicate-command patterns that the library may still use
internally for type-erased implementation seams.

Nor may they spell a key chord out by hand. A chord is the keymap's to state:
a string literal saying "press F9" goes on saying it after the command has
been rebound, so the only literal allowed to name a chord is a command's own
`.chord =` declaration. Every other hint asks the registry
(CommandRegistry::chord_text) for the chord bound right now.

  check_example_hygiene.py              check every source under examples/
  check_example_hygiene.py --self-test  prove each rule rejects what it should
                                        and accepts what it should
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent

PATTERNS = [
    ("widget casts", re.compile(r"static_cast\s*<\s*widgets::")),
    ("Desktop constructor theme plumbing", re.compile(r"Desktop\s*\([^;\n]*theme\s*\(")),
    ("Desktop make_unique theme plumbing", re.compile(r"make_unique\s*<[^>]*Desktop[^;]*theme\s*\(")),
    ("bypassed standard-dialog attachment", re.compile(r"root\s*\(\)\.add_child\s*\([^;]*handle\.window")),
    ("redundant focus-policy defaults", re.compile(r"set_focus_policy\s*\(")),
]

# The field a command's identity is actually declared in. It was `.id` when
# this gate was written and is `.key` now, and the old spelling matched every
# `.id =` in an example -- including an ordinary struct field, which is how
# this rule came to reject correct code while no longer covering a single
# command in the tree. Matching `.key` restores what it was for: two examples
# claiming the same command identity.
COMMAND_KEY = re.compile(r"\.key\s*=\s*([^,\n}]+)")
COMMAND_CHORD = re.compile(r"\.chord\s*=\s*\"([^\"]+)\"")

# What reads as a key chord inside prose: a function key F1..F12 standing as
# a word of its own, or a Ctrl/Alt/Shift combination. "F16", "0xF1" and
# "Shift" on its own are not chords.
CHORD_TEXT = re.compile(r"(?<![\w+])(?:F(?:1[0-2]|[1-9])(?!\w)|(?:Ctrl|Alt|Shift)\+(?=\S))")
# The one place a literal may carry a chord: the declaration that binds it.
CHORD_DECLARATION = re.compile(r"\.chord\s*=\s*$")

# C++ lexical elements that can hide or contain a string literal. Comments are
# matched so that their text is never mistaken for a literal, character
# literals so that '"' is never mistaken for the start of one.
_LEXEME = re.compile(r'''
    //[^\n]*                                                       # line comment
  | /\*.*?\*/                                                      # block comment
  | (?:u8|[uUL])?R"(?P<delim>[^(\s]*)\((?P<raw>.*?)\)(?P=delim)"   # raw string
  | (?:u8|[uUL])?"(?P<text>(?:[^"\\\n]|\\.)*)"                     # ordinary string
  | (?:u8|[uUL])?'(?:[^'\\\n]|\\.)+'                               # character literal
''', re.VERBOSE | re.DOTALL)


def string_literals(text: str):
    """Yield (offset, contents) for every string literal outside comments."""
    for match in _LEXEME.finditer(text):
        if match.group("raw") is not None:
            yield match.start(), match.group("raw")
        elif match.group("text") is not None:
            yield match.start(), match.group("text")


def check_sources(sources: list[tuple[str, str]]) -> list[str]:
    """Every hygiene failure in `sources`, a list of (display path, text)."""
    failures: list[str] = []
    ids: dict[str, str] = {}
    chords: dict[str, str] = {}
    for rel, text in sources:
        for label, pattern in PATTERNS:
            for match in pattern.finditer(text):
                line = text.count("\n", 0, match.start()) + 1
                failures.append(f"{rel}:{line}: forbidden {label}")
        for match in COMMAND_KEY.finditer(text):
            command_key = match.group(1).strip()
            previous = ids.setdefault(command_key, rel)
            if previous != rel:
                failures.append(f"{rel}: duplicate command key {command_key} also declared in {previous}")
        for match in COMMAND_CHORD.finditer(text):
            chord = match.group(1)
            previous = chords.setdefault(chord, rel)
            if previous != rel:
                failures.append(f"{rel}: duplicate command chord {chord} also declared in {previous}")
        for offset, contents in string_literals(text):
            found = CHORD_TEXT.search(contents)
            if found is None or CHORD_DECLARATION.search(text, 0, offset):
                continue
            line = text.count("\n", 0, offset) + 1
            failures.append(f"{rel}:{line}: forbidden hand-encoded chord text \"{found.group(0)}\" in a "
                            "string literal; show CommandRegistry::chord_text for the command instead")
    return failures


def check_root(root: Path) -> list[str]:
    examples = root / "examples"
    paths = sorted(examples.glob("*.hpp")) + sorted(examples.glob("*/*.cpp")) + sorted(examples.glob("*/*.hpp"))
    return check_sources([(path.relative_to(root).as_posix(), path.read_text(encoding="utf-8"))
                          for path in paths])


def report(failures: list[str]) -> int:
    if failures:
        print("example-hygiene: FAIL", file=sys.stderr)
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1
    print("example-hygiene: OK")
    return 0


# Each rejected fixture must produce exactly one failure naming its rule; the
# accepted fixtures must produce none. They are written outside the worktree
# so the gate never needs an exemption for its own intentionally bad sources.
REJECTED = [
    ("widget casts", 'auto* b = static_cast<widgets::Button*>(view);\n'),
    ("Desktop constructor theme plumbing", 'auto desktop = widgets::Desktop(bounds, app.theme());\n'),
    ("Desktop make_unique theme plumbing", 'auto d = std::make_unique<widgets::Desktop>(bounds, app.theme());\n'),
    ("bypassed standard-dialog attachment", 'app.root().add_child(std::move(handle.window));\n'),
    ("redundant focus-policy defaults", 'view->set_focus_policy(ui::FocusPolicy::Tab);\n'),
    ("hand-encoded chord text", 'label->set_text("Press F9 to run");\n'),
    ("hand-encoded chord text", 'label->set_text("Measure (F12)");\n'),
    ("hand-encoded chord text", 'items.push_back(StatusLineItem{"Ctrl+Alt+Space: parent commands"});\n'),
    ("hand-encoded chord text", 'const char* hint = "Shift+Tab goes back";\n'),
    ("hand-encoded chord text", 'const auto chord = KeyChord::parse("Alt+G");\n'),
    ("hand-encoded chord text", 'const char* raw = R"(press Alt+X)";\n'),
    ("hand-encoded chord text", 'const char* wide = u8"F1 opens help";\n'),
    ("hand-encoded chord text", 'const std::string text = "Choose " "Save " "(Ctrl+S)";\n'),
]

ACCEPTED = [
    'app.commands().declare({.key = "demo.run", .title = "&Run", .chord = "F9"});\n',
    'app.commands().declare({.key = "demo.quit", .title = "&Quit", .chord="Alt+X"});\n',
    '// Press F9, or Ctrl+R, to run: comments may name chords.\n',
    '/* Shift+Tab walks back; "F1" is help. */\n',
    'const std::string hint = app.commands().chord_text(run) + " runs";\n',
    'const char* plain = "An F16 jet, 0xF1, Shift and Alt on their own, a CtrlX, and F0.";\n',
    'const char quote = \'"\'; const char* after = "no chord here";\n',
    'bind(Key::F2, {}, add_task_command_);\n',
]


def self_test() -> int:
    problems: list[str] = []
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        expected: list[tuple[str, str]] = []
        for index, (rule, source) in enumerate(REJECTED):
            path = root / "examples" / "rejected" / f"case_{index}.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(source, encoding="utf-8")
            expected.append((path.relative_to(root).as_posix(), rule))
        for index, source in enumerate(ACCEPTED):
            path = root / "examples" / "accepted" / f"case_{index}.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(source, encoding="utf-8")
        # One command identity and one chord claimed by two different files.
        for name in ("first", "second"):
            path = root / "examples" / "duplicate" / f"{name}.cpp"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('app.commands().declare({.key = "shared.quit", .chord = "Alt+Q"});\n',
                            encoding="utf-8")

        failures = check_root(root)
        for rel, rule in expected:
            matching = [failure for failure in failures if failure.startswith(rel + ":")]
            if len(matching) != 1 or rule not in matching[0]:
                problems.append(f"{rel}: expected one '{rule}' failure, got {matching}")
        problems.extend(f"accepted fixture rejected: {failure}"
                        for failure in failures if failure.startswith("examples/accepted/"))
        duplicates = [failure for failure in failures if failure.startswith("examples/duplicate/")]
        if not any("duplicate command key" in failure for failure in duplicates) or \
                not any("duplicate command chord" in failure for failure in duplicates):
            problems.append(f"duplicate key and chord across files not both rejected: {duplicates}")
        if len(failures) != len(REJECTED) + 2:
            problems.append(f"expected {len(REJECTED) + 2} failures in total, got {len(failures)}: {failures}")

    if problems:
        print("example-hygiene self-test: FAIL", file=sys.stderr)
        for problem in problems:
            print(problem, file=sys.stderr)
        return 1
    print(f"example-hygiene self-test: OK ({len(REJECTED)} rejected, {len(ACCEPTED)} accepted fixtures)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--self-test", action="store_true",
                        help="verify every rule rejects its fixtures and accepts clean code")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    return report(check_root(ROOT))


if __name__ == "__main__":
    sys.exit(main())
