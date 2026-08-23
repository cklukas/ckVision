// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The whole of the SysInfo example's platform impurity, in one translation
// unit (the internal plans). Two hosts are implemented — macOS
// through sysctl, mach and getmntinfo; Linux through /proc, /sys and
// /etc/os-release — and any other POSIX compiles and reports absence,
// which is a truthful answer rather than a build failure.
//
// The rule this file follows everywhere: a value that was not obtained is
// left absent. There is no default, no zero and no "unknown" string
// standing in for a measurement, because the pane above cannot tell those
// apart from an answer and neither can the reader.
#include "posix_system_probe.hpp"

#include <pwd.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>
#elif defined(__linux__)
#include <cerrno>
#endif

namespace ckv::sysinfo {
namespace {

std::string trimmed(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    const auto is_space = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    while (begin < end && is_space(text[begin])) ++begin;
    while (end > begin && is_space(text[end - 1])) --end;
    return std::string(text.substr(begin, end - begin));
}

std::optional<std::string> non_empty(std::string text) {
    if (text.empty()) return std::nullopt;
    return text;
}

// Free and available space for one mount point, through the standard
// library rather than a platform call: statvfs and GetDiskFreeSpaceEx
// already live behind it, and the answer is the same on every host.
//
// `available` rather than `free`: the difference is the reserve only a
// privileged process may consume, and a user asking how much room is left
// means the room they have.
[[maybe_unused]] void fill_space(VolumeReport& volume) {
    std::error_code error;
    const std::filesystem::space_info space = std::filesystem::space(volume.mount_point, error);
    if (error) return;
    if (space.capacity != static_cast<std::uintmax_t>(-1)) volume.capacity_bytes = static_cast<std::uint64_t>(space.capacity);
    if (space.available != static_cast<std::uintmax_t>(-1)) volume.free_bytes = static_cast<std::uint64_t>(space.available);
}

#if defined(__APPLE__)

std::optional<std::string> sysctl_string(const char* name) {
    std::size_t size = 0;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0) return std::nullopt;
    std::string value(size, '\0');
    if (::sysctlbyname(name, value.data(), &size, nullptr, 0) != 0) return std::nullopt;
    value.resize(std::strlen(value.c_str()));
    return non_empty(std::move(value));
}

// sysctl's integers are 4 or 8 bytes depending on the name, and asking with
// the wrong width silently truncates on one endianness and garbles on the
// other. So the width is asked for first, and only then the value.
std::optional<std::uint64_t> sysctl_unsigned(const char* name) {
    std::size_t size = 0;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0) != 0) return std::nullopt;
    if (size == sizeof(std::uint64_t)) {
        std::uint64_t value = 0;
        if (::sysctlbyname(name, &value, &size, nullptr, 0) != 0) return std::nullopt;
        return value;
    }
    if (size == sizeof(std::uint32_t)) {
        std::uint32_t value = 0;
        if (::sysctlbyname(name, &value, &size, nullptr, 0) != 0) return std::nullopt;
        return static_cast<std::uint64_t>(value);
    }
    return std::nullopt;
}

std::optional<int> sysctl_count(const char* name) {
    const std::optional<std::uint64_t> value = sysctl_unsigned(name);
    if (!value.has_value() || *value > static_cast<std::uint64_t>(1 << 20)) return std::nullopt;
    return static_cast<int>(*value);
}

std::optional<std::string> cf_string_value(CFDictionaryRef source, CFStringRef key) {
    const void* const entry = ::CFDictionaryGetValue(source, key);
    if (entry == nullptr || ::CFGetTypeID(entry) != ::CFStringGetTypeID()) return std::nullopt;
    char buffer[256];
    if (!::CFStringGetCString(static_cast<CFStringRef>(entry), buffer, sizeof(buffer), kCFStringEncodingUTF8))
        return std::nullopt;
    return non_empty(std::string(buffer));
}

std::optional<int> cf_int_value(CFDictionaryRef source, CFStringRef key) {
    const void* const entry = ::CFDictionaryGetValue(source, key);
    if (entry == nullptr || ::CFGetTypeID(entry) != ::CFNumberGetTypeID()) return std::nullopt;
    int value = 0;
    if (!::CFNumberGetValue(static_cast<CFNumberRef>(entry), kCFNumberIntType, &value)) return std::nullopt;
    return value;
}

std::optional<bool> cf_bool_value(CFDictionaryRef source, CFStringRef key) {
    const void* const entry = ::CFDictionaryGetValue(source, key);
    if (entry == nullptr || ::CFGetTypeID(entry) != ::CFBooleanGetTypeID()) return std::nullopt;
    return ::CFBooleanGetValue(static_cast<CFBooleanRef>(entry)) != 0;
}

#elif defined(__linux__)

std::optional<std::string> read_file_text(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return std::nullopt;
    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

// One "Key: value" line from a /proc file, as text.
std::optional<std::string> proc_field(const std::string& contents, std::string_view key) {
    std::size_t position = 0;
    while (position < contents.size()) {
        const std::size_t line_end = contents.find('\n', position);
        const std::string_view line(contents.data() + position,
                                    (line_end == std::string::npos ? contents.size() : line_end) - position);
        const std::size_t colon = line.find(':');
        if (colon != std::string_view::npos && trimmed(line.substr(0, colon)) == key)
            return trimmed(line.substr(colon + 1));
        if (line_end == std::string::npos) break;
        position = line_end + 1;
    }
    return std::nullopt;
}

// /proc/meminfo speaks in kibibytes, and says so on every line.
std::optional<std::uint64_t> meminfo_bytes(const std::string& contents, std::string_view key) {
    const std::optional<std::string> field = proc_field(contents, key);
    if (!field.has_value()) return std::nullopt;
    char* end = nullptr;
    const unsigned long long kibibytes = std::strtoull(field->c_str(), &end, 10);
    if (end == field->c_str()) return std::nullopt;
    return static_cast<std::uint64_t>(kibibytes) * 1024;
}

std::optional<std::string> os_release_pretty_name() {
    std::optional<std::string> contents = read_file_text("/etc/os-release");
    if (!contents.has_value()) contents = read_file_text("/usr/lib/os-release");
    if (!contents.has_value()) return std::nullopt;
    std::istringstream lines(*contents);
    std::string line;
    while (std::getline(lines, line)) {
        constexpr std::string_view kKey = "PRETTY_NAME=";
        if (line.rfind(kKey, 0) != 0) continue;
        std::string value = line.substr(kKey.size());
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
        return non_empty(trimmed(value));
    }
    return std::nullopt;
}

// A /sys leaf holding a single number.
std::optional<std::uint64_t> sysfs_unsigned(const std::string& path) {
    const std::optional<std::string> contents = read_file_text(path.c_str());
    if (!contents.has_value()) return std::nullopt;
    char* end = nullptr;
    const unsigned long long value = std::strtoull(contents->c_str(), &end, 10);
    if (end == contents->c_str()) return std::nullopt;
    return static_cast<std::uint64_t>(value);
}

std::optional<std::string> sysfs_text(const std::string& path) {
    const std::optional<std::string> contents = read_file_text(path.c_str());
    if (!contents.has_value()) return std::nullopt;
    return non_empty(trimmed(*contents));
}

// "512K", "1024K", "32M" — the spelling /sys/devices/.../cache uses.
std::optional<std::uint64_t> parse_cache_size(const std::string& text) {
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text.c_str(), &end, 10);
    if (end == text.c_str()) return std::nullopt;
    std::uint64_t bytes = static_cast<std::uint64_t>(value);
    if (*end == 'K' || *end == 'k') bytes *= 1024;
    else if (*end == 'M' || *end == 'm') bytes *= 1024 * 1024;
    else if (*end == 'G' || *end == 'g') bytes *= 1024ULL * 1024 * 1024;
    return bytes;
}

// /proc/mounts escapes the characters that would otherwise end a field.
std::string unescape_mount_field(std::string_view field) {
    std::string result;
    result.reserve(field.size());
    for (std::size_t index = 0; index < field.size(); ++index) {
        if (field[index] == '\\' && index + 3 < field.size()) {
            const std::string digits(field.substr(index + 1, 3));
            if (digits.find_first_not_of("01234567") == std::string::npos) {
                result.push_back(static_cast<char>(std::strtol(digits.c_str(), nullptr, 8)));
                index += 3;
                continue;
            }
        }
        result.push_back(field[index]);
    }
    return result;
}

// Kernel bookkeeping mounted as a filesystem. Listing it under "disks"
// would answer a question about storage with a list of things that are not
// storage.
bool is_pseudo_filesystem(std::string_view type) {
    static constexpr std::string_view kPseudo[] = {
        "autofs",   "bpf",       "binfmt_misc", "cgroup",  "cgroup2", "configfs", "debugfs",
        "devpts",   "devtmpfs",  "efivarfs",    "fusectl", "hugetlbfs", "mqueue",  "nsfs",
        "proc",     "pstore",    "ramfs",       "securityfs", "sysfs",  "tracefs", "rpc_pipefs",
        "selinuxfs", "fuse.gvfsd-fuse",
    };
    return std::find(std::begin(kPseudo), std::end(kPseudo), type) != std::end(kPseudo);
}

#endif

}  // namespace

HostReport PosixSystemProbe::host() const {
    HostReport report;

    char host_name[256] = {};
    if (::gethostname(host_name, sizeof(host_name) - 1) == 0) report.host_name = non_empty(std::string(host_name));

    // The account this process runs as, from the password database rather
    // than from $USER — the environment can say anything, and on this
    // machine it usually says what some earlier shell decided.
    if (const struct passwd* const entry = ::getpwuid(::getuid()); entry != nullptr && entry->pw_name != nullptr)
        report.user_name = non_empty(std::string(entry->pw_name));

    struct utsname system_name = {};
    if (::uname(&system_name) == 0) {
        report.kernel = non_empty(trimmed(system_name.sysname) + " " + trimmed(system_name.release));
        report.architecture = non_empty(trimmed(system_name.machine));
    }

    double load[3] = {0.0, 0.0, 0.0};
    if (::getloadavg(load, 3) >= 1) report.load_average_1m = load[0];

#if defined(__APPLE__)
    // "macOS" is a compile-time fact about this build's platform, not a
    // string standing in for something the host was asked and did not say.
    if (const std::optional<std::string> version = sysctl_string("kern.osproductversion"); version.has_value())
        report.os_name = "macOS " + *version;
    struct timeval boot = {};
    std::size_t boot_size = sizeof(boot);
    int boot_name[2] = {CTL_KERN, KERN_BOOTTIME};
    if (::sysctl(boot_name, 2, &boot, &boot_size, nullptr, 0) == 0 && boot.tv_sec > 0) {
        const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
        if (now > boot.tv_sec) report.uptime_seconds = now - static_cast<std::int64_t>(boot.tv_sec);
    }
#elif defined(__linux__)
    report.os_name = os_release_pretty_name();
    if (const std::optional<std::string> uptime = read_file_text("/proc/uptime"); uptime.has_value()) {
        char* end = nullptr;
        const double seconds = std::strtod(uptime->c_str(), &end);
        if (end != uptime->c_str() && seconds >= 0.0) report.uptime_seconds = static_cast<std::int64_t>(seconds);
    }
#endif

    return report;
}

MemoryReport PosixSystemProbe::memory() const {
    MemoryReport report;

#if defined(__APPLE__)
    report.total_bytes = sysctl_unsigned("hw.memsize");

    vm_size_t page_size = 0;
    vm_statistics64_data_t statistics = {};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    const host_t host_port = ::mach_host_self();
    if (::host_page_size(host_port, &page_size) == KERN_SUCCESS &&
        ::host_statistics64(host_port, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&statistics), &count) ==
            KERN_SUCCESS) {
        const auto bytes = [page_size](std::uint64_t pages) { return pages * static_cast<std::uint64_t>(page_size); };
        // Darwin has no single "available" counter, so this is a stated
        // definition rather than a reading: the pages a new allocation can
        // take without evicting anything a running program still wants.
        // The host's own categories are listed underneath, unaggregated,
        // so a reader who defines it differently can do their own sum.
        report.available_bytes = bytes(static_cast<std::uint64_t>(statistics.free_count) +
                                       statistics.inactive_count + statistics.purgeable_count);
        report.detail = {
            {"Wired", bytes(statistics.wire_count)},
            {"Active", bytes(statistics.active_count)},
            {"Inactive", bytes(statistics.inactive_count)},
            {"Speculative", bytes(statistics.speculative_count)},
            {"Compressed", bytes(statistics.compressor_page_count)},
            {"Free", bytes(statistics.free_count)},
        };
    }

    ::mach_port_deallocate(::mach_task_self(), host_port);

    struct xsw_usage swap = {};
    std::size_t swap_size = sizeof(swap);
    if (::sysctlbyname("vm.swapusage", &swap, &swap_size, nullptr, 0) == 0 && swap_size == sizeof(swap)) {
        report.swap_total_bytes = static_cast<std::uint64_t>(swap.xsu_total);
        report.swap_used_bytes = static_cast<std::uint64_t>(swap.xsu_used);
    }
#elif defined(__linux__)
    const std::optional<std::string> meminfo = read_file_text("/proc/meminfo");
    if (meminfo.has_value()) {
        report.total_bytes = meminfo_bytes(*meminfo, "MemTotal");
        // MemAvailable is the kernel's own estimate and exactly the figure
        // this pane wants; nothing here recomputes it from Free + Cached,
        // which is the traditional way of being wrong about it.
        report.available_bytes = meminfo_bytes(*meminfo, "MemAvailable");
        for (const std::string_view key : {"Buffers", "Cached", "Dirty", "Shmem", "MemFree"}) {
            if (const std::optional<std::uint64_t> value = meminfo_bytes(*meminfo, key); value.has_value())
                report.detail.emplace_back(std::string(key), *value);
        }
        const std::optional<std::uint64_t> swap_total = meminfo_bytes(*meminfo, "SwapTotal");
        const std::optional<std::uint64_t> swap_free = meminfo_bytes(*meminfo, "SwapFree");
        report.swap_total_bytes = swap_total;
        if (swap_total.has_value() && swap_free.has_value() && *swap_total >= *swap_free)
            report.swap_used_bytes = *swap_total - *swap_free;
    }
#endif

    return report;
}

ProcessorReport PosixSystemProbe::processor() const {
    ProcessorReport report;

#if defined(__APPLE__)
    report.brand = sysctl_string("machdep.cpu.brand_string");
    report.physical_cores = sysctl_count("hw.physicalcpu");
    report.logical_cores = sysctl_count("hw.logicalcpu");
    // Absent on Apple silicon, where there is no single nominal frequency
    // to report — which is the correct answer, not a gap to fill in.
    report.nominal_hz = sysctl_unsigned("hw.cpufrequency");
    report.performance_cores = sysctl_count("hw.perflevel0.logicalcpu");
    report.efficiency_cores = sysctl_count("hw.perflevel1.logicalcpu");
    report.l1d_cache_bytes = sysctl_unsigned("hw.l1dcachesize");
    report.l2_cache_bytes = sysctl_unsigned("hw.l2cachesize");
    report.l3_cache_bytes = sysctl_unsigned("hw.l3cachesize");
#elif defined(__linux__)
    if (const long online = ::sysconf(_SC_NPROCESSORS_ONLN); online > 0) report.logical_cores = static_cast<int>(online);

    if (const std::optional<std::string> cpuinfo = read_file_text("/proc/cpuinfo"); cpuinfo.has_value()) {
        // x86 says "model name"; ARM says neither, and its "Model" or
        // "Hardware" line is the closest thing the kernel offers.
        for (const std::string_view key : {"model name", "Model", "Hardware", "cpu model"}) {
            report.brand = proc_field(*cpuinfo, key);
            if (report.brand.has_value()) break;
        }
        if (const std::optional<std::string> cores = proc_field(*cpuinfo, "cpu cores"); cores.has_value()) {
            const int value = std::atoi(cores->c_str());
            if (value > 0) report.physical_cores = value;
        }
    }
    if (!report.physical_cores.has_value()) {
        // No topology to read: on a machine without SMT this is the same
        // number, and claiming otherwise would need evidence there is none.
        report.physical_cores = report.logical_cores;
    }

    if (const std::optional<std::uint64_t> khz =
            sysfs_unsigned("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        khz.has_value())
        report.nominal_hz = *khz * 1000;

    for (int index = 0; index < 10; ++index) {
        const std::string base = "/sys/devices/system/cpu/cpu0/cache/index" + std::to_string(index);
        const std::optional<std::string> size_text = sysfs_text(base + "/size");
        const std::optional<std::uint64_t> level = sysfs_unsigned(base + "/level");
        if (!size_text.has_value() || !level.has_value()) continue;
        const std::optional<std::uint64_t> size = parse_cache_size(*size_text);
        const std::optional<std::string> type = sysfs_text(base + "/type");
        if (!size.has_value()) continue;
        if (*level == 1 && type.has_value() && *type == "Data") report.l1d_cache_bytes = size;
        else if (*level == 2) report.l2_cache_bytes = size;
        else if (*level == 3) report.l3_cache_bytes = size;
    }
#endif

    return report;
}

std::vector<VolumeReport> PosixSystemProbe::volumes() const {
    std::vector<VolumeReport> volumes;

#if defined(__APPLE__)
    struct statfs* mounts = nullptr;
    const int count = ::getmntinfo(&mounts, MNT_NOWAIT);
    for (int index = 0; index < count; ++index) {
        const struct statfs& mount = mounts[index];
        const std::string type = trimmed(mount.f_fstypename);
        // devfs is the device tree and autofs is a promise to mount
        // something later; neither has a capacity worth showing. A
        // snapshot is a past state of a volume already listed — a machine
        // with Time Machine running has dozens mounted under
        // /Volumes/.timemachine, and listing them answers "what disks do
        // I have" with a backup history.
        //
        // Except the root filesystem, which since macOS 11 IS a snapshot:
        // the sealed system volume is mounted from one, so the plain rule
        // "skip every snapshot" silently drops the one disk every reader
        // opens this pane to look at. MNT_ROOTFS is how the kernel says
        // which snapshot that is.
        if (type == "devfs" || type == "autofs") continue;
        if ((mount.f_flags & MNT_SNAPSHOT) != 0 && (mount.f_flags & MNT_ROOTFS) == 0) continue;
        VolumeReport volume;
        volume.mount_point = trimmed(mount.f_mntonname);
        volume.device = non_empty(trimmed(mount.f_mntfromname));
        volume.filesystem = non_empty(type);
        volume.read_only = (mount.f_flags & MNT_RDONLY) != 0;
        // MNT_DONTBROWSE is the kernel's own answer to this question -- it
        // is the flag Finder reads to decide what not to show -- so this
        // pane does not need a list of paths that would go stale with the
        // next macOS. On this machine it separates "/" and /Volumes/* from
        // every /System/Volumes/*, every simulator image and every cryptex
        // mount, exactly.
        volume.system = (mount.f_flags & MNT_DONTBROWSE) != 0;
        fill_space(volume);
        volumes.push_back(std::move(volume));
    }
#elif defined(__linux__)
    const std::optional<std::string> mounts = read_file_text("/proc/self/mounts");
    if (mounts.has_value()) {
        std::istringstream lines(*mounts);
        std::string line;
        while (std::getline(lines, line)) {
            std::istringstream fields(line);
            std::string device;
            std::string mount_point;
            std::string type;
            std::string options;
            if (!(fields >> device >> mount_point >> type >> options)) continue;
            if (is_pseudo_filesystem(type)) continue;
            VolumeReport volume;
            volume.mount_point = unescape_mount_field(mount_point);
            volume.device = non_empty(unescape_mount_field(device));
            volume.filesystem = non_empty(type);
            volume.read_only = options == "ro" || options.rfind("ro,", 0) == 0;
            // Linux has no DONTBROWSE, so the question is answered from
            // what the mount is: storage arrives through a block device
            // under /dev, and the places a distribution keeps its own
            // machinery are named. Anything else -- tmpfs, overlay, a bind
            // mount of a directory -- is bookkeeping.
            volume.system = volume.device.value_or("").rfind("/dev/", 0) != 0 ||
                            volume.mount_point.rfind("/snap/", 0) == 0 ||
                            volume.mount_point.rfind("/var/lib/", 0) == 0 ||
                            volume.mount_point.rfind("/run/", 0) == 0;
            fill_space(volume);
            volumes.push_back(std::move(volume));
        }
    }
#endif

    // Largest first: on a machine with a dozen mounts, the one the reader
    // came to look at is almost never the twelfth.
    std::sort(volumes.begin(), volumes.end(), [](const VolumeReport& left, const VolumeReport& right) {
        return left.capacity_bytes.value_or(0) > right.capacity_bytes.value_or(0);
    });
    return volumes;
}

PowerReport PosixSystemProbe::power() const {
    PowerReport report;

#if defined(__APPLE__)
    const CFTypeRef blob = ::IOPSCopyPowerSourcesInfo();
    if (blob == nullptr) return report;
    const CFArrayRef sources = ::IOPSCopyPowerSourcesList(blob);
    if (sources != nullptr) {
        for (CFIndex index = 0; index < ::CFArrayGetCount(sources); ++index) {
            const CFTypeRef source = ::CFArrayGetValueAtIndex(sources, index);
            const CFDictionaryRef description = ::IOPSGetPowerSourceDescription(blob, source);
            if (description == nullptr) continue;
            const std::optional<std::string> state = cf_string_value(description, CFSTR(kIOPSPowerSourceStateKey));
            if (state.has_value())
                report.source = *state == kIOPSACPowerValue ? PowerReport::Source::Line : PowerReport::Source::Battery;
            const std::optional<int> current = cf_int_value(description, CFSTR(kIOPSCurrentCapacityKey));
            const std::optional<int> maximum = cf_int_value(description, CFSTR(kIOPSMaxCapacityKey));
            if (current.has_value() && maximum.has_value() && *maximum > 0)
                report.charge_percent = static_cast<int>((static_cast<std::int64_t>(*current) * 100 + *maximum / 2) / *maximum);
            report.charging = cf_bool_value(description, CFSTR(kIOPSIsChargingKey));
            if (const std::optional<int> minutes = cf_int_value(description, CFSTR(kIOPSTimeToEmptyKey));
                minutes.has_value() && *minutes > 0)
                report.remaining_seconds = static_cast<std::int64_t>(*minutes) * 60;
            break;  // The internal battery is the first source; the rest are peripherals.
        }
        ::CFRelease(sources);
    }
    ::CFRelease(blob);
#elif defined(__linux__)
    std::error_code error;
    const std::filesystem::path root("/sys/class/power_supply");
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root, error)) {
        const std::optional<std::string> type = sysfs_text((entry.path() / "type").string());
        if (!type.has_value()) continue;
        if (*type == "Battery") {
            report.source = PowerReport::Source::Battery;
            if (const std::optional<std::uint64_t> capacity = sysfs_unsigned((entry.path() / "capacity").string());
                capacity.has_value() && *capacity <= 100)
                report.charge_percent = static_cast<int>(*capacity);
            if (const std::optional<std::string> status = sysfs_text((entry.path() / "status").string()); status.has_value())
                report.charging = *status == "Charging";
            break;
        }
        if (*type == "Mains" && report.source == PowerReport::Source::Unknown) {
            if (const std::optional<std::uint64_t> online = sysfs_unsigned((entry.path() / "online").string());
                online.value_or(0) == 1)
                report.source = PowerReport::Source::Line;
        }
    }
#endif

    return report;
}

BuildReport PosixSystemProbe::build() const { return current_build_report(); }

}  // namespace ckv::sysinfo
