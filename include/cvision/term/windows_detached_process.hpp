// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"
#include "cvision/core/detached_process.hpp"

namespace ckv::term {

// Windows-only native backend. Creates a console-detached suspended child,
// requests breakaway, and verifies absence from EVERY job before resuming.
// A restrictive ancestor is a typed refusal, never a successful fragile daemon.
// Cleanup terminates the never-resumed child and waits at most five seconds;
// inability to confirm cleanup remains visible separately in the result.
class CVISION_TYPE_API WindowsDetachedProcessLauncher final : public core::DetachedProcessLauncher {
public:
    // Shared UTF-8/native invocation preparation; no PATH search or shell guess.
    CVISION_MEMBER_API core::DetachedProcessResult launch(const core::ProcessLaunchSpec& spec) override;
};

} // namespace ckv::term
