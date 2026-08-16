# Terminal capabilities: what ckVision supports, and what it depends on

Status: current as of 2026-08-16 · verified against the source, not from memory

There are **two different terminals** in play whenever ckVision runs, and
almost every confusing capability question comes from mixing them up:

| | The **inner** terminal | The **outer** terminal |
|---|---|---|
| What it is | `term::TerminalEmulator` — ckVision's own emulator, which a child program runs inside | The real terminal ckVision itself is running in (iTerm2, Terminal.app, WezTerm, xterm, …) |
| Who talks to it | The child program (a shell, vim, htop) | ckVision's `term::Presenter` and `term::InputDecoder` |
| Who decides what it can do | **We do.** Every sequence in the table below is ours to implement | **The host does.** We can only detect what it offers and degrade honestly |
| Where it is configured | `TerminalCapabilityProfile` | `term::Capabilities` (probed, with `CapabilityOverrides`) |

So a capability falls into one of four cases, and this document marks every
entry with which one applies:

- **E — Emulated.** Works regardless of the host, because we implement it
  ourselves in the inner terminal. A child program gets it always.
- **H — Host-dependent.** Only works if the outer terminal supports it. We
  detect, and degrade to something honest when it is missing.
- **E+H — Emulated, shown host-dependently.** We understand it from the child
  and hold the result, but how much of it reaches the screen depends on the
  host (Sixel is the clearest case: decoded always, displayed only where the
  host can).
- **N — Not supported.** Parsed and ignored without corrupting the stream, or
  not recognised at all. Listed so that "unsupported" is a statement rather
  than a silence.

---

## Inner terminal: what a child program can use

### Cursor movement and positioning — all E

| Sequence | Name | Notes |
|---|---|---|
| `CSI A/B/C/D` | CUU/CUD/CUF/CUB | clamped to the screen |
| `CSI E/F` | CNL/CPL | |
| `CSI G`, `` CSI ` `` | CHA/HPA | |
| `CSI d` | VPA | |
| `CSI H`, `CSI f` | CUP/HVP | honours origin mode |
| `ESC 7` / `ESC 8`, `CSI s` / `CSI u` | DECSC/DECRC | cursor + attributes |
| `ESC D` / `ESC M` / `ESC E` | IND/RI/NEL | respects the scrolling region |

### Editing and erasing — all E

| Sequence | Name | Notes |
|---|---|---|
| `CSI J` | ED (0/1/2/3) | 3 clears scrollback; **fills with the selected background (`bce`)** |
| `CSI K` | EL (0/1/2) | as above |
| `CSI X` | ECH | as above |
| `CSI @` / `CSI P` | ICH/DCH | fills with the selected background |
| `CSI L` / `CSI M` | IL/DL | as above |
| `CSI S` / `CSI T` | SU/SD | as above |
| `CSI b` | REP | repeats the last graphic character |
| `CSI r` | DECSTBM | scrolling region |

An erased cell keeps **only the background**. Bold, italic, underline and
strike describe a glyph that is no longer there; underline in particular
would draw a rule across every cleared line. Reverse video is resolved rather
than dropped: while it is set, the visible background is the selected
foreground, so that is what the cell keeps. Both points were **measured**
against a reference terminal rather than decided — see
`tools/vtconform/README.md`.

### Modes

| Sequence | Name | Case | Notes |
|---|---|---|---|
| `CSI ? 1 h/l` | DECCKM application cursor keys | E | changes what arrow keys send |
| `CSI ? 6 h/l` | DECOM origin mode | E | |
| `CSI ? 7 h/l` | DECAWM autowrap | E | off means the last column is overwritten |
| `CSI ? 25 h/l` | DECTCEM cursor visibility | E | |
| `CSI 4 h/l` | IRM insert mode | E | the only ANSI (non-`?`) mode we implement |
| `CSI ? 47/1047/1048/1049 h/l` | alternate screen | E | 1049 saves and restores the cursor |
| `CSI ? 1000/1002/1003 h/l` | mouse reporting | E+H | we track it; the *host* must actually report mouse events |
| `CSI ? 1006 h/l` | SGR mouse encoding | E+H | as above |
| `CSI ? 1004 h/l` | focus reporting | E+H | host must report focus |
| `CSI ? 2004 h/l` | bracketed paste | E | |
| `CSI ? 1007` | alternate scroll | **N** | wheel-to-arrows on the alternate screen is not tracked |
| `CSI ? 2026` | synchronized output | **N** (inner) / H (outer) | we *use* it against the host; a child asking for it is ignored |
| `CSI ? 1016` | pixel mouse | H | outer-terminal capability, negotiated by the host layer |

### Text rendering — SGR

| Sequence | Case | Notes |
|---|---|---|
| `SGR 0/1/2/3/4/7/9` and `22/23/24/27/29` | E | bold, dim, italic, underline, reverse, strike and their resets |
| `SGR 30-37/39/40-47/49`, `90-97/100-107` | E | 16 colours, defaults, bright |
| `SGR 38;5;n` / `48;5;n` | E | 256 colours |
| `SGR 38;2;r;g;b` / `48;2;r;g;b` | E | true colour |
| Bold brightening of the low 8 colours | E | a program asking for bold on a base colour gets the bright one, as terminals do |
| `SGR 4:3` (undercurl) and `58` (underline colour) | **N** | nvim uses these; they degrade to a plain underline |
| `SGR 21` (double underline), `53` (overline) | **N** | |

Colour is stored **resolved to RGB**, so a child's indexed colour cannot be
re-themed downstream. That is a deliberate simplification, not an oversight,
and it is the one thing blocking per-terminal palette re-theming.

### Character sets

| Sequence | Case | Notes |
|---|---|---|
| `ESC ( 0` / `ESC ( B` | E | DEC Special Graphics — the line-drawing set ncurses uses for boxes |
| Other NRCS sets | **N** | mapped to ASCII |

### Reports the child can ask for — all E

| Sequence | Name | Answer |
|---|---|---|
| `CSI c` | DA1 | reflects the capability profile, so a child that probes gets the truth |
| `CSI n` (5/6) | DSR | status and cursor position, origin-mode aware |
| `CSI 14 t` / `CSI 16 t` | XTWINOPS | pixel geometry, which Sixel-emitting programs ask for |
| `CSI SP q` | DECSCUSR | cursor shape |

### Graphics and metadata

| Sequence | Case | Notes |
|---|---|---|
| `DCS q … ST` | **E+H** | Sixel is **decoded always**, into a scene raster. It reaches the screen as pixels where the host supports Sixel, and as a text placeholder where it does not — never as leaked escape bytes |
| `OSC 0` / `OSC 2` | E | window title, sanitised and length-bounded; policy-gated (`TerminalOscPolicy`) |
| `OSC 1` | E | icon name — deliberately does not change the title |
| `OSC 52` (clipboard) | **N** | a child cannot set the clipboard; opt-in policy is planned |
| `OSC 4/10/11` (palette query) | **N** | vim probes `OSC 11` for the background; it currently gets no answer |
| `OSC 8` (hyperlinks) | **N** | |
| Kitty graphics | **N** | Sixel is the one image path |
| Kitty keyboard (child side) | **N** | the legacy encoding is a correct fallback |

### Deliberately never supported

Printer control (`MC`) — a child must not be able to write to a printer.
Tektronix and ReGIS graphics. DRCS soft fonts. Answerback (`ENQ` returns
empty, never a fingerprint). Unknown `APC`/`PM`/`DCS` strings are parsed to
completion and dropped, so an unrecognised sequence can never desynchronise
the parser.

---

## Outer terminal: what ckVision needs from the host

These are **H** by definition — detected through `term::Capabilities`, with
`CapabilityOverrides` for when a host lies or a user knows better. Every one
degrades rather than failing.

| Capability | If the host has it | If it does not |
|---|---|---|
| `color_depth` (TrueColor / 256 / Mono16) | exact colours | `Presenter` quantises down; Mono keeps meaning through attributes |
| `sixel_graphics` | images drawn as pixels | text fallback cells; **zero raster bytes emitted** |
| `keyboard_protocol` (Kitty / modifyOtherKeys / legacy) | precise key disambiguation | legacy encoding, which is always correct if coarser |
| `mouse_protocol` (SGR / X10 / none) | full mouse | keyboard-only operation stays complete |
| `pixel_mouse` (SGR-Pixels 1016) | pixel-precise positions inside images | cell precision |
| `synchronized_output` (2026) | tear-free frames | frames may tear on a slow host |
| `clipboard_write` (OSC 52) | copy reaches the system clipboard | internal clipboard still works |
| `bracketed_paste` (2004) | paste is distinguishable from typing | paste arrives as keys |
| `focus_events` (1004) | focus in/out known | assumed focused |
| `hyperlinks` (OSC 8) | clickable links | plain text |
| `color_scheme` + notifications (2031) | follows the host's light/dark | fixed theme |
| `cell_pixels` / `text_area_pixels` | correct image placement | assumes a cell, rather than dropping the image |
| `ambiguous_width_is_wide` | agrees with the host on ambiguous glyphs | D-019 absolute cursor re-addressing keeps a disagreement from shifting later cells |

---

## How this document stays true

By measurement, not by assertion. `tools/vtconform/` plays the same byte
scripts through this emulator and through an established terminal and
compares the resulting cells. It currently runs 42 cases; every difference is
either fixed or listed with the documentation that justifies it, and an
unexplained difference fails the build (`ctest -R vt_conformance`).

That harness is how three bugs in this table's own subject matter were found
rather than argued about: IRM insert mode was missing entirely (ANSI modes
were not handled at all), DECAWM could not be turned off, and overwriting
half of a double-width character left the other half orphaned.

## Gaps worth closing next, in order of what a real program will notice

1. **`OSC 11` background query** — vim and nvim probe it to choose a colour
   scheme, and silence makes them guess wrong.
2. **`CSI ? 1007` alternate scroll** — wheel scrolling in `less` and `vim`.
3. **`SGR 4:3` / `58`** — nvim's undercurl and its colour.
4. **Child `OSC 52`**, behind an opt-in policy and a size cap.
5. **Indexed colour preserved through the emulator**, which is what a
   palette-re-theming feature would need.
6. **Kitty keyboard protocol, child side** — progressive enhancement; the
   legacy encoding remains correct until then.
