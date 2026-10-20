// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <mutex>
#include <vector>

#include "cvision/core/log_source.hpp"
#include "cvision/core/style.hpp"

namespace ckv {
// Coalesced cross-thread activity latch. The UI consumes it and schedules
// bounded lane turns; a downstream terminal wake never retains a view.
class LogActivity final : public LogWake {
public:
    // Own an optional enclosing downstream target; one armed latch coalesces work.
    explicit LogActivity(std::shared_ptr<const LogWake> downstream = {}) : downstream_(std::move(downstream)) {}
    void notify() const noexcept override {
        if (!pending_.exchange(true, std::memory_order_acq_rel) && downstream_) downstream_->notify();
    }
    // UI consumption disarms activity; pending inspection leaves it armed.
    bool consume() noexcept { return pending_.exchange(false, std::memory_order_acq_rel); }
    bool pending() const noexcept { return pending_.load(std::memory_order_acquire); }
private:
    mutable std::atomic<bool> pending_{false};
    std::shared_ptr<const LogWake> downstream_;
};
// Bytes belong to one stream; Record is grouped; End publishes lifecycle closure.
enum class LogMessageKind { Bytes, Record, End };
// Saturating known transport loss, with explicitly unknown byte-stream record count.
struct LogLoss {
    // Dropped batches/bytes/records, uncertainty, bounded merge and value equality.
    std::uint64_t batches = 0, bytes = 0, records = 0;
    bool records_unknown = false;
    bool empty() const noexcept { return batches == 0; }
    void merge(const LogLoss& other) noexcept;
    friend bool operator==(const LogLoss&, const LogLoss&) = default;
};

// Reusable caller-owned transfer. Only used bytes/metadata are copied; buffers
// are deliberately not cleared on every publication. Source/epoch/time are
// explicit data. Bytes has one logical producer stream; Record is already
// grouped. Payload setters preserve source/epoch/time. set_record inputs must
// not alias this object's metadata storage. copy_from handles self-copy.
class LogMessage {
public:
    // Reusable fixed storage; copying/moving rebases metadata into this packet.
    LogMessage() noexcept {}
    LogMessage(const LogMessage& other) noexcept { copy_from(other); }
    LogMessage& operator=(const LogMessage& other) noexcept { copy_from(other); return *this; }
    LogMessage(LogMessage&& other) noexcept { copy_from(other); }
    LogMessage& operator=(LogMessage&& other) noexcept { copy_from(other); return *this; }
    // Explicit identity/arrival, payload/lifecycle, native error, style and loss
    // preceding this position. Payload setters preserve source/epoch/time.
    LogSourceId source = 0;
    std::uint64_t epoch = 0, sequence = 0;
    std::int64_t arrival_nanos = 0;
    LogMessageKind kind = LogMessageKind::Bytes;
    LogSourceState state = LogSourceState::Following;
    std::uint32_t native_error = 0;
    Attr attrs = static_cast<Attr>(0);
    LogLoss loss_before;
    // Borrow used payload or adapter write capacity until the next mutation.
    std::span<const char> bytes() const noexcept { return {bytes_.data(), size_}; }
    std::span<char> writable_bytes() noexcept { return bytes_; }
    // Bound a Bytes payload prefix; invalid count changes nothing.
    bool set_byte_count(std::size_t count) noexcept;
    // Copy a bounded nonempty byte chunk.
    bool set_bytes(std::string_view bytes) noexcept;
    // Copy grouped bytes/metadata; inputs must not alias this packet's metadata.
    bool set_record(std::string_view raw, LogMetadata metadata = {}, Attr attributes = static_cast<Attr>(0)) noexcept;
    // Replace content with terminal lifecycle/native error without clearing capacity.
    void set_end(LogSourceState status, std::uint32_t error = 0) noexcept;
    // Borrow metadata and validate identity/content bounds.
    LogMetadata metadata() const noexcept { return {severity_, timestamp_, {fields_.data(), field_count_}}; }
    bool valid() const noexcept;
    // Copy only used bytes, rebasing metadata; self-copy is supported.
    void copy_from(const LogMessage& other) noexcept;
private:
    struct Range { std::size_t name = 0, name_size = 0, value = 0, value_size = 0; };
    void clear_metadata() noexcept;
    void resolve_fields() noexcept;
    std::array<char, kLogBatchBytes> bytes_;
    std::array<char, kLogMetadataBytes> metadata_;
    std::array<Range, kLogMaxFields> ranges_{};
    std::array<LogField, kLogMaxFields> fields_{};
    std::size_t size_ = 0, metadata_size_ = 0, field_count_ = 0;
    LogSeverity severity_ = LogSeverity::Unknown;
    std::optional<std::int64_t> timestamp_;
};

// Nonblocking publication/consumption outcomes; Busy never means acceptance.
enum class LogOfferResult { Accepted, WouldBlock, Busy, Dropped, Stale, Closed, Invalid, Exhausted };
enum class LogTakeResult { Taken, Empty, Busy };
// Fixed publication slots, payload bound and explicit full-ring policy.
struct LogMailboxOptions {
    // 1..64 slots, 1..64 KiB payload; valid checks compatible product limits.
    std::size_t batches = 4, batch_bytes = kLogBatchBytes;
    LogOverflowPolicy overflow = LogOverflowPolicy::DropOldest;
    bool valid() const noexcept;
};
// Source epoch, admission/retirement, current/high-water queue and known loss.
struct LogMailboxStatus {
    // Identity/counters, occupancy/high-water, explicit loss, oldest supplied
    // arrival, closure/armed-work flags and exact snapshot comparison.
    LogSourceId source = 0;
    std::uint64_t epoch = 0, accepted = 0, taken = 0, stale = 0, retired_bytes = 0;
    std::size_t queued_batches = 0, queued_bytes = 0, high_water_batches = 0, high_water_bytes = 0;
    LogLoss dropped;
    std::optional<std::int64_t> oldest_arrival_nanos;
    bool closed = false, pending = false;
    friend bool operator==(const LogMailboxStatus&, const LogMailboxStatus&) = default;
};

// Instance-owned bounded MPSC publication ring. Operations never wait for the
// lock, native I/O or user callbacks. Busy is explicit and leaves input intact;
// a producer retains at most its own declared staging packet before retrying.
// No mutation-time allocation. Wake targets outlive users of this mailbox;
// callbacks run outside the lock and never reference a view. One notification
// remains armed until acknowledge after a bounded drain. A true acknowledgement
// result means schedule another quantum, not post another callback per record.
class LogMailbox {
public:
    // Allocate the ring; shared work/capacity targets enclose all queue uses.
    LogMailbox(LogSourceId source, std::uint64_t epoch, LogMailboxOptions options = {},
               std::shared_ptr<const LogWake> work_wake = {}, std::shared_ptr<const LogWake> capacity_wake = {});
    LogMailbox(const LogMailbox&) = delete;
    LogMailbox& operator=(const LogMailbox&) = delete;
    // Try-publish; retain caller staging on Busy/WouldBlock.
    LogOfferResult offer(const LogMessage& message) noexcept;
    // Try-copy one used packet into caller-owned storage, rebasing its metadata.
    LogTakeResult take(LogMessage& result) noexcept;
    // Disarm after a bounded drain; true requests another scheduling quantum.
    bool acknowledge() noexcept;
    // Try-copy a coherent snapshot; false leaves result unchanged.
    bool status(LogMailboxStatus& result) const noexcept;
    // EOF/failure is remembered out of band and delivered after queued data and
    // trailing DropNewest loss. close/reset/cancel return false on contention.
    bool close(LogSourceState state, std::uint32_t error, std::int64_t arrival_nanos) noexcept;
    bool reset(LogSourceId source, std::uint64_t epoch) noexcept;
    bool cancel(std::int64_t arrival_nanos) noexcept;
    bool pending() const noexcept { return armed_.load(std::memory_order_acquire); }
    std::size_t allocated_bytes() const noexcept;
    const LogMailboxOptions& options() const noexcept { return options_; }
private:
    bool arm() noexcept;
    void notify_work(bool notify) const noexcept;
    void notify_capacity() const noexcept;
    LogMailboxOptions options_;
    std::vector<LogMessage> ring_;
    mutable std::mutex mutex_;
    LogMailboxStatus status_;
    std::size_t head_ = 0, count_ = 0;
    std::uint64_t next_sequence_ = 1;
    LogLoss tail_loss_;
    LogMessage end_;
    bool end_pending_ = false;
    std::atomic<bool> armed_{false};
    std::shared_ptr<const LogWake> work_wake_, capacity_wake_;
};
}  // namespace ckv
