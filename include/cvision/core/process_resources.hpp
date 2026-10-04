// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>

namespace ckv::core {

// Observation-only OS identity. Every Windows DWORD and positive POSIX pid
// fits without narrowing; -1 denotes no process. Never a control handle.
using ProcessId = std::int64_t;

// Complete observations, incomplete observations, absent processes, and
// unsupported/failed queries are deliberately different answers.
enum class ProcessResourceState : std::uint8_t { Unsupported, Available, Partial, Gone, Failed };

// A job's cumulative CPU includes children that have already exited; its
// current resident memory does not. Consumers must not sum live CPU instead.
enum class ProcessCpuScope : std::uint8_t { ProcessLifetime, OwnedJobLifetime };

// A clock-free observation. nullopt is unavailable, not a measured zero.
// Platform adapters produce these values; deterministic hosts may inject them.
struct ProcessResources {
    // Whether the requested subject and fields could be observed.
    ProcessResourceState state = ProcessResourceState::Unsupported;
    // Lifetime whose CPU counter is measured.
    ProcessCpuScope cpu_scope = ProcessCpuScope::ProcessLifetime;
    // Cumulative user plus kernel CPU, in nanoseconds; never a rate.
    std::optional<std::uint64_t> cpu_time_nanos;
    // Current resident working set, summed per live process (shared pages repeat).
    std::optional<std::uint64_t> rss_bytes;
    // Current private resident working set, NOT private committed virtual memory.
    // Absent when the OS cannot supply this field or any member cannot be read.
    std::optional<std::uint64_t> private_rss_bytes;
    // Live processes actually observed, excluding members that exited mid-query.
    std::uint32_t live_processes = 0;
    // Listed members whose state/counters could not be read; totals are absent.
    std::uint32_t unreadable_processes = 0;
    // First native query error, or zero. Optional-field OS support is not an error.
    std::uint32_t system_error = 0;
};

}  // namespace ckv::core
