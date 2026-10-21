// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "windows_system_probe.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cwchar>
#include <string_view>
#include <limits>
#include <map>

namespace ckv::sysinfo {
namespace {
std::optional<std::string> utf8(std::wstring_view text) {
    if (text.empty() || text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
    const auto count = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string value(static_cast<std::size_t>(count), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), value.data(), count, nullptr, nullptr) != count) return {};
    return value;
}
std::optional<std::string> registry_string(const wchar_t* key, const wchar_t* name) {
    DWORD size = 0;
    if (::RegGetValueW(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS || size < sizeof(wchar_t) || size > 32768 || size % sizeof(wchar_t)) return {};
    std::wstring text(size / sizeof(wchar_t), L'\0');
    if (::RegGetValueW(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_SZ, nullptr, text.data(), &size) != ERROR_SUCCESS) return {};
    text.resize(size / sizeof(wchar_t));
    if (text.empty() || text.back() != L'\0') return {};
    text.pop_back();
    if (text.find(L'\0') != text.npos) return {};
    return utf8(text);
}
std::optional<DWORD> registry_number(const wchar_t* key, const wchar_t* name) {
    DWORD value = 0, size = sizeof value;
    if (::RegGetValueW(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS || size != sizeof value) return {};
    return value;
}
constexpr const wchar_t* version_key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
}
HostReport WindowsSystemProbe::host() const {
    HostReport result;
    DWORD size = 0;
    ::GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size);
    if (size > 0 && size <= 32768) {
        std::wstring name(size, L'\0');
        if (::GetComputerNameExW(ComputerNameDnsHostname, name.data(), &size)) result.host_name = utf8(std::wstring_view(name).substr(0, size));
    }
    wchar_t user[257]; size = 257;
    if (::GetUserNameW(user, &size) && size > 1) result.user_name = utf8(std::wstring_view(user, size - 1));
    result.os_name = "Microsoft Windows";
    if (const auto display = registry_string(version_key, L"DisplayVersion")) *result.os_name += " " + *display;
    if (const auto edition = registry_string(version_key, L"EditionID")) *result.os_name += " (" + *edition + ")";
    if (const auto build_number = registry_string(version_key, L"CurrentBuildNumber")) {
        result.kernel = "Windows NT build " + *build_number;
        if (const auto revision = registry_number(version_key, L"UBR")) *result.kernel += "." + std::to_string(*revision);
    }
    SYSTEM_INFO info{}; ::GetNativeSystemInfo(&info);
    switch (info.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: result.architecture = "x86_64"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: result.architecture = "arm64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: result.architecture = "x86"; break;
        default: break;
    }
    result.uptime_seconds = static_cast<std::int64_t>(::GetTickCount64() / 1000);
    return result;
}
MemoryReport WindowsSystemProbe::memory() const {
    MemoryReport result;
    MEMORYSTATUSEX memory{}; memory.dwLength = sizeof memory;
    if (::GlobalMemoryStatusEx(&memory)) {
        result.total_bytes = memory.ullTotalPhys; result.available_bytes = memory.ullAvailPhys;
        PERFORMANCE_INFORMATION performance{};
        performance.cb = sizeof performance;
        if (::GetPerformanceInfo(&performance, sizeof performance) && performance.PageSize > 0) {
            const auto retain = [&](const char* name, SIZE_T pages) {
                if (pages <= std::numeric_limits<std::uint64_t>::max() / performance.PageSize)
                    result.detail.emplace_back(name, static_cast<std::uint64_t>(pages) * performance.PageSize);
            };
            retain("Commit limit", performance.CommitLimit); retain("Committed", performance.CommitTotal);
        }
        // Commit accounting is not physical swap capacity or swap usage.
    }
    return result;
}
ProcessorReport WindowsSystemProbe::processor() const {
    ProcessorReport result;
    constexpr auto key = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    result.brand = registry_string(key, L"ProcessorNameString");
    // Registry ~MHz is a boot estimate, not a trustworthy nominal frequency.
    DWORD size = 0;
    ::GetLogicalProcessorInformationEx(RelationAll, nullptr, &size);
    if (size == 0 || size > 4 * 1024 * 1024) return result;
    std::vector<std::byte> data(size);
    if (!::GetLogicalProcessorInformationEx(RelationAll, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(data.data()), &size)) return result;
    int physical = 0, logical = 0;
    std::map<BYTE, int> classes;
    for (std::size_t offset = 0; offset < size;) {
        if (size - offset < sizeof(LOGICAL_PROCESSOR_RELATIONSHIP) + sizeof(DWORD)) return result;
        const auto* entry = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(data.data() + offset);
        if (entry->Size == 0 || entry->Size > size - offset) return result;
        if (entry->Relationship == RelationProcessorCore) {
            if (entry->Size < offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor.GroupMask)) return result;
            const auto& cpu = entry->Processor;
            if (entry->Size < offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor.GroupMask) + static_cast<std::size_t>(cpu.GroupCount) * sizeof(GROUP_AFFINITY)) return result;
            ++physical; ++classes[cpu.EfficiencyClass];
            for (WORD group = 0; group < cpu.GroupCount; ++group) logical += std::popcount(cpu.GroupMask[group].Mask);
        } else if (entry->Relationship == RelationCache) {
            if (entry->Size < offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Cache) + sizeof(CACHE_RELATIONSHIP)) return result;
            const auto& cache = entry->Cache;
            auto retain = [&](std::optional<std::uint64_t>& target) { target = std::max(target.value_or(0), static_cast<std::uint64_t>(cache.CacheSize)); };
            if (cache.Level == 1 && (cache.Type == CacheData || cache.Type == CacheUnified)) retain(result.l1d_cache_bytes);
            if (cache.Level == 2) retain(result.l2_cache_bytes);
            if (cache.Level == 3) retain(result.l3_cache_bytes);
        }
        offset += entry->Size;
    }
    if (physical > 0) result.physical_cores = physical;
    if (logical > 0) result.logical_cores = logical;
    if (classes.size() > 1) {
        result.performance_cores = classes.rbegin()->second;
        result.efficiency_cores = physical - *result.performance_cores;
    }
    return result;
}
std::vector<VolumeReport> WindowsSystemProbe::volumes() const {
    const DWORD size = ::GetLogicalDriveStringsW(0, nullptr);
    if (size == 0 || size > 32768) return {};
    std::wstring paths(size + 1, L'\0');
    if (!::GetLogicalDriveStringsW(static_cast<DWORD>(paths.size()), paths.data())) return {};
    std::vector<VolumeReport> result;
    for (const wchar_t* path = paths.c_str(); *path; path += std::wcslen(path) + 1) {
        const auto mount = utf8(path);
        if (!mount) continue;
        VolumeReport volume; volume.mount_point = *mount; volume.device = *mount;
        const auto kind = ::GetDriveTypeW(path);
        if (kind == DRIVE_FIXED || kind == DRIVE_REMOVABLE || kind == DRIVE_RAMDISK || kind == DRIVE_CDROM) {
            ULARGE_INTEGER available{}, total{};
            if (::GetDiskFreeSpaceExW(path, &available, &total, nullptr)) { volume.capacity_bytes = total.QuadPart; volume.free_bytes = available.QuadPart; }
            wchar_t filesystem[256]; DWORD flags = 0;
            if (::GetVolumeInformationW(path, nullptr, 0, nullptr, nullptr, &flags, filesystem, 256)) {
                volume.filesystem = utf8(filesystem); volume.read_only = (flags & FILE_READ_ONLY_VOLUME) != 0;
            }
        }
        result.push_back(std::move(volume));
    }
    return result;
}
PowerReport WindowsSystemProbe::power() const {
    PowerReport result;
    SYSTEM_POWER_STATUS status{};
    if (!::GetSystemPowerStatus(&status)) return result;
    if (status.ACLineStatus == 1) result.source = PowerReport::Source::Line;
    else if (status.ACLineStatus == 0) result.source = PowerReport::Source::Battery;
    if (status.BatteryFlag != 255 && (status.BatteryFlag & 128) == 0) {
        if (status.BatteryLifePercent <= 100) result.charge_percent = status.BatteryLifePercent;
        result.charging = (status.BatteryFlag & 8) != 0;
        if (status.BatteryLifeTime != static_cast<DWORD>(-1)) result.remaining_seconds = status.BatteryLifeTime;
    }
    return result;
}
BuildReport WindowsSystemProbe::build() const { return current_build_report(); }
} // namespace ckv::sysinfo
