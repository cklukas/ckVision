// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// Runs in a separate executable: never duplicates the process-wide allocation
// replacements in test_hot_path_allocations.cpp or the TSan runtime.
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <new>
#include <thread>

#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/log_model.hpp"
#include "cvision/term/log_file_service.hpp"
#include "scratch_directory.hpp"

namespace {
std::atomic<bool> measuring{false};
std::atomic<std::size_t> allocations{0};
void* allocate(std::size_t size) {
    if (measuring.load(std::memory_order_relaxed)) allocations.fetch_add(1, std::memory_order_relaxed);
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}
void* allocate_nothrow(std::size_t size) noexcept {
    if (measuring.load(std::memory_order_relaxed)) allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size == 0 ? 1 : size);
}
}
void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return allocate_nothrow(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return allocate_nothrow(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }

CK_TEST(log_model_first_and_repeated_appends_lookups_evictions_and_clear_allocate_nothing) {
    ckv::LogLimits limits;
    limits.records = 4;
    limits.record_bytes = 32;
    limits.retained_bytes = ckv::kLogMetadataBytes + 128;
    ckv::widgets::LogModel model(limits);
    const std::array fields{ckv::LogField{"unit", "demo"}};
    const ckv::LogMetadata metadata{ckv::LogSeverity::Info, 100, fields};
    bool accepted = true;
    allocations = 0;
    measuring = true;
    accepted = model.begin_source(1, 1) && accepted;
    for (std::size_t i = 0; i < 1000; ++i) {
        accepted = model.append_bytes("a\xE4") && accepted;
        accepted = model.append_bytes("\xB8\xAD\xFF\n") && accepted;
        accepted = model.append_record("one\ntwo", metadata) && accepted;
        const auto entry = model.at(model.size() - 1);
        accepted = entry && model.index_of(entry->id).has_value() && accepted;
        if (i % 13 == 0) model.clear();
    }
    model.finish();
    measuring = false;
    CK_CHECK(accepted);
    CK_CHECK(allocations == 0);
}

CK_TEST(native_log_file_requests_reads_and_transfers_allocate_nothing_after_open) {
    ckv::testing::ScratchDirectory scratch("log-file-allocations");
    const auto path = scratch.path() / "source.txt";
    { std::ofstream output(path, std::ios::binary); output << "text"; }
    ckv::term::NativeLogFileService service(1);
    auto session = service.open_session();
    ckv::LogFileResult result;
    ckv::LogFileRequest open;
    open.revision = 1;
    const auto utf8_path = path.u8string();
    open.path.assign(reinterpret_cast<const char*>(utf8_path.data()), utf8_path.size());
    session->request(std::move(open));
    while (!session->take(result)) std::this_thread::yield();
    CK_CHECK(result.state == ckv::LogSourceState::Following);
    bool correct = true;
    allocations = 0; measuring = true;
    for (std::uint64_t revision = 2; revision < 202; ++revision) {
        ckv::LogFileRequest request;
        request.revision = revision; request.operation = ckv::LogFileOperation::Read; request.bytes = 4;
        session->request(std::move(request));
        while (!session->take(result)) std::this_thread::yield();
        correct = correct && result.state == ckv::LogSourceState::Following && result.bytes().size() == 4;
    }
    session->cancel();
    measuring = false;
    CK_CHECK(correct); CK_CHECK(allocations == 0);
}
