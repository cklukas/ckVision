// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/file_trace_sink.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

#include "cvision/testing/cktest.hpp"

using ckv::LogLevel;
using ckv::ManualClock;
using ckv::term::FileTraceSink;

namespace {

// A file of this test's own under the system temporary directory, removed afterwards. The
// random name keeps concurrent runs of the suite from sharing one.
struct ScratchFile {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("ckvision-file-trace-" + std::to_string(std::random_device{}()) + ".log");
    ~ScratchFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    std::string contents() const {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
};

}  // namespace

CK_TEST(a_trace_file_is_stamped_per_line_and_readable_before_the_sink_closes) {
    ScratchFile file;
    ManualClock clock;
    clock.advance(5'000'000'000);
    auto sink = FileTraceSink::open(file.path.string(), FileTraceSink::OpenMode::Truncate, clock);
    CK_CHECK(sink != nullptr);
    clock.advance(1'500'000);
    sink->log(LogLevel::Trace, "presenter: frame");
    // Flushed per line, so the trace is complete while the program still runs.
    const std::string text = file.contents();
    CK_CHECK(text.find("    1.5ms pid ") != std::string::npos);
    CK_CHECK(text.find("] presenter: frame\n") != std::string::npos);
}

CK_TEST(truncate_starts_a_trace_file_afresh_and_append_keeps_what_is_there) {
    ScratchFile file;
    ManualClock clock;
    {
        auto host = FileTraceSink::open(file.path.string(), FileTraceSink::OpenMode::Truncate, clock);
        host->log(LogLevel::Trace, "first run");
    }
    {
        auto host = FileTraceSink::open(file.path.string(), FileTraceSink::OpenMode::Truncate, clock);
        host->log(LogLevel::Trace, "host line");
        auto child = FileTraceSink::open(file.path.string(), FileTraceSink::OpenMode::Append, clock);
        child->log(LogLevel::Trace, "child line");
    }
    const std::string text = file.contents();
    CK_CHECK(text.find("first run") == std::string::npos);
    CK_CHECK(text.find("host line") != std::string::npos);
    CK_CHECK(text.find("child line") != std::string::npos);
}

CK_TEST(a_trace_file_that_cannot_be_opened_yields_no_sink) {
    ManualClock clock;
    const std::filesystem::path missing =
        std::filesystem::temp_directory_path() / "ckvision-no-such-directory" / "trace.log";
    CK_CHECK(FileTraceSink::open(missing.string(), FileTraceSink::OpenMode::Truncate, clock) == nullptr);
}
