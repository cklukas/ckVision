// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <utility>
#if defined(_WIN32)
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_filesystem.hpp"
#include "cvision/term/windows_terminal.hpp"
#include "cvision/term/windows_text.hpp"
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
    auto result = term::windows_utf8(text);
    if (!result) throw std::runtime_error("Invalid Unicode host text");
    return std::move(*result);
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
