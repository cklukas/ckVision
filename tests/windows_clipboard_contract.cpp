// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_clipboard.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/testing/cktest.hpp"

#include <cstdio>
#include <future>
#include <memory>
#include <string>
#include <thread>

namespace {

std::wstring clipboard_text() {
    CK_CHECK(::OpenClipboard(nullptr));
    const HANDLE memory = ::GetClipboardData(CF_UNICODETEXT);
    CK_CHECK(memory != nullptr);
    const auto* text = memory == nullptr ? nullptr : static_cast<const wchar_t*>(::GlobalLock(memory));
    CK_CHECK(text != nullptr);
    const std::wstring result = text == nullptr ? L"" : text;
    if (text != nullptr) ::GlobalUnlock(memory);
    CK_CHECK(::CloseClipboard());
    return result;
}

void chord(ckv::ui::Application& app, std::string text) {
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, std::move(text)}});
}

}  // namespace

CK_TEST(windows_clipboard_unicode_copy_traverses_the_application_and_focused_widget) {
    ckv::term::WindowsClipboardWriter clipboard;
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock, clipboard);
    auto* input = app.root().add(std::make_unique<ckv::widgets::InputLine>());
    const std::string text = "copy \xCE\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80";
    input->set_text(text);
    app.set_focus(input);
    CK_CHECK(app.focused() == input);
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::End, ckv::Modifier::None, ""}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Home, ckv::Modifier::Shift, ""}});
    CK_CHECK(input->has_selection());
    CK_CHECK(input->selection_range().first < input->selection_range().second);
    chord(app, "c");
    CK_CHECK(app.clipboard_text() == text);
    CK_CHECK(clipboard_text() == L"copy \u03A9\u4E2D\U0001F600");
    CK_CHECK(::GetClipboardOwner() == nullptr);  // Publication owner already destroyed.
    CK_CHECK(terminal.clipboard().empty());
}

CK_TEST(windows_clipboard_invalid_utf8_and_embedded_nul_do_not_destroy_existing_content) {
    ckv::term::WindowsClipboardWriter clipboard;
    clipboard.write_text("preserved");
    clipboard.write_text("\xF0\x28\x8C\x28");
    CK_CHECK(clipboard_text() == L"preserved");
    clipboard.write_text(std::string_view("left\0right", 10));
    CK_CHECK(clipboard_text() == L"preserved");
    clipboard.write_text("");
    CK_CHECK(clipboard_text().empty());
    clipboard.write_text("after empty\nsecond line");
    CK_CHECK(clipboard_text() == L"after empty\nsecond line");
}

CK_TEST(windows_clipboard_publication_owners_are_released_and_exports_outlive_the_adapter) {
    ckv::term::WindowsClipboardWriter first;
    first.write_text("first");
    CK_CHECK(clipboard_text() == L"first");
    CK_CHECK(::GetClipboardOwner() == nullptr);
    {
        ckv::term::WindowsClipboardWriter second;
        second.write_text("second");
        CK_CHECK(::GetClipboardOwner() == nullptr);
        CK_CHECK(clipboard_text() == L"second");
    }
    CK_CHECK(clipboard_text() == L"second");
    first.write_text("first again");
    CK_CHECK(::GetClipboardOwner() == nullptr);
    CK_CHECK(clipboard_text() == L"first again");
}

CK_TEST(windows_clipboard_contention_preserves_internal_copy_and_recovers_after_unlock) {
    ckv::term::WindowsClipboardWriter clipboard;
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock, clipboard);
    clipboard.write_text("prior external value");
    std::promise<bool> acquired;
    auto acquired_result = acquired.get_future();
    std::promise<void> release;
    auto release_request = release.get_future();
    std::thread blocker([&] {
        const bool opened = ::OpenClipboard(nullptr) != FALSE;
        acquired.set_value(opened);
        release_request.wait();
        if (opened) ::CloseClipboard();
    });
    const bool locked = acquired_result.get();
    CK_CHECK(locked);
    app.set_clipboard_text("internal while locked");
    CK_CHECK(app.clipboard_text() == "internal while locked");
    release.set_value();
    blocker.join();
    CK_CHECK(clipboard_text() == L"prior external value");
    app.set_clipboard_text("external after unlock");
    CK_CHECK(app.clipboard_text() == "external after unlock");
    CK_CHECK(clipboard_text() == L"external after unlock");
}

int main(int argc, char** argv) {
    // A window station owns its own clipboard. Never run these destructive
    // clipboard tests in the user's interactive station, even on setup failure.
    const HWINSTA original_station = ::GetProcessWindowStation();
    const HDESK original_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
    // An unnamed station uses the logon-session name and may already exist
    // (notably under OpenSSH). CREATE_ONLY plus a process-specific name must
    // create a fresh station; this gate needs station-creation permissions.
    const std::wstring station_name = L"ckvision-clipboard-" + std::to_wstring(::GetCurrentProcessId());
    const HWINSTA station = ::CreateWindowStationW(station_name.c_str(), CWF_CREATE_ONLY,
                                                  GENERIC_ALL, nullptr);
    if (station == nullptr || station == original_station || !::SetProcessWindowStation(station)) {
        std::fprintf(stderr, "Cannot isolate the clipboard test window station (Win32 %lu)\n",
                     ::GetLastError());
        if (station != nullptr) ::CloseWindowStation(station);
        return 1;
    }
    const HDESK desktop = ::CreateDesktopW(L"Default", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (desktop == nullptr || !::SetThreadDesktop(desktop)) {
        const DWORD error = ::GetLastError();
        ::SetProcessWindowStation(original_station);
        if (desktop != nullptr) ::CloseDesktop(desktop);
        ::CloseWindowStation(station);
        std::fprintf(stderr, "Cannot isolate the clipboard test desktop (Win32 %lu)\n", error);
        return 1;
    }
    const int result = ::cktest::run_all(argc, argv);
    // All instance-owned windows and the blocker thread have gone away.
    const BOOL restored_station = ::SetProcessWindowStation(original_station);
    const BOOL restored_desktop = ::SetThreadDesktop(original_desktop);
    const BOOL closed_desktop = ::CloseDesktop(desktop);
    const BOOL closed_station = ::CloseWindowStation(station);
    return result != 0 || !restored_desktop || !restored_station ||
           !closed_desktop || !closed_station ? 1 : 0;
}
