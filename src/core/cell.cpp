// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/cell.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <new>

#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"

namespace ckv {
namespace {

struct HeapGrapheme {
    std::atomic<std::uint32_t> refs{1};
    std::uint32_t size = 0;
    char data[1]{};
};

HeapGrapheme* allocate_heap_grapheme(std::string_view utf8) {
    const std::size_t total = offsetof(HeapGrapheme, data) + utf8.size();
    void* mem = ::operator new(total);
    auto* block = new (mem) HeapGrapheme{};
    block->refs.store(1, std::memory_order_relaxed);
    block->size = static_cast<std::uint32_t>(utf8.size());
    std::memcpy(block->data, utf8.data(), utf8.size());
    return block;
}

const HeapGrapheme* load_heap_ptr(const char (&bytes)[15]) noexcept {
    std::uintptr_t addr = 0;
    static_assert(sizeof(addr) <= sizeof(bytes));
    std::memcpy(&addr, bytes, sizeof(addr));
    return reinterpret_cast<const HeapGrapheme*>(addr);
}

HeapGrapheme* load_heap_ptr(char (&bytes)[15]) noexcept {
    std::uintptr_t addr = 0;
    static_assert(sizeof(addr) <= sizeof(bytes));
    std::memcpy(&addr, bytes, sizeof(addr));
    return reinterpret_cast<HeapGrapheme*>(addr);
}

void store_heap_ptr(char (&bytes)[15], const HeapGrapheme* ptr) noexcept {
    const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    std::memset(bytes, 0, sizeof(bytes));
    std::memcpy(bytes, &addr, sizeof(addr));
}

}  // namespace

void Cell::retain_heap() const noexcept {
    auto* block = const_cast<HeapGrapheme*>(load_heap_ptr(bytes_));
    CKV_ASSERT(block != nullptr);
    block->refs.fetch_add(1, std::memory_order_relaxed);
}

void Cell::release_heap() noexcept {
    HeapGrapheme* block = load_heap_ptr(bytes_);
    CKV_ASSERT(block != nullptr);
    if (block->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        block->~HeapGrapheme();
        ::operator delete(block);
    }
    len_and_flags_ = 0;
}

std::string_view Cell::heap_view() const noexcept {
    const HeapGrapheme* block = load_heap_ptr(bytes_);
    CKV_ASSERT(block != nullptr);
    return std::string_view(block->data, block->size);
}

Cell Cell::from_grapheme(std::string_view grapheme, Style style) {
    // Fast path for empty or single printable ASCII byte: zero heap allocations.
    if (grapheme.empty()) {
        Cell cell;
        cell.style_ = style;
        return cell;
    }
    if (grapheme.size() == 1) {
        const auto b = static_cast<unsigned char>(grapheme[0]);
        if (b >= 0x20u && b < 0x7Fu) {
            Cell cell;
            cell.bytes_[0] = static_cast<char>(b);
            cell.len_and_flags_ = static_cast<std::uint8_t>((1u << kWidthShift) | 1u);
            cell.style_ = style;
            return cell;
        }
    }

    const std::string sanitized_buf = text::is_sanitized_display_text(grapheme)
                                          ? std::string{}
                                          : text::sanitize_display_text(grapheme);
    const std::string_view sanitized = sanitized_buf.empty() && !grapheme.empty()
                                           ? grapheme
                                           : std::string_view{sanitized_buf};
    if (sanitized.empty()) {
        Cell cell;
        cell.style_ = style;
        return cell;
    }

    const std::size_t end = text::grapheme_end(sanitized, 0);
    const std::string_view cluster = sanitized.substr(0, end);
    const int width = text::grapheme_width(cluster);
    CKV_ASSERT(width >= 0 && width <= 2);

    Cell cell;
    cell.style_ = style;
    if (cluster.size() <= kMaxInlineBytes) {
        std::memset(cell.bytes_, 0, sizeof(cell.bytes_));
        std::memcpy(cell.bytes_, cluster.data(), cluster.size());
        cell.len_and_flags_ = static_cast<std::uint8_t>(
            (static_cast<std::uint8_t>(width) << kWidthShift) |
            static_cast<std::uint8_t>(cluster.size()));
    } else {
        HeapGrapheme* block = allocate_heap_grapheme(cluster);
        store_heap_ptr(cell.bytes_, block);
        cell.len_and_flags_ = static_cast<std::uint8_t>(
            kHeapFlag | (static_cast<std::uint8_t>(width) << kWidthShift));
    }
    return cell;
}

}  // namespace ckv
