// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Terminal input decoding under arbitrary bytes, with the WP-29 paste
// invariants of docs/input-decoder.md checked on every event:
//
// - only a bracketed paste arrives as a TextEvent;
// - paste text is well-formed UTF-8 holding no control but tab and line feed:
//   no other C0, no DEL, no C1 (D-040);
// - bytes framed by a paste that arrive before any quiet period expires are
//   paste text and nothing else, whatever markers, chords, replies or escape
//   fragments they contain, so no key, mouse, focus or capability event (and
//   therefore no command) can be synthesized from them;
// - a definite disconnect always ends a paste.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "cvision/core/utf8.hpp"
#include "cvision/term/input_decoder.hpp"
#include "fuzz_common.hpp"

namespace {

void require_sanitized_paste_text(std::string_view text) {
    ckv::fuzz::require(ckv::utf8::is_valid(text));
    for (std::size_t pos = 0; pos < text.size();) {
        const char32_t cp = ckv::utf8::decode(text, pos);
        const bool tab_or_line_feed = cp == U'\t' || cp == U'\n';
        ckv::fuzz::require(tab_or_line_feed || (cp > 0x1F && cp != 0x7F && (cp < 0x80 || cp > 0x9F)));
    }
}

// What every event the decoder delivers must satisfy, for any input.
void require_event_invariants(const ckv::term::TerminalEvent& event) {
    const auto* text = std::get_if<ckv::TextEvent>(&event);
    if (text == nullptr) return;
    ckv::fuzz::require(text->from_paste);
    require_sanitized_paste_text(text->text);
}

// Feeds `input` in fragments whose sizes the input itself chooses, advancing
// the injected clock by `step_nanos` per fragment, and checks every event.
std::vector<ckv::term::TerminalEvent> feed_fragmented(ckv::term::InputDecoder& decoder, std::string_view input,
                                                      std::int64_t step_nanos, std::int64_t& now) {
    std::vector<ckv::term::TerminalEvent> all;
    std::size_t offset = 0;
    while (offset < input.size()) {
        const std::size_t chunk = 1 + (static_cast<unsigned char>(input[offset]) % 17U);
        const std::size_t count = std::min(chunk, input.size() - offset);
        std::vector<ckv::term::TerminalEvent> events = decoder.feed(input.substr(offset, count), now);
        ckv::fuzz::require(events.size() <= input.size() + 1U);
        for (ckv::term::TerminalEvent& event : events) {
            require_event_invariants(event);
            all.push_back(std::move(event));
        }
        offset += count;
        now += step_nanos;
    }
    return all;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);

    // The raw stream, a millisecond per fragment: quiet periods can expire
    // mid-stream, so later bytes may legitimately decode as ordinary input.
    {
        ckv::term::InputDecoder decoder;
        std::int64_t now = 0;
        (void)feed_fragmented(decoder, input, 1'000'000, now);
        for (const ckv::term::TerminalEvent& event :
             decoder.poll_timeout(now + ckv::term::kPasteTerminationQuietNanos + 1))
            require_event_invariants(event);
        for (const ckv::term::TerminalEvent& event : decoder.abort_paste()) require_event_invariants(event);
        ckv::fuzz::require(!decoder.in_paste());
    }

    // The same bytes framed as one paste and arriving within its quiet
    // period: the final end marker is the only one that may close it, so
    // everything the decoder delivers is paste text.
    {
        ckv::term::InputDecoder decoder;
        std::int64_t now = 0;
        const std::string framed = "\x1B[200~" + input + "\x1B[201~";
        std::vector<ckv::term::TerminalEvent> events = feed_fragmented(decoder, framed, 0, now);
        for (ckv::term::TerminalEvent& event : decoder.poll_timeout(now + ckv::term::kPasteTerminationQuietNanos)) {
            require_event_invariants(event);
            events.push_back(std::move(event));
        }
        ckv::fuzz::require(!decoder.in_paste());
        for (const ckv::term::TerminalEvent& event : events)
            ckv::fuzz::require(std::holds_alternative<ckv::TextEvent>(event));
    }
    return 0;
}
