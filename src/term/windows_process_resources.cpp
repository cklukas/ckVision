// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_process_resources.hpp"

#if defined(_WIN32)
#include <psapi.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <new>
#include <vector>

namespace ckv::term {
namespace {
using core::ProcessResources;
using core::ProcessResourceState;

struct ProcessHandle {
    HANDLE value = nullptr;
    ~ProcessHandle() { if (value != nullptr) (void)::CloseHandle(value); }
    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
    explicit ProcessHandle(HANDLE handle) : value(handle) {}
};

void failed_field(ProcessResources& result, DWORD error) noexcept {
    result.state = ProcessResourceState::Partial;
    if (result.system_error == 0) result.system_error = error;
}

bool add_checked(std::uint64_t& total, std::uint64_t value) noexcept {
    if (value > std::numeric_limits<std::uint64_t>::max() - total) return false;
    total += value;
    return true;
}

std::optional<std::uint64_t> cpu_nanos(std::uint64_t user, std::uint64_t kernel) noexcept {
    if (!add_checked(user, kernel) || user > std::numeric_limits<std::uint64_t>::max() / 100)
        return std::nullopt;
    return user * 100;
}

std::uint64_t file_time(FILETIME time) noexcept {
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

bool grow_buffer(std::vector<ULONG_PTR>& buffer, std::size_t required) {
    constexpr std::size_t max_words = std::numeric_limits<DWORD>::max() / sizeof(ULONG_PTR);
    if (required > max_words || buffer.size() > max_words / 2) return false;
    buffer.assign(std::max(required, buffer.size() * 2), 0);
    return true;
}

void sample_memory(HANDLE process, ProcessResources& result) {
    // QueryWorkingSet is available throughout the supported ConPTY OS range.
    // Count actual resident pages, not PrivateUsage's virtual commit charge;
    // no EX2-only field or manifest-dependent OS-version guess is involved.
    std::vector<ULONG_PTR> buffer(1024, 0);
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        const DWORD bytes = static_cast<DWORD>(buffer.size() * sizeof(ULONG_PTR));
        if (::K32QueryWorkingSet(process, buffer.data(), bytes)) {
            const std::size_t count = static_cast<std::size_t>(buffer.front());
            if (count >= buffer.size()) {
                failed_field(result, ERROR_INVALID_DATA);
                return;
            }
            std::uint64_t private_pages = 0;
            for (std::size_t index = 0; index < count; ++index) {
                PSAPI_WORKING_SET_BLOCK page{};
                page.Flags = buffer[index + 1];
                if (!page.Shared) ++private_pages;
            }
            SYSTEM_INFO system{};
            ::GetSystemInfo(&system);
            result.rss_bytes = static_cast<std::uint64_t>(count) * system.dwPageSize;
            result.private_rss_bytes = private_pages * system.dwPageSize;
            return;
        }
        const DWORD error = ::GetLastError();
        if (error != ERROR_BAD_LENGTH) {
            failed_field(result, error);
            return;
        }
        const std::size_t count = static_cast<std::size_t>(buffer.front());
        if (count == std::numeric_limits<std::size_t>::max() || !grow_buffer(buffer, count + 1)) {
            failed_field(result, ERROR_ARITHMETIC_OVERFLOW);
            return;
        }
    }
    failed_field(result, ERROR_RETRY);
}

ProcessResources sample_handle(HANDLE process, bool memory_access) {
    ProcessResources result;
    const DWORD wait = ::WaitForSingleObject(process, 0);
    if (wait == WAIT_OBJECT_0) {
        result.state = ProcessResourceState::Gone;
        return result;
    }
    if (wait != WAIT_TIMEOUT) {
        result.state = ProcessResourceState::Failed;
        result.system_error = ::GetLastError();
        return result;
    }
    result.state = ProcessResourceState::Available;
    result.live_processes = 1;
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (::GetProcessTimes(process, &creation, &exit, &kernel, &user)) {
        result.cpu_time_nanos = cpu_nanos(file_time(user), file_time(kernel));
        if (!result.cpu_time_nanos) failed_field(result, ERROR_ARITHMETIC_OVERFLOW);
    } else failed_field(result, ::GetLastError());
    if (memory_access) sample_memory(process, result);
    else failed_field(result, ERROR_ACCESS_DENIED);
    // Do not expose a just-exited process's stale resident set as live cost.
    if (::WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
        result = {};
        result.state = ProcessResourceState::Gone;
    }
    return result;
}

HANDLE open_process(DWORD pid, bool& memory_access) noexcept {
    memory_access = true;
    HANDLE result = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE,
                                  FALSE, pid);
    if (result == nullptr && ::GetLastError() == ERROR_ACCESS_DENIED) {
        memory_access = false;
        result = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    }
    return result;
}

ProcessResources sample_job(HANDLE job) {
    ProcessResources result;
    result.cpu_scope = core::ProcessCpuScope::OwnedJobLifetime;
    if (job == nullptr || job == INVALID_HANDLE_VALUE) {
        result.state = ProcessResourceState::Failed;
        result.system_error = ERROR_INVALID_HANDLE;
        return result;
    }
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
    if (!::QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                                     &accounting, sizeof(accounting), nullptr)) {
        result.state = ProcessResourceState::Failed;
        result.system_error = ::GetLastError();
        return result;
    }
    result.state = ProcessResourceState::Available;
    if (accounting.TotalUserTime.QuadPart >= 0 && accounting.TotalKernelTime.QuadPart >= 0)
        result.cpu_time_nanos = cpu_nanos(static_cast<std::uint64_t>(accounting.TotalUserTime.QuadPart),
                                        static_cast<std::uint64_t>(accounting.TotalKernelTime.QuadPart));
    if (!result.cpu_time_nanos) failed_field(result, ERROR_ARITHMETIC_OVERFLOW);

    constexpr std::size_t header_words =
        offsetof(JOBOBJECT_BASIC_PROCESS_ID_LIST, ProcessIdList) / sizeof(ULONG_PTR);
    std::vector<ULONG_PTR> buffer(header_words + 16, 0);
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        auto* const list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(buffer.data());
        const DWORD bytes = static_cast<DWORD>(buffer.size() * sizeof(ULONG_PTR));
        const BOOL complete = ::QueryInformationJobObject(job, JobObjectBasicProcessIdList,
                                                          list, bytes, nullptr);
        const DWORD error = complete ? ERROR_SUCCESS : ::GetLastError();
        if (!complete && error != ERROR_MORE_DATA) {
            failed_field(result, error);
            return result;
        }
        const std::size_t count = list->NumberOfProcessIdsInList;
        if (complete && count == list->NumberOfAssignedProcesses &&
            count <= buffer.size() - header_words) {
            std::uint64_t rss = 0;
            std::uint64_t private_rss = 0;
            bool memory_complete = true;
            for (std::size_t index = 0; index < count; ++index) {
                const ULONG_PTR identity = buffer[header_words + index];
                if (identity == 0 || identity > std::numeric_limits<DWORD>::max()) {
                    ++result.unreadable_processes;
                    memory_complete = false;
                    failed_field(result, ERROR_INVALID_DATA);
                    continue;
                }
                bool memory_access = false;
                ProcessHandle process(open_process(static_cast<DWORD>(identity), memory_access));
                if (process.value == nullptr) {
                    const DWORD open_error = ::GetLastError();
                    if (open_error == ERROR_INVALID_PARAMETER) continue;  // Exit raced opening.
                    ++result.unreadable_processes;
                    memory_complete = false;
                    failed_field(result, open_error);
                    continue;
                }
                BOOL member = FALSE;
                if (!::IsProcessInJob(process.value, job, &member)) {
                    ++result.unreadable_processes;
                    memory_complete = false;
                    failed_field(result, ::GetLastError());
                    continue;
                }
                if (!member) continue;  // PID reused outside this owned job.
                const ProcessResources observation = sample_handle(process.value, memory_access);
                if (observation.state == ProcessResourceState::Gone) continue;
                result.live_processes += observation.live_processes;
                if (!observation.rss_bytes || !observation.private_rss_bytes) {
                    ++result.unreadable_processes;
                    memory_complete = false;
                    failed_field(result, observation.system_error);
                } else if (!add_checked(rss, *observation.rss_bytes) ||
                           !add_checked(private_rss, *observation.private_rss_bytes)) {
                    memory_complete = false;
                    failed_field(result, ERROR_ARITHMETIC_OVERFLOW);
                }
            }
            if (memory_complete) {
                result.rss_bytes = rss;
                result.private_rss_bytes = private_rss;
            }
            return result;
        }
        const std::size_t required = static_cast<std::size_t>(list->NumberOfAssignedProcesses);
        if (required > std::numeric_limits<std::size_t>::max() - header_words ||
            !grow_buffer(buffer, required + header_words)) {
            failed_field(result, ERROR_ARITHMETIC_OVERFLOW);
            return result;
        }
    }
    failed_field(result, ERROR_RETRY);
    return result;
}
}  // namespace

core::ProcessResources sample_windows_process(core::ProcessId identity) noexcept {
    ProcessResources result;
    if (identity <= 0 || static_cast<std::uint64_t>(identity) > std::numeric_limits<DWORD>::max()) {
        result.state = ProcessResourceState::Failed;
        result.system_error = ERROR_INVALID_PARAMETER;
        return result;
    }
    bool memory_access = false;
    ProcessHandle process(open_process(static_cast<DWORD>(identity), memory_access));
    if (process.value == nullptr) {
        result.system_error = ::GetLastError();
        result.state = result.system_error == ERROR_INVALID_PARAMETER ?
            ProcessResourceState::Gone : ProcessResourceState::Failed;
        return result;
    }
    try { return sample_handle(process.value, memory_access); }
    catch (const std::bad_alloc&) {
        result.state = ProcessResourceState::Failed;
        result.system_error = ERROR_NOT_ENOUGH_MEMORY;
        return result;
    }
}

core::ProcessResources sample_windows_job(HANDLE job) noexcept {
    try { return sample_job(job); }
    catch (const std::bad_alloc&) {
        ProcessResources result;
        result.state = ProcessResourceState::Failed;
        result.cpu_scope = core::ProcessCpuScope::OwnedJobLifetime;
        result.system_error = ERROR_NOT_ENOUGH_MEMORY;
        return result;
    }
}
}  // namespace ckv::term
#endif
