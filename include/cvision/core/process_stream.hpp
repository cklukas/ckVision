// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/log_mailbox.hpp"
#include "cvision/core/process_runner.hpp"

namespace ckv::core {
struct ProcessStreamRequest {
    ProcessLaunchSpec launch;
    LogSourceId stdout_source = 0, stderr_source = 0;
    std::uint64_t epoch = 1;
    LogMailboxOptions stdout_queue, stderr_queue;
    std::size_t chunk_bytes = 16 * 1024;
};
enum class ProcessStreamValidation { Valid, Launch, Identity, Limits };
ProcessStreamValidation validate_process_stream(const ProcessStreamRequest& request) noexcept;
enum class ProcessStreamState { Preparing, Running, Draining, Completed, Cancelled, LaunchFailed, IoFailed };
enum class ProcessStreamFailure { None, Publication, ExecutionException };
struct ProcessStreamStatus {
    ProcessStreamState state = ProcessStreamState::Preparing;
    ProcessNativeError error;
    ProcessStreamFailure failure = ProcessStreamFailure::None;
    std::optional<ProcessExitStatus> exit;
    std::uint64_t revision = 0, root_process_id = 0, stdout_bytes = 0, stderr_bytes = 0;
    std::size_t stdout_pending_bytes = 0, stderr_pending_bytes = 0;
    std::uint64_t retired_staging_bytes = 0;
    bool root_notifications = false, tail_unverified = false, stdout_eof = false, stderr_eof = false;
};

// Queue state stays alive until both the session and active worker release it.
// Cancel/retirement never joins. Clock/wake/service ownership is explicit at
// native construction; queue use and wake targets must have enclosing lives.
class ProcessStreamSession {
public:
    virtual ~ProcessStreamSession() = default;
    virtual LogMailbox& stdout_mailbox() noexcept = 0;
    virtual LogMailbox& stderr_mailbox() noexcept = 0;
    virtual bool status(ProcessStreamStatus& result) const noexcept = 0;
    virtual void cancel() noexcept = 0;
    // Fixed queue/staging/pump workspace. Account separately for service
    // workers, transient launch/environment storage and native pipe buffers.
    virtual std::size_t allocated_bytes() const noexcept = 0;
};
enum class ProcessStreamOpenState { Opened, Invalid, Busy };
struct ProcessStreamOpenResult {
    ProcessStreamOpenState state = ProcessStreamOpenState::Invalid;
    ProcessStreamValidation validation = ProcessStreamValidation::Valid;
    std::unique_ptr<ProcessStreamSession> session;
};
class ProcessStreamService {
public:
    virtual ~ProcessStreamService() = default;
    // Moves bounded launch data; native I/O occurs only on an owned worker.
    virtual ProcessStreamOpenResult start(ProcessStreamRequest request) = 0;
};
}  // namespace ckv::core
