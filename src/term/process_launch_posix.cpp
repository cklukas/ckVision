// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/process_launch_internal.hpp"
#if !defined(_WIN32)
#include <algorithm>
#include <cerrno>
#include <csignal>
#if defined(__APPLE__)
#include <crt_externs.h>
#else
// Global linkage is essential: a namespace declaration is a different symbol.
extern "C" char** environ;
#endif

namespace ckv::term::detail {

bool prepare_posix_process_launch(const core::ProcessLaunchSpec& spec, PosixLaunchData& data,
                                  ProcessPreparationError& error) {
    if (core::validate_process_launch(spec) != core::ProcessLaunchValidation::Valid || spec.windows_command) {
        error.native = {core::ProcessErrorDomain::Posix, static_cast<std::uint64_t>(spec.windows_command ? ENOTSUP : EINVAL)};
        error.diagnostic = spec.windows_command ? "Windows command-processor launch is unavailable on POSIX"
                                               : "invalid process executable, cwd, arguments or environment";
        return false;
    }
    data.executable = spec.executable;
    data.directory = spec.working_directory;
    data.argument_storage = spec.arguments;
    data.argument_storage.insert(data.argument_storage.begin(), spec.argv0.empty() ? spec.executable : spec.argv0);
    data.argv.clear();
    for (std::string& argument : data.argument_storage) data.argv.push_back(argument.data());
    data.argv.push_back(nullptr);
    data.environment_storage.clear();
    if (spec.environment_policy == core::ProcessEnvironmentPolicy::InheritAndOverride) {
#if defined(__APPLE__)
        char** inherited = *::_NSGetEnviron();
#else
        char** inherited = ::environ;
#endif
        for (char** entry = inherited; entry != nullptr && *entry != nullptr; ++entry)
            data.environment_storage.emplace_back(*entry);
    }
    for (const auto& [name, value] : spec.environment) {
        const auto existing = std::find_if(data.environment_storage.begin(), data.environment_storage.end(),
            [&name](const std::string& entry) {
                return entry.size() > name.size() && entry.compare(0, name.size(), name) == 0 && entry[name.size()] == '=';
            });
        if (existing == data.environment_storage.end()) data.environment_storage.push_back(name + "=" + value);
        else *existing = name + "=" + value;
    }
    data.environment.clear();
    for (std::string& entry : data.environment_storage) data.environment.push_back(entry.data());
    data.environment.push_back(nullptr);
    return true;
}

void reset_process_child_signals() noexcept {
    struct sigaction defaults = {};
    defaults.sa_handler = SIG_DFL;
    sigemptyset(&defaults.sa_mask);
#if defined(NSIG)
    constexpr int signal_count = NSIG;
#elif defined(_NSIG)
    constexpr int signal_count = _NSIG;
#else
    constexpr int signal_count = 65;
#endif
    for (int number = 1; number < signal_count; ++number) (void)sigaction(number, &defaults, nullptr);
    sigset_t unblocked;
    sigemptyset(&unblocked);
    (void)sigprocmask(SIG_SETMASK, &unblocked, nullptr);
}

} // namespace ckv::term::detail
#endif
