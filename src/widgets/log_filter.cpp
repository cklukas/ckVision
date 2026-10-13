// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_filter.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ckv::widgets {
namespace {
void add(std::uint64_t& value, std::uint64_t count = 1) noexcept {
    value += std::min(count, std::numeric_limits<std::uint64_t>::max() - value);
}
std::size_t lower_sequence(const LogModel& model, std::uint64_t sequence) noexcept {
    std::size_t first = 0, last = model.size();
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        if (model.at(middle)->id.sequence < sequence) first = middle + 1;
        else last = middle;
    }
    return first;
}
bool same_query(const LogFilterQuery& a, const LogFilterQuery& b) noexcept {
    if (a.include != b.include || a.exclude != b.exclude || a.case_sensitive != b.case_sensitive ||
        a.severity_mask != b.severity_mask || a.source_count != b.source_count || a.field_count != b.field_count ||
        a.field_matcher != b.field_matcher || a.field_context != b.field_context) return false;
    for (std::size_t i = 0; i < a.source_count; ++i) if (a.sources[i] != b.sources[i]) return false;
    for (std::size_t i = 0; i < a.field_count; ++i) {
        const auto& x = a.fields[i]; const auto& y = b.fields[i];
        if (x.name != y.name || x.value != y.value || x.mode != y.mode) return false;
    }
    return true;
}
}

struct LogFilter::State {
    struct Work {
        LogRecordId id;
        std::uint64_t content_revision = 0;
        std::size_t phase = 0, field = 0, name_byte = 0;
        LogLiteralCursor cursor{};
        bool replacement = false, field_found = false, metadata_checked = false;
    };
    LogFilterQuery query;
    bool passthrough = true;
    std::array<LogLiteral, kLogMaxFields + 2> literals;
    std::vector<LogRecordId> ids;
    std::size_t head = 0, count = 0;
    std::uint64_t next_sequence = 1, processed_sequence = 0;
    std::optional<Work> work;
    std::optional<LogRecordId> mutable_id;
    std::uint64_t mutable_revision = 0;
    std::vector<char> snapshot_bytes;
    std::optional<LogRecord> snapshot;

    explicit State(const LogLimits& limits) : ids(limits.records),
        snapshot_bytes(limits.record_bytes * 4 + kLogMetadataBytes) {
        query.include.reserve(kLogLiteralBytes); query.exclude.reserve(kLogLiteralBytes);
        for (auto& field : query.fields) { field.name.reserve(kLogFieldNameBytes); field.value.reserve(kLogLiteralBytes); }
    }
    LogRecordId id_at(std::size_t index) const noexcept { return ids[(head + index) % ids.size()]; }
    std::optional<std::size_t> find(LogRecordId id) const noexcept {
        std::size_t first = 0, last = count;
        while (first < last) {
            const auto middle = first + (last - first) / 2;
            if (id_at(middle).sequence < id.sequence) first = middle + 1;
            else last = middle;
        }
        return first < count && id_at(first) == id ? std::optional{first} : std::nullopt;
    }
    void compile(const LogFilterQuery& value) noexcept {
        query.include.assign(value.include); query.exclude.assign(value.exclude);
        query.case_sensitive = value.case_sensitive; query.severity_mask = value.severity_mask;
        query.sources = value.sources; query.source_count = value.source_count;
        passthrough = value.include.empty() && value.exclude.empty() && value.severity_mask == kLogAllSeverities &&
            !value.source_count && !value.field_count && !value.field_matcher;
        query.field_count = value.field_count; query.field_matcher = value.field_matcher; query.field_context = value.field_context;
        literals[0].set(value.include, value.case_sensitive); literals[1].set(value.exclude, value.case_sensitive);
        for (std::size_t i = 0; i < kLogMaxFields; ++i) {
            auto& target = query.fields[i];
            if (i < value.field_count) {
                target.name.assign(value.fields[i].name); target.value.assign(value.fields[i].value); target.mode = value.fields[i].mode;
                literals[i + 2].set(target.value, value.case_sensitive);
            } else { target.name.clear(); target.value.clear(); literals[i + 2].set({}); }
        }
    }
    void reset() noexcept {
        head = 0; count = 0; next_sequence = 1; processed_sequence = 0;
        work.reset(); mutable_id.reset(); snapshot.reset();
    }
    void store(const LogRecord& record) noexcept {
        snapshot = record;
        std::size_t offset = 0;
        const auto copy = [&](std::string_view value) {
            const auto begin = offset;
            if (!value.empty()) std::memcpy(snapshot_bytes.data() + offset, value.data(), value.size());
            offset += value.size();
            return std::string_view{snapshot_bytes.data() + begin, value.size()};
        };
        snapshot->raw = copy(record.raw); snapshot->text = copy(record.text);
        for (std::size_t i = 0; i < record.field_count; ++i)
            snapshot->fields[i] = {copy(record.fields[i].name), copy(record.fields[i].value)};
    }
};

std::string_view log_query_error_name(LogQueryError error) noexcept {
    switch (error) {
        case LogQueryError::None: return {};
        case LogQueryError::TooLong: return "query too long";
        case LogQueryError::InvalidText: return "invalid query text";
        case LogQueryError::InvalidPredicate: return "invalid predicate";
        case LogQueryError::RevisionExhausted: return "query revision exhausted";
    }
    return "invalid query";
}
LogFilter::LogFilter(const LogModel& model, LogQueryBudget budget)
    : model_(model), budget_(budget) {
    if (!budget.records || budget.records > 256 ||
        !budget.comparisons || budget.comparisons > kLogBatchBytes)
        throw std::invalid_argument("invalid log query budget");
    budget_.records = std::min(budget_.records, model.limits().filter_records_per_step);
    active_ = std::make_unique<State>(model.limits());
    building_ = std::make_unique<State>(model.limits());
    update_status();
}
LogFilter::~LogFilter() = default;
LogQueryError LogFilter::validate(const LogFilterQuery& query) noexcept {
    if (query.include.size() > kLogLiteralBytes || query.exclude.size() > kLogLiteralBytes) return LogQueryError::TooLong;
    if (!LogLiteral::valid(query.include) || !LogLiteral::valid(query.exclude)) return LogQueryError::InvalidText;
    if (query.source_count > kLogQuerySources || query.field_count > kLogMaxFields || (query.severity_mask & ~static_cast<unsigned>(kLogAllSeverities)))
        return LogQueryError::InvalidPredicate;
    for (std::size_t i = 0; i < query.source_count; ++i) {
        if (!query.sources[i]) return LogQueryError::InvalidPredicate;
        for (std::size_t j = 0; j < i; ++j) if (query.sources[i] == query.sources[j]) return LogQueryError::InvalidPredicate;
    }
    std::size_t bytes = query.include.size() + query.exclude.size();
    for (std::size_t i = 0; i < query.field_count; ++i) {
        const auto& field = query.fields[i];
        if (field.name.empty() || static_cast<unsigned>(field.mode) > static_cast<unsigned>(LogFieldMatch::Contains))
            return LogQueryError::InvalidPredicate;
        if (field.name.size() > kLogFieldNameBytes || field.value.size() > kLogLiteralBytes) return LogQueryError::TooLong;
        if (!LogLiteral::valid(field.name) || !LogLiteral::valid(field.value)) return LogQueryError::InvalidText;
        bytes += field.name.size() + field.value.size();
    }
    return bytes <= kLogQueryBytes ? LogQueryError::None : LogQueryError::TooLong;
}
const LogFilterQuery& LogFilter::requested_query() const noexcept { return status_.query_pending ? building_->query : active_->query; }
const LogFilterQuery& LogFilter::published_query() const noexcept { return active_->query; }
bool LogFilter::begin_query() noexcept {
    if (status_.requested_revision == std::numeric_limits<std::uint64_t>::max()) {
        status_.error = LogQueryError::RevisionExhausted; return false;
    }
    if (!status_.query_pending) building_->compile(active_->query);
    building_->reset();
    ++status_.requested_revision;
    status_.query_pending = true;
    status_.error = LogQueryError::None;
    return true;
}
bool LogFilter::set_query(const LogFilterQuery& query) noexcept {
    const auto error = validate(query);
    if (error != LogQueryError::None) { status_.error = error; return false; }
    if (same_query(query, requested_query())) { status_.error = LogQueryError::None; return true; }
    if (!begin_query()) return false;
    building_->compile(query);
    update_status();
    return true;
}
bool LogFilter::set_include(std::string_view value) noexcept {
    if (value.size() > kLogLiteralBytes) { status_.error = LogQueryError::TooLong; return false; }
    if (!LogLiteral::valid(value)) { status_.error = LogQueryError::InvalidText; return false; }
    if (requested_query().include == value) { status_.error = LogQueryError::None; return true; }
    const auto& query = requested_query();
    std::size_t bytes = value.size() + query.exclude.size();
    for (std::size_t i = 0; i < query.field_count; ++i) bytes += query.fields[i].name.size() + query.fields[i].value.size();
    if (bytes > kLogQueryBytes) { status_.error = LogQueryError::TooLong; return false; }
    if (!begin_query()) return false;
    building_->query.include.assign(value);
    building_->literals[0].set(value, building_->query.case_sensitive);
    const auto& target = building_->query;
    building_->passthrough = target.include.empty() && target.exclude.empty() && target.severity_mask == kLogAllSeverities &&
        !target.source_count && !target.field_count && !target.field_matcher;
    update_status();
    return true;
}

bool LogFilter::prune(State& state) noexcept {
    const auto first = model_.at(0);
    std::size_t removed = 0;
    if (!first) removed = state.count;
    else {
        std::size_t end = state.count;
        while (removed < end) {
            const auto middle = removed + (end - removed) / 2;
            if (state.id_at(middle).sequence < first->id.sequence) removed = middle + 1;
            else end = middle;
        }
    }
    state.head = (state.head + removed) % state.ids.size(); state.count -= removed;
    if (state.snapshot && !model_.index_of(state.snapshot->id)) state.snapshot.reset();
    if (state.mutable_id && !model_.index_of(*state.mutable_id)) state.mutable_id.reset();
    return removed != 0;
}
bool LogFilter::pending(const State& state) const noexcept {
    if (state.passthrough) return false;
    if (state.work || state.processed_sequence < model_.latest_sequence()) return true;
    if (state.mutable_id) {
        const auto index = model_.index_of(*state.mutable_id);
        const auto record = index ? model_.at(*index) : std::nullopt;
        if (record && record->text_revision != state.mutable_revision) return true;
    }
    return false;
}

bool LogFilter::advance(State& state, bool allow_admission, std::size_t& records, std::size_t& operations) noexcept {
    if (state.passthrough) { state.processed_sequence = model_.latest_sequence(); return false; }
    if (records == 0) return false;
    if (state.work) {
        const auto index = model_.index_of(state.work->id);
        const auto record = index ? model_.at(*index) : std::nullopt;
        if (!record) {
            if (!state.work->replacement) state.next_sequence = state.work->id.sequence + 1;
            state.work.reset();
            return true;
        }
        if (state.work->content_revision != record->text_revision) {
            const bool replacement = state.work->replacement;
            state.work = State::Work{record->id, record->text_revision, 0, 0, 0, {}, replacement, false};
        }
    }
    if (!state.work && state.mutable_id) {
        const auto index = model_.index_of(*state.mutable_id);
        const auto record = index ? model_.at(*index) : std::nullopt;
        if (record && record->text_revision == state.mutable_revision) {
            if (state.snapshot && state.snapshot->id == record->id && state.snapshot->content_revision != record->content_revision) {
                state.snapshot->content_revision = record->content_revision;
                state.snapshot->discarded_bytes = record->discarded_bytes;
                state.snapshot->partial = record->partial; state.snapshot->incomplete = record->incomplete;
                if (&state == active_.get()) add(status_.publication);
            }
            if (!record->partial) state.mutable_id.reset();
        }
        if (record && record->text_revision != state.mutable_revision)
            state.work = State::Work{record->id, record->text_revision, 0, 0, 0, {}, true, false};
    }
    if (!state.work) {
        if (!allow_admission) return false;
        const auto record = model_.at(lower_sequence(model_, state.next_sequence));
        if (!record) {
            state.processed_sequence = model_.latest_sequence();
            state.next_sequence = state.processed_sequence + 1;
            return false;
        }
        state.work = State::Work{record->id, record->text_revision};
    }
    --records;
    ++status_.last_step_records;
    const auto index = model_.index_of(state.work->id);
    const auto record = model_.at(*index);
    auto& work = *state.work;
    const auto finish = [&](bool accepted) {
        const auto id = work.id;
        bool changed = false;
        if (work.replacement && state.count && state.id_at(state.count - 1) == id) { --state.count; changed = true; }
        if (accepted) { state.ids[(state.head + state.count) % state.ids.size()] = id; ++state.count; changed = true; }
        if (record->partial || (state.snapshot && state.snapshot->id == id)) { state.store(*record); changed = true; }
        if (record->partial) { state.mutable_id = id; state.mutable_revision = record->text_revision; }
        else state.mutable_id.reset();
        if (!work.replacement) { state.processed_sequence = id.sequence; state.next_sequence = id.sequence + 1; }
        state.work.reset();
        add(status_.evaluated);
        if (&state == active_.get() && changed) add(status_.publication);
    };
    const auto& query = state.query;
    if (!(query.severity_mask & (1U << static_cast<unsigned>(record->severity)))) { finish(false); return true; }
    if (query.source_count) {
        bool included = false;
        for (std::size_t i = 0; i < query.source_count; ++i) included = included || query.sources[i] == record->id.source;
        if (!included) { finish(false); return true; }
    }
    // Called once per record evaluation, not once per yielded literal step.
    if (!work.metadata_checked) {
        work.metadata_checked = true;
        if (query.field_matcher && !query.field_matcher({record->fields.data(), record->field_count}, query.field_context)) {
            finish(false); return true;
        }
    }
    while (work.phase < query.field_count + 2) {
        std::string_view value = record->text;
        const auto& literal = state.literals[work.phase];
        bool exclude = work.phase == 1;
        if (work.phase >= 2) {
            const auto& predicate = query.fields[work.phase - 2];
            while (!work.field_found && work.field < record->field_count) {
                const auto name = record->fields[work.field].name;
                if (name.size() != predicate.name.size()) { ++work.field; work.name_byte = 0; continue; }
                while (work.name_byte < name.size()) {
                    if (!operations) return false;
                    --operations;
                    if (name[work.name_byte] != predicate.name[work.name_byte]) { ++work.field; work.name_byte = 0; break; }
                    ++work.name_byte;
                }
                if (work.name_byte == name.size()) work.field_found = true;
            }
            if (!work.field_found) { finish(false); return true; }
            value = record->fields[work.field].value;
            if (predicate.mode == LogFieldMatch::Present) {
                ++work.phase; work.field = 0; work.name_byte = 0; work.field_found = false; work.cursor = {}; continue;
            }
            if (predicate.mode == LogFieldMatch::Equals && value.size() != literal.pattern().size()) { finish(false); return true; }
        }
        bool matched = literal.pattern().empty();
        if (exclude && literal.pattern().empty()) matched = false;
        if (!literal.pattern().empty()) {
            LogTextRange range;
            const auto result = literal.scan(value, work.cursor, operations, range);
            if (result == LogLiteralStep::Pending) return false;
            matched = result == LogLiteralStep::Match;
        }
        if ((exclude && matched) || (!exclude && !matched)) { finish(false); return true; }
        ++work.phase; work.cursor = {}; work.field = 0; work.name_byte = 0; work.field_found = false;
    }
    finish(true);
    return true;
}

void LogFilter::update_status() noexcept {
    status_.matches = size(); status_.retained = model_.size();
    status_.processed_sequence = active_->passthrough ? model_.latest_sequence() : active_->processed_sequence;
    status_.live_pending = pending(*active_);
}
bool LogFilter::step() noexcept {
    status_.last_step_records = 0; status_.last_step_comparisons = 0;
    if (prune(*active_)) add(status_.publication);
    prune(*building_);
    if (active_->passthrough && observed_model_revision_ != model_.revision()) add(status_.publication);
    observed_model_revision_ = model_.revision();
    auto records = budget_.records, operations = budget_.comparisons;
    // Revalidate the published partial first; its old bounded snapshot stays
    // visible until the replacement predicate is completely evaluated.
    if (status_.query_pending) advance(*active_, false, records, operations);
    auto& target = status_.query_pending ? *building_ : *active_;
    while (records && operations) {
        const auto previous_records = records, previous_operations = operations;
        const bool progressed = advance(target, true, records, operations);
        if (!progressed && previous_records == records && previous_operations == operations) break;
        if (!progressed && !operations) break;
    }
    if (status_.query_pending && !pending(*building_)) {
        active_.swap(building_);
        status_.published_revision = status_.requested_revision;
        status_.query_pending = false;
        add(status_.publication);
    }
    status_.last_step_comparisons = budget_.comparisons - operations;
    add(status_.comparisons, status_.last_step_comparisons);
    update_status();
    return status_.query_pending || status_.live_pending;
}
std::size_t LogFilter::size() const noexcept { return active_->passthrough ? model_.size() : active_->count; }
std::optional<LogRecord> LogFilter::at(std::size_t index) const noexcept {
    if (active_->passthrough) return model_.at(index);
    if (index >= active_->count) return std::nullopt;
    const auto id = active_->id_at(index);
    if (active_->snapshot && active_->snapshot->id == id) return active_->snapshot;
    return detail(id);
}
std::optional<std::size_t> LogFilter::index_of(LogRecordId id) const noexcept { return active_->passthrough ? model_.index_of(id) : active_->find(id); }
std::optional<LogRecord> LogFilter::detail(LogRecordId id) const noexcept {
    const auto index = model_.index_of(id); return index ? model_.at(*index) : std::nullopt;
}
std::size_t LogFilter::allocated_bytes() const noexcept {
    std::size_t total = sizeof(*this) + 2 * sizeof(State);
    for (const auto* state : {active_.get(), building_.get()}) {
        total += state->ids.capacity() * sizeof(LogRecordId) + state->snapshot_bytes.capacity();
        total += state->query.include.capacity() + state->query.exclude.capacity();
        for (const auto& field : state->query.fields) total += field.name.capacity() + field.value.capacity();
        for (const auto& literal : state->literals) total += literal.allocated_bytes() - sizeof(LogLiteral);
    }
    return total;
}

bool LogContextProvider::set_anchor(LogRecordId id, std::size_t before, std::size_t after) noexcept {
    if (before > 128 || after > 128 || !model_.index_of(id)) return false;
    anchor_ = id; before_ = before; after_ = after; add(local_revision_); return true;
}
std::pair<std::size_t, std::size_t> LogContextProvider::window() const noexcept {
    const auto index = anchor_ ? model_.index_of(*anchor_) : std::nullopt;
    if (!index) return {0, 0};
    const auto begin = *index - std::min(*index, before_);
    const auto end = *index + 1 + std::min(model_.size() - *index - 1, after_);
    return {begin, end - begin};
}
std::size_t LogContextProvider::size() const noexcept { return window().second; }
std::optional<LogRecord> LogContextProvider::at(std::size_t index) const noexcept {
    const auto [begin, count] = window(); return index < count ? model_.at(begin + index) : std::nullopt;
}
std::optional<std::size_t> LogContextProvider::index_of(LogRecordId id) const noexcept {
    const auto index = model_.index_of(id); const auto [begin, count] = window();
    return index && *index >= begin && *index - begin < count ? std::optional{*index - begin} : std::nullopt;
}
std::uint64_t LogContextProvider::revision() const noexcept {
    auto result = model_.revision(); add(result, local_revision_); return result;
}
}  // namespace ckv::widgets
