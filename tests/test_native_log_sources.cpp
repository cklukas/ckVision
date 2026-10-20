// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../examples/logmonitor/native_json.hpp"
#include "../examples/logmonitor/process_json_feed.hpp"
#include "../examples/logmonitor/windows_event_record.hpp"
#include "native_log_source_test_fixture.hpp"
#include "cvision/widgets/log_stream_receiver.hpp"
#include "cvision/testing/cktest.hpp"
#include <algorithm>
#include <limits>
#include <fstream>
using namespace ckv;
using namespace ckv::logmonitor;
namespace {
std::string_view field(const LogMessage& message, std::string_view name) {
    for (const auto& value : message.metadata().fields) if (value.name == name) return value.value;
    return {};
}
class ReplaySession final : public core::ProcessStreamSession {
public:
    LogMailbox out{1, 1, {4, 16 * 1024, LogOverflowPolicy::Backpressure}}, err{2, 1, {4, 16 * 1024, LogOverflowPolicy::Backpressure}};
    core::ProcessStreamStatus observation{.state = core::ProcessStreamState::Running};
    LogMailbox& stdout_mailbox() noexcept override { return out; }
    LogMailbox& stderr_mailbox() noexcept override { return err; }
    bool status(core::ProcessStreamStatus& result) const noexcept override { result = observation; return true; }
    void cancel() noexcept override { observation.state = core::ProcessStreamState::Cancelled; }
    std::size_t allocated_bytes() const noexcept override { return sizeof(*this) + out.allocated_bytes() + err.allocated_bytes(); }
    void bytes(std::string_view text, bool stderr_pipe = false) {
        LogMessage message; message.source = stderr_pipe ? 2 : 1; message.epoch = 1; message.arrival_nanos = 10;
        CK_CHECK(message.set_bytes(text)); CK_CHECK((stderr_pipe ? err : out).offer(message) == LogOfferResult::Accepted);
    }
    void finish(std::int64_t code = 0) {
        observation.state = core::ProcessStreamState::Completed; observation.exit = core::ProcessExitStatus{core::ProcessExitKind::Normal, code};
        CK_CHECK(out.close(LogSourceState::Ended, 0, 11)); CK_CHECK(err.close(LogSourceState::Ended, 0, 11));
    }
};
struct FeedFixture {
    ReplaySession* native; std::unique_ptr<ProcessJsonFeed> feed;
    widgets::LogModel model; std::unique_ptr<widgets::LogStreamReceiver> receiver;
    std::int64_t time = 100;
    FeedFixture() {
        auto session = std::make_unique<ReplaySession>(); native = session.get();
        feed = std::make_unique<ProcessJsonFeed>(9, NativeJsonSchema::Journal, std::move(session), nullptr, "Replay: retry may duplicate/gap.");
        receiver = std::make_unique<widgets::LogStreamReceiver>(model, feed->publications());
    }
    void step(bool receive = true) { feed->step(++time); if (receive) receiver->step(time); }
    void settle(std::size_t count = 40) { for (std::size_t index = 0; index < count; ++index) step(); }
};
}
CK_TEST(native_json_journal_maps_metadata_and_preserves_unknown_envelope_bytes) {
    NativeJsonDecoder decoder; LogMessage output; output.source = 9; output.epoch = 3;
    constexpr std::string_view raw = R"({"MESSAGE":"event \ud83d\ude00 <private>","PRIORITY":"4","__REALTIME_TIMESTAMP":"1700000000123456","_COMM":"app","_PID":"42","_SYSTEMD_UNIT":"app.service","__CURSOR":"s=tag;i=7","future":{"unknown":[1,true,null]}})";
    CK_CHECK(decoder.decode(raw, NativeJsonSchema::Journal, output) == NativeJsonResult::Record);
    CK_CHECK(output.source == 9 && output.epoch == 3 && output.metadata().severity == LogSeverity::Warning);
    CK_CHECK(output.metadata().timestamp_nanos == 1700000000123456000LL);
    CK_CHECK(field(output, "MESSAGE") == "event 😀 <private>" && field(output, "__CURSOR") == "s=tag;i=7");
    CK_CHECK(std::string_view(output.bytes().data(), output.bytes().size()) == raw);
}
CK_TEST(native_json_apple_preserves_redaction_unknown_fields_and_explicit_offset_time) {
    NativeJsonDecoder decoder; LogMessage output;
    constexpr std::string_view raw = R"({"eventMessage":"<private>","timestamp":"2023-11-14 23:13:20.123456+0100","messageType":"Fault","processImagePath":"/app","processID":42,"subsystem":"ckv","category":"test","activityIdentifier":7,"future":1})";
    CK_CHECK(decoder.decode(raw, NativeJsonSchema::Apple, output) == NativeJsonResult::Record);
    CK_CHECK(output.metadata().severity == LogSeverity::Critical && field(output, "eventMessage") == "<private>");
    CK_CHECK(output.metadata().timestamp_nanos == 1700000000123456000LL && field(output, "processID") == "42");
}
CK_TEST(native_json_rejects_malformed_grammar_utf8_surrogates_and_selected_duplicates) {
    NativeJsonDecoder decoder; LogMessage output;
    for (auto raw : {R"({"MESSAGE":"x",})", R"({"MESSAGE":"\ud800"})", R"({"MESSAGE":"\udc00"})", R"({"MESSAGE":"\q"})", R"({"MESSAGE":"x","v":01})", R"({"MESSAGE":"x","v":1.})", R"({"MESSAGE":"x","v":truefalse})", R"({"MESSAGE":"x","MESSAGE":"y"})", R"({"MESSAGE":"x"} extra)", R"({"unknown":1})"})
        CK_CHECK(decoder.decode(raw, NativeJsonSchema::Journal, output) == NativeJsonResult::Invalid);
    const std::string bad = "{\"MESSAGE\":\"" + std::string(1, static_cast<char>(0xFF)) + "\"}";
    CK_CHECK(decoder.decode(bad, NativeJsonSchema::Journal, output) == NativeJsonResult::Invalid);
}
CK_TEST(native_json_bounds_nesting_frame_and_metadata_prefix_without_losing_envelope) {
    NativeJsonDecoder decoder; LogMessage output;
    std::string deep = "{\"MESSAGE\":\"x\",\"future\":" + std::string(18, '[') + "0" + std::string(18, ']') + "}";
    CK_CHECK(decoder.decode(deep, NativeJsonSchema::Journal, output) == NativeJsonResult::Limit);
    CK_CHECK(decoder.decode(std::string(kLogBatchBytes + 1, ' '), NativeJsonSchema::Journal, output) == NativeJsonResult::Limit);
    std::string text; for (int index = 0; index < 200; ++index) text += "中";
    const std::string raw = "{\"MESSAGE\":\"" + text + "\",\"PRIORITY\":null}";
    CK_CHECK(decoder.decode(raw, NativeJsonSchema::Journal, output) == NativeJsonResult::Record && decoder.metadata_clipped());
    CK_CHECK(field(output, "MESSAGE").size() == 384 && output.bytes().size() == raw.size() && output.metadata().severity == LogSeverity::Unknown);
}
CK_TEST(native_timestamp_uses_explicit_offsets_leap_validation_and_checked_nanoseconds) {
    CK_CHECK(native_timestamp("1970-01-01T00:00:00Z") == 0);
    CK_CHECK(native_timestamp("1970-01-01T01:00:00+01:00") == 0);
    CK_CHECK(native_timestamp("2024-02-29T00:00:00.000000001Z").has_value());
    for (auto text : {"2023-02-29T00:00:00Z", "2024-01-01T00:00:00", "2024-01-01T00:00:00+", "2024-01-01T00:00:00+2", "2024-01-01T00:00:00+24:00", "2024-01-01T00:00:60Z", "2262-12-31T23:59:59Z", "2024-01-01T00:00:00.1234567890Z"}) CK_CHECK(!native_timestamp(text));
}
CK_TEST(native_json_feed_reassembles_split_pipe_chunks_and_retires_after_both_eofs) {
    FeedFixture fixture;
    fixture.native->bytes("{\"MESSAGE\":\"spl"); fixture.step();
    CK_CHECK(fixture.model.size() == 0 && !fixture.feed->next_deadline());
    fixture.native->bytes("it 中\",\"PRIORITY\":\"6\"}\n"); fixture.native->finish(); fixture.settle();
    CK_CHECK(fixture.model.size() == 1 && fixture.model.at(0)->severity == LogSeverity::Info);
    CK_CHECK(fixture.receiver->status().state == LogSourceState::Ended && !fixture.feed->next_deadline());
}
CK_TEST(native_json_feed_discards_malformed_oversized_and_incomplete_envelopes_explicitly) {
    FeedFixture fixture; fixture.native->bytes("bad\n"); fixture.step();
    const std::string piece(16 * 1024, 'x');
    for (int index = 0; index < 5; ++index) { fixture.native->bytes(piece); fixture.step(); }
    fixture.native->bytes("\n{\"MESSAGE\":\"incomplete"); fixture.native->finish(); fixture.settle();
    CK_CHECK(fixture.model.size() == 3);
    CK_CHECK(fixture.model.at(0)->text.find("malformed") != std::string_view::npos);
    CK_CHECK(fixture.model.at(1)->text.find("64 KiB") != std::string_view::npos);
    CK_CHECK(fixture.model.at(2)->text.find("incomplete") != std::string_view::npos);
    CK_CHECK(fixture.feed->explanation().find("oversized: 1") != std::string_view::npos);
}
CK_TEST(native_json_feed_bounds_work_and_reports_structured_overload_with_real_receiver_gaps) {
    FeedFixture fixture;
    for (int index = 0; index < 12; ++index) { fixture.native->bytes("{\"MESSAGE\":\"a\"}\n{\"MESSAGE\":\"b\"}\n{\"MESSAGE\":\"c\"}\n{\"MESSAGE\":\"d\"}\n"); fixture.step(false); }
    LogMailboxStatus queue; CK_CHECK(fixture.feed->publications().status(queue));
    CK_CHECK(queue.queued_batches <= 8 && queue.dropped.records > 0 && queue.high_water_batches == 8);
    fixture.native->finish(); fixture.settle();
    CK_CHECK(fixture.receiver->status().gap_markers > 0 && fixture.feed->diagnostic().uncertain);
}
CK_TEST(native_json_feed_keeps_permission_stderr_private_and_surfaces_failure_recovery) {
    FeedFixture fixture; fixture.native->bytes("permission denied: journal unavailable\n", true); fixture.native->finish(1); fixture.settle();
    CK_CHECK(fixture.model.size() == 0 && fixture.feed->diagnostic().state == LogSourceState::Failed);
    CK_CHECK(fixture.feed->explanation().find("permission denied") != std::string_view::npos && fixture.feed->explanation().find("File/Sample") != std::string_view::npos);
    fixture.feed->stop(fixture.time); CK_CHECK(fixture.native->observation.state == core::ProcessStreamState::Cancelled);
}
CK_TEST(native_helper_probe_accepts_apples_usage_help_but_rejects_failed_or_truncated_probes) {
    core::ProcessRunResult result; result.state = core::ProcessRunState::Completed;
    result.exit = core::ProcessExitStatus{core::ProcessExitKind::Normal, 64}; result.stderr_capture.bytes = "--style (valid: default, syslog, json, ndjson, compact)";
    CK_CHECK(supports_native_json(NativeJsonSchema::Apple, result));
    CK_CHECK(!supports_native_json(NativeJsonSchema::Journal, result));
    result.exit->code = 1; CK_CHECK(!supports_native_json(NativeJsonSchema::Apple, result));
    result.exit->code = 64; result.stderr_capture.truncated = true; CK_CHECK(!supports_native_json(NativeJsonSchema::Apple, result));
    result.stderr_capture.truncated = false; result.exit->kind = core::ProcessExitKind::Signal; CK_CHECK(!supports_native_json(NativeJsonSchema::Apple, result));
    result.exit->kind = core::ProcessExitKind::Normal; result.exit->code = 0; result.stdout_capture.bytes = "--follow --output=json";
    CK_CHECK(supports_native_json(NativeJsonSchema::Journal, result));
}
CK_TEST(windows_event_record_replay_maps_observed_metadata_preserves_xml_and_reports_missing_messages) {
    LogMessage output; output.source = 9; output.epoch = 7;
    const WindowsEventRecord event{.xml = "<Event><future>unknown</future><Data>tag 中</Data></Event>", .channel = "Application", .provider = "ckVisionTest",
        .bookmark = "<BookmarkList><Bookmark Channel=\"Application\" RecordId=\"42\" IsCurrent=\"true\"/></BookmarkList>",
        .event_id = 123, .record_id = 42, .filetime = 133444736001234567ULL, .level = std::uint8_t{3}, .render_error = 15027};
    const auto result = encode_windows_event(event, output);
    CK_CHECK(result.accepted && !result.metadata_clipped && output.source == 9 && output.epoch == 7);
    CK_CHECK(output.metadata().severity == LogSeverity::Warning && output.metadata().timestamp_nanos == 1700000000123456700LL);
    CK_CHECK(field(output, "provider") == "ckVisionTest" && field(output, "event_id") == "123" && field(output, "record_id") == "42");
    CK_CHECK(field(output, "bookmark") == event.bookmark && field(output, "render_error") == "15027");
    CK_CHECK(std::string_view(output.bytes().data(), output.bytes().size()) == event.xml);
}
CK_TEST(windows_event_record_replay_bounds_unicode_prefixes_and_keeps_unknown_time_level_unknown) {
    LogMessage output; WindowsEventRecord event{.xml = "<Event/>", .channel = "System", .provider = "future", .filetime = std::numeric_limits<std::uint64_t>::max(), .level = std::uint8_t{255}};
    std::string message(383, 'x'); message += "😀 suffix"; event.rendered_message = message;
    const auto result = encode_windows_event(event, output);
    CK_CHECK(result.accepted && result.metadata_clipped && field(output, "rendered_message").size() == 383);
    CK_CHECK(!output.metadata().timestamp_nanos && output.metadata().severity == LogSeverity::Unknown);
    event.filetime = 116444736000000000ULL; event.level = std::uint8_t{0};
    CK_CHECK(encode_windows_event(event, output).accepted && output.metadata().timestamp_nanos == 0 && output.metadata().severity == LogSeverity::Unknown);
    event.xml = std::string_view("\xFF", 1); CK_CHECK(!encode_windows_event(event, output).accepted);
}
CK_TEST(native_source_replay_goldens_cover_three_schemas_denial_stale_and_missing_messages) {
    for (const auto name : testing::native_log_golden_names) {
        std::ifstream input(std::string(CKV_TEST_SOURCE_DIR) + "/golden/logmonitor_native_" + std::string(name) + ".dump", std::ios::binary);
        CK_CHECK(input.good()); const std::string expected((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CK_CHECK(testing::native_log_golden(name) == expected);
    }
    testing::NativeLogFixture fixture("windows"); fixture.monitor.set_filter("controlled tag"); fixture.settle();
    CK_CHECK(fixture.monitor.panel(0)->provider().size() == 1 && fixture.monitor.detail_text().find("bookmark") != std::string::npos);
    fixture.monitor.panel(0)->invoke(widgets::LogPanelAction::Pause); fixture.step(); fixture.monitor.panel(0)->invoke(widgets::LogPanelAction::Resume);
    fixture.monitor.close_source(0); fixture.settle(); CK_CHECK(!fixture.monitor.source_count());
}
