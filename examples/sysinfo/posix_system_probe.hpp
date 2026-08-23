// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The real machine, on macOS and Linux.
//
// This declaration is portable on purpose — it is a plain class with no
// platform type in its interface — and only posix_system_probe.cpp reaches
// for sysctl, mach, /proc and /sys. That file is the whole of this
// example's impurity: everything else, including every pane and every
// test, is written against SystemProbe and does not know which machine (or
// which century) answered.
//
// A Windows probe is the same shape and is WP-37's, not this package's.
#pragma once

#include <vector>

#include "system_probe.hpp"

namespace ckv::sysinfo {

class PosixSystemProbe final : public SystemProbe {
public:
    HostReport host() const override;
    MemoryReport memory() const override;
    ProcessorReport processor() const override;
    std::vector<VolumeReport> volumes() const override;
    PowerReport power() const override;
    BuildReport build() const override;
};

}  // namespace ckv::sysinfo
