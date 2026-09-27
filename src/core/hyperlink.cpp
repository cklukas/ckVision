// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/hyperlink.hpp"

#include <algorithm>
#include <limits>

#include "cvision/core/ascii.hpp"
#include "cvision/core/assert.hpp"

namespace ckv {

bool is_valid_hyperlink_target(std::string_view target) noexcept {
    if (target.empty() || target.size() > kMaxHyperlinkTargetBytes) return false;
    for (const char c : target) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x21 || byte > 0x7E) return false;
    }
    // RFC 3986 §3.1: scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ),
    // ended by the first ':'.
    if (!is_ascii_alpha(target.front())) return false;
    for (std::size_t i = 1; i < target.size(); ++i) {
        const char c = target[i];
        if (c == ':') return true;
        if (!is_ascii_alpha(c) && !is_ascii_digit(c) && c != '+' && c != '-' && c != '.') return false;
    }
    return false;
}

LinkTable::LinkTable(const LinkTable& other)
    : entries_(other.entries_), by_target_(other.by_target_) {
    free_.reserve(entries_.size());
    free_ = other.free_;
}

LinkTable& LinkTable::operator=(const LinkTable& other) {
    if (this == &other) return *this;
    // Assigning into existing capacity is what keeps a table that is copied
    // every frame (the presenter's record of the last one) allocation-free
    // once it has reached its working size.
    // A failure part-way leaves an empty table rather than three vectors
    // that disagree.
    try {
        entries_ = other.entries_;
        free_.reserve(entries_.size());
        free_ = other.free_;
        by_target_ = other.by_target_;
    } catch (...) {
        clear();
        throw;
    }
    return *this;
}

std::vector<LinkId>::const_iterator LinkTable::lower_bound(std::string_view target) const noexcept {
    return std::lower_bound(by_target_.begin(), by_target_.end(), target,
                            [this](LinkId id, std::string_view wanted) {
                                return std::string_view(entries_[id - 1].target) < wanted;
                            });
}

LinkId LinkTable::acquire(std::string_view target) {
    if (!is_valid_hyperlink_target(target)) return kNoLink;
    const auto at = lower_bound(target);
    if (at != by_target_.end() && entries_[*at - 1].target == target) {
        ++entries_[*at - 1].references;
        return *at;
    }
    // Everything that can throw happens before the table changes, so a
    // failed acquire leaves it exactly as it was.
    const auto position = at - by_target_.begin();
    by_target_.reserve(by_target_.size() + 1);
    std::string owned(target);
    LinkId id = kNoLink;
    if (free_.empty()) {
        CKV_ASSERT(entries_.size() < std::numeric_limits<LinkId>::max());
        // free_ can at most hold every id, so reserving that room now is
        // what lets release() record a freed id without allocating.
        free_.reserve(entries_.size() + 1);
        entries_.push_back(Entry{std::move(owned), 1});
        id = static_cast<LinkId>(entries_.size());
    } else {
        id = free_.back();
        free_.pop_back();
        entries_[id - 1] = Entry{std::move(owned), 1};
    }
    by_target_.insert(by_target_.begin() + position, id);
    return id;
}

void LinkTable::retain(LinkId id) noexcept {
    if (id == kNoLink) return;
    CKV_ASSERT(contains(id));
    ++entries_[id - 1].references;
}

void LinkTable::release(LinkId id) noexcept {
    if (id == kNoLink) return;
    CKV_ASSERT(contains(id));
    Entry& entry = entries_[id - 1];
    if (--entry.references > 0) return;
    const auto at = lower_bound(entry.target);
    CKV_ASSERT(at != by_target_.end() && *at == id);
    by_target_.erase(at);
    entry.target.clear();
    // Cannot allocate: acquire() reserved room in free_ for every id.
    free_.push_back(id);
}

std::string_view LinkTable::target(LinkId id) const noexcept {
    if (id == kNoLink) return {};
    CKV_ASSERT(contains(id));
    return entries_[id - 1].target;
}

bool LinkTable::contains(LinkId id) const noexcept {
    return id != kNoLink && id <= entries_.size() && entries_[id - 1].references > 0;
}

void LinkTable::clear() noexcept {
    entries_.clear();
    free_.clear();
    by_target_.clear();
}

}  // namespace ckv
