// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "cvision/core/process_launch.hpp"

namespace ckv::core {

// A mandatory choice about the owned group/job after successful root completion.
enum class ProcessDescendantPolicy { Unspecified, ReleaseOnSuccess, TerminateOnCompletion };

// run() borrows stdin only for the duration of the call. Bytes are opaque and
// private, not command syntax. Zero capture limits mean drain without retention.
struct ProcessRunRequest {
    // Shared invocation; no selected input is appended to these arguments.
    ProcessLaunchSpec launch;
    // Opaque stdin bytes, borrowed until run returns (including embedded NUL).
    std::string_view input;
    // Unspecified is invalid; failures/timeouts clean up under either valid choice.
    ProcessDescendantPolicy descendants = ProcessDescendantPolicy::Unspecified;
    // Positive idle-progress allowance; only actual byte transfer restarts it.
    std::int64_t idle_budget_nanos = 5'000'000'000;
    // Retained stdout prefix limit; excess bytes are drained and marked truncated.
    std::size_t max_stdout_bytes = 64 * 1024;
    // Independent retained stderr prefix limit; zero still drains the stream.
    std::size_t max_stderr_bytes = 64 * 1024;
};

// Completion, unavailable observations and native failures are distinct outcomes.
enum class ProcessRunState { InvalidRequest, LaunchFailed, IoFailed, IdleTimeout, ExitStatusUnavailable, Completed };
// The native namespace in which an error code must be interpreted.
enum class ProcessErrorDomain { None, Posix, Win32, NtStatus };
// Native errors retain their origin rather than being flattened to false/zero.
struct ProcessNativeError {
    // None means no native error was observed.
    ProcessErrorDomain domain = ProcessErrorDomain::None;
    // Full native errno/Win32 value; zero alone does not imply success.
    std::uint64_t code = 0;
};
// Normal root exit and POSIX signal termination are not interchangeable codes.
enum class ProcessExitKind { Normal, Signal };
// A known root observation; optional absence is expressed by ProcessRunResult.
struct ProcessExitStatus {
    // Determines whether code is an exit value or a signal number.
    ProcessExitKind kind = ProcessExitKind::Normal;
    // Preserves every Win32 DWORD, or POSIX exit code/signal without narrowing.
    std::int64_t code = 0;
};
// A bounded private byte prefix plus an independent overflow observation.
struct ProcessCapture {
    // Exact bytes retained, without decoding or forwarding them to a terminal.
    std::string bytes;
    // At least one observed byte exceeded the retention limit.
    bool truncated = false;
};
// All observed facts remain available even when the execution was unsuccessful.
struct ProcessRunResult {
    // Default is invalid/unexecuted, never a successful zero exit.
    ProcessRunState state = ProcessRunState::InvalidRequest;
    // First relevant native failure, if any.
    ProcessNativeError error;
    // Explanation for a caller to surface through its own UI/error channel.
    std::string diagnostic;
    // Requested input length, independent of successful transfer observations.
    std::size_t input_bytes_total = 0;
    // Positively observed stdin transfers, never inferred from root success.
    std::size_t input_bytes_written = 0;
    // Known root exit status; absent is unavailable, not exit zero.
    std::optional<ProcessExitStatus> exit;
    // Stdout is captured independently of stderr and of the success verdict.
    ProcessCapture stdout_capture;
    // Separate diagnostic byte stream, with its own retention/overflow state.
    ProcessCapture stderr_capture;

    // Requires completed/all-input/no-error/known-normal-zero simultaneously.
    bool successful() const noexcept {
        return state == ProcessRunState::Completed && error.domain == ProcessErrorDomain::None &&
               error.code == 0 && input_bytes_written == input_bytes_total && exit &&
               exit->kind == ProcessExitKind::Normal && exit->code == 0;
    }
};

// Pure request validation, before any native resources or processes are created.
inline bool valid_process_request(const ProcessRunRequest& request) noexcept {
    return validate_process_launch(request.launch) == ProcessLaunchValidation::Valid &&
           request.idle_budget_nanos > 0 &&
           (request.descendants == ProcessDescendantPolicy::ReleaseOnSuccess ||
            request.descendants == ProcessDescendantPolicy::TerminateOnCompletion);
}

// Injected synchronous boundary, not a global runner. Native adapters live in
// term and borrow a Clock. They drain both bounded captures while writing stdin,
// close it after the last byte, and complete on root exit (not descendant EOF).
// Failure/timeout cleans owned children; successful release is explicitly opted
// into above. Each pump pass and idle wait is bounded; no persistent I/O thread.
class ProcessRunner {
public:
    // Implementations can be owned through the injected interface.
    virtual ~ProcessRunner() = default;
    // Borrow input for this call, capture privately, then resolve the root/policy.
    virtual ProcessRunResult run(const ProcessRunRequest& request) = 0;
};

} // namespace ckv::core
