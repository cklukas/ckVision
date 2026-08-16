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

CK_TEST(terminal_emulator_rejects_a_child_sixel_image_over_its_declared_pixel_limit) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{2, 2};
    profile.cell_pixels = Size{4, 6};
    term::TerminalSubsessionOptions options;
    options.max_image_pixels = 95;  // The declared terminal plane is 2 * 2 * 4 * 6 = 96 pixels.
    term::TerminalEmulator emulator(profile, options);
    emulator.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.rasters.empty());
    CK_CHECK(!snapshot.diagnostics.empty());
    CK_CHECK(snapshot.diagnostics.back().kind == term::TerminalDiagnostic::Kind::LimitExceeded);
}

CK_TEST(terminal_emulator_rejects_overflowing_pixel_geometry_before_decoder_allocation) {
    term::TerminalCapabilityProfile profile = term::embedded_xterm_sixel_profile();
    profile.cells = Size{2, 2};
    profile.cell_pixels = Size{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
    term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1bPq#0;2;100;0;0~\x1b\\");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.rasters.empty());
    CK_CHECK(!snapshot.diagnostics.empty());
    CK_CHECK(snapshot.diagnostics.back().kind == term::TerminalDiagnostic::Kind::LimitExceeded);
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
    CK_CHECK(cell.style().fg == Color::rgb(205, 49, 49));
    CK_CHECK(cell.style().bg == Color::rgb(36, 114, 200));
}

CK_TEST(terminal_emulator_applies_extended_sgr_colours_without_a_child_diagnostic) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[38;2;1;2;3;48;5;251mX");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    const Cell& cell = snapshot.cell_buffer[0];
    CK_CHECK(cell.style().fg == Color::rgb(1, 2, 3));
    CK_CHECK(cell.style().bg == Color::rgb(198, 198, 198));
    CK_CHECK(snapshot.diagnostics.empty());
}

CK_TEST(terminal_emulator_consumes_nested_capability_queries_at_the_private_boundary) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[?2026$p\x1b[?2031$p\x1b[c\x1b[?1016$p\x1b[16t");
    CK_CHECK(emulator.snapshot().diagnostics.empty());
    CK_CHECK(emulator.take_pending_input() == "\x1b[?1;2c\x1b[6;18;9t");
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
    CK_CHECK(snapshot.cell_buffer[0].style().fg == ckv::Color::rgb(255, 255, 255));
    CK_CHECK(has_attr(snapshot.cell_buffer[0].style().attrs, ckv::Attr::Bold));
}

CK_TEST(bold_brightens_each_of_the_low_eight_to_its_own_bright_form) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[1;31mR\x1b[1;34mB");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().fg == ckv::Color::rgb(241, 76, 76));   // bright red
    CK_CHECK(snapshot.cell_buffer[1].style().fg == ckv::Color::rgb(59, 142, 234));  // bright blue
}

CK_TEST(bold_leaves_a_colour_the_program_chose_explicitly_alone) {
    // A 256-colour or true-colour foreground is a specific choice;
    // brightening it would override the program rather than honour a
    // convention it was written for.
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[1;38;2;10;20;30mX");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().fg == ckv::Color::rgb(10, 20, 30));
}

CK_TEST(text_that_is_not_bold_keeps_the_colour_it_asked_for) {
    term::TerminalEmulator emulator;
    emulator.feed_output("\x1b[31mR");
    const term::TerminalSnapshot snapshot = emulator.snapshot();
    CK_CHECK(snapshot.cell_buffer[0].style().fg == ckv::Color::rgb(205, 49, 49));  // plain red
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
    const ckv::Color green = ckv::Color::rgb(13, 188, 121);

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
    CK_CHECK(snapshot.cell_buffer[0].style().bg == ckv::Color::rgb(36, 114, 200));
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
    CK_CHECK(snapshot.cell_buffer[0].style().bg == ckv::Color::rgb(205, 49, 49));
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
    const ckv::Color blue = ckv::Color::rgb(36, 114, 200);
    for (int x = 0; x < 6; ++x)
        CK_CHECK(snapshot.cell_buffer[static_cast<std::size_t>(2 * 6 + x)].style().bg == blue);
}
