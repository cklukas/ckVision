// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_terminal_subsession.hpp"

#if defined(_WIN32)

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cwchar>
#include <limits>
#include <utility>
#include <vector>

namespace {

using ckv::term::TerminalCapabilityProfile;
using ckv::term::TerminalEnvironmentPolicy;
using ckv::term::TerminalLaunchSpec;

void close_handle(HANDLE& handle) noexcept {
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) (void)::CloseHandle(handle);
    handle = nullptr;
}

std::wstring widen_utf8(std::string_view value) {
    if (value.size() > static_cast<std::size_t>(INT_MAX) || value.find('\0') != std::string_view::npos)
        return {};
    if (value.empty()) return {};
    const int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                             static_cast<int>(value.size()), nullptr, 0);
    if (count == 0) return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                              result.data(), count) != count)
        return {};
    return result;
}

// CreateProcessW takes a single mutable command line even when the executable
// is supplied separately. Backslashes immediately before a quote or the end
// of an argument need doubling under the documented MSVC argv convention.
std::wstring quote_argument(const std::wstring& argument) {
    std::wstring quoted(1, L'"');
    std::size_t slashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++slashes;
            continue;
        }
        quoted.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        quoted.push_back(character);
    }
    quoted.append(slashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

bool same_name(const std::wstring& left, const std::wstring& right) noexcept {
    return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
}

struct EnvironmentEntry {
    std::wstring name;
    std::wstring value;
};

bool make_environment(const TerminalLaunchSpec& spec, std::vector<wchar_t>& block) {
    std::vector<EnvironmentEntry> entries;
    if (spec.environment_policy == TerminalEnvironmentPolicy::InheritAndOverride) {
        wchar_t* const inherited = ::GetEnvironmentStringsW();
        if (inherited == nullptr) return false;
        for (const wchar_t* entry = inherited; *entry != L'\0'; entry += std::wcslen(entry) + 1) {
            // Windows' drive-current-directory entries begin with '='; their
            // separator is the following '='. Preserve them on inheritance.
            const wchar_t* const separator = std::wcschr(entry + (entry[0] == L'=' ? 1 : 0), L'=');
            if (separator != nullptr)
                entries.push_back({std::wstring(entry, separator), std::wstring(separator + 1)});
        }
        (void)::FreeEnvironmentStringsW(inherited);
    }
    std::vector<std::wstring> named_by_spec;
    for (const auto& [utf8_name, utf8_value] : spec.environment) {
        if (utf8_name.empty() || utf8_name.find('=') != std::string::npos) return false;
        const std::wstring name = widen_utf8(utf8_name);
        const std::wstring value = widen_utf8(utf8_value);
        if (name.empty() || (!utf8_value.empty() && value.empty())) return false;
        if (std::any_of(named_by_spec.begin(), named_by_spec.end(),
                        [&name](const std::wstring& other) { return same_name(name, other); }))
            return false;
        named_by_spec.push_back(name);
        const auto existing = std::find_if(entries.begin(), entries.end(),
                                           [&name](const EnvironmentEntry& entry) {
                                               return same_name(entry.name, name);
                                           });
        if (existing == entries.end()) entries.push_back({name, value});
        else existing->value = value;
    }
    std::sort(entries.begin(), entries.end(), [](const EnvironmentEntry& left, const EnvironmentEntry& right) {
        return ::CompareStringOrdinal(left.name.c_str(), -1, right.name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    });
    for (const EnvironmentEntry& entry : entries) {
        block.insert(block.end(), entry.name.begin(), entry.name.end());
        block.push_back(L'=');
        block.insert(block.end(), entry.value.begin(), entry.value.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    if (entries.empty()) block.push_back(L'\0');
    return true;
}

COORD conpty_size(ckv::Size cells) noexcept {
    const int maximum = static_cast<int>(std::numeric_limits<SHORT>::max());
    return COORD{static_cast<SHORT>(std::clamp(cells.width, 1, maximum)),
                 static_cast<SHORT>(std::clamp(cells.height, 1, maximum))};
}

TerminalCapabilityProfile effective_profile(const TerminalLaunchSpec& spec, bool modern_conpty) {
    TerminalCapabilityProfile profile = spec.profile;
    if (!modern_conpty) profile.sixel = false;
    return profile;
}

}  // namespace

namespace ckv::term {

// ClosePseudoConsole may block while emitting a final frame on older Windows.
// This thread owns only the close call; the session owner keeps draining VT.
DWORD WINAPI WindowsTerminalSubsession::close_pseudoconsole_after_exit(void* context) {
    auto* const work = static_cast<CloseWork*>(context);
    work->close(work->handle);
    return 0;
}

WindowsTerminalSubsession::ConptyRuntime WindowsTerminalSubsession::discover_runtime() {
    ConptyRuntime runtime;
    runtime.create = &::CreatePseudoConsole;
    runtime.resize = &::ResizePseudoConsole;
    runtime.close = &::ClosePseudoConsole;

    std::wstring executable(MAX_PATH, L'\0');
    for (;;) {
        const DWORD size = static_cast<DWORD>(executable.size());
        const DWORD count = ::GetModuleFileNameW(nullptr, executable.data(), size);
        if (count == 0) {
            runtime.error = "unable to locate the Windows executable for app-local ConPTY discovery";
            return runtime;
        }
        if (count < size) {
            executable.resize(count);
            break;
        }
        if (executable.size() >= 32768) {
            runtime.error = "Windows executable path exceeds the supported ConPTY discovery limit";
            return runtime;
        }
        executable.resize(executable.size() * 2);
    }
    const std::size_t separator = executable.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        runtime.error = "Windows executable path has no parent directory";
        return runtime;
    }
    const std::wstring dll_path = executable.substr(0, separator + 1) + L"conpty.dll";
    const DWORD attributes = ::GetFileAttributesW(dll_path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = ::GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
            runtime.error = "unable to inspect app-local conpty.dll (Win32 " + std::to_string(error) + ")";
        return runtime;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        runtime.error = "app-local conpty.dll is a directory";
        return runtime;
    }
    runtime.module = ::LoadLibraryExW(dll_path.c_str(), nullptr,
                                      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (runtime.module == nullptr) {
        runtime.error = "unable to load app-local conpty.dll (Win32 " + std::to_string(::GetLastError()) + ")";
        return runtime;
    }
    const FARPROC create = ::GetProcAddress(runtime.module, "ConptyCreatePseudoConsole");
    const FARPROC resize = ::GetProcAddress(runtime.module, "ConptyResizePseudoConsole");
    const FARPROC close = ::GetProcAddress(runtime.module, "ConptyClosePseudoConsole");
    if (create == nullptr || resize == nullptr || close == nullptr) {
        runtime.error = "app-local conpty.dll lacks the required create/resize/close exports";
        (void)::FreeLibrary(runtime.module);
        runtime.module = nullptr;
        return runtime;
    }
    runtime.create = reinterpret_cast<ConptyRuntime::Create>(create);
    runtime.resize = reinterpret_cast<ConptyRuntime::Resize>(resize);
    runtime.close = reinterpret_cast<ConptyRuntime::Close>(close);
    return runtime;
}

WindowsTerminalSubsession::WindowsTerminalSubsession(TerminalLaunchSpec spec,
                                                     TerminalSubsessionOptions options,
                                                     ConptyRuntime runtime)
    : spec_(std::move(spec)), options_(std::move(options)), runtime_(std::move(runtime)),
      emulator_(effective_profile(spec_, runtime_.module != nullptr), options_) {}

std::unique_ptr<WindowsTerminalSubsession> WindowsTerminalSubsession::launch(TerminalLaunchSpec spec,
                                                                              TerminalSubsessionOptions options) {
    auto session = std::unique_ptr<WindowsTerminalSubsession>(
        new WindowsTerminalSubsession(std::move(spec), std::move(options), discover_runtime()));
    if (!session->runtime_.error.empty()) {
        session->emulator_.mark_failed(session->runtime_.error);
        session->release_native();
        return session;
    }
    if (session->spec_.exit_policy == TerminalExitPolicy::Unspecified) {
        session->emulator_.mark_failed("terminal launch spec did not name an exit policy: set "
                                        "TerminateAfterGrace or WaitForExit");
        return session;
    }
    if (!session->spawn()) {
        session->emulator_.mark_failed(session->failure_reason_.empty()
                                           ? "unable to launch private child ConPTY session"
                                           : session->failure_reason_);
        session->release_native();
    }
    return session;
}

WindowsTerminalSubsession::~WindowsTerminalSubsession() { close(); }

void WindowsTerminalSubsession::fail_at(const char* stage, unsigned long code) {
    failure_reason_ = std::string("ConPTY ") + stage + " failed (Win32/HRESULT " + std::to_string(code) + ")";
}

bool WindowsTerminalSubsession::open_channels() {
    const std::wstring pipe_base = L"\\\\.\\pipe\\ckvision-conpty-" +
                                   std::to_wstring(::GetCurrentProcessId()) + L"-" +
                                   std::to_wstring(reinterpret_cast<std::uintptr_t>(this));
    const DWORD pipe_mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
    output_read_ = ::CreateNamedPipeW((pipe_base + L"-output").c_str(),
                                       PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                       pipe_mode, 1, 64 * 1024, 64 * 1024, 0, nullptr);
    if (output_read_ == INVALID_HANDLE_VALUE) {
        fail_at("output pipe creation", ::GetLastError());
        output_read_ = nullptr;
        return false;
    }
    conpty_output_ = ::CreateFileW((pipe_base + L"-output").c_str(), GENERIC_WRITE, 0, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (conpty_output_ == INVALID_HANDLE_VALUE) {
        fail_at("output pipe connection", ::GetLastError());
        conpty_output_ = nullptr;
        return false;
    }
    input_write_ = ::CreateNamedPipeW((pipe_base + L"-input").c_str(),
                                       PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                       pipe_mode, 1, 64 * 1024, 64 * 1024, 0, nullptr);
    if (input_write_ == INVALID_HANDLE_VALUE) {
        fail_at("input pipe creation", ::GetLastError());
        input_write_ = nullptr;
        return false;
    }
    conpty_input_ = ::CreateFileW((pipe_base + L"-input").c_str(), GENERIC_READ, 0, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (conpty_input_ == INVALID_HANDLE_VALUE) {
        fail_at("input pipe connection", ::GetLastError());
        conpty_input_ = nullptr;
        return false;
    }
    read_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    write_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (read_event_ == nullptr || write_event_ == nullptr) {
        fail_at("I/O event creation", ::GetLastError());
        return false;
    }
    read_operation_.hEvent = read_event_;
    write_operation_.hEvent = write_event_;
    return true;
}

bool WindowsTerminalSubsession::spawn() {
    if (spec_.executable.empty() || spec_.working_directory.empty()) {
        failure_reason_ = "ConPTY launch requires an executable and working directory";
        return false;
    }
    if (spec_.windows_command && (!spec_.arguments.empty() || !spec_.argv0.empty())) {
        failure_reason_ = "cmd command text cannot be combined with arguments or argv[0]";
        return false;
    }
    const std::wstring executable = widen_utf8(spec_.executable);
    const std::wstring directory = widen_utf8(spec_.working_directory);
    const std::wstring argv0 = spec_.argv0.empty() ? executable : widen_utf8(spec_.argv0);
    if (executable.empty() || directory.empty() || argv0.empty()) {
        failure_reason_ = "ConPTY launch contains invalid UTF-8 or NUL in a path or argv[0]";
        return false;
    }
    std::wstring command = quote_argument(argv0);
    if (spec_.windows_command) {
        // cmd scans '/' as an option introducer even in its image token.
        // Encode the explicit image in native spelling; CreateProcess still
        // receives the caller's image path separately, without a PATH search.
        std::wstring native_image = executable;
        std::replace(native_image.begin(), native_image.end(), L'/', L'\\');
        command = quote_argument(native_image) + L" /d";
        if (spec_.windows_command->command) {
            const auto& source = *spec_.windows_command->command;
            const std::wstring text = widen_utf8(source);
            if (!source.empty() && text.empty()) {
                failure_reason_ = "cmd command text contains invalid UTF-8 or NUL";
                return false;
            }
            // /s /c removes the outer pair, preserving command syntax.
            command += L" /s /c \"";
            command += text;
            command += L'"';
        }
    } else {
        for (const std::string& argument : spec_.arguments) {
            const std::wstring wide = widen_utf8(argument);
            if (!argument.empty() && wide.empty()) {
                failure_reason_ = "ConPTY launch contains invalid UTF-8 or NUL in an argument";
                return false;
            }
            command.push_back(L' ');
            command += quote_argument(wide);
        }
    }
    if (command.size() >= 32767) {
        failure_reason_ = "ConPTY launch command line exceeds the Windows limit";
        return false;
    }
    std::vector<wchar_t> environment;
    if (!make_environment(spec_, environment)) {
        failure_reason_ = "ConPTY launch environment contains an invalid or duplicate name/value";
        return false;
    }
    if (!open_channels()) return false;
    const HRESULT created = runtime_.create(conpty_size(emulator_.profile().cells), conpty_input_,
                                                    conpty_output_, 0, &pseudoconsole_);
    if (FAILED(created)) {
        fail_at("creation", static_cast<unsigned long>(created));
        return false;
    }

    SIZE_T attributes_bytes = 0;
    (void)::InitializeProcThreadAttributeList(nullptr, 1, 0, &attributes_bytes);
    void* const attributes_memory = ::HeapAlloc(::GetProcessHeap(), 0, attributes_bytes);
    if (attributes_memory == nullptr) {
        fail_at("attribute allocation", ERROR_OUTOFMEMORY);
        return false;
    }
    auto* const attributes = static_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributes_memory);
    const BOOL initialized = ::InitializeProcThreadAttributeList(attributes, 1, 0, &attributes_bytes);
    if (!initialized) {
        fail_at("attribute initialization", ::GetLastError());
        (void)::HeapFree(::GetProcessHeap(), 0, attributes_memory);
        return false;
    }
    const BOOL attached = ::UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                                       pseudoconsole_, sizeof(pseudoconsole_), nullptr, nullptr);
    if (!attached) fail_at("pseudoconsole attachment", ::GetLastError());

    job_ = ::CreateJobObjectW(nullptr, nullptr);
    if (job_ == nullptr) {
        fail_at("process job creation", ::GetLastError());
        ::DeleteProcThreadAttributeList(attributes);
        (void)::HeapFree(::GetProcessHeap(), 0, attributes_memory);
        return false;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits{};
    job_limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!::SetInformationJobObject(job_, JobObjectExtendedLimitInformation,
                                   &job_limits, sizeof(job_limits))) {
        fail_at("process job policy", ::GetLastError());
        ::DeleteProcThreadAttributeList(attributes);
        (void)::HeapFree(::GetProcessHeap(), 0, attributes_memory);
        return false;
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    // A console parent with redirected standard streams can otherwise have
    // those handles duplicated into the child even when inheritance is off.
    // Explicit null handles let ConPTY install its own private streams.
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION child{};
    command.push_back(L'\0');
    const BOOL launched = attached && ::CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr,
                                                        FALSE, EXTENDED_STARTUPINFO_PRESENT |
                                                                   CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,
                                                        environment.data(), directory.c_str(),
                                                        &startup.StartupInfo, &child);
    if (attached && !launched) fail_at("child creation", ::GetLastError());
    ::DeleteProcThreadAttributeList(attributes);
    (void)::HeapFree(::GetProcessHeap(), 0, attributes_memory);
    if (!launched) return false;
    process_ = child.hProcess;
    process_id_ = child.dwProcessId;
    // Assign before the child can spawn descendants; the job owns the whole
    // process tree and gives bounded close a real escalation primitive.
    if (!::AssignProcessToJobObject(job_, process_)) {
        fail_at("child job assignment", ::GetLastError());
        (void)::TerminateProcess(process_, 1);
        (void)::WaitForSingleObject(process_, 2000);
        (void)::CloseHandle(child.hThread);
        return false;
    }
    if (::ResumeThread(child.hThread) == static_cast<DWORD>(-1)) {
        fail_at("child resume", ::GetLastError());
        (void)::TerminateJobObject(job_, 1);
        (void)::WaitForSingleObject(process_, 2000);
        (void)::CloseHandle(child.hThread);
        return false;
    }
    (void)::CloseHandle(child.hThread);
    // ConPTY owns its copies after child creation; retaining the originals
    // would prevent broken-pipe detection on the host ends at shutdown.
    close_handle(conpty_input_);
    close_handle(conpty_output_);
    if (!begin_read()) {
        fail_at("first output read", ::GetLastError());
        return false;
    }
    refresh_wait_handles();
    return true;
}

bool WindowsTerminalSubsession::begin_read() {
    if (output_read_ == nullptr || output_closed_) return false;
    read_size_ = 0;
    read_offset_ = 0;
    read_operation_ = {};
    read_operation_.hEvent = read_event_;
    (void)::ResetEvent(read_event_);
    DWORD received = 0;
    if (::ReadFile(output_read_, read_buffer_.data(), static_cast<DWORD>(read_buffer_.size()),
                   &received, &read_operation_)) {
        read_size_ = received;
        if (received > 0) (void)::SetEvent(read_event_);
        else output_closed_ = true;
        return true;
    }
    const DWORD error = ::GetLastError();
    if (error == ERROR_IO_PENDING) {
        read_pending_ = true;
        return true;
    }
    if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
        output_closed_ = true;
        return true;
    }
    fail_at("output read", error);
    output_closed_ = true;
    return false;
}

bool WindowsTerminalSubsession::collect_read() {
    if (!read_pending_) return read_offset_ < read_size_;
    DWORD received = 0;
    if (::GetOverlappedResult(output_read_, &read_operation_, &received, FALSE)) {
        read_pending_ = false;
        read_size_ = received;
        read_offset_ = 0;
        if (received == 0) output_closed_ = true;
        return received > 0;
    }
    const DWORD error = ::GetLastError();
    if (error == ERROR_IO_INCOMPLETE) return false;
    read_pending_ = false;
    if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_OPERATION_ABORTED) {
        output_closed_ = true;
        return false;
    }
    fail_at("output completion", error);
    output_closed_ = true;
    return false;
}

void WindowsTerminalSubsession::fail_input_write(const char* stage, DWORD error) {
    close_handle(input_write_);
    write_pending_ = false;
    write_queue_.clear();
    active_write_.clear();
    (void)emulator_.take_pending_input();
    // A child that has already exited normally closes its input channel.
    // Losing that channel while it is still running is a session failure,
    // rather than a silent transition to a permanently unresponsive view.
    if (!closed_ && process_ != nullptr && ::WaitForSingleObject(process_, 0) != WAIT_OBJECT_0) {
        fail_at(stage, error);
        emulator_.mark_failed(failure_reason_);
    }
}

void WindowsTerminalSubsession::pump_write() {
    if (input_write_ == nullptr) return;
    for (;;) {
        if (write_pending_) {
            DWORD written = 0;
            if (!::GetOverlappedResult(input_write_, &write_operation_, &written, FALSE)) {
                const DWORD error = ::GetLastError();
                if (error == ERROR_IO_INCOMPLETE) break;
                fail_input_write("input completion", error);
                break;
            }
            write_pending_ = false;
            active_write_.erase(0, written);
        }
        if (active_write_.empty()) {
            if (write_queue_.empty()) break;
            const std::size_t count = std::min<std::size_t>(write_queue_.size(), 4096);
            active_write_.assign(write_queue_, 0, count);
            write_queue_.erase(0, count);
        }
        write_operation_ = {};
        write_operation_.hEvent = write_event_;
        (void)::ResetEvent(write_event_);
        DWORD written = 0;
        if (::WriteFile(input_write_, active_write_.data(), static_cast<DWORD>(active_write_.size()),
                        &written, &write_operation_)) {
            if (written == 0) break;
            active_write_.erase(0, written);
            continue;
        }
        const DWORD error = ::GetLastError();
        if (error == ERROR_IO_PENDING) {
            write_pending_ = true;
            break;
        }
        fail_input_write("input write", error);
        break;
    }
    refresh_wait_handles();
}

void WindowsTerminalSubsession::send_input(std::string_view bytes) {
    emulator_.send_input(bytes);
    if (input_write_ == nullptr) return;
    const std::size_t queued = write_queue_.size() + active_write_.size();
    if (queued < options_.max_input_bytes) {
        std::string pending = emulator_.take_pending_input();
        const std::size_t available = options_.max_input_bytes - queued;
        if (pending.size() > available) {
            write_queue_.append(pending.data(), available);
            emulator_.send_input(std::string_view(pending).substr(available));
        } else write_queue_ += pending;
    }
    pump_write();
}

void WindowsTerminalSubsession::resize(Size cells, PixelSize cell_pixels) {
    emulator_.resize(cells, cell_pixels);
    if (pseudoconsole_ == nullptr) return;
    const HRESULT resized = runtime_.resize(pseudoconsole_, conpty_size(emulator_.profile().cells));
    if (FAILED(resized)) {
        fail_at("resize", static_cast<unsigned long>(resized));
        emulator_.mark_failed(failure_reason_);
    }
}

void WindowsTerminalSubsession::observe_exit() {
    if (process_ == nullptr || process_id_ == 0 || ::WaitForSingleObject(process_, 0) != WAIT_OBJECT_0)
        return;
    if (pseudoconsole_ != nullptr && close_thread_ == nullptr) {
        // The root process has ended, but the pseudoconsole still owns its
        // output pipe. Its close operation runs independently so this thread
        // can consume every final VT byte before publishing Exited.
        close_work_ = CloseWork{runtime_.close, pseudoconsole_};
        close_thread_ = ::CreateThread(nullptr, 0, close_pseudoconsole_after_exit,
                                        &close_work_, 0, nullptr);
        if (close_thread_ == nullptr) {
            fail_at("exit close thread", ::GetLastError());
            emulator_.mark_failed(failure_reason_);
            return;
        }
        pseudoconsole_ = nullptr;  // Ownership moved to close_thread_.
    }
    if (close_thread_ != nullptr && ::WaitForSingleObject(close_thread_, 0) != WAIT_OBJECT_0)
        return;
    if (!output_closed_ || read_offset_ != read_size_) return;
    if (write_pending_ && input_write_ != nullptr) {
        (void)::CancelIoEx(input_write_, &write_operation_);
        DWORD ignored = 0;
        (void)::GetOverlappedResult(input_write_, &write_operation_, &ignored, TRUE);
        write_pending_ = false;
    }
    close_handle(input_write_);
    write_queue_.clear();
    active_write_.clear();
    DWORD code = 0;
    if (!::GetExitCodeProcess(process_, &code)) code = static_cast<DWORD>(-1);
    emulator_.mark_exited(static_cast<int>(code));
    process_id_ = 0;
    refresh_wait_handles();
}

void WindowsTerminalSubsession::refresh_wait_handles() noexcept {
    wait_handle_count_ = 0;
    if (read_event_ != nullptr && (read_pending_ || read_offset_ < read_size_))
        wait_handles_[wait_handle_count_++] =
            WaitHandle{WaitHandleKind::WindowsHandle, reinterpret_cast<std::uintptr_t>(read_event_)};
    if (write_event_ != nullptr && write_pending_)
        wait_handles_[wait_handle_count_++] =
            WaitHandle{WaitHandleKind::WindowsHandle, reinterpret_cast<std::uintptr_t>(write_event_)};
    if (process_ != nullptr && process_id_ != 0 && close_thread_ == nullptr &&
        emulator_.state() != TerminalSubsessionState::Failed)
        wait_handles_[wait_handle_count_++] =
            WaitHandle{WaitHandleKind::WindowsHandle, reinterpret_cast<std::uintptr_t>(process_)};
    if (close_thread_ != nullptr && ::WaitForSingleObject(close_thread_, 0) == WAIT_TIMEOUT)
        wait_handles_[wait_handle_count_++] =
            WaitHandle{WaitHandleKind::WindowsHandle, reinterpret_cast<std::uintptr_t>(close_thread_)};
}

bool WindowsTerminalSubsession::drain(std::size_t byte_budget) {
    const TerminalSubsessionState before = emulator_.state();
    bool changed = false;
    while (byte_budget > 0) {
        if (!collect_read()) break;
        const std::size_t count = std::min(byte_budget, read_size_ - read_offset_);
        emulator_.feed_output(std::string_view(read_buffer_.data() + read_offset_, count));
        read_offset_ += count;
        byte_budget -= count;
        changed = true;
        if (read_offset_ == read_size_ && !begin_read()) {
            emulator_.mark_failed(failure_reason_);
            break;
        }
    }
    send_input({});  // Child query replies stay on the private ConPTY input channel.
    observe_exit();
    refresh_wait_handles();
    return changed || emulator_.state() != before;
}

void WindowsTerminalSubsession::release_native() noexcept {
    wait_handle_count_ = 0;
    if (read_pending_ && output_read_ != nullptr) {
        (void)::CancelIoEx(output_read_, &read_operation_);
        DWORD ignored = 0;
        (void)::GetOverlappedResult(output_read_, &read_operation_, &ignored, TRUE);
        read_pending_ = false;
    }
    if (write_pending_ && input_write_ != nullptr) {
        (void)::CancelIoEx(input_write_, &write_operation_);
        DWORD ignored = 0;
        (void)::GetOverlappedResult(input_write_, &write_operation_, &ignored, TRUE);
        write_pending_ = false;
    }
    // The buffers and OVERLAPPED structures remain alive until cancellation
    // has completed; only then may their handles and storage be released.
    // On Windows releases before 11 24H2 ClosePseudoConsole may wait for a
    // final frame. Close its output sink first so that frame cannot block it.
    close_handle(output_read_);
    close_handle(input_write_);
    close_handle(conpty_input_);
    close_handle(conpty_output_);
    close_handle(job_);
    if (close_thread_ != nullptr) {
        (void)::WaitForSingleObject(close_thread_, INFINITE);
        close_handle(close_thread_);
    } else if (pseudoconsole_ != nullptr) {
        runtime_.close(pseudoconsole_);
        pseudoconsole_ = nullptr;
    }
    close_handle(read_event_);
    close_handle(write_event_);
    close_handle(process_);
    process_id_ = 0;
    if (runtime_.module != nullptr) {
        (void)::FreeLibrary(runtime_.module);
        runtime_.module = nullptr;
    }
}

void WindowsTerminalSubsession::request_termination() noexcept {
    if (closed_ || termination_requested_ || process_ == nullptr ||
        ::WaitForSingleObject(process_, 0) != WAIT_TIMEOUT)
        return;
    termination_requested_ = true;
    try { send_input("\x03"); } catch (...) { /* A host may still request escalation. */ }
}

void WindowsTerminalSubsession::request_kill() noexcept {
    if (closed_ || kill_requested_ || job_ == nullptr) return;
    if (::TerminateJobObject(job_, 1)) kill_requested_ = true;
}

void WindowsTerminalSubsession::close() noexcept {
    if (closed_) return;
    if (process_ != nullptr && ::WaitForSingleObject(process_, 0) != WAIT_OBJECT_0) {
        // Control-C is interpreted by ConPTY for a console child. The write is
        // asynchronous; output remains drained while the child has grace to
        // finish. WaitForExit deliberately has no deadline or escalation.
        request_termination();
        const ULONGLONG grace_started = ::GetTickCount64();
        while (::WaitForSingleObject(process_, 0) != WAIT_OBJECT_0 &&
               (spec_.exit_policy == TerminalExitPolicy::WaitForExit ||
                ::GetTickCount64() - grace_started < 1000)) {
            if (collect_read()) {
                read_offset_ = read_size_;  // Closing discards output but keeps the pipe moving.
                (void)begin_read();
            }
            if (write_pending_ && input_write_ != nullptr) {
                DWORD written = 0;
                if (::GetOverlappedResult(input_write_, &write_operation_, &written, FALSE)) {
                    write_pending_ = false;
                    active_write_.erase(0, written);
                }
            }
            try { pump_write(); } catch (...) { /* Teardown still owns the native handles. */ }
            // The monotonic deadline bounds grace even when a VM sleeps much
            // longer than the requested interval or a pipe event stays signaled.
            ::Sleep(10);
        }
        if (spec_.exit_policy == TerminalExitPolicy::TerminateAfterGrace &&
            ::WaitForSingleObject(process_, 0) != WAIT_OBJECT_0) {
            request_kill();
            // Job termination can complete after TerminateJobObject returns.
            // The bounded policy must not return while its root child remains
            // observable as running; release_native still closes the job so
            // descendants cannot outlive the session.
            (void)::WaitForSingleObject(process_, 2000);
        }
    }
    closed_ = true;
    release_native();
    emulator_.close();
}

}  // namespace ckv::term

#endif
