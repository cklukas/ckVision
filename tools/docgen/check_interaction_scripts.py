# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Require an Application-level interaction script for every public view type.

WP-38's interaction-script rule, checked against the "Interaction scripts"
table in docs/coverage.md. Each row names a view type, one CK_TEST case that
drives it, and the identifier through which that case reaches it:

    | `ListView` | `test_list_view.cpp` `a_case_name` | `ListView` |

The view types are enumerated exactly as the documentation-coverage gate does
(check_docs.view_types: every public widget type that derives from ui::View,
transitively). The gate passes only when:

- every view type has at least one row, and every row names a view type;
- the named test file exists under tests/ and defines CK_TEST(<case>);
- the case calls Application::dispatch, HeadlessTerminal::inject_event or
  Application::step, so events reach the view through the public paths an
  application uses;
- the case makes no direct call to a view's on_key, on_key_release,
  on_mouse, on_text or on_paste handler, which would bypass that routing;
- the case body names the "reached through" identifier as a whole word: the
  view type itself when the case constructs it, or the public factory,
  example entry point or test fixture that builds it (a help viewer's
  TextView is reached through make_help_viewer, for instance).

"The case calls" covers the case body and, transitively, every function the
same test file defines and the body calls by name -- a fixture's press()
that dispatches a key is the case dispatching it. The same reach applies to
the direct-handler rule, so a helper cannot launder a handler call either.

The scan is lexical and needs no compiler, so it runs on every host the gates
run on.

  check_interaction_scripts.py --root <repo>   check the map against the tests
  check_interaction_scripts.py --self-test     prove the checker accepts and
                                               rejects what it should
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from dataclasses import dataclass

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from check_docs import view_types  # noqa: E402  (the one shared enumeration)

MAP_HEADING = "## Interaction scripts"
ROW = re.compile(r"^\| `(\w+)` \| `([\w.]+\.cpp)` `(\w+)` \| `(\w+)` \|\s*$")
DRIVE = re.compile(r"\b(?:dispatch|inject_event|step)\s*\(")
DIRECT_HANDLER = re.compile(r"(?:\.|->)\s*on_(?:key|key_release|mouse|text|paste)\s*\(")


@dataclass(frozen=True)
class Entry:
    view_type: str
    test_file: str
    case: str
    reached_through: str
    line: int


def parse_map(text: str) -> tuple[list[Entry], list[str]]:
    """The rows of the map section, and any row in it that does not parse."""
    entries: list[Entry] = []
    errors: list[str] = []
    in_section = False
    for number, line in enumerate(text.splitlines(), start=1):
        if line.startswith("## "):
            in_section = line.strip() == MAP_HEADING
            continue
        if not in_section or not line.startswith("| `"):
            continue
        match = ROW.match(line)
        if match is None:
            errors.append(f"coverage.md:{number}: malformed interaction-script row: {line}")
            continue
        entries.append(Entry(*match.groups(), line=number))
    return entries, errors


def inside_number(source: str, quote: int) -> bool:
    """Whether the quote at `quote` separates digits of a numeric literal."""
    start = quote
    while start > 0 and (source[start - 1] in "'.xX" or source[start - 1] in "0123456789abcdefABCDEF"):
        start -= 1
    preceded_by_identifier = start > 0 and (source[start - 1].isalnum() or source[start - 1] == "_")
    return start < quote and source[start].isdigit() and not preceded_by_identifier


def strip_literals(source: str) -> str:
    """The source with comments, string and character literals blanked out.

    Keeps every newline and every other character's position, so braces and
    identifiers are found only where the compiler would see them.
    """
    out: list[str] = []
    i = 0
    n = len(source)
    while i < n:
        c = source[i]
        if source.startswith("//", i):
            end = source.find("\n", i)
            end = n if end < 0 else end
            out.append(" " * (end - i))
            i = end
        elif source.startswith("/*", i):
            end = source.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", source[i:end]))
            i = end
        elif source.startswith('R"', i):
            open_paren = source.find("(", i + 2)
            delimiter = source[i + 2:open_paren]
            end = source.find(")" + delimiter + '"', open_paren)
            end = n if end < 0 else end + len(delimiter) + 2
            out.append(re.sub(r"[^\n]", " ", source[i:end]))
            i = end
        elif c == "'" and inside_number(source, i):
            out.append(c)  # a digit separator, as in 1'000
            i += 1
        elif c in "\"'":
            j = i + 1
            while j < n and source[j] != c and source[j] != "\n":
                j += 2 if source[j] == "\\" else 1
            out.append(" " * (j + 1 - i))
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def braced_block(code: str, open_brace: int) -> str:
    """The block that opens at `open_brace`, braces included."""
    depth = 0
    for end in range(open_brace, len(code)):
        if code[end] == "{":
            depth += 1
        elif code[end] == "}":
            depth -= 1
            if depth == 0:
                return code[open_brace:end + 1]
    return code[open_brace:]


# A function definition's head: a name, a parameter list (one level of nested
# parentheses), optional qualifiers or a trailing return type, then its body.
FUNCTION_HEAD = re.compile(
    r"\b([A-Za-z_]\w*)\s*\((?:[^()]|\([^()]*\))*\)\s*(?:const\s*)?(?:noexcept\s*)?"
    r"(?:override\s*)?(?:->\s*[\w:<>,\s*&]+?)?\{")
NOT_A_FUNCTION = {"if", "for", "while", "switch", "catch", "CK_TEST", "sizeof", "decltype", "return"}


@dataclass
class TestFile:
    """One test source: its CK_TEST cases and the functions it defines."""
    cases: dict[str, str]
    helpers: dict[str, list[str]]

    def reaches(self, body: str, pattern: re.Pattern[str]) -> bool:
        """Whether `body`, or a helper it calls by name (transitively), matches `pattern`."""
        pending = [body]
        seen: set[str] = set()
        while pending:
            text = pending.pop()
            if pattern.search(text):
                return True
            for name in re.findall(r"\b([A-Za-z_]\w*)\s*\(", text):
                if name in self.helpers and name not in seen:
                    seen.add(name)
                    pending.extend(self.helpers[name])
        return False


def parse_test_file(source: str) -> TestFile:
    code = strip_literals(source)
    cases: dict[str, str] = {}
    case_spans: list[tuple[int, int]] = []
    for match in re.finditer(r"^CK_TEST\((\w+)\)\s*\{", code, re.MULTILINE):
        body = braced_block(code, match.end() - 1)
        cases[match.group(1)] = body
        case_spans.append((match.start(), match.end() - 1 + len(body)))
    helpers: dict[str, list[str]] = {}
    for match in FUNCTION_HEAD.finditer(code):
        name = match.group(1)
        if name in NOT_A_FUNCTION or any(start <= match.start() < end for start, end in case_spans):
            continue
        helpers.setdefault(name, []).append(braced_block(code, match.end() - 1))
    return TestFile(cases, helpers)


def test_cases(source: str) -> dict[str, str]:
    """Each CK_TEST case's body, braces included, keyed by case name."""
    return parse_test_file(source).cases


def check_entries(types: list[str], entries: list[Entry], sources: dict[str, str]) -> list[str]:
    """Every violation of the rule in the module docstring, one line each."""
    errors: list[str] = []
    mapped = {entry.view_type for entry in entries}
    for name in types:
        if name not in mapped:
            errors.append(f"view type {name} has no interaction-script row in coverage.md")
    parsed: dict[str, TestFile] = {}
    for entry in entries:
        where = f"coverage.md:{entry.line}: {entry.view_type}"
        if entry.view_type not in types:
            errors.append(f"{where}: not a public view type")
        if entry.test_file not in sources:
            errors.append(f"{where}: no test file tests/{entry.test_file}")
            continue
        if entry.test_file not in parsed:
            parsed[entry.test_file] = parse_test_file(sources[entry.test_file])
        test_file = parsed[entry.test_file]
        body = test_file.cases.get(entry.case)
        if body is None:
            errors.append(f"{where}: {entry.test_file} defines no CK_TEST({entry.case})")
            continue
        if not test_file.reaches(body, DRIVE):
            errors.append(f"{where}: {entry.case} never calls dispatch, inject_event or step")
        if test_file.reaches(body, DIRECT_HANDLER):
            errors.append(f"{where}: {entry.case} calls a view's input handler directly")
        if not re.search(rf"\b{re.escape(entry.reached_through)}\b", body):
            errors.append(f"{where}: {entry.case} never names {entry.reached_through}")
    return errors


def check(root: pathlib.Path) -> int:
    coverage = root / "docs/coverage.md"
    entries, errors = parse_map(coverage.read_text(encoding="utf-8"))
    types = view_types(root)
    sources = {path.name: path.read_text(encoding="utf-8") for path in sorted((root / "tests").glob("*.cpp"))}
    errors += check_entries(types, entries, sources)
    for error in errors:
        print(error)
    print(f"{len(types)} view types, {len(entries)} interaction-script rows, {len(errors)} problems")
    return 1 if errors else 0


def self_test_map(*rows: str) -> str:
    return "# Coverage\n\n" + MAP_HEADING + "\n\n| View type | Script | Reached through |\n|---|---|---|\n" + \
        "\n".join(rows) + "\n\n## Next section\n\n| `Ignored` | not a row of the map |\n"


SELF_TEST_SOURCE = """
CK_TEST(scripted) {
    Slider* slider = add_slider(app);
    app.dispatch(key(Key::Right));  // a comment naming on_key(...) is not a call
    const char* text = "slider->on_mouse(event)";
}

CK_TEST(direct) {
    Slider slider;
    slider.on_key(key(Key::Right));
    app.step(0);
}

CK_TEST(unscripted) {
    Slider slider;
    slider.set_value(3);
}

CK_TEST(factory_reached) {
    auto viewer = make_viewer(app);
    term.inject_event(key(Key::Down));
    if (viewer) { app.step(0); }
}

CK_TEST(names_only_a_longer_identifier) {
    SliderGroup group;
    app.dispatch(key(Key::Right));
}

struct Harness {
    void press(Key k) { app.dispatch(key(k)); }
    void poke(Key k) const { slider.on_key(key(k)); }
    void settle() { press(Key::Escape); }
};

CK_TEST(drives_through_a_helper) {
    Harness h;
    h.settle();
}

CK_TEST(launders_a_handler_call_through_a_helper) {
    Harness h;
    h.press(Key::Up);
    h.poke(Key::Down);
}
"""

# (description, map rows, view types, expected number of problems)
SELF_TEST_CASES: list[tuple[str, list[str], list[str], int]] = [
    ("a scripted case satisfies the rule", ["| `Slider` | `t.cpp` `scripted` | `Slider` |"], ["Slider"], 0),
    ("a factory counts as the route", ["| `Viewer` | `t.cpp` `factory_reached` | `make_viewer` |"], ["Viewer"], 0),
    ("an unmapped view type fails", ["| `Slider` | `t.cpp` `scripted` | `Slider` |"], ["Slider", "Knob"], 1),
    ("a row for a non-view type fails", ["| `Knob` | `t.cpp` `scripted` | `Slider` |"], ["Slider"], 2),
    ("a missing test file fails", ["| `Slider` | `u.cpp` `scripted` | `Slider` |"], ["Slider"], 1),
    ("a missing case fails", ["| `Slider` | `t.cpp` `absent` | `Slider` |"], ["Slider"], 1),
    ("a case that drives nothing fails", ["| `Slider` | `t.cpp` `unscripted` | `Slider` |"], ["Slider"], 1),
    ("a direct handler call fails", ["| `Slider` | `t.cpp` `direct` | `Slider` |"], ["Slider"], 1),
    ("the route must be named as a whole word",
     ["| `Slider` | `t.cpp` `names_only_a_longer_identifier` | `Slider` |"], ["Slider"], 1),
    ("a malformed row fails", ["| `Slider` | t.cpp scripted | `Slider` |"], ["Slider"], 2),
    ("a helper that dispatches, reached transitively, counts",
     ["| `Slider` | `t.cpp` `drives_through_a_helper` | `Harness` |"], ["Slider"], 0),
    ("a helper cannot launder a direct handler call",
     ["| `Slider` | `t.cpp` `launders_a_handler_call_through_a_helper` | `Harness` |"], ["Slider"], 1),
]


def self_test() -> int:
    failures = 0
    sources = {"t.cpp": SELF_TEST_SOURCE}
    for name, rows, types, want in SELF_TEST_CASES:
        entries, errors = parse_map(self_test_map(*rows))
        errors += check_entries(types, entries, sources)
        if len(errors) != want:
            failures += 1
            print(f"FAIL {name}: {len(errors)} problems, expected {want}")
            for error in errors:
                print(f"    {error}")
    literal_free = strip_literals('a("}") /* { */ b(\'{\') n(1\'000) u8\'}\' // }\nc')
    if literal_free.count("{") or literal_free.count("}") or not literal_free.endswith("\nc") \
            or "1'000" not in literal_free:
        failures += 1
        print(f"FAIL literals and comments are blanked: {literal_free!r}")
    total = len(SELF_TEST_CASES) + 1
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
