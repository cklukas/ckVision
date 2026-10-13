// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_stream_receiver.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>

namespace ckv::widgets {
namespace {
void add_receive(std::uint64_t& value, std::uint64_t count = 1) noexcept {
    value += std::min(count, std::numeric_limits<std::uint64_t>::max() - value);
}
}
LogStreamReceiver::LogStreamReceiver(LogModel& model, LogMailbox& mailbox, LogReceiveBudget budget)
    : model_(model), mailbox_(mailbox), budget_(budget) {
    if (!budget.bytes || budget.bytes > model.limits().batch_bytes || !budget.records || budget.records > 256)
        throw std::invalid_argument("invalid log receive budget");
    update(0);
}
void LogStreamReceiver::fail() noexcept {
    if (held_) add_receive(status_.retired_bytes, message_.bytes().size() - position_);
    held_ = false; record_started_ = false; position_ = 0;
    status_.state = LogSourceState::Failed; stopped_ = true; closing_ = true;
    model_.finish(); add_receive(status_.revision);
}
bool LogStreamReceiver::generation(LogSourceId source) noexcept {
    if (source == model_.source() && model_.generation() == std::numeric_limits<std::uint64_t>::max()) {
        fail(); return false;
    }
    const auto next = source == model_.source() ? model_.generation() + 1 : 1;
    if (!model_.begin_source(source, next)) { fail(); return false; }
    status_.source = source; status_.generation = next;
    return true;
}
bool LogStreamReceiver::gap(std::size_t& records) noexcept {
    if (message_.loss_before.empty()) return true;
    if (!records || !generation(message_.source)) return false;
    std::array<char, 256> bytes{};
    std::size_t used = 0;
    const auto append = [&](std::string_view value) {
        const auto count = std::min(value.size(), bytes.size() - used);
        std::copy_n(value.data(), count, bytes.data() + used); used += count;
    };
    const auto number = [&](std::uint64_t value) {
        const auto result = std::to_chars(bytes.data() + used, bytes.data() + bytes.size(), value);
        used = static_cast<std::size_t>(result.ptr - bytes.data());
    };
    append("[gap: dropped "); number(message_.loss_before.batches); append(" batches, ");
    number(message_.loss_before.bytes); append(" B, ");
    if (message_.loss_before.records_unknown) append("record count unknown");
    else { number(message_.loss_before.records); append(" records"); }
    append("]");
    if (!model_.append_record({bytes.data(), used}, {LogSeverity::Warning, {}, {}}, Attr::Bold)) return false;
    skip_prefix_ = message_.loss_before.records_unknown; skip_lf_ = false;
    message_.loss_before = {}; --records; ++status_.last_step_records;
    add_receive(status_.records_started); add_receive(status_.gap_markers);
    return true;
}
void LogStreamReceiver::update(std::int64_t now) noexcept {
    LogMailboxStatus mailbox;
    if (mailbox_.status(mailbox)) status_.mailbox = mailbox;
    status_.held_bytes = held_ ? message_.bytes().size() - position_ : 0;
    status_.pending = pending();
    const auto oldest = held_ ? std::optional{message_.arrival_nanos} : status_.mailbox.oldest_arrival_nanos;
    status_.oldest_lag_nanos = oldest && now >= *oldest ? now - *oldest : 0;
}
bool LogStreamReceiver::observe(std::int64_t now) noexcept {
    if (now < last_now_ || now < 0) return false;
    const auto before = status_.mailbox;
    const auto lag = status_.oldest_lag_nanos;
    update(now);
    const bool changed = before != status_.mailbox || lag != status_.oldest_lag_nanos;
    if (changed) add_receive(status_.revision);
    return changed;
}
bool LogStreamReceiver::step(std::int64_t now) noexcept {
    if (now < last_now_ || now < 0) return false;
    last_now_ = now; status_.last_step_bytes = 0; status_.last_step_records = 0;
    const auto before_model = model_.revision();
    const auto before_epoch = status_.epoch;
    LogMailboxStatus mailbox;
    if (!mailbox_.status(mailbox)) { update(now); return false; }
    if (held_ && (message_.source != mailbox.source || message_.epoch != mailbox.epoch)) {
        add_receive(status_.retired_bytes, message_.bytes().size() - position_);
        held_ = false; record_started_ = false; position_ = 0;
    }
    if (stopped_) {
        if (closing_ && mailbox_.close(LogSourceState::Failed, status_.native_error, now)) closing_ = false;
        const auto taken = mailbox_.take(message_);
        if (taken == LogTakeResult::Taken && message_.kind != LogMessageKind::End) add_receive(status_.retired_bytes, message_.bytes().size());
        if (taken != LogTakeResult::Busy) mailbox_.acknowledge();
        update(now); return false;
    }
    if (status_.source != mailbox.source || status_.epoch != mailbox.epoch) {
        if (!generation(mailbox.source)) { update(now); return false; }
        status_.epoch = mailbox.epoch; status_.state = LogSourceState::Following;
        skip_prefix_ = false; skip_lf_ = false;
    }
    if (!held_) {
        const auto result = mailbox_.take(message_);
        if (result != LogTakeResult::Taken) {
            if (result == LogTakeResult::Empty) mailbox_.acknowledge();
            update(now); return before_epoch != status_.epoch;
        }
        held_ = true; position_ = 0; record_started_ = false;
    }
    auto records = budget_.records;
    if (!gap(records)) { update(now); return model_.revision() != before_model; }
    if (message_.kind == LogMessageKind::End) {
        model_.finish(); held_ = false; record_started_ = false;
        status_.state = message_.state; status_.native_error = message_.native_error;
    } else if (message_.kind == LogMessageKind::Record) {
        skip_prefix_ = false; skip_lf_ = false;
        if (!record_started_) {
            if (!records) { update(now); return model_.revision() != before_model; }
            if (!model_.begin_record(message_.metadata(), message_.attrs)) { fail(); update(now); return false; }
            record_started_ = true; --records; ++status_.last_step_records; add_receive(status_.records_started);
        }
        const auto count = std::min(budget_.bytes, message_.bytes().size() - position_);
        const std::string_view value(message_.bytes().data() + position_, count);
        if (!model_.append_record_bytes(value)) { fail(); update(now); return false; }
        position_ += count; status_.last_step_bytes = count; add_receive(status_.bytes_delivered, count);
        if (position_ == message_.bytes().size()) { model_.end_record(); held_ = false; record_started_ = false; }
    } else {
        const auto count = std::min(budget_.bytes, message_.bytes().size() - position_);
        std::size_t skipped = 0;
        if (skip_lf_ && count) { skip_lf_ = false; if (message_.bytes()[position_] == '\n') ++skipped; }
        while (skip_prefix_ && skipped < count) {
            const char byte = message_.bytes()[position_ + skipped++];
            if (byte == '\n' || byte == '\r') { skip_prefix_ = false; skip_lf_ = byte == '\r'; }
        }
        if (!skip_prefix_ && skip_lf_ && skipped < count) { skip_lf_ = false; if (message_.bytes()[position_ + skipped] == '\n') ++skipped; }
        position_ += skipped; add_receive(status_.resync_bytes, skipped);
        const auto available = count - skipped;
        const auto appended = model_.append_bytes_bounded({message_.bytes().data() + position_, available}, records);
        if (!appended.accepted) { fail(); update(now); return false; }
        position_ += appended.bytes;
        status_.last_step_bytes = skipped + appended.bytes; status_.last_step_records += appended.records;
        add_receive(status_.bytes_delivered, appended.bytes); add_receive(status_.records_started, appended.records);
        if (position_ == message_.bytes().size()) held_ = false;
    }
    mailbox_.acknowledge();
    update(now);
    const bool changed = model_.revision() != before_model || before_epoch != status_.epoch;
    if (changed || message_.kind == LogMessageKind::End) add_receive(status_.revision);
    return changed || message_.kind == LogMessageKind::End;
}
bool LogStreamReceiver::stop(std::int64_t now) noexcept {
    if (now < last_now_ || now < 0 || !mailbox_.cancel(now)) return false;
    if (held_) add_receive(status_.retired_bytes, message_.bytes().size() - position_);
    model_.finish(); held_ = false; record_started_ = false; stopped_ = true;
    status_.state = LogSourceState::Cancelled; closing_ = false; last_now_ = now;
    add_receive(status_.revision);
    mailbox_.take(message_); mailbox_.acknowledge(); update(now); return true;
}
}  // namespace ckv::widgets
