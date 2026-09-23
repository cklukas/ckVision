// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
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

CK_TEST(text_editor_treats_shifted_characters_as_text_and_other_modified_characters_as_chords) {
    auto document = std::make_shared<EditorDocument>();
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});

    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Char, Modifier::None, "a"}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Char, Modifier::Shift, "B"}}));
    CK_CHECK(document->text() == "aB");

    CK_CHECK(!editor.on_key(KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}}));
    CK_CHECK(!editor.on_key(KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, "q"}}));
    CK_CHECK(!editor.on_key(KeyEvent{KeyChord{Key::Char, Modifier::Super, "y"}}));
    CK_CHECK(document->text() == "aB");
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

CK_TEST(text_editor_hands_every_requested_change_to_its_edit_handler_first) {
    auto document = std::make_shared<EditorDocument>("- item");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::None, ""}}));

    std::vector<ckv::widgets::EditRequest> requests;
    editor.set_edit_handler([&](TextEditor& callback_editor, const ckv::widgets::EditRequest& request) {
        requests.push_back(request);
        if (request.kind != ckv::widgets::EditKind::LineBreak) return false;
        // A language-aware line break: the host commits its own transaction
        // and restores the caret, and the editor adds nothing of its own.
        auto transaction = document->transaction();
        transaction.replace({document->end(), document->end()}, "\n- ");
        if (!document->commit(std::move(transaction))) return false;
        const auto cursor = document->end();
        return callback_editor.set_selection({cursor, cursor});
    });
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(document->text() == "- item\n- ");
    CK_CHECK(editor.status().line == 2U && editor.status().column == 3U);
    CK_CHECK(requests.size() == 1U);
    CK_CHECK(requests[0].kind == ckv::widgets::EditKind::LineBreak);
    CK_CHECK(requests[0].text == "\n" && requests[0].replacement == "\n");
    CK_CHECK(requests[0].range.begin.byte == 6U && requests[0].range.end.byte == 6U);

    // Declined, the request is committed by the editor exactly as described.
    CK_CHECK(editor.on_text(TextEvent{"x", false}));
    CK_CHECK(document->text() == "- item\n- x");
    CK_CHECK(requests.back().kind == ckv::widgets::EditKind::Insert);
    CK_CHECK(editor.on_text(TextEvent{"pasted", true}));
    CK_CHECK(requests.back().kind == ckv::widgets::EditKind::Paste);
    CK_CHECK(requests.back().text == "pasted");
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}}));
    CK_CHECK(requests.back().kind == ckv::widgets::EditKind::DeleteBackward);
    CK_CHECK(requests.back().replacement.empty());
    CK_CHECK(document->text() == "- item\n- xpaste");
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Backspace, Modifier::Ctrl, ""}}));
    CK_CHECK(requests.back().kind == ckv::widgets::EditKind::DeleteWordBackward);
    CK_CHECK(document->text() == "- item\n- ");
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Home, Modifier::Ctrl, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Delete, Modifier::None, ""}}));
    CK_CHECK(requests.back().kind == ckv::widgets::EditKind::DeleteForward);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Delete, Modifier::Ctrl, ""}}));
    CK_CHECK(requests.back().kind == ckv::widgets::EditKind::DeleteWordForward);
    CK_CHECK(document->text() == "item\n- ");

    // Handled, nothing is committed by the editor.
    editor.set_edit_handler([&](TextEditor&, const ckv::widgets::EditRequest& request) {
        requests.push_back(request);
        return true;
    });
    CK_CHECK(editor.on_text(TextEvent{"ignored", false}));
    CK_CHECK(document->text() == "item\n- ");
}

CK_TEST(text_editor_tab_inserts_its_configured_width_as_an_indent_request) {
    auto document = std::make_shared<EditorDocument>("x");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    editor.set_tab_width(2);
    std::vector<ckv::widgets::EditRequest> requests;
    editor.set_edit_handler([&](TextEditor&, const ckv::widgets::EditRequest& request) {
        requests.push_back(request);
        return false;
    });
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Tab, Modifier::None, ""}}));
    CK_CHECK(document->text() == "  x");
    CK_CHECK(requests.size() == 1U && requests[0].kind == ckv::widgets::EditKind::Indent);
    editor.set_tab_width(0);
    CK_CHECK(editor.tab_width() == 1);
}

CK_TEST(text_editor_key_bindings_are_data_the_application_can_replace) {
    auto document = std::make_shared<EditorDocument>("abc");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    CK_CHECK(editor.key_bindings() == ckv::widgets::default_editor_key_bindings());
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::None, ""}}));
    CK_CHECK(editor.on_text(TextEvent{"d", false}));

    // An application with its own command table takes the chords away: the
    // chord is left unhandled for its keymap, and the verb is still reachable.
    editor.set_key_bindings({});
    const KeyEvent ctrl_z{KeyChord{Key::Char, Modifier::Ctrl, "z"}};
    CK_CHECK(!editor.on_key(ctrl_z));
    CK_CHECK(document->text() == "abcd");
    CK_CHECK(editor.perform(ckv::widgets::EditorCommand::Undo));
    CK_CHECK(document->text() == "abc");

    // A chord bound to another verb runs that one.
    editor.set_key_bindings({{KeyChord{Key::Char, Modifier::Alt, "u"}, ckv::widgets::EditorCommand::Redo},
                             {KeyChord{Key::F9, Modifier::None, ""}, ckv::widgets::EditorCommand::SelectAll}});
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Char, Modifier::Alt, "u"}}));
    CK_CHECK(document->text() == "abcd");
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::F9, Modifier::None, ""}}));
    CK_CHECK(editor.selection().has_value() && editor.selection()->end.byte == 4U);
    // Insert no longer toggles overwrite once it is unbound.
    CK_CHECK(!editor.on_key(KeyEvent{KeyChord{Key::Insert, Modifier::None, ""}}));
    CK_CHECK(!editor.overwrite());
}

CK_TEST(text_editor_select_all_selects_the_whole_document) {
    auto document = std::make_shared<EditorDocument>("one\ntwo");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, "a"}}));
    CK_CHECK(editor.selection().has_value());
    CK_CHECK(editor.selection()->begin.byte == 0U && editor.selection()->end.byte == 7U);
    auto empty = std::make_shared<EditorDocument>();
    TextEditor blank(empty);
    CK_CHECK(!blank.select_all());
}

CK_TEST(text_editor_vertical_motion_returns_to_the_column_it_left) {
    auto document = std::make_shared<EditorDocument>("a long line\nab\nanother long line");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 6});
    for (int i = 0; i < 8; ++i) CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Right, Modifier::None, ""}}));
    CK_CHECK(editor.status().column == 9U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(editor.status().line == 2U && editor.status().column == 3U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(editor.status().line == 3U && editor.status().column == 9U);
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Up, Modifier::None, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Up, Modifier::None, ""}}));
    CK_CHECK(editor.status().line == 1U && editor.status().column == 9U);
    // Horizontal motion sets a new column to return to.
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Left, Modifier::None, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(editor.status().line == 3U && editor.status().column == 8U);
}

CK_TEST(text_editor_paging_moves_the_caret_and_shift_paging_selects) {
    std::string text;
    for (int line = 0; line < 20; ++line) text += "line " + std::to_string(line) + "\n";
    auto document = std::make_shared<EditorDocument>(text);
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 5});
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::PageDown, Modifier::None, ""}}));
    CK_CHECK(editor.status().line == 5U);
    CK_CHECK(!editor.selection().has_value());
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::PageDown, Modifier::Shift, ""}}));
    CK_CHECK(editor.status().line == 9U);
    CK_CHECK(editor.selection().has_value());
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::PageUp, Modifier::None, ""}}));
    CK_CHECK(editor.status().line == 5U);
    CK_CHECK(!editor.selection().has_value());
    for (int i = 0; i < 10; ++i) editor.on_key(KeyEvent{KeyChord{Key::PageDown, Modifier::None, ""}});
    CK_CHECK(editor.status().line == 21U);
    for (int i = 0; i < 10; ++i) editor.on_key(KeyEvent{KeyChord{Key::PageUp, Modifier::None, ""}});
    CK_CHECK(editor.status().line == 1U);
}

CK_TEST(text_editor_virtual_caret_writes_nothing_until_text_arrives) {
    auto document = std::make_shared<EditorDocument>("ab\ncd");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 6});
    // Off by default: a double-click past the text is an ordinary placement.
    CK_CHECK(editor.on_mouse(MouseEvent{MouseAction::DoubleClick, MouseButton::Left, Point{6, 0}, std::nullopt,
                                         Modifier::None}));
    CK_CHECK(!editor.virtual_caret().has_value());
    CK_CHECK(!editor.set_virtual_caret(ckv::widgets::VirtualCaret{0, 6}));

    editor.set_virtual_space(true);
    CK_CHECK(editor.on_mouse(MouseEvent{MouseAction::DoubleClick, MouseButton::Left, Point{6, 0}, std::nullopt,
                                         Modifier::None}));
    CK_CHECK(editor.virtual_caret().has_value());
    CK_CHECK(*editor.virtual_caret() == (ckv::widgets::VirtualCaret{0, 6}));
    CK_CHECK(editor.status().virtual_caret);
    CK_CHECK(editor.status().line == 1U && editor.status().column == 7U);
    CK_CHECK(document->text() == "ab\ncd");

    std::vector<ckv::widgets::EditRequest> requests;
    editor.set_edit_handler([&](TextEditor&, const ckv::widgets::EditRequest& request) {
        requests.push_back(request);
        return false;
    });
    CK_CHECK(editor.on_text(TextEvent{"x", false}));
    CK_CHECK(document->text() == "ab    x\ncd");
    CK_CHECK(requests.size() == 1U);
    CK_CHECK(requests[0].virtual_caret.has_value());
    CK_CHECK(requests[0].text == "x" && requests[0].replacement == "    x");
    CK_CHECK(!editor.virtual_caret().has_value());
    CK_CHECK(document->undo());
    CK_CHECK(document->text() == "ab\ncd");

    // Below the last line: line breaks first, then the column.
    CK_CHECK(editor.on_mouse(MouseEvent{MouseAction::DoubleClick, MouseButton::Left, Point{3, 3}, std::nullopt,
                                         Modifier::None}));
    CK_CHECK(*editor.virtual_caret() == (ckv::widgets::VirtualCaret{3, 3}));
    CK_CHECK(editor.status().line == 4U && editor.status().column == 4U);
    CK_CHECK(editor.on_text(TextEvent{"y", false}));
    CK_CHECK(document->text() == "ab\ncd\n\n   y");

    // Erasing or moving abandons the caret and writes nothing.
    CK_CHECK(editor.set_virtual_caret(ckv::widgets::VirtualCaret{0, 5}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Backspace, Modifier::None, ""}}));
    CK_CHECK(!editor.virtual_caret().has_value());
    CK_CHECK(editor.set_virtual_caret(ckv::widgets::VirtualCaret{0, 5}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Left, Modifier::None, ""}}));
    CK_CHECK(!editor.virtual_caret().has_value());
    CK_CHECK(document->text() == "ab\ncd\n\n   y");
    // Inside the text is not past it.
    CK_CHECK(!editor.set_virtual_caret(ckv::widgets::VirtualCaret{0, 2}));
}

CK_TEST(text_editor_host_highlights_are_revision_bound_and_carried_through_edits) {
    auto document = std::make_shared<EditorDocument>("# Title\nbody");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 30, 4});
    const ckv::ui::RoleId marker{1};
    const ckv::ui::RoleId heading{2};
    CK_CHECK(!editor.has_highlights());
    CK_CHECK(!editor.set_highlights(document->revision() + 1U, {{0, 1, marker}}));
    CK_CHECK(!editor.set_highlights(document->revision(), {{2, 7, heading}, {0, 1, marker}}));
    CK_CHECK(!editor.set_highlights(document->revision(), {{0, 99, marker}}));
    CK_CHECK(editor.set_highlights(document->revision(), {{0, 1, marker}, {2, 7, heading}}));
    CK_CHECK(editor.has_highlights());
    // An edit between the spans shifts the later one and keeps the earlier.
    CK_CHECK(document->replace({*document->position_at_byte(1), *document->position_at_byte(1)}, "#"));
    CK_CHECK(editor.set_highlights(document->revision(), {{0, 2, marker}, {3, 8, heading}}));
    editor.clear_highlights();
    CK_CHECK(!editor.has_highlights());
}

CK_TEST(text_editor_asks_for_a_context_menu_after_placing_the_caret) {
    auto document = std::make_shared<EditorDocument>("alpha beta");
    TextEditor editor(document);
    editor.set_bounds(Rect{0, 0, 20, 2});
    // Without a handler a right click is not the editor's.
    CK_CHECK(!editor.on_mouse(MouseEvent{MouseAction::Down, MouseButton::Right, Point{7, 0}, std::nullopt,
                                          Modifier::None}));
    std::vector<Point> requests;
    editor.set_context_menu_handler([&](TextEditor&, Point cell) { requests.push_back(cell); });
    CK_CHECK(editor.on_mouse(MouseEvent{MouseAction::Down, MouseButton::Right, Point{7, 0}, std::nullopt,
                                         Modifier::None}));
    CK_CHECK(requests.size() == 1U && requests[0] == (Point{7, 0}));
    CK_CHECK(editor.cursor().byte == 7U);
    // Ctrl+click is the same request; inside the selection it keeps it.
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::Home, Modifier::None, ""}}));
    CK_CHECK(editor.on_key(KeyEvent{KeyChord{Key::End, Modifier::Shift, ""}}));
    CK_CHECK(editor.on_mouse(MouseEvent{MouseAction::Down, MouseButton::Left, Point{2, 0}, std::nullopt,
                                         Modifier::Ctrl}));
    CK_CHECK(requests.size() == 2U);
    CK_CHECK(editor.selection().has_value());
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

CK_TEST(text_editor_restores_a_current_grapheme_aligned_selection_for_controller_transactions) {
    auto document = std::make_shared<EditorDocument>("alpha 🙂 beta");
    TextEditor editor(document);
    const auto begin = document->position_at_byte(6U);
    const auto end = document->position_at_byte(10U);
    CK_CHECK(begin.has_value() && end.has_value());
    CK_CHECK(editor.set_selection({*begin, *end}));
    CK_CHECK(editor.selection().has_value());
    CK_CHECK(document->text(*editor.selection()) == "🙂");

    const ckv::widgets::DocumentRange stale{*begin, *end};
    CK_CHECK(document->replace({document->begin(), document->begin()}, "x"));
    CK_CHECK(!editor.set_selection(stale));
    CK_CHECK(!editor.set_selection({document->begin(), {document->revision(), 8U}}));
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

// --- A host that keeps the journal elsewhere --------------------------------

CK_TEST(a_history_handler_takes_the_undo_and_redo_keys_from_the_document) {
    auto document = std::make_shared<ckv::widgets::EditorDocument>("abc");
    ckv::widgets::TextEditor editor(document);
    int undos = 0;
    int redos = 0;
    editor.set_history_handler([&](ckv::widgets::TextEditor&, bool redo) {
        (redo ? redos : undos) += 1;
        return true;
    });
    document->replace(ckv::widgets::DocumentRange{document->end(), document->end()}, "d");
    CK_CHECK(document->can_undo());
    const ckv::KeyEvent undo{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "z"}};
    const ckv::KeyEvent redo{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "y"}};
    CK_CHECK(editor.on_key(undo));
    CK_CHECK(editor.on_key(redo));
    CK_CHECK(undos == 1 && redos == 1);
    // The document's own history was not walked.
    CK_CHECK(document->text() == "abcd");
    // Without a handler the keys walk the document's history as before.
    editor.set_history_handler({});
    CK_CHECK(editor.on_key(undo));
    CK_CHECK(document->text() == "abc");
}

CK_TEST(the_caret_stays_on_its_text_through_an_external_edit) {
    auto document = std::make_shared<ckv::widgets::EditorDocument>("hello world");
    ckv::widgets::TextEditor editor(document);
    const auto at = [&](std::size_t byte) {
        const auto position = document->position_at_byte(byte);
        editor.set_selection(ckv::widgets::DocumentRange{*position, *position});
    };
    at(6);  // before "world"
    // Bytes inserted before the caret carry it along.
    document->replace(ckv::widgets::DocumentRange{document->begin(), document->begin()}, "XX ");
    CK_CHECK(editor.cursor().byte == 9);
    CK_CHECK(document->text().substr(editor.cursor().byte) == "world");
    // Bytes removed before it too.
    document->replace(ckv::widgets::DocumentRange{document->begin(),
                                                  *document->position_at_byte(3)},
                      "");
    CK_CHECK(editor.cursor().byte == 6);
    // An edit after the caret leaves it where it is.
    document->replace(ckv::widgets::DocumentRange{document->end(), document->end()}, "!");
    CK_CHECK(editor.cursor().byte == 6);
    // A replacement that swallows the caret leaves it at the replacement's end.
    document->replace(ckv::widgets::DocumentRange{*document->position_at_byte(4),
                                                  *document->position_at_byte(8)},
                      "-");
    CK_CHECK(editor.cursor().byte == 5);
}

// --- Visual contract: host colouring and a caret past the text --------------
//
// Regenerate deliberately by writing this scene's serialized frame to
// golden/text_editor_highlights_virtual_caret.dump, and review the diff.

namespace {

std::string read_golden(const char* path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

CK_TEST(text_editor_paints_host_highlights_and_places_a_virtual_caret_past_the_text) {
    ckv::term::HeadlessTerminal terminal{ckv::Size{80, 24}};
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    // Roles of the host's own vocabulary, resolved through the theme like any
    // other: a marker that recedes and heading text that stands out.
    const ckv::ui::RoleId marker = app.roles().intern(
        "sample.markup.marker", ckv::Style{ckv::Color::rgb(120, 120, 120), ckv::Color::rgb(255, 255, 255)});
    const ckv::ui::RoleId heading = app.roles().intern(
        "sample.markup.heading", ckv::Style{ckv::Color::rgb(0, 0, 160), ckv::Color::rgb(255, 255, 255),
                                            ckv::Attr::Bold});
    app.theme().set(marker, app.roles().fallback(marker));
    app.theme().set(heading, app.roles().fallback(heading));

    auto document = std::make_shared<EditorDocument>("# Heading\nbody text");
    auto* editor = app.root().make<TextEditor>(document);
    editor->set_bounds(Rect{0, 0, 24, 5});
    editor->set_virtual_space(true);
    app.set_focus(editor);
    CK_CHECK(editor->set_highlights(document->revision(), {{0, 1, marker}, {2, 9, heading}}));
    CK_CHECK(editor->set_virtual_caret(ckv::widgets::VirtualCaret{3, 4}));
    app.step(clock.now_nanos());
    const std::string actual =
        ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    const std::string expected = read_golden("golden/text_editor_highlights_virtual_caret.dump");
    CK_CHECK(!expected.empty());
    CK_CHECK(actual == expected);
}
