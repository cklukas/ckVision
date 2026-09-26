# Terminal capabilities: what ckVision supports, and what it depends on

Status: current as of 2026-08-23 · verified against the source, not from memory

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
| `CSI ? J` / `CSI ? K` | DECSED/DECSEL selective erase | the ordinary erase through the same code: DECSCA is not implemented, so nothing is ever protected |
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
| `CSI ? 1000/1002/1003 h/l` | mouse reporting | E+H | three levels of one setting, told apart (`TerminalMouseTracking`, D-054): presses/releases, plus drags, plus free motion. The *host* must actually report mouse events |
| `CSI ? 1006 h/l` | SGR mouse encoding | E+H | as above |
| `CSI ? 1004 h/l` | focus reporting | E+H | host must report focus |
| `CSI ? 2004 h/l` | bracketed paste | E | |
| `CSI ? 1007 h/l` | alternate scroll | E+H | on unless the child says otherwise; the wheel is the *host's* to report |
| `ESC =` / `ESC >`, `CSI ? 66 h/l` | DECPAM/DECPNM/DECNKM application keypad | **N** | consumed, no effect, **no diagnostic** — the key model has no keypad identity to switch, and every curses program sends these (D-053) |
| `CSI > Pp ; Pv m`, `CSI > Pp ; Pv f` | XTMODKEYS / XTFMTKEYS key-modifier options — modifyOtherKeys and its formats | **N** | consumed, no effect, **no diagnostic** (D-062): the cursor and function keys already travel in xterm's level-2 forms, and modifyOtherKeys' `CSI 27 ; mod ; code ~` encoding of an ordinary key is not implemented. Vim, neovim, emacs, tmux and Claude Code set this on the way in and reset it on the way out; a child that wants to know whether it took asks (`CSI ? Pp m`, below) and is told |
| `CSI ? Pm s` / `CSI ? Pm r` | XTSAVE / XTRESTORE DEC private mode save/restore | **N** | refused with a diagnostic — a program that saved a mode and restores it cannot tell from the bytes that it did not get it back |
| `CSI ? 2026` | synchronized output | **N** (inner) / H (outer) | we *use* it against the host; a child asking for it is ignored |
| `CSI ? 1016` | pixel mouse | H | outer-terminal capability, negotiated by the host layer |

### Text rendering — SGR

| Sequence | Case | Notes |
|---|---|---|
| `SGR 0/1/2/3/4/7/9` and `22/23/24/27/29` | E | bold, dim, italic, underline, reverse, strike and their resets |
| `SGR 30-37/39/40-47/49`, `90-97/100-107` | E | 16 colours, defaults, bright |
| `SGR 38;5;n` / `48;5;n` | E | 256 colours |
| `SGR 38;2;r;g;b` / `48;2;r;g;b` | E | true colour |
| `SGR 38:2::r:g:b`, `38:5:n` | E | the ISO 8613-6 sub-parameter spelling of the same two |
| Bold brightening of the low 8 colours | E | a program asking for bold on a base colour gets the bright one, as terminals do |
| `SGR 4:0` … `4:5` | E | no underline, straight, double, curly, dotted, dashed |
| `SGR 21` | E | double underline — measured against the reference, which reports it back as `4:2` |
| `SGR 58` / `59` | E+H | the underline's own colour, in both spellings; drawn where the host can draw it |
| `SGR 53` (overline) | **N** | |

Colour is stored **as what the program asked for**: a palette index stays an
index, a chosen RGB stays that RGB, and the terminal's own default stays the
default. This is what makes per-terminal palette re-theming possible at all —
"the palette's red" is a different fact from "this particular red", and only
the first can be re-themed later. `core/palette.hpp` is the one place that
turns an index into channels, for the things that genuinely need pixels: a
shadow, an SVG, or a host with fewer colours.

Two consequences worth knowing. A palette index reaches the outer terminal
**as an index**, so a reader's own theme applies to a child's `SGR 31` exactly
as it would outside ckmux. And bold brightening now applies to the low eight
palette entries only — a program's own `38;2;205;49;49` is a colour it chose,
and is left alone, where before it was indistinguishable from palette red.

The underline **shapes** are emulated always, and reach the screen where the
host declares `underline_styles`. Without it every shape degrades to the plain
rule: a terminal that has never heard of the colon form reads `4:3` as two
parameters and applies italics, so a shape is only ever sent to a host that
has said it can draw one.

### Character sets

| Sequence | Case | Notes |
|---|---|---|
| `ESC ( 0` / `ESC ( B` | E | DEC Special Graphics — the line-drawing set ncurses uses for boxes |
| Other NRCS sets | **N** | mapped to ASCII |

### Reports the child can ask for — all E

| Sequence | Name | Answer |
|---|---|---|
| `CSI c` | DA1 | reflects the capability profile, so a child that probes gets the truth — including **parameter 4 when the profile declares Sixel**, which is the advertisement `img2sixel`, `chafa` and gnuplot decide from |
| `CSI n` (5/6) | DSR | status and cursor position, origin-mode aware |
| `CSI 14 t` / `CSI 16 t` | XTWINOPS | pixel geometry, which Sixel-emitting programs ask for |
| `CSI ? Pi ; Pa ; Pv S` | XTSMGRAPHICS | the graphics limits behind that advertisement: the decoder's 256 colour registers, and a maximum geometry of the child's own window in pixels. Requests to *set* a limit, ReGIS geometry, and every request under a profile without Sixel are refused with a status — never with silence, which costs the asking program a timeout |
| `CSI SP q` | DECSCUSR | cursor shape |
| `CSI ? u` | kitty keyboard query | the enhancements actually in force — see below |
| `CSI ? Pp m` | XTQMODKEYS | the key-modifier resource as it really stands, in the XTMODKEYS set-form so the answer restores it: `2` for the cursor and function keys (they already travel as `CSI 1 ; mod A` / `CSI 1 ; mod P`), `0` for modifyOtherKeys, which is not implemented — a program that set it and asks is told it keeps the legacy encoding (D-062) |
| `CSI ? 6 n` | DECXCPR | the cursor position in the DEC form it was asked in, `CSI ? r ; c R`; the answer without its marker is CPR, which a program that asked this way does not read |

### Private markers are read first

A CSI whose parameters begin with `<`, `=`, `>` or `?` — ECMA-48's reserved
parameter bytes 03/12 to 03/15 — is a different control from the public one
that shares its final byte, and the emulator dispatches it by the marker
*before* the final byte decides anything (`handle_private_csi`). Every marked
form in the tables above lives there; a marked form not listed is refused with
a diagnostic, never read as the public control it resembles. That is the class
XTMODKEYS belonged to: decided by final byte alone, `CSI > 4 m` was a malformed
colour, `CSI ? 25 r` (XTRESTORE) set a scrolling region at row 25, and
`CSI > 2 T` (XTRMTITLE) scrolled the screen. Refused this way, by design:
DA2/DA3 and XTVERSION (`CSI > c`, `CSI = c`, `CSI > q`), which ask for an
identity and a firmware version where DA1 asks for capabilities — the same
decision as answerback; the title modes (`CSI > Pm t` / `T`); XTSMPOINTER
(`CSI > Ps p`).

### Graphics and metadata

| Sequence | Case | Notes |
|---|---|---|
| `DCS q … ST` | **E+H** | Sixel is **decoded always**, at the size of the picture (not of the window), into a scene raster. Writing over the cells a picture covers erases it there, as on a real terminal; pictures accumulate, scroll with their text, and go when the screen is cleared or the alternate buffer is entered. It reaches the screen as pixels where the host supports Sixel, and as a text placeholder where it does not — never as leaked escape bytes |
| `OSC 0` / `OSC 2` | E | window title, sanitised and length-bounded; policy-gated (`TerminalOscPolicy`) |
| `OSC 1` | E | icon name — deliberately does not change the title |
| `OSC 52` (clipboard write) | E | opt-in per profile (`TerminalClipboardPolicy`), size-capped, sanitised — see below |
| `OSC 52 ; … ; ?` (clipboard read) | **never** | refused under every policy (D-022): a program that can read the clipboard can read whatever its reader last copied |
| `OSC 4 ; n ; ?`, `OSC 10 ; ?`, `OSC 11 ; ?` | E | answered as `rgb:RRRR/GGGG/BBBB`, with the terminator the child asked with |
| `OSC 4/10/11` *set* | **N** | answering what a colour is and letting a child change it are different questions; a child that could set them could recolour its own window |
| `OSC 8` (hyperlinks) | **N** | |
| Kitty graphics | **N** | Sixel is the one image path |
| Kitty keyboard (child side) | E | the flag stack, per screen; the legacy encoding remains the default and the fallback |
| `CSI Ps i` / `CSI ? Ps i` (`MC`, Media Copy) | **N** | dropped with a diagnostic; **capture** under an opt-in policy is planned — see below |

### The clipboard a child can reach

`OSC 52` is the one thing a child can do that reaches outside its own window:
it replaces whatever the person at the keyboard had copied, without their
having asked. That is a feature when it is `yank` in an editor they are
driving and an attack when it is a file they happened to `cat`, and the
difference is not visible from inside the terminal. So it is opt-in per
profile (`TerminalClipboardPolicy::AllowWrite`, default `Deny`), bounded by
`max_clipboard_bytes`, decoded with a **strict** base64 reader, and sanitised
for the clipboard rather than for a cell — tab and line break are content in a
document, so they survive; every other control byte does not.

The emulator records the text and a count of granted requests;
`widgets::TerminalView` forwards it through `on_clipboard_write` when the
count moves. A snapshot is a value that may be read any number of times, so
the count — not the act of reading — is what makes an export happen once.

Reading is refused under **every** policy, including `AllowWrite`. There is no
setting that turns it on.

### The keyboard protocol a child can ask for

The legacy encoding is correct and stays the default. What it cannot do is say
some things at all: `Ctrl+I` and `Tab` are one byte, a lone `Esc` is
indistinguishable from the start of a sequence until a timer expires, and no
key ever reports being released. The kitty protocol's flags are how a program
asks for those, one at a time, and reads back what it actually got.

| Sequence | Effect |
|---|---|
| `CSI > flags u` | push the current flags, set these |
| `CSI < n u` | pop `n` (default 1); popping past the bottom returns to none |
| `CSI = flags ; mode u` | set (1), add (2) or remove (3) |
| `CSI ? u` | report the flags actually in force |

Two decisions worth stating. The stack is **per screen**, so a full-screen
program's settings do not follow it out onto the shell's screen — and a
program that dies without popping leaves nothing behind for the shell
underneath. And a requested flag is masked to what this terminal can really
deliver *before* it takes effect, so what a program reads back is what it will
really receive: `ReportAlternateKeys` is dropped, because ckVision's input
model carries the key and the text it produced, not the keyboard layout behind
them, and reporting it would have a program stop looking for a fallback it
still needs.

### Printer control: captured, never a printer

`MC` was on the never-list with the reason *"a child must not be able to
write to a printer"*. That reason holds — but it indicts the **sink**, not
the sequence. xterm implements `MC` by piping the captured stream to a shell
command and ships that command empty by default, precisely because `cat`-ing
a hostile file must never reach a device. Replace the sink and the hazard is
gone, while `xterm-256color` — which every embedded terminal here advertises
— keeps promising `mc0`/`mc4`/`mc5` to programs that then print into
silence.

Planned (driven by ckmux's WP-PRINT, implemented here): a
`TerminalPrinterPolicy{Deny, Capture}` on `TerminalCapabilityProfile`,
**defaulting to `Deny` so nothing changes for an application that does not
opt in**; a printer-controller parse state whose `CSI 4 i` terminator match
survives `feed_output()` chunk boundaries; print-screen, print-line and
autoprint; `DECPFF`/`DECPEX`; honest `CSI ? 15 n` replies; a bounded spool
(`max_printer_spool_bytes`) drained out of band by `take_printer_jobs()`,
never through `snapshot()`. On overflow the emulator frees its buffer and
becomes a **sink** — still in controller mode, still scanning for the
terminator, still not touching the grid — because resuming the screen
mid-stream would put the child's raw print bytes on the display. The
emulator captures; it still cannot open a device or run a command.

### Deliberately never supported

Tektronix and ReGIS graphics. DRCS soft fonts. Answerback (`ENQ` returns
empty, never a fingerprint), and for the same reason DA2, DA3 and XTVERSION —
DA1 states what this terminal can do and is answered; those three state what
it *is*. Unknown string sequences are parsed to
completion and dropped, so an unrecognised sequence can never desynchronise
the parser.

**Gap against that last claim:** it is true for `DCS` today, but the parser
has no `APC` (`ESC _`), `PM` (`ESC ^`) or `SOS` (`ESC X`) introducer at all
— `TerminalEmulator::feed_output`'s escape state handles `[ ] P D E M ( ) *
+ 7 8` and sends everything else to a diagnostic and back to Ground, so an
`APC` **payload lands on the screen as text**. Routing those three
introducers into the existing `Discard` state is a three-line fix that makes
the documented contract true.

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
| `underline_styles` (SGR `4:x` / `58`) | curls, dots and a rule with its own colour | every shape degrades to the plain rule, which still reads as emphasis |
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
compares the resulting cells. It currently runs 49 cases; every difference is
either fixed or listed with the documentation that justifies it, and an
unexplained difference fails the build (`ctest -R vt_conformance`).

That harness is how three bugs in this table's own subject matter were found
rather than argued about: IRM insert mode was missing entirely (ANSI modes
were not handled at all), DECAWM could not be turned off, and overwriting
half of a double-width character left the other half orphaned.

It is also how the underline work above was settled rather than assumed. The
reference reports `SGR 21` back as `4:2`, keeps `4:5` as dashed, and treats
`4:0` as no underline at all — so that is what this emulator does, and the
seven cases that establish it fail the build if it ever stops.

## Gaps worth closing next, in order of what a real program will notice

The six that stood here — `OSC 11`, alternate scroll, `SGR 4:3`/`58`, child
`OSC 52`, indexed colour, and the kitty keyboard protocol — are all closed and
documented above. What is left, in the same order of what a program notices:

1. **`OSC 8` hyperlinks**, inner side. A child's links are currently plain
   text. The outer half is presented (D-088): a cell can carry a link and the
   Presenter brackets it in OSC 8 on a host that renders them, so what is left
   is parsing a child's OSC 8 into the emulator's cells, under a policy.
2. **`SGR 53` overline.** Rare, and the only remaining SGR gap.
3. **`ReportAlternateKeys`**, which needs the input model to carry the
   keyboard layout behind a key rather than only the text it produced. Until
   then the flag is dropped at the moment it is set, so a program is told the
   truth about what it will receive.
4. **Printer capture** (`MC`), designed above and waiting on its policy.
5. **The `APC`/`PM`/`SOS` routing bug** noted above, which is a three-line fix
   to a contract this document already claims.
6. **modifyOtherKeys** (`CSI > 4 ; 1 m` / `CSI > 4 ; 2 m`), the xterm side of
   what the kitty flags do: an ordinary key held with a modifier travels as
   `CSI 27 ; mod ; code ~`, which is how vim, neovim and Claude Code tell
   `Shift+Enter` from `Enter` on a terminal without the kitty protocol. Today
   the request is consumed and the query answers 0, which is true (D-062).
   Real support is a level per screen beside the kitty flags in the emulator's
   `KeyboardState` and in `TerminalStatus` (a host that forwards this terminal
   carries it as it carries the flags); `TerminalView::encode_key` producing
   the form for Ctrl- and Alt-held keys and for modified Enter, Tab, Escape
   and Backspace — Shift+Tab stays `CSI Z`, a shifted printable stays text —
   with level 1 sparing the keys whose legacy byte is well known (Ctrl+letter,
   the control aliases, Backspace), per xterm's manual; XTQMODKEYS answering
   the level in force; and the kitty encoding taking precedence where both are
   on, because it is the more specific request.
