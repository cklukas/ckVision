// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The POSIX backend (macOS/Linux): raw-mode tty management, a
// poll-based event loop with a self-pipe for cross-thread wakeups,
// SIGWINCH resize coalescing, suspend/resume, and panic-safe restore
// via a signal-safe session-state ledger (ARCHITECTURE.md §4/§9,
// DECISIONS.md D-024). POSIX-only: guarded out of non-UNIX builds by
// CMakeLists.txt.
#pragma once

#include <string>

#include "cvision/term/input_decoder.hpp"
#include "cvision/term/terminal.hpp"

namespace ckv::term {

// Constructing enters raw mode and the alternate screen on `fd`
// (defaults to the process's own stdin/stdout) and registers this
// session in the async-signal-safe restore registry (D-024) — the sole
// sanctioned process-level facility (AGENTS.md). Destructing restores
// everything and deregisters. Non-copyable, non-movable: it owns OS
// resources and a fixed slot in that registry.
class PosixTerminal final : public Terminal {
public:
    explicit PosixTerminal(int output_fd = 1, int input_fd = 0,
                            Capabilities initial_caps = baseline_capabilities());
    ~PosixTerminal() override;

    PosixTerminal(const PosixTerminal&) = delete;
    PosixTerminal& operator=(const PosixTerminal&) = delete;
    PosixTerminal(PosixTerminal&&) = delete;
    PosixTerminal& operator=(PosixTerminal&&) = delete;

    Capabilities capabilities() const noexcept override { return caps_; }
    Size size() const noexcept override;

    // Blocks on the input fd (and the internal self-pipe, used to wake
    // a blocked poll() from another thread or a signal handler) up to
    // `deadline_nanos` on CLOCK_MONOTONIC. Also resolves a pending lone
    // ESC and delivers a coalesced resize once per call if SIGWINCH
    // fired since the last poll().
    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos) override;

    void write(std::string_view bytes) override;
    void set_title(std::string_view title) override;
    void bell() override;
    void write_clipboard(std::string_view text) override;  // OSC 52

    // Wakes a blocked poll() from any thread (writes one byte to the
    // self-pipe) — the mechanism `Application::post` (M4+) will use.
    void wake() noexcept;

    // CLOCK_MONOTONIC nanoseconds — the epoch poll()'s deadline_nanos
    // is measured against. Exposed so callers (and tests) can
    // construct valid deadlines without duplicating the clock call.
    static std::int64_t monotonic_now_nanos() noexcept;

private:
    int output_fd_;
    int input_fd_;
    Capabilities caps_;
    InputDecoder decoder_;
    int self_pipe_read_ = -1;
    int self_pipe_write_ = -1;
    int session_slot_ = -1;
};

}  // namespace ckv::term
