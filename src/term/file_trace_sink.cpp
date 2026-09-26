// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/file_trace_sink.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ckv::term {

namespace {

// This process's id, so a host and the child inside it can be told apart in one file.
unsigned long process_id() noexcept {
#if defined(_WIN32)
    return static_cast<unsigned long>(::GetCurrentProcessId());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

}  // namespace

std::unique_ptr<FileTraceSink> FileTraceSink::open(const std::string& path, OpenMode mode, const Clock& clock) {
    std::FILE* const stream = std::fopen(path.c_str(), mode == OpenMode::Truncate ? "wb" : "ab");
    if (stream == nullptr) return nullptr;
    return std::unique_ptr<FileTraceSink>(new FileTraceSink(stream, true, clock));
}

std::unique_ptr<FileTraceSink> FileTraceSink::standard_error(const Clock& clock) {
    return std::unique_ptr<FileTraceSink>(new FileTraceSink(stderr, false, clock));
}

FileTraceSink::FileTraceSink(std::FILE* stream, bool owns_stream, const Clock& clock)
    : stream_(stream), owns_stream_(owns_stream), clock_(clock), opened_nanos_(clock.now_nanos()) {}

FileTraceSink::~FileTraceSink() {
    if (owns_stream_) std::fclose(stream_);
}

void FileTraceSink::log(LogLevel, std::string_view message) noexcept {
    const double ms = static_cast<double>(clock_.now_nanos() - opened_nanos_) / 1e6;
    const std::lock_guard<std::mutex> guard(mutex_);
    std::fprintf(stream_, "[%8.1fms pid %lu] %.*s\n", ms, process_id(), static_cast<int>(message.size()),
                 message.data());
    std::fflush(stream_);
}

}  // namespace ckv::term
