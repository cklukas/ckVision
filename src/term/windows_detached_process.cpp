// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_detached_process.hpp"
#include "cvision/term/process_launch_internal.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ckv::term {
namespace {
struct OwnedHandle {
    HANDLE value;
    ~OwnedHandle() { if (value != nullptr) (void)::CloseHandle(value); }
};
core::ProcessNativeError win32_error(DWORD code) {
    return {core::ProcessErrorDomain::Win32, code};
}
void clean_suspended_child(HANDLE child, core::DetachedProcessResult& result) {
    // An ancestor may have terminated it while containment was being checked.
    if (::WaitForSingleObject(child, 0) == WAIT_OBJECT_0) {
        result.cleanup = core::DetachedProcessCleanup::Terminated;
        return;
    }
    if (!::TerminateProcess(child, ERROR_PROCESS_ABORTED)) {
        const DWORD error = ::GetLastError();
        if (::WaitForSingleObject(child, 0) == WAIT_OBJECT_0) {
            result.cleanup = core::DetachedProcessCleanup::Terminated;
            return;
        }
        result.cleanup = core::DetachedProcessCleanup::Failed;
        result.cleanup_error = win32_error(error);
        return;
    }
    const DWORD waited = ::WaitForSingleObject(child, 5000);
    if (waited == WAIT_OBJECT_0) result.cleanup = core::DetachedProcessCleanup::Terminated;
    else {
        result.cleanup = core::DetachedProcessCleanup::Failed;
        result.cleanup_error = win32_error(waited == WAIT_FAILED ? ::GetLastError() : ERROR_TIMEOUT);
    }
}
} // namespace

core::DetachedProcessResult WindowsDetachedProcessLauncher::launch(const core::ProcessLaunchSpec& spec) {
    core::DetachedProcessResult result;
    detail::WindowsLaunchData launch;
    detail::ProcessPreparationError preparation;
    if (!detail::prepare_windows_process_launch(spec, launch, preparation)) {
        result.error = preparation.native;
        result.diagnostic = std::move(preparation.diagnostic);
        return result;
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    const DWORD flags = DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT |
                        CREATE_BREAKAWAY_FROM_JOB | CREATE_SUSPENDED;
    if (!::CreateProcessW(launch.executable.c_str(), launch.command.data(), nullptr, nullptr, FALSE,
                          flags, launch.environment.data(), launch.directory.c_str(), &startup, &child)) {
        result.state = core::DetachedProcessState::LaunchFailed;
        result.error = win32_error(::GetLastError());
        result.diagnostic = "cannot create a console-detached process with job breakaway";
        return result;
    }
    const OwnedHandle process{child.hProcess}, thread{child.hThread};
    result.process_id = static_cast<core::ProcessId>(child.dwProcessId);
    BOOL in_job = FALSE;
    // BREAKAWAY can succeed at an inner job and stop at a restrictive ancestor.
    // A null job handle checks ANY remaining job, not just the immediate one.
    if (!::IsProcessInJob(process.value, nullptr, &in_job)) {
        result.state = core::DetachedProcessState::ContainmentQueryFailed;
        result.error = win32_error(::GetLastError());
        result.diagnostic = "cannot verify detached process job independence";
    } else if (in_job != FALSE) {
        result.state = core::DetachedProcessState::ContainmentRetained;
        result.diagnostic = "an ancestor job prevents an independently living process";
    } else if (::ResumeThread(thread.value) == static_cast<DWORD>(-1)) {
        result.state = core::DetachedProcessState::ResumeFailed;
        result.error = win32_error(::GetLastError());
        result.diagnostic = "cannot resume the verified detached process";
    } else {
        result.state = core::DetachedProcessState::Started;
        return result;
    }
    clean_suspended_child(process.value, result);
    return result;
}

} // namespace ckv::term
