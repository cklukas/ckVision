// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "log_view_test_fixture.hpp"
#include "log_file_test_fixture.hpp"
#include "log_delivery_view_test_fixture.hpp"
#include "cvision/widgets/log_controller.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/testing/cktest.hpp"

#include <charconv>
#include <fstream>
#include <iterator>
#include <string>

using namespace ckv;
using namespace ckv::widgets;
namespace {
struct LaneFixture {
    LogModel model{testing::log_view_limits(32)};
    LogFilter filter{model, {1, 8}};
    LogMailbox mailbox{1, 1, {4, 64, LogOverflowPolicy::DropOldest}};
    LogStreamReceiver receiver{model, mailbox, {32, 2}};
    LogLane lane() { return {&model, &filter, nullptr, nullptr, nullptr, &receiver}; }
    void offer(std::string_view text, std::int64_t now = 0) {
        LogMessage message; message.source = 1; message.epoch = 1; message.arrival_nanos = now;
        message.set_record(text); CK_CHECK(mailbox.offer(message) == LogOfferResult::Accepted);
    }
};
class SlowTerminal final : public term::Terminal {
public:
    term::HeadlessTerminal host{{80, 24}};
    std::string pending;
    std::size_t frames = 0, maximum_pending_bytes = 0;
    bool overflowed = false;
    SlowTerminal() { pending.reserve(1024 * 1024); }
    term::Capabilities capabilities() const noexcept override { return host.capabilities(); }
    Size size() const noexcept override { return host.size(); }
    std::vector<term::TerminalEvent> poll(std::int64_t time) override { return host.poll(time); }
    std::size_t frame_acknowledgements() const noexcept override { return host.frame_acknowledgements(); }
    void write(std::string_view bytes) override {
        if (bytes.find("\x1B[5n") == std::string_view::npos) { host.write(bytes); return; }
        if (!pending.empty()) overflowed = true;
        pending.assign(bytes); ++frames; maximum_pending_bytes = std::max(maximum_pending_bytes, pending.size());
    }
    void complete(std::int64_t time) {
        if (pending.empty()) return;
        host.write(pending); pending.clear(); host.inject_bytes("\x1B[0n", time);
    }
    void set_title(std::string_view) override {}
    void bell() override {}
    ClipboardWriteResult write_clipboard(std::string_view) override { return {ClipboardWriteStatus::Unsupported}; }
};
class Keys final : public ui::View {
public:
    Keys() : View({0, 0, 1, 1}, ui::FocusPolicy::TabStop) { set_fills_root(false); }
    int received = 0;
    bool on_key(const KeyEvent&) override { ++received; return true; }
};
class ChangingText final : public ui::View {
public:
    ChangingText() : View({0, 0, 1, 1}) { set_fills_root(false); }
    char value = '0';
    void draw(scene::Painter& painter) override { painter.draw_text({0, 0}, {&value, 1}, {}); }
};
}
CK_TEST(log_controller_round_robin_gives_each_source_one_bounded_quantum) {
    LaneFixture first, second; LogController controller({2, 1, 1, 10});
    CK_CHECK(controller.attach(first.lane()) && controller.attach(second.lane()));
    first.offer("first"); second.offer("second");
    controller.step(0); CK_CHECK(first.model.size() == 1 && second.model.size() == 0);
    controller.step(0); CK_CHECK(second.model.size() == 0);
    controller.step(1); CK_CHECK(second.model.size() == 1 && controller.status().last_turn_lanes == 1);
    controller.step(2); controller.step(10); CK_CHECK(!controller.next_deadline());
}
CK_TEST(log_controller_coalesces_refreshes_and_yields_between_processing_turns) {
    LaneFixture fixture; LogPanel panel(fixture.filter); auto lane = fixture.lane(); lane.panel = &panel;
    LogController controller({1, 1, 1, 10}); const auto id = controller.attach(lane); CK_CHECK(id);
    fixture.offer("zero"); CK_CHECK(controller.step(0));
    for (std::int64_t time = 1; time < 10; ++time) { fixture.offer("new", time); CK_CHECK(!controller.step(time)); }
    CK_CHECK(controller.status().refreshes == 1 && controller.status().coalesced_updates >= 7);
    CK_CHECK(controller.step(10)); CK_CHECK(controller.status().refreshes == 2);
    CK_CHECK(controller.status().maximum_observed_refresh_lag_nanos == 9);
    CK_CHECK(controller.detach(id) && !controller.kick(id) && !controller.next_deadline());
}
CK_TEST(log_controller_holds_ingestion_until_filter_work_catches_up_and_shows_loss) {
    LaneFixture fixture; LogPanel panel(fixture.filter); auto lane = fixture.lane(); lane.panel = &panel;
    LogController controller({1, 1, 1, 5}); controller.attach(lane);
    fixture.offer("a long retained message"); controller.step(0);
    fixture.filter.set_include("not present");
    const auto processed = fixture.model.stats().processed_bytes;
    for (std::int64_t time = 1; time < 5; ++time) {
        for (int batch = 0; batch < 10; ++batch) fixture.offer("stream flood", time);
        controller.step(time);
    }
    CK_CHECK(controller.status().ingress_deferrals > 0 && fixture.model.stats().processed_bytes == processed);
    controller.step(5);
    CK_CHECK(panel.source_status().dropped > 0 && panel.source_status().backlog_bytes <= 4 * 64);
}
CK_TEST(log_controller_file_deferral_preserves_offsets_and_buffered_read_identity) {
    testing::ScriptedLogFiles files; LogModel model(testing::log_view_limits()); LogFilter filter(model, {1, 1});
    LogFollowOptions options; options.start = LogStartMode::Beginning;
    LogFollower follower(model, files.open_session(), options);
    follower.start("/source", 1, 0);
    LogController controller({1, 1, 1, 10}); const auto id = controller.attach({&model, &filter, nullptr, nullptr, &follower});
    model.begin_source(1, 1); model.append_record("a long existing entry"); filter.set_include("missing");
    const auto revision = files.requested().revision;
    files.publish(revision, LogFileObservation{{1, 1, 0}, 5, 1});
    const auto count = files.request_count(); controller.step(0);
    CK_CHECK(files.request_count() == count && follower.status().offset == 0);
    for (std::int64_t time = 1; time < 100; ++time) { controller.kick(id); controller.step(time); if (files.request_count() > count) break; }
    CK_CHECK(files.request_count() > count && files.requested().operation == LogFileOperation::Read);
}
// Without a wake target nothing kicks the lane, so an idle follower's pending
// change hint must itself make the lane ready instead of waiting out the
// reconciliation interval. The append without a hint is the partner: it is
// still left to reconciliation, so the hint is what moved the first one.
CK_TEST(log_controller_steps_an_idle_file_lane_on_a_change_hint_without_a_wake) {
    testing::ScriptedLogFiles files; LogModel model(testing::log_view_limits()); LogFilter filter(model, {1, 8});
    LogFollowOptions options; options.start = LogStartMode::Beginning; options.result_poll_nanos = 1;
    LogFollower follower(model, files.open_session(), options);
    std::string contents = "one\n"; std::size_t answered = 0;
    const auto respond = [&] {
        if (answered == files.request_count()) return;
        answered = files.request_count(); const auto& request = files.requested();
        std::string_view bytes;
        if (request.operation == LogFileOperation::Read && request.offset < contents.size())
            bytes = std::string_view(contents).substr(static_cast<std::size_t>(request.offset), request.bytes);
        files.publish(request.revision, LogFileObservation{{1, 1, 0}, contents.size(), contents.size()}, bytes);
    };
    LogController controller({1, 1, 1, 10}); controller.attach({&model, &filter, nullptr, nullptr, &follower});
    std::int64_t now = 0;
    const auto run_until = [&](std::int64_t end) { for (; now < end; ++now) { respond(); controller.step(now); } };
    CK_CHECK(follower.start("/source", 1, 0)); run_until(1'000);
    CK_CHECK(model.size() == 1 && !follower.status().pending);
    const auto reconcile = follower.next_deadline(); CK_CHECK(reconcile && *reconcile > 500'000'000);
    contents += "two\n"; files.hint();
    CK_CHECK(follower.next_deadline() && *follower.next_deadline() <= now);
    run_until(now + 1'000);
    CK_CHECK(model.size() == 2 && model.at(1)->raw == "two");
    contents += "three\n"; run_until(now + 1'000);
    CK_CHECK(model.size() == 2 && follower.next_deadline() && *follower.next_deadline() > now);
}
CK_TEST(log_controller_activity_latch_probes_all_lanes_without_an_idle_poll_timer) {
    LaneFixture first, second; LogActivity activity; LogController controller({2, 1, 2, 10}, &activity);
    controller.attach(first.lane()); controller.attach(second.lane()); controller.step(0); controller.step(2); controller.step(10);
    CK_CHECK(!controller.next_deadline());
    activity.notify(); activity.notify(); CK_CHECK(controller.next_deadline().has_value());
    controller.step(12); controller.step(14); CK_CHECK(!controller.next_deadline());
}
CK_TEST(bounded_frame_delivery_holds_small_frames_and_preserves_input_progress) {
    SlowTerminal terminal; ManualClock clock; ui::Application app(terminal, clock);
    const auto roles = ui::intern_standard_roles(app.roles());
    app.set_theme(ui::make_classic_theme(app.roles(), roles));
    app.set_frame_delivery_policy(ui::FrameDeliveryPolicy::Bounded);
    LaneFixture fixture; auto* panel = app.root().add(std::make_unique<LogPanel>(fixture.filter));
    auto* keys = app.root().add(std::make_unique<Keys>()); app.set_focus(keys);
    auto lane = fixture.lane(); lane.panel = panel; LogController controller({1, 1, 1'000'000, 33'333'334}); controller.attach(lane);
    controller.step(0); app.step(0); CK_CHECK(app.frames_awaiting_terminal() == 1);
    terminal.complete(1); clock.set(1); app.step(1);
    for (int event = 0; event < 1000; ++event) {
        const auto now = std::int64_t{event + 1} * 1'000'000; clock.set(now);
        std::array<char, 32> text{}; std::string_view prefix = "event "; std::copy(prefix.begin(), prefix.end(), text.begin());
        const auto number = std::to_chars(text.data() + prefix.size(), text.data() + text.size(), event);
        fixture.offer({text.data(), static_cast<std::size_t>(number.ptr - text.data())}, now);
        controller.step(now);
        if (event % 10 == 0) terminal.host.inject_event(KeyEvent{{Key::Char, Modifier::None, "x"}});
        if (event % 333 == 332) terminal.complete(now);
        app.step(now); CK_CHECK(app.frames_awaiting_terminal() <= 1);
    }
    CK_CHECK(keys->received == 100 && !terminal.overflowed && terminal.frames < 10);
    CK_CHECK(fixture.model.size() == 32 && fixture.model.at(31)->text == "event 999");
    clock.advance(40'000'000); controller.step(clock.now_nanos()); terminal.complete(clock.now_nanos()); app.step(clock.now_nanos());
    terminal.complete(clock.now_nanos()); app.step(clock.now_nanos());
    const auto frame = terminal.host.display().frame(); std::string visible;
    for (int row = 0; row < frame.size().height; ++row) for (int column = 0; column < frame.size().width; ++column)
        visible += frame.at({column, row}).grapheme();
    CK_CHECK(visible.find("event 999") != std::string::npos && terminal.maximum_pending_bytes < 1024 * 1024);
}
CK_TEST(bounded_frame_delivery_never_times_out_an_established_responder) {
    term::HeadlessTerminal terminal({80, 24}); ManualClock clock; ui::Application app(terminal, clock);
    auto* text = app.root().add(std::make_unique<ChangingText>());
    app.set_frame_delivery_policy(ui::FrameDeliveryPolicy::Bounded); app.step(0);
    terminal.inject_bytes("\x1B[0n", 1); clock.set(1); app.step(1);
    text->value = '1'; text->invalidate(); app.step(1); const auto emitted = terminal.written_bytes().size();
    for (int tick = 0; tick < 20; ++tick) {
        text->value = static_cast<char>('2' + tick % 2);
        clock.advance(1'000'000'000); text->invalidate(); app.step(clock.now_nanos());
    }
    CK_CHECK(app.frames_awaiting_terminal() == 1 && terminal.written_bytes().size() == emitted);
    terminal.inject_bytes("\x1B[0n", clock.now_nanos()); app.step(clock.now_nanos());
    CK_CHECK(app.frames_awaiting_terminal() <= 1 && terminal.written_bytes().size() > emitted);
}
CK_TEST(bounded_frame_delivery_nonresponder_uses_explicit_existing_probe_fallback) {
    term::HeadlessTerminal terminal({20, 5}); ManualClock clock; ui::Application app(terminal, clock);
    app.set_frame_delivery_policy(ui::FrameDeliveryPolicy::Bounded);
    for (int attempt = 0; attempt < ui::kFrameCompletionGiveUpCount; ++attempt) {
        app.invalidate_all(); app.step(clock.now_nanos()); clock.advance(ui::kFrameCompletionTimeoutNanos); app.step(clock.now_nanos());
    }
    CK_CHECK(!app.frame_completion_tracking());
}

CK_TEST(log_overload_frames_show_loss_backlog_lag_and_paused_presentation) {
    for (const auto name : testing::log_delivery_golden_names) {
        std::ifstream file(std::string(CKV_TEST_SOURCE_DIR) + "/golden/log_" + std::string(name) + ".dump", std::ios::binary);
        CK_CHECK(file.good());
        const std::string expected{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        CK_CHECK(testing::log_delivery_view_golden(name) == expected);
    }
}
