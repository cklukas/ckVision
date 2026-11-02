// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_text.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <limits>

namespace ckv::term {

std::optional<std::wstring> windows_utf16(std::string_view text) {
    if (text.empty()) return std::wstring{};
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return std::nullopt;
    const int size = static_cast<int>(text.size());
    const int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
    if (count == 0) return std::nullopt;
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, result.data(), count) != count)
        return std::nullopt;
    return result;
}

std::optional<std::string> windows_utf8(std::wstring_view text) {
    if (text.empty()) return std::string{};
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return std::nullopt;
    const int size = static_cast<int>(text.size());
    const int count = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), size, nullptr, 0, nullptr, nullptr);
    if (count == 0) return std::nullopt;
    std::string result(static_cast<std::size_t>(count), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), size, result.data(), count, nullptr, nullptr) != count)
        return std::nullopt;
    return result;
}

std::optional<std::wstring> windows_extended_path(std::wstring_view text) {
    if (text.empty() || text.find(L'\0') != text.npos || text.size() > 32767) return std::nullopt;
    std::wstring normalized(text);
    std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
    if (normalized.starts_with(L"\\\\?\\")) {
        const bool volume = normalized.size() >= 45 && normalized.starts_with(L"\\\\?\\Volume{") &&
            normalized[43] == L'}' && normalized[44] == L'\\';
        if (normalized.starts_with(L"\\\\?\\UNC\\") || volume ||
            (normalized.size() >= 7 && ((normalized[4] >= L'A' && normalized[4] <= L'Z') ||
                (normalized[4] >= L'a' && normalized[4] <= L'z')) && normalized[5] == L':' && normalized[6] == L'\\'))
            return normalized;
        return std::nullopt;
    }
    if (normalized.starts_with(L"\\\\.\\") || normalized.starts_with(L"\\??\\")) return std::nullopt;
    const bool drive = normalized.size() >= 3 &&
        ((normalized[0] >= L'A' && normalized[0] <= L'Z') || (normalized[0] >= L'a' && normalized[0] <= L'z')) &&
        normalized[1] == L':' && normalized[2] == L'\\';
    if (drive) return L"\\\\?\\" + normalized;
    if (normalized.size() < 5 || normalized[0] != L'\\' || normalized[1] != L'\\' || normalized[2] == L'\\')
        return std::nullopt;
    const auto server_end = normalized.find(L'\\', 2);
    if (server_end == std::wstring::npos || server_end == 2) return std::nullopt;
    const auto share_end = normalized.find(L'\\', server_end + 1);
    if (share_end == server_end + 1) return std::nullopt;
    return L"\\\\?\\UNC" + normalized.substr(1);
}

std::optional<std::wstring> windows_absolute_extended_path(std::wstring_view text) {
    if (text.empty() || text.find(L'\0') != text.npos) return std::nullopt;
    std::wstring input(text);
    std::replace(input.begin(), input.end(), L'/', L'\\');
    if (input.starts_with(L"\\\\.\\") || input.starts_with(L"\\??\\")) return std::nullopt;
    const DWORD count = ::GetFullPathNameW(input.c_str(), 0, nullptr, nullptr);
    if (count == 0 || count > 32767) return std::nullopt;
    std::wstring absolute(static_cast<std::size_t>(count), L'\0');
    const DWORD written = ::GetFullPathNameW(input.c_str(), count, absolute.data(), nullptr);
    if (written == 0 || written >= count) return std::nullopt;
    absolute.resize(written);
    return windows_extended_path(absolute);
}

} // namespace ckv::term
