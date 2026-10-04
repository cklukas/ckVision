// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/process_resources.hpp"
#include "cvision/core/process_runner.hpp"

namespace ckv::core {

// A detached launch is not a captured helper or a terminal subsession.
enum class DetachedProcessState {
    InvalidLaunch, LaunchFailed, ContainmentQueryFailed, ContainmentRetained,
    ResumeFailed, Started,
};
// Cleanup of a created but never resumed child is independently observable.
enum class DetachedProcessCleanup { NotNeeded, Terminated, Failed };

// Facts from the native launch boundary; Started is not application readiness.
struct DetachedProcessResult {
    // An unexecuted value is not success.
    DetachedProcessState state = DetachedProcessState::InvalidLaunch;
    // Full-width native identity when a child was created, including refusals.
    ProcessId process_id = -1;
    // First native launch, containment-query or resume failure.
    ProcessNativeError error;
    // Failed launches preserve their separate cleanup outcome.
    DetachedProcessCleanup cleanup = DetachedProcessCleanup::NotNeeded;
    // Native cleanup failure, without replacing the original launch error.
    ProcessNativeError cleanup_error;
    // Explanation suitable for the application's own error channel.
    std::string diagnostic;

    // Requires a resumed child, verified detached, and no native failure.
    bool successful() const noexcept {
        return state == DetachedProcessState::Started && process_id > 0 &&
               error.domain == ProcessErrorDomain::None && error.code == 0 &&
               cleanup == DetachedProcessCleanup::NotNeeded &&
               cleanup_error.domain == ProcessErrorDomain::None && cleanup_error.code == 0;
    }
};

// Injected service for independently living processes, not a hidden singleton.
// Native implementations must prove their platform's detachment before letting
// child application code run. No stdio/other handles are inherited. Success
// releases ownership; application-specific readiness and shutdown stay outside.
class DetachedProcessLauncher {
public:
    // Allows ownership through the injected boundary.
    virtual ~DetachedProcessLauncher() = default;
    // Uses the same explicit invocation/environment model as other processes.
    virtual DetachedProcessResult launch(const ProcessLaunchSpec& spec) = 0;
};

} // namespace ckv::core
