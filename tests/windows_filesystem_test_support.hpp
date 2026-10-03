// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ckv::testing {

inline std::string filesystem_utf8(std::wstring_view text) {
    const int length = static_cast<int>(text.size());
    const int count = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length, nullptr, 0, nullptr, nullptr);
    if (!count) throw std::runtime_error("invalid native fixture name");
    std::string result(static_cast<std::size_t>(count), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length, result.data(), count, nullptr, nullptr) != count)
        throw std::runtime_error("fixture name conversion failed");
    return result;
}

inline std::wstring filesystem_wide(std::string_view text) {
    const int length = static_cast<int>(text.size());
    const int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0);
    if (!count) throw std::runtime_error("invalid UTF-8 fixture name");
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, result.data(), count) != count)
        throw std::runtime_error("fixture name conversion failed");
    return result;
}

class FileTestHandle final {
public:
    explicit FileTestHandle(HANDLE value = INVALID_HANDLE_VALUE) : value_(value) {}
    ~FileTestHandle() { close(); }
    FileTestHandle(const FileTestHandle&) = delete;
    FileTestHandle& operator=(const FileTestHandle&) = delete;
    bool valid() const noexcept { return value_ && value_ != INVALID_HANDLE_VALUE; }
    HANDLE get() const noexcept { return value_; }
    void close() noexcept { if (valid()) ::CloseHandle(value_); value_ = INVALID_HANDLE_VALUE; }
private:
    HANDLE value_;
};

}  // namespace ckv::testing
