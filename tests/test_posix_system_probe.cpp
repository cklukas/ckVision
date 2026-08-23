// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The SysInfo example's real probe, on whatever machine is running the
// suite. Nothing here asserts a value: this test is written on one machine
// and run on others, so the only honest assertions are the ones that must
// hold on every machine — internal consistency, and the rule that an
// unavailable figure is absent rather than zero.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "posix_system_probe.hpp"

using ckv::sysinfo::HostReport;
using ckv::sysinfo::MemoryReport;
using ckv::sysinfo::PosixSystemProbe;
using ckv::sysinfo::ProcessorReport;
using ckv::sysinfo::VolumeReport;

CK_TEST(the_host_report_describes_the_machine_running_the_test) {
    const PosixSystemProbe probe;
    const HostReport host = probe.host();

    // uname and gethostname are POSIX obligations, so their absence would
    // be a defect in this probe rather than a quiet host.
    CK_CHECK(host.host_name.has_value() && !host.host_name->empty());
    CK_CHECK(host.kernel.has_value() && !host.kernel->empty());
    CK_CHECK(host.architecture.has_value() && !host.architecture->empty());

    if (host.uptime_seconds.has_value()) CK_CHECK(*host.uptime_seconds > 0);
    if (host.load_average_1m.has_value()) CK_CHECK(*host.load_average_1m >= 0.0);
}

CK_TEST(reading_the_host_twice_agrees_with_itself) {
    const PosixSystemProbe probe;
    CK_CHECK(probe.host().host_name == probe.host().host_name);
    CK_CHECK(probe.processor().logical_cores == probe.processor().logical_cores);
}

CK_TEST(memory_available_never_exceeds_memory_total) {
    const PosixSystemProbe probe;
    const MemoryReport memory = probe.memory();

    CK_CHECK(memory.total_bytes.has_value());
    CK_CHECK(*memory.total_bytes > 0);
    if (memory.available_bytes.has_value()) CK_CHECK(*memory.available_bytes <= *memory.total_bytes);
    for (const auto& [name, value] : memory.detail) {
        CK_CHECK(!name.empty());
        CK_CHECK(value <= *memory.total_bytes);
    }
    if (memory.swap_total_bytes.has_value() && memory.swap_used_bytes.has_value())
        CK_CHECK(*memory.swap_used_bytes <= *memory.swap_total_bytes);
}

CK_TEST(the_processor_reports_at_least_one_core_and_no_more_physical_than_logical) {
    const PosixSystemProbe probe;
    const ProcessorReport processor = probe.processor();

    CK_CHECK(processor.logical_cores.has_value());
    CK_CHECK(*processor.logical_cores >= 1);
    if (processor.physical_cores.has_value()) {
        CK_CHECK(*processor.physical_cores >= 1);
        CK_CHECK(*processor.physical_cores <= *processor.logical_cores);
    }
    // Where the host distinguishes core kinds, the kinds add up to the
    // whole; where it does not, both are absent and nothing is claimed.
    if (processor.performance_cores.has_value() && processor.efficiency_cores.has_value())
        CK_CHECK(*processor.performance_cores + *processor.efficiency_cores <= *processor.logical_cores);
    if (processor.l2_cache_bytes.has_value() && processor.l1d_cache_bytes.has_value())
        CK_CHECK(*processor.l2_cache_bytes >= *processor.l1d_cache_bytes);
}

CK_TEST(every_reported_volume_has_a_mount_point_and_a_capacity_of_its_own) {
    const PosixSystemProbe probe;
    const std::vector<VolumeReport> volumes = probe.volumes();

    // A POSIX machine has a root filesystem; a probe that finds none has
    // failed to read the host rather than found a host with no disks.
    CK_CHECK(!volumes.empty());
    bool root_seen = false;
    for (const VolumeReport& volume : volumes) {
        CK_CHECK(!volume.mount_point.empty());
        if (volume.capacity_bytes.has_value()) CK_CHECK(*volume.capacity_bytes > 0);
        if (volume.mount_point == "/") root_seen = true;
    }
    CK_CHECK(root_seen);

    // Deliberately NOT asserted: that free space never exceeds capacity.
    // It does, routinely, on a shared APFS container — twelve volumes on
    // the machine this was written on. The pane's answer to that pair is
    // tested against a fixture in test_sysinfo_format.cpp, where it can be
    // stated exactly instead of hoped for.
}

CK_TEST(a_battery_percentage_is_a_percentage) {
    const PosixSystemProbe probe;
    const ckv::sysinfo::PowerReport power = probe.power();
    if (power.charge_percent.has_value()) {
        CK_CHECK(*power.charge_percent >= 0);
        CK_CHECK(*power.charge_percent <= 100);
    }
    // A machine on line power with no battery reports Line or Unknown and
    // no percentage at all, which is not a failure of anything.
}

CK_TEST(the_build_report_describes_this_binary) {
    const PosixSystemProbe probe;
    const ckv::sysinfo::BuildReport build = probe.build();

    CK_CHECK(!build.compiler.empty());
    CK_CHECK(!build.standard_library.empty());
    CK_CHECK(build.cxx_standard == "C++20");
    CK_CHECK(build.pointer_bits == static_cast<int>(sizeof(void*) * 8));
}
