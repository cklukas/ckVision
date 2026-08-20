// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/headless_terminal.hpp"

#include "cvision/testing/cktest.hpp"

using namespace ckv;
using namespace ckv::term;

CK_TEST(curated_terminal_profiles_are_explicit_and_conservative) {
    const Capabilities modern = capabilities_for_profile(TerminalProfile::ModernVt);
    CK_CHECK(modern == baseline_capabilities());
    CK_CHECK(modern.mouse_protocol == MouseProtocol::SGR);
    CK_CHECK(modern.bracketed_paste);
    CK_CHECK(modern.focus_events);

    const Capabilities tmux = capabilities_for_profile(TerminalProfile::TmuxConservative);
    CK_CHECK(tmux.color_depth == ColorDepth::Color256);
    CK_CHECK(tmux.mouse_protocol == MouseProtocol::None);
    CK_CHECK(!tmux.bracketed_paste);
    CK_CHECK(!tmux.focus_events);
    CK_CHECK(!tmux.sixel_graphics);

    const Capabilities screen = capabilities_for_profile(TerminalProfile::ScreenConservative);
    CK_CHECK(screen.color_depth == ColorDepth::Mono16);
    CK_CHECK(screen.mouse_protocol == MouseProtocol::None);

    const Capabilities linux = capabilities_for_profile(TerminalProfile::LinuxConsole);
    CK_CHECK(linux.color_depth == ColorDepth::Mono16);
    CK_CHECK(linux.mouse_protocol == MouseProtocol::None);
    CK_CHECK(!linux.pixel_mouse);
}

CK_TEST(headless_curated_profiles_reject_unrequested_raw_capability_refinement) {
    constexpr TerminalProfile profiles[] = {
        TerminalProfile::TmuxConservative,
        TerminalProfile::ScreenConservative,
        TerminalProfile::LinuxConsole,
    };
    constexpr std::string_view probe_replies =
        "\x1B]11;rgb:0000/0000/0000\x07\x1B[?62;1;4;6c\x1B[?2026;1$y\x1B[?1016;1$y\x1B[6;16;8t";
    for (const TerminalProfile profile : profiles) {
        HeadlessTerminal term(Size{10, 10}, profile);
        const Capabilities expected = capabilities_for_profile(profile);

        term.inject_bytes(probe_replies, 1'000);
        CK_CHECK(term.poll(1'000).empty());
        CK_CHECK(term.capabilities() == expected);

        // Explicit scripting remains the deliberate, deterministic way to
        // model a host policy change in a headless test.
        Capabilities changed = expected;
        changed.color_scheme = ColorScheme::Dark;
        term.inject_capability_change(changed);
        const auto events = term.poll(1'001);
        CK_CHECK(events.size() == 1);
        CK_CHECK(std::get<CapabilityChangedEvent>(events.front()).capabilities == changed);
        CK_CHECK(term.capabilities() == changed);
    }
}

CK_TEST(headless_profile_changes_update_input_policy_before_later_scripted_bytes) {
    HeadlessTerminal term(Size{10, 10}, TerminalProfile::LinuxConsole);
    const Capabilities modern = capabilities_for_profile(TerminalProfile::ModernVt);
    constexpr std::string_view kModernVtFocusAndPaste = "\x1B[I\x1B[200~profile transition\x1B[201~";
    constexpr std::string_view kModernVtExtensions =
        "\x1B[I\x1B[200~profile transition\x1B[201~\x1B[<0;10;20M";

    // A trusted scripted change is an ordered terminal event, not a raw
    // capability reply. The public report and decoder must both observe it
    // before the later bytes are interpreted.
    term.inject_capability_change(modern);
    term.inject_bytes(kModernVtFocusAndPaste, 1'000);
    term.inject_timeout_check(1'000 + kPasteTerminationQuietNanos);
    term.inject_bytes("\x1B[<0;10;20M", 1'000 + kPasteTerminationQuietNanos + 1);
    const auto upgraded_events = term.poll(1'000);
    CK_CHECK(upgraded_events.size() == 4);
    CK_CHECK(std::get<CapabilityChangedEvent>(upgraded_events[0]).capabilities == modern);
    CK_CHECK(std::get<FocusEvent>(upgraded_events[1]).gained);
    const auto& pasted = std::get<TextEvent>(upgraded_events[2]);
    CK_CHECK(pasted.from_paste);
    CK_CHECK(pasted.text == "profile transition");
    const auto& mouse = std::get<MouseEvent>(upgraded_events[3]);
    CK_CHECK(mouse.action == MouseAction::Down);
    CK_CHECK(mouse.button == MouseButton::Left);
    CK_CHECK(mouse.cell == (Point{9, 19}));
    CK_CHECK(!mouse.pixel.has_value());

    const Capabilities conservative = capabilities_for_profile(TerminalProfile::LinuxConsole);
    term.inject_capability_change(conservative);
    term.inject_bytes(kModernVtExtensions, 1'001);
    const auto downgraded_events = term.poll(1'001);
    CK_CHECK(downgraded_events.size() == 1);
    CK_CHECK(std::get<CapabilityChangedEvent>(downgraded_events.front()).capabilities == conservative);
    CK_CHECK(term.capabilities() == conservative);
}

CK_TEST(headless_input_disconnect_recovers_a_partial_paste_without_key_events) {
    HeadlessTerminal term(Size{10, 10});
    term.inject_bytes("\x1B[200~partial\x1B[201~tail", 0);
    term.inject_input_disconnect();

    const auto events = term.poll(0);
    CK_CHECK(events.size() == 1);
    const auto& paste = std::get<TextEvent>(events.front());
    CK_CHECK(paste.from_paste);
    CK_CHECK(paste.paste_recovered);
    CK_CHECK(paste.text == "partialtail");
}

CK_TEST(rejected_raw_capability_replies_cannot_affect_later_bytes_in_the_same_feed) {
    // Probe replies and input can arrive in one read. A backend that disabled
    // refinement must reject the reply before the trailing SGR report is
    // decoded, rather than merely filtering its emitted capability event.
    Capabilities caps = baseline_capabilities();
    HeadlessTerminal term(Size{10, 10}, caps, /*enable_capability_probes=*/false);
    constexpr std::string_view late_replies_then_mouse =
        "\x1B[?1016;1$y\x1B[6;16;8t\x1B[<0;41;33M";

    term.inject_bytes(late_replies_then_mouse, 1'000);
    const auto events = term.poll(1'000);
    CK_CHECK(events.size() == 1);
    const auto mouse = std::get<MouseEvent>(events.front());
    CK_CHECK(mouse.cell == (Point{40, 32}));
    CK_CHECK(!mouse.pixel.has_value());
    CK_CHECK(term.capabilities() == caps);
}

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

CK_TEST(headless_terminal_requires_mode_2031_before_accepting_color_scheme_notifications) {
    HeadlessTerminal term(Size{10, 10});
    term.inject_bytes("\x1B[?997;2n", 0);
    CK_CHECK(term.poll(0).empty());
    CK_CHECK(term.capabilities().color_scheme == ColorScheme::Unknown);

    term.inject_bytes("\x1B[?2031;1$y\x1B[?997;2n", 1);
    const auto events = term.poll(1);
    CK_CHECK(events.size() == 2);
    CK_CHECK(term.capabilities().color_scheme_notifications);
    CK_CHECK(term.capabilities().color_scheme == ColorScheme::Light);
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

CK_TEST(capability_metric_refinement_updates_virtual_display_without_erasing_text) {
    HeadlessTerminal term(Size{2, 2}, headless_sixel_profile());
    term.write("\x1B[1;1HX\x1B[1;2H\x1BPq#0;2;100;0;0~\x1B\\");
    CK_CHECK(term.display().frame().at(Point{0, 0}).grapheme() == "X");
    CK_CHECK(term.display().has_raster_pixels());
    CK_CHECK(term.display().pixel_size() == (Size{18, 36}));

    Capabilities refined = term.capabilities();
    refined.cell_pixels = Size{8, 16};
    term.inject_capability_change(refined);

    const auto events = term.poll(0);
    CK_CHECK(events.size() == 1);
    CK_CHECK(std::get<CapabilityChangedEvent>(events[0]).capabilities.cell_pixels == (Size{8, 16}));
    CK_CHECK(term.display().cell_pixels() == (Size{8, 16}));
    CK_CHECK(term.display().pixel_size() == (Size{16, 32}));
    CK_CHECK(term.display().frame().at(Point{0, 0}).grapheme() == "X");
    CK_CHECK(!term.display().has_raster_pixels());
}

CK_TEST(capability_overrides_layer_client_policy_over_observed_terminal_evidence) {
    Capabilities observed = headless_sixel_profile();
    observed.sixel_color_registers = 256;
    observed.cell_pixels = Size{8, 16};
    HeadlessTerminal term(Size{20, 10}, observed);

    CapabilityOverrides overrides;
    overrides.sixel_graphics = false;
    overrides.cell_pixels = Size{9, 18};
    overrides.sixel_color_registers = 64;
    term.set_capability_overrides(overrides);

    const auto forced = term.poll(0);
    CK_CHECK(forced.size() == 1);
    const auto& forced_caps = std::get<CapabilityChangedEvent>(forced.front()).capabilities;
    CK_CHECK(!forced_caps.sixel_graphics);
    CK_CHECK(forced_caps.cell_pixels == (Size{9, 18}));
    CK_CHECK(forced_caps.sixel_color_registers == 64);
    CK_CHECK(term.display().cell_pixels() == (Size{9, 18}));

    // New probe evidence remains observed, while the explicit policy stays
    // effective until the client deliberately removes it.
    Capabilities refined = observed;
    refined.sixel_color_registers = 32;
    refined.cell_pixels = Size{7, 14};
    term.inject_capability_change(refined);
    const auto refined_events = term.poll(1);
    CK_CHECK(refined_events.size() == 1);
    const auto& refined_caps = std::get<CapabilityChangedEvent>(refined_events.front()).capabilities;
    CK_CHECK(!refined_caps.sixel_graphics);
    CK_CHECK(refined_caps.cell_pixels == (Size{9, 18}));
    CK_CHECK(refined_caps.sixel_color_registers == 32);

    term.set_capability_overrides({});
    const auto restored = term.poll(2);
    CK_CHECK(restored.size() == 1);
    CK_CHECK(std::get<CapabilityChangedEvent>(restored.front()).capabilities == refined);
    CK_CHECK(term.display().cell_pixels() == (Size{7, 14}));
}
