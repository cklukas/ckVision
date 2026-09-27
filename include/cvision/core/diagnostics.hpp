// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/clock.hpp"

namespace ckv {

// The severity attached to each diagnostic message, from least to most severe. It labels a
// message ("trace: ", "debug: " and so on when an Application flushes its buffer); no sink in
// the library filters or reorders by level.
enum class LogLevel {
    // Trace and Debug are developer detail; Info a notable ordinary event; Warning a degraded but
    // recovered condition; Error an operation that failed.
    Trace,
    Debug,
    Info,
    Warning,
    Error,
};

// Injectable log sink (the architecture §11): while the UI owns the
// terminal, stderr is unsafe to write to directly, so diagnostics route
// through a sink an Application owns and flushes to stderr only after
// terminal restoration. No global sink — instance-owned (D-008).
class DiagnosticsSink {
public:
    // Destroys the sink; implementations may be deleted through a DiagnosticsSink pointer.
    virtual ~DiagnosticsSink() = default;

    // Records one message at `level`. The view is only valid for the call, so a sink that keeps
    // the text must copy it. Must not throw and must not write to the terminal: it can be called
    // from any point in a frame, while the UI owns the screen.
    virtual void log(LogLevel level, std::string_view message) noexcept = 0;
};

// One buffered diagnostic: its severity and an owned copy of the message text, as passed to log.
struct DiagnosticsEntry {
    // The level and message exactly as logged; the text carries no level prefix or newline.
    LogLevel level;
    std::string text;
};

// An in-memory sink for tests and headless use. Writing buffered entries out
// after terminal restoration is intentionally a term/ui-layer responsibility;
// core performs no I/O at all (§1), so this type only ever buffers.
class BufferedDiagnostics final : public DiagnosticsSink {
public:
    void log(LogLevel level, std::string_view message) noexcept override {
        // Diagnostics must never turn a recoverable observation into an
        // allocation-triggered termination. The best-effort loss on memory
        // exhaustion is explicit; the original operation remains intact.
        try {
            entries_.push_back(DiagnosticsEntry{level, std::string(message)});
        } catch (...) {
        }
    }

    // Every entry logged since construction or the last clear(), oldest first. The buffer is
    // unbounded; an entry that cannot be stored for lack of memory is silently dropped. clear()
    // empties it.
    const std::vector<DiagnosticsEntry>& entries() const noexcept { return entries_; }
    void clear() noexcept { entries_.clear(); }

private:
    std::vector<DiagnosticsEntry> entries_;
};

// Where the graphics path reports what it did (D-077): whether a child asked about graphics and
// what it was told, whether its picture decoded, how long an encode or a frame took, and what
// erased a picture. Both parts are borrowed from the host, which must keep them alive while any
// Presenter or session holds this; the sink receives LogLevel::Trace lines, and the clock times
// them. A default-constructed trace, or one missing either part, traces nothing, and the
// graphics path then builds no message and reads no clock.
struct GraphicsTrace {
    // The borrowed sink the lines go to, and the borrowed clock that times them.
    DiagnosticsSink* sink = nullptr;
    const Clock* clock = nullptr;

    // Whether tracing is on: both a sink to write to and a clock to time with.
    explicit operator bool() const noexcept { return sink != nullptr && clock != nullptr; }

    // Writes one trace line; nothing when tracing is off.
    void line(std::string_view message) const noexcept {
        if (*this) sink->log(LogLevel::Trace, message);
    }

    // The trace clock's reading in nanoseconds, or 0 when tracing is off.
    std::int64_t now_nanos() const noexcept { return *this ? clock->now_nanos() : 0; }
};

}  // namespace ckv
