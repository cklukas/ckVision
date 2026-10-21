// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "monitor.hpp"
#include <CoreFoundation/CoreFoundation.h>

namespace ckv::sysmon {
// Driver snapshot keys are optional implementation details. Apply only to a
// unified-memory Metal device joined to this exact IORegistry entry.
void apply_apple_gpu_statistics(GpuSample& gpu, CFDictionaryRef properties,
                               std::optional<std::uint64_t> physical_memory);
} // namespace ckv::sysmon
