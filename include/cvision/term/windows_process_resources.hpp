// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include "cvision/core/process_resources.hpp"

#if defined(_WIN32)
namespace ckv::term {

// Opens an observation-only handle for this positive full-width native PID.
// A vanished process reports Gone; access/query failures are visible. No
// privileges are enabled and no clock, global cache or process control is used.
CVISION_API core::ProcessResources sample_windows_process(core::ProcessId process) noexcept;

// Borrows an explicit non-null job handle (`HANDLE` / `void*`), never the
// caller's ambient job. CPU includes exited members; memory/counts use the
// observed live membership, including nested jobs. PID reuse is checked against
// this job after opening. A racing exit is skipped; unreadable members make
// memory totals unavailable. Bounded membership retries report Partial rather
// than truncating a large job.
CVISION_API core::ProcessResources sample_windows_job(void* job) noexcept;

}  // namespace ckv::term
#endif
