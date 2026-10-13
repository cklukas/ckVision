// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/log_mailbox.hpp"
#include "cvision/widgets/log_stream_receiver.hpp"
#include "cvision/testing/cktest.hpp"

#include <atomic>
#include <thread>

using namespace ckv;
using namespace ckv::widgets;
namespace {
LogLimits delivery_limits(std::size_t count = 16) {
    LogLimits limits; limits.records = count; limits.record_bytes = 128;
    limits.retained_bytes = kLogMetadataBytes + count * 512;
    return limits;
}
void bytes(LogMessage& message, std::string_view value, std::uint64_t epoch = 1) {
    message.source = 1; message.epoch = epoch; CK_CHECK(message.set_bytes(value));
}
struct Wake final : LogWake {
    mutable std::atomic<std::size_t> count{0};
    void notify() const noexcept override { count.fetch_add(1, std::memory_order_relaxed); }
};
void drain(LogStreamReceiver& receiver, std::int64_t& time) {
    std::size_t steps = 0;
    do { receiver.step(time++); CK_CHECK(++steps < 10000); } while (receiver.pending());
}
}
CK_TEST(log_message_metadata_copy_and_move_own_their_transfer_storage) {
    LogMessage message; message.source = 1; message.epoch = 1;
    const std::array fields{LogField{"unit", "worker"}};
    CK_CHECK(message.set_record("hello\nworld", {LogSeverity::Error, 123, fields}, Attr::Bold));
    LogMessage copy(message); LogMessage moved(std::move(copy));
    message.set_record("changed");
    CK_CHECK(moved.metadata().fields[0].value == "worker" && moved.metadata().severity == LogSeverity::Error);
    CK_CHECK(std::string_view(moved.bytes().data(), moved.bytes().size()) == "hello\nworld");
    CK_CHECK(moved.attrs == Attr::Bold && moved.valid());
}
CK_TEST(log_mailbox_work_and_activity_wakes_coalesce_until_acknowledged) {
    auto wake = std::make_shared<Wake>(); auto activity = std::make_shared<LogActivity>(wake);
    LogMailbox mailbox(1, 1, {4, 8, LogOverflowPolicy::DropOldest}, activity);
    LogMessage message, result; bytes(message, "one");
    for (int i = 0; i < 4; ++i) CK_CHECK(mailbox.offer(message) == LogOfferResult::Accepted);
    CK_CHECK(wake->count == 1 && activity->pending() && activity->consume());
    CK_CHECK(mailbox.take(result) == LogTakeResult::Taken && mailbox.acknowledge());
    for (int i = 0; i < 3; ++i) CK_CHECK(mailbox.take(result) == LogTakeResult::Taken);
    CK_CHECK(!mailbox.acknowledge());
    CK_CHECK(mailbox.offer(message) == LogOfferResult::Accepted && wake->count == 2);
}
CK_TEST(log_mailbox_drop_oldest_preserves_exact_loss_position_and_counters) {
    LogMailbox mailbox(1, 1, {2, 8, LogOverflowPolicy::DropOldest}); LogMessage message, result;
    for (const auto value : {"a\n", "b\n", "c\n", "d\n"}) { bytes(message, value); CK_CHECK(mailbox.offer(message) == LogOfferResult::Accepted); }
    CK_CHECK(mailbox.take(result) == LogTakeResult::Taken);
    CK_CHECK(std::string_view(result.bytes().data(), result.bytes().size()) == "c\n");
    CK_CHECK(result.loss_before.batches == 2 && result.loss_before.bytes == 4 && result.loss_before.records_unknown);
    CK_CHECK(mailbox.take(result) == LogTakeResult::Taken && result.loss_before.empty());
    LogMailboxStatus status; CK_CHECK(mailbox.status(status));
    CK_CHECK(status.dropped.batches == 2 && status.dropped.bytes == 4 && status.high_water_batches == 2);
}
CK_TEST(log_mailbox_drop_newest_reports_trailing_loss_at_end_not_before_old_data) {
    LogMailbox mailbox(1, 1, {1, 8, LogOverflowPolicy::DropNewest}); LogMessage message, result;
    message.source = 1; message.epoch = 1; message.set_record("first");
    CK_CHECK(mailbox.offer(message) == LogOfferResult::Accepted);
    message.set_record("lost"); CK_CHECK(mailbox.offer(message) == LogOfferResult::Dropped);
    CK_CHECK(mailbox.close(LogSourceState::Ended, 0, 9));
    CK_CHECK(mailbox.take(result) == LogTakeResult::Taken && result.kind == LogMessageKind::Record && result.loss_before.empty());
    CK_CHECK(mailbox.take(result) == LogTakeResult::Taken && result.kind == LogMessageKind::End);
    CK_CHECK(result.loss_before.records == 1 && result.loss_before.bytes == 4 && !result.loss_before.records_unknown);
    CK_CHECK(!mailbox.acknowledge() && mailbox.offer(message) == LogOfferResult::Closed);
}
CK_TEST(log_mailbox_backpressure_preserves_input_and_signals_capacity) {
    auto capacity = std::make_shared<Wake>(); LogMailbox mailbox(1, 1, {1, 8, LogOverflowPolicy::Backpressure}, {}, capacity);
    LogMessage message, result; bytes(message, "first"); CK_CHECK(mailbox.offer(message) == LogOfferResult::Accepted);
    bytes(message, "second"); CK_CHECK(mailbox.offer(message) == LogOfferResult::WouldBlock);
    CK_CHECK(mailbox.take(result) == LogTakeResult::Taken && capacity->count == 1);
    CK_CHECK(mailbox.offer(message) == LogOfferResult::Accepted);
    LogMailboxStatus status; mailbox.status(status); CK_CHECK(status.dropped.empty());
}
CK_TEST(log_mailbox_reset_retires_old_epochs_without_duplicate_pending_notifications) {
    auto wake = std::make_shared<Wake>(); LogMailbox mailbox(1, 1, {}, wake);
    LogMessage message, result; bytes(message, "old"); mailbox.offer(message);
    CK_CHECK(mailbox.reset(1, 2) && !mailbox.reset(1, 1));
    CK_CHECK(mailbox.offer(message) == LogOfferResult::Stale);
    bytes(message, "new", 2); CK_CHECK(mailbox.offer(message) == LogOfferResult::Accepted);
    CK_CHECK(wake->count == 1 && mailbox.take(result) == LogTakeResult::Taken && result.epoch == 2);
    LogMailboxStatus status; mailbox.status(status); CK_CHECK(status.retired_bytes == 3 && status.stale == 1);
}
CK_TEST(log_mailbox_concurrent_producers_and_consumer_remain_ordered_and_bounded) {
    LogMailbox mailbox(1, 1, {4, 16, LogOverflowPolicy::Backpressure});
    std::atomic<unsigned> finished{0}; std::atomic<bool> valid{true};
    std::array<std::thread, 4> producers;
    for (auto& producer : producers) producer = std::thread([&] {
        LogMessage message; message.source = 1; message.epoch = 1; message.set_record("record");
        for (int i = 0; i < 200; ++i) {
            for (;;) {
                const auto result = mailbox.offer(message);
                if (result == LogOfferResult::Accepted) break;
                if (result != LogOfferResult::Busy && result != LogOfferResult::WouldBlock) valid = false;
                std::this_thread::yield();
            }
        }
        ++finished;
    });
    LogMessage result; std::uint64_t sequence = 0;
    while (finished != 4 || mailbox.pending()) {
        const auto taken = mailbox.take(result);
        if (taken == LogTakeResult::Taken) { if (result.sequence != sequence + 1) valid = false; sequence = result.sequence; }
        mailbox.acknowledge(); std::this_thread::yield();
    }
    for (auto& producer : producers) producer.join();
    LogMailboxStatus status; mailbox.status(status);
    CK_CHECK(valid && sequence == 800 && status.accepted == 800 && status.dropped.empty());
    CK_CHECK(status.high_water_batches <= 4 && status.high_water_bytes <= 64);
}
CK_TEST(log_model_record_quota_preserves_crlf_bom_and_unconsumed_suffix) {
    LogModel model(delivery_limits()); model.begin_source(1, 1);
    const auto bom = model.append_bytes_bounded("\xEF\xBB\xBF", 0); CK_CHECK(bom.accepted && bom.bytes == 3 && bom.records == 0);
    const auto first = model.append_bytes_bounded("a\r\nb\n", 1);
    CK_CHECK(first.bytes == 3 && first.records == 1 && model.size() == 1);
    const auto second = model.append_bytes_bounded("b\n", 1); CK_CHECK(second.bytes == 2 && second.records == 1 && model.size() == 2);
    const auto stopped = model.append_bytes_bounded("next", 0); CK_CHECK(stopped.accepted && stopped.bytes == 0);
}
CK_TEST(log_model_incremental_explicit_record_keeps_metadata_identity_and_multiline) {
    LogModel model(delivery_limits()); model.begin_source(1, 1);
    const std::array fields{LogField{"unit", "demo"}};
    CK_CHECK(model.begin_record({LogSeverity::Error, 100, fields}, Attr::Bold));
    const auto id = model.at(0)->id;
    CK_CHECK(model.append_record_bytes("one\n\xE4") && !model.append_bytes("wrong"));
    CK_CHECK(model.append_record_bytes("\xB8\xAD") && model.end_record());
    const auto record = model.at(0);
    CK_CHECK(record->id == id && record->text == "one\n中" && record->fields[0].value == "demo" && record->attrs == Attr::Bold);
    CK_CHECK(!record->partial && !model.end_record());
}
CK_TEST(log_receiver_byte_and_record_quanta_bound_dense_streams) {
    LogMailbox mailbox(1, 1); LogModel model(delivery_limits(64)); LogStreamReceiver receiver(model, mailbox, {32, 3});
    LogMessage message; bytes(message, "a\na\na\na\na\na\na\na\n"); mailbox.offer(message);
    std::int64_t now = 0;
    do { receiver.step(now++); CK_CHECK(receiver.status().last_step_records <= 3 && receiver.status().last_step_bytes <= 32); } while (receiver.pending());
    CK_CHECK(model.size() == 8 && receiver.status().bytes_delivered == 16);
}
CK_TEST(log_receiver_structured_record_is_decoded_across_byte_quanta) {
    LogMailbox mailbox(1, 1); LogModel model(delivery_limits()); LogStreamReceiver receiver(model, mailbox, {3, 1});
    LogMessage message; message.source = 1; message.epoch = 1;
    const std::array fields{LogField{"unit", "demo"}};
    message.set_record("one\n中two", {LogSeverity::Error, {}, fields}, Attr::Bold); mailbox.offer(message);
    receiver.step(0); const auto id = model.at(0)->id;
    CK_CHECK(model.at(0)->partial && model.at(0)->fields[0].value == "demo");
    std::int64_t now = 1; drain(receiver, now);
    CK_CHECK(model.size() == 1 && model.at(0)->id == id && model.at(0)->text == "one\n中two" && !model.at(0)->partial);
}
CK_TEST(log_receiver_byte_loss_never_splices_fragments_and_marks_generation_gap) {
    LogMailbox mailbox(1, 1, {1, 64, LogOverflowPolicy::DropOldest}); LogModel model(delivery_limits());
    LogStreamReceiver receiver(model, mailbox, {64, 8}); LogMessage message;
    bytes(message, "prefix"); mailbox.offer(message); receiver.step(0);
    const auto generation = model.generation();
    bytes(message, "lost"); mailbox.offer(message);
    bytes(message, "fragment\r"); mailbox.offer(message);
    std::int64_t now = 1; drain(receiver, now);
    bytes(message, "\nreal\n"); mailbox.offer(message); drain(receiver, now);
    CK_CHECK(model.generation() > generation && receiver.status().gap_markers == 1);
    CK_CHECK(model.at(0)->text == "prefix" && model.at(0)->incomplete);
    CK_CHECK(model.at(1)->text.starts_with("[gap:") && model.at(2)->text == "real");
    CK_CHECK(receiver.status().resync_bytes == 10);
}
CK_TEST(log_receiver_trailing_drop_newest_loss_survives_eof_with_exact_record_count) {
    LogMailbox mailbox(1, 1, {1, 64, LogOverflowPolicy::DropNewest}); LogModel model(delivery_limits()); LogStreamReceiver receiver(model, mailbox);
    LogMessage message; message.source = 1; message.epoch = 1; message.set_record("kept"); mailbox.offer(message);
    message.set_record("lost"); mailbox.offer(message); mailbox.close(LogSourceState::Ended, 0, 1);
    std::int64_t now = 0; drain(receiver, now);
    CK_CHECK(receiver.status().state == LogSourceState::Ended && model.size() == 2);
    CK_CHECK(model.at(1)->text.find("1 records") != std::string_view::npos && !receiver.status().mailbox.dropped.records_unknown);
}
CK_TEST(log_receiver_source_replacement_retires_a_held_explicit_record) {
    LogMailbox mailbox(1, 1); LogModel model(delivery_limits()); LogStreamReceiver receiver(model, mailbox, {2, 1});
    LogMessage message; message.source = 1; message.epoch = 1; message.set_record("old content"); mailbox.offer(message); receiver.step(0);
    mailbox.reset(1, 2); message.epoch = 2; message.set_record("new"); mailbox.offer(message);
    std::int64_t now = 1; drain(receiver, now);
    CK_CHECK(model.at(0)->text == "ol" && model.at(0)->incomplete && model.at(1)->text == "new");
    CK_CHECK(receiver.status().retired_bytes == 9);
}
