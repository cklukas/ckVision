// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/widgets/log_filter.hpp"

namespace ckv::widgets {
// Navigation direction and resumable request/result lifecycle.
enum class LogSearchDirection { Next, Previous };
enum class LogSearchPhase { Idle, Pending, Found, NotFound, Stale };
// Stable record plus byte origin; default byte starts at record beginning.
struct LogSearchOrigin { LogRecordId id; std::size_t byte = 0; };
// Borrow-free match identity and captured query/provider/text revisions.
struct LogSearchMatch {
    // Matched record, half-open byte interval and the revisions validated on take.
    LogRecordId id;
    LogTextRange range;
    // Captured pattern, eligible-publication and record-text revisions.
    std::uint64_t query_revision = 0, visibility_revision = 0, text_revision = 0;
};
// Query state, bounded comparison/visit work and wrap/staleness diagnostics.
struct LogSearchStatus {
    // Query/visibility identities, cumulative and last-step work, phase/error
    // and wrap/incomplete coverage observations; no implicit history scan.
    std::uint64_t query_revision = 1, visibility_revision = 0;
    std::uint64_t comparisons = 0, record_visits = 0;
    std::size_t last_step_comparisons = 0, last_step_records = 0;
    LogSearchPhase phase = LogSearchPhase::Idle;
    LogQueryError error = LogQueryError::None;
    bool wrapped = false, incomplete = false;
};

// Independent literal search over eligible published records. request is
// bounded; step resumes navigation within record/comparison quotas. It captures
// an upper admission bound, so continuous later arrivals cannot extend a request
// forever. A new filter visibility revision restarts under that revision.
// Result delivery verifies the ID/content/query again; stale results never
// select a different record. Construction allocates the literal's fixed storage.
class LogSearch : public LogHighlighter {
public:
    // Borrow a published provider and reserve bounded literal workspace.
    explicit LogSearch(const LogRecordProvider& provider, LogQueryBudget budget = {});
    // Accept a bounded literal and cancel obsolete work; false preserves query.
    bool set_query(std::string_view text, bool case_sensitive = true) noexcept;
    // Borrow current pattern and inspect ASCII case sensitivity.
    std::string_view query() const noexcept { return literal_.pattern(); }
    bool case_sensitive() const noexcept { return literal_.case_sensitive(); }
    // Start navigation from an optional stable origin, with explicit wrap policy.
    bool request(LogSearchDirection direction, std::optional<LogSearchOrigin> origin = {}, bool wrap = true) noexcept;
    // Resume one comparison/record quantum; never scan future arrivals forever.
    bool step() noexcept;
    // Consume a freshly revalidated match; stale IDs/revisions never select.
    std::optional<LogSearchMatch> take_result() noexcept;
    // Borrow current diagnostics and the enclosing published provider.
    const LogSearchStatus& status() const noexcept { return status_; }
    const LogRecordProvider& provider() const noexcept { return provider_; }
    void mark(const LogRecord& record, std::size_t begin, std::size_t end,
              std::span<unsigned char> marks) const noexcept override;
    // Object plus compiled literal capacity, excluding provider/model storage.
    std::size_t allocated_bytes() const noexcept;
private:
    struct Work { LogRecordId id; std::uint64_t revision = 0; LogLiteralCursor cursor{}; std::optional<LogTextRange> best{}; };
    void restart() noexcept;
    bool wrap_or_finish() noexcept;
    std::optional<std::size_t> candidate() const noexcept;
    void move_past(LogRecordId id) noexcept;
    bool in_leg(LogRecordId id, LogTextRange range) const noexcept;
    const LogRecordProvider& provider_;
    LogQueryBudget budget_;
    LogLiteral literal_;
    std::optional<LogSearchOrigin> origin_;
    std::optional<Work> work_;
    std::optional<LogSearchMatch> result_;
    LogSearchDirection direction_ = LogSearchDirection::Next;
    LogSearchStatus status_;
    std::uint64_t first_ = 0, upper_ = 0, next_sequence_ = 0, observed_evictions_ = 0;
    bool wrap_ = true, second_leg_ = false;
};
}  // namespace ckv::widgets
