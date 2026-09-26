// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "echo_script.hpp"

#include "cvision/term/input_decoder.hpp"

namespace ckv::docgen {

EchoSession::EchoSession() { app.step(clock.now_nanos()); }

std::vector<EchoBeat> echo_script() {
    std::vector<EchoBeat> beats;
    // A printable key, a two-byte UTF-8 character, and Ctrl+A as its C0 byte.
    beats.push_back({.name = "typed", .bytes = "a\xC3\xA9\x01"});
    // CSI and SS3 function keys with and without a modifier parameter, then
    // the C0 keys, and Shift+Tab. Tab and Shift+Tab stay bound to focus
    // traversal, which finds only the echo view to move to.
    beats.push_back({.name = "named_keys", .bytes = "\x1B[A\x1B[1;5C\x1BOP\x1B[15~\x7F\r\t\x1B[Z"});
    // ESC followed at once by a byte is Alt with that byte.
    beats.push_back({.name = "alt_chord", .bytes = "\x1Bg"});
    // ESC with nothing after it is Esc once the decoder's quiet deadline passes.
    beats.push_back({.name = "lone_escape", .bytes = "\x1B", .wait_nanos = term::kEscTimeoutNanos});
    // SGR reports: a left press, a drag, the release, one wheel notch, and a
    // Shift+Ctrl left click.
    beats.push_back({.name = "mouse",
                     .bytes = "\x1B[<0;11;6M\x1B[<32;13;6M\x1B[<0;13;6m\x1B[<65;20;10M"
                              "\x1B[<20;5;5M\x1B[<20;5;5m"});
    // A bracketed paste with a tab and a line break in it. Its end marker
    // closes it only once the decoder's quiet deadline has passed with no
    // later byte, since a pasted byte sequence may itself contain one.
    beats.push_back({.name = "paste",
                     .bytes = "\x1B[200~hello\tworld\r\nline two\x1B[201~",
                     .wait_nanos = term::kPasteTerminationQuietNanos});
    // The host window losing and regaining focus (DEC mode 1004 reports).
    beats.push_back({.name = "focus", .bytes = "\x1B[O\x1B[I"});
    beats.push_back({.name = "resize", .resize = Size{100, 30}});
    return beats;
}

void play(EchoSession& session, const EchoBeat& beat) {
    if (!beat.bytes.empty()) {
        session.terminal.inject_bytes(beat.bytes, session.clock.now_nanos());
        session.app.step(session.clock.now_nanos());
    }
    if (beat.wait_nanos > 0) {
        session.clock.advance(beat.wait_nanos);
        session.terminal.inject_timeout_check(session.clock.now_nanos());
        session.app.step(session.clock.now_nanos());
    }
    if (beat.resize) {
        session.terminal.resize(*beat.resize);
        session.app.step(session.clock.now_nanos());
    }
    // One more turn of the loop, so every beat ends on the frame a reader is
    // left looking at.
    session.app.step(session.clock.now_nanos());
}

}  // namespace ckv::docgen
