// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include "cvision/core/process_runner.hpp"

namespace ckv::term::detail {

// Internal launch-preparation failure; no process has been started yet.
struct ProcessPreparationError {
    // Retains the platform error domain/code when preparation cannot succeed.
    core::ProcessNativeError native;
    // Human-readable stage/validation explanation, not host-terminal output.
    std::string diagnostic;
};

#if !defined(_WIN32)
// Own storage until exec; pointer arrays cannot survive copying this object.
struct PosixLaunchData {
    // Starts with empty owned storage; prepared once before the child fork.
    PosixLaunchData() = default;
    // Pointer arrays refer to owned strings; copies would invalidate ownership.
    PosixLaunchData(const PosixLaunchData&) = delete;
    // Assignment is refused for the same pointer-lifetime reason.
    PosixLaunchData& operator=(const PosixLaunchData&) = delete;
    // Exact executable byte path, kept alive across fork/exec preparation.
    std::string executable;
    // Exact working-directory byte path.
    std::string directory;
    // Owns argv[0] and every argument, including empty/opaque byte values.
    std::vector<std::string> argument_storage;
    // Owns the merged or explicit environment entries.
    std::vector<std::string> environment_storage;
    // Native pointer view of argument_storage, ending in nullptr.
    std::vector<char*> argv;
    // Native pointer view of environment_storage, ending in nullptr.
    std::vector<char*> environment;
};
// Common validation plus native POSIX argv/environment ownership construction.
bool prepare_posix_process_launch(const core::ProcessLaunchSpec& spec, PosixLaunchData& data,
                                  ProcessPreparationError& error);
// Only async-signal-safe operations; called in the child between fork/exec.
void reset_process_child_signals() noexcept;
#else
// Internal owned buffers for CreateProcessW; applications use ProcessLaunchSpec.
struct WindowsLaunchData {
    // Validated UTF-16 application name, not inferred from command text.
    std::wstring executable;
    // Validated UTF-16 working-directory path.
    std::wstring directory;
    // Mutable native command line using explicit cmd or ordinary CRT grammar.
    std::wstring command;
    // Sorted UTF-16 environment block, terminated by two NUL code units.
    std::vector<wchar_t> environment;
};
// Common validation plus native encoding, case-aware environment and argv rules.
bool prepare_windows_process_launch(const core::ProcessLaunchSpec& spec, WindowsLaunchData& data,
                                    ProcessPreparationError& error);
#endif

} // namespace ckv::term::detail
