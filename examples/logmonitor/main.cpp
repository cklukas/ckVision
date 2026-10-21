// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "logmonitor_app.hpp"
#include "native_sources.hpp"
#include "../example_diagnostics.hpp"
#include "cvision/term/log_file_service.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include <limits>
#if defined(_WIN32)
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal.hpp"
#include "cvision/term/windows_filesystem.hpp"
#else
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/posix_filesystem.hpp"
#endif
int main() {
    return ckv::examples::run_reporting_failure([] {
        ckv::examples::NativeClock clock;
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        ckv::examples::NativeTerminal terminal(clock);
        ckv::examples::NativeFileSystem filesystem;
        auto wake = std::make_shared<ckv::term::TerminalLogWake>(terminal);
        auto activity = std::make_shared<ckv::LogActivity>(wake);
        ckv::term::NativeLogFileService files(8, ckv::kLogBatchBytes, ckv::term::LogWatchPreference::Native, activity);
        ckv::logmonitor::NativeExportWriter exports(filesystem, activity);
        ckv::logmonitor::NativeLogSources native(clock, activity);
        ckv::term::TerminalClipboardWriter clipboard(terminal);
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(terminal);
        diagnostics.attach(app);
        ckv::logmonitor::LogMonitorApp monitor(app, files, filesystem, exports, activity, native.catalog());
        while (!app.quit_requested()) {
            monitor.step(clock.now_nanos());
            app.step(monitor.next_deadline().value_or(std::numeric_limits<std::int64_t>::max()));
        }
    });
}
