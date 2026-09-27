// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// OSC emission safety (the architecture §12, WP-29): hostile titles and
// clipboard text go through the public Terminal calls, and the exact bytes a
// host receives are pinned. The same pinned bytes are checked against a live
// PosixTerminal on a real PTY in test_posix_terminal.cpp.
#include "cvision/core/text.hpp"
#include "cvision/core/utf8.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/osc_sequences.hpp"
#include "cvision/term/record_replay_terminal.hpp"
#include "cvision/term/virtual_display.hpp"

#include <string>
#include <string_view>
#include <variant>

#include "cvision/testing/cktest.hpp"
#include "hostile_osc_samples.hpp"

using namespace ckv;
using namespace ckv::term;

namespace {

std::size_t count_byte(std::string_view bytes, char byte) {
    std::size_t count = 0;
    for (const char c : bytes)
        if (c == byte) ++count;
    return count;
}

Capabilities clipboard_host() {
    Capabilities caps = baseline_capabilities();
    caps.clipboard_write = true;
    return caps;
}

}  // namespace

// --- The sanitizer ------------------------------------------------------------

CK_TEST(sanitize_osc_text_strips_embedded_terminator_bytes) {
    CK_CHECK(text::sanitize_osc_text("hello") == "hello");
    CK_CHECK(text::sanitize_osc_text(std::string_view("a\x1B" "b\x07" "c")) == "abc");
}

CK_TEST(sanitize_osc_text_strips_every_c1_string_terminator_and_control) {
    // ECMA-48 §8.3.143: ST is ESC \ in a 7-bit code and 09/12 in an 8-bit
    // one, and §8.3.89 admits no other control into a command string. A
    // UTF-8 receiver meets C1 as the code points U+0080..U+009F, and a lone
    // 0x9C is not UTF-8 at all — neither may reach the OSC as the byte 0x9C
    // or as U+009C. CAN and SUB cancel a sequence in progress (xterm control
    // sequences, "Single-character functions"), so they end a title too.
    const std::string raw_st = text::sanitize_osc_text(std::string_view("a\x9C" "b"));
    CK_CHECK(raw_st.find('\x9C') == std::string::npos);
    CK_CHECK(raw_st.front() == 'a' && raw_st.back() == 'b');
    CK_CHECK(text::sanitize_osc_text(std::string_view("a\xC2\x9C" "b")) == "ab");  // U+009C, ST
    CK_CHECK(text::sanitize_osc_text(std::string_view("a\xC2\x9B" "b")) == "ab");  // U+009B, CSI
    CK_CHECK(text::sanitize_osc_text(std::string_view("a\x18" "b\x1A" "c\x7F" "d")) == "abcd");
    CK_CHECK(text::sanitize_osc_text(std::string_view("tab\there\nnext")) == "tabherenext");
    // Text is left alone, including characters whose encoding contains a
    // 0x80..0x9F continuation byte: U+201C is E2 80 9C.
    CK_CHECK(text::sanitize_osc_text("\xE2\x80\x9C" "quoted" "\xE2\x80\x9D") ==
             "\xE2\x80\x9C" "quoted" "\xE2\x80\x9D");
    CK_CHECK(text::sanitize_osc_text("gr\xC3\xBC\xC3\x9F" "e") == "gr\xC3\xBC\xC3\x9F" "e");
}

CK_TEST(sanitize_osc_text_empty_input) { CK_CHECK(text::sanitize_osc_text("").empty()); }

// --- The pinned sequences -----------------------------------------------------

CK_TEST(a_hostile_title_becomes_one_osc_0_sequence_with_one_introducer_and_one_terminator) {
    const std::string sequence = osc_title_sequence(test::kHostileTitle);
    CK_CHECK(sequence == test::kHostileTitleSequence);
    // The whole argument in four facts: the introducer is the only ESC, the
    // terminator is the only BEL, no byte can be read as 8-bit ST, and the
    // string is well-formed UTF-8 with no C1 code point in it.
    CK_CHECK(count_byte(sequence, '\x1B') == 1 && sequence.front() == '\x1B');
    CK_CHECK(count_byte(sequence, '\x07') == 1 && sequence.back() == '\x07');
    CK_CHECK(sequence.find('\x9C') == std::string::npos);
    CK_CHECK(utf8::is_valid(sequence));
    CK_CHECK(sequence.find("\xC2\x9C") == std::string::npos);
    for (const unsigned char byte : sequence.substr(1, sequence.size() - 2))
        CK_CHECK(byte >= 0x20 && byte != 0x7F);
}

CK_TEST(hostile_clipboard_text_reaches_the_host_only_as_base64) {
    const std::string sequence = osc_clipboard_sequence(test::kHostileClipboardText);
    CK_CHECK(sequence == test::kHostileClipboardSequence);
    CK_CHECK(count_byte(sequence, '\x1B') == 1);
    CK_CHECK(count_byte(sequence, '\x07') == 1);
}

// --- Through the public Terminal calls ----------------------------------------

CK_TEST(headless_set_title_sends_exactly_the_bytes_a_live_backend_sends) {
    HeadlessTerminal terminal(Size{8, 2});
    terminal.set_title(test::kHostileTitle);
    CK_CHECK(terminal.written_bytes() == test::kHostileTitleSequence);
    CK_CHECK(terminal.display().valid());
    CK_CHECK(terminal.title() == test::kHostileTitleAsHostShowsIt);
    // The title is host state outside the screen: nothing was drawn.
    CK_CHECK(terminal.display().frame().at(Point{0, 0}).grapheme() == " ");
}

CK_TEST(headless_clipboard_export_sends_exactly_the_bytes_a_live_backend_sends) {
    HeadlessTerminal terminal(Size{8, 2}, clipboard_host());
    terminal.write_clipboard(test::kHostileClipboardText);
    CK_CHECK(terminal.written_bytes() == test::kHostileClipboardSequence);
    CK_CHECK(terminal.clipboard() == test::kHostileClipboardText);

    HeadlessTerminal refusing(Size{8, 2});
    refusing.write_clipboard(test::kHostileClipboardText);
    CK_CHECK(refusing.written_bytes().empty());
    CK_CHECK(refusing.clipboard().empty());
}

CK_TEST(recording_terminal_keeps_the_callers_title_while_the_host_receives_only_the_safe_sequence) {
    HeadlessTerminal host(Size{8, 2}, clipboard_host());
    RecordingTerminal recorder(host);
    recorder.set_title(test::kHostileTitle);
    recorder.write_clipboard(test::kHostileClipboardText);

    CK_CHECK(recorder.recording().size() == 2);
    CK_CHECK(std::get<RecordedTitle>(recorder.recording()[0]).title == test::kHostileTitle);
    CK_CHECK(std::get<RecordedClipboard>(recorder.recording()[1]).text == test::kHostileClipboardText);
    CK_CHECK(host.written_bytes() ==
             std::string(test::kHostileTitleSequence) + std::string(test::kHostileClipboardSequence));

    // Replaying the recording reproduces the same host bytes.
    HeadlessTerminal replay_host(Size{8, 2}, clipboard_host());
    for (const RecordedEntry& entry : recorder.recording()) {
        if (const auto* title = std::get_if<RecordedTitle>(&entry)) replay_host.set_title(title->title);
        if (const auto* clip = std::get_if<RecordedClipboard>(&entry)) replay_host.write_clipboard(clip->text);
    }
    CK_CHECK(replay_host.written_bytes() == host.written_bytes());
}

// --- The host model refuses what a host could misread -------------------------

CK_TEST(virtual_display_models_osc_0_titles_and_osc_52_exports) {
    VirtualDisplay display(Size{2, 1});
    CK_CHECK(display.write("\x1B]0;ok\x07"));
    CK_CHECK(display.window_title() == "ok");
    CK_CHECK(display.write("\x1B]52;c;aGVsbG8=\x1B\\"));
    CK_CHECK(display.clipboard_text() == "hello");
    display.clear();
    CK_CHECK(display.window_title() == "ok");
    CK_CHECK(display.clipboard_text() == "hello");
}

CK_TEST(virtual_display_refuses_an_osc_title_or_clipboard_payload_a_host_could_misread) {
    for (const std::string_view hostile : {
             std::string_view("\x1B]0;a\x7F" "b\x07"),        // DEL
             std::string_view("\x1B]0;a\xC2\x9C" "b\x07"),    // U+009C, ST to a UTF-8 host
             std::string_view("\x1B]0;a\xC2\x9B" "b\x07"),    // U+009B, CSI to a UTF-8 host
             std::string_view("\x1B]0;a\x9C" "b\x07"),        // 0x9C, ST to an 8-bit host
             std::string_view("\x1B]0;a\x18" "b\x07"),        // CAN
             std::string_view("\x1B]52;c;not base64!\x07"),  // a payload that is not base64
             std::string_view("\x1B]52;p;YQ==\x07"),          // a selection ckVision never writes
         }) {
        VirtualDisplay display(Size{2, 1});
        CK_CHECK(!display.write(hostile));
        CK_CHECK(!display.error().empty());
        CK_CHECK(display.window_title().empty());
        CK_CHECK(display.clipboard_text().empty());
    }
}
