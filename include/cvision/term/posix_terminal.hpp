// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The POSIX backend (macOS/Linux): raw-mode tty management, a
// poll-based event loop with a self-pipe for cross-thread wakeups,
// per-session resize observation, suspend/resume, and panic-safe restore
// via a signal-safe session-state ledger (the architecture §4/§9,
// The decision log D-024). POSIX-only: guarded out of non-UNIX builds by
// CMakeLists.txt.
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "cvision/core/clock.hpp"
#include "cvision/term/input_decoder.hpp"
#include "cvision/term/terminal.hpp"

namespace ckv::term {

// Constructing enters raw mode and the alternate screen on `fd`
// (defaults to the process's own stdin/stdout) and registers this
// session in the async-signal-safe restore registry (D-024) — the sole
// sanctioned process-level facility (the engineering standard). Destructing restores
// everything and deregisters. SIGTSTP restores every registered session;
// SIGCONT re-enters each session and makes its next poll report a capability
// change so Application fully re-presents the frame. Non-copyable,
// non-movable: it owns OS resources and a fixed slot in that registry.
class PosixTerminal final : public Terminal {
public:
    // `initial_caps` is the baseline rendered before any response arrives.
    // When `enable_capability_probes` is true, OSC 10/11, DECRQM 2026/1016/2031,
    // DA1, and XTWINOPS 16 responses may refine it during the bounded probe
    // window; mode 2031 remains enabled for positively verified live
    // color-scheme notifications. False leaves the supplied profile
    // authoritative for the session.
    // `clock` is the same monotonic source used for poll deadlines and by
    // the owning Application. It bounds probe evidence deterministically;
    // it must outlive this terminal session.
    explicit PosixTerminal(const Clock& clock, int output_fd = 1, int input_fd = 0,
                            Capabilities initial_caps = baseline_capabilities(),
                            bool enable_capability_probes = true);

    // Selects one of the named conservative host profiles without consulting
    // environment state. Curated profiles default probes off so their explicit
    // contract remains authoritative; hosts can opt in where they have a
    // documented probe policy.
    PosixTerminal(const Clock& clock, int output_fd, int input_fd, TerminalProfile profile,
                  bool enable_capability_probes = false)
        : PosixTerminal(clock, output_fd, input_fd, capabilities_for_profile(profile), enable_capability_probes) {}
    ~PosixTerminal() override;

    PosixTerminal(const PosixTerminal&) = delete;
    PosixTerminal& operator=(const PosixTerminal&) = delete;
    PosixTerminal(PosixTerminal&&) = delete;
    PosixTerminal& operator=(PosixTerminal&&) = delete;

    // Diagnostic: SGR mouse reports recognized in the input stream.
    std::size_t mouse_reports_seen() const noexcept { return decoder_.mouse_reports_seen(); }

    Capabilities capabilities() const noexcept override { return caps_; }
    std::size_t frame_acknowledgements() const noexcept override {
        return decoder_.frame_acknowledgements();
    }
    // Replaces the explicit client policy layered over probe evidence. A
    // meaningful effective-capability change is delivered through the next
    // poll() as CapabilityChangedEvent and wakes an application blocked in
    // that poll, so callers never need to manufacture a redraw themselves.
    // Fixed cell metrics and color-register caps must be positive.
    void set_capability_overrides(CapabilityOverrides overrides);
    const CapabilityOverrides& capability_overrides() const noexcept { return overrides_; }
    Size size() const noexcept override;
    std::span<const WaitHandle> wait_handles() const noexcept override {
        return std::span<const WaitHandle>(wait_handles_.data(), wait_handle_count_);
    }

    // Blocks on the input fd and the internal self-pipe, which wakes a
    // blocked poll() from another thread, up to
    // `deadline_nanos` on the supplied Clock. Also resolves a pending lone
    // ESC and delivers a coalesced resize when this terminal's own size
    // differs from the size observed by its previous poll(). SIGWINCH
    // merely interrupts a blocked poll promptly; it never routes a resize
    // intended for one terminal session into another one.
    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos) override;
    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos,
                                    std::span<const WaitHandle> additional_wait_handles) override;

    void restore() noexcept override;
    void write_diagnostic_after_restore(std::string_view message) noexcept override;
    [[noreturn]] void terminate_after_callback_failure() noexcept override;
    void write(std::string_view bytes) override;
    void set_title(std::string_view title) override;
    void bell() override;
    void write_clipboard(std::string_view text) override;  // OSC 52

    // Wakes a blocked poll() from any thread (writes one byte to the
    // self-pipe) — Terminal's Application::post()/wake() contract.
    void wake() noexcept override;

private:
    const Clock& clock_;
    int output_fd_;
    int input_fd_;
    Capabilities observed_caps_;
    Capabilities caps_;
    CapabilityOverrides overrides_;
    InputDecoder decoder_;
    Size last_size_;
    int self_pipe_read_ = -1;
    int self_pipe_write_ = -1;
    std::array<WaitHandle, 2> wait_handles_{};
    std::size_t wait_handle_count_ = 0;
    int session_slot_ = -1;
    bool capability_probes_enabled_ = true;
    std::int64_t probe_deadline_nanos_ = -1;
    // Kitty keyboard enhancement negotiation (D-055). The set in force is
    // always what the host's CSI ? u readback said; these record only what
    // this side did — the stated profile contract, whether one of our own
    // pushes sits on the host's stack (the only entry a later step may
    // pop), and whether the one-time request and one-time demotion ran.
    int profile_kitty_flags_ = 0;
    bool kitty_push_active_ = false;
    bool kitty_flags_negotiated_ = false;
    bool kitty_demoted_ = false;
    // Sixel support this backend took away while a resize re-probe was in
    // flight, so that a probe window closing without the fresh maximum
    // geometry can give it back instead of leaving the terminal dark.
    bool withheld_sixel_graphics_ = false;
    // Raw output capture (CKVISION_OUTPUT_CAPTURE), for replaying what a
    // misbehaving host was actually given.
    std::FILE* capture_stream_ = nullptr;
    bool capture_attempted_ = false;
    bool capability_change_pending_ = false;

    void begin_capability_probes();
    // Cell metrics and finite Sixel geometry can both change with a resize.
    // A profile with probing disabled remains fully authoritative; a probing
    // session withdraws runtime evidence before starting the next bounded
    // probe window.
    bool invalidate_resize_dependent_capabilities() noexcept;
    bool update_effective_capabilities() noexcept;
    // TIOCGWINSZ's ws_xpixel/ws_ypixel; {0,0} when the terminal leaves them unset.
    Size window_pixel_size() const noexcept;
    void configure_decoder_capability_update_policy() noexcept;
    void finish_capability_probes();
    void synchronize_sgr_mouse_input_policy() noexcept;
    // One-time full-enhancement request once the kitty protocol is proven,
    // and the one-time step back to the baseline for a host that honours
    // all-keys-as-escape-codes without associated text (D-055).
    void negotiate_kitty_enhancements();
    void maybe_demote_kitty_keyboard();
};

}  // namespace ckv::term
