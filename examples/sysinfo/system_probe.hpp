// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The SysInfo example's single platform boundary (the architecture §5,
// D-039). Everything this application knows about the machine it runs on
// arrives through SystemProbe; no other file in examples/sysinfo includes a
// platform header, reads the environment, or opens a file.
//
// The reports are plain data with no behaviour, so a scripted machine
// (FixedSystemProbe) and a real one (PosixSystemProbe) are interchangeable
// to every line of the application above this header — which is what lets
// the whole interface render headlessly and identically on every host.
//
// Absence is expressed as absence. A host that cannot answer "how many
// physical cores" must not be made to say zero: a plausible wrong number is
// indistinguishable from a measurement, and this is a program whose entire
// purpose is to be believed about numbers.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ckv::sysinfo {

struct HostReport {
    std::optional<std::string> host_name;
    std::optional<std::string> user_name;
    // The operating system as it names itself ("macOS 15.5", "Debian
    // GNU/Linux 13 (trixie)"), which is not the kernel below it.
    std::optional<std::string> os_name;
    std::optional<std::string> kernel;        // "Darwin 25.5.0", "Linux 6.12.0"
    std::optional<std::string> architecture;  // "arm64", "x86_64"
    std::optional<std::int64_t> uptime_seconds;
    std::optional<double> load_average_1m;
};

struct ProcessorReport {
    std::optional<std::string> brand;
    std::optional<int> physical_cores;
    std::optional<int> logical_cores;
    // Nominal, not current: a modern core's instantaneous frequency is a
    // thermal negotiation, and reporting one sample of it as "the speed"
    // is the oldest lie in this genre of program.
    std::optional<std::uint64_t> nominal_hz;
    // Present where the host distinguishes core kinds (Apple silicon
    // perflevels), absent where it does not — and absent is the honest
    // answer for a uniform processor, not zero.
    std::optional<int> performance_cores;
    std::optional<int> efficiency_cores;
    std::optional<std::uint64_t> l1d_cache_bytes;
    std::optional<std::uint64_t> l2_cache_bytes;
    std::optional<std::uint64_t> l3_cache_bytes;
};

// Hosts disagree about what "used" memory is, deeply and for good reasons,
// so this report carries the two figures everyone agrees on and then the
// host's own accounting in the host's own words rather than flattening
// them into categories the machine never used.
struct MemoryReport {
    std::optional<std::uint64_t> total_bytes;
    std::optional<std::uint64_t> available_bytes;
    std::vector<std::pair<std::string, std::uint64_t>> detail;
    std::optional<std::uint64_t> swap_total_bytes;
    std::optional<std::uint64_t> swap_used_bytes;
};

struct VolumeReport {
    std::string mount_point;
    std::optional<std::string> device;
    std::optional<std::string> filesystem;
    std::optional<std::uint64_t> capacity_bytes;
    std::optional<std::uint64_t> free_bytes;
    bool read_only = false;
    // The operating system's own bookkeeping rather than storage anybody
    // thinks of as a disk: preboot and recovery volumes, simulator images,
    // container runtimes' overlays. A machine has a dozen of them and a
    // reader asking "what disks do I have" means none of them -- so the
    // pane hides them until asked, and the probe, which is the only thing
    // here that can tell, is what decides.
    bool system = false;
};

struct PowerReport {
    enum class Source { Unknown, Battery, Line };

    Source source = Source::Unknown;
    std::optional<int> charge_percent;
    std::optional<bool> charging;
    std::optional<std::int64_t> remaining_seconds;
};

// Facts about this binary rather than about the machine — but they belong
// behind the same interface, because a report that mixed the build's real
// compiler into otherwise scripted output would differ between hosts and
// stop being a golden.
struct BuildReport {
    std::string compiler;
    std::string standard_library;
    std::string cxx_standard;
    // The build's own configuration name ("Debug", "Release"), passed in
    // by the build system because the compiler does not know it. It
    // belongs in a program that measures speed: an unoptimized build's
    // benchmark numbers describe the build and not the machine, and the
    // difference is a factor of several.
    std::string build_type;
    int pointer_bits = 0;
    bool little_endian = true;
};

// Whether `build_type` names a configuration whose measurements are about
// the machine. An empty or unrecognized name is not a promise either way,
// so it answers false and the pane says what it does not know.
bool build_is_optimized(const BuildReport& build) noexcept;

// Facts the compiler knows about its own output. No host access at all,
// which is why it is portable code that every probe can share rather than
// something each platform re-derives.
BuildReport current_build_report();

// ckvision-doc: system-probe
class SystemProbe {
public:
    virtual ~SystemProbe() = default;

    // Cheap and re-readable: the application calls host() and memory() on a
    // refresh timer, so an implementation must not cache them.
    virtual HostReport host() const = 0;
    virtual MemoryReport memory() const = 0;

    virtual ProcessorReport processor() const = 0;
    virtual std::vector<VolumeReport> volumes() const = 0;
    virtual PowerReport power() const = 0;
    virtual BuildReport build() const = 0;
};
// ckvision-doc-end: system-probe

}  // namespace ckv::sysinfo
