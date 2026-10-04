// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/process_runner.hpp"
#include "cvision/term/process_launch_internal.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace ckv::term {
namespace {
using namespace core;

class Handle {
public:
    HANDLE value = nullptr;
    ~Handle() { close(); }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    void close() noexcept {
        if (value != nullptr && value != INVALID_HANDLE_VALUE) (void)::CloseHandle(value);
        value = nullptr;
    }
};

void fail(ProcessRunResult& result, ProcessRunState state, const char* stage, DWORD code) {
    if (result.error.domain != ProcessErrorDomain::None || result.state == ProcessRunState::IdleTimeout) return;
    result.state = state;
    result.error = {ProcessErrorDomain::Win32, code};
    result.diagnostic = std::string(stage) + " (Win32 " + std::to_string(code) + ")";
}

// Private endpoints may only be opened by the effective process user. Default
// named-pipe security grants Everyone read access, which is not private stdin.
class PipeSecurity {
public:
    ~PipeSecurity() { if (descriptor_ != nullptr) (void)::LocalFree(descriptor_); }
    bool prepare() {
        Handle token;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token.value)) return false;
        DWORD needed = 0;
        (void)::GetTokenInformation(token.value, TokenUser, nullptr, 0, &needed);
        if (needed == 0) return false;
        std::vector<unsigned char> user(needed);
        if (!::GetTokenInformation(token.value, TokenUser, user.data(), needed, &needed)) return false;
        wchar_t* sid = nullptr;
        if (!::ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid, &sid)) return false;
        const std::wstring acl = L"D:P(A;;GA;;;" + std::wstring(sid) + L")";
        (void)::LocalFree(sid);
        if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(), SDDL_REVISION_1,
                                                                     &descriptor_, nullptr)) return false;
        attributes = {sizeof(SECURITY_ATTRIBUTES), descriptor_, FALSE};
        return true;
    }
    SECURITY_ATTRIBUTES attributes{};
private:
    PSECURITY_DESCRIPTOR descriptor_ = nullptr;
};

// One outstanding operation per endpoint. Cancel-and-join precedes destruction:
// OVERLAPPED and its buffer must remain alive even after CancelIoEx returns.
struct Channel {
    Handle parent;
    Handle child;
    Handle event;
    OVERLAPPED operation{};
    std::array<char, 16 * 1024> buffer{};
    bool pending = false;
    bool input = false;

    ~Channel() { close_parent(); }
    void close_parent() noexcept {
        if (pending) {
            (void)::CancelIoEx(parent.value, &operation);
            DWORD transferred = 0;
            (void)::GetOverlappedResult(parent.value, &operation, &transferred, TRUE);
            pending = false;
        }
        parent.close();
    }
    bool open(const std::wstring& name, bool writing, PipeSecurity& security) {
        input = writing;
        parent.value = ::CreateNamedPipeW(name.c_str(),
            (input ? PIPE_ACCESS_OUTBOUND : PIPE_ACCESS_INBOUND) |
                FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1, 64 * 1024, 64 * 1024, 0, &security.attributes);
        if (parent.value == INVALID_HANDLE_VALUE) return false;
        auto inherited = security.attributes;
        inherited.bInheritHandle = TRUE;
        child.value = ::CreateFileW(name.c_str(), input ? GENERIC_READ : GENERIC_WRITE,
            0, &inherited, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
        if (child.value == INVALID_HANDLE_VALUE) return false;
        event.value = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (event.value == nullptr) return false;
        operation.hEvent = event.value;
        return true;
    }

    // false means a native error; zero transferred with no error means pending
    // or EOF. Reads retain only a prefix, but continue draining past the cap.
    bool pump(const ProcessRunRequest& request, ProcessRunResult& result,
              ProcessCapture* capture, std::size_t cap, DWORD& error, bool& progress) {
        if (parent.value == nullptr) return true;
        DWORD transferred = 0;
        BOOL completed = FALSE;
        if (pending) {
            completed = ::GetOverlappedResult(parent.value, &operation, &transferred, FALSE);
            if (!completed && ::GetLastError() == ERROR_IO_INCOMPLETE) return true;
            pending = false;
        } else {
            if (input && result.input_bytes_written == request.input.size()) {
                close_parent();
                return true;
            }
            if (!::ResetEvent(event.value)) { error = ::GetLastError(); return false; }
            operation = {};
            operation.hEvent = event.value;
            if (input) {
                const auto count = std::min(buffer.size(), request.input.size() - result.input_bytes_written);
                completed = ::WriteFile(parent.value, request.input.data() + result.input_bytes_written,
                                         static_cast<DWORD>(count), &transferred, &operation);
            } else {
                completed = ::ReadFile(parent.value, buffer.data(), static_cast<DWORD>(buffer.size()),
                                        &transferred, &operation);
            }
            if (!completed && ::GetLastError() == ERROR_IO_PENDING) { pending = true; return true; }
        }
        if (!completed) {
            const DWORD code = ::GetLastError();
            if (!input && (code == ERROR_BROKEN_PIPE || code == ERROR_PIPE_NOT_CONNECTED)) {
                close_parent();
                return true;
            }
            error = code;
            return false;
        }
        if (transferred == 0) {
            if (!input) close_parent();
            else { error = ERROR_WRITE_FAULT; return false; }
            return true;
        }
        progress = true;
        if (input) {
            result.input_bytes_written += transferred;
            if (result.input_bytes_written == request.input.size()) close_parent();
        } else {
            const auto count = static_cast<std::size_t>(transferred);
            const auto keep = std::min(count, cap - capture->bytes.size());
            capture->bytes.append(buffer.data(), keep);
            if (keep != count) capture->truncated = true;
        }
        return true;
    }
};

struct Attributes {
    std::vector<unsigned char> storage;
    PPROC_THREAD_ATTRIBUTE_LIST list = nullptr;
    ~Attributes() { if (list != nullptr) ::DeleteProcThreadAttributeList(list); }
    bool prepare(std::array<HANDLE, 3>& handles) {
        SIZE_T bytes = 0;
        (void)::InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        storage.resize(bytes);
        auto* value = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!::InitializeProcThreadAttributeList(value, 1, 0, &bytes)) return false;
        list = value;
        return ::UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                           handles.data(), sizeof(handles), nullptr, nullptr) != FALSE;
    }
};

DWORD idle_slice(const Clock& clock, std::int64_t progress, std::int64_t budget) {
    const auto elapsed = clock.now_nanos() - progress;
    const auto remaining = budget - std::min(budget, std::max<std::int64_t>(0, elapsed));
    if (remaining == 0) return 0;
    return static_cast<DWORD>(std::min<std::int64_t>(20, 1 + (remaining - 1) / 1'000'000));
}

} // namespace

core::ProcessRunResult NativeProcessRunner::run(const core::ProcessRunRequest& request) {
    ProcessRunResult result;
    result.input_bytes_total = request.input.size();
    if (!valid_process_request(request)) {
        result.diagnostic = "invalid helper invocation, idle budget or descendant policy";
        return result;
    }
    detail::WindowsLaunchData launch;
    detail::ProcessPreparationError preparation;
    if (!detail::prepare_windows_process_launch(request.launch, launch, preparation)) {
        result.state = ProcessRunState::LaunchFailed;
        result.error = preparation.native;
        result.diagnostic = preparation.diagnostic;
        return result;
    }
    PipeSecurity security;
    if (!security.prepare()) {
        fail(result, ProcessRunState::LaunchFailed, "helper private pipe security", ::GetLastError());
        return result;
    }
    Channel input, output, errors;
    // Released descendants can retain old client ends. A reused stack address
    // is therefore not a unique pipe name, even for sequential executions.
    std::array<UCHAR, 16> nonce{};
    const NTSTATUS random_status = ::BCryptGenRandom(nullptr, nonce.data(), static_cast<ULONG>(nonce.size()),
                                                     BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (random_status != 0) {
        result.state = ProcessRunState::LaunchFailed;
        result.error = {ProcessErrorDomain::NtStatus, static_cast<std::uint32_t>(random_status)};
        result.diagnostic = "helper private pipe nonce generation failed (NTSTATUS " + std::to_string(result.error.code) + ")";
        return result;
    }
    std::wstring base = L"\\\\.\\pipe\\ckvision-helper-" + std::to_wstring(::GetCurrentProcessId()) + L"-";
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (const auto byte : nonce) { base.push_back(hex[byte >> 4]); base.push_back(hex[byte & 15]); }
    if (!input.open(base + L"-in", true, security) || !output.open(base + L"-out", false, security) ||
        !errors.open(base + L"-err", false, security)) {
        fail(result, ProcessRunState::LaunchFailed, "helper private pipe setup", ::GetLastError());
        return result;
    }
    Handle job;
    job.value = ::CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job.value == nullptr || !::SetInformationJobObject(job.value, JobObjectExtendedLimitInformation,
                                                          &limits, sizeof(limits))) {
        fail(result, ProcessRunState::LaunchFailed, "helper owned job setup", ::GetLastError());
        return result;
    }
    std::array<HANDLE, 3> inherited{input.child.value, output.child.value, errors.child.value};
    Attributes attributes;
    if (!attributes.prepare(inherited)) {
        fail(result, ProcessRunState::LaunchFailed, "helper inherited stdio whitelist", ::GetLastError());
        return result;
    }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.child.value;
    startup.StartupInfo.hStdOutput = output.child.value;
    startup.StartupInfo.hStdError = errors.child.value;
    startup.lpAttributeList = attributes.list;
    PROCESS_INFORMATION created{};
    launch.command.push_back(L'\0');
    if (!::CreateProcessW(launch.executable.c_str(), launch.command.data(), nullptr, nullptr, TRUE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
        launch.environment.data(), launch.directory.c_str(), &startup.StartupInfo, &created)) {
        fail(result, ProcessRunState::LaunchFailed, "helper CreateProcessW", ::GetLastError());
        return result;
    }
    Handle process, thread;
    process.value = created.hProcess;
    thread.value = created.hThread;
    input.child.close(); output.child.close(); errors.child.close();
    if (!::AssignProcessToJobObject(job.value, process.value)) {
        const DWORD code = ::GetLastError();
        (void)::TerminateProcess(process.value, 1);
        (void)::WaitForSingleObject(process.value, INFINITE);
        fail(result, ProcessRunState::LaunchFailed, "helper suspended-root job assignment", code);
        return result;
    }
    if (::ResumeThread(thread.value) == static_cast<DWORD>(-1)) {
        const DWORD code = ::GetLastError();
        (void)::TerminateJobObject(job.value, 1);
        (void)::WaitForSingleObject(process.value, INFINITE);
        fail(result, ProcessRunState::LaunchFailed, "helper root resume", code);
        return result;
    }
    thread.close();
    auto last_progress = clock_.now_nanos();
    result.state = ProcessRunState::Completed;
    if (request.input.empty()) input.close_parent();
    const auto pump = [&] {
        // Each stream gets at most 256 KiB per pass, including cap overflow.
        for (int pass = 0; pass < 16; ++pass) {
            bool progress = false;
            DWORD error = ERROR_SUCCESS;
            const bool wrote = input.pump(request, result, nullptr, 0, error, progress);
            if (!wrote) { fail(result, ProcessRunState::IoFailed, "helper stdin transfer", error); input.close_parent(); }
            const bool read_output = output.pump(request, result, &result.stdout_capture, request.max_stdout_bytes, error, progress);
            if (!read_output) fail(result, ProcessRunState::IoFailed, "helper stdout capture", error);
            const bool read_errors = errors.pump(request, result, &result.stderr_capture, request.max_stderr_bytes, error, progress);
            if (!read_errors) fail(result, ProcessRunState::IoFailed, "helper stderr capture", error);
            if (!wrote || !read_output || !read_errors) return false;
            if (!progress) break;
            last_progress = clock_.now_nanos();
        }
        return true;
    };
    for (;;) {
        const DWORD root = ::WaitForSingleObject(process.value, 0);
        if (root == WAIT_OBJECT_0) {
            DWORD status = 0;
            if (!::GetExitCodeProcess(process.value, &status))
                fail(result, ProcessRunState::ExitStatusUnavailable, "helper root exit observation", ::GetLastError());
            else result.exit = ProcessExitStatus{ProcessExitKind::Normal, status};
            // A surviving descendant may retain these pipes: drain ready bytes
            // only, never wait for EOF. Existing pending stdin is observed too.
            (void)pump();
            break;
        }
        if (root == WAIT_FAILED) {
            fail(result, ProcessRunState::ExitStatusUnavailable, "helper root wait", ::GetLastError());
            break;
        }
        const DWORD slice = idle_slice(clock_, last_progress, request.idle_budget_nanos);
        if (slice == 0) {
            result.state = ProcessRunState::IdleTimeout;
            result.diagnostic = "helper made no byte-transfer progress within its idle budget";
            break;
        }
        if (!pump()) break;
        std::array<HANDLE, 4> waiting{};
        DWORD count = 0;
        waiting[count++] = process.value;
        for (Channel* channel : {&input, &output, &errors})
            if (channel->pending) waiting[count++] = channel->event.value;
        if (::WaitForMultipleObjects(count, waiting.data(), FALSE,
              idle_slice(clock_, last_progress, request.idle_budget_nanos)) == WAIT_FAILED) {
            fail(result, ProcessRunState::IoFailed, "helper I/O wait", ::GetLastError());
            break;
        }
    }
    const bool release = result.successful() && request.descendants == ProcessDescendantPolicy::ReleaseOnSuccess;
    if (release) {
        limits.BasicLimitInformation.LimitFlags = 0;
        if (!::SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
            fail(result, ProcessRunState::IoFailed, "helper successful descendant release", ::GetLastError());
            (void)::TerminateJobObject(job.value, 1);
        }
    } else if (!::TerminateJobObject(job.value, 1)) {
        if (result.successful()) fail(result, ProcessRunState::IoFailed, "helper owned job termination", ::GetLastError());
    }
    if (!release || !result.successful()) {
        // Ensure root teardown before discarding its identity. Job close still
        // kills members if a prior native cleanup operation failed.
        (void)::WaitForSingleObject(process.value, INFINITE);
        if (!result.exit) {
            DWORD status = 0;
            if (::GetExitCodeProcess(process.value, &status)) result.exit = ProcessExitStatus{ProcessExitKind::Normal, status};
        }
    }
    return result;
}

} // namespace ckv::term
#endif
