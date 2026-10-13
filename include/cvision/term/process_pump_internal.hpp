// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "cvision/core/clock.hpp"
#include "cvision/core/process_runner.hpp"
#include "cvision/term/terminal.hpp"

namespace ckv::term::detail {
enum class ProcessPipe { Stdout, Stderr };
enum class PipePublication { Accepted, Retry, Busy, Cancelled };
class ProcessPumpObserver {
public:
    virtual ~ProcessPumpObserver() = default;
    virtual bool cancelled() const noexcept = 0;
    virtual std::size_t chunk_bytes() const noexcept = 0;
    virtual PipePublication publish(ProcessPipe pipe, std::string_view bytes) noexcept = 0;
    virtual WaitHandle wake_handle() const noexcept = 0;
    virtual void consume_wake() noexcept = 0;
    virtual void started(std::uint64_t pid, bool root_notifications) noexcept = 0;
    virtual void root_exited(core::ProcessExitStatus exit) noexcept = 0;
    virtual void pipe_closed(ProcessPipe pipe, bool tail_unverified) noexcept = 0;
};
struct ProcessPumpResult {
    core::ProcessRunResult run;
    bool cancelled = false, tail_unverified = false;
};
ProcessPumpResult run_private_process(const core::ProcessRunRequest& request,
                                     const Clock& clock, ProcessPumpObserver* observer = nullptr);
}  // namespace ckv::term::detail
