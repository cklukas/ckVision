// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/directory_service.hpp"

#include "cvision/core/ascii.hpp"

namespace ckv {

bool directory_item_less(const DirectoryItem& a, const DirectoryItem& b) noexcept {
    if (a.is_directory != b.is_directory) return a.is_directory;
    if (a.name != b.name) return a.name < b.name;
    return a.id < b.id;
}

bool directory_item_matches(const DirectoryItem& item, const DirectoryQuery& query) noexcept {
    if (item.id == 0 || item.name.empty() || item.name == "." || item.name == "..") return false;
    if (!query.show_hidden && item.name.front() == '.') return false;
    if (!item.name.starts_with(query.prefix)) return false;
    if (item.is_directory) return true;
    if (query.directories_only) return false;
    if (query.extensions.empty()) return true;
    for (const auto& extension : query.extensions) {
        if (extension.empty()) return true;
        const bool needs_dot = extension.front() != '.';
        const auto length = extension.size() + (needs_dot ? 1U : 0U);
        if (item.name.size() < length) continue;
        const auto offset = item.name.size() - extension.size();
        if (needs_dot && item.name[offset - 1] != '.') continue;
        bool matches = true;
        for (std::size_t i = 0; i < extension.size(); ++i) {
            if (ascii_lower(item.name[offset + i]) != ascii_lower(extension[i])) {
                matches = false;
                break;
            }
        }
        if (matches) return true;
    }
    return false;
}

bool valid_directory_query(const DirectoryQuery& query) noexcept {
    if (query.revision == 0 || query.extensions.size() > kDirectoryPageSize) return false;
    std::size_t bytes = 0;
    const auto add = [&bytes](const std::string& value) {
        if (value.size() > kDirectoryTextBytes - bytes || value.find('\0') != std::string::npos) return false;
        bytes += value.size();
        return true;
    };
    if (!add(query.directory) || !add(query.prefix) || !add(query.path)) return false;
    for (const auto& extension : query.extensions)
        if (!add(extension)) return false;
    if (query.boundary && !add(query.boundary->name)) return false;
    if (query.preferred && !add(query.preferred->name)) return false;
    return true;
}

}  // namespace ckv
