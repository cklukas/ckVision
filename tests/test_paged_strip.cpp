// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/paged_strip.hpp"

#include <string>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::ManualClock;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::PagedStrip;
namespace ui = ckv::ui;

namespace {

struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);

    std::vector<PagedStrip::Item> items;
    PagedStrip strip;

    Fixture() {
        strip.set_item_source([this] { return items; });
        strip.set_context(ui::Context{&theme, &registry, &app});
    }

    void size(int width) { strip.set_bounds(Rect{0, 0, width, 1}); }

    // Items whose content width is exactly their text's, which is the ordinary
    // case; the icon case is its own test below.
    void list(std::vector<std::string> labels) {
        items.clear();
        for (std::string& label : labels) {
            const int width = ckv::text::text_width(label);
            items.push_back(PagedStrip::Item{width, std::move(label), false});
        }
        strip.refresh_items();
    }

    std::string row(int width) {
        Surface surface(ckv::Size{width, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
        Painter painter(surface, Rect{0, 0, width, 1});
        strip.draw(painter);
        std::string out;
        for (int x = 0; x < width; ++x) out += surface.at(Point{x, 0}).grapheme();
        return out;
    }
};

ckv::MouseEvent press(int x, ckv::MouseButton button = ckv::MouseButton::Left) {
    return ckv::MouseEvent{ckv::MouseAction::Down, button, Point{x, 0}, std::nullopt,
                           Modifier::None};
}

ckv::MouseEvent move(int x) {
    return ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, Point{x, 0}, std::nullopt,
                           Modifier::None};
}

ckv::MouseEvent release(int x) {
    return ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, Point{x, 0}, std::nullopt,
                           Modifier::None};
}

}  // namespace

// --- The glyphs the strip steers with -------------------------------------

CK_TEST(every_steering_glyph_is_one_column_wide_by_the_librarys_own_authority) {
    // The reason this test exists rather than a comment: several geometric
    // shapes are East Asian Ambiguous, and a two-cell triangle would shear the
    // row it is supposed to be steering. If a Unicode data bump ever widens
    // one of these, it fails here instead of in a reader's terminal.
    CK_CHECK(ckv::text::text_width("▼") == 1);  // U+25BC
    CK_CHECK(ckv::text::text_width("▲") == 1);  // U+25B2
    CK_CHECK(ckv::text::text_width("◁") == 1);  // U+25C1
    CK_CHECK(ckv::text::text_width("▷") == 1);  // U+25B7
}

// --- One page: the strip is a plain row -----------------------------------

CK_TEST(items_that_all_fit_take_their_natural_width_and_no_chrome_is_drawn) {
    Fixture f;
    f.size(40);
    f.list({"Alpha", "Bravo", "Charlie"});

    CK_CHECK(f.strip.page_count() == 1);
    CK_CHECK(f.strip.page() == 0);
    // An index that always reads 1/1 is a column spent saying nothing.
    CK_CHECK(f.strip.page_index_text().empty());
    CK_CHECK(f.strip.chrome().index_x == -1);
    CK_CHECK(f.strip.chrome().previous_x == -1);
    CK_CHECK(f.strip.chrome().next_x == -1);
    CK_CHECK(f.strip.chrome().collapse_x == -1);
    CK_CHECK(f.strip.chrome().items_x == 0);
    CK_CHECK(f.strip.chrome().items_width == 40);

    const std::vector<PagedStrip::Placement> placed = f.strip.placed_items();
    CK_CHECK(placed.size() == 3);
    // One padding cell either side of the content, one blank cell between.
    CK_CHECK(placed[0].x == 0 && placed[0].width == 7 && placed[0].text == "Alpha");
    CK_CHECK(placed[1].x == 8 && placed[1].width == 7 && placed[1].text == "Bravo");
    CK_CHECK(placed[2].x == 16 && placed[2].width == 9 && placed[2].text == "Charlie");
    // Nothing elided: paging is the answer to "too narrow", not truncation.
    CK_CHECK(!f.strip.shows_previous_control());
    CK_CHECK(!f.strip.shows_next_control());
}

CK_TEST(an_item_that_carries_an_icon_is_as_wide_as_its_provider_says) {
    // The layout never measures the text: an item that draws a status glyph
    // before its label pays for it in `width` and the columns follow.
    Fixture f;
    f.size(40);
    f.items = {PagedStrip::Item{7, "* Alpha", false}, PagedStrip::Item{5, "Bravo", false}};
    f.strip.refresh_items();

    const std::vector<PagedStrip::Placement> placed = f.strip.placed_items();
    CK_CHECK(placed.size() == 2);
    CK_CHECK(placed[0].width == 9 && placed[0].text == "* Alpha");
    CK_CHECK(placed[1].x == 10 && placed[1].width == 7);
}

CK_TEST(an_icon_role_recolours_the_leading_cells_and_nothing_else_on_the_row) {
    // What a taskbar needs to say "this is the window you are in" the way a
    // window frame says it: the mark is lit, the name beside it is not, and
    // the row's own background runs unbroken underneath both. Only the
    // FOREGROUND comes from the role — a mark that brought its own background
    // would punch a hole in the highlight it is standing on.
    Fixture f;
    f.size(40);
    // A control colour the selected row does not already fill with — the
    // classic theme pairs both with green, which is the collision the
    // legibility floor below is about, and would leave this test asserting
    // the fallback while believing it asserted the borrow.
    f.theme.set(f.roles.window_control, ckv::Style{ckv::Color::rgb(255, 255, 85),
                                                   ckv::Color::rgb(0, 0, 170), ckv::Attr::Bold});
    f.items = {PagedStrip::Item{7, "* Alpha", true, 1, f.roles.window_control},
               PagedStrip::Item{7, "* Bravo", false, 1, ui::kInvalidRole}};
    f.strip.refresh_items();

    Surface surface(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 40, 1});
    f.strip.draw(painter);

    const std::vector<PagedStrip::Placement> placed = f.strip.placed_items();
    const int lit = placed[0].x + PagedStrip::kItemPadding;
    const int plain = placed[1].x + PagedStrip::kItemPadding;
    // The mark is where the test believes it is on both rows, before any
    // claim about colour: an assertion made against a blank cell passes for
    // the wrong reason.
    CK_CHECK(surface.at(Point{lit, 0}).grapheme() == "*");
    CK_CHECK(surface.at(Point{plain, 0}).grapheme() == "*");

    const ckv::Style control = f.theme.resolve(f.roles.window_control);
    const ckv::Style selected = f.theme.resolve(f.roles.status_line_selected);
    CK_CHECK(surface.at(Point{lit, 0}).style().fg == control.fg);
    CK_CHECK(surface.at(Point{lit, 0}).style().bg == selected.bg);
    // One cell on: the label is the row's own style, icon role or not.
    CK_CHECK(surface.at(Point{lit + 2, 0}).style().fg == selected.fg);
    // And an item that named no role is one style end to end.
    CK_CHECK(surface.at(Point{plain, 0}).style().fg ==
             surface.at(Point{plain + 2, 0}).style().fg);
}

CK_TEST(a_mark_whose_role_colour_is_the_rows_own_background_is_drawn_legibly_instead) {
    // The legibility floor. A borrowed foreground lands on someone else's
    // background, and two of the library's own themes pair the window control
    // with a status line that fills with exactly that colour — classic green
    // on green, mono foreground on inverted foreground. Drawn faithfully, the
    // mark would be a gap in the middle of the row; the row's own style wins
    // instead, and the highlight goes on saying which row it is.
    Fixture f;
    f.size(40);
    const ckv::Style selected = f.theme.resolve(f.roles.status_line_selected);
    f.theme.set(f.roles.window_control, ckv::Style{selected.bg, selected.bg, ckv::Attr::Bold});
    f.items = {PagedStrip::Item{7, "* Alpha", true, 1, f.roles.window_control}};
    f.strip.refresh_items();

    Surface surface(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 40, 1});
    f.strip.draw(painter);

    const int mark = f.strip.placed_items()[0].x + PagedStrip::kItemPadding;
    CK_CHECK(surface.at(Point{mark, 0}).grapheme() == "*");
    CK_CHECK(surface.at(Point{mark, 0}).style().fg != surface.at(Point{mark, 0}).style().bg);
    // Not merely "not invisible": it is the row's own foreground, the same
    // one the name beside it is drawn in.
    CK_CHECK(surface.at(Point{mark, 0}).style().fg == selected.fg);
    CK_CHECK(surface.at(Point{mark, 0}).style().fg == surface.at(Point{mark + 2, 0}).style().fg);
}

// --- Paging ----------------------------------------------------------------

CK_TEST(a_strip_too_narrow_for_its_items_pages_them_instead_of_truncating) {
    Fixture f;
    f.size(20);
    f.list({"Alpha", "Bravo", "Charlie"});

    // 20 columns: one control, a three-cell index, a blank, then 13 cells of
    // items, a blank and the right control.
    CK_CHECK(f.strip.chrome().previous_x == 0);
    CK_CHECK(f.strip.chrome().index_x == 1);
    CK_CHECK(f.strip.chrome().index_width == 3);
    CK_CHECK(f.strip.chrome().items_x == 5);
    CK_CHECK(f.strip.chrome().items_width == 13);
    CK_CHECK(f.strip.chrome().next_x == 19);
    CK_CHECK(f.strip.page_count() == 3);

    // Each page carries whole items at their natural width.
    CK_CHECK(f.strip.page_index_text() == "1/3");
    CK_CHECK(f.strip.placed_items().size() == 1);
    CK_CHECK(f.strip.placed_items()[0].index == 0);
    CK_CHECK(f.strip.placed_items()[0].x == 5);
    CK_CHECK(f.strip.placed_items()[0].width == 7);
    CK_CHECK(f.strip.placed_items()[0].text == "Alpha");

    CK_CHECK(f.strip.next_page());
    CK_CHECK(f.strip.page_index_text() == "2/3");
    CK_CHECK(f.strip.placed_items()[0].index == 1);
    // Items keep their columns as the reader pages: the chrome band is the
    // same on every page.
    CK_CHECK(f.strip.placed_items()[0].x == 5);

    CK_CHECK(f.strip.next_page());
    CK_CHECK(f.strip.page_index_text() == "3/3");
    CK_CHECK(f.strip.placed_items()[0].index == 2);
    CK_CHECK(f.strip.placed_items()[0].text == "Charlie");

    // And there is nowhere further to go.
    CK_CHECK(!f.strip.next_page());
    CK_CHECK(f.strip.page() == 2);
}

CK_TEST(a_page_control_is_there_only_while_it_can_do_something) {
    Fixture f;
    f.size(20);
    f.list({"Alpha", "Bravo", "Charlie"});

    // First page: the previous cell is reserved but dead, so a press on it is
    // over nothing at all.
    CK_CHECK(!f.strip.shows_previous_control());
    CK_CHECK(f.strip.shows_next_control());
    CK_CHECK(f.strip.hit_test(Point{f.strip.chrome().previous_x, 0}).region ==
             PagedStrip::Region::None);
    CK_CHECK(f.strip.hit_test(Point{f.strip.chrome().next_x, 0}).region ==
             PagedStrip::Region::NextPage);
    CK_CHECK(!f.strip.previous_page());

    CK_CHECK(f.strip.set_page(2));
    CK_CHECK(f.strip.shows_previous_control());
    CK_CHECK(!f.strip.shows_next_control());
    CK_CHECK(f.strip.hit_test(Point{f.strip.chrome().next_x, 0}).region ==
             PagedStrip::Region::None);
    // The page index is information, never a button.
    CK_CHECK(f.strip.hit_test(Point{f.strip.chrome().index_x, 0}).region ==
             PagedStrip::Region::None);
}

CK_TEST(the_page_controls_are_clicked_and_report_every_move) {
    Fixture f;
    f.size(20);
    f.list({"Alpha", "Bravo", "Charlie"});
    std::vector<std::size_t> heard;
    f.strip.on_page_changed = [&](std::size_t page) { heard.push_back(page); };

    // A control acts on the press, the way a scrollbar's arrows do.
    CK_CHECK(f.strip.on_mouse(press(f.strip.chrome().next_x)));
    CK_CHECK(f.strip.page() == 1);
    CK_CHECK(f.strip.on_mouse(press(f.strip.chrome().next_x)));
    CK_CHECK(f.strip.page() == 2);
    CK_CHECK(f.strip.on_mouse(press(f.strip.chrome().previous_x)));
    CK_CHECK(f.strip.page() == 1);
    CK_CHECK(heard.size() == 3);
    CK_CHECK(heard[0] == 1 && heard[1] == 2 && heard[2] == 1);
}

CK_TEST(the_index_is_wide_enough_for_the_page_count_on_every_page) {
    // Eleven pages means two digits, and the band is that wide from page one
    // so it does not breathe as the reader walks from 9 to 10. Its width feeds
    // back into how much room the items get, which is why the layout settles
    // it at a fixed point rather than guessing.
    Fixture f;
    f.size(14);
    f.list({"Item01", "Item02", "Item03", "Item04", "Item05", "Item06", "Item07", "Item08",
            "Item09", "Item10", "Item11"});
    CK_CHECK(f.strip.page_count() == 11);
    CK_CHECK(f.strip.chrome().index_width == 5);  // "NN/NN"
    CK_CHECK(f.strip.chrome().index_x == 1);
    CK_CHECK(f.strip.chrome().items_x == 7);
    CK_CHECK(f.strip.chrome().items_width == 5);
    CK_CHECK(f.strip.page_index_text() == "1/11");
    CK_CHECK(f.strip.set_page(8));
    CK_CHECK(f.strip.page_index_text() == "9/11");
    // The band did not move under the shorter number.
    CK_CHECK(f.strip.chrome().index_width == 5);
    CK_CHECK(f.strip.chrome().items_x == 7);
}

// --- Revalidation ----------------------------------------------------------

CK_TEST(adding_and_removing_items_re_pages_the_strip) {
    Fixture f;
    f.size(20);
    f.list({"Alpha", "Bravo", "Charlie"});
    CK_CHECK(f.strip.page_count() == 3);

    f.list({"Alpha", "Bravo", "Charlie", "Delta"});
    CK_CHECK(f.strip.page_count() == 4);

    f.list({"Alpha", "Bravo"});
    // Two items fit side by side in the full width, so there is nothing left
    // to page and the chrome goes with it.
    CK_CHECK(f.strip.page_count() == 1);
    CK_CHECK(f.strip.chrome().next_x == -1);
    CK_CHECK(f.strip.placed_items().size() == 2);
}

CK_TEST(a_removal_that_empties_the_current_page_falls_back_to_the_previous_one) {
    Fixture f;
    f.size(20);
    f.list({"Alpha", "Bravo", "Charlie", "Delta"});
    CK_CHECK(f.strip.page_count() == 4);
    CK_CHECK(f.strip.set_page(3));

    std::vector<std::size_t> heard;
    f.strip.on_page_changed = [&](std::size_t page) { heard.push_back(page); };

    // The last item goes — a terminal closed — and the page the reader was on
    // no longer exists. It must not show a blank row under a 4/3 index.
    f.list({"Alpha", "Bravo", "Charlie"});
    CK_CHECK(f.strip.page_count() == 3);
    CK_CHECK(f.strip.page() == 2);
    CK_CHECK(f.strip.page_index_text() == "3/3");
    CK_CHECK(f.strip.placed_items().size() == 1);
    CK_CHECK(f.strip.placed_items()[0].index == 2);
    CK_CHECK(heard.size() == 1 && heard[0] == 2);
}

CK_TEST(removing_an_item_from_an_earlier_page_leaves_the_reader_on_their_page) {
    Fixture f;
    f.size(20);
    f.list({"Alpha", "Bravo", "Charlie", "Delta"});
    CK_CHECK(f.strip.set_page(2));

    std::vector<std::size_t> heard;
    f.strip.on_page_changed = [&](std::size_t page) { heard.push_back(page); };

    // The first item goes. The page the reader chose still exists, so they
    // stay on it and hear nothing.
    f.list({"Bravo", "Charlie", "Delta"});
    CK_CHECK(f.strip.page_count() == 3);
    CK_CHECK(f.strip.page() == 2);
    CK_CHECK(heard.empty());
}

CK_TEST(a_refresh_that_changes_nothing_leaves_the_page_where_it_was) {
    Fixture f;
    f.size(20);
    f.list({"Alpha", "Bravo", "Charlie"});
    CK_CHECK(f.strip.set_page(1));
    f.strip.refresh_items();
    f.strip.refresh_items();
    CK_CHECK(f.strip.page() == 1);
}

CK_TEST(a_strip_made_narrower_re_pages_and_clamps_the_page_it_was_showing) {
    Fixture f;
    f.size(40);
    f.list({"Alpha", "Bravo", "Charlie"});
    CK_CHECK(f.strip.page_count() == 1);

    f.size(20);
    CK_CHECK(f.strip.page_count() == 3);
    CK_CHECK(f.strip.set_page(2));

    // Wide again: one page, and the reader is on it rather than on a page
    // number that no longer exists.
    f.size(40);
    CK_CHECK(f.strip.page_count() == 1);
    CK_CHECK(f.strip.page() == 0);
}

// --- Elision, in the one case it survives ----------------------------------

CK_TEST(an_item_wider_than_the_whole_area_is_alone_on_its_page_and_elided) {
    Fixture f;
    f.size(20);
    f.list({"Extraordinarily long", "Beta"});
    CK_CHECK(f.strip.page_count() == 2);

    // 13 cells of item area against a 22-cell box: nowhere to move it to, so
    // this is where elision still happens.
    const std::vector<PagedStrip::Placement> placed = f.strip.placed_items();
    CK_CHECK(placed.size() == 1);
    CK_CHECK(placed[0].width == 13);
    CK_CHECK(placed[0].text == "Extraordin…");
    CK_CHECK(ckv::text::text_width(placed[0].text) == 11);

    // Its neighbour keeps its own name whole on the next page.
    CK_CHECK(f.strip.next_page());
    CK_CHECK(f.strip.placed_items()[0].text == "Beta");
    CK_CHECK(f.strip.placed_items()[0].width == 6);
}

// --- The collapse toggle ---------------------------------------------------

CK_TEST(the_collapse_toggle_costs_no_column_until_a_host_asks_for_one) {
    Fixture f;
    f.size(40);
    f.list({"Alpha"});
    CK_CHECK(f.strip.chrome().collapse_x == -1);
    CK_CHECK(f.strip.placed_items()[0].x == 0);

    f.strip.set_collapsible(true);
    CK_CHECK(f.strip.chrome().collapse_x == 0);
    CK_CHECK(f.strip.chrome().items_x == 2);
    CK_CHECK(f.strip.placed_items()[0].x == 2);
}

CK_TEST(the_toggle_reports_its_state_and_the_strip_changes_nothing_else) {
    Fixture f;
    f.size(40);
    f.strip.set_collapsible(true);
    f.list({"Alpha", "Bravo"});

    std::vector<bool> heard;
    f.strip.on_collapse_changed = [&](bool collapsed) { heard.push_back(collapsed); };

    const PagedStrip::Chrome before = f.strip.chrome();
    const std::vector<PagedStrip::Placement> laid_out = f.strip.placed_items();

    CK_CHECK(f.strip.hit_test(Point{0, 0}).region == PagedStrip::Region::CollapseToggle);
    CK_CHECK(f.strip.on_mouse(press(0)));
    CK_CHECK(f.strip.collapsed());
    CK_CHECK(heard.size() == 1 && heard[0]);
    // Nothing of the strip's own moved: what collapsing MEANS is the host's.
    CK_CHECK(f.strip.chrome().items_x == before.items_x);
    CK_CHECK(f.strip.chrome().items_width == before.items_width);
    CK_CHECK(f.strip.page_count() == 1);
    CK_CHECK(f.strip.placed_items().size() == laid_out.size());
    CK_CHECK(f.strip.placed_items()[0].x == laid_out[0].x);

    CK_CHECK(f.strip.on_mouse(press(0)));
    CK_CHECK(!f.strip.collapsed());
    CK_CHECK(heard.size() == 2 && !heard[1]);

    // A direct set reports too, and a set that changes nothing does not.
    f.strip.set_collapsed(true);
    CK_CHECK(heard.size() == 3);
    f.strip.set_collapsed(true);
    CK_CHECK(heard.size() == 3);
}

// --- Clicking an item ------------------------------------------------------

CK_TEST(an_item_activates_on_the_release_and_a_press_dragged_away_takes_it_back) {
    Fixture f;
    f.size(40);
    f.list({"Alpha", "Bravo"});
    std::vector<std::size_t> activated;
    f.strip.on_item_activated = [&](std::size_t index) { activated.push_back(index); };

    CK_CHECK(f.strip.on_mouse(press(10)));  // over "Bravo"
    CK_CHECK(f.strip.item_at(Point{10, 0}).value_or(99) == 1);
    CK_CHECK(activated.empty());  // the press alone decides nothing
    CK_CHECK(f.strip.on_mouse(release(10)));
    CK_CHECK(activated.size() == 1 && activated[0] == 1);

    CK_CHECK(f.strip.on_mouse(press(3)));  // over "Alpha"
    CK_CHECK(f.strip.on_mouse(release(30)));  // the blank run past the last item
    CK_CHECK(activated.size() == 1);

    // And the blank run claims nothing at all.
    CK_CHECK(!f.strip.on_mouse(press(30)));
}

CK_TEST(the_strip_says_when_a_press_is_in_flight_so_a_subclass_can_hold_still) {
    // A subclass that resizes its own items on a TIMER needs to know not to,
    // while the reader has one held down: this strip resolves a click by
    // index, so moving the columns under a held pointer either makes the
    // release miss or makes it name a different item.
    struct Probe : PagedStrip {
        using PagedStrip::press_in_flight;
    };
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    std::vector<PagedStrip::Item> items{{5, "Alpha", false}, {5, "Bravo", false}};
    Probe strip;
    strip.set_item_source([&] { return items; });
    strip.set_context(ui::Context{&theme, &registry, &app});
    strip.set_bounds(Rect{0, 0, 40, 1});
    strip.refresh_items();

    CK_CHECK(!strip.press_in_flight());     // at rest
    CK_CHECK(strip.on_mouse(press(10)));    // over "Bravo"
    CK_CHECK(strip.press_in_flight());
    CK_CHECK(strip.on_mouse(release(10)));
    CK_CHECK(!strip.press_in_flight());     // and the release gives it back

    // A press dragged OFF its item is still a press: the claim survives, which
    // is the whole reason the flag is not "the pointer is over an item".
    CK_CHECK(strip.on_mouse(press(3)));
    CK_CHECK(strip.press_in_flight());
    CK_CHECK(strip.on_mouse(move(30)));     // the blank run past the last item
    CK_CHECK(strip.press_in_flight());
    CK_CHECK(strip.on_mouse(release(30)));
    CK_CHECK(!strip.press_in_flight());
}

CK_TEST(a_right_press_is_the_hosts_to_consume_or_to_leave_alone) {
    Fixture f;
    f.size(40);
    f.list({"Alpha", "Bravo"});
    std::vector<std::size_t> asked;
    f.strip.on_item_context_press = [&](std::size_t index, Point) {
        asked.push_back(index);
        return false;  // a host with nothing to offer for this item
    };
    CK_CHECK(!f.strip.on_mouse(press(10, ckv::MouseButton::Right)));
    CK_CHECK(asked.size() == 1 && asked[0] == 1);

    f.strip.on_item_context_press = [](std::size_t, Point) { return true; };
    CK_CHECK(f.strip.on_mouse(press(10, ckv::MouseButton::Right)));
    // Never over the chrome or the empty run.
    CK_CHECK(!f.strip.on_mouse(press(30, ckv::MouseButton::Right)));
}

// --- What actually reaches the cells --------------------------------------

CK_TEST(the_row_draws_the_toggle_the_controls_the_index_and_the_page) {
    Fixture f;
    f.size(20);
    f.strip.set_collapsible(true);
    f.list({"Alpha", "Bravo", "Charlie"});

    CK_CHECK(f.strip.chrome().collapse_x == 0);
    CK_CHECK(f.strip.chrome().previous_x == 2);
    CK_CHECK(f.strip.chrome().index_x == 3);
    CK_CHECK(f.strip.chrome().items_x == 7);
    CK_CHECK(f.strip.chrome().items_width == 11);
    CK_CHECK(f.strip.chrome().next_x == 19);
    CK_CHECK(f.strip.page_count() == 3);

    // Every glyph one column: the row is exactly as wide as the strip.
    CK_CHECK(f.row(20) == "▼  1/3  Alpha      ▷");
    CK_CHECK(f.strip.next_page());
    CK_CHECK(f.row(20) == "▼ ◁2/3  Bravo      ▷");
    CK_CHECK(f.strip.next_page());
    CK_CHECK(f.row(20) == "▼ ◁3/3  Charlie     ");
    f.strip.set_collapsed(true);
    CK_CHECK(f.row(20) == "▲ ◁3/3  Charlie     ");
}

// --- Giving the chrome up, narrower and narrower --------------------------

CK_TEST(the_index_is_the_first_thing_given_up_when_the_row_runs_out_of_room) {
    Fixture f;
    f.size(7);
    f.list({"Alpha", "Bravo", "Charlie"});
    // Function before information: the controls stay, the index goes.
    CK_CHECK(f.strip.chrome().index_x == -1);
    CK_CHECK(f.strip.chrome().previous_x == 0);
    CK_CHECK(f.strip.chrome().next_x == 6);
    CK_CHECK(f.strip.chrome().items_x == 2);
    CK_CHECK(f.strip.chrome().items_width == 3);
    CK_CHECK(f.strip.page_count() == 3);
    CK_CHECK(f.strip.page_index_text() == "1/3");  // still true, just not shown
}

CK_TEST(the_collapse_toggle_is_given_up_before_the_page_controls_are) {
    Fixture f;
    f.size(5);
    f.strip.set_collapsible(true);
    f.list({"Alpha", "Bravo", "Charlie"});
    // Steering the host's furniture matters less than reaching one's items.
    CK_CHECK(f.strip.chrome().collapse_x == -1);
    CK_CHECK(f.strip.chrome().previous_x == 0);
    CK_CHECK(f.strip.chrome().next_x == 4);
    CK_CHECK(f.strip.chrome().items_width == 1);
    CK_CHECK(f.strip.page_count() == 3);
    // One cell still says an item is here.
    CK_CHECK(f.strip.placed_items()[0].text == "…");
}

CK_TEST(a_strip_too_narrow_to_steer_still_counts_its_pages_honestly) {
    Fixture f;
    f.size(3);
    f.list({"Alpha", "Bravo", "Charlie"});
    CK_CHECK(f.strip.chrome().previous_x == -1);
    CK_CHECK(f.strip.chrome().next_x == -1);
    CK_CHECK(!f.strip.shows_next_control());
    // The pages are real even where no control fits to walk them.
    CK_CHECK(f.strip.page_count() == 3);
    CK_CHECK(f.strip.next_page());
    CK_CHECK(f.strip.page() == 1);
}

CK_TEST(a_strip_with_no_items_and_no_width_lays_out_nothing_and_does_not_page) {
    Fixture f;
    f.size(40);
    CK_CHECK(f.strip.page_count() == 0);
    CK_CHECK(f.strip.placed_items().empty());
    CK_CHECK(f.strip.page_index_text().empty());
    CK_CHECK(!f.strip.next_page());

    f.list({"Alpha"});
    f.size(0);
    CK_CHECK(f.strip.page_count() == 0);
    CK_CHECK(f.strip.placed_items().empty());
    CK_CHECK(f.strip.hit_test(Point{0, 0}).region == PagedStrip::Region::None);
}

CK_TEST(the_strip_asks_for_exactly_one_row) {
    Fixture f;
    CK_CHECK((f.strip.vertical_size_hint() == ui::SizeHint{1, 1, 1}));
}
