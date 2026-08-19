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
  `ckv::text::sanitize_display_text`). `Application` routes these as key
  events. Editable controls normalize unmodified and Shift-modified
  `Key::Char` events through their `on_text` insertion path, while Alt, Ctrl,
  and Super character chords remain available to shortcut routing. IME input
  and bracketed paste arrive directly as `TextEvent`.
- **Legacy Alt+key** (`ESC` + one ordinary byte): recognized and
  tagged with `Modifier::Alt`, reusing the plain byte/UTF-8 decode path
  recursively.
- **Lone Escape vs. sequence start**: resolved against `kEscTimeoutNanos`
  (50 ms) on the caller-supplied clock time only for `Legacy` and
  `ModifyOtherKeys` profiles — see `poll_timeout`. A negotiated `Kitty`
  profile delivers a bare Escape immediately because its protocol removes the
  Escape-versus-Alt ambiguity.
- **Cursor/navigation keys**: arrows, Home/End (`CSI` letter-final and
  `SS3` letter-final forms), Insert/Delete/PageUp/PageDown, F1-F12 (both
  the classic `SS3 P/Q/R/S` encoding for F1-F4 and the `CSI n~` encoding
  for F1-F12), Shift+Tab (`CSI Z`), each with the standard
  `;modifier` parameter when present.
- **Kitty keyboard protocol**: the full
  `CSI code[:alternates];modifiers[:event];text u` form, accepted only when
  `Capabilities::keyboard_protocol == KeyboardProtocol::Kitty`. Named keys
  (Enter/Tab/Escape/Backspace), arbitrary Unicode codepoints as `Key::Char`,
  and the functional private-use block 57344–63743: the keypad's navigation
  keys map to their named keys, the keypad's character keys type their
  canonical character, and every other functional code — modifier and lock
  keys, media keys, F13 and beyond — is consumed deliberately rather than
  delivered as private-use text. The alternate-key subparameters are parsed
  and ignored (D-047: the model carries a key and the text it produced, not
  the layout behind them); the associated-text field is authoritative for
  `Key::Char` text, because under the all-keys-as-escape-codes enhancement
  it is the only correct source for a shifted or composed character. The
  event type also rides on the legacy-form functional keys
  (`CSI 1;1:3 A` is Up going back up, not a second Up). Press and repeat
  use `View::on_key`, while release is delivered only through
  `View::on_key_release` and can never execute a command binding.
  `KeyEvent::reports_release` is true exactly when the event arrived
  escape-coded on a session whose verified enhancement set includes event
  types — the POSIX backend pushes the full requested set once the
  protocol is proven, reads back what the host honoured (`CSI ? u`), and
  records that answer in `Capabilities::kitty_keyboard_flags` (D-055).
- **xterm modifyOtherKeys**: the `CSI 27;modifier;keycode~` form,
  sharing the same codepoint-to-KeyChord conversion as kitty and accepted only
  when the selected keyboard protocol is `ModifyOtherKeys`.
- **SGR mouse** (`CSI < button;x;y M/m`): press/release/motion/wheel,
  modifiers, and — when `Capabilities::pixel_mouse` is set — pixel
  coordinates alongside cell coordinates in the same `MouseEvent`
  (D-018). It is accepted only for an `SGR` mouse profile. While a POSIX
  capability probe has temporarily enabled SGR-pixel mode but has not yet
  established both mode 1016 and an XTWINOPS cell metric, complete SGR reports
  are consumed without delivery; their coordinates are deliberately not guessed
  as cells. A proof earlier in the same read enables pixel delivery for a later
  report in that read.
- **X10 mouse** (`CSI M` followed by three byte-offset payload bytes):
  button presses and cell coordinates for an explicitly selected `X10`
  profile. It does not carry releases, motion, or pixel coordinates. Every
  other profile consumes the complete report without exposing its payload as
  keyboard text.
- **Bracketed paste** (`CSI 200~` ... `CSI 201~`): delivered as one
  atomic, sanitized `TextEvent` (D-040's paste rule: C0 stripped except
  tab/newline, C1 and malformed bytes replaced with U+FFFD). An end marker is
  a candidate for 50 ms: bytes arriving before that quiet boundary remain
  paste text, never ordinary key/mouse/OSC input. Large pastes are accumulated
  without re-scanning already-confirmed-safe bytes on every `feed()` call.
- **Focus events** (`CSI I` / `CSI O`).

Focus events and bracketed paste are likewise accepted only when their
capabilities are enabled. An unnegotiated bracketed-paste opening delimiter
starts an opaque discard through its closing delimiter; its payload never
falls through as ordinary text input.
- **Nine probe responses**, enough to prove the probe-response pipeline
  end to end: OSC 10/11 dynamic foreground/background queries (→
  `Capabilities::color_scheme`). OSC 11's background luminance is direct
  evidence; OSC 10 is an inverse-contrast fallback only until a valid OSC 11
  reply arrives. The DECRPM
  reports for modes 2026 (→ `Capabilities::synchronized_output`) and 2031
  (→ `Capabilities::color_scheme_notifications`), plus a
  Primary DA response advertising parameter 4 (→
  `Capabilities::sixel_graphics`), an XTWINOPS 16 reply (→
  `Capabilities::cell_pixels`), and DECRPM 1016 combined with known cell
  metrics (→ `Capabilities::pixel_mouse`). The last two may arrive in either
  order; pixel coordinates are advertised only after both facts are known.
  XTSMGRAPHICS reports a verified Sixel color-register limit and maximum pixel
  geometry. A successful, finite positive Sixel-geometry reply is also
  positive Sixel evidence; an error, malformed, or zero-sized reply is
  ignored. A resize withdraws runtime graphics until such a fresh geometry
  reply arrives, because the terminal's maximum may be window-limited. These
  produce a
  `CapabilityChangedEvent` while the backend's bounded probe window is active.
  The backend admits or rejects each candidate inside the decoder, before any
  later sequence from the same input read is parsed. Consequently a late or
  unrequested probe reply cannot transiently alter pixel-mouse, graphics, or
  other decode state even when a real input event follows it in the same batch.
Once mode 2031 is positively verified, `CSI ? 997 ; 1 n` (dark) and
`CSI ? 997 ; 2 n` (light) are accepted as live color-preference changes for
the lifetime of that terminal session; an unsolicited notification never
establishes that capability by itself. Those explicit notifications take
precedence over both OSC color inferences.

  The POSIX backend temporarily enables mode 2026 before querying it, then
  resets it in the same probe write. A positive DECRPM reply therefore proves
  support rather than merely observing an unusually already-enabled mode.

Every unrecognized-but-well-formed sequence is consumed as a whole
(never leaks into a resync loop); only a genuinely malformed byte causes
single-byte resync.

## Known v1 scope gaps

- **DA1 is intentionally conservative:** parameter 4 enables Sixel when
  advertised, but a response that omits it never disables a curated or
  forced graphics profile. A successful XTSMGRAPHICS Sixel-geometry response
  is independent positive evidence; its reported geometry preserves the text
  fallback when an image would exceed that limit, and its color-register limit
  bounds the encoder's palette.
- **XTGETTCAP is not implemented.**
- **Kitty alternate-key codes are parsed and deliberately not retained.**
  The decoder reads the full subparameter grammar (event types, embedded
  text-as-codepoints), but the shifted/base-layout key codes have no
  representation in the `KeyEvent` contract by decision, not omission:
  ckVision never requests that enhancement (D-047), and a host volunteering
  the codes anyway changes nothing. Caps-lock and num-lock modifier bits in
  the modifiers field are likewise ignored rather than misread.
## Bracketed-paste trust boundary and recovery

`CSI 201~` is an in-band delimiter. A pasted byte sequence can literally
contain it, and the decoder cannot prove whether any occurrence closes the
paste. The decoder therefore does **not** treat the first marker as immediate
permission to interpret following bytes as keystrokes.

It retains the newest candidate for `kPasteTerminationQuietNanos` (50 ms). A
later byte before that deadline is recovered as paste text; a later end marker
makes the earlier one visible as sanitized literal text (`U+FFFD[201~`). When
the deadline expires, the newest marker closes the paste and the atomic
`TextEvent` is emitted. `paste_recovered` is true for an ambiguous tail,
multiple candidates, or disconnect. An input widget therefore visibly receives
the recovered text and an application can surface an additional warning from
that flag.

This conservatively protects a normal clipboard stream, including arbitrary
backend read fragmentation: immediate command chords, OSC terminators, and
mouse reports after a look-alike marker remain paste data. It does not claim
that a hostile party able to inject arbitrary terminal bytes *after* the quiet
period has clipboard provenance. Such later bytes are normal terminal input;
stronger confirmation requires an out-of-band host signal or application
policy. On a definite terminal disconnect, `abort_paste()` emits the sanitized
partial text as recovered paste rather than decoding it as commands.
