// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/log_source.hpp"
#include "cvision/core/style.hpp"

namespace ckv::widgets {

// Interpret lone CR as a line boundary or replacement of the current progress line.
enum class LogCarriageReturn : std::uint8_t { LineBreak, Progress };

// All strings borrow the model until its next mutation. Copy the view value,
// not its strings, for immediate paint/inspection. Fields borrow the same
// lifetime, and metadata() borrows this view's field array as well.
struct LogRecord {
    // Stable admission identity and revision for raw/metadata/decoder changes.
    LogRecordId id;
    std::uint64_t content_revision = 0;
    // Changes only with display text, not raw decoder/counter bookkeeping.
    std::uint64_t text_revision = 0;
    // Explicit style attributes and borrowed original bytes, never ANSI execution.
    Attr attrs = static_cast<Attr>(0);
    std::string_view raw;
    // Sanitized UTF-8; explicit multiline records preserve LF separators.
    std::string_view text;
    // Observed metadata, partial/incomplete flags and known discarded-byte count.
    LogSeverity severity = LogSeverity::Unknown;
    std::optional<std::int64_t> timestamp_nanos;
    std::array<LogField, kLogMaxFields> fields{};
    std::size_t field_count = 0;
    bool partial = false;
    bool incomplete = false;
    std::uint64_t discarded_bytes = 0;
    LogMetadata metadata() const noexcept {
        return {severity, timestamp_nanos, {fields.data(), field_count}};
    }
};

// Cumulative committed/evicted/lost/processed bytes and provider mutation revision.
struct LogModelStats {
    // Saturating counters; clearing retention does not rewind them.
    std::uint64_t committed = 0;
    std::uint64_t evicted = 0;
    std::uint64_t discarded_bytes = 0;
    std::uint64_t processed_bytes = 0;
    std::uint64_t revision = 0;
};
// Bounded append acceptance and actual consumed bytes/new record admissions.
struct LogAppendResult {
    // Invalid requests consume nothing; retained bytes may be fewer than consumed.
    bool accepted = false;
    std::size_t bytes = 0, records = 0;
};

// UI-thread snapshot borrowed by a LogView. No mutation during a lookup or
// paint. Strings expire at the next mutation; IDs never alias another record.
// Records are in ascending admission-sequence order.
// index_of must be logarithmic or better. latest_sequence counts admissions
// before filtering; it never decreases, including when retention is cleared.
class LogRecordProvider {
public:
    // Provider lifetime encloses every view/query borrowing it.
    virtual ~LogRecordProvider() = default;
    // Eligible count/lookups, stable-ID lookup, mutation/admission/eviction
    // revisions, bounded text capacity and total retained count.
    virtual std::size_t size() const noexcept = 0;
    virtual std::optional<LogRecord> at(std::size_t index) const noexcept = 0;
    virtual std::optional<std::size_t> index_of(LogRecordId id) const noexcept = 0;
    virtual std::uint64_t revision() const noexcept = 0;
    virtual std::uint64_t latest_sequence() const noexcept = 0;
    virtual std::uint64_t evicted_count() const noexcept = 0;
    virtual std::size_t max_text_bytes() const noexcept = 0;
    virtual std::size_t retained_size() const noexcept { return size(); }
    // Visibility/query publication identity, independent of later admissions.
    // Zero means this provider has no filter query.
    virtual std::uint64_t query_revision() const noexcept { return 0; }
};

// Instance-owned, UI-thread-only bounded storage/decoder. Construction allocates
// its complete payload/slot/staging capacity; append, eviction and lookup do
// not allocate. LogLimits::valid() is required, otherwise construction throws
// invalid_argument. Oversized lines keep a prefix and drain through their next
// separator, counting discarded bytes. Invalid append requests change nothing.
class LogModel : public LogRecordProvider {
public:
    // Validate product capacities and allocate complete storage outside dispatch.
    explicit LogModel(LogLimits limits = {},
                      LogCarriageReturn carriage_return = LogCarriageReturn::LineBreak);
    LogModel(const LogModel&) = delete;
    LogModel& operator=(const LogModel&) = delete;

    // Borrow configured limits/counters and inspect current source/generation.
    const LogLimits& limits() const noexcept { return limits_; }
    const LogModelStats& stats() const noexcept { return stats_; }
    LogSourceId source() const noexcept { return source_; }
    std::uint64_t generation() const noexcept { return generation_; }
    // Conservative storage bound: model object plus backing capacities, not
    // allocator bookkeeping. Includes slots/partial/UTF-8/metadata staging;
    // the encoding string's capacity is counted even when stored inline.
    std::size_t allocated_bytes() const noexcept;
    // Actual occupied arena bytes, excluding fixed slots/index/staging capacities.
    std::size_t retained_payload_bytes() const noexcept { return arena_used_; }
    std::size_t size() const noexcept override { return count_ + (partial_id_ ? 1U : 0U); }
    std::optional<LogRecord> at(std::size_t index) const noexcept override;
    std::optional<std::size_t> index_of(LogRecordId id) const noexcept override;

    std::uint64_t revision() const noexcept override { return stats_.revision; }
    std::uint64_t latest_sequence() const noexcept override { return next_sequence_ - 1; }
    std::uint64_t evicted_count() const noexcept override { return stats_.evicted; }
    std::size_t max_text_bytes() const noexcept override { return limits_.record_bytes * 3; }

    // A changed source/generation commits existing nonempty partial text as
    // incomplete, then begins BOM detection afresh. Repeating the current
    // pair is a no-op; a backwards generation of the same source is refused.
    bool begin_source(LogSourceId source, std::uint64_t generation, bool consume_bom = true) noexcept;
    // Each call is bounded by batch_bytes. A partial line is visible under
    // one stable ID, including while a UTF-8 scalar crosses a batch boundary.
    bool append_bytes(std::string_view bytes) noexcept;
    // Stops before admitting more than max_records new identities. Zero may
    // continue an existing partial/CRLF/BOM, but never starts another record.
    // The unconsumed suffix remains caller-owned. bytes is actual consumed
    // input, not retained payload; malformed/oversized bytes still consume it.
    LogAppendResult append_bytes_bounded(std::string_view bytes, std::size_t max_records) noexcept;
    // An explicit record is not split by LF and never interpreted as ANSI.
    // Metadata and content are copied into bounded storage during this call.
    // Existing streamed partial text is committed incomplete first.
    bool append_record(std::string_view raw, LogMetadata metadata = {},
                       Attr attrs = static_cast<Attr>(0)) noexcept;
    // Bounded structured-record construction across dispatch quanta. Metadata
    // is copied once, LF remains data, and one partial ID survives each append.
    // Mixing byte-stream input into an open explicit record is rejected.
    bool begin_record(LogMetadata metadata = {}, Attr attrs = static_cast<Attr>(0)) noexcept;
    bool append_record_bytes(std::string_view raw) noexcept;
    bool end_record() noexcept;
    // End a real stream, not temporary file EOF: resolve pending UTF-8/BOM
    // bytes and commit the unfinished record with incomplete=true.
    void finish() noexcept;
    // Discards retained/partial data, keeping IDs, source and cumulative
    // counters. Evicted increases by the entries removed. Never rewinds IDs.
    void clear() noexcept;

private:
    struct FieldRange { std::size_t name = 0, name_size = 0, value = 0, value_size = 0; };
    struct Slot {
        LogRecordId id;
        std::uint64_t content_revision = 0, text_revision = 0;
        Attr attrs = static_cast<Attr>(0);
        std::size_t offset = 0, raw_size = 0, text_size = 0, allocation = 0;
        LogSeverity severity = LogSeverity::Unknown;
        std::optional<std::int64_t> timestamp;
        std::array<FieldRange, kLogMaxFields> fields{};
        std::size_t field_count = 0;
        bool incomplete = false;
        std::uint64_t discarded = 0;
    };

    bool start_partial() noexcept;
    void content_byte(char byte) noexcept;
    void line_byte(char byte) noexcept;
    void decode_pending(bool final) noexcept;
    void flush_bom() noexcept;
    void commit_partial(bool incomplete) noexcept;
    void reset_partial() noexcept;
    void evict_oldest() noexcept;
    LogRecord partial_record() const noexcept;

    LogLimits limits_;
    LogCarriageReturn carriage_return_;
    std::vector<char> arena_;
    std::vector<Slot> slots_;
    std::vector<char> raw_, display_;
    std::string encoded_;
    std::array<char, kLogMetadataBytes> metadata_bytes_{};
    std::array<FieldRange, kLogMaxFields> partial_fields_{};
    std::size_t metadata_size_ = 0, field_count_ = 0;
    Attr attrs_ = static_cast<Attr>(0);
    LogSeverity severity_ = LogSeverity::Unknown;
    std::optional<std::int64_t> timestamp_;
    std::optional<LogRecordId> partial_id_;
    LogSourceId source_ = 0;
    std::uint64_t generation_ = 0, next_sequence_ = 1, partial_discarded_ = 0, partial_text_revision_ = 0;
    std::array<char, 4> utf8_bytes_{};
    std::size_t utf8_size_ = 0;
    std::array<char, 3> bom_bytes_{};
    std::size_t bom_size_ = 0;
    bool bom_pending_ = true, after_cr_ = false, multiline_ = false;
    std::size_t head_ = 0, count_ = 0, arena_tail_ = 0, arena_used_ = 0;
    LogModelStats stats_;
};

}  // namespace ckv::widgets
