// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The whole of what this program knows, as a document.
//
// Composed from the same functions the panes are composed from, so the
// file a reader saves and the screen they saved it from cannot disagree.
// A report that recomputed its own figures would be a second program, and
// the first bug in it would be invisible.
//
// Pure: no clock, no filesystem, no environment. Writing it out is the
// application's job, through the injected FileSystem.
#pragma once

#include <string>
#include <vector>

#include "benchmark.hpp"
#include "system_probe.hpp"

namespace ckv::sysinfo {

enum class ReportFormat {
    Text,
    Markdown,
};

// `terminal_report` is what the Terminal pane shows; it comes from the
// application because only the application has a terminal.
std::string compose_report(const SystemProbe& probe, const std::vector<BenchmarkResult>& results,
                           const std::string& terminal_report, ReportFormat format);

// The default name offered in the save dialog for each format.
std::string default_report_name(ReportFormat format);

}  // namespace ckv::sysinfo
