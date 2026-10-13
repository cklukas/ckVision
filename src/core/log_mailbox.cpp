// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/log_mailbox.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ckv {
namespace {
void add(std::uint64_t& value, std::uint64_t count = 1) noexcept {
    value += std::min(count, std::numeric_limits<std::uint64_t>::max() - value);
}
LogLoss loss_of(const LogMessage& message) noexcept {
    return {1, message.bytes().size(), message.kind == LogMessageKind::Record ? 1U : 0U,
            message.kind == LogMessageKind::Bytes};
}
bool terminal_state(LogSourceState state) noexcept {
    return state == LogSourceState::Ended || state == LogSourceState::Failed || state == LogSourceState::Cancelled;
}
}
void LogLoss::merge(const LogLoss& other) noexcept {
    add(batches, other.batches); add(bytes, other.bytes); add(records, other.records);
    records_unknown = records_unknown || other.records_unknown;
}
void LogMessage::clear_metadata() noexcept {
    metadata_size_ = 0; field_count_ = 0; severity_ = LogSeverity::Unknown;
    timestamp_.reset(); attrs = static_cast<Attr>(0);
}
bool LogMessage::set_byte_count(std::size_t count) noexcept {
    if (count == 0 || count > bytes_.size()) return false;
    kind = LogMessageKind::Bytes; state = LogSourceState::Following; native_error = 0;
    size_ = count; clear_metadata(); loss_before = {};
    return true;
}
bool LogMessage::set_bytes(std::string_view bytes) noexcept {
    if (!set_byte_count(bytes.size())) return false;
    std::memmove(bytes_.data(), bytes.data(), bytes.size()); return true;
}
bool LogMessage::set_record(std::string_view raw, LogMetadata metadata, Attr attributes) noexcept {
    if (raw.size() > bytes_.size() || !metadata.valid()) return false;
    kind = LogMessageKind::Record; state = LogSourceState::Following; native_error = 0;
    size_ = raw.size(); clear_metadata(); loss_before = {};
    if (!raw.empty()) std::memmove(bytes_.data(), raw.data(), raw.size());
    severity_ = metadata.severity; timestamp_ = metadata.timestamp_nanos; attrs = attributes;
    field_count_ = metadata.fields.size();
    for (std::size_t i = 0; i < field_count_; ++i) {
        const auto& field = metadata.fields[i];
        ranges_[i] = {metadata_size_, field.name.size(), metadata_size_ + field.name.size(), field.value.size()};
        std::memcpy(metadata_.data() + metadata_size_, field.name.data(), field.name.size());
        metadata_size_ += field.name.size();
        if (!field.value.empty()) std::memcpy(metadata_.data() + metadata_size_, field.value.data(), field.value.size());
        metadata_size_ += field.value.size();
    }
    resolve_fields(); return true;
}
void LogMessage::set_end(LogSourceState status, std::uint32_t error) noexcept {
    kind = LogMessageKind::End; state = status; native_error = error;
    size_ = 0; clear_metadata(); loss_before = {};
}
void LogMessage::resolve_fields() noexcept {
    for (std::size_t i = 0; i < field_count_; ++i) {
        const auto& range = ranges_[i];
        fields_[i] = {{metadata_.data() + range.name, range.name_size}, {metadata_.data() + range.value, range.value_size}};
    }
}
bool LogMessage::valid() const noexcept {
    if (!source || !epoch || arrival_nanos < 0 || size_ > kLogBatchBytes) return false;
    switch (kind) {
        case LogMessageKind::Bytes: return size_ != 0 && field_count_ == 0 && state == LogSourceState::Following;
        case LogMessageKind::Record: return metadata().valid() && state == LogSourceState::Following;
        case LogMessageKind::End: return size_ == 0 && field_count_ == 0 && terminal_state(state);
    }
    return false;
}
void LogMessage::copy_from(const LogMessage& other) noexcept {
    if (&other == this) return;
    source = other.source; epoch = other.epoch; sequence = other.sequence; arrival_nanos = other.arrival_nanos;
    kind = other.kind; state = other.state; native_error = other.native_error; attrs = other.attrs; loss_before = other.loss_before;
    size_ = other.size_; metadata_size_ = other.metadata_size_; field_count_ = other.field_count_;
    severity_ = other.severity_; timestamp_ = other.timestamp_; ranges_ = other.ranges_;
    if (size_) std::memcpy(bytes_.data(), other.bytes_.data(), size_);
    if (metadata_size_) std::memcpy(metadata_.data(), other.metadata_.data(), metadata_size_);
    resolve_fields();
}
bool LogMailboxOptions::valid() const noexcept {
    return batches > 0 && batches <= 64 && batch_bytes > 0 && batch_bytes <= kLogBatchBytes &&
        static_cast<unsigned>(overflow) <= static_cast<unsigned>(LogOverflowPolicy::DropNewest);
}
LogMailbox::LogMailbox(LogSourceId source, std::uint64_t epoch, LogMailboxOptions options,
                       std::shared_ptr<const LogWake> work_wake, std::shared_ptr<const LogWake> capacity_wake)
    : options_(options), work_wake_(std::move(work_wake)), capacity_wake_(std::move(capacity_wake)) {
    if (!source || !epoch || !options.valid()) throw std::invalid_argument("invalid log mailbox");
    ring_.resize(options.batches); status_.source = source; status_.epoch = epoch;
}
bool LogMailbox::arm() noexcept { return !armed_.exchange(true, std::memory_order_acq_rel); }
void LogMailbox::notify_work(bool notify) const noexcept { if (notify && work_wake_) work_wake_->notify(); }
void LogMailbox::notify_capacity() const noexcept { if (capacity_wake_) capacity_wake_->notify(); }
LogOfferResult LogMailbox::offer(const LogMessage& message) noexcept {
    if (!message.valid() || message.kind == LogMessageKind::End || (!message.loss_before.empty() || message.loss_before.bytes || message.loss_before.records || message.loss_before.records_unknown) ||
        message.bytes().size() > options_.batch_bytes) return LogOfferResult::Invalid;
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return LogOfferResult::Busy;
    if (message.source != status_.source || message.epoch != status_.epoch) { add(status_.stale); return LogOfferResult::Stale; }
    if (status_.closed) return LogOfferResult::Closed;
    if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) return LogOfferResult::Exhausted;
    LogLoss prefix;
    if (count_ == ring_.size()) {
        if (options_.overflow == LogOverflowPolicy::Backpressure) return LogOfferResult::WouldBlock;
        if (options_.overflow == LogOverflowPolicy::DropNewest) {
            const auto loss = loss_of(message); status_.dropped.merge(loss); tail_loss_.merge(loss);
            return LogOfferResult::Dropped;
        }
        auto& oldest = ring_[head_];
        prefix = oldest.loss_before; const auto loss = loss_of(oldest); prefix.merge(loss); status_.dropped.merge(loss);
        status_.queued_bytes -= oldest.bytes().size();
        head_ = (head_ + 1) % ring_.size(); --count_;
        if (count_) { ring_[head_].loss_before.merge(prefix); prefix = {}; }
    }
    auto& slot = ring_[(head_ + count_) % ring_.size()];
    slot.copy_from(message); slot.sequence = next_sequence_++;
    slot.loss_before.merge(prefix); slot.loss_before.merge(tail_loss_); tail_loss_ = {};
    ++count_; add(status_.accepted); status_.queued_bytes += message.bytes().size();
    status_.high_water_batches = std::max(status_.high_water_batches, count_);
    status_.high_water_bytes = std::max(status_.high_water_bytes, status_.queued_bytes);
    const bool notify = arm(); lock.unlock(); notify_work(notify);
    return LogOfferResult::Accepted;
}
LogTakeResult LogMailbox::take(LogMessage& result) noexcept {
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return LogTakeResult::Busy;
    if (count_) {
        const bool capacity = count_ == ring_.size();
        result.copy_from(ring_[head_]); status_.queued_bytes -= result.bytes().size();
        head_ = (head_ + 1) % ring_.size(); --count_; add(status_.taken);
        lock.unlock(); if (capacity) notify_capacity(); return LogTakeResult::Taken;
    }
    if (end_pending_) { result.copy_from(end_); end_pending_ = false; add(status_.taken); return LogTakeResult::Taken; }
    return LogTakeResult::Empty;
}
bool LogMailbox::acknowledge() noexcept {
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return true;
    const bool pending = count_ || end_pending_;
    if (!pending) armed_.store(false, std::memory_order_release);
    return pending;
}
bool LogMailbox::status(LogMailboxStatus& result) const noexcept {
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    result = status_; result.queued_batches = count_; result.pending = count_ || end_pending_;
    result.oldest_arrival_nanos = count_ ? std::optional{ring_[head_].arrival_nanos}
        : end_pending_ ? std::optional{end_.arrival_nanos} : std::nullopt;
    return true;
}
bool LogMailbox::close(LogSourceState state, std::uint32_t error, std::int64_t arrival) noexcept {
    if (!terminal_state(state) || arrival < 0) return false;
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    if (status_.closed) return true;
    status_.closed = true; end_.source = status_.source; end_.epoch = status_.epoch;
    end_.sequence = next_sequence_; end_.arrival_nanos = arrival; end_.set_end(state, error);
    end_.loss_before = tail_loss_; tail_loss_ = {}; end_pending_ = true;
    const bool notify = arm(); lock.unlock(); notify_work(notify); notify_capacity(); return true;
}
bool LogMailbox::reset(LogSourceId source, std::uint64_t epoch) noexcept {
    if (!source || !epoch) return false;
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    if (source == status_.source && epoch <= status_.epoch) return false;
    const auto retired = status_.retired_bytes;
    status_ = {}; status_.retired_bytes = retired;
    for (std::size_t i = 0; i < count_; ++i) add(status_.retired_bytes, ring_[(head_ + i) % ring_.size()].bytes().size());
    head_ = 0; count_ = 0; next_sequence_ = 1; tail_loss_ = {}; end_pending_ = false;
    status_.source = source; status_.epoch = epoch;
    const bool notify = arm();
    lock.unlock(); notify_work(notify); notify_capacity(); return true;
}
bool LogMailbox::cancel(std::int64_t arrival) noexcept {
    if (arrival < 0) return false;
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    if (status_.closed && end_.state == LogSourceState::Cancelled) return true;
    add(status_.retired_bytes, status_.queued_bytes);
    head_ = 0; count_ = 0; status_.queued_bytes = 0; tail_loss_ = {}; status_.closed = true;
    end_.source = status_.source; end_.epoch = status_.epoch; end_.arrival_nanos = arrival;
    end_.sequence = next_sequence_; end_.set_end(LogSourceState::Cancelled); end_pending_ = true;
    const bool notify = arm(); lock.unlock(); notify_work(notify); notify_capacity(); return true;
}
std::size_t LogMailbox::allocated_bytes() const noexcept {
    return sizeof(*this) + ring_.capacity() * sizeof(LogMessage);
}
}  // namespace ckv
