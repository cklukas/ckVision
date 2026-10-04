// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_wait_set.hpp"
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal.hpp"
#include "cvision/testing/cktest.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {
using ckv::term::WaitHandle;
using ckv::term::WaitHandleKind;
using ckv::term::WindowsWaitSet;

struct OwnedHandle {
    HANDLE value = nullptr;
    explicit OwnedHandle(HANDLE handle) : value(handle) {}
    ~OwnedHandle() { if (value && value != INVALID_HANDLE_VALUE) ::CloseHandle(value); }
    OwnedHandle(const OwnedHandle&) = delete;
    OwnedHandle& operator=(const OwnedHandle&) = delete;
    WaitHandle source() const { return {WaitHandleKind::WindowsHandle, reinterpret_cast<std::uintptr_t>(value)}; }
};

struct FixedClock final : ckv::Clock {
    std::int64_t now = 0;
    std::int64_t now_nanos() const noexcept override { return now; }
};

bool contains(std::span<const WaitHandle> ready, WaitHandle source) {
    return std::find(ready.begin(), ready.end(), source) != ready.end();
}

struct FreshConsole {
    bool allocated;
    OwnedHandle input;
    OwnedHandle output;
    FreshConsole() : allocated((::FreeConsole(), ::AllocConsole()) != FALSE),
        input(::CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)),
        output(::CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)) {}
    ~FreshConsole() { if (allocated) ::FreeConsole(); }
};

struct OwnedChild {
    PROCESS_INFORMATION information;
    explicit OwnedChild(PROCESS_INFORMATION process) : information(process) {}
    ~OwnedChild() {
        DWORD code = 0;
        if (::GetExitCodeProcess(information.hProcess, &code) && code == STILL_ACTIVE) {
            (void)::TerminateProcess(information.hProcess, 99);
            (void)::WaitForSingleObject(information.hProcess, 1000);
        }
        ::CloseHandle(information.hThread);
        ::CloseHandle(information.hProcess);
    }
    OwnedChild(const OwnedChild&) = delete;
    OwnedChild& operator=(const OwnedChild&) = delete;
    WaitHandle source() const {
        return {WaitHandleKind::WindowsHandle,
            reinterpret_cast<std::uintptr_t>(information.hProcess)};
    }
};
}  // namespace

CK_TEST(windows_wait_set_delivers_130_sources_before_and_after_environment_changes) {
    ckv::term::WindowsClock clock;
    WindowsWaitSet waits(clock);
    std::vector<std::unique_ptr<OwnedHandle>> events;
    std::vector<WaitHandle> sources;
    for (int i = 0; i < 130; ++i) {
        events.push_back(std::make_unique<OwnedHandle>(::CreateEventW(nullptr, TRUE, FALSE, nullptr)));
        CK_CHECK(events.back()->value != nullptr);
        sources.push_back(events.back()->source());
    }
    CK_CHECK(waits.wait(clock.now_nanos(), sources).empty());
    // The environment changes while registrations are live, not only before
    // constructing the wait set. Both ends lie beyond a Win32 64-object batch.
    for (const auto& event : events) CK_CHECK(::SetEvent(event->value));
    std::vector<WaitHandle> delivered;
    for (int batch = 0; delivered.size() < sources.size() && batch < 130; ++batch) {
        const auto ready = waits.wait(clock.now_nanos() + 1'000'000'000, sources);
        CK_CHECK(!ready.empty());
        for (const auto source : ready) {
            CK_CHECK(!contains(delivered, source));
            delivered.push_back(source);
            CK_CHECK(::ResetEvent(reinterpret_cast<HANDLE>(source.value)));
        }
    }
    CK_CHECK(delivered.size() == 130);
    for (const auto source : sources) CK_CHECK(contains(delivered, source));
    CK_CHECK(waits.wait(clock.now_nanos(), sources).empty());
    waits.clear();
}

CK_TEST(windows_wait_set_coalesces_duplicates_and_rearms_after_consumption) {
    ckv::term::WindowsClock clock;
    OwnedHandle event(::CreateEventW(nullptr, TRUE, TRUE, nullptr));
    WindowsWaitSet waits(clock);
    const std::array sources{event.source(), event.source(),
        WaitHandle{WaitHandleKind::WindowsHandle, 0}, WaitHandle{WaitHandleKind::PosixFileDescriptor, 77}};
    auto ready = waits.wait(clock.now_nanos(), sources);
    CK_CHECK(ready.size() == 1 && ready.front() == event.source());
    ready = waits.wait(clock.now_nanos(), sources);  // still signaled, fresh association
    CK_CHECK(ready.size() == 1 && ready.front() == event.source());
    CK_CHECK(::ResetEvent(event.value));
    CK_CHECK(waits.wait(clock.now_nanos(), sources).empty());
    CK_CHECK(::SetEvent(event.value));
    ready = waits.wait(clock.now_nanos() + 1'000'000'000, sources);
    CK_CHECK(ready.size() == 1 && ready.front() == event.source());
    waits.clear();
    CK_CHECK(waits.wait(std::numeric_limits<std::int64_t>::max(), {}).empty());
}

CK_TEST(windows_wait_set_a_hot_source_does_not_hide_other_ready_sources) {
    ckv::term::WindowsClock clock;
    OwnedHandle hot(::CreateEventW(nullptr, TRUE, TRUE, nullptr));
    OwnedHandle peer(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    WindowsWaitSet waits(clock);
    const std::array sources{hot.source(), peer.source()};
    CK_CHECK(contains(waits.wait(clock.now_nanos() + 1'000'000'000, sources), hot.source()));
    CK_CHECK(::SetEvent(peer.value));
    bool saw_peer = false;
    for (int batch = 0; batch < 16 && !saw_peer; ++batch) {
        const auto ready = waits.wait(clock.now_nanos() + 1'000'000'000, sources);
        saw_peer = contains(ready, peer.source());
        CK_CHECK(ready.size() <= 2);
    }
    CK_CHECK(saw_peer);
    waits.clear();
}

CK_TEST(windows_wait_set_retires_completions_during_concurrent_cancellation) {
    ckv::term::WindowsClock clock;
    WindowsWaitSet waits(clock);
    for (int iteration = 0; iteration < 200; ++iteration) {
        OwnedHandle old(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
        const std::array old_sources{old.source()};
        CK_CHECK(waits.wait(clock.now_nanos(), old_sources).empty());
        // Only the test uses a short-lived signaler. The wait backend creates
        // no thread. Removal races actual kernel signaling/completion queuing.
        std::thread signaler([handle = old.value] { (void)::SetEvent(handle); });
        waits.clear();
        signaler.join();
        OwnedHandle replacement(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
        const std::array sources{replacement.source()};
        CK_CHECK(waits.wait(clock.now_nanos(), sources).empty());
        CK_CHECK(::SetEvent(replacement.value));
        const auto ready = waits.wait(clock.now_nanos() + 1'000'000'000, sources);
        CK_CHECK(ready.size() == 1 && ready.front() == replacement.source());
        waits.clear();
    }
}

CK_TEST(windows_wait_set_reports_invalid_sources_and_recovers_without_stale_work) {
    ckv::term::WindowsClock clock;
    WindowsWaitSet waits(clock);
    const std::array bad{WaitHandle{WaitHandleKind::WindowsHandle,
        reinterpret_cast<std::uintptr_t>(INVALID_HANDLE_VALUE)}};
    bool refused = false;
    try { (void)waits.wait(clock.now_nanos(), bad); }
    catch (const std::system_error& error) { refused = error.code().value() == ERROR_INVALID_HANDLE; }
    CK_CHECK(refused);
    OwnedHandle valid(::CreateEventW(nullptr, TRUE, TRUE, nullptr));
    const std::array sources{valid.source()};
    CK_CHECK(contains(waits.wait(clock.now_nanos() + 1'000'000'000, sources), valid.source()));
    waits.clear();
}

CK_TEST(windows_wait_set_clear_releases_owned_native_handles) {
    ckv::term::WindowsClock clock;
    WindowsWaitSet waits(clock);
    DWORD before = 0;
    CK_CHECK(::GetProcessHandleCount(::GetCurrentProcess(), &before));
    for (int iteration = 0; iteration < 100; ++iteration) {
        OwnedHandle event(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
        const std::array sources{event.source()};
        CK_CHECK(waits.wait(clock.now_nanos(), sources).empty());
        waits.clear();
    }
    DWORD after = 0;
    CK_CHECK(::GetProcessHandleCount(::GetCurrentProcess(), &after));
    CK_CHECK(after == before);
}

CK_TEST(windows_wait_sets_do_not_consume_or_clear_each_others_readiness) {
    ckv::term::WindowsClock clock;
    OwnedHandle event(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    const std::array sources{event.source()};
    WindowsWaitSet first(clock);
    WindowsWaitSet second(clock);
    CK_CHECK(first.wait(clock.now_nanos(), sources).empty());
    CK_CHECK(second.wait(clock.now_nanos(), sources).empty());
    CK_CHECK(::SetEvent(event.value));
    CK_CHECK(contains(first.wait(clock.now_nanos() + 1'000'000'000, sources), event.source()));
    first.clear();
    CK_CHECK(contains(second.wait(clock.now_nanos() + 1'000'000'000, sources), event.source()));
    CK_CHECK(::ResetEvent(event.value));
    CK_CHECK(second.wait(clock.now_nanos(), sources).empty());
    second.clear();
}

CK_TEST(windows_wait_set_refuses_a_closed_source_before_packet_allocation_can_reuse_it) {
    ckv::term::WindowsClock clock;
    WindowsWaitSet waits(clock);
    const HANDLE event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    CK_CHECK(event != nullptr);
    if (!event) return;
    const std::array closed{WaitHandle{WaitHandleKind::WindowsHandle,
        reinterpret_cast<std::uintptr_t>(event)}};
    CK_CHECK(::CloseHandle(event));
    bool refused = false;
    try { (void)waits.wait(clock.now_nanos(), closed); }
    catch (const std::system_error& error) { refused = error.code().value() == ERROR_INVALID_HANDLE; }
    CK_CHECK(refused);
    OwnedHandle replacement(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    const std::array sources{replacement.source()};
    CK_CHECK(waits.wait(clock.now_nanos(), sources).empty());
    CK_CHECK(::SetEvent(replacement.value));
    CK_CHECK(contains(waits.wait(clock.now_nanos() + 1'000'000'000, sources), replacement.source()));
    waits.clear();
}

CK_TEST(windows_wait_set_observes_a_native_process_exiting_after_registration) {
    std::wstring image(32768, L'\0');
    const UINT count = ::GetSystemDirectoryW(image.data(), static_cast<UINT>(image.size()));
    CK_CHECK(count > 0 && count < image.size());
    if (count == 0 || count >= image.size()) return;
    image.resize(count);
    image += L"\\cmd.exe";
    std::wstring command = L"\"" + image + L"\" /d /c \"exit 37\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION information{};
    const BOOL created = ::CreateProcessW(image.c_str(), command.data(), nullptr,
        nullptr, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr,
        &startup, &information);
    CK_CHECK(created);
    if (!created) return;
    OwnedChild child(information);
    ckv::term::WindowsClock clock;
    WindowsWaitSet waits(clock);
    const std::array sources{child.source()};
    CK_CHECK(waits.wait(clock.now_nanos(), sources).empty());
    CK_CHECK(::ResumeThread(information.hThread) != static_cast<DWORD>(-1));
    CK_CHECK(contains(waits.wait(clock.now_nanos() + 2'000'000'000, sources), child.source()));
    DWORD code = 0;
    CK_CHECK(::GetExitCodeProcess(information.hProcess, &code));
    CK_CHECK(code == 37);
    // An exited process remains ready; a new association must still report it.
    CK_CHECK(contains(waits.wait(clock.now_nanos() + 1'000'000'000, sources), child.source()));
    waits.clear();
}

CK_TEST(windows_wait_set_and_public_terminal_support_isolated_console_input_beyond_64_sources) {
    FreshConsole console;
    CK_CHECK(console.allocated);
    CK_CHECK(console.input.value != INVALID_HANDLE_VALUE && console.output.value != INVALID_HANDLE_VALUE);
    if (!console.allocated || console.input.value == INVALID_HANDLE_VALUE || console.output.value == INVALID_HANDLE_VALUE) return;
    CK_CHECK(::FlushConsoleInputBuffer(console.input.value));
    FixedClock clock;
    std::vector<std::unique_ptr<OwnedHandle>> events;
    std::vector<WaitHandle> sources;
    for (int i = 0; i < 130; ++i) {
        events.push_back(std::make_unique<OwnedHandle>(::CreateEventW(nullptr, TRUE, FALSE, nullptr)));
        sources.push_back(events.back()->source());
    }
    ckv::term::WindowsTerminal terminal(clock, console.output.value, console.input.value);
    clock.now = 1'000'000'000;  // finish the silent probe before the extra-source wait
    (void)terminal.poll(clock.now, sources);
    CK_CHECK(::SetEvent(events.back()->value));
    CK_CHECK(terminal.poll(std::numeric_limits<std::int64_t>::max(), sources).empty());
    CK_CHECK(::ResetEvent(events.back()->value));
    terminal.wake();
    CK_CHECK(terminal.poll(std::numeric_limits<std::int64_t>::max(), sources).empty());
    INPUT_RECORD record{};
    record.EventType = KEY_EVENT;
    record.Event.KeyEvent.bKeyDown = TRUE;
    record.Event.KeyEvent.wRepeatCount = 1;
    record.Event.KeyEvent.uChar.UnicodeChar = L'X';
    DWORD written = 0;
    CK_CHECK(::WriteConsoleInputW(console.input.value, &record, 1, &written));
    CK_CHECK(written == 1);
    const auto input = terminal.poll(std::numeric_limits<std::int64_t>::max(), sources);
    CK_CHECK(!input.empty());
    CK_CHECK(std::any_of(input.begin(), input.end(), [](const auto& event) {
        const auto* key = std::get_if<ckv::KeyEvent>(&event);
        return key && key->chord.key == ckv::Key::Char && key->chord.text == "X";
    }));
    record.Event.KeyEvent.uChar.UnicodeChar = L'\x1B';
    CK_CHECK(::WriteConsoleInputW(console.input.value, &record, 1, &written));
    CK_CHECK(written == 1);
    CK_CHECK(terminal.poll(clock.now, sources).empty());
    // No more console input is sent. A caller's indefinite wait must still
    // resolve the decoder-owned Escape deadline instead of hanging forever.
    clock.now += ckv::term::kEscTimeoutNanos;
    const auto escaped = terminal.poll(std::numeric_limits<std::int64_t>::max(), sources);
    CK_CHECK(std::any_of(escaped.begin(), escaped.end(), [](const auto& event) {
        const auto* key = std::get_if<ckv::KeyEvent>(&event);
        return key && key->chord.key == ckv::Key::Escape;
    }));
    terminal.restore();
}
