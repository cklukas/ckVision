// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/log_mailbox.hpp"
#include "cvision/core/process_runner.hpp"

namespace ckv::core {
// Owned launch and independent stdout/stderr queue policies for one root.
struct ProcessStreamRequest {
    // Explicit invocation, distinct nonzero stream IDs, epoch and fixed capacities.
    ProcessLaunchSpec launch;
    LogSourceId stdout_source = 0, stderr_source = 0;
    std::uint64_t epoch = 1;
    LogMailboxOptions stdout_queue, stderr_queue;
    std::size_t chunk_bytes = 16 * 1024;
};
// Pure rejection category; validation performs no native discovery or launch.
enum class ProcessStreamValidation { Valid, Launch, Identity, Limits };
ProcessStreamValidation validate_process_stream(const ProcessStreamRequest& request) noexcept;
enum class ProcessStreamState { Preparing, Running, Draining, Completed, Cancelled, LaunchFailed, IoFailed };
enum class ProcessStreamFailure { None, Publication, ExecutionException };
// Snapshot of root lifecycle, per-pipe progress and retirement uncertainty.
struct ProcessStreamStatus {
    // State/error/exit remain distinct; counters are cumulative bytes, staging
    // is bounded pending data, and EOF/root-notification flags are observations.
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
    // Retirement releases the session reference without joining its worker.
    virtual ~ProcessStreamSession() = default;
    // Borrow each private output queue for the enclosing session/service life.
    virtual LogMailbox& stdout_mailbox() noexcept = 0;
    virtual LogMailbox& stderr_mailbox() noexcept = 0;
    // Try-copy current observations; false leaves result unchanged on contention.
    virtual bool status(ProcessStreamStatus& result) const noexcept = 0;
    // Request owned-root/group cleanup without waiting for native I/O.
    virtual void cancel() noexcept = 0;
    // Fixed queue/staging/pump workspace. Account separately for service
    // workers, transient launch/environment storage and native pipe buffers.
    virtual std::size_t allocated_bytes() const noexcept = 0;
};
// Admission distinguishes invalid input from temporary worker-capacity refusal.
enum class ProcessStreamOpenState { Opened, Invalid, Busy };
// An Opened result owns a session; rejected results retain their validation cause.
struct ProcessStreamOpenResult {
    // Admission state, structural rejection and optional owned session.
    ProcessStreamOpenState state = ProcessStreamOpenState::Invalid;
    ProcessStreamValidation validation = ProcessStreamValidation::Valid;
    std::unique_ptr<ProcessStreamSession> session;
};
// App-owned execution resources enclosing all sessions and wake/clock use.
class ProcessStreamService {
public:
    // Shutdown waits for owned workers; destroy only outside UI dispatch.
    virtual ~ProcessStreamService() = default;
    // Moves bounded launch data; native I/O occurs only on an owned worker.
    virtual ProcessStreamOpenResult start(ProcessStreamRequest request) = 0;
};
}  // namespace ckv::core
