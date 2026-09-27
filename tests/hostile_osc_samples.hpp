// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The hostile OSC payloads and the exact bytes every VT backend must send for
// them (docs/terminal-host-integration.md#osc-emission-safety). Shared by the
// portable emission suite and the real-PTY suite, so the headless model and a
// live PosixTerminal are held to one pinned byte string.
#pragma once

#include <string_view>

namespace ckv::test {

// A title that tries every way out of an OSC string: ESC \ (7-bit ST), BEL
// (xterm's terminator), a raw 0x9C (8-bit ST, malformed UTF-8), U+009C (ST as
// a UTF-8 host meets it), CAN and SUB (which cancel a sequence), DEL, a line
// break, and a complete nested OSC 0 that would retitle the window after the
// real one ended.
inline constexpr std::string_view kHostileTitle =
    "A\x1B\\B\x07" "C\x9C" "D\xC2\x9C" "E\x18" "F\x1A" "G\x7F" "H\nI\x1B]0;evil\x07" "J";

// What survives: the printable text, the lone 0x9C as U+FFFD, and nothing
// that could end the string or begin another sequence.
inline constexpr std::string_view kHostileTitleAsHostShowsIt =
    "A\\BC\xEF\xBF\xBD" "DEFGHI]0;evilJ";

// The complete OSC 0 sequence: exactly one ESC (the introducer) and exactly
// one BEL (the terminator).
inline constexpr std::string_view kHostileTitleSequence =
    "\x1B]0;A\\BC\xEF\xBF\xBD" "DEFGHI]0;evilJ\x07";

// Clipboard text carrying an OSC 52 read request, BEL, raw 0x9C, U+009C and a
// line break. OSC 52 carries it as base64, so it reaches the clipboard
// unchanged and none of it reaches the terminal as a control.
inline constexpr std::string_view kHostileClipboardText =
    "copy\x1B]52;c;?\x07\x9C\xC2\x9C\n";

// OSC 52 ; c ; base64(kHostileClipboardText) BEL.
inline constexpr std::string_view kHostileClipboardSequence =
    "\x1B]52;c;Y29weRtdNTI7Yzs/B5zCnAo=\x07";

}  // namespace ckv::test
