// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/file_trace_sink.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

#include "cvision/testing/cktest.hpp"
#include "scratch_directory.hpp"

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

CK_TEST(unicode_trace_file_names_keep_their_exact_spelling_and_append_behavior) {
    ckv::testing::ScratchDirectory scratch("unicode-trace");
    const auto path = scratch.path() / std::filesystem::path(u8"記録-é-😀.log");
    const auto encoded = path.u8string();
    const std::string name(encoded.begin(), encoded.end());
    ManualClock clock;
    {
        auto sink = FileTraceSink::open(name, FileTraceSink::OpenMode::Truncate, clock);
        CK_CHECK(sink != nullptr);
        if (!sink) return;
        sink->log(LogLevel::Trace, "first line");
    }
    {
        auto sink = FileTraceSink::open(name, FileTraceSink::OpenMode::Append, clock);
        CK_CHECK(sink != nullptr);
        if (!sink) return;
        sink->log(LogLevel::Trace, "second line");
    }
    std::ifstream input(path, std::ios::binary);
    const std::string bytes(std::istreambuf_iterator<char>(input), {});
    CK_CHECK(bytes.find("first line\n") != std::string::npos);
    CK_CHECK(bytes.find("second line\n") != std::string::npos);
    CK_CHECK(bytes.find('\r') == std::string::npos);
    std::size_t entries = 0;
    for (const auto& entry : std::filesystem::directory_iterator(scratch.path())) {
        CK_CHECK(entry.path().filename() == path.filename());
        ++entries;
    }
    CK_CHECK(entries == 1);
}

CK_TEST(embedded_nul_trace_paths_do_not_create_their_truncated_prefix) {
    ckv::testing::ScratchDirectory scratch("nul-trace");
    const auto prefix = scratch.path() / "prefix.log";
    const auto encoded = prefix.u8string();
    std::string name(encoded.begin(), encoded.end());
    name.append("\0suffix", 7);
    ManualClock clock;
    CK_CHECK(FileTraceSink::open("", FileTraceSink::OpenMode::Truncate, clock) == nullptr);
    CK_CHECK(FileTraceSink::open(name, FileTraceSink::OpenMode::Truncate, clock) == nullptr);
    CK_CHECK(!std::filesystem::exists(prefix));
}

#if defined(_WIN32)
CK_TEST(malformed_utf8_trace_paths_do_not_create_ansi_named_files) {
    ckv::testing::ScratchDirectory scratch("invalid-trace");
    const auto encoded = scratch.path().u8string();
    const std::string name(encoded.begin(), encoded.end());
    ManualClock clock;
    CK_CHECK(FileTraceSink::open(name + "/invalid-\xff.log", FileTraceSink::OpenMode::Truncate, clock) == nullptr);
    CK_CHECK(std::filesystem::directory_iterator(scratch.path()) == std::filesystem::directory_iterator{});
}
#endif

CK_TEST(a_trace_file_is_stamped_per_line_and_readable_before_the_sink_closes) {
    ScratchFile file;
    ManualClock clock;
    clock.advance(5'000'000'000);
    auto sink = FileTraceSink::open(file.path.string(), FileTraceSink::OpenMode::Truncate, clock);
    CK_CHECK(sink != nullptr);
    if (!sink) return;
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
        CK_CHECK(host != nullptr);
        if (!host) return;
        host->log(LogLevel::Trace, "first run");
    }
    {
        auto host = FileTraceSink::open(file.path.string(), FileTraceSink::OpenMode::Truncate, clock);
        CK_CHECK(host != nullptr);
        if (!host) return;
        host->log(LogLevel::Trace, "host line");
        auto child = FileTraceSink::open(file.path.string(), FileTraceSink::OpenMode::Append, clock);
        CK_CHECK(child != nullptr);
        if (!child) return;
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
