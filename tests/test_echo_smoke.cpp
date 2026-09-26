// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The Echo example (examples/echo) driven headlessly with the raw host bytes
// of tools/docgen/echo_script.hpp: each beat is decoded by HeadlessTerminal's
// input decoder, dispatched by the Application, and must leave exactly the
// decoded lines below on the echo view. The frames the script opens and ends
// on are pinned in tests/golden/echo_*.dump, which generate_echo_goldens
// writes from the same script.
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../examples/example_about.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/testing/cktest.hpp"
#include "echo_script.hpp"

using ckv::docgen::EchoBeat;
using ckv::docgen::EchoSession;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool frame_is_pinned(const EchoSession& session, const std::string& file) {
    const std::string pinned = read_file("golden/" + file);
    return !pinned.empty() && ckv::golden::serialize(ckv::scene::capture(session.app.composed_surface(),
                                                                          session.app.current_cursor())) == pinned;
}

// The lines recorded since `before`, without their sequence numbers.
std::vector<std::string> lines_since(const EchoSession& session, std::size_t before) {
    std::vector<std::string> lines;
    const auto& kept = session.echo.view().lines();
    for (std::size_t index = before; index < kept.size(); ++index) lines.push_back(kept[index].substr(8));
    return lines;
}

// Plays the beat called `name` and returns the lines it added.
std::vector<std::string> play_beat(EchoSession& session, std::string_view name) {
    const std::size_t before = session.echo.view().lines().size();
    for (const EchoBeat& beat : ckv::docgen::echo_script())
        if (beat.name == name) ckv::docgen::play(session, beat);
    return lines_since(session, before);
}

bool screen_contains(const EchoSession& session, std::string_view needle) {
    const ckv::FrameView frame = session.terminal.display().frame();
    for (int y = 0; y < frame.size().height; ++y) {
        std::string row;
        for (int x = 0; x < frame.size().width; ++x) row += frame.at(ckv::Point{x, y}).grapheme();
        if (row.find(needle) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

CK_TEST(echo_opens_with_the_host_profile_its_size_and_its_focus) {
    EchoSession session;
    CK_CHECK(lines_since(session, 0) ==
             (std::vector<std::string>{"host    keyboard legacy, mouse SGR, focus reports on, bracketed paste on",
                                       "resize  80x24", "focus   gained"}));
    CK_CHECK(session.echo.view().lines().front() ==
             "     1  host    keyboard legacy, mouse SGR, focus reports on, bracketed paste on");
    CK_CHECK(session.app.focused() == &session.echo.view());
    CK_CHECK(screen_contains(session, "ckVision input echo"));
    CK_CHECK(screen_contains(session, "Alt+X quits"));
    CK_CHECK(screen_contains(session, ckv::examples::kCopyrightNotice));
    CK_CHECK(frame_is_pinned(session, "echo_initial.dump"));
}

CK_TEST(echo_decodes_characters_and_control_bytes_into_quoted_text_and_codepoints) {
    EchoSession session;
    CK_CHECK(play_beat(session, "typed") ==
             (std::vector<std::string>{"key     press   \"a\"  U+0061", "key     press   \"\xC3\xA9\"  U+00E9",
                                       "key     press   Ctrl+\"a\"  U+0061"}));
}

CK_TEST(echo_names_function_keys_and_keeps_tab_bound_to_traversal) {
    EchoSession session;
    // Tab and Shift+Tab reach the keymap after their lines are written;
    // traversal finds nothing but the echo view, so no focus line follows.
    CK_CHECK(play_beat(session, "named_keys") ==
             (std::vector<std::string>{"key     press   Up", "key     press   Ctrl+Right", "key     press   F1",
                                       "key     press   F5", "key     press   Backspace", "key     press   Enter",
                                       "key     press   Tab", "key     press   Shift+Tab"}));
    CK_CHECK(session.app.focused() == &session.echo.view());
}

CK_TEST(echo_tells_an_alt_chord_from_a_lone_escape_by_the_quiet_deadline) {
    EchoSession session;
    CK_CHECK(play_beat(session, "alt_chord") == (std::vector<std::string>{"key     press   Alt+\"g\"  U+0067"}));
    CK_CHECK(play_beat(session, "lone_escape") == (std::vector<std::string>{"key     press   Esc"}));
}

CK_TEST(echo_reports_sgr_mouse_presses_drags_wheel_and_modifiers_in_cells) {
    EchoSession session;
    CK_CHECK(play_beat(session, "mouse") ==
             (std::vector<std::string>{"mouse   down    left        cell 10,5", "mouse   move    left        cell 12,5",
                                       "mouse   up      left        cell 12,5",
                                       "mouse   wheel   wheel-down  cell 19,9",
                                       "mouse   down    left        cell 4,4  Ctrl+Shift",
                                       "mouse   up      left        cell 4,4  Ctrl+Shift"}));
}

CK_TEST(echo_shows_a_bracketed_paste_as_one_escaped_text_event) {
    EchoSession session;
    // The tab and the line feed survive paste sanitization and are shown
    // escaped; the carriage return is a C0 control the paste rule replaces
    // with U+FFFD (docs/input-decoder.md, D-040).
    CK_CHECK(play_beat(session, "paste") ==
             (std::vector<std::string>{"paste   23 bytes  \"hello\\tworld\xEF\xBF\xBD\\nline two\""}));
    CK_CHECK(session.app.clipboard_text() == "hello\tworld\xEF\xBF\xBD\nline two");
}

CK_TEST(echo_forwards_host_focus_reports_and_follows_a_resize) {
    EchoSession session;
    CK_CHECK(play_beat(session, "focus") == (std::vector<std::string>{"focus   lost", "focus   gained"}));
    CK_CHECK(play_beat(session, "resize") == (std::vector<std::string>{"resize  100x30"}));
}

CK_TEST(echo_after_the_whole_script_matches_the_pinned_frame) {
    EchoSession session;
    for (const EchoBeat& beat : ckv::docgen::echo_script()) ckv::docgen::play(session, beat);
    CK_CHECK(frame_is_pinned(session, "echo_scripted.dump"));
}

CK_TEST(echo_quits_on_the_standard_quit_chord_after_recording_it) {
    EchoSession session;
    session.terminal.inject_bytes("\x1Bx", session.clock.now_nanos());
    session.app.step(session.clock.now_nanos());
    CK_CHECK(session.echo.view().lines().back().substr(8) == "key     press   Alt+\"x\"  U+0078");
    CK_CHECK(session.app.quit_requested());
}

CK_TEST(echo_keeps_a_bounded_history_but_hands_every_line_to_its_observer) {
    ckv::term::HeadlessTerminal terminal{ckv::Size{80, 24}};
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    std::vector<std::string> observed;
    ckv::echo::EchoApp echo{app, [&observed](std::string_view line) { observed.emplace_back(line); }};
    app.step(0);
    for (std::size_t index = 0; index < ckv::echo::EchoView::kKeptLines + 5; ++index) echo.view().record("line");
    CK_CHECK(echo.view().lines().size() == ckv::echo::EchoView::kKeptLines);
    CK_CHECK(echo.view().recorded() == ckv::echo::EchoView::kKeptLines + 8);
    CK_CHECK(observed.size() == ckv::echo::EchoView::kKeptLines + 8);
    CK_CHECK(!observed.empty() && observed.front().substr(8, 4) == "host");
    CK_CHECK(echo.view().lines().back() == observed.back());
}
