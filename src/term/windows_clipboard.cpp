// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_clipboard.hpp"

#include <cassert>
#include <limits>

namespace ckv::term {

WindowsClipboardWriter::WindowsClipboardWriter() : thread_(::GetCurrentThreadId()) {}

WindowsClipboardWriter::~WindowsClipboardWriter() {
    assert(thread_ == ::GetCurrentThreadId());
}

void WindowsClipboardWriter::write_text(std::string_view text) {
    assert(thread_ == ::GetCurrentThreadId());
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        text.find('\0') != std::string_view::npos) return;
    const int length = text.empty() ? 0 :
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                              static_cast<int>(text.size()), nullptr, 0);
    if (!text.empty() && length <= 0) return;
    const auto units = static_cast<std::size_t>(length) + 1;
    if (units > std::numeric_limits<std::size_t>::max() / sizeof(wchar_t)) return;
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, units * sizeof(wchar_t));
    if (memory == nullptr) return;
    auto* const buffer = static_cast<wchar_t*>(::GlobalLock(memory));
    if (buffer == nullptr) {
        ::GlobalFree(memory);
        return;
    }
    const bool converted = length == 0 ||
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                              static_cast<int>(text.size()), buffer, length) == length;
    buffer[length] = L'\0';
    ::GlobalUnlock(memory);
    // STATIC is a built-in system class, so no global registration is needed.
    // Immediate CF_UNICODETEXT storage belongs to the system after publication.
    // Destroying the owner here avoids leaving an unpumped window owning the
    // clipboard while a terminal application waits on native I/O handles.
    const HWND owner = converted ?
        ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0,
                          HWND_MESSAGE, nullptr, nullptr, nullptr) : nullptr;
    if (owner != nullptr && ::OpenClipboard(owner)) {
        // Passing nullptr to OpenClipboard would make EmptyClipboard assign
        // a null owner, which Microsoft documents as blocking SetClipboardData.
        // Verify the documented ownership postcondition instead of relying on
        // newer hosts that happen to accept undocumented null-owner writes.
        if (::EmptyClipboard() && ::GetClipboardOwner() == owner &&
            ::SetClipboardData(CF_UNICODETEXT, memory) != nullptr)
            memory = nullptr;  // Ownership transferred to the system.
        ::CloseClipboard();
    }
    if (owner != nullptr) ::DestroyWindow(owner);
    if (memory != nullptr) ::GlobalFree(memory);
}

}  // namespace ckv::term
