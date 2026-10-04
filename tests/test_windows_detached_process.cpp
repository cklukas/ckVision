// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>
#include "cvision/term/windows_detached_process.hpp"
#include "cvision/term/windows_argv.hpp"
#include "cvision/term/windows_text.hpp"
#include "cvision/testing/cktest.hpp"

namespace {
using namespace ckv::core;
struct OwnedHandle {
    HANDLE value = nullptr;
    OwnedHandle() = default;
    OwnedHandle(const OwnedHandle&) = delete;
    OwnedHandle& operator=(const OwnedHandle&) = delete;
    ~OwnedHandle() { close(); }
    void close() { if (value != nullptr && value != INVALID_HANDLE_VALUE) (void)::CloseHandle(value); value = nullptr; }
};
struct OwnedChild : OwnedHandle {
    ~OwnedChild() {
        if (value != nullptr && ::WaitForSingleObject(value, 0) == WAIT_TIMEOUT) {
            (void)::TerminateProcess(value, 81);
            (void)::WaitForSingleObject(value, 5000);
        }
    }
};
struct WireResult {
    DetachedProcessState state;
    ProcessId identity;
    ProcessNativeError error;
    DetachedProcessCleanup cleanup;
    ProcessNativeError cleanup_error;
};
std::wstring own_image;
bool transfer_file(const std::filesystem::path& file, void* bytes, DWORD size, bool write) {
    OwnedHandle stream;
    stream.value = ::CreateFileW(file.c_str(), write ? GENERIC_WRITE : GENERIC_READ,
        FILE_SHARE_READ, nullptr, write ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (stream.value == INVALID_HANDLE_VALUE) return false;
    DWORD count = 0;
    const bool transferred = write ? ::WriteFile(stream.value, bytes, size, &count, nullptr) != FALSE :
                                    ::ReadFile(stream.value, bytes, size, &count, nullptr) != FALSE;
    return transferred && count == size;
}
bool configure_job(HANDLE job, bool breakaway) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
        (breakaway ? JOB_OBJECT_LIMIT_BREAKAWAY_OK : 0);
    return ::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != FALSE;
}
struct Scope {
    std::filesystem::path directory;
    Scope() {
        const std::filesystem::path root(CKV_TEST_TEMP_ROOT);
        for (unsigned index = 0; index < 10000; ++index) {
            directory = root / (L"detached-\u00fc-" + std::to_wstring(::GetCurrentProcessId()) + L"-" + std::to_wstring(index));
            if (::CreateDirectoryW(directory.c_str(), nullptr)) return;
            if (::GetLastError() != ERROR_ALREADY_EXISTS) break;
        }
        directory.clear();
    }
    ~Scope() {
        if (!directory.empty()) { std::error_code error; std::filesystem::remove_all(directory, error); }
    }
};
void exercise(bool have_outer, bool outer_breakaway, bool have_inner, bool inner_breakaway,
              DetachedProcessState expected) {
    Scope scope;
    CK_CHECK(!scope.directory.empty());
    if (scope.directory.empty()) return;
    OwnedHandle outer, inner, thread;
    OwnedChild controller, detached;
    if (have_outer) {
        outer.value = ::CreateJobObjectW(nullptr, nullptr);
        const bool configured = outer.value != nullptr && configure_job(outer.value, outer_breakaway);
        CK_CHECK(configured);
        if (!configured) return;
    }
    if (have_inner) {
        inner.value = ::CreateJobObjectW(nullptr, nullptr);
        const bool configured = inner.value != nullptr && configure_job(inner.value, inner_breakaway);
        CK_CHECK(configured);
        if (!configured) return;
    }
    const std::array<std::wstring_view, 3> args{own_image, L"--launch", scope.directory.native()};
    auto command = ckv::term::windows_argv_command_line(args);
    CK_CHECK(command.has_value());
    if (!command) return;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    const bool created = ::CreateProcessW(own_image.c_str(), command->data(), nullptr, nullptr, FALSE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB,
        nullptr, scope.directory.c_str(), &startup, &child) != FALSE;
    CK_CHECK(created);
    if (!created) return;
    controller.value = child.hProcess;
    thread.value = child.hThread;
    BOOL member = TRUE;
    const bool initially_independent = ::IsProcessInJob(controller.value, nullptr, &member) && member == FALSE;
    CK_CHECK(initially_independent);
    if (!initially_independent) return;
    const bool assigned = (!have_outer || ::AssignProcessToJobObject(outer.value, controller.value)) &&
                          (!have_inner || ::AssignProcessToJobObject(inner.value, controller.value));
    CK_CHECK(assigned);
    if (!assigned) return;
    CK_CHECK(::ResumeThread(thread.value) != static_cast<DWORD>(-1));
    const DWORD waited = ::WaitForSingleObject(controller.value, 10000);
    CK_CHECK(waited == WAIT_OBJECT_0);
    if (waited != WAIT_OBJECT_0) return;
    DWORD exit = 99;
    CK_CHECK(::GetExitCodeProcess(controller.value, &exit) && exit == 0);
    WireResult result{};
    const bool read = transfer_file(scope.directory / L"result", &result, sizeof(result), false);
    CK_CHECK(read);
    if (!read) return;
    CK_CHECK(result.state == expected);
    CK_CHECK(result.cleanup_error.domain == ProcessErrorDomain::None);
    if (expected == DetachedProcessState::Started) {
        CK_CHECK(result.identity > 0);
        CK_CHECK(result.error.domain == ProcessErrorDomain::None);
        CK_CHECK(result.cleanup == DetachedProcessCleanup::NotNeeded);
        detached.value = ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE,
                                       FALSE, static_cast<DWORD>(result.identity));
        CK_CHECK(detached.value != nullptr);
        if (detached.value == nullptr) return;
        CK_CHECK(::WaitForSingleObject(detached.value, 0) == WAIT_TIMEOUT);
        CK_CHECK(::IsProcessInJob(detached.value, nullptr, &member) && member == FALSE);
        // The actor that can kill a contained process is actually removed here.
        inner.close(); outer.close();
        CK_CHECK(::WaitForSingleObject(detached.value, 250) == WAIT_TIMEOUT);
        // Child's marker also proves native argv/cwd/environment, no console,
        // no inherited stdio and no remaining job from INSIDE the child.
        DWORD marker = 0;
        bool ready = false;
        for (int attempt = 0; attempt < 100 && !ready; ++attempt) {
            ready = transfer_file(scope.directory / L"started", &marker, sizeof(marker), false);
            if (!ready) ::Sleep(10);
        }
        CK_CHECK(ready && marker == static_cast<DWORD>(result.identity));
    } else {
        CK_CHECK(!std::filesystem::exists(scope.directory / L"entered"));
        CK_CHECK(!std::filesystem::exists(scope.directory / L"started"));
        if (expected == DetachedProcessState::ContainmentRetained) {
            CK_CHECK(result.identity > 0);
            CK_CHECK(result.error.domain == ProcessErrorDomain::None);
            CK_CHECK(result.cleanup == DetachedProcessCleanup::Terminated);
        } else {
            CK_CHECK(result.identity == -1);
            CK_CHECK(result.error.domain == ProcessErrorDomain::Win32 && result.error.code == ERROR_ACCESS_DENIED);
            CK_CHECK(result.cleanup == DetachedProcessCleanup::NotNeeded);
        }
        // Wait for job accounting to retire the signaled controller too.
        // Process exit signaling and job accounting publication are separate
        // observations; an instantaneous zero assertion conflates them.
        for (HANDLE job : {outer.value, inner.value}) if (job != nullptr) {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
            bool observed = false;
            for (int attempt = 0; attempt < 500; ++attempt) {
                observed = ::QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                    &accounting, sizeof(accounting), nullptr) != FALSE;
                if (!observed || accounting.ActiveProcesses == 0) break;
                ::Sleep(10);
            }
            CK_CHECK(observed && accounting.ActiveProcesses == 0);
        }
    }
}
int controller(const std::filesystem::path& scope) {
    const auto image = ckv::term::windows_utf8(own_image);
    const auto directory = ckv::term::windows_utf8(scope.native());
    if (!image || !directory) return 90;
    auto spec = ProcessLaunchSpec::program(*image, {"--detached-child", R"(quoted "argument" \)"});
    spec.working_directory = *directory;
    spec.environment.emplace_back("CKV_DETACHED_PROBE", "native-value");
    ckv::term::WindowsDetachedProcessLauncher launcher;
    const auto result = launcher.launch(spec);
    WireResult wire{result.state, result.process_id, result.error, result.cleanup, result.cleanup_error};
    std::fprintf(stderr, "detached state=%d pid=%lld error=%llu cleanup=%d cleanup_error=%llu %s\n",
        static_cast<int>(result.state), static_cast<long long>(result.process_id),
        static_cast<unsigned long long>(result.error.code), static_cast<int>(result.cleanup),
        static_cast<unsigned long long>(result.cleanup_error.code), result.diagnostic.c_str());
    return transfer_file(scope / L"result", &wire, sizeof(wire), true) ? 0 : 91;
}
int detached_child(int argc, wchar_t** argv) {
    // Record entry before testing containment/stdio. Otherwise an incorrectly
    // resumed child could reject itself and make "never ran" pass vacuously.
    DWORD identity = ::GetCurrentProcessId();
    if (!transfer_file(L"entered", &identity, sizeof(identity), true)) return 97;
    if (argc != 3 || std::wstring_view(argv[2]) != LR"(quoted "argument" \)") return 92;
    BOOL member = TRUE;
    if (!::IsProcessInJob(::GetCurrentProcess(), nullptr, &member) || member != FALSE ||
        ::GetConsoleWindow() != nullptr) return 93;
    for (DWORD standard : {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE}) {
        const HANDLE handle = ::GetStdHandle(standard);
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) return 94;
    }
    std::array<wchar_t, 64> environment{};
    if (::GetEnvironmentVariableW(L"CKV_DETACHED_PROBE", environment.data(),
        static_cast<DWORD>(environment.size())) != 12 || std::wstring_view(environment.data()) != L"native-value") return 95;
    if (!transfer_file(L"started", &identity, sizeof(identity), true)) return 96;
    ::Sleep(10000);
    return 0;
}
} // namespace

CK_TEST(windows_detached_process_without_jobs_survives_its_launcher) {
    exercise(false, false, false, false, DetachedProcessState::Started);
}
CK_TEST(windows_detached_process_permissive_job_closure_does_not_kill_child) {
    exercise(true, true, false, false, DetachedProcessState::Started);
}
CK_TEST(windows_detached_process_permissive_nested_jobs_release_all_ancestors) {
    exercise(true, true, true, true, DetachedProcessState::Started);
}
CK_TEST(windows_detached_process_immediate_restrictive_job_refuses_creation) {
    exercise(true, false, false, false, DetachedProcessState::LaunchFailed);
}
CK_TEST(windows_detached_process_restrictive_outer_job_never_runs_child_code) {
    exercise(true, false, true, true, DetachedProcessState::ContainmentRetained);
}
CK_TEST(windows_detached_process_invalid_launch_has_no_child_or_cleanup) {
    ckv::term::WindowsDetachedProcessLauncher launcher;
    const auto result = launcher.launch(ProcessLaunchSpec{});
    CK_CHECK(result.state == DetachedProcessState::InvalidLaunch);
    CK_CHECK(result.process_id == -1);
    CK_CHECK(result.cleanup == DetachedProcessCleanup::NotNeeded);
    CK_CHECK(!result.successful());
}

int wmain(int argc, wchar_t** argv) {
    std::array<wchar_t, 32768> image{};
    const DWORD length = ::GetModuleFileNameW(nullptr, image.data(), static_cast<DWORD>(image.size()));
    if (length == 0 || length >= image.size()) return 89;
    own_image.assign(image.data(), length);
    // Test-host eligibility, not a library launch or application policy. A CI
    // runner can retain a non-breakaway ancestor unknown to this fixture.
    if (argc == 2 && std::wstring_view(argv[1]) == L"--probe-test-host") {
        BOOL member = TRUE;
        if (!::IsProcessInJob(::GetCurrentProcess(), nullptr, &member)) return 89;
        return member == FALSE ? 0 : 42;
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--launch") return controller(argv[2]);
    if (argc >= 2 && std::wstring_view(argv[1]) == L"--detached-child") return detached_child(argc, argv);
    return ::cktest::run_all(0, nullptr);
}
