// Copyright (c) 2026 C. Klukas. All rights reserved.
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
    const TerminalCapabilityProfile& profile() const noexcept override { return emulator_.profile(); }
    void feed_output(std::string_view bytes) override { emulator_.feed_output(bytes); }
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
    void close() noexcept override;
    int file_descriptor() const noexcept { return master_fd_; }

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
