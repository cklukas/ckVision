// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace ckv::term {

// Native Windows UTF-8/UTF-16 boundaries, independent of locale/ANSI code page.
// Empty text succeeds; embedded NULs are preserved, not filename validation.
// Malformed Unicode, excessive native lengths or OS conversion failure return
// no value. These native adapter functions are implemented on Windows only.
std::optional<std::wstring> windows_utf16(std::string_view text);
std::optional<std::string> windows_utf8(std::wstring_view text);

} // namespace ckv::term
