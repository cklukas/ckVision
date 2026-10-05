// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <string_view>

namespace ckv::term {

// Native preflight states, not a promise that the Windows loader will succeed.
enum class WindowsProcessImageState { InvalidPath, Unavailable, InvalidImage, UnsupportedArchitecture, Compatible };

// Read-only PE header and host user-mode architecture inspection.
struct WindowsProcessImageInfo {
    // Compatible requires executable (not DLL) headers and supported user-mode code.
    WindowsProcessImageState state = WindowsProcessImageState::InvalidPath;
    // Declared PE/COFF machine, when valid headers were read; zero otherwise.
    std::uint16_t machine = 0;
    // Win32 I/O error or architecture-query HRESULT; zero for a policy refusal.
    std::uint32_t native_error = 0;
    // Header compatibility only: no signature, dependency, readiness or TOCTOU guarantee.
    bool compatible() const noexcept { return state == WindowsProcessImageState::Compatible; }
};

// Windows-only, absolute UTF-8 path, no PATH search and no process execution.
// Reads bounded PE headers, rejects DLLs/non-Windows subsystems, and queries
// host architecture support rather than assuming the caller's own architecture.
// CreateProcess remains authoritative; the file can change after this inspection.
WindowsProcessImageInfo inspect_windows_process_image(std::string_view path);

} // namespace ckv::term
