# VT conformance harness

Terminal semantics are not a matter of taste. "Does an erase keep the
underline attribute?" is not a question anybody on this project should answer
from memory or from what seems reasonable — it has an answer that established
terminals already agree on, and the way to get it is to run the same bytes
through them and compare the screens.

This harness does that. It plays a byte script on ckVision's embedded
terminal emulator and on a reference terminal, prints both screens in the same
form, and reports every cell where they differ.

```
cmake --build build --target ckvision_vt_dump
python3 tools/vtconform/conform.py --dump-tool build/ckvision_vt_dump
```

Add `-v` to print the runs even where they agree, or `--case NAME` to look at
one script.

## What the two halves are

`dump_emulator.cpp` builds `ckvision_vt_dump`: it reads a script on stdin,
feeds it to `term::TerminalEmulator`, and prints one line per run of
identically-styled cells — row, columns, foreground, background, attributes,
text. Colours are printed as palette indices, not as resolved RGB, so a
difference reads as a difference of fact rather than of representation.

`conform.py` plays the same script on a reference terminal, reads its screen
back, normalizes it into exactly that form, and diffs.

## Provenance

The reference is driven entirely through its own **documented command-line
interface**, and its screen is read the way any user may read it. No reference
implementation's source is consulted — that is binding, and it is why this is
built as a black-box instrument rather than as a comparison of code. See the
provenance rule in the engineering standard.

## A reference is evidence, not scripture

A reference terminal can be wrong, or can have made a different choice on
purpose. When ckVision and the reference disagree, the disagreement is
settled by documentation, not by assuming either side is right:

- **terminfo** says what a program running under a given `TERM` is entitled
  to assume. `infocmp -1 xterm-256color | grep bce` is how the
  background-colour-erase question was settled: `xterm-256color` declares
  `bce` ("screen erased with background color") and the reference's own entry
  does not. ckVision advertises `xterm-256color` to its children, so it owes
  them `bce` behaviour whatever the reference does.
- **Published standards** — ECMA-48, the xterm control-sequence
  documentation, the kitty protocol specs — are the authority on what a
  sequence means.
- **The reference's own consistency** is evidence in itself. It applies the
  selected background on `EL`, `ECH`, `ICH`, `DCH`, `IL`, `DL` and scrolling,
  and not on `ED` below the cursor row. An emulator that had decided against
  background-colour erase would be consistent about it.

Cases where ckVision deliberately differs are listed in `DEVIATIONS`, each
with its reason in `DEVIATION_REASONS`. Those report as `note`, not as
failures. **Any difference that is not on that list fails the run** — which is
what makes this a gate rather than a report.

## Adding a case

Put one decision in each script. A case that exercises three rules at once
cannot say which of them a difference came from. Give it a screen only as big
as it needs, so the dump stays readable.

```python
"el_after_bg": (20, 2, "\033[42m\033[1;1H\033[K\033[1;3HPID"),
```

That one is htop's table header reduced to its essentials: select a colour,
home the cursor, erase the line, then write text a couple of columns in —
which is the order ncurses emits, and the reason a terminal without
background-colour erase shows the bar only underneath the letters.

If a new case fails, that is the harness working. Decide with evidence which
side is right, then either fix the emulator or add the case to `DEVIATIONS`
with the documentation that justifies it.

## When no reference is installed

The script exits 0 with a note on stderr. A conformance check nobody can run
is worse than one that says plainly it did not run.
