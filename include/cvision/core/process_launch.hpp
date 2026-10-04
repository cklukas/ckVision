// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ckv::core {

// Inherit the caller's environment and override entries, or start entirely empty.
enum class ProcessEnvironmentPolicy { InheritAndOverride, ExplicitOnly };

// Explicit cmd syntax, never an inference from an executable's filename.
// Absent command is interactive; present (including empty) executes and exits.
struct WindowsCommandProcessorLaunch {
    // Absent is interactive; present (even empty) executes trusted syntax and exits.
    std::optional<std::string> command;
};

// Shared by terminal and non-terminal children. No PATH search, shell guess,
// inherited working directory, or conversion of stdin into command arguments.
struct ProcessLaunchSpec {
    // Image path, used without a PATH search; empty or embedded NUL is invalid.
    std::string executable;
    // Ordinary argument data excluding argv[0]; empty values are meaningful.
    std::vector<std::string> arguments;
    // Explicit Windows command form, mutually exclusive with arguments/argv0.
    std::optional<WindowsCommandProcessorLaunch> windows_command;
    // Empty uses executable. A login shell may explicitly use e.g. "-zsh".
    std::string argv0;
    // Explicit native cwd. It must exist; empty or embedded NUL is invalid.
    std::string working_directory = "/";
    // Name/value overrides: names are nonempty, contain no '=', and are unique.
    std::vector<std::pair<std::string, std::string>> environment;
    // Where the child environment starts before applying the overrides.
    ProcessEnvironmentPolicy environment_policy = ProcessEnvironmentPolicy::InheritAndOverride;

    // Builds an ordinary invocation; remaining fields retain their defaults.
    static ProcessLaunchSpec program(std::string executable, std::vector<std::string> arguments = {}) {
        ProcessLaunchSpec spec;
        spec.executable = std::move(executable);
        spec.arguments = std::move(arguments);
        return spec;
    }
    // Builds an explicitly selected cmd invocation; no executable-name heuristic.
    static ProcessLaunchSpec windows_command_processor(
        std::string executable, std::optional<std::string> command = std::nullopt) {
        ProcessLaunchSpec spec;
        spec.executable = std::move(executable);
        spec.windows_command = WindowsCommandProcessorLaunch{std::move(command)};
        return spec;
    }
};

// Pure structural validation; filesystem/native encoding checks belong in term.
enum class ProcessLaunchValidation {
    Valid, InvalidExecutable, InvalidWorkingDirectory, InvalidArgument,
    InvalidEnvironment, ConflictingCommandForms, InvalidCommandText,
};

// Pure common validation. POSIX byte paths/arguments need not be Unicode;
// Windows additionally validates UTF-8, case-insensitive environment names,
// native command-line lengths and unsupported command forms at its boundary.
inline ProcessLaunchValidation validate_process_launch(const ProcessLaunchSpec& spec) noexcept {
    const auto has_nul = [](const std::string& text) { return text.find('\0') != std::string::npos; };
    if (spec.executable.empty() || has_nul(spec.executable)) return ProcessLaunchValidation::InvalidExecutable;
    if (spec.working_directory.empty() || has_nul(spec.working_directory))
        return ProcessLaunchValidation::InvalidWorkingDirectory;
    if (has_nul(spec.argv0) || std::any_of(spec.arguments.begin(), spec.arguments.end(), has_nul))
        return ProcessLaunchValidation::InvalidArgument;
    if (spec.windows_command && (!spec.arguments.empty() || !spec.argv0.empty()))
        return ProcessLaunchValidation::ConflictingCommandForms;
    if (spec.windows_command && spec.windows_command->command && has_nul(*spec.windows_command->command))
        return ProcessLaunchValidation::InvalidCommandText;
    if (spec.environment_policy != ProcessEnvironmentPolicy::InheritAndOverride &&
        spec.environment_policy != ProcessEnvironmentPolicy::ExplicitOnly)
        return ProcessLaunchValidation::InvalidEnvironment;
    for (std::size_t i = 0; i < spec.environment.size(); ++i) {
        const auto& [name, value] = spec.environment[i];
        if (name.empty() || name.find('=') != std::string::npos || has_nul(name) || has_nul(value))
            return ProcessLaunchValidation::InvalidEnvironment;
        for (std::size_t j = 0; j < i; ++j)
            if (spec.environment[j].first == name) return ProcessLaunchValidation::InvalidEnvironment;
    }
    return ProcessLaunchValidation::Valid;
}

} // namespace ckv::core
