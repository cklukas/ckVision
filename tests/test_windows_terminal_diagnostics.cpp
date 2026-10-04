// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_terminal.hpp"
#include "cvision/term/record_replay_terminal.hpp"
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal_subsession.hpp"
#include "cvision/testing/cktest.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
struct TestClock final : ckv::Clock {
    std::int64_t now = 0;
    std::int64_t now_nanos() const noexcept override { return now; }
};

struct OwnedHandle {
    HANDLE value;
    explicit OwnedHandle(HANDLE handle) : value(handle) {}
    ~OwnedHandle() { if (value && value != INVALID_HANDLE_VALUE) ::CloseHandle(value); }
    OwnedHandle(const OwnedHandle&) = delete;
    OwnedHandle& operator=(const OwnedHandle&) = delete;
};

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
    bool valid() const {
        return allocated && input.value && input.value != INVALID_HANDLE_VALUE &&
            output.value && output.value != INVALID_HANDLE_VALUE;
    }
};

bool inject(FreshConsole& console, std::string_view bytes) {
    for (const unsigned char byte : bytes) {
        INPUT_RECORD record{};
        record.EventType = KEY_EVENT;
        record.Event.KeyEvent.bKeyDown = TRUE;
        record.Event.KeyEvent.wRepeatCount = 1;
        record.Event.KeyEvent.uChar.UnicodeChar = static_cast<WCHAR>(byte);
        DWORD written = 0;
        if (!::WriteConsoleInputW(console.input.value, &record, 1, &written) || written != 1) return false;
    }
    return true;
}

std::size_t mouse_events(const std::vector<ckv::term::TerminalEvent>& events) {
    return static_cast<std::size_t>(std::count_if(events.begin(), events.end(),
        [](const auto& event) { return std::holds_alternative<ckv::MouseEvent>(event); }));
}

std::wstring read_cells(HANDLE output, COORD at, DWORD count) {
    std::wstring result(count, L'\0');
    DWORD read = 0;
    if (!::ReadConsoleOutputCharacterW(output, result.data(), count, at, &read)) return {};
    result.resize(read);
    return result;
}
}  // namespace

CK_TEST(windows_terminal_capture_orders_real_output_and_idempotent_restoration) {
    FreshConsole console;
    CK_CHECK(console.valid());
    if (!console.valid()) return;
    TestClock clock;
    std::string captured;
    ckv::term::WindowsTerminal terminal(clock, console.output.value, console.input.value);
    terminal.set_output_capture([&](std::string_view bytes) { captured.append(bytes); });
    CK_CHECK(captured.empty());  // No constructor output delivered retroactively.
    CONSOLE_SCREEN_BUFFER_INFO before{};
    CK_CHECK(::GetConsoleScreenBufferInfo(console.output.value, &before));
    terminal.write("CAPTURE-MARKER");
    CK_CHECK(read_cells(console.output.value, before.dwCursorPosition, 14) == L"CAPTURE-MARKER");
    terminal.bell();
    terminal.set_title("Native\nTitle");
    CK_CHECK(captured == "CAPTURE-MARKER\x07\x1B]0;NativeTitle\x07");
    const auto written_size = captured.size();
    terminal.restore();
    CK_CHECK(captured.size() > written_size);
    CK_CHECK(captured.find("\x1B[?1049l", written_size) != std::string::npos);
    CK_CHECK(captured.ends_with("\x1B]22;\x1B\\"));
    const auto restored = captured;
    terminal.restore();
    CK_CHECK(captured == restored);
    terminal.set_output_capture({});
}

CK_TEST(windows_terminal_capture_failure_cannot_suppress_native_output_or_restore) {
    FreshConsole console;
    CK_CHECK(console.valid());
    if (!console.valid()) return;
    TestClock clock;
    ckv::term::WindowsTerminal terminal(clock, console.output.value, console.input.value);
    int attempts = 0;
    terminal.set_output_capture([&](std::string_view) {
        ++attempts;
        throw std::runtime_error("test sink unavailable");
    });
    CONSOLE_SCREEN_BUFFER_INFO before{};
    CK_CHECK(::GetConsoleScreenBufferInfo(console.output.value, &before));
    terminal.write("SINK-FAILURE");
    CK_CHECK(attempts == 1);
    CK_CHECK(read_cells(console.output.value, before.dwCursorPosition, 12) == L"SINK-FAILURE");
    terminal.restore();
    CK_CHECK(attempts == 3);  // payload, restoration sequence, pointer reset.
    DWORD output_mode = 0;
    CK_CHECK(::GetConsoleMode(console.output.value, &output_mode));
    terminal.set_output_capture({});
}

CK_TEST(windows_terminal_empty_capture_disables_only_the_observation) {
    FreshConsole console;
    CK_CHECK(console.valid());
    if (!console.valid()) return;
    TestClock clock;
    std::string captured;
    ckv::term::WindowsTerminal terminal(clock, console.output.value, console.input.value);
    terminal.set_output_capture([&](std::string_view bytes) { captured.append(bytes); });
    terminal.write("FIRST");
    terminal.set_output_capture({});
    CONSOLE_SCREEN_BUFFER_INFO before{};
    CK_CHECK(::GetConsoleScreenBufferInfo(console.output.value, &before));
    terminal.write("UNCAPTURED");
    CK_CHECK(read_cells(console.output.value, before.dwCursorPosition, 10) == L"UNCAPTURED");
    terminal.restore();
    CK_CHECK(captured == "FIRST");
}

CK_TEST(windows_terminal_reports_decoder_mouse_counts_and_recording_preserves_real_events) {
    FreshConsole console;
    CK_CHECK(console.valid());
    if (!console.valid()) return;
    TestClock clock;
    std::string captured;
    ckv::term::WindowsTerminal terminal(clock, console.output.value, console.input.value);
    clock.now = 250'000'000;
    (void)terminal.poll(clock.now);  // Settle the probe's temporary SGR suppression.
    terminal.set_output_capture([&](std::string_view bytes) { captured.append(bytes); });
    CK_CHECK(terminal.mouse_reports_seen() == 0);
    ckv::term::RecordingTerminal recording(terminal);
    recording.write("RECORDED");
    CK_CHECK(captured == "RECORDED");
    CK_CHECK(inject(console, "\x1B[<0;2;3M"));
    const auto pressed = recording.poll(clock.now);
    CK_CHECK(mouse_events(pressed) == 1);
    CK_CHECK(terminal.mouse_reports_seen() == 1);
    CK_CHECK(recording.poll(clock.now).empty());
    CK_CHECK(terminal.mouse_reports_seen() == 1);
    CK_CHECK(inject(console, "\x1B[<32;3;3M\x1B[<0;3;3m"));
    const auto moved = recording.poll(clock.now);
    CK_CHECK(mouse_events(moved) == 2);
    CK_CHECK(terminal.mouse_reports_seen() == 3);
    ckv::term::ReplayTerminal replay(recording.recording(), recording.initial_capabilities(), recording.initial_size());
    replay.write("RECORDED");
    CK_CHECK(replay.poll(0) == pressed);
    CK_CHECK(replay.poll(0).empty());
    CK_CHECK(replay.poll(0) == moved);
    CK_CHECK(replay.replayed() == recording.recording());
    terminal.restore();
    terminal.set_output_capture({});
}

CK_TEST(windows_terminal_traces_settlement_without_duplicate_summary_on_repeated_polls) {
    FreshConsole console;
    CK_CHECK(console.valid());
    if (!console.valid()) return;
    TestClock clock;
    ckv::BufferedDiagnostics trace;
    ckv::term::WindowsTerminal terminal(clock, console.output.value, console.input.value);
    terminal.set_graphics_trace({&trace, &clock});
    CK_CHECK(trace.entries().empty());
    clock.now = 250'000'000;
    (void)terminal.poll(clock.now);
    CK_CHECK(trace.entries().size() == 1);
    if (!trace.entries().empty()) {
        CK_CHECK(trace.entries().front().level == ckv::LogLevel::Trace);
        CK_CHECK(trace.entries().front().text.starts_with("terminal: sixel=NO cell="));
        CK_CHECK(trace.entries().front().text.find("keyboard=legacy") != std::string::npos);
        CK_CHECK(trace.entries().front().text.ends_with("overrides{sixel=- sync=- cell=-}"));
    }
    (void)terminal.poll(clock.now);
    CK_CHECK(trace.entries().size() == 1);
    terminal.set_graphics_trace({});
    (void)terminal.poll(clock.now);
    CK_CHECK(trace.entries().size() == 1);
}

CK_TEST(windows_terminal_traces_real_conpty_resize_and_disabling_survives_a_second_resize) {
    auto spec = ckv::term::TerminalLaunchSpec::program(CKV_WINDOWS_DIAGNOSTICS_CHILD_PATH, {"outer-diagnostics"});
    spec.exit_policy = ckv::core::TerminalExitPolicy::TerminateAfterGrace;
    spec.profile.cells = {48, 8};
    spec.profile.query_policy = ckv::term::TerminalQueryPolicy::NoResponse;
    spec.working_directory = std::filesystem::path(CKV_WINDOWS_DIAGNOSTICS_CHILD_PATH).parent_path().string();
    auto child = ckv::term::WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(child->state() != ckv::term::TerminalSubsessionState::Failed);
    ckv::term::WindowsClock clock;
    ckv::term::WindowsWaitSet waits(clock);
    const auto pump = [&](std::string_view marker, bool exit) {
        const auto deadline = clock.now_nanos() + 8'000'000'000LL;
        while (clock.now_nanos() < deadline) {
            (void)waits.wait(clock.now_nanos() + 20'000'000LL, child->wait_handles());
            (void)child->drain(16 * 1024);
            if (exit && child->state() == ckv::term::TerminalSubsessionState::Exited) return true;
            std::string screen;
            for (const auto& cell : child->snapshot().cell_buffer)
                if (!cell.is_continuation()) screen += cell.grapheme();
            if (!exit && screen.find(marker) != std::string::npos) return true;
            if (child->state() == ckv::term::TerminalSubsessionState::Failed ||
                child->state() == ckv::term::TerminalSubsessionState::Exited) return false;
        }
        return false;
    };
    const bool first = pump("DIAG-FIRST-READY", false);
    CK_CHECK(first);
    if (!first) { waits.clear(); return; }
    child->resize({57, 10}, {9, 18});
    const bool second = pump("DIAG-SECOND-READY", false);
    CK_CHECK(second);
    if (!second) { waits.clear(); return; }
    child->resize({61, 11}, {9, 18});
    CK_CHECK(pump({}, true));
    CK_CHECK(child->exit_code() == 0);
    std::string final_screen;
    for (const auto& cell : child->snapshot().cell_buffer)
        if (!cell.is_continuation()) final_screen += cell.grapheme();
    CK_CHECK(final_screen.find("DIAGNOSTICS-RESIZE-OK") != std::string::npos);
    waits.clear();
}
