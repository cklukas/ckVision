// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ckv::term {

// Quotes one non-argv[0] value under Microsoft's CRT parsing rules (D-129).
// Pure, locale independent; preserves empty values and opaque wide code units.
// Refuses embedded NULs or an encoded value longer than 32766 code units.
// This is not cmd or PowerShell quoting and must not encode shell command text.
inline std::optional<std::wstring> quote_windows_argument(std::wstring_view argument) {
    if (argument.find(L'\0') != std::wstring_view::npos || argument.size() > 32766) return std::nullopt;
    std::wstring quoted(1, L'"');
    std::size_t slashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') { ++slashes; continue; }
        quoted.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        quoted.push_back(character);
    }
    quoted.append(slashes * 2, L'\\');
    quoted.push_back(L'"');
    if (quoted.size() > 32766) return std::nullopt;
    return quoted;
}

// Encodes argv[0] plus arguments for CreateProcessW, not a command interpreter.
// argv[0] follows the CRT's special filename grammar: nonempty, no quotes/NUL,
// backslashes unchanged. Other values use quote_windows_argument(). Refuses
// invalid values or a total length exceeding 32766 (32767 including the NUL).
// The returned owned string is mutable, as CreateProcessW requires.
inline std::optional<std::wstring> windows_argv_command_line(std::span<const std::wstring_view> arguments) {
    if (arguments.empty() || arguments.front().empty() || arguments.front().size() > 32764 ||
        arguments.front().find(L'\0') != std::wstring_view::npos ||
        arguments.front().find(L'"') != std::wstring_view::npos) return std::nullopt;
    std::wstring line(1, L'"');
    line.append(arguments.front());
    line.push_back(L'"');
    for (std::size_t i = 1; i < arguments.size(); ++i) {
        auto quoted = quote_windows_argument(arguments[i]);
        if (!quoted || line.size() >= 32766 || quoted->size() > 32766 - line.size() - 1) return std::nullopt;
        line.push_back(L' ');
        line.append(*quoted);
    }
    return line;
}

} // namespace ckv::term
