// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/testing/cktest.hpp"

#include <fstream>
#include <iterator>
#include <limits>
#include <string>

#include "cvision/term/terminal_emulator.hpp"

using namespace ckv;

namespace {

std::string read_fixture(const char* name) {
    std::ifstream input(std::string(CKV_TEST_SOURCE_DIR) + "/fixtures/" + name, std::ios::binary);
    if (!input) return {};
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

CK_TEST(terminal_emulator_keeps_child_output_private_and_tracks_cells) {
    term::TerminalEmulator emulator;
    emulator.feed_output("hello\x1b[2;3Hworld");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    const Size expected_cells{80, 24};
    CK_CHECK(snapshot.cells == expected_cells);
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "h");
    CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(snapshot.cells.width + 2)].grapheme() == "w");
}

CK_TEST(terminal_emulator_uses_the_profile_default_terminal_colours_for_blank_and_reset_cells) {
    term::TerminalEmulator emulator;
    const auto profile = emulator.profile();
    CK_CHECK(emulator.snapshot().cell_buffer[0].style().fg == profile.default_style.fg);
    CK_CHECK(emulator.snapshot().cell_buffer[0].style().bg == profile.default_style.bg);
    emulator.feed_output("\x1b[31mred\x1b[0mreset");
    CK_CHECK(emulator.snapshot().cell_buffer[3].style() == profile.default_style);
}

CK_TEST(terminal_emulator_preserves_split_utf8_codepoints_across_child_reads) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\xc3");
    CK_CHECK(emulator.snapshot().cell_buffer[0].grapheme() == " ");
    emulator.feed_output("\xa9");
    CK_CHECK(emulator.snapshot().cell_buffer[0].grapheme() == "é");
}

CK_TEST(terminal_emulator_has_isolated_alternate_buffer) {
    term::TerminalEmulator emulator;
    emulator.feed_output("primary\x1b[?1049halt\x1b[?1049l");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(!snapshot.alternate_buffer);
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "p");
    CK_CHECK(snapshot.cursor.position == (Point{7, 0}));
}

CK_TEST(terminal_emulator_restores_saved_cursor_for_dec_and_xterm_alternate_screen_modes) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{8, 2};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("abc\x1b[?1049hfull\x1b[?1049l\x1b" "7\x1b[2;2H\x1b" "8");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(!snapshot.alternate_buffer);
    CK_CHECK(snapshot.cursor.position == (Point{3, 0}));
}

CK_TEST(terminal_emulator_recovers_after_bounded_control_string) {
    term::TerminalSubsessionOptions options;
    options.max_control_bytes = 4;
    term::TerminalEmulator emulator(term::embedded_xterm_sixel_profile(), options);
    emulator.feed_output("\x1b]0;oversize\x1b\\ok");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(!snapshot.diagnostics.empty());
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "o");
}

CK_TEST(terminal_emulator_keeps_discarding_an_oversized_control_after_nonterminating_escape) {
    term::TerminalSubsessionOptions options;
    options.max_control_bytes = 3;
    term::TerminalEmulator emulator(term::embedded_xterm_sixel_profile(), options);
    emulator.feed_output("\x1b]1234\x1bXignored\x1b\\ok");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(!snapshot.diagnostics.empty());
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "o");
}

CK_TEST(terminal_emulator_limits_and_drains_child_input) {
    term::TerminalSubsessionOptions options;
    options.max_input_bytes = 3;
    term::TerminalEmulator emulator(term::embedded_xterm_sixel_profile(), options);
    emulator.send_input("hello");
    CK_CHECK(emulator.take_pending_input() == "hel");
    CK_CHECK(!emulator.snapshot().diagnostics.empty());
}

CK_TEST(terminal_emulator_does_not_reenter_running_state_after_child_exit) {
    term::TerminalEmulator emulator;
    emulator.feed_output("before-exit");
    emulator.mark_exited(0);
    emulator.feed_output("late-output");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.state == term::TerminalSubsessionState::Exited);
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "b");
}

CK_TEST(terminal_emulator_defers_output_beyond_one_parser_budget_without_losing_the_tail) {
    term::TerminalSubsessionOptions options;
    options.max_parser_work_per_step = 3;
    options.max_output_bytes = 32;
    options.max_printable_run_bytes = 32;
    options.max_control_bytes = 32;
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{8, 1};
    term::TerminalEmulator emulator(profile, options);

    emulator.feed_output("abcdef");
    CK_CHECK(emulator.snapshot().cell_buffer[0].grapheme() == "a");
    CK_CHECK(emulator.snapshot().cell_buffer[2].grapheme() == "c");
    CK_CHECK(emulator.snapshot().cell_buffer[3].grapheme() == " ");
    emulator.feed_output({});
    CK_CHECK(emulator.snapshot().cell_buffer[3].grapheme() == "d");
    CK_CHECK(emulator.snapshot().cell_buffer[5].grapheme() == "f");
}

CK_TEST(terminal_emulator_reports_bounded_output_queue_overflow_and_recovers_at_a_sequence_boundary) {
    term::TerminalSubsessionOptions options;
    options.max_parser_work_per_step = 4;
    options.max_output_bytes = 4;
    term::TerminalEmulator emulator(term::embedded_xterm_sixel_profile(), options);
    emulator.feed_output("abcdefgh");
    emulator.feed_output("\x1b[2Jok");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(!snapshot.diagnostics.empty());
    CK_CHECK(snapshot.diagnostics.back().kind == term::TerminalDiagnostic::Kind::LimitExceeded);
    emulator.feed_output("ok");
    CK_CHECK(emulator.snapshot().cell_buffer[4].grapheme() == "o");
}

CK_TEST(terminal_emulator_resize_preserves_visible_cells_and_clamps_cursor) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{4, 2};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("abcd\r\nefgh\x1b[2;4H");
    emulator.resize(Size{3, 3}, Size{9, 18});
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    const Size expected_cells{3, 3};
    const Point expected_cursor{2, 1};
    CK_CHECK(snapshot.cells == expected_cells);
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "a");
    CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(snapshot.cells.width + 2)].grapheme() == "g");
    CK_CHECK(snapshot.cursor.position == expected_cursor);
}

CK_TEST(terminal_emulator_resize_preserves_active_child_input_modes) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{8, 2};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b[?1;1000;1004;1006;2004h");
    emulator.resize(Size{12, 3}, Size{10, 20});
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.application_cursor_keys);
    CK_CHECK(snapshot.focus_reporting_enabled);
    CK_CHECK(snapshot.mouse_reporting_enabled);
    CK_CHECK(snapshot.mouse_encoding == term::TerminalMouseEncoding::Sgr);
    CK_CHECK(snapshot.bracketed_paste_enabled);
}

CK_TEST(terminal_emulator_resize_rebuilds_private_raster_placement_without_stale_pixels) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{8, 4};
    profile.cell_pixels = Size{4, 6};
    term::TerminalEmulator emulator(profile);
    emulator.set_raster_identity(43);
    emulator.feed_output("\x1b[2;3H\x1bPq#0;2;100;0;0!8~-!8~\x1b\\");
    const term::TerminalSnapshot before = emulator.snapshot();
    CK_CHECK(before.rasters.size() == 1U);
    CK_CHECK(before.rasters[0].anchor == (Point{2, 1}));
    emulator.resize(Size{12, 6}, Size{8, 12});
    const term::TerminalSnapshot after = emulator.snapshot();
    CK_CHECK(after.rasters.size() == 1U);
    CK_CHECK(after.rasters[0].id == 43);
    CK_CHECK(after.rasters[0].anchor == (Point{2, 1}));
    CK_CHECK(after.rasters[0].cell_extent == (Size{1, 1}));
    CK_CHECK(after.rasters[0].image != nullptr);
}

CK_TEST(terminal_emulator_decodes_bounded_child_sixel_to_private_raster_state) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{2, 2};
    profile.cell_pixels = Size{4, 6};
    term::TerminalEmulator emulator(profile);
    emulator.set_raster_identity(41);
    emulator.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.rasters.size() == 1);
    CK_CHECK(snapshot.rasters[0].id == 41);
    CK_CHECK(snapshot.rasters[0].image != nullptr);
    const Image::Rgba pixel = snapshot.rasters[0].image->pixel(0, 0);
    CK_CHECK(pixel.r == 255);
    CK_CHECK(pixel.g == 0);
    CK_CHECK(pixel.b == 0);
    CK_CHECK(pixel.a == 255);
}

CK_TEST(terminal_emulator_accepts_parameterized_sixel_dcs_commands) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{2, 2};
    profile.cell_pixels = Size{4, 6};
    term::TerminalEmulator emulator(profile);
    emulator.set_raster_identity(77);
    emulator.feed_output("\x1bP0;1q#0;2;100;0;0~\x1b\\");
    CK_CHECK(emulator.snapshot().rasters.size() == 1U);
    CK_CHECK(emulator.snapshot().rasters[0].id == 77);
}

CK_TEST(terminal_emulator_decodes_the_published_raw_snake_sixel_fixture) {
    const std::string sixel = read_fixture("snake.six");
    CK_CHECK(sixel.size() > 200'000U);
    CK_CHECK(sixel.starts_with("\x1bP"));
    CK_CHECK(sixel.find('q') != std::string::npos);
    CK_CHECK(sixel.ends_with("\x1b\\"));

    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{80, 30};
    profile.cell_pixels = Size{8, 16};
    term::TerminalSubsessionOptions options;
    options.max_output_bytes = sixel.size() + 16;
    options.max_graphics_payload_bytes = sixel.size() + 16;
    options.max_parser_work_per_step = sixel.size() + 16;
    options.max_image_pixels = 80 * 30 * 8 * 16;
    term::TerminalEmulator emulator(profile, options);
    emulator.set_raster_identity(501);
    emulator.feed_output(sixel);

    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.rasters.size() == 1U);
    if (!snapshot.rasters.empty()) {
        CK_CHECK(snapshot.rasters[0].id == 501);
        CK_CHECK(snapshot.rasters[0].image != nullptr);
        CK_CHECK(snapshot.rasters[0].image->width() > 0);
        CK_CHECK(snapshot.rasters[0].image->height() > 0);
    }
    CK_CHECK(snapshot.diagnostics.empty());
}

CK_TEST(terminal_emulator_allows_a_bounded_sixel_payload_larger_than_control_strings) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{2, 2};
    profile.cell_pixels = Size{4, 6};
    term::TerminalEmulator emulator(profile);
    emulator.set_raster_identity(78);
    std::string sixel = "\x1bPq#0;2;100;0;0";
    sixel.append(4000, '~');
    sixel += "\x1b\\";
    emulator.feed_output(sixel);
    const auto snapshot = emulator.snapshot();
    CK_CHECK(snapshot.rasters.size() == 1U);
    CK_CHECK(snapshot.rasters[0].id == 78);
}

CK_TEST(terminal_emulator_crops_child_sixel_to_its_visible_cell_footprint) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{16, 8};
    profile.cell_pixels = Size{4, 6};
    term::TerminalEmulator emulator(profile);
    emulator.set_raster_identity(42);
    emulator.feed_output("\x1b[2;3H\x1bPq#0;2;100;0;0!8~-!8~\x1b\\");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.rasters.size() == 1U);
    CK_CHECK(snapshot.rasters[0].anchor == (Point{2, 1}));
    CK_CHECK(snapshot.rasters[0].cell_extent == (Size{2, 2}));
    CK_CHECK(snapshot.rasters[0].image->width() == 8);
    CK_CHECK(snapshot.rasters[0].image->height() == 12);
}

CK_TEST(terminal_emulator_clear_removes_child_raster_before_next_snapshot) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{2, 2};
    profile.cell_pixels = Size{4, 6};
    term::TerminalEmulator emulator(profile);
    emulator.set_raster_identity(9);
    emulator.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");
    CK_CHECK(emulator.snapshot().rasters.size() == 1);
    emulator.feed_output("\x1b[2J");
    CK_CHECK(emulator.snapshot().rasters.empty());
}

CK_TEST(terminal_emulator_rejects_a_child_sixel_picture_over_its_declared_pixel_limit) {
    // The limit is on the picture, which is the thing a child controls. It
    // used to be applied to the terminal's own plane, so the same small
    // picture was accepted or dropped depending on how large the reader had
    // made their window — and a large window had no graphics at all.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{80, 24};
    profile.cell_pixels = Size{8, 16};
    term::TerminalSubsessionOptions options;
    options.max_image_pixels = 95;  // the picture below is 16 x 6 = 96 pixels
    term::TerminalEmulator emulator(profile, options);
    emulator.feed_output("\x1bPq#0;2;100;0;0!16~\x1b\\");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.rasters.empty());
    CK_CHECK(!snapshot.diagnostics.empty());
    // And it says which picture and which limit, so the reader can act on it.
    CK_CHECK(snapshot.diagnostics.back().message.find("16x6") != std::string::npos);
    CK_CHECK(snapshot.diagnostics.back().message.find("95") != std::string::npos);
}

CK_TEST(terminal_emulator_decodes_a_picture_at_its_own_size_not_the_windows) {
    // The same picture in a small terminal and in a large one is the same
    // picture: decoding it into a plane the size of the window made a logo
    // cost twenty times more in a full-screen window than in a small one.
    const std::string picture = "\x1bPq#0;2;100;0;0!24~-!24~\x1b\\";
    for (const Size cells : {Size{20, 6}, Size{200, 60}}) {
        term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
        profile.cells = cells;
        profile.cell_pixels = Size{8, 16};
        term::TerminalEmulator emulator(profile);
        emulator.feed_output(picture);
        const term::TerminalSnapshot snapshot = emulator.snapshot();
        CK_CHECK(snapshot.rasters.size() == 1U);
        CK_CHECK(snapshot.rasters[0].image->width() == 24);
        CK_CHECK(snapshot.rasters[0].image->height() == 12);
        CK_CHECK(snapshot.diagnostics.empty());
    }
}

CK_TEST(terminal_emulator_cuts_a_picture_off_at_the_edge_of_the_screen) {
    // Wider than the screen is not too wide to show: a terminal draws what
    // fits and stops, which is also all that can ever be seen — a picture is
    // anchored to the cell it started on and cannot be scrolled sideways.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{10, 4};
    profile.cell_pixels = Size{8, 16};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1bPq#0;2;100;0;0!10000~\x1b\\");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.rasters.size() == 1U);
    CK_CHECK(snapshot.rasters[0].image->width() == 80);  // 10 cells of 8 px
    CK_CHECK(snapshot.diagnostics.empty());
}

CK_TEST(terminal_emulator_advertises_graphics_however_large_the_window_is) {
    // A window of any size can be drawn in, because what is decoded is the
    // picture and not the window. This once answered "no graphics" for a
    // window past the pixel budget, which is how a maximised terminal came to
    // have no pictures in it at all.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{300, 100};
    profile.cell_pixels = Size{16, 48};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b[c\x1b[?2;4;0S");
    CK_CHECK(emulator.take_pending_input() == "\x1b[?1;2;4c\x1b[?2;0;4800;4800S");
    emulator.feed_output("\x1bPq#0;2;100;0;0!32~\x1b\\");
    CK_CHECK(emulator.snapshot().rasters.size() == 1U);
    CK_CHECK(emulator.snapshot().diagnostics.empty());
}

CK_TEST(text_written_over_a_child_picture_erases_it_the_way_a_terminal_does) {
    // Reported from a running ckmux: a dialog with a logo in it was closed,
    // the program repainted those cells with text, and the picture stayed on
    // screen on top of the new text. A Sixel is pixels on the screen, and
    // writing over the cells it covers is what takes them off again.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{20, 6};
    profile.cell_pixels = Size{8, 16};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1bPq#0;2;100;0;0!24~-!24~\x1b\\");
    CK_CHECK(emulator.snapshot().rasters.size() == 1U);
    const Size covered = emulator.snapshot().rasters[0].cell_extent;
    CK_CHECK(covered.width == 3 && covered.height == 1);

    // One cell of it, and the rest of the picture is still there.
    emulator.feed_output("\x1b[1;1Hx");
    CK_CHECK(emulator.snapshot().rasters.size() == 1U);
    // The rest of its cells, and it is gone entirely.
    emulator.feed_output("xx");
    CK_CHECK(emulator.snapshot().rasters.empty());
}

CK_TEST(erasing_the_cells_under_a_child_picture_removes_it) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{20, 6};
    profile.cell_pixels = Size{8, 16};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b[3;1H\x1bPq#0;2;100;0;0!24~\x1b\\");
    CK_CHECK(emulator.snapshot().rasters.size() == 1U);
    emulator.feed_output("\x1b[3;1H\x1b[K");  // erase that line
    CK_CHECK(emulator.snapshot().rasters.empty());
}

CK_TEST(two_child_pictures_side_by_side_are_both_kept) {
    // A nested ckVision emits one Sixel per visible slice when a window
    // overlaps its picture. Keeping only the last one showed a fragment of
    // the image and nothing else.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{20, 6};
    profile.cell_pixels = Size{8, 16};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b[1;1H\x1bPq#0;2;100;0;0!8~\x1b\\");
    emulator.feed_output("\x1b[3;5H\x1bPq#0;2;0;100;0!8~\x1b\\");
    CK_CHECK(emulator.snapshot().rasters.size() == 2U);
    CK_CHECK(emulator.snapshot().rasters[0].anchor == (Point{0, 0}));
    CK_CHECK(emulator.snapshot().rasters[1].anchor == (Point{4, 2}));
}

CK_TEST(a_child_picture_drawn_over_another_replaces_what_it_covers) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{20, 6};
    profile.cell_pixels = Size{8, 16};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b[1;1H\x1bPq#0;2;100;0;0!8~\x1b\\");
    emulator.feed_output("\x1b[1;1H\x1bPq#0;2;0;100;0!8~\x1b\\");
    CK_CHECK(emulator.snapshot().rasters.size() == 1U);
}

CK_TEST(a_child_picture_scrolls_with_the_text_it_was_drawn_beside) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{20, 6};
    profile.cell_pixels = Size{8, 16};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b[3;1H\x1bPq#0;2;100;0;0!8~\x1b\\");
    CK_CHECK(emulator.snapshot().rasters.size() == 1U);
    CK_CHECK(emulator.snapshot().rasters[0].anchor.y == 2);
    emulator.feed_output("\x1b[6;1H\n");  // one scroll of the whole screen
    CK_CHECK(emulator.snapshot().rasters.size() == 1U);
    CK_CHECK(emulator.snapshot().rasters[0].anchor.y == 1);
    // ...and off the top, where a picture anchored to a cell cannot follow.
    emulator.feed_output("\n\n");
    CK_CHECK(emulator.snapshot().rasters.empty());
}

CK_TEST(terminal_emulator_survives_an_absurd_cell_metric_without_overflowing) {
    // A cell metric is something a host reports, and a host can report
    // nonsense. It used to decide the size of the plane a picture was decoded
    // into, so nonsense there had to be refused before it was multiplied out.
    // A picture is now decoded at its own size and this cannot reach an
    // allocation at all — but every pixel bound derived from the metric is
    // still computed in 64 bits, and this is what says so.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{2, 2};
    profile.cell_pixels = Size{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.rasters.size() == 1U);
    CK_CHECK(snapshot.rasters[0].image->width() == 1);
    CK_CHECK(snapshot.rasters[0].cell_extent == (Size{1, 1}));
    // ...and writing over it still takes it away, bounds and all.
    emulator.feed_output("\x1b[1;1Hx");
    CK_CHECK(emulator.snapshot().rasters.empty());
}

CK_TEST(terminal_emulator_recovers_from_malformed_child_sixel_without_leaking_state_to_another_session) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{2, 2};
    profile.cell_pixels = Size{4, 6};
    term::TerminalEmulator first(profile);
    term::TerminalEmulator second(profile);
    first.set_raster_identity(1);
    second.set_raster_identity(2);

    first.feed_output("\x1bPq#0;2;100;0;0\x1b\\ok");
    second.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");

    CK_CHECK(first.snapshot().rasters.empty());
    CK_CHECK(!first.snapshot().diagnostics.empty());
    CK_CHECK(first.snapshot().cell_buffer[0].grapheme() == "o");
    CK_CHECK(second.snapshot().rasters.size() == 1U);
    CK_CHECK(second.snapshot().rasters[0].id == 2);
}

CK_TEST(terminal_emulator_retains_bounded_primary_scrollback) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{2, 2};
    term::TerminalSubsessionOptions options;
    options.max_scrollback_lines = 1;
    term::TerminalEmulator emulator(profile, options);
    emulator.feed_output("aa\r\nbb\r\ncc");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.scrollback.size() == 2);
    CK_CHECK(snapshot.scrollback[0].grapheme() == "a");
}

CK_TEST(terminal_emulator_keeps_no_history_at_all_when_scrollback_capacity_is_zero) {
    // "Remember nothing" is a real request, not merely the smallest number:
    // a shell that has just read a password, or a host with a memory budget.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{2, 2};
    term::TerminalSubsessionOptions options;
    options.max_scrollback_lines = 0;
    term::TerminalEmulator emulator(profile, options);
    emulator.feed_output("aa\r\nbb\r\ncc\r\ndd");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.scrollback.empty());
    // The screen itself still scrolls; only the history is refused.
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "c");
}

CK_TEST(terminal_emulator_relays_stored_history_lines_when_the_width_changes) {
    // The history is flat rows of the grid's width. Without re-laying them a
    // resize leaves every stored line read at the wrong offset, and what the
    // reader scrolls back to is not what was ever on their screen.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{4, 2};
    term::TerminalSubsessionOptions options;
    options.max_scrollback_lines = 8;
    term::TerminalEmulator emulator(profile, options);
    emulator.feed_output("ab\r\ncd\r\nef\r\ngh");
    CK_CHECK(emulator.snapshot().scrollback.size() == 8U);  // two lines of four

    emulator.resize(Size{6, 2}, Size{9, 18});
    const term::TerminalSnapshot wider = emulator.snapshot();
    CK_CHECK(wider.scrollback.size() == 12U);  // still two lines, now of six
    CK_CHECK(wider.scrollback[0].grapheme() == "a");
    CK_CHECK(wider.scrollback[1].grapheme() == "b");
    CK_CHECK(wider.scrollback[6].grapheme() == "c");  // the second line starts here
    CK_CHECK(wider.scrollback[7].grapheme() == "d");

    emulator.resize(Size{1, 2}, Size{9, 18});
    const term::TerminalSnapshot narrower = emulator.snapshot();
    CK_CHECK(narrower.scrollback.size() == 2U);  // two lines of one
    CK_CHECK(narrower.scrollback[0].grapheme() == "a");
    CK_CHECK(narrower.scrollback[1].grapheme() == "c");
}

CK_TEST(terminal_emulator_survives_repeated_resize_and_noisy_scrollback_cycles) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{40, 12};
    profile.cell_pixels = Size{8, 16};
    term::TerminalSubsessionOptions options;
    options.max_scrollback_lines = 64;
    options.max_output_bytes = 64 * 1024;
    options.max_parser_work_per_step = 64 * 1024;
    options.max_printable_run_bytes = 4 * 1024;
    term::TerminalEmulator emulator(profile, options);

    for (int cycle = 0; cycle < 128; ++cycle) {
        std::string output;
        for (int line = 0; line < 20; ++line)
            output += "noise-" + std::to_string(cycle) + "-" + std::to_string(line) + "\r\n";
        emulator.feed_output(output);
        const Size cells = cycle % 2 == 0 ? Size{40, 12} : Size{53, 15};
        emulator.resize(cells, Size{8 + (cycle % 3), 16});
        const term::TerminalSnapshot snapshot = emulator.snapshot();
        CK_CHECK(snapshot.cells == cells);
        CK_CHECK(snapshot.cell_buffer.size() == static_cast<std::size_t>(cells.width * cells.height));
        CK_CHECK(snapshot.scrollback.size() <= options.max_scrollback_lines * 53U);
        CK_CHECK(snapshot.diagnostics.size() <= 64U);
    }
}

CK_TEST(terminal_emulator_keeps_scrolling_regions_private_and_preserves_their_outside_rows) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{4, 4};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("A0\r\nB1\r\nC2\r\nD3\x1b[2;3r\x1b[3;1HZ\n");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "A");
    CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(profile.cells.width)].grapheme() == "Z");
    CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(2 * profile.cells.width)].grapheme() == " ");
    CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(3 * profile.cells.width)].grapheme() == "D");
    CK_CHECK(snapshot.scrollback.empty());
}

CK_TEST(terminal_emulator_honours_origin_mode_and_reverse_index_inside_the_scrolling_region) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{3, 4};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("abc\r\ndef\r\nghi\r\njkl\x1b[2;3r\x1b[?6h\x1b[H\x1bM");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "a");
    CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(profile.cells.width)].grapheme() == " ");
    CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(2 * profile.cells.width)].grapheme() == "d");
    CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(3 * profile.cells.width)].grapheme() == "j");
}

CK_TEST(terminal_emulator_applies_character_insert_delete_and_erase_privately) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{6, 1};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("abcdef\x1b[1;3H\x1b[2P\x1b[1@\x1b[X");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "a");
    CK_CHECK(snapshot.cell_buffer[1].grapheme() == "b");
    CK_CHECK(snapshot.cell_buffer[2].grapheme() == " ");
    CK_CHECK(snapshot.cell_buffer[3].grapheme() == "e");
    CK_CHECK(snapshot.cell_buffer[4].grapheme() == "f");
}

CK_TEST(terminal_emulator_applies_ansi_sgr_colours_to_private_cells) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[31;44mX");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    const Cell& cell = snapshot.cell_buffer[0];
    // The palette entries the program named, still named: `31` is the
    // palette's red rather than one particular red.
    CK_CHECK(cell.style().fg == Color::indexed(1));
    CK_CHECK(cell.style().bg == Color::indexed(4));
}

CK_TEST(terminal_emulator_applies_extended_sgr_colours_without_a_child_diagnostic) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[38;2;1;2;3;48;5;251mX");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    const Cell& cell = snapshot.cell_buffer[0];
    CK_CHECK(cell.style().fg == Color::rgb(1, 2, 3));
    CK_CHECK(cell.style().bg == Color::indexed(251));
    CK_CHECK(snapshot.diagnostics.empty());
}

CK_TEST(terminal_emulator_consumes_nested_capability_queries_at_the_private_boundary) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[?2026$p\x1b[?2031$p\x1b[c\x1b[?1016$p\x1b[16t");
    CK_CHECK(emulator.snapshot().diagnostics.empty());
    CK_CHECK(emulator.take_pending_input() == "\x1b[?1;2;4c\x1b[6;18;9t");
}

CK_TEST(terminal_emulator_advertises_declared_graphics_in_device_attributes) {
    // Parameter 4 is what a Sixel-emitting program looks for; without it the
    // picture is never sent, however well the emulator would decode it.
    term::TerminalEmulator with_graphics;
    with_graphics.feed_output("\x1b[c");
    CK_CHECK(with_graphics.take_pending_input() == "\x1b[?1;2;4c");

    term::TerminalCapabilityProfile text_only = term::embedded_xterm_sixel_profile();
    text_only.sixel = false;
    term::TerminalEmulator without_graphics(text_only);
    without_graphics.feed_output("\x1b[c");
    CK_CHECK(without_graphics.take_pending_input() == "\x1b[?1;2c");
}

CK_TEST(terminal_emulator_reports_graphics_limits_instead_of_scrolling_the_child) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{4, 3};
    profile.cell_pixels = Size{10, 20};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("ab\x1b[?1;4;0S\x1b[?2;4;0S");
    // The registers the decoder holds, then the child's own window in pixels:
    // a Sixel reaching past the last cell is cropped and never seen.
    CK_CHECK(emulator.take_pending_input() == "\x1b[?1;0;256S\x1b[?2;0;40;60S");
    // XTSMGRAPHICS and SU share their final byte. Read as SU, both probes
    // would have scrolled this line off the top of the child's screen.
    CK_CHECK(emulator.snapshot().cell_buffer[0].grapheme() == "a");
    CK_CHECK(emulator.snapshot().diagnostics.empty());
}

CK_TEST(terminal_emulator_answers_graphics_requests_it_will_not_grant) {
    term::TerminalEmulator emulator;
    // Setting a limit, ReGIS geometry, an unknown item, an unknown action:
    // each refused, none left unanswered, because the child is waiting.
    emulator.feed_output("\x1b[?1;3;16S\x1b[?3;1;0S\x1b[?7;1;0S\x1b[?1;9;0S");
    CK_CHECK(emulator.take_pending_input() == "\x1b[?1;3;0S\x1b[?3;3;0S\x1b[?7;1;0S\x1b[?1;2;0S");

    term::TerminalCapabilityProfile text_only = term::embedded_xterm_sixel_profile();
    text_only.sixel = false;
    term::TerminalEmulator without_graphics(text_only);
    without_graphics.feed_output("\x1b[?2;4;0S");
    CK_CHECK(without_graphics.take_pending_input() == "\x1b[?2;3;0S");
}

CK_TEST(terminal_emulator_tracks_declared_child_input_modes) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[?1;1004;2004h\x1b[?1000;1006h");
    CK_CHECK(emulator.snapshot().application_cursor_keys);
    CK_CHECK(emulator.snapshot().focus_reporting_enabled);
    CK_CHECK(emulator.snapshot().bracketed_paste_enabled);
    CK_CHECK(emulator.snapshot().mouse_reporting_enabled);
    CK_CHECK(emulator.snapshot().mouse_encoding == term::TerminalMouseEncoding::Sgr);
    emulator.feed_output("\x1b[?1;1004;2004l\x1b[?1000;1006l");
    CK_CHECK(!emulator.snapshot().application_cursor_keys);
    CK_CHECK(!emulator.snapshot().focus_reporting_enabled);
    CK_CHECK(!emulator.snapshot().bracketed_paste_enabled);
    CK_CHECK(!emulator.snapshot().mouse_reporting_enabled);
    CK_CHECK(emulator.snapshot().mouse_encoding == term::TerminalMouseEncoding::None);
}

CK_TEST(terminal_emulator_answers_declared_cursor_query_in_private_input_queue) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[3;5H\x1b[5n\x1b[6n");
    CK_CHECK(emulator.take_pending_input() == "\x1b[0n\x1b[3;5R");
}

CK_TEST(terminal_emulator_applies_explicit_child_osc_title_policy) {
    term::TerminalCapabilityProfile denied = term::embedded_xterm_sixel_profile();
    denied.osc_policy = term::TerminalOscPolicy::Deny;
    term::TerminalEmulator deny_emulator(denied);
    deny_emulator.feed_output("\x1b]0;untrusted\a");
    CK_CHECK(deny_emulator.snapshot().title.empty());

    term::TerminalCapabilityProfile stored = term::embedded_xterm_sixel_profile();
    stored.osc_policy = term::TerminalOscPolicy::StoreMetadata;
    term::TerminalEmulator store_emulator(stored);
    store_emulator.feed_output("\x1b]0;private title\x1b\\");
    CK_CHECK(store_emulator.snapshot().title == "private title");
}

CK_TEST(terminal_emulator_preserves_shell_tab_layout_and_cursor_style_without_a_diagnostic) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{16, 1};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("left\tright\x1b[6 q");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "l");
    CK_CHECK(snapshot.cell_buffer[8].grapheme() == "r");
    CK_CHECK(snapshot.cursor.shape == CursorShape::Bar);
    CK_CHECK(snapshot.diagnostics.empty());
}

CK_TEST(terminal_emulator_recovers_incomplete_output_when_child_exits) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\xc3\x1b[");
    emulator.mark_exited(0);
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.state == term::TerminalSubsessionState::Exited);
    CK_CHECK(!snapshot.diagnostics.empty());
}

CK_TEST(terminal_emulator_reports_a_child_exit_mid_sixel_without_publishing_partial_pixels) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{8, 4};
    profile.cell_pixels = Size{8, 16};
    term::TerminalEmulator emulator(profile);
    emulator.set_raster_identity(502);
    emulator.feed_output("\x1bPq#0;2;100;0;0!32~");
    emulator.mark_exited(1);

    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.state == term::TerminalSubsessionState::Exited);
    CK_CHECK(snapshot.rasters.empty());
    CK_CHECK(!snapshot.diagnostics.empty());
    CK_CHECK(snapshot.diagnostics.back().kind == term::TerminalDiagnostic::Kind::MalformedSequence);
}

// --- Designated character sets --------------------------------------------

CK_TEST(a_designated_line_drawing_set_turns_letters_into_frame_pieces) {
    // What ncurses actually sends to draw a box: designate the DEC special
    // graphics set, send the letters, designate ASCII again. Read literally
    // these spell "lqqk", which is what an emulator ignoring the
    // designation puts on screen.
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b(0lqqk\x1b(B");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "\u250c");
    CK_CHECK(snapshot.cell_buffer[1].grapheme() == "\u2500");
    CK_CHECK(snapshot.cell_buffer[2].grapheme() == "\u2500");
    CK_CHECK(snapshot.cell_buffer[3].grapheme() == "\u2510");
}

CK_TEST(the_designation_itself_never_reaches_the_screen) {
    // The failure this replaced printed the final byte of the designation
    // as text, so a frame corner arrived as "0lqqB".
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b(0x\x1b(BWarning");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "\u2502");
    CK_CHECK(snapshot.cell_buffer[1].grapheme() == "W");
    CK_CHECK(snapshot.cell_buffer[2].grapheme() == "a");
}

CK_TEST(ascii_is_restored_by_designation_so_later_text_reads_normally) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b(0q\x1b(Bq");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "\u2500");
    CK_CHECK(snapshot.cell_buffer[1].grapheme() == "q");  // the same byte, now itself
}

CK_TEST(shift_out_and_shift_in_move_between_the_designated_sets) {
    // G1 carries the line-drawing set and SO/SI switch to it and back
    // without redesignating — the other way a curses program draws frames.
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b)0" "A" "\x0e" "q" "\x0f" "B");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "A");
    CK_CHECK(snapshot.cell_buffer[1].grapheme() == "\u2500");
    CK_CHECK(snapshot.cell_buffer[2].grapheme() == "B");
}

// --- REP -------------------------------------------------------------------

CK_TEST(a_repeat_draws_the_whole_run_a_curses_program_asked_for) {
    // How a rule is actually sent: one character and a count. Without REP
    // only the first cell of every rule appears, so a frame arrives with
    // its corners drawn and its edges missing.
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b(0q\x1b[4b");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    for (int i = 0; i < 5; ++i)
        CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(i)].grapheme() == "\u2500");
    CK_CHECK(snapshot.cell_buffer[5].grapheme() == " ");
}

CK_TEST(a_repeat_carries_the_style_the_run_was_started_in) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[1m-\x1b[3b");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    for (int i = 0; i < 4; ++i) {
        CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(i)].grapheme() == "-");
        CK_CHECK(has_attr(snapshot.cell_buffer[static_cast<std::size_t>(i)].style().attrs, ckv::Attr::Bold));
    }
}

CK_TEST(a_repeat_with_no_count_repeats_once) {
    term::TerminalEmulator emulator;
    emulator.feed_output("x\x1b[b");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "x");
    CK_CHECK(snapshot.cell_buffer[1].grapheme() == "x");
    CK_CHECK(snapshot.cell_buffer[2].grapheme() == " ");
}

CK_TEST(a_repeat_before_anything_was_printed_does_nothing) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[5b");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == " ");
}

CK_TEST(an_absurd_repeat_count_is_bounded_by_the_screen) {
    // The count comes from the child, so it cannot be trusted to be sane.
    term::TerminalEmulator emulator;
    emulator.feed_output("z\x1b[999999999b");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cells.width == 80);  // survived, and still coherent
}

// --- Bold and the low eight colours ---------------------------------------

CK_TEST(a_real_curses_frame_keeps_its_bars_plain_and_its_body_below_white) {
    // Bytes recorded from ncdu against a real pty. Emphasis has to be
    // visibly brighter than body text, not a shade above it: the whole
    // point of the convention is that a reader can see the difference.
    term::TerminalEmulator emulator;
    emulator.feed_output(read_fixture("ncdu_frame.raw"));
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    int bold_white = 0;
    int body = 0;
    int reversed_bar = 0;
    int printed = 0;
    for (const Cell& cell : snapshot.cell_buffer) {
        if (cell.grapheme() == " ") continue;
        ++printed;
        const ckv::Style style = cell.style();
        if (has_attr(style.attrs, ckv::Attr::Reverse)) {
            // ncdu's header and footer bars. Their colours must be the plain
            // pair whichever way round they are drawn -- brightening one of
            // them puts a pale patch behind the emphasised words.
            if (style.fg == ckv::Color::rgb(187, 187, 187)) ++reversed_bar;
        } else if (has_attr(style.attrs, ckv::Attr::Bold)) {
            if (style.fg == ckv::Color::rgb(255, 255, 255)) ++bold_white;
        } else if (style.fg == ckv::Color::rgb(187, 187, 187)) {
            ++body;
        }
    }
    // This frame is ncdu's browse view, where every emphasised word lives
    // inside a reversed bar — which is exactly the case that used to come
    // out wrong, so it is the one worth pinning against real bytes. Bold
    // against a normal background is covered synthetically below.
    CK_CHECK(bold_white == 0);
    CK_CHECK(body > 0);
    CK_CHECK(reversed_bar > 0);
    // Every printed cell is accounted for: nothing fell through as an
    // unbrightened bold or an unexpected colour.
    CK_CHECK(bold_white + body + reversed_bar == printed);
}

CK_TEST(bold_on_the_default_foreground_arrives_white_not_merely_heavier) {
    // ncurses asks for bold against the default foreground and every other
    // terminal answers with white. Emitting the same grey with a weight
    // attribute is a visibly different thing.
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[1mB");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().fg == ckv::Color::indexed(15));
    CK_CHECK(has_attr(snapshot.cell_buffer[0].style().attrs, ckv::Attr::Bold));
}

CK_TEST(bold_brightens_each_of_the_low_eight_to_its_own_bright_form) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[1;31mR\x1b[1;34mB");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().fg == ckv::Color::indexed(9));   // bright red
    CK_CHECK(snapshot.cell_buffer[1].style().fg == ckv::Color::indexed(12));  // bright blue
}

CK_TEST(bold_leaves_a_colour_the_program_chose_explicitly_alone) {
    // A true-colour foreground is a specific colour the program picked;
    // brightening it would override the program rather than honour a
    // convention it was written for. That the picked value happens to be the
    // palette's own red makes no difference — before a colour kept its index
    // the two were indistinguishable, and this one was brightened too.
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[1;38;2;10;20;30mX\x1b[1;38;2;205;49;49mY");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().fg == ckv::Color::rgb(10, 20, 30));
    CK_CHECK(snapshot.cell_buffer[1].style().fg == ckv::Color::rgb(205, 49, 49));
}

CK_TEST(bold_brightens_a_low_palette_index_however_it_was_spelled) {
    // `SGR 31` and `SGR 38;5;1` are the same request — the palette's first
    // colour — so the convention that brightens one brightens the other.
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[1;38;5;1mR");
    CK_CHECK(emulator.snapshot().cell_buffer[0].style().fg == ckv::Color::indexed(9));
}

CK_TEST(text_that_is_not_bold_keeps_the_colour_it_asked_for) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[31mR");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().fg == ckv::Color::indexed(1));  // plain red
}

CK_TEST(osc_two_sets_the_title_the_same_way_osc_zero_does) {
    // OSC 2 is the ordinary "set window title": shells and editors emit it
    // constantly. Recognising only OSC 0 left a host's caption frozen at
    // whatever it started as under most programs.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.osc_policy = ckv::core::TerminalOscPolicy::StoreMetadata;
    ckv::term::TerminalEmulator emulator(profile);

    emulator.feed_output("\x1b]0;first\a");
    CK_CHECK(emulator.snapshot().title == "first");
    emulator.feed_output("\x1b]2;second\a");
    CK_CHECK(emulator.snapshot().title == "second");
    // The string-terminator spelling is equally ordinary and must behave the
    // same as BEL.
    emulator.feed_output("\x1b]2;third\x1b\\");
    CK_CHECK(emulator.snapshot().title == "third");
    // An empty title is a real value: it is how a program hands the caption
    // back rather than leaving its own stale text there.
    emulator.feed_output("\x1b]2;\a");
    CK_CHECK(emulator.snapshot().title.empty());
}

CK_TEST(osc_one_sets_the_icon_name_only_and_leaves_the_title_alone) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.osc_policy = ckv::core::TerminalOscPolicy::StoreMetadata;
    ckv::term::TerminalEmulator emulator(profile);

    emulator.feed_output("\x1b]2;kept\a");
    emulator.feed_output("\x1b]1;icon name\a");
    CK_CHECK(emulator.snapshot().title == "kept");
}

CK_TEST(a_title_carrying_control_bytes_is_neutralised_before_any_host_sees_it) {
    // The title is child-supplied text bound for a window frame. Control
    // bytes there would be drawn as frame content, so they never leave here.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.osc_policy = ckv::core::TerminalOscPolicy::StoreMetadata;
    ckv::term::TerminalEmulator emulator(profile);

    emulator.feed_output("\x1b]2;a\rb\nc\td\a");
    const std::string title = emulator.snapshot().title;
    CK_CHECK(title.find('\r') == std::string::npos);
    CK_CHECK(title.find('\n') == std::string::npos);
    CK_CHECK(title.find('\t') == std::string::npos);
    CK_CHECK(title.find('a') != std::string::npos);
    CK_CHECK(title.find('d') != std::string::npos);
}

CK_TEST(the_title_stays_denied_unless_the_profile_asks_for_metadata) {
    // Default policy: a contained child says nothing about itself.
    ckv::term::TerminalEmulator emulator(ckv::term::embedded_xterm_sixel_profile());
    emulator.feed_output("\x1b]2;ignored\a");
    CK_CHECK(emulator.snapshot().title.empty());
}

CK_TEST(bold_inside_reverse_video_does_not_pale_the_bar_behind_it) {
    // A header bar drawn reversed, with its heading emphasised. The two
    // colours must stay the plain pair: brightening the foreground before
    // the swap turns the background pale exactly where the emphasis is.
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[0;7mbar\x1b[0;1;7mBOLD");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().fg == snapshot.cell_buffer[3].style().fg);
    CK_CHECK(snapshot.cell_buffer[0].style().bg == snapshot.cell_buffer[3].style().bg);
    CK_CHECK(has_attr(snapshot.cell_buffer[3].style().attrs, ckv::Attr::Bold));
    CK_CHECK(has_attr(snapshot.cell_buffer[3].style().attrs, ckv::Attr::Reverse));
}

CK_TEST(erasing_fills_with_the_selected_background_not_the_terminal_default) {
    // Background colour erase, which xterm-family terminfo advertises as
    // `bce`. A curses program paints a full-width header bar by selecting the
    // colour and erasing the line, not by writing spaces across it — so a
    // terminal that erases to its own default shows that bar only underneath
    // the characters that happened to be written. This is htop's table
    // header, and it was coloured from "PID" to "Command" and nowhere else.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{20, 3};
    ckv::term::TerminalEmulator emulator(profile);

    // Green background, home the cursor, erase the line, then write text
    // starting a couple of columns in — exactly ncurses' order.
    emulator.feed_output("\x1b[42m\x1b[1;1H\x1b[K\x1b[1;3HPID");
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    const ckv::Color green = ckv::Color::indexed(2);

    // The bar starts at the first column, before any text...
    CK_CHECK(snapshot.cell_buffer[0].style().bg == green);
    CK_CHECK(snapshot.cell_buffer[1].style().bg == green);
    // ...covers the text...
    CK_CHECK(snapshot.cell_buffer[2].grapheme() == "P");
    CK_CHECK(snapshot.cell_buffer[2].style().bg == green);
    // ...and runs to the end of the line rather than stopping after it.
    CK_CHECK(snapshot.cell_buffer[19].style().bg == green);
    // The row below is untouched: erasing one line colours one line.
    CK_CHECK(snapshot.cell_buffer[20].style().bg == profile.default_style.bg);
}

CK_TEST(an_erased_cell_keeps_the_background_but_not_the_glyph_attributes) {
    // Weight and italics describe a glyph that is no longer there, and
    // underline would draw a rule across every cleared line.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{8, 2};
    ckv::term::TerminalEmulator emulator(profile);

    emulator.feed_output("\x1b[1;4;44m\x1b[2J");
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().bg == ckv::Color::indexed(4));
    CK_CHECK(!has_attr(snapshot.cell_buffer[0].style().attrs, ckv::Attr::Underline));
    CK_CHECK(!has_attr(snapshot.cell_buffer[0].style().attrs, ckv::Attr::Bold));
}

CK_TEST(reverse_video_erases_with_what_the_reader_sees_as_the_background) {
    // While reverse is set the visible background is the selected
    // foreground, so that is what an erase has to leave behind.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{8, 2};
    ckv::term::TerminalEmulator emulator(profile);

    emulator.feed_output("\x1b[31;7m\x1b[2J");  // red foreground, reversed
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().bg == ckv::Color::indexed(1));
}

CK_TEST(scrolling_fills_the_exposed_line_with_the_selected_background) {
    // A scroll exposes a new line; under a coloured pen that line is part of
    // the coloured region, which is how a full-screen program keeps its
    // background continuous while content moves.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{6, 3};
    ckv::term::TerminalEmulator emulator(profile);

    emulator.feed_output("\x1b[44m\x1b[3;1H\n");  // blue pen, scroll from the last row
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    const ckv::Color blue = ckv::Color::indexed(4);
    for (int x = 0; x < 6; ++x)
        CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(2 * 6 + x)].style().bg == blue);
}

CK_TEST(insert_mode_pushes_the_rest_of_the_line_right) {
    // IRM, the ANSI mode CSI 4 h. Found by differential conformance against
    // an established terminal: ckVision had no handling for ANSI modes at all
    // (only the DEC private '?' ones), so writing in insert mode overwrote
    // what a program expected to be pushed aside.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{12, 2};
    ckv::term::TerminalEmulator emulator(profile);

    emulator.feed_output("\x1b[1;1Habcdef\x1b[1;3H\x1b[4hXY");
    ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    std::string row;
    for (int x = 0; x < 8; ++x) row += snapshot.cell_buffer[static_cast<std::size_t>(x)].grapheme();
    CK_CHECK(row == "abXYcdef");

    // Replace mode is the default and must still overwrite.
    emulator.feed_output("\x1b[4l\x1b[1;1H\x1b[2K\x1b[1;1Habcdef\x1b[1;3HZZ");
    snapshot = emulator.snapshot();
    row.clear();
    for (int x = 0; x < 6; ++x) row += snapshot.cell_buffer[static_cast<std::size_t>(x)].grapheme();
    CK_CHECK(row == "abZZef");
}

CK_TEST(autowrap_can_be_turned_off_so_the_last_column_is_written_over) {
    // DECAWM (?7) reset. A program fills the bottom-right cell this way
    // without the screen scrolling out from under it.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{5, 3};
    ckv::term::TerminalEmulator emulator(profile);

    emulator.feed_output("\x1b[?7labcdefgh");
    ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    std::string first;
    for (int x = 0; x < 5; ++x) first += snapshot.cell_buffer[static_cast<std::size_t>(x)].grapheme();
    CK_CHECK(first == "abcdh");  // the tail piles up in the last column
    CK_CHECK(snapshot.cell_buffer[5].grapheme() == " ");  // nothing wrapped

    // Turning it back on restores wrapping.
    emulator.feed_output("\x1b[?7h\x1b[2J\x1b[1;1Habcdefgh");
    snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[5].grapheme() == "f");
}

CK_TEST(overwriting_half_of_a_wide_character_blanks_the_other_half) {
    // The two cells of a double-width character only mean anything together.
    // Left behind, the orphaned half renders as a fragment of a character
    // that is no longer on the screen.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{10, 2};
    ckv::term::TerminalEmulator emulator(profile);

    emulator.feed_output("\x1b[1;1H\xe4\xbd\xa0\x1b[1;1HX");  // write over the left half
    ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "X");
    CK_CHECK(snapshot.cell_buffer[1].grapheme() == " ");
    CK_CHECK(!snapshot.cell_buffer[1].is_continuation());

    // ...and over the right half, which orphans the left one instead.
    emulator.feed_output("\x1b[2J\x1b[1;1H\xe4\xbd\xa0\x1b[1;2HY");
    snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == " ");
    CK_CHECK(snapshot.cell_buffer[1].grapheme() == "Y");
}

CK_TEST(an_underline_style_survives_as_the_shape_the_program_asked_for) {
    // nvim draws a spelling mistake with a curl and a type error with a
    // dotted rule; an emulator that knows only one rule shows the same mark
    // for both, and the distinction the editor was making is lost.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[4:3mA\x1b[4:4mB\x1b[4:2mC\x1b[4mD");
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().underline == ckv::UnderlineShape::Curly);
    CK_CHECK(snapshot.cell_buffer[1].style().underline == ckv::UnderlineShape::Dotted);
    CK_CHECK(snapshot.cell_buffer[2].style().underline == ckv::UnderlineShape::Double);
    CK_CHECK(snapshot.cell_buffer[3].style().underline == ckv::UnderlineShape::Straight);
    for (std::size_t i = 0; i < 4; ++i)
        CK_CHECK(has_attr(snapshot.cell_buffer[i].style().attrs, ckv::Attr::Underline));
    CK_CHECK(snapshot.diagnostics.empty());
}

CK_TEST(turning_an_underline_off_leaves_no_shape_or_colour_behind) {
    // Two cells that look alike have to compare alike, or the presenter
    // redraws cells nothing happened to.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[4:3;58;5;9mA\x1b[24mB\x1b[4:3;58;5;9m\x1b[4:0mC");
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    const ckv::Style plain = snapshot.cell_buffer[1].style();
    CK_CHECK(!has_attr(plain.attrs, ckv::Attr::Underline));
    CK_CHECK(plain.underline == ckv::UnderlineShape::Straight);
    CK_CHECK(plain.underline_color.is_default());
    // `4:0` is the same statement written the other way round.
    CK_CHECK(snapshot.cell_buffer[2].style() == plain);
}

CK_TEST(an_underline_can_be_coloured_without_recolouring_the_word) {
    // The point of SGR 58: a red curl under a black word. Both spellings are
    // in circulation and nvim writes the sub-parameter one.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[58;5;9mA\x1b[58:2::10:20:30mB\x1b[58:2:0:1:2:3mC\x1b[59mD");
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().underline_color == ckv::Color::indexed(9));
    CK_CHECK(snapshot.cell_buffer[1].style().underline_color == ckv::Color::rgb(10, 20, 30));
    CK_CHECK(snapshot.cell_buffer[2].style().underline_color == ckv::Color::rgb(1, 2, 3));
    CK_CHECK(snapshot.cell_buffer[3].style().underline_color.is_default());
    // The text keeps the colour it had throughout.
    for (std::size_t i = 0; i < 4; ++i)
        CK_CHECK(snapshot.cell_buffer[i].style().fg == emulator.profile().default_style.fg);
    CK_CHECK(snapshot.diagnostics.empty());
}

CK_TEST(sgr_21_is_the_double_underline_terminals_answer_it_with) {
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[21mA");
    const ckv::Style style = emulator.snapshot().cell_buffer[0].style();
    CK_CHECK(has_attr(style.attrs, ckv::Attr::Underline));
    CK_CHECK(style.underline == ckv::UnderlineShape::Double);
}

CK_TEST(a_colour_written_as_sub_parameters_is_one_parameter_not_five) {
    // The ISO 8613-6 spelling. Read as ordinary parameters the trailing
    // numbers would be applied as attributes of their own.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[38:2::1:2:3;48:5:238mX");
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().fg == ckv::Color::rgb(1, 2, 3));
    CK_CHECK(snapshot.cell_buffer[0].style().bg == ckv::Color::indexed(238));
    CK_CHECK(snapshot.diagnostics.empty());
}

CK_TEST(an_unknown_underline_shape_still_leaves_an_underline) {
    // The program plainly wants a rule; losing it because the shape is one
    // nobody has defined would be a worse reading than drawing the rule.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[4:9mA");
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(has_attr(snapshot.cell_buffer[0].style().attrs, ckv::Attr::Underline));
    CK_CHECK(snapshot.cell_buffer[0].style().underline == ckv::UnderlineShape::Straight);
    CK_CHECK(snapshot.diagnostics.size() == 1);
    CK_CHECK(snapshot.diagnostics[0].kind ==
             ckv::term::TerminalDiagnostic::Kind::UnsupportedSequence);
}

CK_TEST(a_child_asking_for_the_background_colour_is_told_what_it_is) {
    // vim and nvim ask before choosing a colour scheme. A terminal that says
    // nothing leaves them guessing, and on a dark background they guess light
    // — every syntax colour then comes out wrong at once.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b]11;?\x07");
    CK_CHECK(emulator.take_pending_input() == "\x1b]11;rgb:0000/0000/0000\a");

    // The foreground is the other half of the same question.
    emulator.feed_output("\x1b]10;?\x1b\\");
    CK_CHECK(emulator.take_pending_input() == "\x1b]10;rgb:bbbb/bbbb/bbbb\x1b\\");
}

CK_TEST(a_colour_report_comes_back_with_the_terminator_it_was_asked_with) {
    // Programs read for one or the other, not for both; answering with the
    // wrong one is answering into a read that never completes.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b]11;?\x1b\\");
    const std::string reply = emulator.take_pending_input();
    CK_CHECK(reply.size() > 2 && reply.substr(reply.size() - 2) == "\x1b\\");
}

CK_TEST(a_child_can_ask_what_a_palette_entry_is) {
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b]4;1;?\x07");
    CK_CHECK(emulator.take_pending_input() == "\x1b]4;1;rgb:cdcd/3131/3131\a");
    // Several entries in one sequence are answered one reply each, which is
    // what the asking program reads back.
    emulator.feed_output("\x1b]4;0;?;15;?\x07");
    CK_CHECK(emulator.take_pending_input() ==
             "\x1b]4;0;rgb:0000/0000/0000\a\x1b]4;15;rgb:ffff/ffff/ffff\a");
}

CK_TEST(a_child_cannot_redefine_the_colours_the_terminal_is_made_of) {
    // Answering what a colour is and letting a child change it are different
    // questions: a child that could set them could recolour its own window.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b]11;#ff0000\x07\x1b]4;1;#00ff00\x07");
    CK_CHECK(emulator.take_pending_input().empty());
    CK_CHECK(emulator.profile().default_style.bg == ckv::Color::rgb(0, 0, 0));
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.diagnostics.size() == 2);
}

CK_TEST(a_silent_query_policy_answers_no_colour_question_either) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.query_policy = ckv::term::TerminalQueryPolicy::NoResponse;
    ckv::term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b]11;?\x07\x1b]10;?\x07\x1b]4;1;?\x07");
    CK_CHECK(emulator.take_pending_input().empty());
}

CK_TEST(alternate_scroll_is_on_until_the_child_turns_it_off) {
    ckv::term::TerminalEmulator emulator;
    CK_CHECK(emulator.snapshot().alternate_scroll_enabled);
    emulator.feed_output("\x1b[?1007l");
    CK_CHECK(!emulator.snapshot().alternate_scroll_enabled);
    emulator.feed_output("\x1b[?1007h");
    CK_CHECK(emulator.snapshot().alternate_scroll_enabled);
    CK_CHECK(emulator.snapshot().diagnostics.empty());
}

CK_TEST(a_profile_may_open_with_alternate_scroll_off) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.alternate_scroll = false;
    ckv::term::TerminalEmulator emulator(profile);
    CK_CHECK(!emulator.snapshot().alternate_scroll_enabled);
}

CK_TEST(a_child_clipboard_write_is_denied_unless_the_profile_allows_it) {
    // It is the one thing a child can do that reaches outside its window:
    // yank in an editor somebody is driving, or a file they happened to cat.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b]52;c;aGVsbG8=\x07");
    const ckv::term::TerminalSnapshot denied = emulator.snapshot();
    CK_CHECK(denied.clipboard_serial == 0);
    CK_CHECK(denied.clipboard_text.empty());
    CK_CHECK(denied.diagnostics.size() == 1);
}

CK_TEST(an_allowed_child_clipboard_write_arrives_decoded_and_counted) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.clipboard_policy = ckv::term::TerminalClipboardPolicy::AllowWrite;
    ckv::term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b]52;c;aGVsbG8=\x07");
    CK_CHECK(emulator.snapshot().clipboard_text == "hello");
    CK_CHECK(emulator.snapshot().clipboard_serial == 1);

    // A second request with the same text still counts as a request: the
    // reader asked for it twice and the clipboard was set twice.
    emulator.feed_output("\x1b]52;c;aGVsbG8=\x1b\\");
    CK_CHECK(emulator.snapshot().clipboard_serial == 2);
    CK_CHECK(emulator.snapshot().diagnostics.empty());
}

CK_TEST(a_child_clipboard_read_is_refused_under_every_policy) {
    // A program that can read the clipboard can read whatever its reader last
    // copied, which is usually a password.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.clipboard_policy = ckv::term::TerminalClipboardPolicy::AllowWrite;
    ckv::term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b]52;c;?\x07");
    CK_CHECK(emulator.take_pending_input().empty());
    CK_CHECK(emulator.snapshot().clipboard_serial == 0);
}

CK_TEST(a_child_clipboard_write_is_bounded_and_must_be_well_formed) {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.clipboard_policy = ckv::term::TerminalClipboardPolicy::AllowWrite;
    ckv::term::TerminalSubsessionOptions options;
    options.max_clipboard_bytes = 8;
    ckv::term::TerminalEmulator emulator(profile, options);

    emulator.feed_output("\x1b]52;c;bm90IGJhc2U2NCEh!!\x07");  // not base64 at all
    CK_CHECK(emulator.snapshot().clipboard_serial == 0);
    emulator.feed_output("\x1b]52;c;YWJjZGVmZ2hpamtsbW5vcA==\x07");  // sixteen bytes
    CK_CHECK(emulator.snapshot().clipboard_serial == 0);
    emulator.feed_output("\x1b]52;c;YWJjZGVmZ2g=\x07");  // eight bytes, exactly the cap
    CK_CHECK(emulator.snapshot().clipboard_text == "abcdefgh");
    CK_CHECK(emulator.snapshot().clipboard_serial == 1);
}

CK_TEST(clipboard_text_keeps_its_lines_and_loses_its_control_bytes) {
    // A clipboard holds documents: tabs and line breaks are content there.
    // An escape byte is not, and would be a control sequence the next time
    // that text was pasted anywhere.
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.clipboard_policy = ckv::term::TerminalClipboardPolicy::AllowWrite;
    ckv::term::TerminalEmulator emulator(profile);
    // "a\tb\r\nc\x1b[31md"
    emulator.feed_output("\x1b]52;c;YQliDQpj G1szMW1k\x07");
    CK_CHECK(emulator.snapshot().clipboard_serial == 0);  // whitespace is not base64
    emulator.feed_output("\x1b]52;c;YQliDQpjG1szMW1k\x07");
    CK_CHECK(emulator.snapshot().clipboard_text == "a\tb\nc\xEF\xBF\xBD[31md");
}

CK_TEST(a_child_pushes_and_pops_the_keyboard_enhancements_it_wants) {
    // The stack is what makes progressive enhancement safe: a program turns
    // on what it needs, and puts the terminal back exactly as it found it.
    ckv::term::TerminalEmulator emulator;
    CK_CHECK(emulator.snapshot().keyboard_flags == ckv::term::TerminalKeyboardFlags::None);
    emulator.feed_output("\x1b[>1u");
    CK_CHECK(emulator.snapshot().keyboard_flags ==
             ckv::term::TerminalKeyboardFlags::DisambiguateEscapeCodes);
    emulator.feed_output("\x1b[>3u");
    CK_CHECK(emulator.snapshot().keyboard_flags ==
             (ckv::term::TerminalKeyboardFlags::DisambiguateEscapeCodes |
              ckv::term::TerminalKeyboardFlags::ReportEventTypes));
    emulator.feed_output("\x1b[<u");
    CK_CHECK(emulator.snapshot().keyboard_flags ==
             ckv::term::TerminalKeyboardFlags::DisambiguateEscapeCodes);
    emulator.feed_output("\x1b[<u");
    CK_CHECK(emulator.snapshot().keyboard_flags == ckv::term::TerminalKeyboardFlags::None);
    CK_CHECK(emulator.snapshot().diagnostics.empty());
}

CK_TEST(popping_more_than_was_pushed_leaves_the_terminal_as_it_started) {
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[>9u\x1b[<5u");
    CK_CHECK(emulator.snapshot().keyboard_flags == ckv::term::TerminalKeyboardFlags::None);
}

CK_TEST(a_child_is_told_only_the_enhancements_it_will_really_receive) {
    // The protocol exists to be asked. Reporting a flag we cannot honour
    // would have a program stop looking for the fallback it still needs.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[>31u\x1b[?u");
    const int supported =
        static_cast<int>(ckv::term::supported_terminal_keyboard_flags());
    CK_CHECK(emulator.take_pending_input() == "\x1b[?" + std::to_string(supported) + "u");
    CK_CHECK(!has_flag(emulator.snapshot().keyboard_flags,
                       ckv::term::TerminalKeyboardFlags::ReportAlternateKeys));
}

CK_TEST(the_keyboard_flags_can_be_set_added_to_and_taken_from) {
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[=1;1u");  // set
    CK_CHECK(emulator.snapshot().keyboard_flags ==
             ckv::term::TerminalKeyboardFlags::DisambiguateEscapeCodes);
    emulator.feed_output("\x1b[=2;2u");  // add
    CK_CHECK(emulator.snapshot().keyboard_flags ==
             (ckv::term::TerminalKeyboardFlags::DisambiguateEscapeCodes |
              ckv::term::TerminalKeyboardFlags::ReportEventTypes));
    emulator.feed_output("\x1b[=1;3u");  // remove
    CK_CHECK(emulator.snapshot().keyboard_flags == ckv::term::TerminalKeyboardFlags::ReportEventTypes);
}

CK_TEST(a_full_screen_programs_keyboard_settings_do_not_follow_it_out) {
    // A program that dies on the alternate screen without putting the
    // keyboard back would otherwise leave the shell underneath it receiving
    // keys in an encoding the shell never asked for.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[?1049h\x1b[>15u");
    CK_CHECK(emulator.snapshot().keyboard_flags != ckv::term::TerminalKeyboardFlags::None);
    emulator.feed_output("\x1b[?1049l");
    CK_CHECK(emulator.snapshot().keyboard_flags == ckv::term::TerminalKeyboardFlags::None);
}

CK_TEST(a_bare_cursor_restore_is_still_a_cursor_restore) {
    // `CSI u` without a private prefix has meant "restore the cursor" since
    // long before the keyboard protocol existed.
    ckv::term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[3;4H\x1b[s\x1b[1;1H\x1b[u");
    CK_CHECK(emulator.snapshot().cursor.position == (ckv::Point{3, 2}));
    CK_CHECK(emulator.snapshot().keyboard_flags == ckv::term::TerminalKeyboardFlags::None);
}

CK_TEST(a_childs_bell_is_counted_rather_than_latched) {
    // A flag reading clears makes the first reader the only one to see the
    // bell; a flag reading does not clear cannot say a second one arrived.
    term::TerminalEmulator emulator;
    CK_CHECK(emulator.snapshot().bell_serial == 0U);
    emulator.feed_output("ding\a");
    CK_CHECK(emulator.snapshot().bell_serial == 1U);
    CK_CHECK(emulator.snapshot().bell_serial == 1U);  // reading it changes nothing
    emulator.feed_output("\a\a");
    CK_CHECK(emulator.snapshot().bell_serial == 3U);
    // It rang; it did not write. The text around it is untouched and the
    // cursor has not moved past what was printed.
    CK_CHECK(emulator.snapshot().cell_buffer[0].grapheme() == "d");
    CK_CHECK(emulator.snapshot().cursor.position == (Point{4, 0}));
}

CK_TEST(a_bell_between_the_halves_of_a_character_does_not_split_it) {
    // BEL writes nothing, so it must not flush a grapheme that has not
    // finished arriving — an emulator that flushes here shows a replacement
    // character wherever a program happened to ring mid-word.
    term::TerminalEmulator emulator;
    emulator.feed_output("\xc3");
    emulator.feed_output("\a");
    emulator.feed_output("\xa9");
    CK_CHECK(emulator.snapshot().bell_serial == 1U);
    CK_CHECK(emulator.snapshot().cell_buffer[0].grapheme() == "é");
}

CK_TEST(a_full_reset_puts_a_wedged_terminal_back) {
    // What `reset(1)` sends, and what an emulator that ignores it leaves the
    // reader stuck with: an alternate screen, a scroll region, a colour, a
    // charset drawing letters as box pieces, and the cursor hidden.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{4, 3};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("one\r\ntwo\r\nsix\r\nten");  // scrolls, so there is history
    emulator.feed_output("\x1b[?1049h\x1b[?25l\x1b[?1000h\x1b[?2004h\x1b[?1h\x1b[31m\x1b[2;3r\x1b(0\x1b[?7l");
    const term::TerminalSnapshot wedged = emulator.snapshot();
    CK_CHECK(wedged.alternate_buffer);
    CK_CHECK(!wedged.cursor.visible);
    CK_CHECK(wedged.mouse_reporting_enabled);

    emulator.feed_output("\x1b" "c");
    const term::TerminalSnapshot reset = emulator.snapshot();
    CK_CHECK(!reset.alternate_buffer);
    CK_CHECK(reset.cursor.visible);
    CK_CHECK(reset.cursor.position == (Point{0, 0}));
    CK_CHECK(!reset.mouse_reporting_enabled);
    CK_CHECK(!reset.bracketed_paste_enabled);
    CK_CHECK(!reset.application_cursor_keys);
    CK_CHECK(reset.scrollback.empty());
    for (const Cell& cell : reset.cell_buffer) CK_CHECK(cell.grapheme() == " ");

    // The modes really went back, rather than the screen merely being blanked:
    // the letters below are the DEC line-drawing set's, the scroll region is
    // the whole screen again, and the colour is the profile's own.
    emulator.feed_output("qq");
    const term::TerminalSnapshot after = emulator.snapshot();
    CK_CHECK(after.cell_buffer[0].grapheme() == "q");
    CK_CHECK(after.cell_buffer[0].style() == profile.default_style);
}

CK_TEST(a_full_reset_keeps_the_window_caption_and_the_childs_fate) {
    // A program clearing its own screen has not said the window is nameless,
    // and a reset cannot bring a dead child back to life.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.osc_policy = core::TerminalOscPolicy::StoreMetadata;
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b]2;building\a");
    emulator.feed_output("\x1b" "c");
    CK_CHECK(emulator.snapshot().title == "building");

    emulator.mark_exited(0);
    emulator.feed_output("\x1b" "c");
    CK_CHECK(emulator.snapshot().state == core::TerminalSubsessionState::Exited);
}

CK_TEST(the_alignment_pattern_fills_the_screen_and_takes_the_margins_with_it) {
    // DECALN, which is how a terminal's alignment is checked and how most of
    // vttest's chapters begin.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{3, 3};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b[2;2r\x1b[3;3H");
    emulator.feed_output("\x1b#8");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    for (const Cell& cell : snapshot.cell_buffer) CK_CHECK(cell.grapheme() == "E");
    CK_CHECK(snapshot.cursor.position == (Point{0, 0}));

    // The margins are the whole screen again: three newlines from the top row
    // reach the bottom row rather than scrolling a two-row region.
    emulator.feed_output("\x1b[1;1HZ\n\n");
    CK_CHECK(emulator.snapshot().cell_buffer[0].grapheme() == "Z");
}

CK_TEST(an_unknown_dec_line_attribute_is_reported_rather_than_printed) {
    // ESC # 3 (double-height top half) is not implemented; what must not
    // happen is the '3' landing on the screen as text.
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b#3ok");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].grapheme() == "o");
    CK_CHECK(snapshot.cell_buffer[1].grapheme() == "k");
    bool reported = false;
    for (const term::TerminalDiagnostic& diagnostic : snapshot.diagnostics)
        if (diagnostic.kind == term::TerminalDiagnostic::Kind::UnsupportedSequence) reported = true;
    CK_CHECK(reported);
}

CK_TEST(a_child_may_name_the_window_only_where_the_profile_allows_it) {
    // The policy flag is the whole of the permission: with metadata storage
    // off, a title is understood and deliberately without effect, and no
    // diagnostic — a shell writing its prompt title is not doing anything
    // wrong, it is doing something this profile does not keep.
    term::TerminalEmulator denied;
    denied.feed_output("\x1b]2;secret project\a");
    CK_CHECK(denied.snapshot().title.empty());

    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.osc_policy = core::TerminalOscPolicy::StoreMetadata;
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b]2;vim: plans/04.md\a");
    CK_CHECK(emulator.snapshot().title == "vim: plans/04.md");
    // OSC 0 is icon-and-title and sets it too; OSC 1 is the icon name alone
    // and deliberately leaves the caption where it was.
    emulator.feed_output("\x1b]0;make\x1b\\");
    CK_CHECK(emulator.snapshot().title == "make");
    emulator.feed_output("\x1b]1;an icon\a");
    CK_CHECK(emulator.snapshot().title == "make");
}

CK_TEST(a_title_is_bounded_and_cut_on_a_character_boundary) {
    // The snapshot carries the title by value and a host reads one per
    // terminal per frame, so an unbounded title is an unbounded copy at frame
    // rate. Cutting mid-character would leave a replacement mark the child
    // never sent.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.osc_policy = core::TerminalOscPolicy::StoreMetadata;
    term::TerminalSubsessionOptions options;
    options.max_title_bytes = 9;
    term::TerminalEmulator emulator(profile, options);

    emulator.feed_output("\x1b]2;0123456789abc\a");
    CK_CHECK(emulator.snapshot().title == "012345678");

    // "é" is two bytes: the cut at nine bytes falls inside the fifth one, so
    // the fourth character is dropped whole rather than half-kept.
    emulator.feed_output("\x1b]2;ééééé\a");
    CK_CHECK(emulator.snapshot().title == "éééé");
    bool reported = false;
    for (const term::TerminalDiagnostic& diagnostic : emulator.snapshot().diagnostics)
        if (diagnostic.kind == term::TerminalDiagnostic::Kind::LimitExceeded) reported = true;
    CK_CHECK(reported);
}

CK_TEST(a_program_gets_the_caption_back_that_it_pushed) {
    // XTWINOPS 22/23. Without it a shell keeps the name of the editor that
    // exited, because the editor set a title and nothing put the old one back.
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.osc_policy = core::TerminalOscPolicy::StoreMetadata;
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b]2;zsh\a");
    emulator.feed_output("\x1b[22;0t");          // the editor saves it
    emulator.feed_output("\x1b]2;vim README\a");  // ...and renames the window
    CK_CHECK(emulator.snapshot().title == "vim README");
    emulator.feed_output("\x1b[23;0t");          // ...and puts it back on exit
    CK_CHECK(emulator.snapshot().title == "zsh");

    // Nesting works, and popping more than was pushed leaves the caption
    // alone rather than blanking it: a program asking for what it started
    // with should not be answered with nothing.
    emulator.feed_output("\x1b[22;2t\x1b]2;one\a\x1b[22;2t\x1b]2;two\a");
    emulator.feed_output("\x1b[23;2t");
    CK_CHECK(emulator.snapshot().title == "one");
    emulator.feed_output("\x1b[23;2t\x1b[23;2t\x1b[23;2t");
    CK_CHECK(emulator.snapshot().title == "zsh");
}

CK_TEST(the_title_stack_is_bounded_and_refused_where_the_policy_refuses_titles) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.osc_policy = core::TerminalOscPolicy::StoreMetadata;
    term::TerminalSubsessionOptions options;
    options.max_title_stack_depth = 2;
    term::TerminalEmulator emulator(profile, options);
    emulator.feed_output("\x1b]2;first\a\x1b[22;0t");
    emulator.feed_output("\x1b]2;second\a\x1b[22;0t");
    emulator.feed_output("\x1b]2;third\a\x1b[22;0t");  // pushes out "first"
    emulator.feed_output("\x1b]2;now\a");
    emulator.feed_output("\x1b[23;0t");
    CK_CHECK(emulator.snapshot().title == "third");
    emulator.feed_output("\x1b[23;0t");
    CK_CHECK(emulator.snapshot().title == "second");
    emulator.feed_output("\x1b[23;0t");
    CK_CHECK(emulator.snapshot().title == "second");  // the bottom fell off, and says nothing

    // A profile that does not keep titles does not keep a stack of them
    // either — the sequence is understood and does nothing.
    term::TerminalEmulator denied;
    denied.feed_output("\x1b[22;0t\x1b[23;0t");
    CK_CHECK(denied.snapshot().title.empty());
}

CK_TEST(a_full_reset_empties_the_title_stack_it_cannot_pop) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.osc_policy = core::TerminalOscPolicy::StoreMetadata;
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b]2;before\a\x1b[22;0t\x1b]2;after\a");
    emulator.feed_output("\x1b" "c");
    CK_CHECK(emulator.snapshot().title == "after");  // the caption survives a reset
    emulator.feed_output("\x1b[23;0t");
    CK_CHECK(emulator.snapshot().title == "after");  // ...and nothing is left to pop
}
