// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/history.hpp"

#include <algorithm>

namespace ckv::ui {

const std::vector<std::string> HistoryRegistry::kEmpty;

void HistoryRegistry::record(std::string_view key, std::string value) {
    Entry& entry = lists_.try_emplace(std::string(key), Entry{{}, default_capacity_}).first->second;
    if (entry.capacity == 0) return;

    auto it = std::find(entry.values.begin(), entry.values.end(), value);
    if (it != entry.values.end()) entry.values.erase(it);
    entry.values.insert(entry.values.begin(), std::move(value));
    if (entry.values.size() > entry.capacity) entry.values.resize(entry.capacity);
}

const std::vector<std::string>& HistoryRegistry::entries(std::string_view key) const noexcept {
    auto it = lists_.find(std::string(key));
    return it == lists_.end() ? kEmpty : it->second.values;
}

void HistoryRegistry::clear(std::string_view key) {
    auto it = lists_.find(std::string(key));
    if (it != lists_.end()) it->second.values.clear();
}

void HistoryRegistry::set_capacity(std::string_view key, std::size_t capacity) {
    Entry& entry = lists_.try_emplace(std::string(key), Entry{{}, default_capacity_}).first->second;
    entry.capacity = capacity;
    if (entry.values.size() > capacity) entry.values.resize(capacity);
}

std::size_t HistoryRegistry::capacity(std::string_view key) const noexcept {
    auto it = lists_.find(std::string(key));
    return it == lists_.end() ? default_capacity_ : it->second.capacity;
}

}  // namespace ckv::ui
