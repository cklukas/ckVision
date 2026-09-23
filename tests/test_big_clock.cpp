// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/big_clock.hpp"

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using namespace ckv::widgets;

namespace {

struct Standalone {
    ckv::ui::RoleRegistry registry;
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::ui::Context context() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

// What the face drew, read back from a surface as the reader would see it.
struct Drawn {
    ckv::scene::Surface surface;
    ckv::Style lit;

    std::string row(int y) const {
        std::string out;
        for (int x = 0; x < surface.size().width; ++x) out += surface.at(ckv::Point{x, y}).grapheme();
        return out;
    }
    // The lit cells of one row as '#' and ' ', which is the glyph mask's own
    // spelling, so a drawn row can be compared with big_glyph_rows().
    std::string mask(int y) const {
        std::string out;
        for (int x = 0; x < surface.size().width; ++x)
            out += surface.at(ckv::Point{x, y}).style().bg == lit.bg ? '#' : ' ';
        return out;
    }
    int lit_cells() const {
        int count = 0;
        for (int y = 0; y < surface.size().height; ++y)
            for (const char c : mask(y)) count += c == '#' ? 1 : 0;
        return count;
    }
};

Drawn draw(BigClockView& face, Standalone& s, ckv::Size size) {
    face.set_bounds(Rect{0, 0, size.width, size.height});
    Drawn drawn{ckv::scene::Surface(size, ckv::Cell::from_grapheme(" ", ckv::Style{})),
                s.theme.resolve(s.registry.find("ckv.menu.bar.active"))};
    ckv::scene::Painter painter(drawn.surface, Rect{0, 0, size.width, size.height});
    face.draw(painter);
    return drawn;
}

}  // namespace

// --- Glyphs ------------------------------------------------------------------

CK_TEST(block_glyphs_are_five_rows_of_equal_width_with_one_column_between_them) {
    const std::vector<std::string> rows = big_glyph_rows("12:34");
    CK_CHECK(rows.size() == static_cast<std::size_t>(kBigGlyphHeight));
    // Four five-column digits, a one-column colon, and four gaps.
    CK_CHECK(big_glyph_width("12:34") == 4 * 5 + 1 + 4);
    for (const std::string& row : rows) CK_CHECK(static_cast<int>(row.size()) == big_glyph_width("12:34"));
    // The eight reads as a closed box with a bar through its middle.
    const std::vector<std::string> eight = big_glyph_rows("8");
    CK_CHECK(eight[0] == "#####");
    CK_CHECK(eight[1] == "#   #");
    CK_CHECK(eight[2] == "#####");
    CK_CHECK(eight[3] == "#   #");
    CK_CHECK(eight[4] == "#####");
    // The 1 fills its cell from the middle: a segment 1 hugging the right
    // edge left four blank columns in front of it, and 18:19 read "18: 19".
    const std::vector<std::string> one = big_glyph_rows("1");
    CK_CHECK(one[2] == "  #  ");
    for (const std::string& row : one) CK_CHECK(row[0] == ' ' && row[4] == ' ');
    CK_CHECK(big_glyph_width("") == 0);
    CK_CHECK(big_glyph_rows("")[0].empty());
}

CK_TEST(every_digit_has_a_glyph_of_its_own) {
    // Ten shapes, no two alike: a clock whose 6 and 8 were the same glyph
    // would be wrong for a tenth of every hour and pass any test that only
    // looked at one digit.
    std::set<std::vector<std::string>> shapes;
    for (char c = '0'; c <= '9'; ++c) {
        CK_CHECK(has_big_glyph(c));
        shapes.insert(big_glyph_rows(std::string(1, c)));
    }
    CK_CHECK(shapes.size() == 10);
    for (const char c : std::string(":-./ ")) CK_CHECK(has_big_glyph(c));
}

CK_TEST(a_character_without_a_glyph_leaves_a_gap_rather_than_closing_up) {
    CK_CHECK(!has_big_glyph('x'));
    CK_CHECK(big_glyph_width("1x1") == big_glyph_width("101"));
    const std::vector<std::string> rows = big_glyph_rows("1x1");
    for (const std::string& row : rows) CK_CHECK(row.substr(6, 5) == "     ");
}

// --- What the face shows ---------------------------------------------------

CK_TEST(a_big_clock_shows_the_time_the_date_or_both_from_one_reading) {
    Standalone s;
    BigClockView face;
    face.set_context(s.context());
    DateTimeValue now{DateValue{2026, 9, 23}, TimeValue{14, 5, 9}};
    face.set_moment_provider([&] { return now; });

    CK_CHECK(face.content() == BigClockContent::Time);
    CK_CHECK(face.lines() == std::vector<std::string>{"14:05:09"});
    face.set_show_seconds(false);
    CK_CHECK(face.lines() == std::vector<std::string>{"14:05"});
    face.set_content(BigClockContent::Date);
    CK_CHECK(face.lines() == std::vector<std::string>{"2026-09-23"});
    face.set_content(BigClockContent::DateAndTime);
    CK_CHECK((face.lines() == std::vector<std::string>{"2026-09-23", "14:05"}));
    CK_CHECK(face.caption().empty());
}

CK_TEST(a_twelve_hour_face_puts_the_meridiem_word_under_the_digits) {
    Standalone s;
    BigClockView face;
    face.set_context(s.context());
    DateTimeValue now{DateValue{2026, 9, 23}, TimeValue{0, 30, 0}};
    face.set_moment_provider([&] { return now; });
    face.set_show_seconds(false);
    face.set_hour_format(HourFormat::TwelveHour);
    CK_CHECK(face.lines() == std::vector<std::string>{"12:30"});  // midnight is twelve
    CK_CHECK(face.caption() == "AM");
    face.set_meridiem_labels("vorm.", "nachm.");
    now.time = TimeValue{14, 5, 0};
    face.set_meridiem_labels("vorm.", "nachm.");  // force a re-read
    CK_CHECK(face.lines() == std::vector<std::string>{"2:05"});
    CK_CHECK(face.caption() == "nachm.");
    face.set_meridiem_labels("", "");
    CK_CHECK(face.caption().empty());
}

// --- Drawing ---------------------------------------------------------------

CK_TEST(a_face_that_fits_draws_its_glyphs_centred_in_the_theme_s_lit_colour) {
    Standalone s;
    BigClockView face;
    face.set_context(s.context());
    face.on_attached();
    face.set_moment_provider([] { return DateTimeValue{DateValue{2026, 9, 23}, TimeValue{8, 8, 8}}; });

    const Drawn drawn = draw(face, s, ckv::Size{60, 11});
    CK_CHECK(face.drawn_large());
    // "08:08:08" is 39 columns; centred in 60 it starts at column 10, and
    // five rows in 11 start at row 3.
    const std::vector<std::string> glyphs = big_glyph_rows("08:08:08");
    for (int row = 0; row < kBigGlyphHeight; ++row)
        CK_CHECK(drawn.mask(3 + row).substr(10, glyphs[0].size()) == glyphs[static_cast<std::size_t>(row)]);
    // Nothing lit outside the block.
    int expected = 0;
    for (const std::string& row : glyphs) expected += static_cast<int>(std::count(row.begin(), row.end(), '#'));
    CK_CHECK(drawn.lit_cells() == expected);
}

CK_TEST(a_face_follows_its_window_between_block_glyphs_and_plain_text) {
    // The environment changes under a running face: the window it fills is
    // made too small for the glyphs, then large again. Too small still tells
    // the time, as text — and back at full size it is glyphs again, not text
    // left over from the small moment.
    Standalone s;
    BigClockView face;
    face.set_context(s.context());
    face.on_attached();
    face.set_content(BigClockContent::DateAndTime);
    face.set_moment_provider([] { return DateTimeValue{DateValue{2026, 9, 23}, TimeValue{14, 5, 9}}; });

    Drawn drawn = draw(face, s, ckv::Size{80, 24});
    CK_CHECK(face.drawn_large());
    CK_CHECK(drawn.lit_cells() > 0);
    CK_CHECK(drawn.row(11).find("14:05:09") == std::string::npos);

    // Wide enough, but one row short of date + gap + time.
    drawn = draw(face, s, ckv::Size{80, 2 * kBigGlyphHeight});
    CK_CHECK(!face.drawn_large());
    CK_CHECK(drawn.lit_cells() == 0);
    CK_CHECK(drawn.row(4).find("2026-09-23") != std::string::npos);
    CK_CHECK(drawn.row(5).find("14:05:09") != std::string::npos);

    // Tall enough, but narrower than the date's 55 columns.
    drawn = draw(face, s, ckv::Size{54, 24});
    CK_CHECK(!face.drawn_large());
    CK_CHECK(drawn.lit_cells() == 0);

    drawn = draw(face, s, ckv::Size{80, 24});
    CK_CHECK(face.drawn_large());
    CK_CHECK(drawn.lit_cells() > 0);
}

// --- Running ---------------------------------------------------------------

CK_TEST(a_running_face_repaints_when_what_it_shows_changes_and_not_otherwise) {
    ckv::term::HeadlessTerminal term(ckv::Size{60, 14});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);

    DateTimeValue now{DateValue{2026, 9, 23}, TimeValue{23, 59, 0}};
    auto* face = static_cast<BigClockView*>(app.root().add_child(std::make_unique<BigClockView>()));
    face->set_bounds(Rect{0, 0, 60, 14});
    face->set_content(BigClockContent::DateAndTime);
    face->set_show_seconds(false);
    face->set_moment_provider([&] { return now; });
    app.step(0);
    CK_CHECK(!term.written_bytes().empty());

    // Seconds pass inside one minute: the face ticks and asks for nothing.
    term.clear_written();
    std::int64_t at = clock.now_nanos();
    const auto tick = [&] {
        at += 1'000'000'000;
        clock.advance(1'000'000'000);
        app.step(at);
    };
    for (int i = 1; i <= 5; ++i) {
        now.time.second = i;
        tick();
    }
    CK_CHECK(term.written_bytes().empty());

    // Midnight: the date and the time turn over together, from one reading.
    now = DateTimeValue{DateValue{2026, 9, 24}, TimeValue{0, 0, 0}};
    tick();
    CK_CHECK(!term.written_bytes().empty());
    CK_CHECK((face->lines() == std::vector<std::string>{"2026-09-24", "00:00"}));
}

CK_TEST(any_key_or_a_click_on_a_focused_face_asks_for_it_to_be_dismissed) {
    ckv::term::HeadlessTerminal term(ckv::Size{60, 14});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);

    auto* face = static_cast<BigClockView*>(app.root().add_child(std::make_unique<BigClockView>()));
    face->set_bounds(Rect{0, 0, 60, 14});
    face->set_moment_provider([] { return DateTimeValue{}; });
    int dismissed = 0;
    face->on_dismiss = [&] { ++dismissed; };
    app.set_focus(face);
    app.step(0);

    // Through the application, the way a reader's key arrives. A printable
    // key and a special key alike: the face has no keys of its own to keep.
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "q"}}));
    CK_CHECK(dismissed == 1);
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(dismissed == 2);

    // A key the application binds is its command, not a dismissal: the
    // command runs, and the face stays up.
    int ran = 0;
    const ckv::ui::CommandId next = app.commands().declare(ckv::ui::CommandDescriptor{
        .key = "test.next", .title = "Next", .category = "test", .handler = [&] { ++ran; }});
    app.commands().bind_key(KeyChord{Key::F6, Modifier::None, ""}, next);
    app.dispatch(ckv::KeyEvent{KeyChord{Key::F6, Modifier::None, ""}});
    CK_CHECK(ran == 1);
    CK_CHECK(dismissed == 2);

    // A press alone is not yet a click.
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{30, 7},
                                 std::nullopt, Modifier::None});
    CK_CHECK(dismissed == 2);
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{30, 7},
                                 std::nullopt, Modifier::None});
    CK_CHECK(dismissed == 3);
}

CK_TEST(a_face_stays_up_while_the_reader_works_elsewhere) {
    // The reader shows the time in one window and goes on typing in another:
    // the face stays. Coming back by clicking it only gives it the keyboard;
    // the next key, or a click once it has the keyboard, is what puts it away.
    ckv::term::HeadlessTerminal term(ckv::Size{60, 14});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);

    auto* face = static_cast<BigClockView*>(app.root().add_child(std::make_unique<BigClockView>()));
    face->set_bounds(Rect{0, 0, 30, 14});
    face->set_moment_provider([] { return DateTimeValue{}; });
    auto* elsewhere = static_cast<BigClockView*>(app.root().add_child(std::make_unique<BigClockView>()));
    elsewhere->set_bounds(Rect{30, 0, 30, 14});
    int dismissed = 0;
    face->on_dismiss = [&] { ++dismissed; };
    app.set_focus(face);
    app.step(0);

    app.set_focus(elsewhere);
    CK_CHECK(dismissed == 0);
    CK_CHECK(app.focused() == elsewhere);

    const auto click = [&](ckv::Point at) {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, at, std::nullopt,
                                     Modifier::None});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, at, std::nullopt,
                                     Modifier::None});
    };
    click(ckv::Point{10, 5});  // back to the face: focuses it, dismisses nothing
    CK_CHECK(dismissed == 0);
    CK_CHECK(app.focused() == face);
    click(ckv::Point{10, 5});  // now it has the keyboard: this one asks
    CK_CHECK(dismissed == 1);
}
