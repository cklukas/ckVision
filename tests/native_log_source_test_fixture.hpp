// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "logmonitor_test_fixture.hpp"
#include "../examples/logmonitor/native_json.hpp"
#include "../examples/logmonitor/windows_event_record.hpp"
namespace ckv::testing {
inline constexpr std::array<std::string_view, 6> native_log_golden_names{
    "apple", "journal", "windows", "windows_denied", "windows_stale", "windows_missing_message"};
class NativeReplayFeed final : public logmonitor::StreamFeed {
public:
    NativeReplayFeed(LogSourceId id, std::shared_ptr<const LogWake> wake, std::string_view name)
        : output_(id, 1, {8, kLogBatchBytes, LogOverflowPolicy::DropOldest}, std::move(wake)) {
        status_.state = LogSourceState::Following; status_.watch = LogWatchMode::Native; status_.uncertain = true;
        if (name == "windows_denied" || name == "windows_stale") {
            status_.state = name == "windows_denied" ? LogSourceState::Denied : LogSourceState::Failed;
            status_.native_error = name == "windows_denied" ? 5 : 15011;
            text_ = name == "windows_denied" ? "Channel permission denied. Choose an accessible channel or File/Sample." : "Stale bookmark: event continuity lost. Retry without stale resume state or choose File/Sample.";
            output_.close(status_.state, status_.native_error, 0); return;
        }
        LogMessage message; message.source = id; message.epoch = 1;
        if (name == "apple" || name == "journal") {
            logmonitor::NativeJsonDecoder decoder;
            const std::string_view raw = name == "apple" ? R"({"eventMessage":"controlled tag 中 <private>","timestamp":"2023-11-14 23:13:20.123456+0100","messageType":"Error","processImagePath":"/sample/app","processID":42,"subsystem":"org.ckvision.proof","category":"sample","future":true})" :
                R"({"MESSAGE":"controlled tag 中","PRIORITY":"4","__REALTIME_TIMESTAMP":"1700000000123456","_SYSTEMD_UNIT":"sample.service","_COMM":"app","_PID":"42","__CURSOR":"s=sample;i=1","future":[1,2]})";
            decoder.decode(raw, name == "apple" ? logmonitor::NativeJsonSchema::Apple : logmonitor::NativeJsonSchema::Journal, message);
            text_ = name == "apple" ? "Live info/default; no startup history. Unknown native JSON and redaction retained." : "Recent 100 accessible records then live; retry may duplicate/gap. Cursor is source metadata.";
        } else {
            const bool missing = name == "windows_missing_message";
            logmonitor::encode_windows_event({.xml = "<Event><System><Provider Name=\"ckVisionTest\"/><EventID>123</EventID></System><EventData><Data>controlled tag 中</Data><Future>unknown</Future></EventData></Event>",
                .channel = "Application", .provider = "ckVisionTest", .bookmark = "<BookmarkList><Bookmark Channel=\"Application\" RecordId=\"42\" IsCurrent=\"true\"/></BookmarkList>",
                .rendered_message = missing ? "" : "controlled tag 中", .event_id = 123, .record_id = 42,
                .filetime = missing ? std::nullopt : std::optional<std::uint64_t>{133444736001234567ULL}, .level = std::uint8_t{2}, .render_error = missing ? 15027U : 0U}, message);
            text_ = missing ? "Rendered message unavailable (15027); raw XML fields retained. Source timestamp unknown." : "Future-only subscription; channel/bookmark explicit. Retry declares potential continuity gap.";
        }
        output_.offer(message);
    }
    LogMailbox& publications() noexcept override { return output_; }
    void step(std::int64_t) override {}
    std::optional<std::int64_t> next_deadline() const noexcept override { return {}; }
    widgets::LogPanelSourceStatus diagnostic() const override { return status_; }
    std::string_view explanation() const noexcept override { return text_; }
    void stop(std::int64_t now) noexcept override { status_.state = LogSourceState::Cancelled; output_.cancel(now); }
private:
    LogMailbox output_; widgets::LogPanelSourceStatus status_; std::string text_;
};
struct NativeLogFixture {
    ManualClock clock; term::HeadlessTerminal terminal{{100, 30}}; ui::Application app{terminal, clock};
    MemoryFileSystem fs; ScriptedLogFiles files; MemoryExportWriter exports{fs};
    std::shared_ptr<LogActivity> activity = std::make_shared<LogActivity>();
    logmonitor::LogMonitorApp monitor;
    explicit NativeLogFixture(std::string name) : monitor(app, files, fs, exports, activity,
        {logmonitor::NativeSourceDescriptor{"native-replay", "Native " + name, "Controlled native replay", true,
            [name](LogSourceId id, std::shared_ptr<const LogWake> wake) { return std::make_unique<NativeReplayFeed>(id, std::move(wake), name); }}}) {
        monitor.open_native(0); settle();
    }
    void step() { clock.advance(1'000'000); monitor.step(clock.now_nanos()); if (app.frames_awaiting_terminal()) terminal.inject_bytes("\x1B[0n", clock.now_nanos()); app.step(clock.now_nanos()); }
    void settle() { for (int turn = 0; turn < 40; ++turn) step(); }
    std::string golden() { return golden::serialize(scene::capture_frame(app.compositor(), app.current_cursor())); }
};
inline std::string native_log_golden(std::string_view name) { NativeLogFixture fixture{std::string(name)}; fixture.monitor.show_details(); fixture.settle(); return fixture.golden(); }
}  // namespace ckv::testing
