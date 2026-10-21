// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "sysmon_app.hpp"
#include "native_monitor.hpp"
#include "../example_diagnostics.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/core/version.hpp"
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string_view>
#if defined(_WIN32)
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_filesystem.hpp"
#include "cvision/term/windows_terminal.hpp"
#else
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_filesystem.hpp"
#include "cvision/term/posix_terminal.hpp"
#endif
namespace {
std::string settings_path() {
#if defined(_WIN32)
    std::filesystem::path root;
    if (const auto* appdata = ::_wgetenv(L"APPDATA"); appdata && *appdata) root = appdata;
    else if (const auto* home = ::_wgetenv(L"USERPROFILE"); home && *home) root = std::filesystem::path(home) / L"AppData/Roaming";
    if (root.empty()) return {};
    const auto utf8 = (root / L"ckvision/ck-sysmon.settings").u8string();
    return {utf8.begin(), utf8.end()};
#else
    const auto* home = std::getenv("HOME");
#if defined(__APPLE__)
    return home && *home ? std::string(home) + "/Library/Application Support/ckvision/ck-sysmon.settings" : std::string{};
#else
    if (const auto* config = std::getenv("XDG_CONFIG_HOME"); config && *config == '/')
        return std::string(config) + "/ckvision/ck-sysmon.settings";
    return home && *home ? std::string(home) + "/.config/ckvision/ck-sysmon.settings" : std::string{};
#endif
#endif
}
std::tm local_time() {
    const auto now = std::time(nullptr); std::tm local{};
#if defined(_WIN32)
    ::localtime_s(&local, &now);
#else
    ::localtime_r(&now, &local);
#endif
    return local;
}
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ck-sysmon " << ckv::version_string() << '\n'; return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: ck-sysmon [--version | --help | --sample | --sample-host alias]\n"; return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--sample") {
        std::cout << ckv::sysmon::sample_text(ckv::sysmon::native_sample()); return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--sample-host") {
#if defined(_WIN32)
        ckv::term::WindowsClock clock;
#else
        ckv::term::PosixClock clock;
#endif
        ckv::sysmon::NativeMonitorSource source(clock);
        const auto sample = source.sample(argv[2]);
        if (!sample.diagnostic.empty()) std::cerr << sample.diagnostic << '\n';
        if (!sample.reachable) return 1;
        std::cout << ckv::sysmon::sample_text(sample); return 0;
    }
    if (argc != 1) { std::cerr << "Usage: ck-sysmon [--version | --help | --sample | --sample-host alias]\n"; return 2; }
    return ckv::examples::run_reporting_failure([] {
        ckv::examples::NativeClock clock;
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        ckv::examples::NativeTerminal terminal(clock);
        ckv::examples::NativeFileSystem files;
        ckv::sysmon::SettingsStore settings(files, settings_path());
        auto loaded = settings.load();
        ckv::term::TerminalClipboardWriter clipboard(terminal);
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(terminal);
        diagnostics.attach(app);
        ckv::sysmon::NativeMonitorSource source(clock);
        ckv::sysmon::SysmonApp monitor(app, source, {
            [] { const auto now = local_time(); return ckv::widgets::TimeValue{now.tm_hour, now.tm_min, now.tm_sec}; },
            [] { const auto now = local_time(); return ckv::widgets::DateValue{now.tm_year + 1900, now.tm_mon + 1, now.tm_mday}; }},
            {std::move(loaded.value), [&settings](const ckv::sysmon::Settings& value) { return settings.save(value); }, std::move(loaded.diagnostic)});
        app.run();
    });
}
