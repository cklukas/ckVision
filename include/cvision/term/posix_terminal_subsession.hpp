// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// POSIX private-PTY adapter for the D-042 embedded-terminal boundary.
#pragma once

#if !defined(_WIN32)

#include <array>
#include <memory>
#include <optional>

#include "cvision/term/terminal_emulator.hpp"

namespace ckv::term {

class PosixTerminalSubsession final : public TerminalSubsession {
public:
    // A launch failure is represented as a session in Failed state so callers
    // can surface it in TerminalView without exceptions escaping the event loop.
    static std::unique_ptr<PosixTerminalSubsession> launch(TerminalLaunchSpec spec,
                                                             TerminalSubsessionOptions options = {});
    ~PosixTerminalSubsession() override;

    PosixTerminalSubsession(const PosixTerminalSubsession&) = delete;
    PosixTerminalSubsession& operator=(const PosixTerminalSubsession&) = delete;

    TerminalSnapshot snapshot() const override { return emulator_.snapshot(); }
    TerminalStatus status() const override { return emulator_.status(); }
    // The no-copy path, forwarded rather than reached through the emulator,
    // so a host that owns one of these never has to know it embeds one. This
    // is the seam a multiplexer's server actually holds (it links only core
    // and term), and the whole point of U0-b is that reading a terminal at
    // tick rate must not copy what that terminal remembers.
    TerminalSnapshot snapshot(TerminalSnapshotOptions options) const {
        return emulator_.snapshot(options);
    }
    std::span<const Cell> cells() const noexcept override { return emulator_.cells(); }
    std::span<const Cell> scrollback() const noexcept override { return emulator_.scrollback(); }
    std::span<const TerminalRaster> rasters() const noexcept override { return emulator_.rasters(); }
    std::span<const TerminalDiagnostic> diagnostics() const noexcept override {
        return emulator_.diagnostics();
    }
    // Drained, not borrowed: the emulator hands each finished job over once
    // and forgets it (core::TerminalSubsession::take_printer_jobs).
    std::vector<TerminalPrinterJob> take_printer_jobs() override {
        return emulator_.take_printer_jobs();
    }
    void set_printer_policy(TerminalPrinterPolicy policy) override {
        emulator_.set_printer_policy(policy);
    }
    void set_printer_spool_limit(std::size_t bytes) override {
        emulator_.set_printer_spool_limit(bytes);
    }
    const TerminalDamage& damage() const noexcept override { return emulator_.damage(); }
    void clear_damage() noexcept override { emulator_.clear_damage(); }
    bool synchronized_output_active() const noexcept override {
        return emulator_.synchronized_output_active();
    }
    const TerminalCapabilityProfile& profile() const noexcept override { return emulator_.profile(); }
    void feed_output(std::string_view bytes) override { emulator_.feed_output(bytes); }
    void set_raster_identity(int identity) noexcept override { emulator_.set_raster_identity(identity); }
    void resize(Size cells, Size cell_pixels) override;
    void send_input(std::string_view bytes) override;
    std::string take_pending_input() override { return emulator_.take_pending_input(); }
    TerminalSubsessionState state() const noexcept override { return emulator_.state(); }
    std::span<const WaitHandle> wait_handles() const noexcept override {
        return std::span<const WaitHandle>(wait_handles_.data(), wait_handle_count_);
    }

    // Called by the owning application's readiness step.  It consumes at most
    // `byte_budget`, preserving fairness with outer input and painting.
    bool drain(std::size_t byte_budget) override;

    // Asks the child to end, and returns immediately.
    //
    // `close()` decides for itself how long to wait and then escalates; this is
    // the half a host needs when the waiting is ITS decision — a multiplexer
    // killing a session tells every child to go, keeps drawing while they do,
    // and escalates on its own schedule. Doing that with `close()` would mean
    // blocking the loop once per terminal, and doing it without any primitive
    // at all would mean a host reaching for the pid, which is exactly what this
    // class exists to own.
    //
    // SIGHUP then SIGTERM, to the process group, exactly as `close()` sends
    // them; the PTY stays open so a child blocked writing can finish and see
    // them (the wedge of a177a95). Safe to call more than once, and safe to
    // call before `close()` — which is the ordinary way it is used.
    void request_termination() noexcept;

    // Ends the child now, and returns immediately.
    //
    // The other end of the same decision `request_termination()` hands a host:
    // once the grace a host timed itself has run out, something has to escalate,
    // and `close()` escalates by WAITING — up to three seconds per terminal
    // while it reaps, which is exactly what a host with its own loop and its own
    // clock must not spend. A host ending ten terminals that ignore SIGTERM pays
    // that ten times over, with everything else it owns frozen behind it.
    //
    // SIGKILL, to the process group, exactly as `close()` sends it; the PTY
    // stays open, because a child wedged writing to its terminal cannot be
    // reaped until somebody empties it, and the emptying is the host's `drain()`
    // as it always was. The exit is observed the same way any other is. Safe to
    // call more than once, safe before `close()`, and safe for a child that has
    // already gone.
    void request_kill() noexcept;
    void close() noexcept override;
    int file_descriptor() const noexcept { return master_fd_; }
    // The spawned child's pid, for OBSERVATION — and -1 once the child's exit
    // has been observed, or when the launch failed. A host measuring what the
    // process tree under a terminal costs (CPU time, resident memory) needs to
    // know where that tree is rooted, and enumeration and sampling are
    // platform work no wrapper here could own.
    //
    // It is NOT a handle for control. Hosts end a child through
    // `request_termination()`/`request_kill()`/`close()`, which own the
    // process-group semantics and the PTY's part in the sequence; a host that
    // signalled this pid itself would race the reaping this class does.
    // Nothing about that rule is changed by the pid being readable — and a
    // reader sampling it should remember a pid outlives its process only as a
    // number, so a stale copy may name somebody else's process.
    //
    // An override since the seam grew the question (U5-b): a host holding a
    // `core::TerminalSubsession&` may ask uniformly, and the sessions that
    // hold no process — a mirror, a fake — answer -1 by the base's default.
    int process_id() const noexcept override { return child_pid_; }
    // The status the child exited with, once it has. Forwarded because the
    // emulator is the thing the PTY's read boundary was reported to, and a
    // host that owns one of these should not have to reach through it to find
    // out how the program it started ended — a multiplexer's exit banner is
    // exactly that question ("exited 1" is a different window from "exited
    // 0"), and without this it could only say "gone".
    std::optional<int> exit_code() const noexcept { return emulator_.exit_code(); }

private:
    PosixTerminalSubsession(TerminalLaunchSpec spec, TerminalSubsessionOptions options);
    bool spawn();
    void observe_exit();

    TerminalLaunchSpec spec_;
    TerminalSubsessionOptions options_;
    TerminalEmulator emulator_;
    int master_fd_ = -1;
    int child_pid_ = -1;
    std::array<WaitHandle, 1> wait_handles_{};
    std::size_t wait_handle_count_ = 0;
    bool closed_ = false;
};

}  // namespace ckv::term

#endif
