// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "cvision/core/clock.hpp"
#include "cvision/core/process_runner.hpp"
#include "cvision/term/terminal.hpp"

namespace ckv::term::detail {
// Internal endpoint identity and bounded observer-publication outcomes.
enum class ProcessPipe { Stdout, Stderr };
enum class PipePublication { Accepted, Retry, Busy, Cancelled };
// Internal worker-only hook shared by finite and continuous private pipe pumps.
class ProcessPumpObserver {
public:
    // Observer lifetime encloses the pump call; it never owns a view.
    virtual ~ProcessPumpObserver() = default;
    // Cancellation, chunk bound and publication are nonblocking observations.
    // Retry waits for capacity; Busy yields without consuming producer bytes.
    virtual bool cancelled() const noexcept = 0;
    virtual std::size_t chunk_bytes() const noexcept = 0;
    virtual PipePublication publish(ProcessPipe pipe, std::string_view bytes) noexcept = 0;
    // Borrow/consume native cancellation or capacity readiness.
    virtual WaitHandle wake_handle() const noexcept = 0;
    virtual void consume_wake() noexcept = 0;
    // Report root identity/readiness, actual exit and independent pipe closure.
    // tail_unverified distinguishes forced closure from an observed EOF.
    virtual void started(std::uint64_t pid, bool root_notifications) noexcept = 0;
    virtual void root_exited(core::ProcessExitStatus exit) noexcept = 0;
    virtual void pipe_closed(ProcessPipe pipe, bool tail_unverified) noexcept = 0;
};
// Finite pump result plus cancellation and unverified trailing-byte closure.
struct ProcessPumpResult {
    // Captured root/pipe result and cleanup observations.
    core::ProcessRunResult run;
    bool cancelled = false, tail_unverified = false;
};
// Run owned private pipes synchronously on a worker, using the injected clock.
ProcessPumpResult run_private_process(const core::ProcessRunRequest& request,
                                     const Clock& clock, ProcessPumpObserver* observer = nullptr);
}  // namespace ckv::term::detail
