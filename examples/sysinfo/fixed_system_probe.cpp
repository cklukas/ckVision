// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "fixed_system_probe.hpp"

namespace ckv::sysinfo {

FixedSystemProbe::FixedSystemProbe() {
    host_report.host_name = "sysinfo-demo";
    host_report.user_name = "demo";
    host_report.os_name = "ckVision Demo OS 1.0";
    host_report.kernel = "Demo 1.0.0";
    host_report.architecture = "arm64";
    host_report.uptime_seconds = 357'730;  // 4d 03:22:10
    host_report.load_average_1m = 1.25;

    memory_report.total_bytes = 16ULL * 1024 * 1024 * 1024;
    memory_report.available_bytes = 6ULL * 1024 * 1024 * 1024;
    memory_report.detail = {
        {"Wired", 3ULL * 1024 * 1024 * 1024},
        {"Active", 5ULL * 1024 * 1024 * 1024},
        {"Inactive", 2ULL * 1024 * 1024 * 1024},
        {"Compressed", 1ULL * 1024 * 1024 * 1024},
    };
    memory_report.swap_total_bytes = 2ULL * 1024 * 1024 * 1024;
    memory_report.swap_used_bytes = 512ULL * 1024 * 1024;

    processor_report.brand = "ckVision Demo CPU";
    processor_report.physical_cores = 8;
    processor_report.logical_cores = 8;
    processor_report.nominal_hz = 3'200'000'000;
    processor_report.performance_cores = 4;
    processor_report.efficiency_cores = 4;
    processor_report.l1d_cache_bytes = 128ULL * 1024;
    processor_report.l2_cache_bytes = 4ULL * 1024 * 1024;
    // Left absent on purpose: this fictional processor has no level-3
    // cache to report, and the pane showing that as "not reported" rather
    // than as "0 B" is the fixture teaching the rule.
    processor_report.l3_cache_bytes = std::nullopt;

    volume_reports = {
        VolumeReport{"/", std::string("/dev/demo0s1"), std::string("demofs"),
                     512ULL * 1024 * 1024 * 1024, 128ULL * 1024 * 1024 * 1024, false},
        VolumeReport{"/Volumes/Archive", std::string("/dev/demo1s1"), std::string("demofs"),
                     2048ULL * 1024 * 1024 * 1024, 96ULL * 1024 * 1024 * 1024, true},
    };

    power_report.source = PowerReport::Source::Battery;
    power_report.charge_percent = 82;
    power_report.charging = false;
    power_report.remaining_seconds = 9'000;

    build_report.compiler = "Demo C++ 1.0";
    build_report.standard_library = "Demo Standard Library";
    build_report.cxx_standard = "C++20";
    build_report.build_type = "Release";
    build_report.pointer_bits = 64;
    build_report.little_endian = true;
}

HostReport FixedSystemProbe::host() const {
    ++host_reads_;
    return host_report;
}

MemoryReport FixedSystemProbe::memory() const {
    ++memory_reads_;
    return memory_report;
}

ProcessorReport FixedSystemProbe::processor() const { return processor_report; }

std::vector<VolumeReport> FixedSystemProbe::volumes() const { return volume_reports; }

PowerReport FixedSystemProbe::power() const { return power_report; }

BuildReport FixedSystemProbe::build() const { return build_report; }

}  // namespace ckv::sysinfo
