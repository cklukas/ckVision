# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Require a reference comment on every public declaration in the installed headers.

WP-38's reference rule: a public declaration is documented when a `//` comment
sits immediately above it, or above a run of consecutive declarations with no
blank line between them. What counts as a public declaration:

- every declaration at namespace scope, and in the public section of a class,
  struct or union, including nested types, aliases, data members, and
  functions;
- every macro a header defines.

What does not count, because it introduces no API of its own: a forward
declaration, a using-declaration that re-exports a name from another
namespace, a friend declaration of a class, a deleted function, an override
(documented by the base it overrides; a comment is still expected where the
override narrows that contract, which no lexical scan can tell), enumerators
(the enum's comment covers them), and anything in a private or protected
section or inside a function body.

The scan is lexical and needs no compiler, so it runs on every host the gates
run on.

  check_api_comments.py --root <repo>   check every header under include/cvision
  check_api_comments.py --self-test     prove the scanner accepts and rejects
                                        what it should
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from dataclasses import dataclass, field


@dataclass
class Token:
    text: str
    line: int


@dataclass
class Declaration:
    first_line: int
    last_line: int
    text: str
    counted: bool
    documented: bool = False


@dataclass
class Header:
    """One header split into tokens and a per-line classification."""
    tokens: list[Token]
    line_kind: dict[int, str]           # "blank", "comment", "code" or "preprocessor"
    macros: list[Token] = field(default_factory=list)


_TOKEN = re.compile(r'''
    R"(?P<delim>[^(\s]*)\((?:.|\n)*?\)(?P=delim)"   # raw string literal
  | "(?:[^"\\\n]|\\.)*"                            # string literal
  | '(?:[^'\\\n]|\\.)+'                            # character literal
  | [A-Za-z_]\w*                                    # identifier or keyword
  | \d[\w.']*                                       # number, digit separators included
  | ::|->|<<=|>>=|&&|\|\||[-+*/%&|^!=<>]=?|.       # punctuation
''', re.VERBOSE | re.DOTALL)


def lex(text: str) -> Header:
    """Tokens of the code, with comments, preprocessor lines and literals set aside."""
    lines = text.split("\n")
    line_kind: dict[int, str] = {}
    code_lines: list[str] = []
    macros: list[Token] = []
    in_block = False
    continued = False
    for number, raw in enumerate(lines, start=1):
        stripped = raw.strip()
        if continued:
            line_kind[number] = "preprocessor"
            continued = stripped.endswith("\\")
            code_lines.append("")
            continue
        if not in_block and stripped.startswith("#"):
            conditional = re.match(r'#\s*(if|ifdef|ifndef|elif|else|endif)\b', stripped)
            line_kind[number] = "conditional" if conditional else "preprocessor"
            continued = stripped.endswith("\\")
            define = re.match(r'#\s*define\s+(\w+)', stripped)
            if define:
                macros.append(Token(define.group(1), number))
            code_lines.append("")
            continue
        code, in_block_after = strip_comments(raw, in_block)
        if code.strip():
            line_kind[number] = "code"
        elif stripped == "":
            line_kind[number] = "blank"
        else:
            line_kind[number] = "comment"
        in_block = in_block_after
        code_lines.append(code)
    tokens: list[Token] = []
    source = "\n".join(code_lines)
    line = 1
    position = 0
    for match in _TOKEN.finditer(source):
        line += source.count("\n", position, match.start())
        position = match.start()
        if not match.group().isspace():
            tokens.append(Token(match.group(), line))
    return Header(tokens, line_kind, macros)


def strip_comments(line: str, in_block: bool) -> tuple[str, bool]:
    """The line without its comments; string literals are left for the lexer."""
    out: list[str] = []
    i = 0
    quote = None
    while i < len(line):
        if in_block:
            end = line.find("*/", i)
            if end < 0:
                return "".join(out), True
            i = end + 2
            in_block = False
            out.append(" ")
            continue
        c = line[i]
        if quote:
            out.append(c)
            if c == "\\" and i + 1 < len(line):
                out.append(line[i + 1])
                i += 2
                continue
            if c == quote:
                quote = None
            i += 1
            continue
        if line.startswith("//", i):
            break
        if line.startswith("/*", i):
            in_block = True
            i += 2
            continue
        if c == '"' or (c == "'" and not (i > 0 and line[i - 1].isalnum())):
            quote = c
        out.append(c)
        i += 1
    return "".join(out), in_block


class Parser:
    def __init__(self, header: Header) -> None:
        self.tokens = header.tokens
        self.line_kind = header.line_kind
        self.i = 0
        self.declarations: list[Declaration] = []

    def peek(self, offset: int = 0) -> str | None:
        j = self.i + offset
        return self.tokens[j].text if j < len(self.tokens) else None

    def skip_balanced(self) -> None:
        """Skip from an opening brace to just past its match."""
        depth = 0
        while self.i < len(self.tokens):
            text = self.tokens[self.i].text
            self.i += 1
            if text == "{":
                depth += 1
            elif text == "}":
                depth -= 1
                if depth == 0:
                    return

    def scope(self, kind: str, access: str, enclosing_public: bool) -> None:
        """Parse declarations until the scope's closing brace (consumed) or the end.

        A member is public when its own section is and the type it belongs to is.
        """
        while self.i < len(self.tokens):
            text = self.peek()
            if text == "}":
                self.i += 1
                return
            if text == ";":
                self.i += 1
                continue
            if kind == "class" and text in ("public", "protected", "private") and self.peek(1) == ":":
                access = text
                self.i += 2
                continue
            self.statement(enclosing_public and access == "public")

    def statement(self, public: bool) -> None:
        start = self.tokens[self.i]
        head: list[str] = []
        paren = 0
        last_line = start.line
        body_kind = None
        while self.i < len(self.tokens):
            token = self.tokens[self.i]
            text = token.text
            if text in ("(", "["):
                paren += 1
            elif text in (")", "]"):
                paren -= 1
            elif paren == 0 and text == ";":
                last_line = token.line
                self.i += 1
                break
            elif paren == 0 and text == "{":
                body_kind = self.classify(head)
                if body_kind == "namespace":
                    self.i += 1
                    self.scope("namespace", "public", True)
                    return
                if body_kind == "class":
                    self.i += 1
                    declaration = self.record(start, head + ["{}"], last_line, public)
                    declaration.last_line = -1  # no member continues the type's own run
                    default = "private" if self.class_keyword(head) == "class" else "public"
                    self.scope("class", default, public)
                    # A type definition may declare variables before its semicolon.
                    self.consume_to_semicolon()
                    # A run of declarations continues only after the whole definition.
                    declaration.last_line = self.tokens[self.i - 1].line
                    return
                self.skip_balanced()
                last_line = self.tokens[self.i - 1].line
                if body_kind == "function":
                    head.append("{}")
                    break
                head.append("{}")
                continue
            head.append(text)
            last_line = token.line
            self.i += 1
        if head:
            self.record(start, head, last_line, public)

    def consume_to_semicolon(self) -> None:
        while self.i < len(self.tokens) and self.tokens[self.i].text not in (";", "}"):
            if self.tokens[self.i].text == "{":
                self.skip_balanced()
            else:
                self.i += 1
        if self.i < len(self.tokens) and self.tokens[self.i].text == ";":
            self.i += 1

    @staticmethod
    def outside_template_parameters(words: list[str]) -> list[str]:
        """The words of a declaration head that are not inside `<...>`."""
        depth = 0
        tail: list[str] = []
        for word in words:
            if word == "<":
                depth += 1
            elif word == ">":
                depth -= 1
            elif word == ">>":
                depth -= 2
            elif depth == 0:
                tail.append(word)
        return tail

    @staticmethod
    def class_keyword(head: list[str]) -> str | None:
        for word in Parser.outside_template_parameters(head):
            if word in ("class", "struct", "union"):
                return word
            if word == "(":
                return None
        return None

    def classify(self, head: list[str]) -> str:
        """What the `{` after `head` opens."""
        if "namespace" in head:
            return "namespace"
        if head[:1] == ["extern"] and len(head) >= 2 and head[1].startswith('"'):
            return "namespace"
        if "enum" in head:
            return "enum"
        if head and head[-1] in ("=", ",", "return") or not head:
            return "initializer"
        # `template <class T> struct X` names its keyword inside the angle brackets too;
        # the class keyword that matters is the one after the template parameter list.
        before_paren = head[:head.index("(")] if "(" in head else head
        tail = self.outside_template_parameters(before_paren)
        if "(" not in head and any(word in ("class", "struct", "union") for word in tail):
            return "class"
        if "(" in head:
            return "function"
        return "initializer"  # `Type name{...}` or `Type name = {...}`

    def record(self, start: Token, head: list[str], last_line: int, public: bool) -> Declaration:
        counted = public and not self.exempt(head)
        declaration = Declaration(start.line, last_line, " ".join(w for w in head if w != "{}")[:100], counted)
        declaration.documented = self.documented(start.line)
        self.declarations.append(declaration)
        return declaration

    @staticmethod
    def exempt(head: list[str]) -> bool:
        words = [w for w in head if w != "{}"]
        if not words:
            return True
        if words[0] in ("static_assert", "friend"):
            # A friend function defined here is part of the type's interface
            # (operator== and the like); a friend class grant is not.
            return words[0] == "static_assert" or (len(words) >= 2 and words[1] in ("class", "struct"))
        if words[0] == "using" and "=" not in words and "namespace" not in words:
            return True  # a using-declaration re-exports a name documented elsewhere
        if words[0] == "using" and "namespace" in words:
            return True
        if "{}" not in head and Parser.is_forward(words):
            return True  # a forward declaration: `class X;`, `template <...> struct Y;`
        if "override" in words:
            return True
        if len(words) >= 2 and words[-2:] == ["=", "delete"]:
            return True
        return False

    @staticmethod
    def is_forward(words: list[str]) -> bool:
        rest = [w for w in Parser.outside_template_parameters(words) if w != "template"]
        return len(rest) == 2 and rest[0] in ("class", "struct", "union")

    def documented(self, line: int) -> bool:
        above = line - 1
        # A declaration only some platforms see keeps the comment above its `#if`.
        while self.line_kind.get(above) == "conditional":
            above -= 1
        kind = self.line_kind.get(above)
        if kind == "comment":
            return True
        if kind != "code":
            return False
        # Part of a run: the declaration ending on the line above carries the run's comment.
        for previous in reversed(self.declarations):
            if previous.last_line == above:
                return previous.documented
            if previous.last_line < above:
                break
        return False


def scan(text: str, path: str) -> tuple[int, list[tuple[int, str]]]:
    """Count the public declarations in one header and list the undocumented ones."""
    header = lex(text)
    parser = Parser(header)
    parser.scope("namespace", "public", True)
    counted = [d for d in parser.declarations if d.counted]
    missing = [(d.first_line, d.text) for d in counted if not d.documented]
    for macro in header.macros:
        counted.append(Declaration(macro.line, macro.line, macro.text, True))
        if header.line_kind.get(macro.line - 1) not in ("comment", "preprocessor", "conditional"):
            missing.append((macro.line, "#define " + macro.text))
    return len(counted), sorted(missing)


def check(root: pathlib.Path) -> int:
    total = 0
    problems = 0
    headers = sorted((root / "include/cvision").rglob("*.hpp"))
    for header in headers:
        path = header.relative_to(root).as_posix()
        count, missing = scan(header.read_text(encoding="utf-8"), path)
        total += count
        problems += len(missing)
        for line, text in missing:
            print(f"{path}:{line}: undocumented public declaration: {text}")
    print(f"{len(headers)} headers, {total} public declarations, {problems} undocumented")
    return 1 if problems else 0


SELF_TEST_CASES: list[tuple[str, str, int, int]] = [
    ("documented free function", "namespace ckv {\n// Adds.\nint add(int a, int b);\n}\n", 1, 0),
    ("undocumented free function", "namespace ckv {\nint add(int a, int b);\n}\n", 1, 1),
    ("a run shares one comment", "namespace ckv {\n// Arithmetic.\nint add(int, int);\nint sub(int, int);\n}\n", 2, 0),
    ("a blank line ends the run", "namespace ckv {\n// Adds.\nint add(int, int);\n\nint sub(int, int);\n}\n", 2, 1),
    ("private members need no comment",
     "namespace ckv {\n// A thing.\nclass T {\npublic:\n    // Size.\n    int size() const;\nprivate:\n    int size_ = 0;\n};\n}\n",
     2, 0),
    ("a class starts private",
     "namespace ckv {\n// A thing.\nclass T {\n    int hidden_;\npublic:\n    // Shown.\n    int shown();\n};\n}\n", 2, 0),
    ("undocumented public member", "namespace ckv {\n// A thing.\nstruct T {\n    int size() const;\n};\n}\n", 2, 1),
    ("an access specifier breaks a run",
     "namespace ckv {\n// A thing.\nclass T {\npublic:\n    int size() const;\n};\n}\n", 2, 1),
    ("an override is documented by its base",
     "namespace ckv {\n// A thing.\nclass T : public B {\npublic:\n    void draw() override;\n};\n}\n", 1, 0),
    ("inline bodies are not declarations",
     "namespace ckv {\n// Twice.\ninline int twice(int v) {\n    int r = v;\n    return r * 2;\n}\n}\n", 1, 0),
    ("enumerators are not declarations", "namespace ckv {\n// Colours.\nenum class C {\n    Red,\n    Green,\n};\n}\n", 1, 0),
    ("a nested public member needs a comment",
     "namespace ckv {\n// Outer.\nstruct O {\n    // Inner.\n    struct I {\n        int v = 0;\n    };\n};\n}\n", 3, 1),
    ("a multi-line declaration counts once", "namespace ckv {\n// Joins.\nstd::string join(int a,\n                 int b);\n}\n", 1, 0),
    ("a comment after a blank line documents the next run", "namespace ckv {\n// A.\nint a();\n\n// B.\nint b();\n}\n", 2, 0),
    ("a string brace does not open a scope",
     "namespace ckv {\n// Brace.\ninline const char* brace() { return \"{\"; }\n// Next.\nint next();\n}\n", 2, 0),
    ("a run continues after an inline body",
     "namespace ckv {\n// F and G.\ninline int f() { return 1; }\ninline int g() { return 2; }\n}\n", 2, 0),
    ("a class after an inline body starts undocumented",
     "namespace ckv {\n// F.\ninline int f() { return 1; }\n\nclass X {\npublic:\n    int v();\n};\n}\n", 3, 2),
    ("forward declarations and re-exports are exempt",
     "namespace ckv {\nclass Application;\ntemplate <class T> struct Box;\nusing ui::View;\n}\n", 0, 0),
    ("an alias is a declaration", "namespace ckv {\nusing Id = int;\n}\n", 1, 1),
    ("a deleted copy is exempt", "namespace ckv {\n// T.\nclass T {\npublic:\n    T(const T&) = delete;\n};\n}\n", 1, 0),
    ("a template class keyword in the parameter list",
     "namespace ckv {\n// Box.\ntemplate <class T>\nstruct Box {\n    // Value.\n    T value;\n};\n}\n", 2, 0),
    ("a macro needs a comment", "#define CK_ONE 1\n// Two.\n#define CK_TWO 2\n", 2, 1),
    ("a comment above a platform conditional documents what it guards",
     "namespace ckv {\n// Posix only.\n#if defined(X)\nint posix();\n#endif\n\n#if defined(Y)\nint other();\n#endif\n}\n", 2, 1),
    ("a braced initializer is not a scope",
     "namespace ckv {\n// Table.\ninline constexpr int table[] = {1, 2, 3};\n// Size.\ninline constexpr Size size{1, 2};\n}\n", 2, 0),
]


def self_test() -> int:
    failures = 0
    for name, text, want_count, want_missing in SELF_TEST_CASES:
        count, missing = scan(text, name)
        if count != want_count or len(missing) != want_missing:
            failures += 1
            print(f"FAIL {name}: {count} declarations, {len(missing)} undocumented; "
                  f"expected {want_count} and {want_missing}")
            for line, text in missing:
                print(f"    line {line}: {text}")
    print(f"{len(SELF_TEST_CASES) - failures}/{len(SELF_TEST_CASES)} self-test cases pass")
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
