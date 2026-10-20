// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "fuzz_common.hpp"
#include "cvision/widgets/log_controller.hpp"
#include "cvision/core/utf8.hpp"
#include <algorithm>
namespace {
ckv::LogLimits limits() { ckv::LogLimits value; value.records = 32; value.record_bytes = 512; value.retained_bytes = 32 * 1024; return value; }
void invariant(const ckv::widgets::LogModel& model) {
    ckv::fuzz::require(model.size() <= model.limits().records && model.retained_payload_bytes() <= model.limits().retained_bytes);
    std::uint64_t sequence = 0;
    for (std::size_t index = 0; index < model.size(); ++index) {
        const auto record = model.at(index); ckv::fuzz::require(record.has_value());
        ckv::fuzz::require(record->id.sequence > sequence && record->raw.size() <= model.limits().record_bytes && ckv::utf8::is_valid(record->text));
        ckv::fuzz::require(!ckv::fuzz::contains_terminal_control(record->text)); sequence = record->id.sequence;
    }
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace ckv; using namespace ckv::widgets;
    size = std::min(size, kLogBatchBytes); const auto text = std::string_view(reinterpret_cast<const char*>(data), size);
    const auto carriage = size && (data[0] & 1U) ? LogCarriageReturn::Progress : LogCarriageReturn::LineBreak;
    LogModel whole(limits(), carriage), divided(limits(), carriage); whole.begin_source(1, 1); divided.begin_source(1, 1);
    fuzz::require(whole.append_bytes(text));
    for (std::size_t position = 0; position < size;) {
        const auto count = std::min<std::size_t>(1 + data[position] % 31, size - position);
        fuzz::require(divided.append_bytes(text.substr(position, count))); position += count;
    }
    whole.finish(); divided.finish(); invariant(whole); invariant(divided); fuzz::require(whole.size() == divided.size());
    for (std::size_t index = 0; index < whole.size(); ++index) {
        const auto first = whole.at(index), second = divided.at(index);
        fuzz::require(first->id == second->id && first->raw == second->raw && first->text == second->text && first->discarded_bytes == second->discarded_bytes && first->incomplete == second->incomplete);
    }
    LogFilter filter(whole, {3, 17}); LogSearch search(filter, {2, 11});
    LogMailbox queue(2, 1, {2, 1024, LogOverflowPolicy::DropOldest}); LogStreamReceiver receiver(whole, queue, {31, 3});
    LogMessage message; message.source = 2; message.epoch = 1;
    for (std::size_t index = 0; index < std::min<std::size_t>(size, 128); ++index) {
        const auto value = text.substr(index, std::min<std::size_t>(size - index, data[index] % 32));
        switch (data[index] % 7) {
            case 0: filter.set_include(value); break;
            case 1: { const std::array fields{LogField{"field", value}}; message.set_record(value, {LogSeverity::Info, {}, fields}); queue.offer(message); break; }
            case 2: receiver.step(static_cast<std::int64_t>(index)); break;
            case 3: whole.clear(); break;
            case 4: search.set_query(value, (data[index] & 1U) != 0); search.request(LogSearchDirection::Next); break;
            case 5: search.step(); search.take_result(); break;
            default: whole.append_record(value); break;
        }
        filter.step(); fuzz::require(filter.status().last_step_records <= 3 && filter.status().last_step_comparisons <= 17);
        fuzz::require(search.status().last_step_records <= 2 && search.status().last_step_comparisons <= 11); invariant(whole);
    }
    return 0;
}
