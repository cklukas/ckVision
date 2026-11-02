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
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include "cvision/core/utf8.hpp"
#include "cvision/term/windows_text.hpp"
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
    if (path.empty() || path.find('\0') != std::string::npos) return nullptr;
#if defined(_WIN32)
    if (!utf8::is_valid(path)) return nullptr;
    const auto wide = windows_utf16(path);
    if (!wide) return nullptr;
    const auto native = windows_absolute_extended_path(*wide);
    if (!native) return nullptr;
    const DWORD access = mode == OpenMode::Truncate ? GENERIC_WRITE : FILE_APPEND_DATA;
    const DWORD disposition = mode == OpenMode::Truncate ? CREATE_ALWAYS : OPEN_ALWAYS;
    HANDLE handle = ::CreateFileW(native->c_str(), access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, disposition,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return nullptr;
    return std::unique_ptr<FileTraceSink>(new FileTraceSink(handle, clock));
#else
    std::FILE* const stream = std::fopen(path.c_str(), mode == OpenMode::Truncate ? "wb" : "ab");
#endif
    if (stream == nullptr) return nullptr;
    return std::unique_ptr<FileTraceSink>(new FileTraceSink(stream, true, clock));
}

std::unique_ptr<FileTraceSink> FileTraceSink::standard_error(const Clock& clock) {
    return std::unique_ptr<FileTraceSink>(new FileTraceSink(stderr, false, clock));
}

FileTraceSink::FileTraceSink(std::FILE* stream, bool owns_stream, const Clock& clock)
    : stream_(stream), owns_stream_(owns_stream), clock_(clock), opened_nanos_(clock.now_nanos()) {}

#if defined(_WIN32)
FileTraceSink::FileTraceSink(void* native_handle, const Clock& clock)
    : stream_(nullptr), owns_stream_(true), native_handle_(native_handle), clock_(clock),
      opened_nanos_(clock.now_nanos()) {}
#endif

FileTraceSink::~FileTraceSink() {
#if defined(_WIN32)
    if (native_handle_ != nullptr) { (void)::CloseHandle(static_cast<HANDLE>(native_handle_)); return; }
#endif
    if (owns_stream_) std::fclose(stream_);
}

void FileTraceSink::log(LogLevel, std::string_view message) noexcept {
    const double ms = static_cast<double>(clock_.now_nanos() - opened_nanos_) / 1e6;
    const std::lock_guard<std::mutex> guard(mutex_);
#if defined(_WIN32)
    if (native_handle_ != nullptr) {
        std::array<char, 96> prefix{};
        const int prefix_size = std::snprintf(prefix.data(), prefix.size(), "[%8.1fms pid %lu] ", ms, process_id());
        if (prefix_size < 0 || static_cast<std::size_t>(prefix_size) >= prefix.size()) return;
        const auto write = [handle = static_cast<HANDLE>(native_handle_)](const char* data, std::size_t size) {
            while (size != 0) {
                const DWORD count = static_cast<DWORD>(std::min<std::size_t>(size, std::numeric_limits<DWORD>::max()));
                DWORD written = 0;
                if (!::WriteFile(handle, data, count, &written, nullptr) || written != count) return false;
                data += written; size -= written;
            }
            return true;
        };
        if (!write(prefix.data(), static_cast<std::size_t>(prefix_size)) ||
            !write(message.data(), message.size()) || !write("\n", 1)) return;
        return;
    }
#endif
    std::fprintf(stream_, "[%8.1fms pid %lu] %.*s\n", ms, process_id(), static_cast<int>(message.size()),
                 message.data());
    std::fflush(stream_);
}

}  // namespace ckv::term
