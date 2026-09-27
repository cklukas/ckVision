// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Hyperlink targets in core (D-088): which targets are terminal hyperlinks,
// the reference-counted LinkTable a grid resolves its cells' link ids
// through, a Cell's link id, and FrameView's target-based cell comparison.
#include "cvision/core/cell.hpp"
#include "cvision/core/frame_view.hpp"
#include "cvision/core/hyperlink.hpp"

#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"

using namespace ckv;

// --- Which targets are terminal hyperlinks ------------------------------------

CK_TEST(an_absolute_uri_of_printable_ascii_is_a_terminal_hyperlink) {
    CK_CHECK(is_valid_hyperlink_target("https://example.org/"));
    CK_CHECK(is_valid_hyperlink_target("file://host/tmp/a%20b.txt"));
    CK_CHECK(is_valid_hyperlink_target("mailto:someone@example.org"));
    CK_CHECK(is_valid_hyperlink_target("x-custom+v1.2:path;with;semicolons?q=1#frag"));
}

CK_TEST(a_relative_reference_or_a_malformed_scheme_is_not) {
    // A terminal has no base to resolve a relative reference against.
    CK_CHECK(!is_valid_hyperlink_target("guide.md"));
    CK_CHECK(!is_valid_hyperlink_target("#section"));
    CK_CHECK(!is_valid_hyperlink_target("/absolute/path"));
    CK_CHECK(!is_valid_hyperlink_target(":no-scheme"));
    CK_CHECK(!is_valid_hyperlink_target("1http://digit-first"));
    CK_CHECK(!is_valid_hyperlink_target("ht_tp://underscore"));
    CK_CHECK(!is_valid_hyperlink_target(""));
}

CK_TEST(a_target_with_a_control_a_space_or_a_non_ascii_byte_is_not) {
    // Every spelling of a string terminator, and everything the OSC 8
    // convention requires to be percent-encoded, is refused, not repaired.
    CK_CHECK(!is_valid_hyperlink_target("https://x.test/\x1B\\"));
    CK_CHECK(!is_valid_hyperlink_target("https://x.test/\x07"));
    CK_CHECK(!is_valid_hyperlink_target("https://x.test/\x9C"));
    CK_CHECK(!is_valid_hyperlink_target("https://x.test/\xC2\x9C"));
    CK_CHECK(!is_valid_hyperlink_target("https://x.test/\x7F"));
    CK_CHECK(!is_valid_hyperlink_target("https://x.test/a b"));
    CK_CHECK(!is_valid_hyperlink_target("https://x.test/caf\xC3\xA9"));
    CK_CHECK(!is_valid_hyperlink_target(std::string_view("https://x.test/\0", 16)));
}

CK_TEST(a_target_is_bounded_at_the_documented_length) {
    const std::string prefix = "https://x.test/";
    const std::string longest = prefix + std::string(kMaxHyperlinkTargetBytes - prefix.size(), 'a');
    CK_CHECK(is_valid_hyperlink_target(longest));
    CK_CHECK(!is_valid_hyperlink_target(longest + "a"));
}

// --- LinkTable ------------------------------------------------------------------

CK_TEST(equal_targets_share_one_entry_and_one_id) {
    LinkTable table;
    const LinkId a = table.acquire("https://a.test/");
    const LinkId b = table.acquire("https://b.test/");
    const LinkId again = table.acquire("https://a.test/");
    CK_CHECK(a != kNoLink && b != kNoLink && a != b);
    CK_CHECK(again == a);
    CK_CHECK(table.size() == 2);
    CK_CHECK(table.target(a) == "https://a.test/");
    CK_CHECK(table.target(b) == "https://b.test/");
    CK_CHECK(table.target(kNoLink).empty());
}

CK_TEST(an_invalid_target_is_refused_and_leaves_the_table_unchanged) {
    LinkTable table;
    CK_CHECK(table.acquire("guide.md") == kNoLink);
    CK_CHECK(table.acquire("https://x.test/\x1B]8;;evil\x07") == kNoLink);
    CK_CHECK(table.empty());
}

CK_TEST(an_entry_lives_exactly_as_long_as_its_references) {
    LinkTable table;
    const LinkId id = table.acquire("https://a.test/");
    table.retain(id);
    table.release(id);
    CK_CHECK(table.contains(id));
    table.release(id);
    CK_CHECK(!table.contains(id));
    CK_CHECK(table.empty());
    // kNoLink is nobody's reference.
    table.retain(kNoLink);
    table.release(kNoLink);
    CK_CHECK(table.empty());
}

CK_TEST(a_freed_id_is_reused_most_recent_first_so_numbering_is_deterministic) {
    LinkTable table;
    const LinkId a = table.acquire("https://a.test/");
    const LinkId b = table.acquire("https://b.test/");
    const LinkId c = table.acquire("https://c.test/");
    table.release(a);
    table.release(c);
    CK_CHECK(table.acquire("https://d.test/") == c);
    CK_CHECK(table.acquire("https://e.test/") == a);
    CK_CHECK(table.acquire("https://f.test/") == c + 1);
    CK_CHECK(table.target(b) == "https://b.test/");
    CK_CHECK(table.size() == 4);
}

CK_TEST(a_copied_table_resolves_the_same_ids_and_stays_independent) {
    LinkTable table;
    const LinkId a = table.acquire("https://a.test/");
    const LinkTable copy = table;
    table.release(a);
    CK_CHECK(!table.contains(a));
    CK_CHECK(copy.target(a) == "https://a.test/");
    LinkTable assigned;
    assigned.acquire("https://other.test/");
    assigned = copy;
    CK_CHECK(assigned.target(a) == "https://a.test/");
    CK_CHECK(assigned.size() == 1);
    assigned.release(a);
    CK_CHECK(assigned.empty());
}

CK_TEST(clear_frees_every_entry_and_restarts_numbering) {
    LinkTable table;
    table.acquire("https://a.test/");
    table.acquire("https://b.test/");
    table.clear();
    CK_CHECK(table.empty());
    CK_CHECK(table.acquire("https://c.test/") == 1);
}

// --- Cell and FrameView -----------------------------------------------------

namespace {
// Cell's layout before it carried a link: what "pays nothing" is measured
// against.
struct CellWithoutLink {
    std::string grapheme;
    Style style;
    int width;
};
}  // namespace

// A link id costs a cell nothing: it sits in padding the layout already had.
static_assert(sizeof(Cell) == sizeof(CellWithoutLink), "a Cell is no larger for carrying a link id");

CK_TEST(every_cell_factory_makes_a_cell_that_is_not_a_link) {
    CK_CHECK(Cell{}.link() == kNoLink);
    CK_CHECK(Cell::from_grapheme("a", Style{}).link() == kNoLink);
    CK_CHECK(Cell::continuation(Style{}).link() == kNoLink);
    Cell cell = Cell::from_grapheme("a", Style{});
    cell.set_link(3);
    CK_CHECK(cell.link() == 3);
    CK_CHECK(!(cell == Cell::from_grapheme("a", Style{})));
    CK_CHECK(cell.same_content(Cell::from_grapheme("a", Style{})));
}

CK_TEST(same_cell_compares_links_by_target_across_two_grids) {
    LinkTable first_links;
    LinkTable second_links;
    second_links.acquire("https://unrelated.test/");  // shifts the numbering
    const LinkId in_first = first_links.acquire("https://a.test/");
    const LinkId in_second = second_links.acquire("https://a.test/");
    CK_CHECK(in_first != in_second);

    std::vector<Cell> first(2, Cell::from_grapheme("x", Style{}));
    std::vector<Cell> second(2, Cell::from_grapheme("x", Style{}));
    first[0].set_link(in_first);
    second[0].set_link(in_second);
    const FrameView a(first.data(), Size{2, 1}, &first_links);
    const FrameView b(second.data(), Size{2, 1}, &second_links);
    CK_CHECK(a.link_target(Point{0, 0}) == "https://a.test/");
    CK_CHECK(a.link_target(Point{1, 0}).empty());
    CK_CHECK(same_cell(a, Point{0, 0}, b, Point{0, 0}));
    CK_CHECK(same_cell(a, Point{1, 0}, b, Point{1, 0}));
    CK_CHECK(!same_cell(a, Point{0, 0}, b, Point{1, 0}));
    second[0].set_link(1);  // the unrelated target
    CK_CHECK(!same_cell(a, Point{0, 0}, b, Point{0, 0}));
}
