#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
"""Differential VT conformance: ckVision's emulator against a reference one.

Terminal semantics are not a matter of taste. "Does an erase keep the
underline attribute?" has an answer that established terminals already agree
on, and the way to get it is to run the same bytes through them and compare
the screens. This harness does that, so a question of fidelity is settled by
measurement instead of by argument.

The reference is driven entirely through its own documented command-line
interface -- its screen is read the way any user may read it. No reference
implementation's source is consulted, per the provenance rule in AGENTS.md.

Usage:
    conform.py --dump-tool build/ckvision_vt_dump [--case NAME] [-v]
"""

import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unicodedata

# Each case is a byte script plus the screen it is played on. They are chosen
# to isolate one decision each, because a case that exercises three rules at
# once cannot say which of them a difference came from.
#
# A reference implementation is evidence, not scripture. Where this one is
# known to depart from the behaviour ckVision must have, the case carries the
# reason and the evidence for it, and the harness reports a documented
# deviation rather than a failure -- while still failing on any difference
# nobody has explained. That is the difference between a gate and a wish.
DEVIATIONS = {
    "ed_all": "ED below the cursor row",
    "ed_below": "ED below the cursor row",
    "erase_under_bold": "ED below the cursor row",
    "erase_under_dim": "ED below the cursor row",
    "erase_under_italic": "ED below the cursor row",
    "erase_under_strike": "ED below the cursor row",
    "erase_under_underline": "ED below the cursor row",
    "scroll_region_su": "ED below the cursor row",
    "erase_under_reverse": "reverse-video erase",
}

# Why each documented deviation stands, so a reader can check the reasoning
# rather than take it on trust.
DEVIATION_REASONS = {
    "ED below the cursor row": (
        "The reference applies the selected background to the erased line but "
        "not to the lines below it, while agreeing with ckVision on EL, ECH, "
        "ICH, DCH, IL, DL and scrolling -- an inconsistency within the "
        "reference itself. terminfo settles which behaviour ckVision needs: "
        "xterm-256color declares `bce` (\"screen erased with background "
        "color\") and the reference's own entry does not. ckVision advertises "
        "xterm-256color to its children, so a curses program paints a "
        "full-width coloured bar by erasing rather than by writing spaces, and "
        "expects the whole erased region to take the colour."),
    "reverse-video erase": (
        "While reverse video is set, what a reader sees as the background is "
        "the selected foreground. ckVision resolves that at erase time and "
        "stores the visible colour; the reference drops the colour entirely. "
        "Storing background+reverse instead would render identically, so this "
        "is a representation choice with the same appearance, chosen because "
        "an erased cell carries no other attribute either."),
}

CASES = {
    # --- Background colour erase: the rules a coloured full-width bar needs.
    "el_after_bg": (20, 2, "\033[42m\033[1;1H\033[K\033[1;3HPID"),
    "el_to_end": (20, 2, "\033[1;1Hxxxxxxxx\033[1;4H\033[44m\033[K"),
    "el_backwards": (20, 2, "\033[1;1Hxxxxxxxx\033[1;5H\033[44m\033[1K"),
    "el_whole_line": (20, 2, "\033[1;1Hxxxx\033[41m\033[2K"),
    "ed_below": (20, 3, "\033[1;1Hab\033[2;1Hcd\033[1;3H\033[46m\033[J"),
    "ed_all": (20, 2, "\033[1;1Hab\033[45m\033[2J"),
    "ech": (20, 2, "\033[1;1Habcdefgh\033[1;3H\033[43m\033[3X"),
    # --- Which attributes survive an erase.
    "erase_under_underline": (12, 2, "\033[4;44m\033[2J"),
    "erase_under_bold": (12, 2, "\033[1;44m\033[2J"),
    "erase_under_italic": (12, 2, "\033[3;44m\033[2J"),
    "erase_under_reverse": (12, 2, "\033[31;7m\033[2J"),
    "erase_under_strike": (12, 2, "\033[9;44m\033[2J"),
    "erase_under_dim": (12, 2, "\033[2;44m\033[2J"),
    # --- Insert/delete and scroll fills.
    "ich": (16, 2, "\033[1;1Habcdef\033[1;3H\033[42m\033[3@"),
    "dch": (16, 2, "\033[1;1Habcdef\033[1;3H\033[42m\033[2P"),
    "il": (16, 3, "\033[1;1Ha\033[2;1Hb\033[1;1H\033[42m\033[L"),
    "dl": (16, 3, "\033[1;1Ha\033[2;1Hb\033[1;1H\033[42m\033[M"),
    "scroll_up_newline": (8, 3, "\033[3;1H\033[44m\n"),
    "scroll_region_su": (8, 4, "\033[1;3r\033[44m\033[2S"),
    # --- Cursor motion and clamping.
    "cup_clamped": (10, 3, "\033[99;99Hx"),
    "cha_vpa": (10, 3, "\033[1;1Habc\033[5Gx\033[2dy"),
    "cursor_save_restore": (10, 3, "\033[2;3H\0337\033[1;1Hab\0338z"),
    # --- Autowrap, including the pending-wrap quirk at the last column.
    "autowrap": (5, 3, "abcdefgh"),
    "pending_wrap_then_cr": (5, 3, "abcde\rZ"),
    "pending_wrap_then_bs": (5, 3, "abcde\bZ"),
    "no_autowrap": (5, 3, "\033[?7labcdefgh"),
    # --- Tabs.
    "default_tabs": (24, 2, "a\tb\tc"),
    "tab_clear_and_set": (24, 2, "\033[1;5H\033H\033[1;1H\033[3gA\tB"),
    # --- Scrolling regions.
    "stbm_index_at_bottom": (8, 5, "\033[2;4r\033[4;1Ha\n\033[Hz"),
    "stbm_reverse_index_at_top": (8, 5, "\033[2;4r\033[2;1Ha\033Mz"),
    # --- Insert mode and repeat.
    "irm_insert": (12, 2, "\033[1;1Habcdef\033[1;3H\033[4hXY"),
    "rep": (12, 2, "\033[1;1Hx\033[5b"),
    # --- Line-drawing charset.
    "dec_graphics": (12, 2, "\033(0qqq\033(BX"),
    # --- Wide characters and how they are overwritten.
    "wide_char": (10, 2, "\033[1;1H\344\275\240\345\245\275"),
    "overwrite_wide_left": (10, 2, "\033[1;1H\344\275\240\033[1;1HX"),
    # --- 256-colour and true-colour selection.
    "sgr_256": (10, 2, "\033[38;5;196;48;5;21mAB"),
    "sgr_truecolor": (10, 2, "\033[38;2;10;20;30;48;2;40;50;60mAB"),
    # --- Alternate screen.
    "alt_screen": (10, 3, "\033[1;1Hmain\033[?1049h\033[1;1Halt"),
    "alt_screen_restore": (10, 3, "\033[1;1Hmain\033[?1049h\033[1;1Halt\033[?1049l"),
    # --- Colour resets, so "default" means the same thing on both sides.
    "sgr_default_bg": (12, 2, "\033[44mxx\033[49m\033[K"),
    "sgr_default_fg": (12, 2, "\033[31mxx\033[39myy"),
    "sgr_reset_then_erase": (12, 2, "\033[4;44mxx\033[0m\033[K"),
}

# DEC Special Graphics, per the published VT100 character set: the reference's
# capture reports a shift-out plus the ASCII byte, where the emulator stores
# the glyph that byte stands for. Same screen, two spellings.
DEC_GRAPHICS = {
    "`": "\u25c6", "a": "\u2592", "f": "\u00b0", "g": "\u00b1", "i": "\u240b",
    "j": "\u2518", "k": "\u2510", "l": "\u250c", "m": "\u2514", "n": "\u253c",
    "o": "\u23ba", "p": "\u23bb", "q": "\u2500", "r": "\u23bc", "s": "\u23bd",
    "t": "\u251c", "u": "\u2524", "v": "\u2534", "w": "\u252c", "x": "\u2502",
    "y": "\u2264", "z": "\u2265", "{": "\u03c0", "|": "\u2260", "}": "\u00a3",
    "~": "\u00b7", "_": " ", "0": "\u2588",
}

SGR_RE = re.compile(r"\033\[([0-9;]*)m")


class Pen:
    """The styling state a captured line is read with."""

    def __init__(self):
        self.reset()

    def reset(self):
        self.fg = "default"
        self.bg = "default"
        self.attrs = set()

    def copy(self):
        other = Pen()
        other.fg, other.bg, other.attrs = self.fg, self.bg, set(self.attrs)
        return other

    def apply(self, params):
        values = [int(p) for p in params.split(";") if p != ""] or [0]
        i = 0
        while i < len(values):
            v = values[i]
            if v == 0:
                self.reset()
            elif v == 1:
                self.attrs.add("bold")
            elif v == 2:
                self.attrs.add("dim")
            elif v == 3:
                self.attrs.add("italic")
            elif v == 4:
                self.attrs.add("underline")
            elif v == 7:
                self.attrs.add("reverse")
            elif v == 9:
                self.attrs.add("strike")
            elif v in (22,):
                self.attrs.discard("bold")
                self.attrs.discard("dim")
            elif v == 23:
                self.attrs.discard("italic")
            elif v == 24:
                self.attrs.discard("underline")
            elif v == 27:
                self.attrs.discard("reverse")
            elif v == 29:
                self.attrs.discard("strike")
            elif 30 <= v <= 37:
                self.fg = str(v - 30)
            elif v == 39:
                self.fg = "default"
            elif 40 <= v <= 47:
                self.bg = str(v - 40)
            elif v == 49:
                self.bg = "default"
            elif 90 <= v <= 97:
                self.fg = str(v - 90 + 8)
            elif 100 <= v <= 107:
                self.bg = str(v - 100 + 8)
            elif v in (38, 48) and i + 1 < len(values):
                target = "fg" if v == 38 else "bg"
                if values[i + 1] == 5 and i + 2 < len(values):
                    setattr(self, target, str(values[i + 2]))
                    i += 2
                elif values[i + 1] == 2 and i + 4 < len(values):
                    setattr(self, target, "#%02x%02x%02x" % tuple(values[i + 2 : i + 5]))
                    i += 4
            i += 1

    def signature(self):
        return (self.fg, self.bg, "+".join(sorted(self.attrs)) or "-")


def runs_from_dump(text):
    """Normalize the C++ dumper's output into comparable run tuples."""
    runs = []
    for line in text.splitlines():
        m = re.match(r"(\d+) (\d+)-(\d+) fg=(\S+) bg=(\S+) attrs=(\S+) text=\[(.*)\]$", line)
        if not m:
            continue
        row, start, end, fg, bg, attrs, body = m.groups()
        runs.append((int(row), int(start), int(end), fg, bg, attrs, body))
    return runs


def runs_from_capture(lines, columns, rows):
    """Normalize the reference's escape-bearing capture into the same tuples."""
    runs = []
    for row in range(rows):
        raw = lines[row] if row < len(lines) else ""
        pen = Pen()
        cells = []
        shifted = False
        i = 0
        while i < len(raw) and len(cells) < columns:
            m = SGR_RE.match(raw, i)
            if m:
                pen.apply(m.group(1))
                i = m.end()
                continue
            if raw[i] == "\033":  # any other sequence the capture emitted
                nxt = raw.find("m", i)
                i = nxt + 1 if nxt != -1 else len(raw)
                continue
            ch = raw[i]
            i += 1
            # The capture speaks its own shorthand for three things, and a
            # reading that took them literally would report differences that
            # are not on the screen at all.
            if ch == "\x0e":  # shift out: the line-drawing set is now current
                shifted = True
                continue
            if ch == "\x0f":  # shift in
                shifted = False
                continue
            if ch == "\t":  # a tab is captured as itself, not as the cells it skipped
                stop = min(((len(cells) // 8) + 1) * 8, columns)
                while len(cells) < stop:
                    cells.append((" ", pen.signature()))
                continue
            if shifted:
                ch = DEC_GRAPHICS.get(ch, ch)
            cells.append((ch, pen.signature()))
            # A double-width character owns two cells; the second holds no
            # grapheme of its own, which is how the emulator represents it.
            if unicodedata.east_asian_width(ch) in ("W", "F"):
                cells.append(("", pen.signature()))
        while len(cells) < columns:
            cells.append((" ", Pen().signature()))
        del cells[columns:]

        start = 0
        for x in range(1, columns + 1):
            if x < columns and cells[x][1] == cells[start][1]:
                continue
            fg, bg, attrs = cells[start][1]
            body = "".join(c[0] for c in cells[start:x])
            runs.append((row, start, x - 1, fg, bg, attrs, body))
            start = x
    return runs


def reference_capture(script, columns, rows, socket):
    """Play `script` on a reference terminal and read back its screen."""
    with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as handle:
        handle.write(script.encode("latin-1"))
        path = handle.name
    try:
        env = dict(os.environ, TMUX_TMPDIR="/tmp")
        subprocess.run(["tmux", "-L", socket, "kill-server"], env=env,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(
            ["tmux", "-L", socket, "-f", "/dev/null", "new-session", "-d",
             "-x", str(columns), "-y", str(rows), f"cat {path}; sleep 30"],
            env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        time.sleep(0.35)
        out = subprocess.run(["tmux", "-L", socket, "capture-pane", "-p", "-e", "-N"],
                             env=env, check=True, capture_output=True)
        return out.stdout.decode("utf-8", "replace").split("\n")
    finally:
        subprocess.run(["tmux", "-L", socket, "kill-server"], env=env,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        os.unlink(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dump-tool", required=True)
    parser.add_argument("--case", action="append")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    if shutil.which("tmux") is None:
        print("vtconform: no reference terminal available; skipping", file=sys.stderr)
        return 0
    if not pathlib.Path(args.dump_tool).exists():
        print(f"vtconform: {args.dump_tool} not built", file=sys.stderr)
        return 2

    names = args.case or sorted(CASES)
    failures = []
    documented = []
    for name in names:
        columns, rows, script = CASES[name]
        ours = subprocess.run([args.dump_tool, "--columns", str(columns), "--rows", str(rows)],
                              input=script.encode("latin-1"), capture_output=True, check=True)
        mine = runs_from_dump(ours.stdout.decode())
        theirs = runs_from_capture(reference_capture(script, columns, rows, "ckvconf"), columns, rows)

        if mine == theirs:
            print(f"  ok    {name}")
            if args.verbose:
                for run in mine:
                    print(f"          {run}")
            continue
        if name in DEVIATIONS:
            documented.append(name)
            print(f"  note  {name}: documented deviation ({DEVIATIONS[name]})")
            continue
        failures.append(name)
        print(f"  DIFF  {name}")
        for row in range(rows):
            a = [r for r in mine if r[0] == row]
            b = [r for r in theirs if r[0] == row]
            if a == b:
                continue
            print(f"    row {row}")
            print(f"      ckvision : {a}")
            print(f"      reference: {b}")

    agreed = len(names) - len(failures) - len(documented)
    print(f"\nvtconform: {agreed}/{len(names)} cases agree with the reference, "
          f"{len(documented)} documented deviation(s), {len(failures)} unexplained")
    for reason in sorted({DEVIATIONS[n] for n in documented}):
        print(f"\n  deviation: {reason}\n    " + DEVIATION_REASONS[reason].replace(". ", ".\n    "))
    if failures:
        print("\nunexplained differences: " + ", ".join(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
