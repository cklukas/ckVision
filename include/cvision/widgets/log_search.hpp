// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/widgets/log_filter.hpp"

namespace ckv::widgets {
enum class LogSearchDirection { Next, Previous };
enum class LogSearchPhase { Idle, Pending, Found, NotFound, Stale };
struct LogSearchOrigin { LogRecordId id; std::size_t byte = 0; };
struct LogSearchMatch {
    LogRecordId id;
    LogTextRange range;
    std::uint64_t query_revision = 0, visibility_revision = 0, text_revision = 0;
};
struct LogSearchStatus {
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
    explicit LogSearch(const LogRecordProvider& provider, LogQueryBudget budget = {});
    bool set_query(std::string_view text, bool case_sensitive = true) noexcept;
    std::string_view query() const noexcept { return literal_.pattern(); }
    bool case_sensitive() const noexcept { return literal_.case_sensitive(); }
    bool request(LogSearchDirection direction, std::optional<LogSearchOrigin> origin = {}, bool wrap = true) noexcept;
    bool step() noexcept;
    std::optional<LogSearchMatch> take_result() noexcept;
    const LogSearchStatus& status() const noexcept { return status_; }
    const LogRecordProvider& provider() const noexcept { return provider_; }
    void mark(const LogRecord& record, std::size_t begin, std::size_t end,
              std::span<unsigned char> marks) const noexcept override;
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
