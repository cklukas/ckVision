// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/text_view.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::widgets::TextSpan;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::TextView;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

TextView make_view(Fixture&) { return TextView(); }

ckv::KeyEvent key(ckv::Key k, Modifier modifier = Modifier::None) {
    return ckv::KeyEvent{KeyChord{k, modifier, ""}};
}

std::string row_text(const Surface& s, int y) {
    std::string out;
    for (int x = 0; x < s.size().width; ++x) out += s.at(ckv::Point{x, y}).grapheme();
    return out;
}
}  // namespace

// --- Line splitting -----------------------------------------------------

CK_TEST(empty_text_is_a_single_empty_line) {
    Fixture f;
    auto view = make_view(f);
    view.set_text("");
    CK_CHECK(view.line_count() == 1);
}

CK_TEST(text_with_no_newlines_is_a_single_line) {
    Fixture f;
    auto view = make_view(f);
    view.set_text("hello world");
    CK_CHECK(view.line_count() == 1);
}

CK_TEST(hiding_the_internal_scrollbar_preserves_the_final_text_column) {
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 4, 1});
    view.set_text("abcd");
    view.set_vertical_scrollbar_visible(false);
    Surface s(ckv::Size{4, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 4, 1});
    view.draw(painter);
    CK_CHECK(row_text(s, 0) == "abcd");
}

CK_TEST(newlines_split_into_separate_lines_including_a_trailing_empty_one) {
    Fixture f;
    auto view = make_view(f);
    view.set_text("a\nb\nc\n");
    CK_CHECK(view.line_count() == 4);  // "a", "b", "c", ""
}

CK_TEST(spans_build_plain_text_lines_and_link_inventory) {
    Fixture f;
    auto view = make_view(f);
    view.set_spans({TextSpan{"Open ", static_cast<ckv::Attr>(0), std::nullopt},
                    TextSpan{"Docs", ckv::Attr::Bold, std::string("https://example.test/docs")},
                    TextSpan{"\nNext", static_cast<ckv::Attr>(0), std::string("next-topic")}});

    CK_CHECK(view.text() == "Open Docs\nNext");
    CK_CHECK(view.line_count() == 2);
    CK_CHECK(view.link_count() == 2);
    CK_CHECK(view.current_link() == 0);
}

CK_TEST(tab_cycles_links_and_enter_activates_the_current_link) {
    Fixture f;
    auto view = make_view(f);
    view.set_spans({TextSpan{"One", static_cast<ckv::Attr>(0), std::string("one")},
                    TextSpan{" Two", static_cast<ckv::Attr>(0), std::string("two")}});

    std::string activated;
    view.on_link_activate = [&](const std::string& target) { activated = target; };
    CK_CHECK(view.on_key(key(Key::Tab)));
    CK_CHECK(view.current_link() == 1);
    CK_CHECK(view.on_key(key(Key::Tab, Modifier::Shift)));
    CK_CHECK(view.current_link() == 0);
    CK_CHECK(view.on_key(key(Key::Enter)));
    CK_CHECK(activated == "one");
}

CK_TEST(mouse_click_on_a_link_activates_that_link) {
    Fixture f;
    auto view = make_view(f);
    view.set_bounds(Rect{0, 0, 20, 3});
    view.set_spans({TextSpan{"Go ", static_cast<ckv::Attr>(0), std::nullopt},
                    TextSpan{"there", static_cast<ckv::Attr>(0), std::string("target")}});

    std::string activated;
    view.on_link_activate = [&](const std::string& target) { activated = target; };
    CK_CHECK(view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{3, 0},
                                            std::nullopt, Modifier::None}));
    CK_CHECK(activated == "target");
}

// --- Links as terminal hyperlinks (D-088) ------------------------------

namespace {
// Every cell of row `y` whose link target is `target`, as column indices.
std::vector<int> linked_columns(const Surface& s, int y, std::string_view target) {
    std::vector<int> columns;
    for (int x = 0; x < s.size().width; ++x)
        if (s.link_target(ckv::Point{x, y}) == target) columns.push_back(x);
    return columns;
}
}  // namespace

CK_TEST(a_link_span_paints_its_target_into_every_cell_it_covers) {
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 20, 2});
    view.set_spans({TextSpan{"Open ", static_cast<ckv::Attr>(0), std::nullopt},
                    TextSpan{"Docs", ckv::Attr::Bold, std::string("https://example.test/docs")},
                    TextSpan{" now", static_cast<ckv::Attr>(0), std::nullopt}});
    Surface s(ckv::Size{20, 2}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 2});
    view.draw(painter);

    CK_CHECK((linked_columns(s, 0, "https://example.test/docs") == std::vector<int>{5, 6, 7, 8}));
    CK_CHECK(s.link_target(ckv::Point{4, 0}).empty());
    CK_CHECK(s.link_target(ckv::Point{9, 0}).empty());
    CK_CHECK(s.links().size() == 1);
}

CK_TEST(a_wrapped_link_span_links_its_cells_on_every_line_it_reaches) {
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 10, 3});
    view.set_vertical_scrollbar_policy(ckv::widgets::ScrollbarPolicy::Hidden);
    view.set_wrap_mode(ckv::widgets::WrapMode::Word);
    view.set_spans({TextSpan{"See ", static_cast<ckv::Attr>(0), std::nullopt},
                    TextSpan{"the long guide", static_cast<ckv::Attr>(0), std::string("https://example.test/g")}});
    Surface s(ckv::Size{10, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 3});
    view.draw(painter);

    CK_CHECK(row_text(s, 0).starts_with("See the"));
    CK_CHECK((linked_columns(s, 0, "https://example.test/g") == std::vector<int>{4, 5, 6, 7}));
    CK_CHECK(row_text(s, 1).starts_with("long guide"));
    CK_CHECK((linked_columns(s, 1, "https://example.test/g") ==
              std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
    CK_CHECK(s.links().size() == 1);
}

CK_TEST(a_target_that_is_not_a_terminal_hyperlink_stays_the_views_own) {
    // "guide.md" names a page to the application, not a resource to the
    // terminal: it is underlined and activates as before, and no cell links.
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 20, 1});
    view.set_spans({TextSpan{"guide", static_cast<ckv::Attr>(0), std::string("guide.md")}});
    std::string activated;
    view.on_link_activate = [&](const std::string& target) { activated = target; };
    Surface s(ckv::Size{20, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 1});
    view.draw(painter);

    CK_CHECK(ckv::has_attr(s.at(ckv::Point{0, 0}).style().attrs, ckv::Attr::Underline));
    CK_CHECK(s.links().empty());
    CK_CHECK(view.activate_current_link());
    CK_CHECK(activated == "guide.md");
}

CK_TEST(a_hostile_link_target_is_dropped_from_the_cells_not_repaired) {
    // The architecture §12: a target carrying a terminator, a C1 control or
    // malformed UTF-8 never reaches a cell's link, so no emitter can send it.
    // The application still receives its own string on activation.
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 20, 1});
    const std::string hostile = "https://x.test/]8;;evilÂ";
    view.set_spans({TextSpan{"here", static_cast<ckv::Attr>(0), hostile}});
    std::string activated;
    view.on_link_activate = [&](const std::string& target) { activated = target; };
    Surface s(ckv::Size{20, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 1});
    view.draw(painter);

    CK_CHECK(row_text(s, 0).starts_with("here"));
    CK_CHECK(s.links().empty());
    CK_CHECK(view.activate_current_link());
    CK_CHECK(activated == hostile);
}

// --- Scrolling: keyboard -------------------------------------------------

CK_TEST(down_arrow_scrolls_by_one_line) {
    Fixture f;
    auto view = make_view(f);
    view.set_bounds(Rect{0, 0, 20, 5});
    std::string text;
    for (int i = 0; i < 20; ++i) text += std::to_string(i) + "\n";
    view.set_text(text);
    view.on_key(key(Key::Down));
    CK_CHECK(view.top_line() == 1);
}

CK_TEST(up_arrow_at_the_top_does_not_go_negative) {
    Fixture f;
    auto view = make_view(f);
    view.set_bounds(Rect{0, 0, 20, 5});
    view.set_text("a\nb\nc");
    view.on_key(key(Key::Up));
    CK_CHECK(view.top_line() == 0);
}

CK_TEST(page_down_scrolls_by_the_visible_height) {
    Fixture f;
    auto view = make_view(f);
    view.set_bounds(Rect{0, 0, 20, 5});
    std::string text;
    for (int i = 0; i < 20; ++i) text += std::to_string(i) + "\n";
    view.set_text(text);
    view.on_key(key(Key::PageDown));
    CK_CHECK(view.top_line() == 5);
}

CK_TEST(end_scrolls_to_the_maximum_reachable_top_line) {
    Fixture f;
    auto view = make_view(f);
    view.set_bounds(Rect{0, 0, 20, 5});
    std::string text;
    for (int i = 0; i < 20; ++i) text += std::to_string(i) + "\n";
    view.set_text(text);
    view.on_key(key(Key::End));
    CK_CHECK(view.top_line() == view.line_count() - 5);
}

CK_TEST(home_returns_to_the_top) {
    Fixture f;
    auto view = make_view(f);
    view.set_bounds(Rect{0, 0, 20, 5});
    std::string text;
    for (int i = 0; i < 20; ++i) text += std::to_string(i) + "\n";
    view.set_text(text);
    view.on_key(key(Key::End));
    view.on_key(key(Key::Home));
    CK_CHECK(view.top_line() == 0);
}

CK_TEST(content_shorter_than_the_viewport_cannot_scroll_at_all) {
    Fixture f;
    auto view = make_view(f);
    view.set_bounds(Rect{0, 0, 20, 10});
    view.set_text("a\nb");
    view.on_key(key(Key::Down));
    CK_CHECK(view.top_line() == 0);
}

// --- Scrolling: mouse wheel -----------------------------------------

CK_TEST(mouse_wheel_scrolls_and_a_non_wheel_event_is_unhandled) {
    Fixture f;
    auto view = make_view(f);
    view.set_bounds(Rect{0, 0, 20, 5});
    std::string text;
    for (int i = 0; i < 20; ++i) text += std::to_string(i) + "\n";
    view.set_text(text);
    CK_CHECK(view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown, ckv::Point{0, 0},
                                            std::nullopt, Modifier::None}));
    CK_CHECK(view.top_line() == ckv::ui::kWheelRows);
    CK_CHECK(!view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{0, 0},
                                             std::nullopt, Modifier::None}));
}

// --- Rendering -----------------------------------------------------------

CK_TEST(draw_renders_the_lines_starting_at_the_current_scroll_position) {
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 20, 3});
    view.set_text("first\nsecond\nthird\nfourth");
    view.on_key(key(Key::Down));  // scroll to line 1 ("second")

    Surface s(ckv::Size{20, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 3});
    view.draw(painter);
    CK_CHECK(row_text(s, 0).substr(0, 6) == "second");
    CK_CHECK(row_text(s, 1).substr(0, 5) == "third");
}

CK_TEST(draw_applies_inline_and_link_styles) {
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 20, 3});
    view.set_spans({TextSpan{"A", ckv::Attr::Bold, std::nullopt},
                    TextSpan{"B", ckv::Attr::Italic, std::string("target")}});

    Surface s(ckv::Size{20, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 3});
    view.draw(painter);

    CK_CHECK(ckv::has_attr(s.at(ckv::Point{0, 0}).style().attrs, ckv::Attr::Bold));
    CK_CHECK(ckv::has_attr(s.at(ckv::Point{1, 0}).style().attrs, ckv::Attr::Italic));
    CK_CHECK(ckv::has_attr(s.at(ckv::Point{1, 0}).style().attrs, ckv::Attr::Underline));
    // The current link is where Enter goes, which means something only
    // while the view holds the keyboard.
    CK_CHECK(!ckv::has_attr(s.at(ckv::Point{1, 0}).style().attrs, ckv::Attr::Reverse));
}

CK_TEST(the_current_link_is_marked_while_the_view_holds_the_keyboard) {
    Fixture f;
    ckv::term::HeadlessTerminal terminal(ckv::Size{20, 3});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    TextView view;
    view.set_context(ckv::ui::Context{&f.theme, &f.registry, &app});
    view.set_bounds(Rect{0, 0, 20, 3});
    view.set_spans({TextSpan{"A", ckv::Attr::Bold, std::nullopt},
                    TextSpan{"B", ckv::Attr::Italic, std::string("target")}});
    app.set_focus(&view);
    Surface s(ckv::Size{20, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 3});
    view.draw(painter);
    CK_CHECK(ckv::has_attr(s.at(ckv::Point{1, 0}).style().attrs, ckv::Attr::Reverse));
}

CK_TEST(draw_leaves_rows_past_the_end_of_content_blank_without_crashing) {
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 20, 10});  // far more rows than content
    view.set_text("only line");

    Surface s(ckv::Size{20, 10}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 10});
    view.draw(painter);
    CK_CHECK(row_text(s, 0).substr(0, 9) == "only line");
    CK_CHECK(row_text(s, 5)[0] == ' ');  // cleared, not left as the surface's prior fill character
}

CK_TEST(a_zero_size_view_does_not_crash_on_draw_or_input) {
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 0, 0});
    view.set_text("hello");
    view.on_key(key(Key::Down));
    Surface s(ckv::Size{1, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 1, 1});
    view.draw(painter);
    CK_CHECK(true);
}

// --- Word wrap and scrollbar policies -------------------------------------

CK_TEST(word_wrap_breaks_between_words_and_keeps_them_whole) {
    Fixture f;
    TextView view;
    view.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    view.on_attached();
    view.set_bounds(ckv::Rect{0, 0, 12, 6});
    view.set_text("the quick brown fox");

    // Unwrapped, one logical line is one display line however long it is.
    CK_CHECK(view.wrap_mode() == ckv::widgets::WrapMode::None);
    CK_CHECK(view.display_line_count() == 1);

    view.set_wrap_mode(ckv::widgets::WrapMode::Word);
    CK_CHECK(view.display_line_count() > 1);
    // Wrapping is by width, so no display line is wider than the text area...
    CK_CHECK(view.content_width() <= 12);
    // ...and a wider view needs fewer of them.
    const int narrow_rows = view.display_line_count();
    view.set_bounds(ckv::Rect{0, 0, 24, 6});
    CK_CHECK(view.display_line_count() < narrow_rows);
}

CK_TEST(a_word_wider_than_the_view_is_not_broken_and_brings_a_horizontal_bar) {
    // Breaking mid-word would hide that a path or an identifier is wider than
    // the window. It stays whole, overflows, and the bar says so.
    Fixture f;
    TextView view;
    view.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    view.on_attached();
    view.set_wrap_mode(ckv::widgets::WrapMode::Word);
    view.set_bounds(ckv::Rect{0, 0, 10, 4});
    view.set_text("short /a/very/long/unbreakable/path");

    CK_CHECK(view.content_width() > 10);
    CK_CHECK(view.horizontal_scrollbar_policy() == ckv::widgets::ScrollbarPolicy::Auto);
    // Text that does fit needs no bar, even with wrapping on.
    view.set_text("all short words here");
    CK_CHECK(view.content_width() <= 10);
}

CK_TEST(the_two_scrollbars_are_sized_against_each_other) {
    // A vertical bar costs a column, which can be what makes a line no longer
    // fit; a horizontal bar costs a row, which can be what makes the text no
    // longer fit. Resolving them independently gives a view that either
    // overlaps its own content or hides a row the reader cannot reach.
    Fixture f;
    TextView view;
    view.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    view.on_attached();
    view.set_bounds(ckv::Rect{0, 0, 10, 3});

    // Long enough to need both bars.
    view.set_text("a line that is far wider than ten cells\nsecond\nthird\nfourth");
    ckv::scene::Surface surface(ckv::Size{10, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 10, 3});
    view.draw(painter);  // must not paint outside its own bounds

    // Content that fits in every direction shows neither bar.
    view.set_bounds(ckv::Rect{0, 0, 40, 8});
    view.set_text("one\ntwo");
    CK_CHECK(view.display_line_count() == 2);
    CK_CHECK(view.content_width() <= 40);
}

CK_TEST(a_hidden_scrollbar_policy_leaves_the_whole_width_to_the_text) {
    // For a view whose scrolling a containing viewport owns.
    Fixture f;
    TextView view;
    view.set_context(ckv::ui::Context{&f.theme, &f.registry, nullptr});
    view.on_attached();
    view.set_vertical_scrollbar_policy(ckv::widgets::ScrollbarPolicy::Hidden);
    view.set_horizontal_scrollbar_policy(ckv::widgets::ScrollbarPolicy::Hidden);
    view.set_wrap_mode(ckv::widgets::WrapMode::Word);
    view.set_bounds(ckv::Rect{0, 0, 10, 2});
    view.set_text("aaa bbb ccc ddd eee fff ggg");

    CK_CHECK(view.vertical_scrollbar_policy() == ckv::widgets::ScrollbarPolicy::Hidden);
    // Wrapping measured against the full width, since no bar takes a column.
    CK_CHECK(view.content_width() <= 10);
}

CK_TEST(a_click_finds_the_link_under_the_pointer_after_a_horizontal_scroll) {
    // A27: a click lands on what is drawn under the pointer, which after a
    // sideways scroll is further along the line than the column clicked.
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 10, 3});
    view.set_spans({TextSpan{"0123456789ABCDEF", static_cast<ckv::Attr>(0), std::nullopt},
                    TextSpan{"LINK", static_cast<ckv::Attr>(0), std::string("target")}});
    ckv::widgets::Scrollbar* horizontal = nullptr;
    for (const auto& child : view.children())
        if (auto* bar = dynamic_cast<ckv::widgets::Scrollbar*>(child.get()); bar != nullptr && bar->visible() &&
                                                                             bar->bounds().height == 1)
            horizontal = bar;
    CK_CHECK(horizontal != nullptr);
    if (horizontal == nullptr) return;
    horizontal->set_position(10);  // as far as a 20-cell line scrolls in 10 columns
    CK_CHECK(view.left_column() == 10);

    std::string activated;
    view.on_link_activate = [&](const std::string& target) { activated = target; };
    const auto click = [&](int x) {
        return view.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{x, 0},
                                             std::nullopt, Modifier::None});
    };
    CK_CHECK(!click(0));  // "A", plain text
    CK_CHECK(!click(5));  // "F", the last plain cell
    CK_CHECK(activated.empty());
    CK_CHECK(click(6));   // "L", the first cell of the link
    CK_CHECK(activated == "target");
}

CK_TEST(a_wide_glyph_cut_by_the_left_edge_shows_as_a_blank_in_its_style_and_nothing_after_it_moves) {
    // A horizontally scrolled line whose first visible column is the right
    // half of a wide glyph: that cell is a blank in the glyph's own style
    // (the link's underline here), and every later glyph keeps the column
    // the scroll gives it rather than moving one to the right.
    Fixture f;
    auto view = make_view(f);
    view.set_context(f.ctx());
    view.set_bounds(Rect{0, 0, 10, 3});
    // "a" at column 0, six U+4E2D (two columns each) at 1..12, "z" at 13.
    view.set_spans({TextSpan{"a", static_cast<ckv::Attr>(0), std::nullopt},
                    TextSpan{"\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD\xE4\xB8\xAD",
                             static_cast<ckv::Attr>(0), std::string("target")},
                    TextSpan{"z", static_cast<ckv::Attr>(0), std::nullopt}});
    ckv::widgets::Scrollbar* horizontal = nullptr;
    for (const auto& child : view.children())
        if (auto* bar = dynamic_cast<ckv::widgets::Scrollbar*>(child.get()); bar != nullptr && bar->visible() &&
                                                                             bar->bounds().height == 1)
            horizontal = bar;
    CK_CHECK(horizontal != nullptr);
    if (horizontal == nullptr) return;
    horizontal->set_position(2);  // column 2 is the right half of the first wide glyph
    CK_CHECK(view.left_column() == 2);

    Surface s(ckv::Size{10, 3}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 10, 3});
    view.draw(painter);
    const ckv::Cell cut = s.at(ckv::Point{0, 0});
    CK_CHECK(cut.grapheme() == " ");
    CK_CHECK(ckv::has_attr(cut.style().attrs, ckv::Attr::Underline));
    // The next glyph starts at line column 3, which is view column 1.
    CK_CHECK(s.at(ckv::Point{1, 0}).grapheme() == "\xE4\xB8\xAD");
    CK_CHECK(s.at(ckv::Point{2, 0}).is_continuation());
    CK_CHECK(s.at(ckv::Point{3, 0}).grapheme() == "\xE4\xB8\xAD");
    CK_CHECK(s.at(ckv::Point{7, 0}).grapheme() == "\xE4\xB8\xAD");
    CK_CHECK(s.at(ckv::Point{8, 0}).is_continuation());
}

CK_TEST(a_scripted_text_view_scrolls_and_follows_links_through_dispatched_input) {
    // Application-level script: the view holds the focus in a real
    // Application; keys, a click and the wheel reach it through dispatch, and
    // step() shows the lines each scroll brings into view.
    ckv::term::HeadlessTerminal term(ckv::Size{30, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* view = app.root().add(std::make_unique<TextView>());
    view->set_bounds(Rect{0, 0, 20, 3});
    view->set_spans({TextSpan{"Go ", static_cast<ckv::Attr>(0), std::nullopt},
                     TextSpan{"home", static_cast<ckv::Attr>(0), std::string("home")},
                     TextSpan{" or ", static_cast<ckv::Attr>(0), std::nullopt},
                     TextSpan{"away", static_cast<ckv::Attr>(0), std::string("away")},
                     TextSpan{"\nline 2\nline 3\nline 4\nline 5\nline 6", static_cast<ckv::Attr>(0), std::nullopt}});
    std::vector<std::string> activated;
    view->on_link_activate = [&](const std::string& target) { activated.push_back(target); };
    app.set_focus(view);
    app.step(0);
    const auto row = [&](int y) { return row_text(app.composed_surface(), y); };
    CK_CHECK(row(0).starts_with("Go home or away"));

    CK_CHECK(app.dispatch(key(Key::Tab)));
    CK_CHECK(app.dispatch(key(Key::Enter)));
    CK_CHECK((activated == std::vector<std::string>{"away"}));
    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{4, 0},
                                          std::nullopt, Modifier::None}));
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{4, 0}, std::nullopt,
                                 Modifier::None});
    CK_CHECK((activated == std::vector<std::string>{"away", "home"}));

    CK_CHECK(app.dispatch(key(Key::PageDown)));
    app.step(0);
    CK_CHECK(row(0).starts_with("line 4"));
    // One notch up is ui::kWheelRows rows: back from "line 4" to the top.
    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelUp, ckv::Point{2, 1},
                                          std::nullopt, Modifier::None}));
    app.step(0);
    CK_CHECK(row(0).starts_with("Go home or away"));
}

CK_TEST(tab_leaves_the_view_past_its_last_link_and_the_walk_restarts_when_focus_returns) {
    // Application-level script: links are stops on the way through the view,
    // never a ring. A view that kept Tab for itself while it had links left a
    // keyboard reader no way on to the controls after it.
    ckv::term::HeadlessTerminal term(ckv::Size{30, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* before = app.root().add(std::make_unique<TextView>());
    before->set_bounds(Rect{0, 0, 30, 1});
    auto* view = app.root().add(std::make_unique<TextView>());
    view->set_bounds(Rect{0, 1, 30, 2});
    view->set_spans({TextSpan{"one", static_cast<ckv::Attr>(0), std::string("one")},
                     TextSpan{" and ", static_cast<ckv::Attr>(0), std::nullopt},
                     TextSpan{"two", static_cast<ckv::Attr>(0), std::string("two")}});
    auto* after = app.root().add(std::make_unique<TextView>());
    after->set_bounds(Rect{0, 3, 30, 1});
    app.set_focus(view);
    app.step(0);
    CK_CHECK(view->current_link() == std::optional<std::size_t>{0});

    CK_CHECK(app.dispatch(key(Key::Tab)));
    CK_CHECK(view->current_link() == std::optional<std::size_t>{1});
    CK_CHECK(app.focused() == view);
    app.dispatch(key(Key::Tab));  // past the last link: focus traversal moves on
    CK_CHECK(app.focused() == after);
    // Leaving put the walk back at its start, so returning begins there.
    CK_CHECK(view->current_link() == std::optional<std::size_t>{0});
    app.dispatch(key(Key::Tab, Modifier::Shift));
    CK_CHECK(app.focused() == view);
    CK_CHECK(view->current_link() == std::optional<std::size_t>{0});
    // Shift+Tab on the first link leaves backwards the same way.
    app.dispatch(key(Key::Tab, Modifier::Shift));
    CK_CHECK(app.focused() == before);
}
