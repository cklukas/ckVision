// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/record_replay_terminal.hpp"

#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/testing/cktest.hpp"

using namespace ckv;
using namespace ckv::term;

namespace {

class ReplayFrameProbe final : public ui::View {
public:
    void draw(scene::Painter& painter) override {
        painter.draw_text(Point{0, 0}, "Recorded frame",
                          Style{Color::rgb(17, 34, 51), Color::rgb(68, 85, 102), Attr{}});
    }
};

}  // namespace

CK_TEST(recording_forwards_writes_and_captures_them) {
    HeadlessTerminal inner(Size{10, 10});
    RecordingTerminal recorder(inner);
    recorder.write("abc");
    recorder.write("def");
    CK_CHECK(inner.written_bytes() == "abcdef");  // forwarded through

    CK_CHECK(recorder.recording().size() == 2);
    CK_CHECK(std::get<RecordedWrite>(recorder.recording()[0]).bytes == "abc");
    CK_CHECK(std::get<RecordedWrite>(recorder.recording()[1]).bytes == "def");
}

CK_TEST(deterministic_terminals_expose_no_external_wait_handles) {
    HeadlessTerminal headless(Size{10, 10});
    RecordingTerminal recording(headless);
    ReplayTerminal replay({}, baseline_capabilities(), Size{10, 10});

    CK_CHECK(headless.wait_handles().empty());
    CK_CHECK(recording.wait_handles().empty());
    CK_CHECK(replay.wait_handles().empty());
}

CK_TEST(recording_forwards_poll_and_captures_events_including_empty_batches) {
    HeadlessTerminal inner(Size{10, 10});
    RecordingTerminal recorder(inner);

    const auto empty = recorder.poll(0);
    CK_CHECK(empty.empty());

    inner.inject_event(TerminalEvent{FocusEvent{true}});
    const auto with_event = recorder.poll(0);
    CK_CHECK(with_event.size() == 1);

    CK_CHECK(recorder.recording().size() == 2);  // both calls recorded, even the empty one
    CK_CHECK(std::get<RecordedEvents>(recorder.recording()[0]).events.empty());
    CK_CHECK(std::get<RecordedEvents>(recorder.recording()[1]).events.size() == 1);
}

CK_TEST(recording_forwards_title_bell_clipboard) {
    Capabilities caps = baseline_capabilities();
    caps.clipboard_write = true;
    HeadlessTerminal inner(Size{10, 10}, caps);
    RecordingTerminal recorder(inner);
    recorder.set_title("t");
    recorder.bell();
    recorder.write_clipboard("copied");
    CK_CHECK(inner.title() == "t");
    CK_CHECK(inner.bell_count() == 1);
    CK_CHECK(inner.clipboard() == "copied");
    CK_CHECK(recorder.recording().size() == 3);
    CK_CHECK(std::get<RecordedTitle>(recorder.recording()[0]).title == "t");
    CK_CHECK(std::holds_alternative<RecordedBell>(recorder.recording()[1]));
    CK_CHECK(std::get<RecordedClipboard>(recorder.recording()[2]).text == "copied");
}

CK_TEST(record_replay_keeps_post_restore_diagnostics_deterministic) {
    HeadlessTerminal inner(Size{10, 10});
    RecordingTerminal recorder(inner);
    ManualClock clock;

    // Drive the real Application lifecycle: it restores first and emits
    // severity, message, and newline through its terminal boundary only
    // afterward. Headless keeps the forwarding side free of host I/O.
    {
        ui::Application recorded_app(recorder, clock);
        recorded_app.diagnostics().log(LogLevel::Warning, "replay this");
    }
    CK_CHECK(recorder.recording().size() == 3);
    CK_CHECK(std::get<RecordedDiagnostic>(recorder.recording()[0]).message == "warning: ");
    CK_CHECK(std::get<RecordedDiagnostic>(recorder.recording()[1]).message == "replay this");
    CK_CHECK(std::get<RecordedDiagnostic>(recorder.recording()[2]).message == "\n");

    ReplayTerminal replay(recorder.recording(), recorder.initial_capabilities(), recorder.initial_size());
    {
        ui::Application replay_app(replay, clock);
        replay_app.diagnostics().log(LogLevel::Warning, "replay this");
    }
    CK_CHECK(replay.diagnostic_bytes() == "warning: replay this\n");
    CK_CHECK(replay.matches_recording());
}

CK_TEST(replay_returns_recorded_events_in_order_ignoring_write_entries) {
    std::vector<RecordedEntry> log;
    log.push_back(RecordedEntry{RecordedEvents{{TerminalEvent{FocusEvent{true}}}}});
    log.push_back(RecordedEntry{RecordedWrite{"some output that isn't input"}});
    log.push_back(RecordedEntry{RecordedEvents{{TerminalEvent{FocusEvent{false}}}}});

    ReplayTerminal replay(log, baseline_capabilities(), Size{80, 24});
    const auto first = replay.poll(0);
    CK_CHECK(first.size() == 1);
    CK_CHECK(std::get<FocusEvent>(first[0]).gained);

    const auto second = replay.poll(0);  // skips the RecordedWrite entry automatically
    CK_CHECK(second.size() == 1);
    CK_CHECK(!std::get<FocusEvent>(second[0]).gained);

    CK_CHECK(replay.exhausted());
    CK_CHECK(replay.poll(0).empty());
}

CK_TEST(replay_ignores_the_deadline_argument_entirely) {
    std::vector<RecordedEntry> log;
    log.push_back(RecordedEntry{RecordedEvents{{TerminalEvent{FocusEvent{true}}}}});
    ReplayTerminal replay(log, baseline_capabilities(), Size{80, 24});
    // A deadline in the distant past must still yield the scripted event.
    const auto events = replay.poll(-1'000'000'000);
    CK_CHECK(events.size() == 1);
}

CK_TEST(replay_capability_changed_event_updates_reported_capabilities) {
    Capabilities updated = baseline_capabilities();
    updated.color_scheme = ColorScheme::Dark;
    std::vector<RecordedEntry> log;
    log.push_back(RecordedEntry{RecordedEvents{{TerminalEvent{CapabilityChangedEvent{updated}}}}});
    ReplayTerminal replay(log, baseline_capabilities(), Size{80, 24});
    CK_CHECK(replay.capabilities().color_scheme == ColorScheme::Unknown);
    replay.poll(0);
    CK_CHECK(replay.capabilities().color_scheme == ColorScheme::Dark);
}

CK_TEST(replay_resize_event_updates_the_reported_terminal_size) {
    std::vector<RecordedEntry> log;
    log.push_back(RecordedEntry{RecordedEvents{{TerminalEvent{ResizeEvent{Size{120, 40}}}}}});

    ReplayTerminal replay(std::move(log), baseline_capabilities(), Size{80, 24});
    CK_CHECK(replay.size() == (Size{80, 24}));
    CK_CHECK(replay.poll(0).size() == 1);
    CK_CHECK(replay.size() == (Size{120, 40}));
}

CK_TEST(record_replay_preserves_a_complete_capability_refinement_batch_and_its_frame) {
    HeadlessTerminal inner(Size{80, 24});
    RecordingTerminal recorder(inner);

    Capabilities dark = baseline_capabilities();
    dark.color_scheme = ColorScheme::Dark;
    Capabilities refined = dark;
    refined.sixel_graphics = true;
    refined.sixel_color_registers = 16;
    refined.sixel_max_geometry = Size{640, 480};
    refined.cell_pixels = Size{8, 16};
    refined.pixel_mouse = true;
    refined.synchronized_output = true;
    refined.color_scheme_notifications = true;

    // A real POSIX read can contain all probe replies at once. Preserve their
    // intermediate events as one poll batch, then prove that replay reaches
    // the final profile before the capability-dependent frame is produced.
    inner.inject_capability_change(dark);
    inner.inject_capability_change(refined);
    const auto recorded_events = recorder.poll(0);
    CK_CHECK(recorded_events.size() == 2);
    recorder.write("capability-refined-frame");

    ReplayTerminal replay(recorder.recording(), recorder.initial_capabilities(), recorder.initial_size());
    CK_CHECK(replay.capabilities() == baseline_capabilities());
    CK_CHECK(replay.poll(-1) == recorded_events);
    CK_CHECK(replay.capabilities() == refined);
    replay.write("capability-refined-frame");
    CK_CHECK(replay.matches_recording());
}

CK_TEST(replay_captures_its_own_output_for_diffing_against_the_original_recording) {
    ReplayTerminal replay({}, baseline_capabilities(), Size{10, 10});
    replay.write("re-rendered output");
    CK_CHECK(replay.written_bytes() == "re-rendered output");
    replay.clear_written();
    CK_CHECK(replay.written_bytes().empty());
}

CK_TEST(end_to_end_record_then_replay_round_trip) {
    Capabilities caps = baseline_capabilities();
    caps.clipboard_write = true;
    HeadlessTerminal inner(Size{10, 10}, caps);
    RecordingTerminal recorder(inner);
    inner.inject_event(TerminalEvent{KeyEvent{KeyChord{Key::Enter, Modifier::None, {}}}});
    const auto recorded_events = recorder.poll(0);
    CK_CHECK(recorded_events.size() == 1);
    recorder.write("frame bytes");
    recorder.set_title("Replay title");
    recorder.bell();
    recorder.write_clipboard("clipboard");
    CK_CHECK(recorder.poll(0).empty());

    ReplayTerminal replay(recorder.recording(), recorder.initial_capabilities(), recorder.initial_size());
    const auto replayed_events = replay.poll(0);
    CK_CHECK(replayed_events == recorded_events);
    replay.write("frame bytes");
    replay.set_title("Replay title");
    replay.bell();
    replay.write_clipboard("clipboard");
    CK_CHECK(replay.poll(0).empty());
    CK_CHECK(replay.matches_recording());
}

CK_TEST(application_record_replay_preserves_resize_capability_refinement_and_presented_frames) {
    Capabilities initial = baseline_capabilities();
    initial.color_depth = ColorDepth::TrueColor;
    HeadlessTerminal recorded_inner(Size{40, 12}, initial);
    RecordingTerminal recorder(recorded_inner);
    ManualClock recorded_clock;
    ui::Application recorded_app(recorder, recorded_clock);
    auto recorded_probe = std::make_unique<ReplayFrameProbe>();
    recorded_probe->set_bounds(Rect{2, 2, 20, 1});
    recorded_app.root().add_child(std::move(recorded_probe));

    // The first frame is always produced from the baseline supplied at
    // construction. The second batch combines the terminal-sized resize and
    // refined presentation policy exactly as a live poll() may deliver them.
    recorded_app.step(0);
    recorded_inner.resize(Size{60, 20});
    Capabilities refined = initial;
    refined.color_depth = ColorDepth::Mono16;
    refined.color_scheme = ColorScheme::Dark;
    recorded_inner.inject_capability_change(refined);
    CK_CHECK(recorded_app.step(0));
    CK_CHECK(recorded_app.root().bounds() == (Rect{0, 0, 60, 20}));

    ReplayTerminal replay(recorder.recording(), recorder.initial_capabilities(), recorder.initial_size());
    ManualClock replay_clock;
    ui::Application replay_app(replay, replay_clock);
    auto replay_probe = std::make_unique<ReplayFrameProbe>();
    replay_probe->set_bounds(Rect{2, 2, 20, 1});
    replay_app.root().add_child(std::move(replay_probe));

    replay_app.step(0);
    replay_app.step(0);
    CK_CHECK(replay.size() == (Size{60, 20}));
    CK_CHECK(replay.capabilities() == refined);
    CK_CHECK(replay_app.root().bounds() == (Rect{0, 0, 60, 20}));
    CK_CHECK(replay.matches_recording());
}

CK_TEST(replay_reports_a_byte_or_operation_mismatch_against_the_original_recording) {
    std::vector<RecordedEntry> log;
    log.push_back(RecordedEntry{RecordedWrite{"original"}});
    log.push_back(RecordedEntry{RecordedEvents{}});
    ReplayTerminal replay(log, baseline_capabilities(), Size{10, 10});
    replay.write("changed");
    replay.poll(0);
    CK_CHECK(!replay.matches_recording());
}
