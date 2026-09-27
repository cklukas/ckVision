# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Regenerate the appearance matrix and prove it is complete and pinned.

The generator (tools/docgen/generate_appearance_matrix) renders every
specimen x state x scheme. This check:

1. regenerates into scratch and requires exactly the pinned file tree under
   tests/golden/appearance, byte for byte;
2. requires every public view type to be a catalog element or an explained
   exemption, and nothing to be exempt that is also an element;
3. requires the states each element's traits demand (the internal plans
   matrix.md): every element `normal`; a focusable element `focused`; a
   control, which sets a value of the reader's, also `disabled`;
   text-bearing `wide` and `narrow`; window chrome `active` and `inactive`;
   raster `sixel` (with its decoded plane) and `no-graphics`;
4. requires every state in all four schemes, and requires the pictures to
   say what the states claim: the four schemes of one state are pairwise
   different (or, for a state the catalog declares fixed with a reason,
   identical), and in every scheme `focused` (for anything that takes the
   keyboard) and `disabled` (for a control) differ from `normal`;
5. requires a symbolic scene dump (`<state>.<scheme>.scene`, the composed
   frame with its `raster` records) for every state of a raster element and
   for nothing else, and in every scheme requires the `sixel` scene to record
   at least one raster and to equal the `no-graphics` scene: the same script
   composes the same scene, and only the presenter decides whether pixels
   cover the fallback (D-080).
"""

from __future__ import annotations

import argparse
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys
import uuid

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "docgen"))
from check_docs import view_types  # noqa: E402

REQUIRED = {
    "focusable": ("focused",),
    "control": ("focused", "disabled"),
    "text": ("wide", "narrow"),
    "window": ("active", "inactive"),
    "raster": ("sixel", "no-graphics"),
}
# States that exist to be told apart from `normal` in every scheme, and the
# traits that make them so.
DISTINCT_FROM_NORMAL = {"focused": ("focusable", "control"), "disabled": ("control",)}


def parse_catalog(text: str):
    lines = text.splitlines()
    if not lines or lines[0] != "ckvision-appearance-catalog 1":
        raise ValueError("catalog.txt: unknown header")
    schemes: list[str] = []
    elements: dict[str, dict] = {}
    exemptions: dict[str, str] = {}
    for line in lines[1:]:
        kind, _, rest = line.partition(" ")
        if kind == "schemes":
            schemes = rest.split()
        elif kind == "element":
            name, header, *traits = rest.split()
            elements[name] = {"header": header, "traits": set(traits) - {"plain"}, "states": {}}
        elif kind == "state":
            element, state, size, graphics = rest.split()
            elements[element]["states"][state] = graphics
        elif kind == "fixed":
            element, state, reason = rest.split(" ", 2)
            elements[element].setdefault("fixed", {})[state] = reason
        elif kind == "exempt":
            name, _, reason = rest.partition(" ")
            exemptions[name] = reason
        else:
            raise ValueError(f"catalog.txt: unknown line {line!r}")
    return schemes, elements, exemptions


def tree(directory: pathlib.Path) -> dict[str, bytes]:
    return {path.relative_to(directory).as_posix(): path.read_bytes()
            for path in sorted(directory.rglob("*")) if path.is_file()}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generator", type=pathlib.Path, required=True)
    parser.add_argument("--fixtures", type=pathlib.Path, required=True)
    parser.add_argument("--scratch", type=pathlib.Path, required=True)
    args = parser.parse_args()

    scratch = args.scratch.resolve()
    scratch.mkdir(parents=True, exist_ok=True)
    session = scratch / uuid.uuid4().hex
    session.mkdir()
    output = session / "appearance"
    environment = os.environ.copy()
    environment.update(TMPDIR=str(session), TMP=str(session), TEMP=str(session))
    completed = subprocess.run([str(args.generator), str(output)], capture_output=True, text=True,
                               check=False, env=environment)
    if completed.returncode:
        print(f"generator failed ({completed.returncode}): {completed.stderr.strip()}", file=sys.stderr)
        return 1

    failures: list[str] = []
    generated = tree(output)
    pinned = tree(args.fixtures)
    for name in sorted(set(pinned) - set(generated)):
        failures.append(f"pinned but not generated: {name}")
    for name in sorted(set(generated) - set(pinned)):
        failures.append(f"generated but not pinned: {name}")
    for name in sorted(set(generated) & set(pinned)):
        if generated[name] != pinned[name]:
            failures.append(f"{name}: generated SHA-256 {hashlib.sha256(generated[name]).hexdigest()} "
                            f"!= pinned {hashlib.sha256(pinned[name]).hexdigest()}")

    schemes, elements, exemptions = parse_catalog(generated["catalog.txt"].decode("utf-8"))
    if schemes != ["classic", "dark", "light", "mono"]:
        failures.append(f"catalog schemes are {schemes}, not the four built-in schemes")

    for name in view_types(ROOT):
        if name not in elements and name not in exemptions:
            failures.append(f"public view type {name} is neither an appearance element nor exempt")
    for name, reason in exemptions.items():
        if name in elements:
            failures.append(f"{name} is both an element and exempt")
        if not reason.strip():
            failures.append(f"exemption {name} gives no reason")

    for name, element in elements.items():
        if not (ROOT / element["header"]).is_file():
            failures.append(f"{name}: header {element['header']} does not exist")
        states = element["states"]
        required = ["normal"]
        for trait in element["traits"]:
            required.extend(REQUIRED[trait])
        for state in dict.fromkeys(required):
            if state not in states:
                failures.append(f"{name}: missing required state {state!r} ({', '.join(sorted(element['traits'])) or 'plain'})")
        if "raster" in element["traits"]:
            if states.get("sixel") != "sixel":
                failures.append(f"{name}: state 'sixel' is not captured under the Sixel profile")
            if states.get("no-graphics") != "none":
                failures.append(f"{name}: state 'no-graphics' is not captured without graphics")
        for state, graphics in states.items():
            dumps = {}
            for scheme in schemes:
                key = f"{name}/{state}.{scheme}.dump"
                if key not in generated:
                    failures.append(f"{key}: not generated")
                else:
                    dumps[scheme] = generated[key]
            if graphics == "sixel" and f"{name}/{state}.rgba" not in generated:
                failures.append(f"{name}/{state}: Sixel state has no decoded plane")
            values = list(dumps.values())
            if state in element.get("fixed", {}):
                if len(set(values)) != 1:
                    failures.append(f"{name}/{state}: declared fixed, but the schemes render differently")
            elif len(set(values)) != len(values):
                failures.append(f"{name}/{state}: two schemes render identically")
            if state in DISTINCT_FROM_NORMAL and element["traits"] & set(DISTINCT_FROM_NORMAL[state]):
                for scheme in schemes:
                    normal = generated.get(f"{name}/normal.{scheme}.dump")
                    if normal is not None and normal == dumps.get(scheme):
                        failures.append(f"{name}/{state}.{scheme}: renders exactly as normal")

        if "raster" in element["traits"]:
            for state in states:
                for scheme in schemes:
                    if f"{name}/{state}.{scheme}.scene" not in generated:
                        failures.append(f"{name}/{state}.{scheme}: raster element has no symbolic scene dump")
            for scheme in schemes:
                sixel = generated.get(f"{name}/sixel.{scheme}.scene")
                fallback = generated.get(f"{name}/no-graphics.{scheme}.scene")
                if sixel is None or fallback is None:
                    continue  # reported above
                if not any(line.startswith("raster ") for line in sixel.decode("utf-8").splitlines()):
                    failures.append(f"{name}/sixel.{scheme}.scene: records no raster")
                if sixel != fallback:
                    failures.append(f"{name}: sixel.{scheme}.scene and no-graphics.{scheme}.scene differ; "
                                    "the graphics profile must not change the composed scene")

    for key in generated:
        element_name, _, file_name = key.partition("/")
        if file_name.endswith(".scene") and "raster" not in elements.get(element_name, {}).get("traits", set()):
            failures.append(f"{key}: a symbolic scene dump for an element without the raster trait")

    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        print(f"generated files retained at {session}", file=sys.stderr)
        return 1
    shutil.rmtree(session)
    manifest = hashlib.sha256()
    for name, data in sorted(generated.items()):
        manifest.update(name.encode("utf-8") + b"\0" + len(data).to_bytes(8, "little") + data)
    state_count = sum(len(element["states"]) for element in elements.values())
    scene_count = sum(1 for key in generated if key.endswith(".scene"))
    print(f"appearance matrix: {len(elements)} elements, {state_count} states x {len(schemes)} schemes, "
          f"{len(exemptions)} exemptions, {len(generated)} files match ({scene_count} symbolic scenes); "
          f"manifest SHA-256 {manifest.hexdigest()}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError) as error:
        print(f"appearance matrix check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
