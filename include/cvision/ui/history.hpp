// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Application-scoped named history lists (the architecture §5
// "Application services"): input lines, combo boxes, and file dialogs
// referencing the same key share one deduplicated, capacity-bounded
// list rather than each keeping widget-local state.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ckv::ui {

class HistoryRegistry {
public:
    // Per-key capacity: the newest `capacity` entries are kept, oldest
    // dropped. A capacity of 0 disables recording entirely (record()
    // becomes a no-op) — useful for an application that wants the
    // shared-key mechanism without persistence for a given key.
    explicit HistoryRegistry(std::size_t default_capacity = 20) : default_capacity_(default_capacity) {}

    // Most-recently-used first: `value` moves to the front if already
    // present (deduplication — never two copies of the same string in
    // one key's list), otherwise it is inserted at the front and the
    // list is truncated to that key's capacity.
    void record(std::string_view key, std::string value);

    // Newest-first. Empty (not an error) for a key with no history yet.
    const std::vector<std::string>& entries(std::string_view key) const noexcept;

    void clear(std::string_view key);

    void set_capacity(std::string_view key, std::size_t capacity);
    std::size_t capacity(std::string_view key) const noexcept;

private:
    struct Entry {
        std::vector<std::string> values;
        std::size_t capacity;
    };

    std::size_t default_capacity_;
    std::unordered_map<std::string, Entry> lists_;
    static const std::vector<std::string> kEmpty;
};

}  // namespace ckv::ui
