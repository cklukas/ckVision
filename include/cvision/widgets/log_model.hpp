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

namespace ckv::widgets {

enum class LogCarriageReturn : std::uint8_t { LineBreak, Progress };

// All strings borrow the model until its next mutation. Copy the view value,
// not its strings, for immediate paint/inspection. Fields borrow the same
// lifetime, and metadata() borrows this view's field array as well.
struct LogRecord {
    LogRecordId id;
    std::string_view raw;
    // Sanitized UTF-8; explicit multiline records preserve LF separators.
    std::string_view text;
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

struct LogModelStats {
    std::uint64_t committed = 0;
    std::uint64_t evicted = 0;
    std::uint64_t discarded_bytes = 0;
    std::uint64_t processed_bytes = 0;
    std::uint64_t revision = 0;
};

// Instance-owned, UI-thread-only bounded storage/decoder. Construction allocates
// its complete payload/slot/staging capacity; append, eviction and lookup do
// not allocate. LogLimits::valid() is required, otherwise construction throws
// invalid_argument. Oversized lines keep a prefix and drain through their next
// separator, counting discarded bytes. Invalid append requests change nothing.
class LogModel {
public:
    explicit LogModel(LogLimits limits = {},
                      LogCarriageReturn carriage_return = LogCarriageReturn::LineBreak);
    LogModel(const LogModel&) = delete;
    LogModel& operator=(const LogModel&) = delete;

    const LogLimits& limits() const noexcept { return limits_; }
    const LogModelStats& stats() const noexcept { return stats_; }
    LogSourceId source() const noexcept { return source_; }
    std::uint64_t generation() const noexcept { return generation_; }
    // Conservative storage bound: model object plus backing capacities, not
    // allocator bookkeeping. Includes slots/partial/UTF-8/metadata staging;
    // the encoding string's capacity is counted even when stored inline.
    std::size_t allocated_bytes() const noexcept;
    std::size_t retained_payload_bytes() const noexcept { return arena_used_; }
    std::size_t size() const noexcept { return count_ + (partial_id_ ? 1U : 0U); }
    std::optional<LogRecord> at(std::size_t index) const noexcept;
    std::optional<std::size_t> index_of(LogRecordId id) const noexcept;

    // A changed source/generation commits existing nonempty partial text as
    // incomplete, then begins BOM detection afresh. Repeating the current
    // pair is a no-op; a backwards generation of the same source is refused.
    bool begin_source(LogSourceId source, std::uint64_t generation, bool consume_bom = true) noexcept;
    // Each call is bounded by batch_bytes. A partial line is visible under
    // one stable ID, including while a UTF-8 scalar crosses a batch boundary.
    bool append_bytes(std::string_view bytes) noexcept;
    // An explicit record is not split by LF and never interpreted as ANSI.
    // Metadata and content are copied into bounded storage during this call.
    // Existing streamed partial text is committed incomplete first.
    bool append_record(std::string_view raw, LogMetadata metadata = {}) noexcept;
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
    LogSeverity severity_ = LogSeverity::Unknown;
    std::optional<std::int64_t> timestamp_;
    std::optional<LogRecordId> partial_id_;
    LogSourceId source_ = 0;
    std::uint64_t generation_ = 0, next_sequence_ = 1, partial_discarded_ = 0;
    std::array<char, 4> utf8_bytes_{};
    std::size_t utf8_size_ = 0;
    std::array<char, 3> bom_bytes_{};
    std::size_t bom_size_ = 0;
    bool bom_pending_ = true, after_cr_ = false, multiline_ = false;
    std::size_t head_ = 0, count_ = 0, arena_tail_ = 0, arena_used_ = 0;
    LogModelStats stats_;
};

}  // namespace ckv::widgets
