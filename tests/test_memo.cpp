// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/memo.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/core/clock.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Memo;
using ckv::widgets::MemoPosition;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

Memo make_memo(Fixture&) { return Memo(); }

ckv::KeyEvent key(ckv::Key k, ckv::Modifier m = Modifier::None) { return ckv::KeyEvent{KeyChord{k, m, ""}}; }

ckv::KeyEvent ctrl_char(std::string text) {
    return ckv::KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, std::move(text)}};
}

std::string row_text(const ckv::scene::Surface& surface, int row) {
    std::string out;
    for (int x = 0; x < surface.size().width; ++x) out += surface.at(ckv::Point{x, row}).grapheme();
    return out;
}
}  // namespace

// --- set_text / text round trip ------------------------------------

CK_TEST(a_freshly_constructed_memo_is_a_single_empty_line) {
    Fixture f;
    auto memo = make_memo(f);
    CK_CHECK(memo.text().empty());
    CK_CHECK(memo.cursor() == (MemoPosition{0, 0}));
}

CK_TEST(set_text_round_trips_through_text_exactly) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("line one\nline two\nline three");
    CK_CHECK(memo.text() == "line one\nline two\nline three");
}

CK_TEST(set_text_places_the_cursor_at_the_very_start) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("hello\nworld");
    CK_CHECK(memo.cursor() == (MemoPosition{0, 0}));
}

// --- Cursor navigation across line boundaries ------------------------

CK_TEST(right_arrow_at_end_of_line_wraps_to_the_start_of_the_next_line) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("ab\ncd");
    memo.on_key(key(Key::End));  // cursor at (0,2), end of "ab"
    memo.on_key(key(Key::Right));
    CK_CHECK(memo.cursor() == (MemoPosition{1, 0}));
}

CK_TEST(left_arrow_at_start_of_line_wraps_to_the_end_of_the_previous_line) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("ab\ncd");
    memo.on_key(key(Key::Down));  // (1, 0)
    memo.on_key(key(Key::Left));
    CK_CHECK(memo.cursor() == (MemoPosition{0, 2}));
}

CK_TEST(down_arrow_clamps_column_to_the_shorter_next_line) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("longer line\nab");
    memo.on_key(key(Key::End));  // (0, 11)
    memo.on_key(key(Key::Down));
    CK_CHECK(memo.cursor() == (MemoPosition{1, 2}));  // clamped to "ab"'s length
}

CK_TEST(up_arrow_at_the_first_line_does_not_move) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("a\nb");
    memo.on_key(key(Key::Up));
    CK_CHECK(memo.cursor() == (MemoPosition{0, 0}));
}

// --- Editing: insertion ------------------------------------------------

CK_TEST(typed_text_inserts_at_the_cursor) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("helloworld");
    for (int i = 0; i < 5; ++i) memo.on_key(key(Key::Right));
    memo.on_text(ckv::TextEvent{" ", false});
    CK_CHECK(memo.text() == "hello world");
}

CK_TEST(character_key_events_insert_text_but_modified_character_chords_do_not) {
    Fixture f;
    auto memo = make_memo(f);
    CK_CHECK(memo.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, "a"}}));
    CK_CHECK(memo.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Shift, "B"}}));
    CK_CHECK(memo.text() == "aB");

    CK_CHECK(!memo.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}}));
    CK_CHECK(!memo.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, "c"}}));
    CK_CHECK(!memo.on_key(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Super, "v"}}));
    CK_CHECK(memo.text() == "aB");
}

CK_TEST(enter_splits_the_current_line_into_two) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("helloworld");
    for (int i = 0; i < 5; ++i) memo.on_key(key(Key::Right));
    memo.on_key(key(Key::Enter));
    CK_CHECK(memo.text() == "hello\nworld");
    CK_CHECK(memo.cursor() == (MemoPosition{1, 0}));
}

CK_TEST(pasted_text_with_embedded_newlines_splits_correctly_and_preserves_the_tail) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("XY");
    memo.on_key(key(Key::Right));  // cursor between X and Y
    memo.on_text(ckv::TextEvent{"1\n2\n3", false});
    CK_CHECK(memo.text() == "X1\n2\n3Y");
}

// --- Editing: deletion --------------------------------------------------

CK_TEST(backspace_at_start_of_a_non_first_line_merges_it_into_the_previous_line) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("ab\ncd");
    memo.on_key(key(Key::Down));  // (1, 0)
    memo.on_key(key(Key::Backspace));
    CK_CHECK(memo.text() == "abcd");
    CK_CHECK(memo.cursor() == (MemoPosition{0, 2}));
}

CK_TEST(backspace_at_the_very_start_of_the_document_is_a_no_op) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("ab");
    memo.on_key(key(Key::Backspace));
    CK_CHECK(memo.text() == "ab");
    CK_CHECK(memo.cursor() == (MemoPosition{0, 0}));
}

CK_TEST(delete_at_end_of_a_non_last_line_merges_the_next_line_into_it) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("ab\ncd");
    memo.on_key(key(Key::End));  // (0, 2)
    memo.on_key(key(Key::Delete));
    CK_CHECK(memo.text() == "abcd");
    CK_CHECK(memo.cursor() == (MemoPosition{0, 2}));  // stays put — the merge doesn't move the cursor
}

CK_TEST(delete_at_the_very_end_of_the_document_is_a_no_op) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("ab");
    memo.on_key(key(Key::End));
    memo.on_key(key(Key::Delete));
    CK_CHECK(memo.text() == "ab");
}

CK_TEST(backspace_merging_the_last_line_away_leaves_the_document_non_empty) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("a\n");
    memo.on_key(key(Key::Down));
    memo.on_key(key(Key::Backspace));
    CK_CHECK(memo.text() == "a");
}

// --- Selection ---------------------------------------------------------

CK_TEST(shift_right_extends_selection_and_typing_over_it_replaces_the_selection) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("hello");
    memo.on_key(key(Key::Right, Modifier::Shift));
    memo.on_key(key(Key::Right, Modifier::Shift));
    CK_CHECK(memo.has_selection());
    memo.on_text(ckv::TextEvent{"X", false});
    CK_CHECK(memo.text() == "Xllo");
}

CK_TEST(selection_spanning_multiple_lines_deletes_the_lines_in_between_entirely) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("aaa\nbbb\nccc");
    memo.on_key(key(Key::End));                    // (0,3)
    memo.on_key(key(Key::Down, Modifier::Shift));   // extend to (1,3)
    memo.on_key(key(Key::Down, Modifier::Shift));   // extend to (2,3)
    memo.on_key(key(Key::Backspace));               // delete the selection
    CK_CHECK(memo.text() == "aaa");
}

CK_TEST(plain_arrow_after_a_selection_collapses_it_without_deleting_anything) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("hello");
    memo.on_key(key(Key::Right, Modifier::Shift));
    CK_CHECK(memo.has_selection());
    memo.on_key(key(Key::Left));
    CK_CHECK(!memo.has_selection());
    CK_CHECK(memo.text() == "hello");
}

CK_TEST(copy_cut_and_paste_use_the_application_clipboard) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    ckv::ui::Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);

    Memo memo;
    memo.set_context(ckv::ui::Context{&app.theme(), &app.roles(), &app});
    memo.set_text("hello");
    memo.on_key(key(Key::Right, Modifier::Shift));
    memo.on_key(key(Key::Right, Modifier::Shift));

    CK_CHECK(memo.on_key(ctrl_char("c")));
    CK_CHECK(app.clipboard_text() == "he");
    CK_CHECK(memo.text() == "hello");

    CK_CHECK(memo.on_key(ctrl_char("x")));
    CK_CHECK(app.clipboard_text() == "he");
    CK_CHECK(memo.text() == "llo");

    memo.on_key(key(Key::End));
    CK_CHECK(memo.on_key(ctrl_char("v")));
    CK_CHECK(memo.text() == "llohe");
}

CK_TEST(memo_uses_word_and_document_navigation_with_legacy_insert_clipboard_bindings) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    ckv::ui::Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);

    Memo memo;
    memo.set_context(ckv::ui::Context{&app.theme(), &app.roles(), &app});
    memo.set_text("one two\nthree");
    memo.on_key(key(Key::End, Modifier::Ctrl));
    CK_CHECK(memo.on_key(key(Key::Home, Modifier::Ctrl | Modifier::Shift)));
    CK_CHECK(memo.selection_range() == std::make_pair(MemoPosition{0, 0}, MemoPosition{1, 5}));
    CK_CHECK(memo.on_key(key(Key::Insert, Modifier::Ctrl)));
    CK_CHECK(app.clipboard_text() == "one two\nthree");
    CK_CHECK(memo.on_key(key(Key::Delete, Modifier::Shift)));
    CK_CHECK(memo.text().empty());
    CK_CHECK(memo.on_key(key(Key::Insert, Modifier::Shift)));
    CK_CHECK(memo.text() == "one two\nthree");
    CK_CHECK(memo.on_key(key(Key::End)));
    CK_CHECK(memo.on_key(key(Key::Left, Modifier::Ctrl)));
    CK_CHECK(memo.cursor() == (MemoPosition{1, 0}));
}

CK_TEST(undo_restores_the_previous_memo_text_cursor_and_selection) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("hello");
    memo.on_key(key(Key::Right, Modifier::Shift));
    memo.on_key(key(Key::Right, Modifier::Shift));
    memo.on_text(ckv::TextEvent{"X", false});

    CK_CHECK(memo.text() == "Xllo");
    CK_CHECK(memo.on_key(ctrl_char("z")));
    CK_CHECK(memo.text() == "hello");
    CK_CHECK(memo.has_selection());
    CK_CHECK(memo.selection_range() == std::make_pair(MemoPosition{0, 0}, MemoPosition{0, 2}));
}

// --- Mouse ------------------------------------------------------------

CK_TEST(clicking_places_the_cursor_at_the_clicked_line_and_column) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_bounds(Rect{0, 0, 20, 5});
    memo.set_text("aaa\nbbb\nccc");
    memo.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 1}, std::nullopt,
                                   Modifier::None});
    CK_CHECK(memo.cursor() == (MemoPosition{1, 1}));
}

CK_TEST(click_outside_the_memos_bounds_is_ignored) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_bounds(Rect{0, 0, 20, 5});
    memo.set_text("a");
    CK_CHECK(!memo.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 50},
                                             std::nullopt, Modifier::None}));
}

CK_TEST(mouse_drag_extends_the_memo_selection) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_bounds(Rect{0, 0, 20, 5});
    memo.set_text("abcdef");

    CK_CHECK(memo.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{1, 0},
                                            std::nullopt, Modifier::None}));
    CK_CHECK(memo.on_mouse(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{4, 0},
                                            std::nullopt, Modifier::None}));
    CK_CHECK(memo.has_selection());
    CK_CHECK(memo.selection_range() == std::make_pair(MemoPosition{0, 1}, MemoPosition{0, 4}));
    CK_CHECK(memo.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{4, 0},
                                            std::nullopt, Modifier::None}));
}

// --- Rendering does not crash --------------------------------------

CK_TEST(draw_does_not_crash_for_an_empty_focused_memo) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_context(f.ctx());
    memo.set_bounds(Rect{0, 0, 20, 5});
    memo.on_focus(ckv::FocusEvent{true});
    ckv::scene::Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 20, 5});
    memo.draw(painter);
    CK_CHECK(true);
}

CK_TEST(wrapping_draws_soft_rows_without_changing_the_document_text) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_context(f.ctx());
    memo.on_attached();
    memo.set_bounds(Rect{0, 0, 6, 3});
    memo.set_text("abcdefgh");

    ckv::scene::Surface surface(ckv::Size{6, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 6, 3});

    // Character mode breaks where the edge falls, mid-word.
    memo.set_wrap_mode(ckv::widgets::WrapMode::Character);
    memo.draw(painter);
    CK_CHECK(memo.text() == "abcdefgh");  // the document is untouched either way
    CK_CHECK(row_text(surface, 0).substr(0, 6) == "abcdef");
    CK_CHECK(row_text(surface, 1).substr(0, 2) == "gh");

    // Word mode will not split one long word: it stays whole and overflows,
    // which is what brings the horizontal bar out rather than telling the
    // reader the word is shorter than it is.
    memo.set_wrap_mode(ckv::widgets::WrapMode::Word);
    memo.draw(painter);
    CK_CHECK(memo.text() == "abcdefgh");
    CK_CHECK(memo.content_width() == 8);
    CK_CHECK(row_text(surface, 1).substr(0, 2) == "  ");  // nothing wrapped down
}

CK_TEST(a_grapheme_correct_backspace_removes_a_whole_multi_byte_cluster) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("a\xC3\xA9z");  // "aéz"; cursor starts at (0,0)
    memo.on_key(key(Key::Right));  // past 'a'
    memo.on_key(key(Key::Right));  // past 'é' -> cursor between é and z
    memo.on_key(key(Key::Backspace));
    CK_CHECK(memo.text() == "az");
}

// --- Wrap modes and scrollbar policies ------------------------------------

CK_TEST(a_memo_offers_all_three_wrap_modes_and_word_wraps_by_default) {
    // Prose is what a memo holds, so Word is the default here — unlike an
    // editor, where source and logs mean what they mean at their own breaks.
    Fixture f;
    Memo memo = make_memo(f);
    memo.set_context(f.ctx());
    memo.on_attached();
    memo.set_bounds(Rect{0, 0, 12, 6});
    CK_CHECK(memo.wrap_mode() == ckv::widgets::WrapMode::Word);

    memo.set_text("the quickbrown fox");
    ckv::scene::Surface surface(ckv::Size{12, 6}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 12, 6});
    memo.draw(painter);
    // Word wrap keeps "quickbrown" whole, so it cannot share row 0.
    CK_CHECK(row_text(surface, 0).find("quickb") == std::string::npos);

    memo.set_wrap_mode(ckv::widgets::WrapMode::Character);
    memo.draw(painter);
    CK_CHECK(row_text(surface, 0).find("quickb") != std::string::npos);

    // No wrap: one display row per logical line, reached sideways instead.
    memo.set_wrap_mode(ckv::widgets::WrapMode::None);
    memo.draw(painter);
    CK_CHECK(memo.content_width() >= 18);
}

CK_TEST(a_memo_shows_a_horizontal_bar_only_when_something_runs_off_the_edge) {
    Fixture f;
    Memo memo = make_memo(f);
    memo.set_context(f.ctx());
    memo.on_attached();
    memo.set_bounds(Rect{0, 0, 12, 6});
    CK_CHECK(memo.horizontal_scrollbar_policy() == ckv::widgets::ScrollbarPolicy::Auto);

    // Wrapped, nothing overflows sideways however long the text is.
    memo.set_wrap_mode(ckv::widgets::WrapMode::Word);
    memo.set_text("aaa bbb ccc ddd eee fff ggg hhh");
    CK_CHECK(memo.content_width() <= 12);

    // Unwrapped, the same text is one long row and the bar has work to do.
    memo.set_wrap_mode(ckv::widgets::WrapMode::None);
    CK_CHECK(memo.content_width() > 12);
}

CK_TEST(the_memo_cursor_stays_on_screen_horizontally_as_it_walks_a_long_line) {
    // Document coordinates are what the cursor is stored in; the view scrolls
    // to keep the display position of that cursor inside the viewport.
    Fixture f;
    Memo memo = make_memo(f);
    memo.set_context(f.ctx());
    memo.on_attached();
    memo.set_wrap_mode(ckv::widgets::WrapMode::None);
    memo.set_bounds(Rect{0, 0, 12, 4});
    memo.set_text("0123456789abcdefghijklmnopqrstuvwxyz");
    memo.on_focus(ckv::FocusEvent{true});
    CK_CHECK(memo.left_column() == 0);

    for (int i = 0; i < 30; ++i) memo.on_key(key(Key::Right));
    CK_CHECK(memo.cursor().column == 30);
    // The view followed it rather than letting it walk off the edge.
    CK_CHECK(memo.left_column() > 0);
    CK_CHECK(memo.cursor().column >= memo.left_column());

    memo.on_key(key(Key::Home));
    CK_CHECK(memo.left_column() == 0);
}

CK_TEST(a_memo_rewrap_moves_no_cursor_and_no_selection) {
    // The whole reason cursor and selection live in document coordinates:
    // changing how lines are displayed must not edit where the reader is.
    Fixture f;
    Memo memo = make_memo(f);
    memo.set_context(f.ctx());
    memo.on_attached();
    memo.set_bounds(Rect{0, 0, 14, 6});
    memo.set_text("alpha beta gamma delta epsilon");
    memo.on_focus(ckv::FocusEvent{true});
    for (int i = 0; i < 12; ++i) memo.on_key(key(Key::Right));
    const MemoPosition before = memo.cursor();

    memo.set_wrap_mode(ckv::widgets::WrapMode::Character);
    CK_CHECK(memo.cursor() == before);
    memo.set_wrap_mode(ckv::widgets::WrapMode::None);
    CK_CHECK(memo.cursor() == before);
    memo.set_bounds(Rect{0, 0, 8, 6});  // a resize rewraps too
    CK_CHECK(memo.cursor() == before);
}
