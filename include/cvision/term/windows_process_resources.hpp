// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/process_resources.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ckv::term {

// Opens an observation-only handle for this positive full-width native PID.
// A vanished process reports Gone; access/query failures are visible. No
// privileges are enabled and no clock, global cache or process control is used.
core::ProcessResources sample_windows_process(core::ProcessId process) noexcept;

// Borrows an explicit non-null job handle, never the caller's ambient job.
// CPU includes exited members; memory/counts use the observed live membership,
// including nested jobs. PID reuse is checked against this job after opening.
// A racing exit is skipped; unreadable members make memory totals unavailable.
// Bounded membership retries report Partial rather than truncating a large job.
core::ProcessResources sample_windows_job(HANDLE job) noexcept;

}  // namespace ckv::term
#endif
