// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cvision/core/filesystem.hpp"

namespace ckv {

// Hard bounds on one query/result, independent of the directory's size.
inline constexpr std::size_t kDirectoryPageSize = 64;
inline constexpr std::size_t kDirectoryTextBytes = 32768;
inline constexpr std::size_t kDirectoryNameBytes = 4096;

// Identity and order key within one discovery generation. Zero is invalid.
struct DirectoryItem {
    // Nonzero identity within one source generation.
    std::uint64_t id = 0;
    // Exact entry basename; not a display-escaped path.
    std::string name;
    // Whether navigation may enter this entry.
    bool is_directory = false;
    // Memberwise identity, spelling and kind equality.
    friend bool operator==(const DirectoryItem&, const DirectoryItem&) = default;
};

// Browse enumerates one directory. Resolve classifies one submitted full path.
enum class DirectoryOperation { Browse, Resolve };
// Pending/Partial may change again; Ready and Failed finish this query.
enum class DirectoryState { Pending, Partial, Ready, Failed };

// Immutable request. All paths are explicit full paths; no implicit cwd.
struct DirectoryQuery {
    // Consumer query revision; nonzero and monotonically increasing.
    std::uint64_t revision = 0;
    // Explicit refresh generation; query changes alone do not rescan the source.
    std::uint64_t source_generation = 0;
    // Whether to browse a directory or classify a submitted path.
    DirectoryOperation operation = DirectoryOperation::Browse;
    // Explicit directory path; canonical on publication.
    std::string directory;
    // Exact case-sensitive basename prefix; empty matches all names.
    std::string prefix;
    // ASCII-insensitive file suffixes; empty matches all files.
    std::vector<std::string> extensions;
    // Include names starting with a dot when true.
    bool show_hidden = false;
    // Exclude non-directories when true.
    bool directories_only = false;
    // Strict page boundary in directory-first, name, id order. Backward asks
    // for the last page before it; forward asks for the first page after it.
    std::optional<DirectoryItem> boundary;
    bool backward = false;
    // Retain an explicitly chosen eligible entry across partial reordering.
    std::optional<DirectoryItem> preferred;
    // Resolve for navigation additionally probes opening the directory before
    // reporting success. Select-only directory identity does not require it.
    bool for_navigation = false;
    // Full path to classify for Resolve. Browse does not inspect this member.
    std::string path;
};

// Size-bounded immutable publication. Entries and counts share one eligibility
// rule. Page boundaries omit preferred rows lying outside the page.
struct DirectorySnapshot {
    // Consumer query revision; nonzero and monotonically increasing.
    std::uint64_t revision = 0;
    // Publication sequence within this session; partial batches increment it.
    std::uint64_t sequence = 0;
    // Explicit refresh generation; query changes alone do not rescan the source.
    std::uint64_t source_generation = 0;
    // Discovery/classification progress for this revision.
    DirectoryState state = DirectoryState::Pending;
    // Explicit directory path; canonical on publication.
    std::string directory;
    // Ordered bounded page, optionally retaining an explicitly preferred row.
    std::vector<DirectoryItem> entries;
    // Entries indexed so far, before filtering.
    std::uint64_t discovered = 0;
    // Entries matching this query among those discovered.
    std::uint64_t matching = 0;
    // An earlier eligible page is currently known.
    bool has_previous = false;
    // A later eligible page is currently known.
    bool has_next = false;
    // First ordinary page boundary; excludes an out-of-page preferred row.
    std::optional<DirectoryItem> first;
    // Last ordinary page boundary; excludes an out-of-page preferred row.
    std::optional<DirectoryItem> last;
    // Resolve: true iff the path resolves to an existing directory; path then
    // holds its canonical identity. Otherwise it holds the normalized input.
    bool resolved_directory = false;
    std::string path;
    // Bounded plain diagnostic, never a platform error's untrusted bytes.
    std::string diagnostic;
};

// One UI-owned consumer. Methods and destruction are bounded and nonblocking;
// implementations never invoke client callbacks or retain views/Application.
class DirectorySession {
public:
    // Signals cancellation without waiting for blocked platform work.
    virtual ~DirectorySession() = default;
    // Replace queued work. Revision must be nonzero and increase monotonically.
    // Oversized queries produce Failed rather than exceeding storage bounds.
    virtual void request(DirectoryQuery query) = 0;
    // Latest immutable publication; null before any delivery. Old revisions
    // can be observed and must be rejected by the consumer.
    virtual std::shared_ptr<const DirectorySnapshot> snapshot() const noexcept = 0;
};

// Explicit application-owned execution boundary. Never construct/destroy a
// native implementation inside dispatch; outstanding platform calls may block
// shutdown. The service must outlive all its sessions.
class DirectoryService {
public:
    // Releases execution resources; native shutdown may wait outside dispatch.
    virtual ~DirectoryService() = default;
    // A bounded session, or null when all configured slots are occupied.
    virtual std::unique_ptr<DirectorySession> open_session() = 0;
    // Path spelling helpers only. Consumers may not call I/O through this
    // reference during dispatch; all discovery/classification uses sessions.
    virtual const FileSystem& filesystem() const noexcept = 0;
};

// Shared deterministic eligibility/order/size policy, also for scripted sources.
bool directory_item_less(const DirectoryItem& a, const DirectoryItem& b) noexcept;
bool directory_item_matches(const DirectoryItem& item, const DirectoryQuery& query) noexcept;
bool valid_directory_query(const DirectoryQuery& query) noexcept;

}  // namespace ckv
