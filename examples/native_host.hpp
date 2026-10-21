// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#if defined(_WIN32)
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_filesystem.hpp"
#include "cvision/term/windows_terminal.hpp"
#else
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_filesystem.hpp"
#include "cvision/term/posix_terminal.hpp"
#include <pwd.h>
#include <unistd.h>
#endif

namespace ckv::examples {
#if defined(_WIN32)
using NativeClock = term::WindowsClock;
using NativeTerminal = term::WindowsTerminal;
using NativeFileSystem = term::WindowsFileSystem;
#else
using NativeClock = term::PosixClock;
using NativeTerminal = term::PosixTerminal;
using NativeFileSystem = term::PosixFileSystem;
#endif
#if defined(_WIN32)
inline std::string utf8(std::wstring_view text) {
    if (text.empty()) return {};
    const auto count = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) throw std::runtime_error("Invalid Unicode host text");
    std::string result(static_cast<std::size_t>(count), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), count, nullptr, nullptr) != count)
        throw std::runtime_error("Cannot convert Unicode host text");
    return result;
}
inline std::vector<std::string> native_arguments(int argc, wchar_t** argv) {
    std::vector<std::string> result;
    for (int i = 0; i < argc; ++i) result.push_back(utf8(argv[i]));
    return result;
}
#else
inline std::vector<std::string> native_arguments(int argc, char** argv) {
    return {argv, argv + argc};
}
#endif
struct UserIdentity { std::string home, name; };
inline UserIdentity current_user() {
    UserIdentity result;
#if defined(_WIN32)
    if (const auto* home = ::_wgetenv(L"USERPROFILE")) result.home = utf8(home);
    if (const auto* user = ::_wgetenv(L"USERNAME")) result.name = utf8(user);
#else
    if (const passwd* record = ::getpwuid(::getuid())) {
        if (record->pw_dir) result.home = record->pw_dir;
        if (record->pw_name) result.name = record->pw_name;
    }
    if (const auto* home = std::getenv("HOME"); result.home.empty() && home) result.home = home;
    if (const auto* user = std::getenv("USER"); result.name.empty() && user) result.name = user;
#endif
    if (result.name.empty()) result.name = "local-user";
    return result;
}
} // namespace ckv::examples
