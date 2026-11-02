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

// Return a validated extended-length spelling for an absolute DOS, UNC, or
// volume path. This is a boundary conversion only: it does not canonicalize,
// resolve links, collapse dot components, or use the current directory.
std::optional<std::wstring> windows_extended_path(std::wstring_view text);

// Resolve a relative filesystem name against the current directory and return
// its validated extended-length spelling. Device namespace names are refused.
std::optional<std::wstring> windows_absolute_extended_path(std::wstring_view text);

} // namespace ckv::term
