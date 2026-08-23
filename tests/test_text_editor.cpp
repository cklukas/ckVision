// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"

#include "cvision/widgets/text_editor.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::KeyEvent;
using ckv::MouseAction;
using ckv::MouseButton;
using ckv::MouseEvent;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::TextEvent;
using ckv::widgets::EditorDocument;
using ckv::widgets::DocumentEncoding;
using ckv::widgets::DocumentNewline;
using ckv::widgets::EditorStatus;
using ckv::widgets::EditorStatusModel;
using ckv::widgets::TextEditor;

CK_TEST(text_editor_edits_shared_document_and_reports_status) {
    auto document = std::make_shared<EditorDocument>("hello");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::None, ""}}));
    CK_CHECK(editor.on_text(TextEvent{" world", false}));
    CK_CHECK(document->text() == "hello world");
    CK_CHECK(editor.status().line == 1U);
    CK_CHECK(editor.status().column == 12U);
}

CK_TEST(text_editor_uses_file_name_to_select_a_standard_profile) {
    auto document = std::make_shared<EditorDocument>("name: ckvision\n");
    TextEditor editor(document);
    editor.set_file_name("config.yaml");
    CK_CHECK(editor.profile_id() == "yaml");
}

CK_TEST(text_editor_backspace_and_undo_operate_on_document_transactions) {
    auto document = std::make_shared<EditorDocument>("abc");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::None, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}}));
    CK_CHECK(document->text() == "ab");
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, "z"}}));
    CK_CHECK(document->text() == "abc");
}

CK_TEST(text_editor_overwrite_replaces_complete_graphemes_without_crossing_a_line) {
    auto document = std::make_shared<EditorDocument>("a🙂b\ncd");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::None, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Insert, Modifier::None, ""}}));
    CK_CHECK(editor.status().overwrite);
    CK_CHECK(editor.on_text(TextEvent{"XY", false}));
    CK_CHECK(document->text() == "aXY\ncd");
    CK_CHECK(editor.on_text(TextEvent{"!", false}));
    CK_CHECK(document->text() == "aXY!\ncd");
}

CK_TEST(text_editor_publishes_the_overwrite_mode_with_the_keystroke_that_toggled_it) {
    auto document = std::make_shared<EditorDocument>("abc");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    std::vector<bool> published;
    editor.set_status_changed_handler([&](const EditorStatus& state) { published.push_back(state.overwrite); });
    // The handler answers once on installation, and that first answer is the
    // insert mode the frame starts out showing.
    CK_CHECK(published.size() == 1U);
    CK_CHECK(!published.back());
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Insert, Modifier::None, ""}}));
    // Not "the state is now overwrite" — the point is that it was said out
    // loud during this keystroke, with no cursor move behind it to say it.
    CK_CHECK(published.size() == 2U);
    CK_CHECK(published.back());
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Insert, Modifier::None, ""}}));
    CK_CHECK(published.size() == 3U);
    CK_CHECK(!published.back());
}

CK_TEST(text_editor_search_uses_selection_as_a_revision_bound_query_and_wraps_in_both_directions) {
    auto document = std::make_shared<EditorDocument>("alpha beta alpha");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::Shift, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::Shift, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::Shift, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::Shift, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::Shift, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, "f"}}));
    CK_CHECK(editor.search_match_count() == 2U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::F3, Modifier::None, ""}}));
    CK_CHECK(editor.selection()->begin.byte == 11U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::F3, Modifier::Shift, ""}}));
    CK_CHECK(editor.selection()->begin.byte == 0U);
}

CK_TEST(text_editor_replace_current_and_replace_all_keep_the_document_transactional) {
    auto document = std::make_shared<EditorDocument>("item item item");
    TextEditor editor(document);
    editor.set_search_query(ckv::widgets::EditorSearchQuery{"item", true, true});
    CK_CHECK(editor.find_next());
    CK_CHECK(editor.replace_current_search_match("one"));
    CK_CHECK(document->text() == "one item item");
    const auto result = editor.replace_all_search_matches("two");
    CK_CHECK(result);
    CK_CHECK(document->text() == "one two two");
    CK_CHECK(document->undo());
    CK_CHECK(document->text() == "one item item");
}

CK_TEST(text_editor_supports_word_document_and_tab_editing_navigation) {
    auto document = std::make_shared<EditorDocument>("one two\nthree");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::Ctrl, ""}}));
    CK_CHECK(editor.status().column == 5U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::Ctrl, ""}}));
    CK_CHECK(editor.status().line == 2U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Home, Modifier::Ctrl, ""}}));
    CK_CHECK(editor.status().line == 1U && editor.status().column == 1U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Tab, Modifier::None, ""}}));
    CK_CHECK(document->text().starts_with("    one"));
}

CK_TEST(text_editor_supports_control_word_deletion_and_shift_extended_word_selection) {
    auto document = std::make_shared<EditorDocument>("one two three");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::Ctrl | Modifier::Shift, ""}}));
    CK_CHECK(editor.selection().has_value());
    CK_CHECK(editor.selection()->begin.byte == 0U && editor.selection()->end.byte == 4U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Delete, Modifier::None, ""}}));
    CK_CHECK(document->text() == "two three");
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::Ctrl, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Backspace, Modifier::Ctrl, ""}}));
    CK_CHECK(document->text() == "two ");
}

CK_TEST(text_editor_extends_and_collapses_selection_for_cursor_home_end_and_document_navigation) {
    auto document = std::make_shared<EditorDocument>("first\nsecond");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});

    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::Shift, ""}}));
    CK_CHECK(editor.selection()->begin.byte == 0U && editor.selection()->end.byte == 1U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::Shift, ""}}));
    CK_CHECK(editor.selection()->begin.byte == 0U && editor.selection()->end.byte == 5U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Down, Modifier::Shift, ""}}));
    CK_CHECK(editor.selection()->begin.byte == 0U && editor.selection()->end.byte == 11U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Home, Modifier::None, ""}}));
    CK_CHECK(!editor.selection().has_value());
    CK_CHECK(editor.status().line == 2U && editor.status().column == 1U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::Ctrl | Modifier::Shift, ""}}));
    CK_CHECK(editor.selection()->begin.byte == 6U && editor.selection()->end.byte == 12U);
}

CK_TEST(text_editor_status_exposes_document_encoding_and_newline_metadata) {
    auto document = std::make_shared<EditorDocument>("\xEF\xBB\xBF" "first\r\nsecond\r\n");
    TextEditor editor(document);
    const auto status = editor.status();
    CK_CHECK(status.encoding == DocumentEncoding::Utf8);
    CK_CHECK(status.newline == DocumentNewline::Crlf);
}

CK_TEST(text_editor_drag_selection_auto_scrolls_when_the_pointer_leaves_its_viewport) {
    auto document = std::make_shared<EditorDocument>("one\ntwo\nthree\nfour");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 20, 2});
    CK_CHECK(editor.on_mouse(MouseEvent{MouseAction::Down, MouseButton::Left, Point{0, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(editor.on_mouse(MouseEvent{MouseAction::Move, MouseButton::Left, Point{0, 4}, std::nullopt, Modifier::None}));
    CK_CHECK(editor.status().line == 3U);
    CK_CHECK(editor.selection().has_value());
    CK_CHECK(editor.on_mouse(MouseEvent{MouseAction::Up, MouseButton::Left, Point{0, 4}, std::nullopt, Modifier::None}));
}

CK_TEST(text_editor_double_click_selects_the_clicked_ascii_word_without_a_clock_dependency) {
    auto document = std::make_shared<EditorDocument>("alpha beta");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 20, 1});
    CK_CHECK(editor.on_mouse(MouseEvent{MouseAction::DoubleClick, MouseButton::Left, Point{2, 0}, std::nullopt,
                                         Modifier::None}));
    CK_CHECK(editor.selection().has_value());
    CK_CHECK(document->text(*editor.selection()) == "alpha");
}

CK_TEST(text_editor_disabled_state_rejects_keyboard_text_and_mouse_mutation) {
    auto document = std::make_shared<EditorDocument>("alpha");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 20, 1});
    editor.set_enabled(false);
    CK_CHECK(!editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::None, ""}}));
    CK_CHECK(!editor.on_text(TextEvent{"!", false}));
    CK_CHECK(!editor.on_mouse(MouseEvent{MouseAction::Down, MouseButton::Left, Point{2, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(document->text() == "alpha");
}

CK_TEST(editor_status_model_tracks_an_editor_without_assuming_window_chrome) {
    auto document = std::make_shared<EditorDocument>("one");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 20, 1});
    EditorStatusModel status(editor);
    std::size_t notifications = 0;
    const auto observer = status.subscribe([&notifications](const auto&) { ++notifications; });
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::None, ""}}));
    CK_CHECK(editor.on_text(TextEvent{" two", false}));
    CK_CHECK(status.value().line == 1U);
    CK_CHECK(status.value().column == 8U);
    CK_CHECK(status.value().modified);
    CK_CHECK(notifications != 0U);
    status.unsubscribe(observer);
}

// --- Wrap modes and scrollbar policies ------------------------------------

CK_TEST(the_editor_offers_all_three_wrap_modes_and_defaults_to_none) {
    // Source and logs mean what they mean at their own line breaks, so an
    // editor does not rewrap them unless asked. The horizontal bar is how
    // the rest of a long line is reached instead.
    auto document = std::make_shared<EditorDocument>("the quickbrown fox jumps");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 12, 5});
    CK_CHECK(editor.wrap_mode() == ckv::widgets::WrapMode::None);
    CK_CHECK(editor.content_width() > 12);  // one long row, reached sideways

    // Word: broken between words, and the row now fits.
    editor.set_wrap_mode(ckv::widgets::WrapMode::Word);
    CK_CHECK(editor.content_width() <= 12);

    // Character: broken at the edge, so it also fits — by a different rule.
    editor.set_wrap_mode(ckv::widgets::WrapMode::Character);
    CK_CHECK(editor.content_width() <= 12);
}

CK_TEST(the_editor_scrollbar_policies_are_settable_and_default_to_auto) {
    auto document = std::make_shared<EditorDocument>("short");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 20, 5});
    CK_CHECK(editor.vertical_scrollbar_policy() == ckv::widgets::ScrollbarPolicy::Auto);
    CK_CHECK(editor.horizontal_scrollbar_policy() == ckv::widgets::ScrollbarPolicy::Auto);

    editor.set_horizontal_scrollbar_policy(ckv::widgets::ScrollbarPolicy::Hidden);
    CK_CHECK(editor.horizontal_scrollbar_policy() == ckv::widgets::ScrollbarPolicy::Hidden);
    editor.set_vertical_scrollbar_policy(ckv::widgets::ScrollbarPolicy::Always);
    CK_CHECK(editor.vertical_scrollbar_policy() == ckv::widgets::ScrollbarPolicy::Always);
}

CK_TEST(the_editor_cursor_stays_on_screen_horizontally_on_a_long_line) {
    auto document = std::make_shared<EditorDocument>("0123456789abcdefghijklmnopqrstuvwxyz");
    TextEditor editor(document);
    editor.set_wrap_mode(ckv::widgets::WrapMode::None);
    editor.set_bounds(Rect{0, 0, 12, 4});
    CK_CHECK(editor.left_column() == 0);

    for (int i = 0; i < 30; ++i) editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    CK_CHECK(editor.status().column == 31U);
    // The view followed the cursor instead of letting it walk off the edge.
    CK_CHECK(editor.left_column() > 0);

    editor.on_key(KeyEvent{KeyChord{Key::Home, Modifier::None, ""}});
    CK_CHECK(editor.left_column() == 0);
}

CK_TEST(an_editor_rewrap_leaves_the_document_position_where_it_was) {
    // The document's line/column model is logical; wrapping is display only.
    auto document = std::make_shared<EditorDocument>("alpha beta gamma delta epsilon zeta");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 14, 6});
    for (int i = 0; i < 12; ++i) editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    const auto before = editor.status();

    editor.set_wrap_mode(ckv::widgets::WrapMode::Word);
    CK_CHECK(editor.status().line == before.line);
    CK_CHECK(editor.status().column == before.column);
    editor.set_wrap_mode(ckv::widgets::WrapMode::Character);
    CK_CHECK(editor.status().column == before.column);
    editor.set_bounds(Rect{0, 0, 8, 6});  // a resize rewraps too
    CK_CHECK(editor.status().column == before.column);
    CK_CHECK(document->text() == "alpha beta gamma delta epsilon zeta");
}
