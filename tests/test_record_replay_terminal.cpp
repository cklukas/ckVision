// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/record_replay_terminal.hpp"

#include "cvision/term/headless_terminal.hpp"
#include "cktest.hpp"

using namespace ckv;
using namespace ckv::term;

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

CK_TEST(replay_captures_its_own_output_for_diffing_against_the_original_recording) {
    ReplayTerminal replay({}, baseline_capabilities(), Size{10, 10});
    replay.write("re-rendered output");
    CK_CHECK(replay.written_bytes() == "re-rendered output");
    replay.clear_written();
    CK_CHECK(replay.written_bytes().empty());
}

CK_TEST(end_to_end_record_then_replay_round_trip) {
    HeadlessTerminal inner(Size{10, 10});
    RecordingTerminal recorder(inner);
    inner.inject_event(TerminalEvent{KeyEvent{KeyChord{Key::Enter, Modifier::None, {}}}});
    const auto recorded_events = recorder.poll(0);
    CK_CHECK(recorded_events.size() == 1);

    ReplayTerminal replay(recorder.recording(), inner.capabilities(), inner.size());
    const auto replayed_events = replay.poll(0);
    CK_CHECK(replayed_events == recorded_events);
}
