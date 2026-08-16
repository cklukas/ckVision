// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/presenter.hpp"

#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cktest.hpp"

using namespace ckv;
using namespace ckv::term;
using ckv::scene::Surface;

namespace {

Surface make_surface(int w, int h, std::string_view fill = " ") {
    return Surface(Size{w, h}, Cell::from_grapheme(fill, Style{}));
}

}  // namespace

// --- Cell diffing -------------------------------------------------------------

CK_TEST(first_present_writes_every_cell) {
    HeadlessTerminal term(Size{3, 1});
    Presenter presenter(term);
    Surface s = make_surface(3, 1, "x");
    presenter.present(s.view(), CursorState{});
    CK_CHECK(term.written_bytes().find("xxx") != std::string_view::npos);
}

CK_TEST(second_present_with_no_changes_writes_no_bytes) {
    HeadlessTerminal term(Size{3, 1});
    Presenter presenter(term);
    Surface s = make_surface(3, 1, "x");
    presenter.present(s.view(), CursorState{});
    term.clear_written();

    presenter.present(s.view(), CursorState{});
    CK_CHECK(term.written_bytes().empty());
    CK_CHECK(presenter.last_bytes_emitted() == 0);
}

CK_TEST(only_the_changed_cell_is_rewritten) {
    HeadlessTerminal term(Size{5, 1});
    Presenter presenter(term);
    Surface s = make_surface(5, 1, ".");
    presenter.present(s.view(), CursorState{});
    term.clear_written();

    s.set_cell(Point{2, 0}, Cell::from_grapheme("Z", Style{}));
    presenter.present(s.view(), CursorState{});
    CK_CHECK(term.written_bytes().find('Z') != std::string_view::npos);
    // Cursor move to column 2 (0-based) -> "3" in 1-based CUP.
    CK_CHECK(term.written_bytes().find("\x1B[1;3H") != std::string_view::npos);
}

CK_TEST(invalidate_forces_the_next_present_to_rewrite_everything) {
    HeadlessTerminal term(Size{3, 1});
    Presenter presenter(term);
    Surface s = make_surface(3, 1, "x");
    presenter.present(s.view(), CursorState{});
    term.clear_written();

    presenter.invalidate();
    presenter.present(s.view(), CursorState{});
    CK_CHECK(term.written_bytes().find("xxx") != std::string_view::npos);
}

CK_TEST(resize_between_presents_forces_a_full_repaint) {
    HeadlessTerminal term(Size{3, 1});
    Presenter presenter(term);
    Surface s3 = make_surface(3, 1, "x");
    presenter.present(s3.view(), CursorState{});
    term.clear_written();

    Surface s5 = make_surface(5, 1, "y");
    presenter.present(s5.view(), CursorState{});
    CK_CHECK(term.written_bytes().find("yyyyy") != std::string_view::npos);
}

// --- Continuation cells ---------------------------------------------------------

CK_TEST(continuation_cells_contribute_no_text_of_their_own) {
    HeadlessTerminal term(Size{4, 1});
    Presenter presenter(term);
    Surface s = make_surface(4, 1, ".");
    s.set_cell(Point{0, 0}, Cell::from_grapheme("\xE4\xB8\xAD", Style{}));  // 中, width 2
    s.set_cell(Point{1, 0}, Cell::continuation(Style{}));
    presenter.present(s.view(), CursorState{});

    // The wide glyph's 3-byte UTF-8 encoding must appear exactly once
    // in the written output, not duplicated for its continuation cell.
    const std::string_view glyph = "\xE4\xB8\xAD";
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = term.written_bytes().find(glyph, pos)) != std::string_view::npos) {
        ++count;
        pos += glyph.size();
    }
    CK_CHECK(count == 1);
}

// --- Style batching and color-depth degradation -----------------------------

CK_TEST(style_to_sgr_truecolor) {
    const Style st{Color::rgb(10, 20, 30), Color::rgb(1, 2, 3), Attr::Bold};
    const std::string sgr = style_to_sgr(st, ColorDepth::TrueColor);
    CK_CHECK(sgr.find(";1") != std::string::npos);        // bold
    CK_CHECK(sgr.find("38;2;10;20;30") != std::string::npos);
    CK_CHECK(sgr.find("48;2;1;2;3") != std::string::npos);
}

CK_TEST(style_to_sgr_default_colors_emit_no_color_escape) {
    const Style st{Color{}, Color{}, Attr{}};
    const std::string sgr = style_to_sgr(st, ColorDepth::TrueColor);
    CK_CHECK(sgr.find("38;") == std::string::npos);
    CK_CHECK(sgr.find("48;") == std::string::npos);
    CK_CHECK(sgr == "\x1B[0m");  // just a reset
}

CK_TEST(style_to_sgr_256_color_uses_indexed_form) {
    const Style st{Color::rgb(255, 0, 0), Color{}, Attr{}};
    const std::string sgr = style_to_sgr(st, ColorDepth::Color256);
    CK_CHECK(sgr.find("38;5;") != std::string::npos);
    CK_CHECK(sgr.find("38;2;") == std::string::npos);  // never truecolor form at this depth
}

CK_TEST(style_to_sgr_256_color_picks_a_plausible_red) {
    // Pure red should map to a palette entry that is itself red-ish
    // (high red channel index), not an arbitrary color — verified by
    // checking it is NOT the palette's darkest/grayscale region.
    const Style st{Color::rgb(255, 0, 0), Color{}, Attr{}};
    const std::string sgr = style_to_sgr(st, ColorDepth::Color256);
    const std::size_t pos = sgr.find("38;5;");
    CK_CHECK(pos != std::string::npos);
    const int index = std::stoi(sgr.substr(pos + 5));
    CK_CHECK(index >= 16 && index <= 231);  // within the 6x6x6 cube, not grayscale/basic
}

CK_TEST(style_to_sgr_mono16_uses_basic_sgr_codes) {
    const Style bright_red{Color::rgb(255, 0, 0), Color{}, Attr{}};
    const std::string sgr = style_to_sgr(bright_red, ColorDepth::Mono16);
    CK_CHECK(sgr.find("38;") == std::string::npos);  // no indexed/truecolor form at all
    // Must contain a plain SGR foreground code (30-37 or 90-97).
    bool found_basic_code = false;
    for (int code = 30; code <= 37; ++code)
        if (sgr.find(";" + std::to_string(code)) != std::string::npos) found_basic_code = true;
    for (int code = 90; code <= 97; ++code)
        if (sgr.find(";" + std::to_string(code)) != std::string::npos) found_basic_code = true;
    CK_CHECK(found_basic_code);
}

CK_TEST(repeated_same_style_in_a_run_emits_the_style_escape_only_once) {
    Capabilities caps = baseline_capabilities();
    caps.color_depth = ColorDepth::TrueColor;  // baseline is Color256; force truecolor for this check
    HeadlessTerminal term(Size{3, 1}, caps);
    Presenter presenter(term);
    Surface s = make_surface(3, 1, " ");
    const Style red{Color::rgb(255, 0, 0), Color{}, Attr{}};
    s.set_cell(Point{0, 0}, Cell::from_grapheme("a", red));
    s.set_cell(Point{1, 0}, Cell::from_grapheme("b", red));
    s.set_cell(Point{2, 0}, Cell::from_grapheme("c", red));
    presenter.present(s.view(), CursorState{});

    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = term.written_bytes().find("38;2;255;0;0", pos)) != std::string_view::npos) {
        ++count;
        pos += 1;
    }
    CK_CHECK(count == 1);  // one style escape covers all three same-styled cells
}

// --- Width-safe cursor addressing (D-019) -------------------------------------

CK_TEST(width_unsafe_glyph_forces_readdress_even_for_a_genuinely_adjacent_next_cell) {
    // The meaningful case: col0 (unsafe) and col1 (ordinary) are BOTH
    // dirty and directly adjacent — with no gap, so nothing except the
    // width-unsafe rule would ever force a second cursor-move escape
    // here. If the presenter merged them into one run and trusted
    // natural cursor advance, col1 would be written with no separate
    // positioning escape between "\xE2\x80\x8D" and "Q". It must not.
    HeadlessTerminal term(Size{6, 1});
    Presenter presenter(term);
    Surface s = make_surface(6, 1, " ");
    presenter.present(s.view(), CursorState{});  // stable baseline
    term.clear_written();

    s.set_cell(Point{0, 0}, Cell::from_grapheme("\xE2\x80\x8D", Style{}));  // lone ZWJ, width 0
    s.set_cell(Point{1, 0}, Cell::from_grapheme("Q", Style{}));
    presenter.present(s.view(), CursorState{});

    // Two explicit CUP escapes: one for column 0 (1-based col 1), one
    // for column 1 (1-based col 2) — not one run written straight
    // through.
    CK_CHECK(term.written_bytes().find(";1H") != std::string_view::npos);
    CK_CHECK(term.written_bytes().find(";2H") != std::string_view::npos);
    // And the two glyphs must not appear back-to-back with no escape
    // between them (which is what "merged into one run" would produce).
    CK_CHECK(term.written_bytes().find("\xE2\x80\x8DQ") == std::string_view::npos);
}

CK_TEST(width_unsafe_run_still_correctly_stops_at_a_genuine_unchanged_gap) {
    // A simpler case where the gap itself already forces a move,
    // regardless of the unsafe rule — kept as a baseline sanity check.
    HeadlessTerminal term(Size{6, 1});
    Presenter presenter(term);
    Surface s = make_surface(6, 1, " ");
    presenter.present(s.view(), CursorState{});
    term.clear_written();

    s.set_cell(Point{0, 0}, Cell::from_grapheme("\xE2\x80\x8D", Style{}));
    s.set_cell(Point{4, 0}, Cell::from_grapheme("Q", Style{}));
    presenter.present(s.view(), CursorState{});
    CK_CHECK(term.written_bytes().find(";5H") != std::string_view::npos);
}

CK_TEST(an_ambiguous_width_profile_reestablishes_the_cursor_after_non_ascii_text) {
    Capabilities caps = baseline_capabilities();
    caps.ambiguous_width_is_wide = true;
    HeadlessTerminal term(Size{3, 1}, caps);
    Presenter presenter(term);
    Surface s = make_surface(3, 1, " ");
    s.set_cell(Point{0, 0}, Cell::from_grapheme("\xC2\xB1", Style{}));  // ±, East-Asian Ambiguous
    s.set_cell(Point{1, 0}, Cell::from_grapheme("Q", Style{}));

    presenter.present(s.view(), CursorState{});

    // The profile changes no logical Cell width. It does ensure an alternate
    // terminal convention cannot displace the following cell: the next write
    // begins from its logical absolute column rather than an assumed advance.
    CK_CHECK(term.written_bytes().find("\x1B[1;2H") != std::string_view::npos);
}

CK_TEST(the_baseline_width_agreement_boundary_readdresses_after_cjk_text) {
    HeadlessTerminal term(Size{3, 1});
    Presenter presenter(term);
    Surface s = make_surface(3, 1, " ");
    s.set_cell(Point{0, 0}, Cell::from_grapheme("\xE4\xB8\xAD", Style{}));  // 中
    s.set_cell(Point{1, 0}, Cell::continuation(Style{}));
    s.set_cell(Point{2, 0}, Cell::from_grapheme("Q", Style{}));

    presenter.present(s.view(), CursorState{});

    // CJK's logical width is two cells, but output must not assume that a
    // particular terminal agrees. The trailing cell is addressed explicitly.
    CK_CHECK(term.written_bytes().find("\x1B[1;3H") != std::string_view::npos);
}

// --- Cursor visibility ---------------------------------------------------------

CK_TEST(visible_cursor_moves_there_and_shows_it) {
    HeadlessTerminal term(Size{5, 5});
    Presenter presenter(term);
    Surface s = make_surface(5, 5);
    presenter.present(s.view(), CursorState{true, Point{2, 3}, CursorShape::Block});
    CK_CHECK(term.written_bytes().find("\x1B[4;3H") != std::string_view::npos);  // row3+1;col2+1
    CK_CHECK(term.written_bytes().find("\x1B[?25h") != std::string_view::npos);
}

CK_TEST(visible_cursor_declares_a_blinking_shape_to_the_host_terminal) {
    HeadlessTerminal term(Size{5, 5});
    Presenter presenter(term);
    Surface s = make_surface(5, 5);
    presenter.present(s.view(), CursorState{true, Point{1, 1}, CursorShape::Block, true});
    CK_CHECK(term.written_bytes().find("\x1B[1 q") != std::string_view::npos);
}

CK_TEST(hidden_cursor_is_explicitly_hidden) {
    HeadlessTerminal term(Size{5, 5});
    Presenter presenter(term);
    Surface s = make_surface(5, 5);
    presenter.present(s.view(), CursorState{false, Point{0, 0}, CursorShape::Block});
    CK_CHECK(term.written_bytes().find("\x1B[?25l") != std::string_view::npos);
}

// --- Synchronized output --------------------------------------------------------

CK_TEST(synchronized_output_wraps_the_frame_when_supported) {
    Capabilities caps = baseline_capabilities();
    caps.synchronized_output = true;
    HeadlessTerminal term(Size{3, 1}, caps);
    Presenter presenter(term);
    Surface s = make_surface(3, 1, "x");
    presenter.present(s.view(), CursorState{});
    CK_CHECK(term.written_bytes().substr(0, 8) == "\x1B[?2026h");
    CK_CHECK(term.written_bytes().find("\x1B[?2026l") != std::string_view::npos);
}

CK_TEST(no_synchronized_output_wrapping_when_unsupported) {
    HeadlessTerminal term(Size{3, 1});  // baseline: synchronized_output = false
    Presenter presenter(term);
    Surface s = make_surface(3, 1, "x");
    presenter.present(s.view(), CursorState{});
    CK_CHECK(term.written_bytes().find("?2026") == std::string_view::npos);
}

// --- Raster slice emission --------------------------------------------------------

CK_TEST(raster_slices_are_not_emitted_without_graphics_capability) {
    HeadlessTerminal term(Size{5, 5});  // baseline: sixel_graphics = false
    Presenter presenter(term);
    Surface s = make_surface(5, 5);
    const auto image = std::make_shared<Image>(8, 8);
    std::vector<RasterSlice> rasters{{1, Rect{0, 0, 2, 2}, Rect{0, 0, 2, 2}, image, true}};
    presenter.present(s.view(), CursorState{}, rasters);
    CK_CHECK(term.written_bytes().find("\x1BP0;0;0q") == std::string_view::npos);
}

CK_TEST(sixel_presentation_replaces_fallback_text_with_clean_background_cells) {
    HeadlessTerminal term(Size{8, 1}, headless_sixel_profile());
    Presenter presenter(term);
    Surface s = make_surface(8, 1, ".");
    const Style panel{Color::rgb(0, 0, 0), Color::rgb(180, 180, 180), Attr{}};
    constexpr std::string_view fallback = "[image]";
    for (int x = 0; x < static_cast<int>(fallback.size()); ++x)
        s.set_cell(Point{x, 0},
                   Cell::from_grapheme(fallback.substr(static_cast<std::size_t>(x), 1), panel));

    auto image = std::make_shared<Image>(7, 1);
    for (int x = 0; x < image->width(); ++x)
        image->set_pixel(x, 0, Image::Rgba{255, 0, 0, 255});
    const std::vector<RasterSlice> rasters{{1, Rect{0, 0, 7, 1}, Rect{0, 0, 7, 1}, image, true}};
    presenter.present(s.view(), CursorState{}, rasters);

    CK_CHECK(term.written_bytes().find(fallback) == std::string_view::npos);
    for (int x = 0; x < 7; ++x) {
        const Cell& cell = term.display().frame().at(Point{x, 0});
        CK_CHECK(cell.grapheme() == " ");
        CK_CHECK(cell.style().bg == panel.bg);
    }
    CK_CHECK(term.display().raster_plane().pixel(0, 0).a == 255);
}

CK_TEST(sixel_geometry_limit_preserves_the_mandatory_text_fallback) {
    Capabilities caps = headless_sixel_profile();
    caps.sixel_max_geometry = Size{4, 4};
    HeadlessTerminal term(Size{5, 1}, caps);
    Presenter presenter(term);
    Surface s = make_surface(5, 1, ".");
    constexpr std::string_view fallback = "[img]";
    for (int x = 0; x < static_cast<int>(fallback.size()); ++x)
        s.set_cell(Point{x, 0}, Cell::from_grapheme(fallback.substr(static_cast<std::size_t>(x), 1), Style{}));

    const auto image = std::make_shared<Image>(8, 8);  // exceeds the verified 4×4 geometry limit
    const std::vector<RasterSlice> rasters{{1, Rect{0, 0, 5, 1}, Rect{0, 0, 5, 1}, image, true}};
    presenter.present(s.view(), CursorState{}, rasters);

    CK_CHECK(term.written_bytes().find("\x1BP0;0;0q") == std::string_view::npos);
    CK_CHECK(term.written_bytes().find(fallback) != std::string_view::npos);
}

CK_TEST(unoccluded_raster_slice_emits_the_full_image_cropped_to_itself) {
    Capabilities caps = baseline_capabilities();
    caps.sixel_graphics = true;
    HeadlessTerminal term(Size{5, 5}, caps);
    Presenter presenter(term);
    Surface s = make_surface(5, 5);
    const auto image = std::make_shared<Image>(8, 8);
    image->set_pixel(0, 0, Image::Rgba{200, 0, 0, 255});
    // visible_rect == full_anchor: unoccluded, the whole image applies.
    std::vector<RasterSlice> rasters{{1, Rect{1, 1, 2, 2}, Rect{1, 1, 2, 2}, image, true}};
    presenter.present(s.view(), CursorState{}, rasters);
    CK_CHECK(term.written_bytes().find("\x1BP0;0;0q") != std::string_view::npos);
    CK_CHECK(term.written_bytes().find("#0;2;78;0;0") != std::string_view::npos);  // 200/255 ~ 78%
}

CK_TEST(occluded_slice_crops_to_its_sub_rect_not_the_whole_image) {
    Capabilities caps = baseline_capabilities();
    caps.sixel_graphics = true;
    HeadlessTerminal term(Size{10, 10}, caps);
    Presenter presenter(term);
    Surface s = make_surface(10, 10);
    // A 4x4-cell anchor mapped onto an 8x8 image (2 px/cell). Left half
    // (cols 0-1) is red, right half (cols 2-3) is blue.
    auto image = std::make_shared<Image>(8, 8);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            image->set_pixel(x, y, x < 4 ? Image::Rgba{255, 0, 0, 255} : Image::Rgba{0, 0, 255, 255});
    // Occlusion left only the LEFT half of the anchor visible.
    std::vector<RasterSlice> rasters{{1, Rect{0, 0, 2, 4}, Rect{0, 0, 4, 4}, image, true}};
    presenter.present(s.view(), CursorState{}, rasters);
    CK_CHECK(term.written_bytes().find("#0;2;100;0;0") != std::string_view::npos);  // red present
    CK_CHECK(term.written_bytes().find("0;0;100") == std::string_view::npos);        // blue absent: cropped out
}

CK_TEST(raster_slice_is_positioned_at_its_visible_rect) {
    Capabilities caps = baseline_capabilities();
    caps.sixel_graphics = true;
    HeadlessTerminal term(Size{10, 10}, caps);
    Presenter presenter(term);
    Surface s = make_surface(10, 10);
    const auto image = std::make_shared<Image>(4, 4);
    std::vector<RasterSlice> rasters{{1, Rect{3, 2, 2, 2}, Rect{3, 2, 2, 2}, image, true}};
    presenter.present(s.view(), CursorState{}, rasters);
    // Cursor move to row2+1;col3+1 = "3;4H" must precede the sixel DCS.
    const std::size_t move_pos = term.written_bytes().find(";4H");
    const std::size_t dcs_pos = term.written_bytes().find("\x1BP0;0;0q");
    CK_CHECK(move_pos != std::string_view::npos);
    CK_CHECK(dcs_pos != std::string_view::npos);
    CK_CHECK(move_pos < dcs_pos);
}

CK_TEST(moving_an_active_raster_repaints_its_old_cells_and_removes_stale_virtual_pixels) {
    HeadlessTerminal term(Size{4, 1}, headless_sixel_profile());
    Presenter presenter(term);
    Surface surface = make_surface(4, 1, " ");
    auto image = std::make_shared<Image>(9, 18);
    for (int y = 0; y < image->height(); ++y)
        for (int x = 0; x < image->width(); ++x) image->set_pixel(x, y, Image::Rgba{255, 0, 0, 255});

    presenter.present(surface.view(), CursorState{},
                      {{1, Rect{0, 0, 1, 1}, Rect{0, 0, 1, 1}, image, true}});
    CK_CHECK(term.display().raster_plane().pixel(0, 0).a == 255);
    term.clear_written();

    presenter.present(surface.view(), CursorState{},
                      {{1, Rect{2, 0, 1, 1}, Rect{2, 0, 1, 1}, image, true}});
    CK_CHECK(!term.written_bytes().empty());
    CK_CHECK(term.display().raster_plane().pixel(0, 0).a == 0);
    CK_CHECK(term.display().raster_plane().pixel(18, 0).r == 255);
    CK_CHECK(term.display().raster_plane().pixel(18, 0).a == 255);
}

// --- OSC safety ------------------------------------------------------------------

CK_TEST(sanitize_osc_text_strips_embedded_terminator_bytes) {
    CK_CHECK(sanitize_osc_text("hello") == "hello");
    CK_CHECK(sanitize_osc_text(std::string_view("a\x1B" "b\x07" "c")) == "abc");
}

CK_TEST(sanitize_osc_text_empty_input) { CK_CHECK(sanitize_osc_text("").empty()); }

// --- Moving a raster ------------------------------------------------------

namespace {
std::vector<ckv::RasterSlice> one_raster(Rect where, const std::shared_ptr<ckv::Image>& image) {
    return {ckv::RasterSlice{1, where, where, image, false}};
}
std::shared_ptr<ckv::Image> solid_image(int w, int h) {
    auto image = std::make_shared<ckv::Image>(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) image->set_pixel(x, y, ckv::Image::Rgba{200, 30, 30, 255});
    return image;
}
}  // namespace

CK_TEST(a_raster_moved_left_repaints_every_cell_it_vacated) {
    Capabilities caps = headless_sixel_profile();
    caps.cell_pixels = Size{10, 20};
    HeadlessTerminal term(Size{20, 4}, caps);
    Presenter presenter(term);
    Surface s = make_surface(20, 4, ".");
    auto image = solid_image(40, 40);  // 4 cells x 2 rows at 10x20

    presenter.present(s.view(), CursorState{}, one_raster(Rect{8, 1, 4, 2}, image));
    term.clear_written();

    presenter.present(s.view(), CursorState{}, one_raster(Rect{6, 1, 4, 2}, image));
    const std::string out{term.written_bytes()};

    // Columns 10 and 11 are no longer under the image and must be painted
    // back to desktop content. Nothing else can put them right: a Sixel
    // paints, it never erases what it moved away from.
    CK_CHECK(out.find("..") != std::string::npos);
}

CK_TEST(a_raster_that_disappears_repaints_the_cells_it_held) {
    Capabilities caps = headless_sixel_profile();
    caps.cell_pixels = Size{10, 20};
    HeadlessTerminal term(Size{20, 4}, caps);
    Presenter presenter(term);
    Surface s = make_surface(20, 4, ".");
    auto image = solid_image(40, 40);

    presenter.present(s.view(), CursorState{}, one_raster(Rect{8, 1, 4, 2}, image));
    term.clear_written();

    presenter.present(s.view(), CursorState{}, {});
    const std::string out{term.written_bytes()};
    CK_CHECK(out.find("....") != std::string::npos);
}

CK_TEST(a_moved_raster_also_repaints_the_cell_row_its_pixels_could_bleed_into) {
    // The image is 2 rows and 5 px of a third: whatever the terminal makes
    // of that, it can put pixels on row 3, so row 3 has to be repainted when
    // the image leaves. Nothing else will ever clear it.
    Capabilities caps = headless_sixel_profile();
    caps.cell_pixels = Size{10, 20};
    HeadlessTerminal term(Size{20, 6}, caps);
    Presenter presenter(term);
    Surface s = make_surface(20, 6, ".");
    auto image = solid_image(40, 45);

    presenter.present(s.view(), CursorState{}, one_raster(Rect{8, 1, 4, 2}, image));
    term.clear_written();
    presenter.present(s.view(), CursorState{}, one_raster(Rect{2, 1, 4, 2}, image));
    const std::string out{term.written_bytes()};

    // Row 3 (1-based row 4) must appear in the repaint, for the columns the
    // image no longer stands on.
    CK_CHECK(out.find("\x1B[4;") != std::string::npos);
}
