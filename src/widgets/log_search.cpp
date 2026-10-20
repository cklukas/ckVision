// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_search.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ckv::widgets {
namespace {
void add_search(std::uint64_t& value, std::uint64_t count = 1) noexcept {
    value += std::min(count, std::numeric_limits<std::uint64_t>::max() - value);
}
}
LogSearch::LogSearch(const LogRecordProvider& provider, LogQueryBudget budget)
    : provider_(provider), budget_(budget) {
    if (!budget.records || budget.records > 256 || !budget.comparisons || budget.comparisons > kLogBatchBytes)
        throw std::invalid_argument("invalid log search budget");
}
bool LogSearch::set_query(std::string_view text, bool sensitive) noexcept {
    if (text.size() > kLogLiteralBytes) { status_.error = LogQueryError::TooLong; return false; }
    if (!LogLiteral::valid(text)) { status_.error = LogQueryError::InvalidText; return false; }
    if (text == query() && sensitive == case_sensitive()) { status_.error = LogQueryError::None; return true; }
    if (status_.query_revision == std::numeric_limits<std::uint64_t>::max()) {
        status_.error = LogQueryError::RevisionExhausted; return false;
    }
    literal_.set(text, sensitive);
    ++status_.query_revision;
    status_.error = LogQueryError::None; status_.phase = LogSearchPhase::Idle;
    work_.reset(); result_.reset();
    return true;
}
bool LogSearch::request(LogSearchDirection direction, std::optional<LogSearchOrigin> origin, bool wrap) noexcept {
    direction_ = direction; origin_ = origin; wrap_ = wrap;
    result_.reset();
    if (query().empty() || status_.error != LogQueryError::None) { status_.phase = LogSearchPhase::Idle; return false; }
    restart();
    return true;
}
void LogSearch::restart() noexcept {
    work_.reset(); result_.reset(); second_leg_ = false;
    status_.visibility_revision = provider_.query_revision();
    status_.wrapped = false; status_.incomplete = false; observed_evictions_ = provider_.evicted_count(); status_.phase = LogSearchPhase::Pending;
    const auto first = provider_.at(0);
    const auto last = provider_.size() ? provider_.at(provider_.size() - 1) : std::nullopt;
    first_ = first ? first->id.sequence : 0; upper_ = last ? last->id.sequence : 0;
    if (!first || !last) { status_.phase = LogSearchPhase::NotFound; return; }
    next_sequence_ = origin_ ? origin_->id.sequence : direction_ == LogSearchDirection::Next ? first_ : upper_;
}
std::optional<std::size_t> LogSearch::candidate() const noexcept {
    std::size_t first = 0, last = provider_.size();
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        const auto record = provider_.at(middle);
        if (!record) return std::nullopt;
        const bool before = direction_ == LogSearchDirection::Next ? record->id.sequence < next_sequence_ : record->id.sequence <= next_sequence_;
        if (before) first = middle + 1;
        else last = middle;
    }
    if (direction_ == LogSearchDirection::Previous) { if (!first) return std::nullopt; --first; }
    return first < provider_.size() ? std::optional{first} : std::nullopt;
}
bool LogSearch::in_leg(LogRecordId id, LogTextRange range) const noexcept {
    if (!origin_) return true;
    if (direction_ == LogSearchDirection::Next) {
        if (!second_leg_) return id.sequence > origin_->id.sequence || (id == origin_->id && range.begin >= origin_->byte);
        return id.sequence < origin_->id.sequence || (id == origin_->id && range.begin < origin_->byte);
    }
    if (!second_leg_) return id.sequence < origin_->id.sequence || (id == origin_->id && range.begin < origin_->byte);
    return id.sequence > origin_->id.sequence || (id == origin_->id && range.begin >= origin_->byte);
}
void LogSearch::move_past(LogRecordId id) noexcept {
    next_sequence_ = direction_ == LogSearchDirection::Next ? id.sequence + 1 : id.sequence - 1;
    work_.reset();
}
bool LogSearch::wrap_or_finish() noexcept {
    work_.reset();
    if (wrap_ && origin_ && !second_leg_) {
        second_leg_ = true; status_.wrapped = true;
        next_sequence_ = direction_ == LogSearchDirection::Next ? first_ : upper_;
        return true;
    }
    status_.phase = LogSearchPhase::NotFound;
    return false;
}
bool LogSearch::step() noexcept {
    status_.last_step_comparisons = 0; status_.last_step_records = 0;
    if (status_.phase != LogSearchPhase::Pending) return false;
    if (provider_.query_revision() != status_.visibility_revision) restart();
    if (provider_.evicted_count() != observed_evictions_) status_.incomplete = true;
    auto operations = budget_.comparisons;
    while (status_.phase == LogSearchPhase::Pending && operations && status_.last_step_records < budget_.records) {
        if (!work_) {
            const auto index = candidate();
            const auto record = index ? provider_.at(*index) : std::nullopt;
            if (!record || record->id.sequence < first_ || record->id.sequence > upper_ ||
                (second_leg_ && origin_ && (direction_ == LogSearchDirection::Next ? record->id.sequence > origin_->id.sequence : record->id.sequence < origin_->id.sequence))) {
                if (!wrap_or_finish()) break;
                continue;
            }
            work_ = Work{record->id, record->text_revision};
            if (direction_ == LogSearchDirection::Next && !second_leg_ && origin_ && record->id == origin_->id)
                work_->cursor.byte = std::min(origin_->byte, record->text.size());
        }
        const auto index = provider_.index_of(work_->id);
        const auto record = index ? provider_.at(*index) : std::nullopt;
        ++status_.last_step_records;
        if (!record) { const auto id = work_->id; move_past(id); continue; }
        if (record->text_revision != work_->revision) {
            const auto id = work_->id;
            work_ = Work{id, record->text_revision};
            if (direction_ == LogSearchDirection::Next && !second_leg_ && origin_ && id == origin_->id)
                work_->cursor.byte = std::min(origin_->byte, record->text.size());
        }
        LogLiteralStep scanned;
        do {
            LogTextRange range;
            scanned = literal_.scan(record->text, work_->cursor, operations, range);
            if (scanned == LogLiteralStep::Match && in_leg(record->id, range)) {
                if (direction_ == LogSearchDirection::Next) {
                    result_ = LogSearchMatch{record->id, range, status_.query_revision, status_.visibility_revision, record->text_revision};
                    status_.phase = LogSearchPhase::Found;
                    break;
                }
                work_->best = range;
            }
        } while (scanned == LogLiteralStep::Match);
        if (status_.phase == LogSearchPhase::Found) break;
        if (scanned == LogLiteralStep::End) {
            if (work_->best) {
                result_ = LogSearchMatch{record->id, *work_->best, status_.query_revision, status_.visibility_revision, record->text_revision};
                status_.phase = LogSearchPhase::Found;
                break;
            }
            const auto id = work_->id; move_past(id);
        }
    }
    status_.last_step_comparisons = budget_.comparisons - operations;
    add_search(status_.comparisons, status_.last_step_comparisons);
    add_search(status_.record_visits, status_.last_step_records);
    return status_.phase == LogSearchPhase::Pending;
}
std::optional<LogSearchMatch> LogSearch::take_result() noexcept {
    if (!result_) return std::nullopt;
    const auto result = result_; result_.reset();
    const auto index = provider_.index_of(result->id);
    const auto record = index ? provider_.at(*index) : std::nullopt;
    if (status_.error != LogQueryError::None || !record || record->text_revision != result->text_revision || provider_.query_revision() != result->visibility_revision ||
        status_.query_revision != result->query_revision) {
        status_.phase = LogSearchPhase::Stale;
        return std::nullopt;
    }
    return result;
}
void LogSearch::mark(const LogRecord& record, std::size_t begin, std::size_t end,
                     std::span<unsigned char> marks) const noexcept {
    std::fill(marks.begin(), marks.end(), static_cast<unsigned char>(0));
    if (status_.error != LogQueryError::None || query().empty() || begin > end || end > record.text.size() || marks.size() != end - begin) return;
    const auto overlap = query().size() - 1;
    const auto first = begin - std::min(begin, overlap);
    const auto last = end + std::min(record.text.size() - end, overlap);
    const auto window = record.text.substr(first, last - first);
    LogLiteralCursor cursor;
    std::size_t operations = std::numeric_limits<std::size_t>::max();
    std::optional<LogTextRange> run;
    const auto flush = [&] {
        if (run) std::fill(marks.begin() + static_cast<std::ptrdiff_t>(run->begin - begin),
                           marks.begin() + static_cast<std::ptrdiff_t>(run->end - begin), static_cast<unsigned char>(1));
    };
    for (;;) {
        LogTextRange range;
        const auto result = literal_.scan(window, cursor, operations, range);
        if (result != LogLiteralStep::Match) break;
        range.begin = std::max(begin, range.begin + first); range.end = std::min(end, range.end + first);
        if (range.begin >= range.end) continue;
        if (run && range.begin > run->end) { flush(); run.reset(); }
        if (!run) run = range;
        else run->end = std::max(run->end, range.end);
    }
    flush();
}
std::size_t LogSearch::allocated_bytes() const noexcept {
    return sizeof(*this) + literal_.allocated_bytes() - sizeof(LogLiteral);
}
}  // namespace ckv::widgets
