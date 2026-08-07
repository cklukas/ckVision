// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/memo.hpp"

#include "cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
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

CK_TEST(a_grapheme_correct_backspace_removes_a_whole_multi_byte_cluster) {
    Fixture f;
    auto memo = make_memo(f);
    memo.set_text("a\xC3\xA9z");  // "aéz"; cursor starts at (0,0)
    memo.on_key(key(Key::Right));  // past 'a'
    memo.on_key(key(Key::Right));  // past 'é' -> cursor between é and z
    memo.on_key(key(Key::Backspace));
    CK_CHECK(memo.text() == "az");
}
