// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/clock.hpp"
#include "cvision/core/process_runner.hpp"

namespace ckv::term {

// Captured binary helper execution through private pipes, never a PTY/ConPTY.
// The borrowed clock must outlive this runner and advance during real waits.
// Each call owns its process group/job and has no persistent I/O thread.
class NativeProcessRunner final : public core::ProcessRunner {
public:
    // Explicitly inject the host's monotonic clock; no global time source.
    explicit NativeProcessRunner(const Clock& clock) noexcept : clock_(clock) {}
    // Synchronous bounded-pump execution; input is borrowed only for this call.
    core::ProcessRunResult run(const core::ProcessRunRequest& request) override;

private:
    const Clock& clock_;
};

} // namespace ckv::term
