// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A scripted machine, for the tests and the generated screenshots. It is
// the SysInfo counterpart of MemoryFileSystem in examples/filebrowser: the
// application above the probe cannot tell it from a real host, so every
// pane can be rendered and compared byte for byte on any platform.
//
// The reports are public members rather than constructor arguments, because
// what a test usually wants is the plausible machine with one field changed
// — most often to absence, which is the case a real host reaches often and
// a fixture otherwise never does.
#pragma once

#include <vector>

#include "system_probe.hpp"

namespace ckv::sysinfo {

class FixedSystemProbe final : public SystemProbe {
public:
    // A machine that is obviously fictional and still plausible in every
    // proportion. Naming a real processor here would put a claim about
    // somebody's hardware into a screenshot that never measured it.
    FixedSystemProbe();

    HostReport host_report;
    MemoryReport memory_report;
    ProcessorReport processor_report;
    std::vector<VolumeReport> volume_reports;
    PowerReport power_report;
    BuildReport build_report;

    HostReport host() const override;
    MemoryReport memory() const override;
    ProcessorReport processor() const override;
    std::vector<VolumeReport> volumes() const override;
    PowerReport power() const override;
    BuildReport build() const override;

    // How often the live reports have been re-read. The application
    // refreshes them on a timer, and this is how a test observes that the
    // timer does what the application claims rather than that the values
    // happen to be on screen.
    int host_reads() const noexcept { return host_reads_; }
    int memory_reads() const noexcept { return memory_reads_; }

private:
    mutable int host_reads_ = 0;
    mutable int memory_reads_ = 0;
};

}  // namespace ckv::sysinfo
