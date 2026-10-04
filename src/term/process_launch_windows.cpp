// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/process_launch_internal.hpp"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cwchar>
#include "cvision/term/windows_argv.hpp"
#include "cvision/term/windows_text.hpp"

namespace ckv::term::detail {
namespace {
std::wstring widen(std::string_view value) {
    return windows_utf16(value).value_or(std::wstring{});
}
bool same_name(const std::wstring& left, const std::wstring& right) noexcept {
    return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
}
struct EnvironmentEntry { std::wstring name; std::wstring value; };
bool make_environment(const core::ProcessLaunchSpec& spec, std::vector<wchar_t>& block,
                      ProcessPreparationError& error) {
    std::vector<EnvironmentEntry> entries;
    if (spec.environment_policy == core::ProcessEnvironmentPolicy::InheritAndOverride) {
        wchar_t* const inherited = ::GetEnvironmentStringsW();
        if (inherited == nullptr) {
            error.native = {core::ProcessErrorDomain::Win32, ::GetLastError()};
            error.diagnostic = "unable to read inherited process environment";
            return false;
        }
        for (const wchar_t* entry = inherited; *entry != L'\0'; entry += std::wcslen(entry) + 1) {
            const wchar_t* const separator = std::wcschr(entry + (entry[0] == L'=' ? 1 : 0), L'=');
            if (separator) entries.push_back({std::wstring(entry, separator), std::wstring(separator + 1)});
        }
        (void)::FreeEnvironmentStringsW(inherited);
    }
    std::vector<std::wstring> names;
    for (const auto& [utf8_name, utf8_value] : spec.environment) {
        const std::wstring name = widen(utf8_name);
        const std::wstring value = widen(utf8_value);
        if (name.empty() || (!utf8_value.empty() && value.empty()) ||
            std::any_of(names.begin(), names.end(), [&name](const std::wstring& other) { return same_name(name, other); })) {
            error.native = {core::ProcessErrorDomain::Win32, ERROR_INVALID_PARAMETER};
            error.diagnostic = "process environment contains invalid UTF-8 or a duplicate name";
            return false;
        }
        names.push_back(name);
        const auto existing = std::find_if(entries.begin(), entries.end(),
            [&name](const EnvironmentEntry& entry) { return same_name(entry.name, name); });
        if (existing == entries.end()) entries.push_back({name, value});
        else existing->value = value;
    }
    std::sort(entries.begin(), entries.end(), [](const EnvironmentEntry& left, const EnvironmentEntry& right) {
        return ::CompareStringOrdinal(left.name.c_str(), -1, right.name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    });
    block.clear();
    for (const auto& entry : entries) {
        block.insert(block.end(), entry.name.begin(), entry.name.end());
        block.push_back(L'=');
        block.insert(block.end(), entry.value.begin(), entry.value.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    if (entries.empty()) block.push_back(L'\0');
    return true;
}
} // namespace

bool prepare_windows_process_launch(const core::ProcessLaunchSpec& spec, WindowsLaunchData& data,
                                    ProcessPreparationError& error) {
    const auto invalid = [&error](const char* diagnostic) {
        error.native = {core::ProcessErrorDomain::Win32, ERROR_INVALID_PARAMETER};
        error.diagnostic = diagnostic;
        return false;
    };
    const auto validation = core::validate_process_launch(spec);
    if (validation == core::ProcessLaunchValidation::ConflictingCommandForms)
        return invalid("cmd command text cannot be combined with arguments or argv[0]");
    if (validation != core::ProcessLaunchValidation::Valid)
        return invalid("invalid process executable, cwd, arguments or environment");
    data.executable = widen(spec.executable);
    data.directory = widen(spec.working_directory);
    const std::wstring argv0 = spec.argv0.empty() ? data.executable : widen(spec.argv0);
    if (data.executable.empty() || data.directory.empty() || argv0.empty())
        return invalid("process launch contains invalid UTF-8 in a path or argv[0]");
    if (spec.windows_command) {
        std::wstring image = data.executable;
        std::replace(image.begin(), image.end(), L'/', L'\\');
        const std::array<std::wstring_view, 1> arguments{image};
        auto encoded = windows_argv_command_line(arguments);
        if (!encoded) return invalid("cmd image token cannot be encoded");
        data.command = std::move(*encoded) + L" /d";
        if (spec.windows_command->command) {
            const auto& source = *spec.windows_command->command;
            const std::wstring text = widen(source);
            if (!source.empty() && text.empty()) return invalid("cmd command text contains invalid UTF-8");
            data.command += L" /s /c \"";
            data.command += text;
            data.command += L'"';
        }
    } else {
        std::vector<std::wstring> arguments{argv0};
        for (const auto& argument : spec.arguments) {
            const auto wide = widen(argument);
            if (!argument.empty() && wide.empty()) return invalid("process argument contains invalid UTF-8");
            arguments.push_back(wide);
        }
        std::vector<std::wstring_view> views;
        for (const auto& argument : arguments) views.push_back(argument);
        auto encoded = windows_argv_command_line(views);
        if (!encoded) return invalid("process argv cannot be encoded within the Windows limit");
        data.command = std::move(*encoded);
    }
    if (data.command.size() >= 32767) return invalid("process command line exceeds the Windows limit");
    return make_environment(spec, data.environment, error);
}
} // namespace ckv::term::detail
#endif
