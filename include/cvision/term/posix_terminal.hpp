// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The POSIX backend (macOS/Linux): raw-mode tty management, a
// poll-based event loop with a self-pipe for cross-thread wakeups,
// per-session resize observation, suspend/resume, and panic-safe restore
// via a signal-safe session-state ledger (ARCHITECTURE.md §4/§9,
// DECISIONS.md D-024). POSIX-only: guarded out of non-UNIX builds by
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
// sanctioned process-level facility (AGENTS.md). Destructing restores
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

    Capabilities capabilities() const noexcept override { return caps_; }
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
    Capabilities caps_;
    InputDecoder decoder_;
    Size last_size_;
    int self_pipe_read_ = -1;
    int self_pipe_write_ = -1;
    std::array<WaitHandle, 2> wait_handles_{};
    std::size_t wait_handle_count_ = 0;
    int session_slot_ = -1;
    bool capability_probes_enabled_ = true;
    std::int64_t probe_deadline_nanos_ = -1;

    void begin_capability_probes();
    // Cell metrics and finite Sixel geometry can both change with a resize.
    // A profile with probing disabled remains fully authoritative; a probing
    // session withdraws runtime evidence before starting the next bounded
    // probe window.
    bool invalidate_resize_dependent_capabilities() noexcept;
    void configure_decoder_capability_update_policy() noexcept;
    void finish_capability_probes();
    void synchronize_sgr_mouse_input_policy() noexcept;
};

}  // namespace ckv::term
