#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Rewrite citations of unpublished documents in the publication snapshot.

The source comments cite the design record by path — "(ARCHITECTURE.md §5)",
"under DECISIONS.md D-012", "(plans/widgets.md M5 baseline)". Those paths are
correct in this repository and dangling in the published one, where none of
those files exist. This replaces the path with the document's name in prose, so
the reasoning survives and the dead pointer does not.

It edits the snapshot index only. Run with GIT_INDEX_FILE pointing at the
temporary index make-public.sh builds; the working tree is never touched.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys

from spans import rewritable_spans

# Each specification document, as a reader of the published tree should see it
# named. The keys are the forms the comments actually use: the bare number
# ("plans/03"), the number with its slug, and the full filename.
PLANS: dict[str, str] = {
    "widgets": "the widget catalog",
    "corrective-assurance": "the corrective assurance plan",
    "api-ergonomics-review-2": "the API ergonomics review",
    "editor-architecture": "the editor architecture plan",
    "editor-delivery-target": "the editor delivery plan",
    "embedded-terminal-architecture": "the embedded terminal architecture",
    "embedded-terminal-delivery-target": "the embedded terminal delivery plan",
    "documentation-revamp": "the documentation plan",
    "data-flow-architecture": "the data flow architecture",
    "clean-room-derivation": "the clean-room derivation record",
    "example-coverage": "the example coverage matrix",
    "mac-first-delivery-target": "the Mac-first delivery plan",
    "mac-first-traceability": "the Mac-first traceability matrix",
    "mac-performance-baseline": "the performance baseline",
    "term_capabilities": "the terminal capability matrix",
}
ROOT_DOCUMENTS: dict[str, str] = {
    "ARCHITECTURE.md": "the architecture",
    "DECISIONS.md": "the decision log",
    "ROADMAP.md": "the roadmap",
    "VISION.md": "the vision",
}
INTERNAL_PLANS = "the internal plans"
RELEASE_AUDIT = "the release audit"
AGENTS = "the engineering standard"

# A markdown link whose target is an unpublished document, e.g.
# "[widgets](../plans/widgets.md)". The whole construct goes, label included:
# a label naming a file nobody can open is no better than the link.
MD_LINK = re.compile(
    r"\[[^\]\n]*\]\(\s*(?:\.\./)*"
    r"(?P<target>plans/[^)\s]*|(?:docs/)?release-audit\.md"
    r"|ARCHITECTURE\.md|DECISIONS\.md|ROADMAP\.md|VISION\.md|AGENTS\.md)\s*\)")

# A bare citation. "§5", "D-060" and "'s" are prose and stay; a ":123" points
# into a document nobody can open, so it goes with the path.
TOKEN = re.compile(
    r"(?:\.\./)*plans/(?P<plan>[A-Za-z0-9_-]+)?(?:\.md)?(?P<line>:\d+)?"
    r"|(?:\.\./)*(?:docs/)?release-audit\.md"
    r"|(?<![\w])(?:ARCHITECTURE|DECISIONS|ROADMAP|VISION|AGENTS)\.md")

# `plans/widgets.md` is marked up as a path. The name that replaces it is
# prose, and prose in code font reads as an identifier that does not exist.
BACKTICKED = re.compile("`(?:" + TOKEN.pattern + ")`")

# Rewriting a file that is read as data, rather than as prose, would change
# behavior rather than a pointer.
TEXT_SUFFIXES = {".cc", ".cmake", ".cpp", ".h", ".hpp", ".md", ".py", ".sh",
                 ".yaml", ".yml"}
TEXT_NAMES = {".gitattributes", "CMakeLists.txt"}
SKIP_PREFIXES = ("fuzz/corpus/",)

# A citation that opens a sentence has to keep opening one, and one that
# already has an article in front of it must not grow a second.
SENTENCE_END = re.compile(r"(?:^|[.!?]|//+|/\*|^\s*\*|#)$")
DETERMINER = re.compile(r"(?:\b(?:an?|An?|the|The|this|This)|[\w)]'s|[\w)]’s)$")


# "plans/widgets.md", "../plans/widgets.md", "ARCHITECTURE.md" — one parser,
# so a markdown link and a bare citation cannot disagree about a name.
TARGET = re.compile(r"(?:\.\./)*plans/(?P<plan>[A-Za-z0-9_-]+)?(?:\.md)?(?:#.*)?$")


def phrase_for_target(target: str) -> str:
    for filename, phrase in ROOT_DOCUMENTS.items():
        if target.endswith(filename):
            return phrase
    if target.endswith("AGENTS.md"):
        return AGENTS
    if target.endswith("release-audit.md"):
        return RELEASE_AUDIT
    parsed = TARGET.match(target)
    if parsed is None:
        return INTERNAL_PLANS
    return PLANS.get(parsed.group("plan") or "", INTERNAL_PLANS)


def phrase_for(match: re.Match[str]) -> str:
    groups = match.groupdict()
    if "target" in groups:
        return phrase_for_target(groups["target"])
    text = match.group(0)
    if groups.get("line"):
        text = text[: -len(groups["line"])]
    return phrase_for_target(text.strip("`"))


def rewrite_span(text: str) -> tuple[str, int]:
    count = 0

    def replace(match: re.Match[str]) -> str:
        nonlocal count
        count += 1
        phrase = phrase_for(match)
        before = match.string[:match.start()].rstrip(" \t")
        # "half of the plans/03 row" and "a DECISIONS.md matter" already
        # supply the article; a second one would read as a stammer.
        if DETERMINER.search(before):
            return phrase.removeprefix("the ")
        if SENTENCE_END.search(before[-2:] if before else ""):
            phrase = phrase[0].upper() + phrase[1:]
        return phrase

    text = MD_LINK.sub(replace, text)
    text = BACKTICKED.sub(replace, text)
    text = TOKEN.sub(replace, text)
    return text, count


def rewrite(path: str, text: str) -> tuple[str, int]:
    """Rewrite citations in the prose of `text`, leaving its code alone."""
    pieces: list[str] = []
    total = 0
    cursor = 0
    for start, end in rewritable_spans(path, text):
        if start < cursor:                       # defensive: never emit overlap
            continue
        rewritten, count = rewrite_span(text[start:end])
        pieces.append(text[cursor:start])
        pieces.append(rewritten)
        total += count
        cursor = end
    pieces.append(text[cursor:])
    return "".join(pieces), total


def eligible(path: str) -> bool:
    if path.startswith(SKIP_PREFIXES):
        return False
    name = path.rsplit("/", 1)[-1]
    suffix = name[name.rfind("."):] if "." in name else ""
    return name in TEXT_NAMES or suffix in TEXT_SUFFIXES


def git(*args: str, stdin: bytes | None = None) -> bytes:
    return subprocess.run(("git",) + args, check=True, input=stdin,
                          stdout=subprocess.PIPE).stdout


def main() -> int:
    if not os.environ.get("GIT_INDEX_FILE"):
        print("derefer: refusing to run without GIT_INDEX_FILE — this edits an "
              "index, and the repository index is not the one it may edit",
              file=sys.stderr)
        return 2

    updates: list[str] = []
    files = citations = 0
    for entry in git("ls-files", "-s").decode().splitlines():
        meta, path = entry.split("\t", 1)
        mode, blob, _stage = meta.split()
        if not eligible(path):
            continue
        raw = git("cat-file", "blob", blob)
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            continue
        new_text, count = rewrite(path, text)
        if not count:
            continue
        new_blob = git("hash-object", "-w", "--stdin",
                       stdin=new_text.encode("utf-8")).decode().strip()
        updates.append(f"{mode} {new_blob}\t{path}")
        files += 1
        citations += count

    if updates:
        git("update-index", "--index-info",
            stdin=("\n".join(updates) + "\n").encode())
    print(f"derefer: rewrote {citations} citations in {files} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
