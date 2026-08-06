// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ckv {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
};

// Injectable log sink (ARCHITECTURE.md §11): while the UI owns the
// terminal, stderr is unsafe to write to directly, so diagnostics route
// through a sink an Application owns and flushes to stderr only after
// terminal restoration. No global sink — instance-owned (D-008).
class DiagnosticsSink {
public:
    virtual ~DiagnosticsSink() = default;

    virtual void log(LogLevel level, std::string_view message) noexcept = 0;
};

struct DiagnosticsEntry {
    LogLevel level;
    std::string text;
};

// An in-memory sink for tests and headless use. Writing buffered
// entries out (e.g. to stderr, only after terminal restoration) is a
// term/ui-layer responsibility that does not exist yet (ARCHITECTURE.md
// §11) — core performs no I/O at all (§1), so this type only ever
// buffers; it never writes anywhere itself.
class BufferedDiagnostics final : public DiagnosticsSink {
public:
    void log(LogLevel level, std::string_view message) noexcept override {
        entries_.push_back(DiagnosticsEntry{level, std::string(message)});
    }

    const std::vector<DiagnosticsEntry>& entries() const noexcept { return entries_; }
    void clear() noexcept { entries_.clear(); }

private:
    std::vector<DiagnosticsEntry> entries_;
};

}  // namespace ckv
