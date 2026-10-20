// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_model.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "cvision/core/utf8.hpp"
#include "cvision/testing/cktest.hpp"

namespace {
ckv::LogLimits small_limits(std::size_t records = 64, std::size_t record_bytes = 128) {
    ckv::LogLimits limits;
    limits.records = records;
    limits.record_bytes = record_bytes;
    limits.retained_bytes = record_bytes * 4 + ckv::kLogMetadataBytes;
    return limits;
}

struct SavedRecord {
    ckv::LogRecordId id;
    std::string raw, text;
    bool incomplete;
    std::uint64_t discarded;
    friend bool operator==(const SavedRecord&, const SavedRecord&) = default;
};
std::vector<SavedRecord> snapshot(const ckv::widgets::LogModel& model) {
    std::vector<SavedRecord> records;
    for (std::size_t i = 0; i < model.size(); ++i) {
        const auto record = model.at(i);
        if (record) records.push_back({record->id, std::string(record->raw), std::string(record->text),
                                      record->incomplete, record->discarded_bytes});
    }
    return records;
}
}

CK_TEST(log_model_golden_records_cover_empty_control_and_unfinished_lines) {
    ckv::widgets::LogModel model(small_limits());
    CK_CHECK(model.begin_source(6, 4));
    CK_CHECK(model.append_bytes("A\r\n\n\x1B[31mred\nfinal\xE4"));
    model.finish();
    const std::vector<SavedRecord> golden{
        {{6, 4, 1}, "A", "A", false, 0},
        {{6, 4, 2}, "", "", false, 0},
        {{6, 4, 3}, "\x1B[31mred", "\xEF\xBF\xBD[31mred", false, 0},
        {{6, 4, 4}, "final\xE4", "final\xEF\xBF\xBD", true, 0}
    };
    CK_CHECK(snapshot(model) == golden);
}

CK_TEST(log_model_rejects_invalid_limits_and_admission_without_mutation) {
    auto limits = small_limits();
    limits.records = 0;
    bool rejected = false;
    try { ckv::widgets::LogModel invalid(limits); }
    catch (const std::invalid_argument&) { rejected = true; }
    CK_CHECK(rejected);
    ckv::widgets::LogModel model(small_limits());
    CK_CHECK(!model.append_bytes("text"));
    CK_CHECK(!model.begin_source(0, 1));
    CK_CHECK(!model.begin_source(1, 0));
    CK_CHECK(model.begin_source(1, 1));
    const auto revision = model.stats().revision;
    const std::string too_big(ckv::kLogBatchBytes + 1, 'x');
    CK_CHECK(!model.append_bytes(too_big));
    CK_CHECK(model.size() == 0);
    CK_CHECK(model.stats().revision == revision);
    CK_CHECK(!model.at(0));
}

CK_TEST(log_model_chunk_boundaries_preserve_unicode_controls_bom_crlf_and_identity) {
    std::string input = "\xEF\xBB\xBF" "first\r\n\n\xE4\xB8\xAD e\xCC\x81 \xF0\x9F\x98\x80\n";
    input += std::string("control\0\x1B\xC2\x9C\n", 12);
    input += "bad\xE0\x80\xFF\nlast\xE4\xB8";
    ckv::widgets::LogModel complete(small_limits());
    CK_CHECK(complete.begin_source(7, 2));
    CK_CHECK(complete.append_bytes(input));
    complete.finish();
    const auto expected = snapshot(complete);
    for (std::size_t split = 0; split <= input.size(); ++split) {
        ckv::widgets::LogModel divided(small_limits());
        CK_CHECK(divided.begin_source(7, 2));
        CK_CHECK(divided.append_bytes(std::string_view(input).substr(0, split)));
        CK_CHECK(divided.append_bytes(std::string_view(input).substr(split)));
        divided.finish();
        CK_CHECK(snapshot(divided) == expected);
    }
    ckv::widgets::LogModel single_bytes(small_limits());
    CK_CHECK(single_bytes.begin_source(7, 2));
    for (const auto& byte : input) CK_CHECK(single_bytes.append_bytes({&byte, 1}));
    single_bytes.finish();
    CK_CHECK(snapshot(single_bytes) == expected);
    CK_CHECK(expected.front().raw == "first");
    for (std::size_t i = 0; i < complete.size(); ++i) CK_CHECK(ckv::utf8::is_valid(complete.at(i)->text));
}

CK_TEST(log_model_partial_record_keeps_id_through_scalar_completion_and_commit) {
    ckv::widgets::LogModel model(small_limits());
    CK_CHECK(model.begin_source(1, 1));
    CK_CHECK(model.append_bytes("a\xE4"));
    CK_CHECK(model.size() == 1);
    const auto id = model.at(0)->id;
    CK_CHECK(model.at(0)->partial);
    CK_CHECK(model.at(0)->text == "a");
    CK_CHECK(model.append_bytes("\xB8\xAD\n"));
    CK_CHECK(model.size() == 1);
    CK_CHECK(model.at(0)->id == id);
    CK_CHECK(!model.at(0)->partial);
    CK_CHECK(model.at(0)->text == "a\xE4\xB8\xAD");
    CK_CHECK(model.index_of(id) == 0);
}

CK_TEST(log_model_eof_resolves_incomplete_utf8_bom_and_final_line) {
    ckv::widgets::LogModel model(small_limits());
    CK_CHECK(model.begin_source(1, 1));
    CK_CHECK(model.append_bytes("\xEF\xBB"));
    CK_CHECK(model.size() == 0);
    model.finish();
    CK_CHECK(model.size() == 1);
    CK_CHECK(model.at(0)->raw == "\xEF\xBB");
    CK_CHECK(model.at(0)->text == "\xEF\xBF\xBD\xEF\xBF\xBD");
    CK_CHECK(model.at(0)->incomplete);
    model.finish();
    CK_CHECK(model.size() == 1);
}

CK_TEST(log_model_explicit_nonzero_stream_start_does_not_consume_literal_bom) {
    ckv::widgets::LogModel model(small_limits());
    CK_CHECK(model.begin_source(1, 1, false));
    CK_CHECK(model.append_bytes("\xEF\xBB\xBF" "literal\n"));
    CK_CHECK(model.at(0)->raw == "\xEF\xBB\xBF" "literal");
    CK_CHECK(model.source() == 1); CK_CHECK(model.generation() == 1);
}

CK_TEST(log_model_byte_exact_raw_is_separate_from_safe_display) {
    ckv::widgets::LogModel model(small_limits());
    CK_CHECK(model.begin_source(1, 1));
    const std::string raw("a\0\x1B\x7F\xC2\x9C\xFF", 7);
    CK_CHECK(model.append_record(raw));
    const auto record = model.at(0);
    CK_CHECK(record->raw == raw);
    CK_CHECK(record->text == "a\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");
}

CK_TEST(log_model_carriage_return_policies_are_explicit_and_crlf_is_one_boundary) {
    ckv::widgets::LogModel lines(small_limits());
    CK_CHECK(lines.begin_source(1, 1));
    CK_CHECK(lines.append_bytes("one\rtwo\r"));
    CK_CHECK(lines.append_bytes("\nthree\n"));
    CK_CHECK(lines.size() == 3);
    CK_CHECK(lines.at(0)->text == "one");
    CK_CHECK(lines.at(1)->text == "two");
    CK_CHECK(lines.at(2)->text == "three");

    ckv::widgets::LogModel progress(small_limits(), ckv::widgets::LogCarriageReturn::Progress);
    CK_CHECK(progress.begin_source(1, 1));
    CK_CHECK(progress.append_bytes("10%\r"));
    const auto id = progress.at(0)->id;
    CK_CHECK(progress.append_bytes("20%\r"));
    CK_CHECK(progress.at(0)->id == id);
    CK_CHECK(progress.at(0)->text == "20%");
    CK_CHECK(progress.append_bytes("\n"));
    CK_CHECK(progress.size() == 1);
    CK_CHECK(progress.at(0)->id == id);
    CK_CHECK(!progress.at(0)->partial);
}

CK_TEST(log_model_oversized_line_drains_and_counts_without_splitting_into_fake_records) {
    ckv::widgets::LogModel model(small_limits(64, 4));
    CK_CHECK(model.begin_source(1, 1));
    CK_CHECK(model.append_bytes("abcdefghij\nnext\n"));
    CK_CHECK(model.size() == 2);
    CK_CHECK(model.at(0)->raw == "abcd");
    CK_CHECK(model.at(0)->discarded_bytes == 6);
    CK_CHECK(model.at(1)->raw == "next");
    CK_CHECK(model.stats().discarded_bytes == 6);
}

CK_TEST(log_model_record_ring_evicts_old_ids_and_counts_partial_record_in_limit) {
    ckv::widgets::LogModel model(small_limits(3));
    CK_CHECK(model.begin_source(1, 1));
    CK_CHECK(model.append_bytes("0\n1\n2\n"));
    const auto first = model.at(0)->id;
    const auto survivor = model.at(1)->id;
    CK_CHECK(model.append_bytes("3"));
    CK_CHECK(model.size() == 3);
    CK_CHECK(!model.index_of(first));
    CK_CHECK(model.index_of(survivor) == 0);
    CK_CHECK(model.stats().evicted == 1);
    CK_CHECK(model.append_bytes("\n"));
    CK_CHECK(model.size() == 3);
    CK_CHECK(model.stats().evicted == 1);
}

CK_TEST(log_model_arena_wraps_and_evicts_payload_without_corrupting_survivors) {
    ckv::widgets::LogModel model(small_limits(128));
    CK_CHECK(model.begin_source(1, 1));
    for (std::size_t iteration = 0; iteration < 300; ++iteration) {
        const std::string raw = std::to_string(iteration) + std::string(97, 'x');
        CK_CHECK(model.append_record(raw));
        CK_CHECK(model.retained_payload_bytes() <= model.limits().retained_bytes);
        for (std::size_t i = 0; i < model.size(); ++i) {
            const auto entry = model.at(i);
            CK_CHECK(entry->text == entry->raw);
            CK_CHECK(entry->raw == std::to_string(entry->id.sequence - 1) + std::string(97, 'x'));
            CK_CHECK(model.index_of(entry->id) == i);
        }
    }
    CK_CHECK(model.stats().evicted > 0);
    CK_CHECK(model.stats().committed == 300);
}

CK_TEST(log_model_progress_overwrite_is_invariant_at_every_chunk_boundary) {
    const std::string input = "one\rtwo\r\n\xE4\xB8\xAD\rfinal\n";
    ckv::widgets::LogModel whole(small_limits(), ckv::widgets::LogCarriageReturn::Progress);
    CK_CHECK(whole.begin_source(1, 1));
    CK_CHECK(whole.append_bytes(input));
    whole.finish();
    const auto expected = snapshot(whole);
    for (std::size_t split = 0; split <= input.size(); ++split) {
        ckv::widgets::LogModel divided(small_limits(), ckv::widgets::LogCarriageReturn::Progress);
        CK_CHECK(divided.begin_source(1, 1));
        CK_CHECK(divided.append_bytes(std::string_view(input).substr(0, split)));
        CK_CHECK(divided.append_bytes(std::string_view(input).substr(split)));
        divided.finish();
        CK_CHECK(snapshot(divided) == expected);
    }
}

CK_TEST(log_model_metadata_multiline_and_timestamps_are_explicit_borrowed_values) {
    ckv::widgets::LogModel model(small_limits());
    CK_CHECK(model.begin_source(8, 3));
    std::array fields{ckv::LogField{"unit", "demo"}, ckv::LogField{"event", "42"}};
    ckv::LogMetadata metadata{ckv::LogSeverity::Warning, 1234, fields};
    CK_CHECK(model.append_record("error\n  detail", metadata));
    const auto record = model.at(0);
    CK_CHECK(record->text == "error\n  detail");
    CK_CHECK(record->severity == ckv::LogSeverity::Warning);
    CK_CHECK(record->timestamp_nanos == 1234);
    CK_CHECK(record->field_count == 2);
    CK_CHECK(record->fields[0].name == "unit");
    CK_CHECK(record->fields[0].value == "demo");
    CK_CHECK(record->metadata().valid());
    fields[1].name = "unit";
    CK_CHECK(!model.append_record("must not appear", metadata));
    CK_CHECK(model.size() == 1);
}

CK_TEST(log_model_source_transition_finishes_partial_and_clear_never_reuses_ids) {
    ckv::widgets::LogModel model(small_limits());
    CK_CHECK(model.begin_source(1, 2));
    CK_CHECK(model.append_bytes("old"));
    const auto old = model.at(0)->id;
    CK_CHECK(!model.begin_source(1, 1));
    CK_CHECK(model.begin_source(1, 3));
    CK_CHECK(model.at(0)->id == old);
    CK_CHECK(model.at(0)->incomplete);
    CK_CHECK(model.append_bytes("new\n"));
    const auto next = model.at(1)->id;
    CK_CHECK(next.generation == 3);
    CK_CHECK(next.sequence > old.sequence);
    model.clear();
    CK_CHECK(model.size() == 0);
    CK_CHECK(!model.index_of(old));
    CK_CHECK(model.append_bytes("after\n"));
    CK_CHECK(model.at(0)->id.sequence > next.sequence);
    CK_CHECK(model.stats().evicted == 2);
}

CK_TEST(log_model_allocated_capacity_is_fixed_during_prolonged_append_and_clear) {
    ckv::widgets::LogModel model(small_limits(4));
    CK_CHECK(model.begin_source(1, 1));
    const auto capacity = model.allocated_bytes();
    for (std::size_t i = 0; i < 1000; ++i) {
        CK_CHECK(model.append_bytes("plain \xE4\xB8\xAD \xFF\n"));
        if (i % 7 == 0) model.clear();
        CK_CHECK(model.allocated_bytes() == capacity);
        CK_CHECK(model.size() <= 4);
    }
}
