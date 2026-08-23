#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Differential VT conformance: ckVision's emulator against a reference one.

Terminal semantics are not a matter of taste. "Does an erase keep the
underline attribute?" has an answer that established terminals already agree
on, and the way to get it is to run the same bytes through them and compare
the screens. This harness does that, so a question of fidelity is settled by
measurement instead of by argument.

The reference is driven entirely through its own documented command-line
interface -- its screen is read the way any user may read it. No reference
implementation's source is consulted, per the provenance rule in the engineering standard.

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


class HarnessError(RuntimeError):
    """The reference terminal misbehaved, so this case measured nothing.

    Kept distinct from a conformance difference on purpose: an instrument
    that failed to take a reading must never be reported as agreement, nor
    as a fault in the emulator it was pointed at.
    """

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
    "tab_clear_and_set": "no tab-stop table",
    "tab_past_last_stop": "tab at the end of a line",
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
    "no tab-stop table": (
        "The reference has no tab stops, only a division by eight. Measured "
        "rather than assumed: with every stop cleared (CSI 3 g) and one set "
        "at column 13 (ESC H), `A\\tB\\tC` puts B at 8 and C at 16 in the "
        "reference -- it neither cleared the defaults nor honoured the new "
        "stop. ckVision keeps a real table, so B lands on 13. This is the one "
        "case where the reference is not evidence of anything except its own "
        "limits: HTS and TBC are how a program lays out columns once instead "
        "of padding every row with spaces, xterm implements them, and a "
        "curses program that sets stops and then tabs between them gets every "
        "field after the first in the wrong place without them."),
    "tab at the end of a line": (
        "Past the last stop, ckVision moves the cursor to the last column and "
        "leaves it there; the reference carries it past the margin, so the "
        "next character wraps to the following line. A tab does not end a "
        "line -- the character written after it is what decides that -- and "
        "the DEC and xterm behaviour ckVision advertises to its children "
        "(xterm-256color) is to stop at the right margin. The case is here "
        "because that is a decision, not an accident: it changed with the "
        "tab-stop table, and a decision nobody wrote down is one the next "
        "reader has to rediscover."),
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
    "tab_past_last_stop": (24, 2, "\033[1;20HA\tB"),
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
    # --- Underline shapes and the underline's own colour.
    "sgr_underline_curly": (12, 2, "\033[4:3mAB"),
    "sgr_underline_dashed": (12, 2, "\033[4:5mAB"),
    "sgr_underline_off": (12, 2, "\033[4:3mAB\033[4:0mCD"),
    "sgr_underline_21_is_double": (12, 2, "\033[21mAB"),
    "sgr_underline_colour": (12, 2, "\033[4:3;58;5;9mAB"),
    "sgr_underline_colour_subparams": (12, 2, "\033[4:3;58:2::10:20:30mAB"),
    "sgr_underline_colour_reset": (12, 2, "\033[4;58;5;9mAB\033[59mCD"),
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

# SGR is the one control that carries colon-separated sub-parameters: the
# shape of an underline, and an underline colour written as one parameter.
SGR_RE = re.compile(r"\033\[([0-9;:]*)m")


class Pen:
    """The styling state a captured line is read with."""

    def __init__(self):
        self.reset()

    def reset(self):
        self.fg = "default"
        self.bg = "default"
        self.attrs = set()
        # An underline that is not being drawn has no shape and no colour of
        # its own, so both are spelled by their absence.
        self.underline = None
        self.ul_color = "default"

    def copy(self):
        other = Pen()
        other.fg, other.bg, other.attrs = self.fg, self.bg, set(self.attrs)
        other.underline, other.ul_color = self.underline, self.ul_color
        return other

    UNDERLINE_SHAPES = {1: "", 2: "double", 3: "curly", 4: "dotted", 5: "dashed"}

    def apply(self, params):
        # Each parameter is a head value and its colon-separated tail. An
        # empty field is zero, which is how `58:2::R:G:B` spells "no colour
        # space named".
        groups = [[int(x) if x != "" else 0 for x in g.split(":")]
                  for g in params.split(";")] or [[0]]
        i = 0
        while i < len(groups):
            v, subs = groups[i][0], groups[i][1:]
            if v == 0:
                self.reset()
            elif v == 1:
                self.attrs.add("bold")
            elif v == 2:
                self.attrs.add("dim")
            elif v == 3:
                self.attrs.add("italic")
            elif v == 4:
                shape = subs[0] if subs else 1
                if shape == 0:
                    self.clear_underline()
                else:
                    self.underline = self.UNDERLINE_SHAPES.get(shape, "")
            elif v == 7:
                self.attrs.add("reverse")
            elif v == 9:
                self.attrs.add("strike")
            elif v == 21:
                self.underline = "double"
            elif v in (22,):
                self.attrs.discard("bold")
                self.attrs.discard("dim")
            elif v == 23:
                self.attrs.discard("italic")
            elif v == 24:
                self.clear_underline()
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
            elif v == 59:
                self.ul_color = "default"
            elif 90 <= v <= 97:
                self.fg = str(v - 90 + 8)
            elif 100 <= v <= 107:
                self.bg = str(v - 100 + 8)
            elif v in (38, 48, 58):
                target = {38: "fg", 48: "bg", 58: "ul_color"}[v]
                if subs:
                    # The whole colour as one parameter's sub-parameters.
                    color, consumed = self.color_from(subs), 0
                else:
                    color, consumed = self.color_from([g[0] for g in groups[i + 1:]]), 0
                    if color is not None:
                        consumed = 2 if groups[i + 1][0] == 5 else 4
                if color is not None:
                    setattr(self, target, color)
                    i += consumed
            i += 1

    @staticmethod
    def color_from(values):
        """A colour from `5, index` or `2, [space,] r, g, b`, in either spelling."""
        if not values:
            return None
        if values[0] == 5 and len(values) >= 2:
            return str(values[1])
        if values[0] == 2:
            channels = values[2:5] if len(values) >= 5 else values[1:4]
            if len(channels) == 3:
                return "#%02x%02x%02x" % tuple(channels)
        return None

    def clear_underline(self):
        self.underline = None
        self.ul_color = "default"

    def signature(self):
        names = set(self.attrs)
        if self.underline is not None:
            name = "underline" + (":" + self.underline if self.underline else "")
            if self.ul_color != "default":
                name += "@" + self.ul_color
            names.add(name)
        return (self.fg, self.bg, "+".join(sorted(names)) or "-")


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
        # The pane runs `cat <script>; sleep 30`, so the foreground command
        # flips from `cat` to `sleep` exactly when the last script byte has
        # been written. Waiting for that beats sleeping a guessed interval,
        # which reads a half-drawn screen whenever the machine is loaded.
        deadline = time.monotonic() + 10.0
        while True:
            probe = subprocess.run(
                ["tmux", "-L", socket, "display-message", "-p", "#{pane_current_command}"],
                env=env, capture_output=True)
            if probe.returncode == 0 and probe.stdout.decode().strip() == "sleep":
                break
            if time.monotonic() > deadline:
                raise HarnessError(
                    "reference terminal never finished playing the script "
                    f"(last probe: rc={probe.returncode} "
                    f"{probe.stderr.decode('utf-8', 'replace').strip()!r})")
            time.sleep(0.02)
        # `cat` exiting proves the bytes were written, not that tmux has
        # parsed them — it drains the pty on its own schedule. Capture until
        # two consecutive reads agree, which observes the screen settling
        # instead of assuming an interval long enough to have covered it.
        previous = None
        while True:
            out = subprocess.run(["tmux", "-L", socket, "capture-pane", "-p", "-e", "-N"],
                                 env=env, capture_output=True)
            if out.returncode != 0:
                raise HarnessError("reference terminal could not be captured: "
                                   f"{out.stderr.decode('utf-8', 'replace').strip()}")
            current = out.stdout.decode("utf-8", "replace")
            if current == previous:
                return current.split("\n")
            if time.monotonic() > deadline:
                raise HarnessError("reference terminal screen never stopped changing")
            previous = current
            time.sleep(0.02)
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

    # Private to this process. A fixed socket name lets two concurrent runs
    # -- a developer's suite overlapping CI's, say -- kill each other's
    # reference server mid-capture, which reads as an emulator fault.
    socket = f"ckvconf{os.getpid()}"
    names = args.case or sorted(CASES)
    failures = []
    documented = []
    unmeasured = []
    for name in names:
        columns, rows, script = CASES[name]
        ours = subprocess.run([args.dump_tool, "--columns", str(columns), "--rows", str(rows)],
                              input=script.encode("latin-1"), capture_output=True, check=True)
        mine = runs_from_dump(ours.stdout.decode())
        # One retry, because the reference terminal is a separate process
        # competing for a loaded machine. Two failures in a row is the
        # instrument being broken rather than busy, and is reported as such.
        for attempt in range(2):
            try:
                theirs = runs_from_capture(reference_capture(script, columns, rows, socket),
                                           columns, rows)
                break
            except HarnessError as error:
                if attempt == 0:
                    continue
                unmeasured.append(name)
                print(f"  ERROR {name}: {error}")
                theirs = None
        if theirs is None:
            continue

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

    agreed = len(names) - len(failures) - len(documented) - len(unmeasured)
    print(f"\nvtconform: {agreed}/{len(names)} cases agree with the reference, "
          f"{len(documented)} documented deviation(s), {len(failures)} unexplained"
          + (f", {len(unmeasured)} unmeasured" if unmeasured else ""))
    for reason in sorted({DEVIATIONS[n] for n in documented}):
        print(f"\n  deviation: {reason}\n    " + DEVIATION_REASONS[reason].replace(". ", ".\n    "))
    if failures:
        print("\nunexplained differences: " + ", ".join(failures))
    if unmeasured:
        print("\nthe reference terminal failed to produce a reading for: "
              + ", ".join(unmeasured) + "\nthis says nothing about ckVision; "
              "it means the harness could not measure.")
    return 1 if failures or unmeasured else 0


if __name__ == "__main__":
    sys.exit(main())
