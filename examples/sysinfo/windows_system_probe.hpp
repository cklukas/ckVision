// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "system_probe.hpp"
namespace ckv::sysinfo {
class WindowsSystemProbe final : public SystemProbe {
public:
    HostReport host() const override;
    MemoryReport memory() const override;
    ProcessorReport processor() const override;
    std::vector<VolumeReport> volumes() const override;
    PowerReport power() const override;
    BuildReport build() const override;
};
} // namespace ckv::sysinfo
