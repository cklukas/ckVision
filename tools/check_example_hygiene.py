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

And every library presentation call names its modality (the decision log D-087):
`present_modal_*`, `present_modeless_*` or `exec_modal_*`, never a bare
`present_<family>` whose modality only the reference states.

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


# D-087: a presentation call states its modality in its name. These are the
# only spellings that do; `exec_` is always modal, because it blocks.
MODALITY_STATED = re.compile(r"(?:present_modal|present_modeless|exec_modal)(?:_\w+)?")
# A call that is the library's by the way it is reached: through the widgets
# namespace, or on a Desktop (`desktop.`, `desktop_->`, `stage.desktop().`).
QUALIFIED_PRESENTATION = re.compile(
    r"(?:\bwidgets::|\b\w*desktop\w*\s*(?:\(\s*\))?\s*(?:\.|->)\s*)(?P<name>(?:present|exec)_\w+)\s*\(")
# An unqualified call. It is the library's when what follows `present_` or
# `exec_` is one of the library's presentation families; an example's own
# helper (`present_lane_insert`) names no library family and is left alone.
UNQUALIFIED_PRESENTATION = re.compile(r"(?<![\w:.>])(?P<name>(?:present|exec)_(?P<rest>\w+))\s*\(")
# How the library declares a presentation function, which is where the family
# set comes from: `present_modal_message_box(` declares the family
# `message_box`, so an unqualified `present_message_box(` reaches for it.
LIBRARY_PRESENTATION = re.compile(r"\b(?:present_modal|present_modeless|exec_modal)_(?P<family>\w+)\s*\(")


def presentation_families(root: Path) -> set[str]:
    """Every presentation family a library header under include/cvision declares."""
    families: set[str] = set()
    for header in sorted((root / "include" / "cvision").rglob("*.hpp")):
        families.update(match.group("family")
                        for match in LIBRARY_PRESENTATION.finditer(header.read_text(encoding="utf-8")))
    return families


def string_literals(text: str):
    """Yield (offset, contents) for every string literal outside comments."""
    for match in _LEXEME.finditer(text):
        if match.group("raw") is not None:
            yield match.start(), match.group("raw")
        elif match.group("text") is not None:
            yield match.start(), match.group("text")


def code_only(text: str) -> str:
    """`text` with every comment and literal blanked, keeping offsets and lines."""
    return _LEXEME.sub(lambda match: re.sub(r"[^\n]", " ", match.group(0)), text)


def unstated_modality(text: str, families: set[str]):
    """Yield (offset, name) for each library presentation call that does not
    name its modality."""
    code = code_only(text)
    for match in QUALIFIED_PRESENTATION.finditer(code):
        if MODALITY_STATED.fullmatch(match.group("name")) is None:
            yield match.start("name"), match.group("name")
    for match in UNQUALIFIED_PRESENTATION.finditer(code):
        if match.group("rest") in families:
            yield match.start("name"), match.group("name")


def check_sources(sources: list[tuple[str, str]], families: set[str]) -> list[str]:
    """Every hygiene failure in `sources`, a list of (display path, text);
    `families` are the library's presentation families."""
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
        for offset, name in unstated_modality(text, families):
            line = text.count("\n", 0, offset) + 1
            failures.append(f"{rel}:{line}: forbidden unstated presentation modality in {name}(); call "
                            "present_modal_*, present_modeless_* or exec_modal_* (D-087)")
    return failures


def check_root(root: Path) -> list[str]:
    families = presentation_families(root)
    if not families:
        # Without the family set the unqualified half of the modality rule
        # would pass everything; a gate that cannot see the library fails.
        return [f"{root / 'include' / 'cvision'}: no presentation functions declared; "
                "the modality rule has nothing to check against"]
    examples = root / "examples"
    paths = sorted(examples.glob("*.hpp")) + sorted(examples.glob("*/*.cpp")) + sorted(examples.glob("*/*.hpp"))
    return check_sources([(path.relative_to(root).as_posix(), path.read_text(encoding="utf-8"))
                          for path in paths], families)


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
    ("unstated presentation modality", 'auto box = widgets::present_message_box(app, desktop, roles, info);\n'),
    ("unstated presentation modality", 'auto help = ckv::widgets::present_help_viewer(help_, "intro", app_, *desktop_, roles_);\n'),
    ("unstated presentation modality", 'const auto file = widgets::exec_file_dialog(mode, "/", fs, app, desktop, roles);\n'),
    ("unstated presentation modality", 'desktop_->present_window(std::move(handle), app_);\n'),
    ("unstated presentation modality", 'stage.desktop().present_dialog(std::move(handle), stage.app());\n'),
    ("unstated presentation modality", 'using namespace ckv::widgets;\nauto box = present_message_box(app, desktop, roles, info);\n'),
    ("unstated presentation modality", 'const auto answer = exec_message_box (app, desktop, roles, info);\n'),
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
    'auto box = widgets::present_modal_message_box(app, desktop, roles, info);\n',
    'auto help = widgets::present_modeless_help_viewer(help_, "intro", app_, *desktop_, roles_);\n',
    'const auto file = ckv::widgets::exec_modal_file_dialog(mode, "/", fs, app, desktop, roles);\n',
    'keys_ = desktop_->present_modal(widgets::WindowHandle{std::move(window), first}, app_);\n',
    'desktop.present_modeless(std::move(handle), app);\n',
    'const auto result = stage.desktop().exec_modal(app, std::move(handle));\n',
    'void TodoApp::present_lane_insert(bool before) { present_lane_insert(true); present_help(); }\n',
    'board->on_open = [this] { this->present_message_box_for_board(); };\n',
    '// widgets::present_message_box(...) is how this read before D-087.\n',
    'const char* note = "desktop->present_dialog(handle)";\n',
]

# The library header the self-test's family set comes from: one modal family
# with both spellings, one modeless family.
LIBRARY_FIXTURE = """\
[[nodiscard]] MessageBoxPresentation present_modal_message_box(ui::Application& app, Desktop& desktop);
MessageBoxResult exec_modal_message_box(ui::Application& app, Desktop& desktop);
[[nodiscard]] FileDialogPresentation present_modal_file_dialog(FileDialogMode mode);
[[nodiscard]] HelpViewerPresentation present_modeless_help_viewer(const HelpProvider& provider);
"""


def self_test() -> int:
    problems: list[str] = []
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        header = root / "include" / "cvision" / "widgets" / "presentation.hpp"
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text(LIBRARY_FIXTURE, encoding="utf-8")
        families = presentation_families(root)
        if families != {"message_box", "file_dialog", "help_viewer"}:
            problems.append(f"library presentation families misread: {sorted(families)}")
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
