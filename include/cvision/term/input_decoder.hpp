// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The incremental byte-to-event state machine (the architecture §4).
// Coverage and every scope decision are documented in
// docs/input-decoder.md — read that file before changing this one.
#pragma once

#include <cstdint>
#include <optional>
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

// A bracketed-paste end marker is in-band and can occur literally in pasted
// bytes. A candidate marker becomes authoritative only after this quiet
// interval. Bytes received before it expires remain sanitized paste text,
// never ordinary key/command input. See docs/input-decoder.md and D-040.
inline constexpr std::int64_t kPasteTerminationQuietNanos = 50'000'000;  // 50 ms

// The backend owns whether a raw terminal capability reply is currently
// authoritative. Keeping this admission decision inside the decoder prevents
// a rejected reply from changing decode state for later bytes in the same OS
// read (for example, a late pixel-mode reply followed by an SGR report).
enum class CapabilityUpdatePolicy {
    // A bounded probe window is open: solicited replies refine freely.
    AcceptProbeRefinements,
    // A probing session between windows: geometry measurements, color-
    // scheme changes on a session that verified mode 2031, and kitty
    // enhancement-flag readbacks on a session that verified the protocol —
    // each an update whose authority was established inside a window and
    // whose late arrival is part of its own protocol.
    AcceptVerifiedLiveRefinements,
    // An explicit capability contract (probes disabled): nothing but
    // geometry measurements moves.
    Reject,
};

class InputDecoder {
public:
    explicit InputDecoder(Capabilities initial_caps = baseline_capabilities())
        : caps_(initial_caps), pixel_mouse_mode_enabled_(initial_caps.pixel_mouse) {}

    // Feeds newly received bytes and returns however many complete
    // events could be decoded from them plus any previously buffered
    // partial input. `now_nanos` is the injected Clock's current time,
    // used solely for the ESC quiet-deadline check.
    std::vector<TerminalEvent> feed(std::string_view data, std::int64_t now_nanos);

    // Call periodically even when no bytes have arrived: resolves a
    // pending lone ESC once its quiet deadline has passed. Returns an
    // empty vector when there is nothing to resolve yet.
    std::vector<TerminalEvent> poll_timeout(std::int64_t now_nanos);

    // Ends an interrupted paste without reinterpreting already-received bytes
    // as keyboard input. A terminal backend calls this on a definite input
    // disconnect; tests and deterministic hosts may use it to model one.
    std::vector<TerminalEvent> abort_paste();

    // The earliest decoder-owned deadline a backend must include in its wait.
    // Currently this is the guarded bracketed-paste candidate; nullopt means
    // no decoder state needs a timed wakeup.
    std::optional<std::int64_t> next_timeout_nanos() const noexcept;

    void set_capabilities(Capabilities caps) noexcept {
        caps_ = caps;
        pixel_mouse_mode_enabled_ = caps.pixel_mouse;
        color_scheme_from_osc11_ = false;
    }
    void set_capability_update_policy(CapabilityUpdatePolicy policy) noexcept {
        capability_update_policy_ = policy;
    }
    // A backend that has enabled SGR-pixel mode but has not yet established
    // its cell metric must consume SGR reports without delivering them as
    // ordinary cell coordinates. This is a temporary protocol boundary, not
    // a capability update policy: it prevents precision from being guessed
    // while allowing complete probe replies in the same byte stream to make
    // later reports deliverable.
    void set_sgr_mouse_input_suppressed(bool suppressed) noexcept {
        sgr_mouse_input_suppressed_ = suppressed;
    }
    // The terminal's current cell grid (columns, rows). It is the
    // discriminator between cell and pixel SGR coordinates: a 1-based cell
    // report can never exceed the grid the terminal itself renders, so a
    // report beyond it is positive evidence of pixel coordinates — evidence
    // that outranks a DECRPM answer, because terminals exist (iTerm2) that
    // report mode 1016 as reset yet engage it anyway. It is also the divisor
    // that turns an XTWINOPS 14 text-area reply into a cell metric when
    // XTWINOPS 16 goes unanswered. The backend keeps it current across
    // resizes; {0,0} disables both uses.
    void set_cell_grid(Size grid) noexcept { cell_grid_ = grid; }
    // How many SGR mouse reports this decoder has recognized in the byte
    // stream, counted before any policy can consume them. Compared against
    // the events an application actually dispatches, it separates "the
    // terminal never sent one" from "something between here and there
    // dropped it" — two failures that look identical from the UI.
    std::size_t mouse_reports_seen() const noexcept { return mouse_reports_seen_; }
    // How many Device Status Report replies (`CSI 0 n`) this decoder has
    // recognized. A terminal answers one only after it has consumed every
    // byte written before the question, so a backend that writes the query
    // at the end of a frame learns from this counter that the frame has
    // been taken in — the one thing an application otherwise cannot know,
    // because writing a frame reports only that the bytes left this side.
    std::size_t frame_acknowledgements() const noexcept { return frame_acknowledgements_; }
    // A resize invalidates a runtime XTSMGRAPHICS geometry limit. Until a
    // fresh, finite geometry reply arrives, a DA1 advertisement alone must
    // not re-enable raster output: its old limit may have been reduced by
    // the new terminal window.
    void require_verified_sixel_geometry(bool required) noexcept {
        sixel_geometry_required_ = required;
    }
    // Starts a backend capability-probe window. If the previous read ended
    // midway through one terminal sequence, preserve those bytes for normal
    // input recovery but never let their eventual completion refine the new
    // window's capabilities: terminal replies carry no request identifier.
    void begin_capability_probe_window(Capabilities caps) noexcept {
        set_capabilities(caps);
        capability_update_policy_ = CapabilityUpdatePolicy::AcceptProbeRefinements;
        discard_capability_update_from_pending_sequence_ = !pending_.empty();
    }
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
    ParseResult parse_paste_continuation(std::string_view buf, std::int64_t now_nanos);
    ParseResult parse_utf8_or_control(std::string_view buf);
    bool accepts_capability_update(const Capabilities& candidate) const noexcept;
    std::optional<TerminalEvent> finish_paste_if_quiet(std::int64_t now_nanos);
    std::optional<TerminalEvent> abort_active_paste();

    std::string pending_;
    std::int64_t esc_first_seen_nanos_ = -1;
    bool in_paste_ = false;
    bool discarding_unnegotiated_paste_ = false;
    std::string paste_accum_;
    // The most recent end marker is held out of paste_accum_ until its quiet
    // period proves it is final. Bytes after it are kept separately: if a
    // newer marker arrives, the old one becomes literal sanitized payload;
    // if quiet wins, the old marker is discarded and the tail is delivered as
    // recovered paste rather than as ordinary input.
    bool paste_end_candidate_ = false;
    std::int64_t paste_end_candidate_nanos_ = -1;
    std::string paste_candidate_tail_;
    bool paste_recovered_ = false;
    Capabilities caps_;
    // DECRPM 1016 and XTWINOPS 16 can arrive in either order. Keep the
    // reported mode separately until a positive cell-size reply makes pixel
    // coordinates usable by callers.
    bool pixel_mouse_mode_enabled_ = false;
    // `drain` stops only at one incomplete sequence, so at a probe-window
    // boundary any pending bytes belong to exactly that pre-boundary sequence.
    // The next completed sequence consumes this marker.
    bool discard_capability_update_from_pending_sequence_ = false;
    // OSC 10 is only a contrast-based fallback. A valid OSC 11 background
    // report is direct evidence and must win even if replies are reordered.
    bool color_scheme_from_osc11_ = false;
    bool sgr_mouse_input_suppressed_ = false;
    bool sixel_geometry_required_ = false;
    Size cell_grid_{0, 0};
    std::size_t mouse_reports_seen_ = 0;
    std::size_t frame_acknowledgements_ = 0;
    CapabilityUpdatePolicy capability_update_policy_ = CapabilityUpdatePolicy::AcceptProbeRefinements;
};

}  // namespace ckv::term
