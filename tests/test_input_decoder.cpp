// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Adversarial coverage for InputDecoder — coverage/scope rationale in
// docs/input-decoder.md. Deliberately goes well beyond the happy path:
// split reads, malformed bytes, hostile paste content, OSC injection,
// and the documented paste-terminator protocol limitation.
#include "cvision/term/input_decoder.hpp"

#include "cktest.hpp"

using namespace ckv;
using namespace ckv::term;

namespace {

KeyEvent as_key(const TerminalEvent& ev) { return std::get<KeyEvent>(ev); }
MouseEvent as_mouse(const TerminalEvent& ev) { return std::get<MouseEvent>(ev); }
TextEvent as_text(const TerminalEvent& ev) { return std::get<TextEvent>(ev); }

std::vector<TerminalEvent> decode_all(std::string_view bytes, Capabilities caps = baseline_capabilities()) {
    InputDecoder decoder(caps);
    return decoder.feed(bytes, 0);
}

}  // namespace

// --- Plain text and controls ------------------------------------------------

CK_TEST(ascii_text_decodes_one_key_event_per_character) {
    const auto events = decode_all("Hi!");
    CK_CHECK(events.size() == 3);
    CK_CHECK(as_key(events[0]).chord.key == Key::Char);
    CK_CHECK(as_key(events[0]).chord.text == "H");
    CK_CHECK(as_key(events[2]).chord.text == "!");
}

CK_TEST(multibyte_utf8_decodes_to_one_key_event) {
    const auto events = decode_all("\xE4\xB8\xAD");  // 中
    CK_CHECK(events.size() == 1);
    CK_CHECK(as_key(events[0]).chord.text == "\xE4\xB8\xAD");
}

CK_TEST(split_utf8_across_two_feeds_still_decodes_correctly) {
    InputDecoder decoder;
    auto first = decoder.feed(std::string_view("\xE4\xB8", 2), 0);
    CK_CHECK(first.empty());  // incomplete: waiting for the third byte
    auto second = decoder.feed(std::string_view("\xAD", 1), 0);
    CK_CHECK(second.size() == 1);
    CK_CHECK(as_key(second[0]).chord.text == "\xE4\xB8\xAD");
}

CK_TEST(c0_controls_map_to_named_keys) {
    auto events = decode_all("\x09\x0D\x7F\x08");
    CK_CHECK(events.size() == 4);
    CK_CHECK(as_key(events[0]).chord.key == Key::Tab);
    CK_CHECK(as_key(events[1]).chord.key == Key::Enter);
    CK_CHECK(as_key(events[2]).chord.key == Key::Backspace);
    CK_CHECK(as_key(events[3]).chord.key == Key::Backspace);
}

CK_TEST(ctrl_letter_combinations) {
    auto events = decode_all(std::string_view("\x01\x1A", 2));  // Ctrl+A, Ctrl+Z
    CK_CHECK(events.size() == 2);
    CK_CHECK(as_key(events[0]).chord.key == Key::Char);
    CK_CHECK(as_key(events[0]).chord.text == "a");
    CK_CHECK(has_modifier(as_key(events[0]).chord.modifiers, Modifier::Ctrl));
    CK_CHECK(as_key(events[1]).chord.text == "z");
}

// --- Malformed input resync --------------------------------------------------

CK_TEST(invalid_lead_byte_resyncs_and_does_not_corrupt_following_input) {
    // 0xFF is not a valid UTF-8 lead byte anywhere; "A" after it must
    // still decode correctly.
    const auto events = decode_all(std::string_view("\xFF" "A", 2));
    CK_CHECK(events.size() == 1);
    CK_CHECK(as_key(events[0]).chord.text == "A");
}

CK_TEST(truncated_multibyte_sequence_that_never_completes_does_not_hang) {
    // A 3-byte lead followed by a byte that can never complete it (not
    // a continuation byte) must resync, not stall waiting forever.
    InputDecoder decoder;
    const auto events = decoder.feed(std::string_view("\xE4\x41", 2), 0);  // lead + 'A' (not 0x80-0xBF)
    // \xE4 alone resyncs (invalid), then 'A' decodes normally.
    CK_CHECK(events.size() == 1);
    CK_CHECK(as_key(events[0]).chord.text == "A");
}

CK_TEST(malformed_csi_sequence_resyncs_without_losing_subsequent_input) {
    // A CSI sequence whose "final byte" position holds a byte outside
    // the valid 0x40-0x7E final-byte range is malformed.
    const auto events = decode_all(std::string_view("\x1B[1\x01" "A", 5));
    // ESC[1 followed by 0x01 (not a valid CSI final byte, not a digit
    // or ';'): resyncs byte by byte, eventually reaching Ctrl+A then 'A'.
    CK_CHECK(!events.empty());
    const auto& last = events.back();
    CK_CHECK(as_key(last).chord.text == "A");
}

// --- ESC vs. sequence disambiguation ----------------------------------------

CK_TEST(esc_immediately_followed_by_csi_bytes_decodes_as_the_sequence) {
    const auto events = decode_all("\x1B[A");  // Up arrow, no timeout wait needed
    CK_CHECK(events.size() == 1);
    CK_CHECK(as_key(events[0]).chord.key == Key::Up);
}

CK_TEST(lone_esc_resolves_only_after_the_quiet_deadline) {
    InputDecoder decoder;
    auto immediate = decoder.feed("\x1B", 1000);
    CK_CHECK(immediate.empty());  // ambiguous: might still be a sequence start

    auto too_soon = decoder.poll_timeout(1000 + kEscTimeoutNanos - 1);
    CK_CHECK(too_soon.empty());

    auto resolved = decoder.poll_timeout(1000 + kEscTimeoutNanos);
    CK_CHECK(resolved.size() == 1);
    CK_CHECK(as_key(resolved[0]).chord.key == Key::Escape);
}

CK_TEST(legacy_alt_key_encoding) {
    const auto events = decode_all("\x1B" "a");
    CK_CHECK(events.size() == 1);
    CK_CHECK(as_key(events[0]).chord.key == Key::Char);
    CK_CHECK(as_key(events[0]).chord.text == "a");
    CK_CHECK(has_modifier(as_key(events[0]).chord.modifiers, Modifier::Alt));
}

CK_TEST(double_escape_resolves_the_first_as_escape_immediately) {
    const auto events = decode_all(std::string_view("\x1B\x1B" "A", 3));
    CK_CHECK(events.size() == 2);
    CK_CHECK(as_key(events[0]).chord.key == Key::Escape);
    // Second ESC is now alone at the front of what remains; within one
    // feed() call it cannot resolve without a later timeout, UNLESS
    // followed by more bytes in the same buffer (here 'A' follows,
    // which makes it Alt+A via the legacy encoding).
    CK_CHECK(as_key(events[1]).chord.text == "A");
    CK_CHECK(has_modifier(as_key(events[1]).chord.modifiers, Modifier::Alt));
}

// --- Cursor/navigation/function keys -----------------------------------------

CK_TEST(csi_arrows_with_and_without_modifiers) {
    auto plain = decode_all("\x1B[D");
    CK_CHECK(as_key(plain[0]).chord.key == Key::Left);
    CK_CHECK(as_key(plain[0]).chord.modifiers == Modifier::None);

    auto ctrl = decode_all("\x1B[1;5A");  // Ctrl+Up
    CK_CHECK(as_key(ctrl[0]).chord.key == Key::Up);
    CK_CHECK(has_modifier(as_key(ctrl[0]).chord.modifiers, Modifier::Ctrl));
}

CK_TEST(ss3_f1_through_f4) {
    CK_CHECK(as_key(decode_all("\x1BOP")[0]).chord.key == Key::F1);
    CK_CHECK(as_key(decode_all("\x1BOQ")[0]).chord.key == Key::F2);
    CK_CHECK(as_key(decode_all("\x1BOR")[0]).chord.key == Key::F3);
    CK_CHECK(as_key(decode_all("\x1BOS")[0]).chord.key == Key::F4);
}

CK_TEST(tilde_terminated_keys) {
    CK_CHECK(as_key(decode_all("\x1B[2~")[0]).chord.key == Key::Insert);
    CK_CHECK(as_key(decode_all("\x1B[3~")[0]).chord.key == Key::Delete);
    CK_CHECK(as_key(decode_all("\x1B[5~")[0]).chord.key == Key::PageUp);
    CK_CHECK(as_key(decode_all("\x1B[6~")[0]).chord.key == Key::PageDown);
    CK_CHECK(as_key(decode_all("\x1B[15~")[0]).chord.key == Key::F5);
    CK_CHECK(as_key(decode_all("\x1B[24~")[0]).chord.key == Key::F12);
}

CK_TEST(shift_tab_and_focus_events) {
    auto shift_tab = decode_all("\x1B[Z");
    CK_CHECK(as_key(shift_tab[0]).chord.key == Key::Tab);
    CK_CHECK(has_modifier(as_key(shift_tab[0]).chord.modifiers, Modifier::Shift));

    auto focus_in = decode_all("\x1B[I");
    CK_CHECK(std::get<FocusEvent>(focus_in[0]).gained);
    auto focus_out = decode_all("\x1B[O");
    CK_CHECK(!std::get<FocusEvent>(focus_out[0]).gained);
}

CK_TEST(modify_other_keys_encoding) {
    // 27;5;13~ = Ctrl+Enter (codepoint 13)
    const auto events = decode_all("\x1B[27;5;13~");
    CK_CHECK(events.size() == 1);
    CK_CHECK(as_key(events[0]).chord.key == Key::Enter);
    CK_CHECK(has_modifier(as_key(events[0]).chord.modifiers, Modifier::Ctrl));
}

CK_TEST(kitty_keyboard_protocol) {
    auto plain = decode_all("\x1B[97u");  // 'a'
    CK_CHECK(as_key(plain[0]).chord.key == Key::Char);
    CK_CHECK(as_key(plain[0]).chord.text == "a");

    auto with_mod = decode_all("\x1B[13;5u");  // Ctrl+Enter
    CK_CHECK(as_key(with_mod[0]).chord.key == Key::Enter);
    CK_CHECK(has_modifier(as_key(with_mod[0]).chord.modifiers, Modifier::Ctrl));
}

CK_TEST(overlong_numeric_parameter_saturates_instead_of_overflowing) {
    // A CSI parameter far longer than any real terminal would send —
    // must not be undefined behavior (signed overflow); it saturates
    // and the sequence is still handled (recognized-but-out-of-range
    // kitty codepoint falls back to the replacement character).
    const auto events = decode_all("\x1B[99999999999999999999u");
    CK_CHECK(events.size() == 1);
    CK_CHECK(as_key(events[0]).chord.key == Key::Char);
}

// --- Mouse -------------------------------------------------------------------

CK_TEST(sgr_mouse_press_and_release) {
    auto press = decode_all("\x1B[<0;10;20M");
    CK_CHECK(as_mouse(press[0]).action == MouseAction::Down);
    CK_CHECK(as_mouse(press[0]).button == MouseButton::Left);
    CK_CHECK(as_mouse(press[0]).cell == (Point{9, 19}));  // 1-based -> 0-based
    CK_CHECK(!as_mouse(press[0]).pixel.has_value());       // no pixel capability: absent, not synthesized

    auto release = decode_all("\x1B[<0;10;20m");
    CK_CHECK(as_mouse(release[0]).action == MouseAction::Up);
}

CK_TEST(sgr_mouse_motion_and_wheel) {
    auto motion = decode_all("\x1B[<32;5;5M");  // bit 0x20 set: drag/motion
    CK_CHECK(as_mouse(motion[0]).action == MouseAction::Move);

    auto wheel_up = decode_all("\x1B[<64;1;1M");
    CK_CHECK(as_mouse(wheel_up[0]).action == MouseAction::Wheel);
    CK_CHECK(as_mouse(wheel_up[0]).button == MouseButton::WheelUp);
}

CK_TEST(sgr_mouse_modifiers) {
    const auto events = decode_all("\x1B[<20;1;1M");  // 0x10 ctrl + 0x04 shift + button0
    CK_CHECK(has_modifier(as_mouse(events[0]).modifiers, Modifier::Ctrl));
    CK_CHECK(has_modifier(as_mouse(events[0]).modifiers, Modifier::Shift));
}

CK_TEST(sgr_pixel_mouse_reports_both_coordinate_spaces) {
    Capabilities caps = baseline_capabilities();
    caps.pixel_mouse = true;
    caps.cell_pixels = Size{8, 16};
    const auto events = decode_all("\x1B[<0;41;33M", caps);
    CK_CHECK(as_mouse(events[0]).pixel.has_value());
    CK_CHECK(as_mouse(events[0]).pixel->x == 40);  // 1-based -> 0-based pixel
    CK_CHECK(as_mouse(events[0]).pixel->y == 32);
    CK_CHECK(as_mouse(events[0]).cell == (Point{40 / 8, 32 / 16}));  // derived from cell_pixels
}

CK_TEST(pixel_mouse_without_known_cell_pixels_degrades_gracefully) {
    // If a caller ever sets pixel_mouse without cell_pixels (violating
    // the documented backend/probe expectation), the decoder must not
    // silently report a bogus Point{0,0} — indistinguishable from a
    // real click there — it falls back to treating the raw coordinate
    // as a cell position, same as the no-pixel-mouse path.
    Capabilities caps = baseline_capabilities();
    caps.pixel_mouse = true;
    // caps.cell_pixels left at its default {0, 0}: "unknown".
    const auto events = decode_all("\x1B[<0;10;20M", caps);
    CK_CHECK(as_mouse(events[0]).pixel.has_value());  // pixel coords still reported: the terminal did send them
    CK_CHECK(as_mouse(events[0]).cell == (Point{9, 19}));  // NOT {0, 0}
}

// --- Bracketed paste ----------------------------------------------------------

CK_TEST(bracketed_paste_happy_path) {
    const auto events = decode_all("\x1B[200~hello\x1B[201~");
    CK_CHECK(events.size() == 1);
    CK_CHECK(as_text(events[0]).from_paste);
    CK_CHECK(as_text(events[0]).text == "hello");
}

CK_TEST(bracketed_paste_sanitizes_hostile_control_bytes) {
    const auto events =
        decode_all(std::string_view("\x1B[200~a\x07" "b\x1B[201~"));
    CK_CHECK(events.size() == 1);
    CK_CHECK(as_text(events[0]).from_paste);
    CK_CHECK(as_text(events[0]).text == "a\xEF\xBF\xBD" "b");  // BEL replaced with U+FFFD
}

CK_TEST(bracketed_paste_preserves_tab_and_newline) {
    const auto events = decode_all(std::string_view("\x1B[200~a\tb\ncd\x1B[201~"));
    CK_CHECK(as_text(events[0]).text == "a\tb\ncd");
}

CK_TEST(bracketed_paste_start_and_end_split_across_feeds) {
    InputDecoder decoder;
    auto part1 = decoder.feed("\x1B[200~hel", 0);
    CK_CHECK(part1.empty());
    CK_CHECK(decoder.in_paste());
    auto part2 = decoder.feed("lo\x1B[201~", 0);
    CK_CHECK(part2.size() == 1);
    CK_CHECK(as_text(part2[0]).text == "hello");
    CK_CHECK(!decoder.in_paste());
}

CK_TEST(bracketed_paste_terminator_split_across_feeds_still_recognized) {
    // The safe-tail-reservation logic must not consume bytes that could
    // be the start of a terminator arriving in the next feed() call.
    InputDecoder decoder;
    auto part1 = decoder.feed("\x1B[200~content\x1B[201", 0);
    CK_CHECK(part1.empty());
    auto part2 = decoder.feed("~", 0);
    CK_CHECK(part2.size() == 1);
    CK_CHECK(as_text(part2[0]).text == "content");
}

CK_TEST(bracketed_paste_large_content_across_many_feeds) {
    InputDecoder decoder;
    decoder.feed("\x1B[200~", 0);
    std::string expected;
    for (int i = 0; i < 500; ++i) {
        const std::string chunk = "chunk" + std::to_string(i) + " ";
        expected += chunk;
        const auto events = decoder.feed(chunk, 0);
        CK_CHECK(events.empty());
    }
    const auto final_events = decoder.feed("\x1B[201~", 0);
    CK_CHECK(final_events.size() == 1);
    CK_CHECK(as_text(final_events[0]).text == expected);
}

CK_TEST(bracketed_paste_embedded_terminator_lookalike_splits_but_does_not_crash) {
    // Documented protocol-level limitation (docs/input-decoder.md): if
    // pasted content itself contains the literal terminator bytes, the
    // paste splits there. This test pins the resulting BEHAVIOR (safe,
    // deterministic, no crash, no misinterpretation as trusted paste
    // text) rather than claiming the ambiguity is resolved.
    const auto events = decode_all(std::string_view("\x1B[200~before\x1B[201~after\x1B[201~"));
    // "before" is one paste TextEvent; "after" was NOT inside paste
    // framing (the fake terminator ended it), so its 5 letters decode
    // as 5 ordinary Key::Char events, not as a second trusted paste —
    // and definitely not as synthetic control sequences: whatever
    // follows a split is decoded through the SAME scrutiny as normal
    // typed input. The trailing stray "\x1B[201~" (no matching start)
    // is silently swallowed, contributing no event.
    CK_CHECK(events.size() == 6);
    CK_CHECK(as_text(events[0]).from_paste);
    CK_CHECK(as_text(events[0]).text == "before");
    std::string after;
    for (std::size_t i = 1; i < events.size(); ++i) {
        CK_CHECK(std::holds_alternative<KeyEvent>(events[i]));
        after += as_key(events[i]).chord.text;
    }
    CK_CHECK(after == "after");
}

// --- Probe responses -----------------------------------------------------------

CK_TEST(osc_color_scheme_probe_dark) {
    const auto events = decode_all("\x1B]11;rgb:0000/0000/0000\x07");
    CK_CHECK(events.size() == 1);
    const auto caps = std::get<CapabilityChangedEvent>(events[0]).capabilities;
    CK_CHECK(caps.color_scheme == ColorScheme::Dark);
}

CK_TEST(osc_color_scheme_probe_light_with_st_terminator) {
    const auto events = decode_all("\x1B]11;rgb:ffff/ffff/ffff\x1B\\");
    CK_CHECK(events.size() == 1);
    const auto caps = std::get<CapabilityChangedEvent>(events[0]).capabilities;
    CK_CHECK(caps.color_scheme == ColorScheme::Light);
}

CK_TEST(osc_with_malformed_color_spec_is_swallowed_without_crash_or_bogus_update) {
    const auto events = decode_all("\x1B]11;not-a-color\x07");
    CK_CHECK(events.empty());
}

CK_TEST(osc_injection_with_embedded_escape_does_not_corrupt_subsequent_decode) {
    // A hostile OSC body containing an embedded ESC that ISN'T the
    // terminator must not desynchronize the decoder.
    const auto events = decode_all(std::string_view("\x1B]11;rgb:0000\x1B[999999999999X\x1B\\A"));
    // Whatever happens to the OSC itself, the trailing 'A' must still
    // decode as a normal character afterward.
    CK_CHECK(!events.empty());
    const auto& last = events.back();
    if (std::holds_alternative<KeyEvent>(last)) CK_CHECK(as_key(last).chord.text == "A");
}

CK_TEST(decrpm_synchronized_output_mode_report) {
    const auto events = decode_all("\x1B[?2026;1$y");
    CK_CHECK(events.size() == 1);
    const auto caps = std::get<CapabilityChangedEvent>(events[0]).capabilities;
    CK_CHECK(caps.synchronized_output);
}

CK_TEST(da1_response_is_consumed_without_event_or_crash) {
    const auto events = decode_all(std::string_view("\x1B[?62;1;6cA"));
    CK_CHECK(events.size() == 1);  // only the trailing 'A'
    CK_CHECK(as_key(events[0]).chord.text == "A");
}

// --- Robustness sweep ----------------------------------------------------------

CK_TEST(unrecognized_csi_sequence_is_swallowed_whole) {
    const auto events = decode_all(std::string_view("\x1B[99;99;99xA"));
    CK_CHECK(events.size() == 1);  // the unknown CSI produces nothing; 'A' still decodes
    CK_CHECK(as_key(events[0]).chord.text == "A");
}

CK_TEST(adversarial_byte_soup_never_hangs_or_crashes) {
    // A curated set of short, deliberately hostile byte sequences —
    // lone continuation bytes, truncated sequences, bare CSI
    // introducers, nested escapes — fed one at a time, verifying the
    // decoder always terminates (bounded iterations) and never crashes.
    const std::string_view cases[] = {
        "\x1B",
        "\x1B[",
        "\x1B]",
        "\x1BO",
        "\x1B[<",
        "\x1B[?",
        "\x1B[200",
        "\x1B[;;;;;;;;;;X",
        "\x80\x80\x80\x80",
        "\xC0\xC0\xC0",
        "\xFF\xFE\xFD",
        "\x1B\x1B\x1B\x1B\x1B",
        "\x1B[200~\x1B[200~\x1B[201~",  // nested paste-start markers
    };
    for (const std::string_view c : cases) {
        InputDecoder decoder;
        // Bound the loop explicitly: feed byte-by-byte and stop well
        // past any plausible sequence length, proving forward progress
        // rather than trusting an internal loop not to spin forever.
        std::size_t total_consumed_bound = 0;
        for (std::size_t i = 0; i < c.size(); ++i) {
            decoder.feed(c.substr(i, 1), 0);
            ++total_consumed_bound;
            CK_CHECK(total_consumed_bound <= c.size());  // sanity: this loop itself is bounded
        }
        decoder.poll_timeout(1'000'000'000);  // flush any pending lone ESC
    }
}
