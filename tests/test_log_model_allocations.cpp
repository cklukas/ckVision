// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// Runs in a separate executable: never duplicates the process-wide allocation
// replacements in test_hot_path_allocations.cpp or the TSan runtime.
#include <cstdlib>
#include <new>

#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/log_model.hpp"

namespace {
bool measuring = false;
std::size_t allocations = 0;
void* allocate(std::size_t size) {
    if (measuring) ++allocations;
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}
void* allocate_nothrow(std::size_t size) noexcept {
    if (measuring) ++allocations;
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
