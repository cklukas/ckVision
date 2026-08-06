// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/headless_terminal.hpp"

#include "cktest.hpp"

using namespace ckv;
using namespace ckv::term;

CK_TEST(headless_terminal_reports_its_construction_size_and_capabilities) {
    Capabilities caps = baseline_capabilities();
    caps.color_depth = ColorDepth::Mono16;
    HeadlessTerminal term(Size{80, 24}, caps);
    CK_CHECK(term.size().width == 80 && term.size().height == 24);
    CK_CHECK(term.capabilities().color_depth == ColorDepth::Mono16);
}

CK_TEST(write_appends_to_a_readable_buffer) {
    HeadlessTerminal term(Size{10, 10});
    term.write("hello ");
    term.write("world");
    CK_CHECK(term.written_bytes() == "hello world");
    term.clear_written();
    CK_CHECK(term.written_bytes().empty());
}

CK_TEST(title_bell_and_clipboard) {
    Capabilities caps = baseline_capabilities();
    caps.clipboard_write = true;
    HeadlessTerminal term(Size{10, 10}, caps);
    term.set_title("my window");
    CK_CHECK(term.title() == "my window");
    CK_CHECK(term.bell_count() == 0);
    term.bell();
    term.bell();
    CK_CHECK(term.bell_count() == 2);
    term.write_clipboard("copied text");
    CK_CHECK(term.clipboard() == "copied text");
}

CK_TEST(clipboard_write_is_a_no_op_when_capability_absent) {
    HeadlessTerminal term(Size{10, 10});  // baseline: clipboard_write = false
    term.write_clipboard("should not stick");
    CK_CHECK(term.clipboard().empty());
}

CK_TEST(inject_bytes_feeds_the_decoder_and_poll_drains_it) {
    HeadlessTerminal term(Size{10, 10});
    CK_CHECK(term.poll(0).empty());  // nothing queued yet

    term.inject_bytes("Hi", 0);
    const auto events = term.poll(0);
    CK_CHECK(events.size() == 2);
    CK_CHECK(std::get<KeyEvent>(events[0]).chord.text == "H");

    // poll() drains: a second call sees nothing new.
    CK_CHECK(term.poll(0).empty());
}

CK_TEST(inject_event_bypasses_decoding) {
    HeadlessTerminal term(Size{10, 10});
    term.inject_event(TerminalEvent{FocusEvent{true}});
    const auto events = term.poll(0);
    CK_CHECK(events.size() == 1);
    CK_CHECK(std::get<FocusEvent>(events[0]).gained);
}

CK_TEST(resize_queues_a_resize_event_and_updates_size) {
    HeadlessTerminal term(Size{10, 10});
    term.resize(Size{20, 5});
    CK_CHECK(term.size() == (Size{20, 5}));
    const auto events = term.poll(0);
    CK_CHECK(events.size() == 1);
    CK_CHECK(std::get<ResizeEvent>(events[0]).cells == (Size{20, 5}));
}

CK_TEST(inject_timeout_check_resolves_a_pending_lone_esc) {
    HeadlessTerminal term(Size{10, 10});
    term.inject_bytes("\x1B", 1000);
    CK_CHECK(term.poll(0).empty());  // ambiguous, not yet resolved

    term.inject_timeout_check(1000 + kEscTimeoutNanos - 1);
    CK_CHECK(term.poll(0).empty());  // still too soon

    term.inject_timeout_check(1000 + kEscTimeoutNanos);
    const auto events = term.poll(0);
    CK_CHECK(events.size() == 1);
    CK_CHECK(std::get<KeyEvent>(events[0]).chord.key == Key::Escape);
}

CK_TEST(capability_changed_events_update_the_terminals_own_reported_capabilities) {
    HeadlessTerminal term(Size{10, 10});
    CK_CHECK(term.capabilities().color_scheme == ColorScheme::Unknown);
    term.inject_bytes("\x1B]11;rgb:0000/0000/0000\x07", 0);
    const auto events = term.poll(0);
    CK_CHECK(events.size() == 1);
    CK_CHECK(term.capabilities().color_scheme == ColorScheme::Dark);
}

CK_TEST(set_capabilities_propagates_to_the_internal_decoder) {
    HeadlessTerminal term(Size{10, 10});
    Capabilities caps = baseline_capabilities();
    caps.pixel_mouse = true;
    caps.cell_pixels = Size{8, 16};
    term.set_capabilities(caps);
    term.inject_bytes("\x1B[<0;9;17M", 0);  // SGR mouse; decoder should now compute pixel coords
    const auto events = term.poll(0);
    CK_CHECK(events.size() == 1);
    const auto mouse = std::get<MouseEvent>(events[0]);
    CK_CHECK(mouse.pixel.has_value());
    CK_CHECK(mouse.pixel->x == 8);
    CK_CHECK(mouse.pixel->y == 16);
}
