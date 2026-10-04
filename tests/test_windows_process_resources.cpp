// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_process_resources.hpp"
#include "cvision/term/windows_argv.hpp"
#include "cvision/term/windows_text.hpp"
#include "cvision/testing/cktest.hpp"
#include <aclapi.h>

#include <array>
#include <cstdint>
#include <limits>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
using ckv::core::ProcessResourceState;

void show_job(HANDLE job, const char* label, const ckv::core::ProcessResources& resources) {
    std::cerr << "JOB-OBSERVATION " << label << " state=" << static_cast<unsigned>(resources.state)
              << " live=" << resources.live_processes << " unreadable=" << resources.unreadable_processes
              << " error=" << resources.system_error << '\n';
    std::vector<ULONG_PTR> storage(258, 0);
    auto* const list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(storage.data());
    if (!::QueryInformationJobObject(job, JobObjectBasicProcessIdList, list,
                                     static_cast<DWORD>(storage.size() * sizeof(ULONG_PTR)), nullptr)) return;
    constexpr std::size_t header_words = offsetof(JOBOBJECT_BASIC_PROCESS_ID_LIST, ProcessIdList) / sizeof(ULONG_PTR);
    for (std::size_t index = 0; index < list->NumberOfProcessIdsInList; ++index) {
        const DWORD identity = static_cast<DWORD>(storage[header_words + index]);
        const HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, identity);
        std::array<wchar_t, 1024> image{};
        DWORD length = static_cast<DWORD>(image.size());
        if (process != nullptr && ::QueryFullProcessImageNameW(process, 0, image.data(), &length))
            std::cerr << "JOB-MEMBER pid=" << identity << " image="
                      << ckv::term::windows_utf8(std::wstring_view(image.data(), length)).value_or("?") << '\n';
        else std::cerr << "JOB-MEMBER pid=" << identity << " query-error=" << ::GetLastError() << '\n';
        if (process != nullptr) (void)::CloseHandle(process);
    }
}

struct Handle {
    HANDLE value = nullptr;
    explicit Handle(HANDLE input = nullptr) : value(input) {}
    ~Handle() { if (value != nullptr) (void)::CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

// This disposable test process may inherit enabled debugging privilege from
// its host. That privilege bypasses a process DACL. Disable it only within
// this explicit fixture scope and restore the exact prior state on exit;
// the library itself neither enables nor changes any privilege.
class ScopedDebugPrivilege {
public:
    ScopedDebugPrivilege() {
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                                &token_.value)) return;
        TOKEN_PRIVILEGES disabled{};
        disabled.PrivilegeCount = 1;
        if (!::LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &disabled.Privileges[0].Luid)) return;
        disabled.Privileges[0].Attributes = 0;
        DWORD bytes = sizeof(previous_);
        ::SetLastError(ERROR_SUCCESS);
        const BOOL changed = ::AdjustTokenPrivileges(token_.value, FALSE, &disabled,
                                                     bytes, &previous_, &bytes);
        const DWORD error = ::GetLastError();
        valid_ = changed && (error == ERROR_SUCCESS || error == ERROR_NOT_ALL_ASSIGNED);
        restore_ = valid_ && previous_.PrivilegeCount != 0;
        std::cerr << "DEBUG-PRIVILEGE fixture previous-count=" << previous_.PrivilegeCount
                  << " previous-attributes=" << previous_.Privileges[0].Attributes
                  << " error=" << error << '\n';
    }
    ~ScopedDebugPrivilege() {
        if (restore_) {
            ::SetLastError(ERROR_SUCCESS);
            CK_CHECK(::AdjustTokenPrivileges(token_.value, FALSE, &previous_, 0, nullptr, nullptr));
            CK_CHECK(::GetLastError() == ERROR_SUCCESS);
        }
    }
    bool valid() const noexcept { return valid_; }
private:
    Handle token_;
    TOKEN_PRIVILEGES previous_{};
    bool valid_ = false;
    bool restore_ = false;
};

struct Worker {
    Handle ready;
    Handle stop;
    Handle process;
    DWORD identity = 0;

    Worker() {
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        ready.value = ::CreateEventW(&attributes, TRUE, FALSE, nullptr);
        stop.value = ::CreateEventW(&attributes, TRUE, FALSE, nullptr);
    }
    ~Worker() { if (process.value != nullptr) finish(); }
    bool start(HANDLE job, HANDLE inner_job = nullptr) {
        if (ready.value == nullptr || stop.value == nullptr) return false;
        const std::wstring image = ckv::term::windows_utf16(CKV_WINDOWS_PROCESS_WORKER_PATH).value();
        const std::array<std::wstring, 3> arguments{
            image, std::to_wstring(reinterpret_cast<std::uintptr_t>(ready.value)),
            std::to_wstring(reinterpret_cast<std::uintptr_t>(stop.value))};
        const std::array<std::wstring_view, 3> views{arguments[0], arguments[1], arguments[2]};
        auto command = ckv::term::windows_argv_command_line(views);
        if (!command) return false;
        SIZE_T bytes = 0;
        (void)::InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        std::vector<unsigned char> storage(bytes);
        auto* const attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!::InitializeProcThreadAttributeList(attributes, 1, 0, &bytes)) return false;
        const std::array<HANDLE, 2> inherited{ready.value, stop.value};
        const BOOL updated = ::UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            const_cast<HANDLE*>(inherited.data()), sizeof(inherited), nullptr, nullptr);
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.lpAttributeList = attributes;
        PROCESS_INFORMATION child{};
        const BOOL created = updated && ::CreateProcessW(image.c_str(), command->data(), nullptr,
            nullptr, TRUE, CREATE_SUSPENDED | DETACHED_PROCESS | EXTENDED_STARTUPINFO_PRESENT,
            nullptr, nullptr, &startup.StartupInfo, &child);
        ::DeleteProcThreadAttributeList(attributes);
        if (!created) return false;
        process.value = child.hProcess;
        identity = child.dwProcessId;
        Handle thread(child.hThread);
        if (!::AssignProcessToJobObject(job, process.value) ||
            (inner_job != nullptr && !::AssignProcessToJobObject(inner_job, process.value))) {
            (void)::TerminateProcess(process.value, 85);
            return false;
        }
        if (::ResumeThread(thread.value) == static_cast<DWORD>(-1)) return false;
        return ::WaitForSingleObject(ready.value, 8000) == WAIT_OBJECT_0;
    }
    bool finish() {
        (void)::SetEvent(stop.value);
        if (::WaitForSingleObject(process.value, 5000) != WAIT_OBJECT_0) {
            (void)::TerminateProcess(process.value, 86);
            (void)::WaitForSingleObject(process.value, 5000);
            return false;
        }
        DWORD code = 1;
        const bool success = ::GetExitCodeProcess(process.value, &code) && code == 0;
        (void)::CloseHandle(process.value);
        process.value = nullptr;
        return success;
    }
};

HANDLE make_job() {
    const HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
    if (job == nullptr) return nullptr;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        (void)::CloseHandle(job);
        return nullptr;
    }
    return job;
}
}

CK_TEST(windows_process_resources_measure_live_resident_not_virtual_commit) {
    const auto result = ckv::term::sample_windows_process(::GetCurrentProcessId());
    CK_CHECK(result.state == ProcessResourceState::Available);
    CK_CHECK(result.cpu_scope == ckv::core::ProcessCpuScope::ProcessLifetime);
    CK_CHECK(result.cpu_time_nanos.has_value());
    CK_CHECK(result.rss_bytes.has_value());
    CK_CHECK(result.private_rss_bytes.has_value());
    CK_CHECK(result.live_processes == 1);
    CK_CHECK(result.system_error == 0);
    if (result.rss_bytes && result.private_rss_bytes)
        CK_CHECK(*result.private_rss_bytes <= *result.rss_bytes);
    // Reserve/commit without faulting pages in: virtual private commitment
    // must not be counted as private RESIDENT memory.
    constexpr std::size_t reservation = 64 * 1024 * 1024;
    void* const allocation = ::VirtualAlloc(nullptr, reservation, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CK_CHECK(allocation != nullptr);
    if (allocation == nullptr) return;
    const auto reserved = ckv::term::sample_windows_process(::GetCurrentProcessId());
    CK_CHECK(reserved.state == ProcessResourceState::Available);
    if (result.private_rss_bytes && reserved.private_rss_bytes)
        CK_CHECK(*reserved.private_rss_bytes < *result.private_rss_bytes + reservation / 2);
    CK_CHECK(::VirtualFree(allocation, 0, MEM_RELEASE));
}

CK_TEST(windows_process_resources_refuse_invalid_identity_and_ambient_job) {
    for (const ckv::core::ProcessId identity :
         {-1LL, 0LL, static_cast<long long>(std::numeric_limits<DWORD>::max()) + 1}) {
        const auto result = ckv::term::sample_windows_process(identity);
        CK_CHECK(result.state == ProcessResourceState::Failed);
        CK_CHECK(result.system_error == ERROR_INVALID_PARAMETER);
        CK_CHECK(!result.rss_bytes);
    }
    const auto absent = ckv::term::sample_windows_job(nullptr);
    CK_CHECK(absent.state == ProcessResourceState::Failed);
    CK_CHECK(absent.system_error == ERROR_INVALID_HANDLE);
    CK_CHECK(!absent.cpu_time_nanos);
    Handle job(make_job());
    CK_CHECK(job.value != nullptr);
    if (job.value == nullptr) return;
    Handle denied;
    CK_CHECK(::DuplicateHandle(::GetCurrentProcess(), job.value, ::GetCurrentProcess(),
                               &denied.value, JOB_OBJECT_TERMINATE, FALSE, 0));
    if (denied.value == nullptr) return;
    const auto refusal = ckv::term::sample_windows_job(denied.value);
    CK_CHECK(refusal.state == ProcessResourceState::Failed);
    CK_CHECK(refusal.system_error == ERROR_ACCESS_DENIED);
    CK_CHECK(!refusal.cpu_time_nanos);
    const auto empty = ckv::term::sample_windows_job(job.value);
    CK_CHECK(empty.state == ProcessResourceState::Available);
    CK_CHECK(empty.live_processes == 0);
    CK_CHECK(empty.cpu_time_nanos == 0);
    CK_CHECK(empty.rss_bytes == 0);
    CK_CHECK(empty.private_rss_bytes == 0);
}

CK_TEST(windows_job_resources_keep_exited_cpu_but_only_live_memory) {
    Handle job(make_job());
    Handle nested(make_job());
    CK_CHECK(job.value != nullptr && nested.value != nullptr);
    if (job.value == nullptr || nested.value == nullptr) return;
    Worker root;
    Worker descendant;
    CK_CHECK(root.start(job.value));
    CK_CHECK(descendant.start(job.value, nested.value));
    const auto initial = ckv::term::sample_windows_job(job.value);
    show_job(job.value, "initial", initial);
    CK_CHECK(initial.state == ProcessResourceState::Available);
    CK_CHECK(initial.live_processes == 2);
    CK_CHECK(initial.cpu_scope == ckv::core::ProcessCpuScope::OwnedJobLifetime);
    CK_CHECK(initial.cpu_time_nanos.has_value());
    if (initial.cpu_time_nanos) CK_CHECK(*initial.cpu_time_nanos >= 100'000'000);
    CK_CHECK(initial.rss_bytes.has_value());
    CK_CHECK(initial.private_rss_bytes.has_value());
    if (initial.private_rss_bytes) CK_CHECK(*initial.private_rss_bytes >= 8 * 1024 * 1024);
    CK_CHECK(descendant.finish());
    const auto after_exit = ckv::term::sample_windows_job(job.value);
    show_job(job.value, "after-exit", after_exit);
    CK_CHECK(after_exit.state == ProcessResourceState::Available);
    CK_CHECK(after_exit.live_processes == 1);
    if (initial.cpu_time_nanos && after_exit.cpu_time_nanos)
        CK_CHECK(*after_exit.cpu_time_nanos >= *initial.cpu_time_nanos);
    if (initial.private_rss_bytes && after_exit.private_rss_bytes)
        CK_CHECK(*after_exit.private_rss_bytes < *initial.private_rss_bytes);
    CK_CHECK(root.finish());
    const auto empty = ckv::term::sample_windows_job(job.value);
    show_job(job.value, "finished", empty);
    CK_CHECK(empty.live_processes == 0);
    CK_CHECK(empty.rss_bytes == 0);
    CK_CHECK(empty.private_rss_bytes == 0);
    if (initial.cpu_time_nanos && empty.cpu_time_nanos)
        CK_CHECK(*empty.cpu_time_nanos >= *initial.cpu_time_nanos);
}

CK_TEST(windows_job_resources_grow_membership_without_truncation) {
    Handle job(make_job());
    CK_CHECK(job.value != nullptr);
    if (job.value == nullptr) return;
    std::vector<std::unique_ptr<Worker>> workers;
    for (unsigned index = 0; index < 19; ++index) {
        auto worker = std::make_unique<Worker>();
        CK_CHECK(worker->start(job.value));
        workers.push_back(std::move(worker));
    }
    const auto result = ckv::term::sample_windows_job(job.value);
    show_job(job.value, "large", result);
    CK_CHECK(result.state == ProcessResourceState::Available);
    CK_CHECK(result.live_processes == 19);
    CK_CHECK(result.unreadable_processes == 0);
    CK_CHECK(result.system_error == 0);
    if (result.private_rss_bytes) CK_CHECK(*result.private_rss_bytes >= 19 * 4 * 1024 * 1024);
    else CK_CHECK(false);
    for (auto& worker : workers) CK_CHECK(worker->finish());
}

CK_TEST(windows_job_resources_expose_unreadable_memory_without_partial_totals) {
    Handle job(make_job());
    CK_CHECK(job.value != nullptr);
    if (job.value == nullptr) return;
    Worker readable;
    Worker restricted;
    CK_CHECK(readable.start(job.value));
    CK_CHECK(restricted.start(job.value));
    if (restricted.process.value == nullptr) return;

    // Deny only observation's full query/VM-read rights on this owned disposable
    // worker; limited query and existing control handles stay usable. Do not
    // mutate system privilege policy, another process, or the enclosing job's ACL.
    PACL original_acl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const DWORD read_error = ::GetSecurityInfo(restricted.process.value, SE_KERNEL_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, &original_acl, nullptr, &descriptor);
    CK_CHECK(read_error == ERROR_SUCCESS);
    if (read_error != ERROR_SUCCESS) return;
    std::array<unsigned char, SECURITY_MAX_SID_SIZE> everyone{};
    DWORD sid_bytes = static_cast<DWORD>(everyone.size());
    const BOOL sid_created = ::CreateWellKnownSid(WinWorldSid, nullptr, everyone.data(), &sid_bytes);
    CK_CHECK(sid_created);
    if (!sid_created) { (void)::LocalFree(descriptor); return; }
    EXPLICIT_ACCESSW restriction{};
    restriction.grfAccessPermissions = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    restriction.grfAccessMode = DENY_ACCESS;
    restriction.grfInheritance = NO_INHERITANCE;
    restriction.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    restriction.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    restriction.Trustee.ptstrName = reinterpret_cast<LPWSTR>(everyone.data());
    PACL restricted_acl = nullptr;
    const DWORD acl_error = ::SetEntriesInAclW(1, &restriction, original_acl, &restricted_acl);
    CK_CHECK(acl_error == ERROR_SUCCESS);
    if (acl_error == ERROR_SUCCESS) {
        CK_CHECK(::SetSecurityInfo(restricted.process.value, SE_KERNEL_OBJECT,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
            nullptr, nullptr, restricted_acl, nullptr) == ERROR_SUCCESS);
        (void)::LocalFree(restricted_acl);
    }
    (void)::LocalFree(descriptor);

    ScopedDebugPrivilege fixture_privilege;
    CK_CHECK(fixture_privilege.valid());
    if (!fixture_privilege.valid()) return;
    Handle refused(::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE,
                                  FALSE, restricted.identity));
    const DWORD refused_error = ::GetLastError();
    CK_CHECK(refused.value == nullptr);
    if (refused.value == nullptr) CK_CHECK(refused_error == ERROR_ACCESS_DENIED);

    const auto process = ckv::term::sample_windows_process(restricted.identity);
    CK_CHECK(process.state == ProcessResourceState::Partial);
    CK_CHECK(process.system_error == ERROR_ACCESS_DENIED);
    CK_CHECK(process.cpu_time_nanos.has_value());
    CK_CHECK(!process.rss_bytes);
    CK_CHECK(!process.private_rss_bytes);
    const auto total = ckv::term::sample_windows_job(job.value);
    show_job(job.value, "restricted", total);
    CK_CHECK(total.state == ProcessResourceState::Partial);
    CK_CHECK(total.live_processes == 2);
    CK_CHECK(total.unreadable_processes == 1);
    CK_CHECK(total.cpu_time_nanos.has_value());
    CK_CHECK(total.system_error == ERROR_ACCESS_DENIED);
    CK_CHECK(!total.rss_bytes);
    CK_CHECK(!total.private_rss_bytes);
    CK_CHECK(readable.finish());
    CK_CHECK(restricted.finish());
}
