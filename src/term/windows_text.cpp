// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_text.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
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

} // namespace ckv::term
