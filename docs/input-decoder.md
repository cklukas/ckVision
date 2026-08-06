# ckVision Input Decoder, v1

`ckv::term::InputDecoder` (`include/cvision/term/input_decoder.hpp`,
`src/term/input_decoder.cpp`) turns terminal input bytes into typed
`TerminalEvent`s. This document records exactly what is covered, what
is deliberately deferred, and why — the same discipline as
`docs/text-width.md` and `docs/golden-format.md`.

## Coverage

- **C0 controls**: Tab, Enter, Backspace (both `0x7F` and `0x08`),
  Ctrl+A through Ctrl+Z (`0x01`-`0x1A`).
- **UTF-8 text**: every non-control byte sequence decodes to one
  `KeyEvent{Key::Char, ...}` per codepoint, with correct incomplete/
  malformed handling for a byte stream that may be split across
  multiple `feed()` calls (not just a single complete buffer, unlike
  `ckv::text::sanitize_display_text`).
- **Legacy Alt+key** (`ESC` + one ordinary byte): recognized and
  tagged with `Modifier::Alt`, reusing the plain byte/UTF-8 decode path
  recursively.
- **Lone Escape vs. sequence start**: resolved against `kEscTimeoutNanos`
  (50 ms) on the caller-supplied clock time — see `poll_timeout`.
- **Cursor/navigation keys**: arrows, Home/End (`CSI` letter-final and
  `SS3` letter-final forms), Insert/Delete/PageUp/PageDown, F1-F12 (both
  the classic `SS3 P/Q/R/S` encoding for F1-F4 and the `CSI n~` encoding
  for F1-F12), Shift+Tab (`CSI Z`), each with the standard
  `;modifier` parameter when present.
- **Kitty keyboard protocol**: the `CSI code[;modifiers]u` form —
  named keys (Enter/Tab/Escape/Backspace) plus arbitrary Unicode
  codepoints as `Key::Char`.
- **xterm modifyOtherKeys**: the `CSI 27;modifier;keycode~` form,
  sharing the same codepoint-to-KeyChord conversion as kitty.
- **SGR mouse** (`CSI < button;x;y M/m`): press/release/motion/wheel,
  modifiers, and — when `Capabilities::pixel_mouse` is set — pixel
  coordinates alongside cell coordinates in the same `MouseEvent`
  (D-018).
- **Bracketed paste** (`CSI 200~` ... `CSI 201~`): delivered as one
  atomic, sanitized `TextEvent` (D-023's paste rule: C0 stripped except
  tab/newline, C1 and malformed bytes replaced with U+FFFD). Large
  pastes are accumulated without re-scanning already-confirmed-safe
  bytes on every `feed()` call.
- **Focus events** (`CSI I` / `CSI O`).
- **Two probe responses**, enough to prove the probe-response pipeline
  end to end: OSC 10/11 background-color queries (→
  `Capabilities::color_scheme`, via relative luminance) and the DECRPM
  report for mode 2026 (→ `Capabilities::synchronized_output`). Both
  produce a `CapabilityChangedEvent`.

Every unrecognized-but-well-formed sequence is consumed as a whole
(never leaks into a resync loop); only a genuinely malformed byte causes
single-byte resync.

## Known v1 scope gaps

- **DA1 (`CSI ? ... c`) is recognized and consumed but not parsed**:
  the response identifies terminal features (including, on some
  terminals, Sixel support) via its parameter list, which this version
  does not decode. Detecting Sixel/graphics support via DA1 is deferred;
  `Capabilities::sixel_graphics`/`kitty_graphics` must be set by other
  means (a forced override, or a future XTSMGRAPHICS-based probe) until
  this is implemented.
- **XTGETTCAP and XTSMGRAPHICS are not implemented at all.**
- **No mixed CSI sub-parameters** (the `:` separator, used for e.g.
  kitty's extended key-event fields) — `:` is folded into `;` handling,
  which loses sub-parameter structure. Low practical impact for v1's
  key set.
- **The bracketed-paste terminator has an inherent, protocol-level
  ambiguity this decoder cannot fully resolve**: if pasted clipboard
  content itself contains the literal byte sequence `ESC[201~`, the
  terminal emulator typically sends it verbatim (most emulators do not
  escape accidental terminator-look-alike bytes within paste content),
  and the decoder has no way to distinguish that from a genuine
  end-of-paste marker. This is a known limitation of the bracketed-
  paste protocol itself, not specific to this implementation — the
  decoder does the best a receiving parser can do: split at the first
  occurrence, sanitize whatever is classified as paste content, and
  decode whatever follows as ordinary input (never as "still-trusted
  paste text"). Applications pasting into contexts where injected
  keystrokes could cause damage should not assume bracketed paste alone
  is a complete defense.
