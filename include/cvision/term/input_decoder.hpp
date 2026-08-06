// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The incremental byte-to-event state machine (ARCHITECTURE.md §4).
// Coverage and every scope decision are documented in
// docs/input-decoder.md — read that file before changing this one.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/term/capabilities.hpp"
#include "cvision/term/terminal.hpp"

namespace ckv::term {

// The quiet deadline (docs/input-decoder.md) used to resolve a lone ESC
// press versus the start of a longer escape sequence, on legacy
// keyboard-protocol profiles only.
inline constexpr std::int64_t kEscTimeoutNanos = 50'000'000;  // 50 ms

class InputDecoder {
public:
    explicit InputDecoder(Capabilities initial_caps = baseline_capabilities())
        : caps_(initial_caps) {}

    // Feeds newly received bytes and returns however many complete
    // events could be decoded from them plus any previously buffered
    // partial input. `now_nanos` is the injected Clock's current time,
    // used solely for the ESC quiet-deadline check.
    std::vector<TerminalEvent> feed(std::string_view data, std::int64_t now_nanos);

    // Call periodically even when no bytes have arrived: resolves a
    // pending lone ESC once its quiet deadline has passed. Returns an
    // empty vector when there is nothing to resolve yet.
    std::vector<TerminalEvent> poll_timeout(std::int64_t now_nanos);

    void set_capabilities(Capabilities caps) noexcept { caps_ = caps; }
    const Capabilities& capabilities() const noexcept { return caps_; }

    // True while a bracketed paste is being accumulated — exposed for
    // tests and diagnostics, not part of the normal control flow.
    bool in_paste() const noexcept { return in_paste_; }

private:
    enum class Status { Incomplete, Invalid, Complete };
    struct ParseResult {
        Status status = Status::Invalid;
        std::size_t consumed = 0;
        std::optional<TerminalEvent> event;
        std::optional<Capabilities> updated_caps;
    };

    std::vector<TerminalEvent> drain(std::int64_t now_nanos);
    ParseResult try_parse_one(std::string_view buf, std::int64_t now_nanos);
    ParseResult parse_escape(std::string_view buf, std::int64_t now_nanos);
    ParseResult parse_csi(std::string_view buf);
    ParseResult parse_ss3(std::string_view buf);
    ParseResult parse_osc(std::string_view buf);
    ParseResult parse_paste_continuation(std::string_view buf);
    ParseResult parse_utf8_or_control(std::string_view buf);

    std::string pending_;
    std::int64_t esc_first_seen_nanos_ = -1;
    bool in_paste_ = false;
    std::string paste_accum_;
    Capabilities caps_;
};

}  // namespace ckv::term
