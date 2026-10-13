// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <memory>
#include <utility>
#include "cvision/widgets/log_literal.hpp"
#include "cvision/widgets/log_model.hpp"

namespace ckv::widgets {

inline constexpr std::size_t kLogQueryBytes = 8192;
inline constexpr std::size_t kLogQuerySources = 8;
inline constexpr std::size_t kLogFieldNameBytes = 128;
inline constexpr std::uint16_t kLogAllSeverities = (1U << (static_cast<unsigned>(LogSeverity::Critical) + 1U)) - 1U;
enum class LogFieldMatch { Present, Equals, Contains };
struct LogFieldPredicate {
    std::string name, value;
    LogFieldMatch mode = LogFieldMatch::Equals;
};
using LogFieldMatcher = bool (*)(std::span<const LogField>, const void*) noexcept;
struct LogFilterQuery {
    std::string include, exclude;
    bool case_sensitive = true;
    // One bit per LogSeverity, including Unknown. No inference from text.
    std::uint16_t severity_mask = kLogAllSeverities;
    std::array<LogSourceId, kLogQuerySources> sources{};
    std::size_t source_count = 0;
    std::array<LogFieldPredicate, kLogMaxFields> fields{};
    std::size_t field_count = 0;
    // Optional AND predicate. Context is borrowed until both published and
    // rebuilding queries retire it. Must be deterministic, allocation-free,
    // nonblocking and bounded over the metadata contract's eight/4096 bytes.
    LogFieldMatcher field_matcher = nullptr;
    const void* field_context = nullptr;
};
enum class LogQueryError { None, TooLong, InvalidText, InvalidPredicate, RevisionExhausted };
std::string_view log_query_error_name(LogQueryError error) noexcept;
struct LogQueryBudget {
    std::size_t records = 256;
    std::size_t comparisons = kLogBatchBytes;
};
struct LogFilterStatus {
    std::uint64_t requested_revision = 1, published_revision = 1;
    std::uint64_t publication = 1, evaluated = 0, comparisons = 0;
    std::uint64_t processed_sequence = 0;
    std::size_t matches = 0, retained = 0, last_step_records = 0, last_step_comparisons = 0;
    bool query_pending = false, live_pending = false;
    LogQueryError error = LogQueryError::None;
};

// UI-thread incremental filter over an exclusive-writer LogModel. Two fixed
// ID rings and bounded partial snapshots keep the old query honest while a
// new revision builds. Call step after model mutation and before painting.
// No history scan in getters, set_query, paint or input dispatch. Construction
// allocates; setters/steps/publication do not. Quotas cap each step; custom
// metadata callbacks retain their explicit bounded-work obligation.
class LogFilter : public LogRecordProvider {
public:
    explicit LogFilter(const LogModel& model, LogQueryBudget budget = {});
    ~LogFilter() override;
    LogFilter(const LogFilter&) = delete;
    LogFilter& operator=(const LogFilter&) = delete;
    bool set_query(const LogFilterQuery& query) noexcept;
    bool set_include(std::string_view text) noexcept;
    const LogFilterQuery& requested_query() const noexcept;
    const LogFilterQuery& published_query() const noexcept;
    // True when more bounded work is pending. Invalid/zero budgets are
    // rejected at construction, not treated as permission for an endless loop.
    bool step() noexcept;
    const LogFilterStatus& status() const noexcept { return status_; }
    std::size_t allocated_bytes() const noexcept;
    std::size_t size() const noexcept override;
    std::optional<LogRecord> at(std::size_t index) const noexcept override;
    std::optional<std::size_t> index_of(LogRecordId id) const noexcept override;
    std::uint64_t revision() const noexcept override { return status_.publication; }
    std::uint64_t query_revision() const noexcept override { return status_.published_revision; }
    std::uint64_t latest_sequence() const noexcept override { return model_.latest_sequence(); }
    std::uint64_t evicted_count() const noexcept override { return model_.evicted_count(); }
    std::size_t max_text_bytes() const noexcept override { return model_.max_text_bytes(); }
    std::size_t retained_size() const noexcept override { return model_.size(); }
    // Raw/safe bytes and metadata of any retained ID, even when excluded.
    std::optional<LogRecord> detail(LogRecordId id) const noexcept;
private:
    struct State;
    static LogQueryError validate(const LogFilterQuery& query) noexcept;
    bool begin_query() noexcept;
    bool advance(State& state, bool allow_admission, std::size_t& records,
                 std::size_t& comparisons) noexcept;
    bool prune(State& state) noexcept;
    bool pending(const State& state) const noexcept;
    void update_status() noexcept;
    const LogModel& model_;
    LogQueryBudget budget_;
    std::unique_ptr<State> active_, building_;
    LogFilterStatus status_;
    std::uint64_t observed_model_revision_ = 0;
};

// Bounded original-model context, independent of the primary filter result.
// Neighbors never enter its match count/index just because they are displayed.
// If the anchor is evicted, the context is empty rather than silently moved.
class LogContextProvider : public LogRecordProvider {
public:
    explicit LogContextProvider(const LogModel& model) : model_(model) {}
    bool set_anchor(LogRecordId id, std::size_t before = 3, std::size_t after = 3) noexcept;
    std::size_t size() const noexcept override;
    std::optional<LogRecord> at(std::size_t index) const noexcept override;
    std::optional<std::size_t> index_of(LogRecordId id) const noexcept override;
    std::uint64_t revision() const noexcept override;
    std::uint64_t latest_sequence() const noexcept override { return model_.latest_sequence(); }
    std::uint64_t evicted_count() const noexcept override { return model_.evicted_count(); }
    std::size_t max_text_bytes() const noexcept override { return model_.max_text_bytes(); }
    std::size_t retained_size() const noexcept override { return model_.size(); }
    bool anchor_available() const noexcept { return anchor_ && model_.index_of(*anchor_).has_value(); }
private:
    std::pair<std::size_t, std::size_t> window() const noexcept;
    const LogModel& model_;
    std::optional<LogRecordId> anchor_;
    std::size_t before_ = 3, after_ = 3;
    std::uint64_t local_revision_ = 0;
};

}  // namespace ckv::widgets
