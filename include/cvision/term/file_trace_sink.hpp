// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A DiagnosticsSink that writes each line to a file or standard error as it arrives: the
// platform adapter behind a live graphics trace (D-077). Nothing in the library creates one;
// a host that wants a trace constructs it and hands it over through
// ui::Application::set_graphics_trace() and the backend's own set_graphics_trace().
#pragma once

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "cvision/core/clock.hpp"
#include "cvision/core/diagnostics.hpp"

namespace ckv::term {

// Writes trace lines to a file it owns or to standard error, one flushed line at a time.
class FileTraceSink final : public DiagnosticsSink {
public:
    // How opening a file treats what is already there. A host and the contained child it
    // launches share one trace file to be read together, so the first process starts it
    // empty and the others append.
    enum class OpenMode { Truncate, Append };

    // Opens `path` for tracing, or returns null when it cannot be opened. Windows paths are
    // strict UTF-8, converted to UTF-16 without the ANSI code page. Unix paths retain native
    // byte spelling. Embedded NULs are refused on every platform. Lines end in a bare
    // line feed on every platform, so one trace reads the same wherever it was written. Each
    // line is stamped with this process's id and the milliseconds `clock` has advanced since
    // the sink was opened; `clock` is borrowed and must outlive the sink.
    static std::unique_ptr<FileTraceSink> open(const std::string& path, OpenMode mode, const Clock& clock);

    // A sink on standard error, which it does not close.
    static std::unique_ptr<FileTraceSink> standard_error(const Clock& clock);

    // Closes the file, unless the sink writes to standard error.
    ~FileTraceSink() override;
    FileTraceSink(const FileTraceSink&) = delete;
    FileTraceSink& operator=(const FileTraceSink&) = delete;

    // Writes "[<ms> pid <id>] <message>" and flushes, so a trace read while the program is
    // still running, or after it crashed, is complete up to the last line. Lines from
    // concurrent threads are serialized. The level is not written: everything a graphics
    // trace sends is LogLevel::Trace. A write that fails is dropped.
    void log(LogLevel level, std::string_view message) noexcept override;

private:
    FileTraceSink(std::FILE* stream, bool owns_stream, const Clock& clock);

    std::FILE* stream_;
    bool owns_stream_;
    const Clock& clock_;
    std::int64_t opened_nanos_;
    std::mutex mutex_;
};

}  // namespace ckv::term
