// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/editor_document.hpp"

using ckv::widgets::DocumentEditStatus;
using ckv::widgets::DocumentLineColumn;
using ckv::widgets::DocumentPosition;
using ckv::widgets::DocumentRange;
using ckv::widgets::EditorDocument;
using ckv::widgets::EditorDocumentOptions;
using ckv::widgets::InvalidUtf8Policy;

namespace {
DocumentRange range(EditorDocument& document, std::size_t begin, std::size_t end) {
    const auto first = document.position_at_byte(begin);
    const auto last = document.position_at_byte(end);
    CK_CHECK(first.has_value());
    CK_CHECK(last.has_value());
    return DocumentRange{*first, *last};
}
}  // namespace

CK_TEST(editor_document_normalizes_crlf_and_records_newline_preference) {
    EditorDocument document{"one\r\ntwo\rthree"};
    CK_CHECK(document.text() == "one\ntwo\nthree");
    CK_CHECK(document.line_count() == 3U);
    CK_CHECK(document.preferred_newline() == ckv::widgets::DocumentNewline::Crlf);
}

CK_TEST(editor_document_positions_are_revision_bound_and_grapheme_safe) {
    EditorDocument document{"a\xCC\x81" "b"};
    const auto start = document.position_at_byte(0);
    const auto after_combining = document.position_at_line_column(0, 1);
    CK_CHECK(start.has_value());
    CK_CHECK(after_combining.has_value());
    CK_CHECK(after_combining->byte == 3U);
    CK_CHECK(!document.position_at_byte(1).has_value());

    CK_CHECK(document.replace(DocumentRange{*start, *start}, "x"));
    CK_CHECK(!document.line_column(*after_combining).has_value());
}

CK_TEST(editor_document_replace_preserves_unaffected_piece_text) {
    EditorDocument document{"prefix-middle-suffix"};
    CK_CHECK(document.replace(range(document, 7, 13), "CENTER"));
    CK_CHECK(document.text() == "prefix-CENTER-suffix");
    CK_CHECK(document.line_count() == 1U);
}

CK_TEST(editor_document_transaction_is_atomic_and_advances_revision_once) {
    EditorDocument document{"abc def ghi"};
    const auto base = document.revision();
    auto transaction = document.transaction();
    transaction.replace(range(document, 8, 11), "GHI");
    transaction.replace(range(document, 0, 3), "ABC");
    const auto result = document.commit(std::move(transaction));
    CK_CHECK(result);
    CK_CHECK(result.change.has_value());
    CK_CHECK(result.change->previous_revision == base);
    CK_CHECK(result.change->revision == base + 1U);
    CK_CHECK(document.text() == "ABC def GHI");
}

CK_TEST(editor_document_rejects_overlapping_or_stale_transaction_ranges) {
    EditorDocument document{"abcdef"};
    auto overlap = document.transaction();
    overlap.replace(range(document, 1, 4), "x");
    overlap.replace(range(document, 3, 5), "y");
    CK_CHECK(document.commit(std::move(overlap)).status == DocumentEditStatus::InvalidRange);
    CK_CHECK(document.text() == "abcdef");

    const DocumentRange stale = range(document, 0, 1);
    CK_CHECK(document.replace(range(document, 1, 2), "B"));
    CK_CHECK(document.replace(stale, "A").status == DocumentEditStatus::StaleRevision);
}

CK_TEST(editor_document_undo_and_redo_restore_persistent_roots) {
    EditorDocument document{"before"};
    CK_CHECK(document.replace(range(document, 0, 6), "after"));
    CK_CHECK(document.text() == "after");
    CK_CHECK(document.undo());
    CK_CHECK(document.text() == "before");
    CK_CHECK(document.redo());
    CK_CHECK(document.text() == "after");
}

CK_TEST(editor_document_invalid_utf8_is_rejected_when_requested) {
    EditorDocumentOptions options;
    options.invalid_utf8 = InvalidUtf8Policy::Reject;
    EditorDocument document{"ok", options};
    const std::string invalid{"\xC3", 1};
    CK_CHECK(document.replace(range(document, 0, 0), invalid).status == DocumentEditStatus::InvalidUtf8);
    CK_CHECK(document.text() == "ok");
}

CK_TEST(editor_document_rejects_invalid_utf8_by_default_without_rewriting_the_document) {
    EditorDocument document{"stable"};
    const std::string invalid{"\xC3", 1};
    CK_CHECK(document.set_text(invalid) == DocumentEditStatus::InvalidUtf8);
    CK_CHECK(document.text() == "stable");
}

CK_TEST(editor_document_retains_utf8_bom_and_the_first_observed_newline_convention_as_format_metadata) {
    EditorDocument document{"\xEF\xBB\xBFone\rtwo\r\nthree\n"};
    CK_CHECK(document.encoding() == ckv::widgets::DocumentEncoding::Utf8);
    CK_CHECK(document.has_utf8_bom());
    CK_CHECK(document.preferred_newline() == ckv::widgets::DocumentNewline::Cr);
    CK_CHECK(document.text() == "one\ntwo\nthree\n");
}

CK_TEST(editor_document_rejects_limit_exceeding_edits_atomically) {
    EditorDocumentOptions options;
    options.max_document_bytes = 5U;
    EditorDocument document{"abc", options};
    const auto before = document.revision();
    const auto end = document.end();
    CK_CHECK(document.replace(DocumentRange{end, end}, "def").status == DocumentEditStatus::LimitExceeded);
    CK_CHECK(document.revision() == before);
    CK_CHECK(document.text() == "abc");
    CK_CHECK(document.set_text("123456") == DocumentEditStatus::LimitExceeded);
    CK_CHECK(document.text() == "abc");
}

CK_TEST(editor_document_observers_receive_precise_revision_change) {
    EditorDocument document{"one\ntwo"};
    std::optional<ckv::widgets::DocumentChange> observed;
    const auto observer = document.subscribe([&observed](const auto& change) { observed = change; });
    CK_CHECK(document.replace(range(document, 4, 7), "three"));
    CK_CHECK(observed.has_value());
    CK_CHECK(observed->first_affected_line == 1U);
    CK_CHECK(observed->last_affected_line == 1U);
    document.unsubscribe(observer);
}

CK_TEST(editor_document_line_and_position_lookup_remain_correct_after_piece_fragmentation) {
    EditorDocument document{"zero\none\ntwo\nthree"};
    CK_CHECK(document.replace(range(document, 5, 5), "inserted\n"));
    CK_CHECK(document.replace(range(document, 0, 0), "start\n"));
    CK_CHECK(document.replace(range(document, document.byte_size(), document.byte_size()), "\nend"));
    const auto position = document.position_at_line_column(3U, 1U);
    CK_CHECK(position.has_value());
    CK_CHECK(document.text(DocumentRange{*position, *document.position_at_line_column(3U, 3U)}) == "ne");
    const auto location = document.line_column(*position);
    CK_CHECK(location.has_value());
    CK_CHECK(location->line == 3U && location->column == 1U);
}

CK_TEST(editor_document_edits_at_one_position_apply_in_queue_order_before_a_replacement_starting_there) {
    // Insertions that share a position land in the order they were queued, ahead of a
    // replacement that begins at the same byte; the two only touch, so neither order of queuing
    // may be refused as an overlap.
    EditorDocument document{"abcdef"};
    auto insert_first = document.transaction();
    insert_first.replace(range(document, 2, 2), "1");
    insert_first.replace(range(document, 2, 4), "X");
    insert_first.replace(range(document, 2, 2), "2");
    CK_CHECK(document.commit(std::move(insert_first)));
    CK_CHECK(document.text() == "ab12Xef");

    EditorDocument reversed{"abcdef"};
    auto replace_first = reversed.transaction();
    replace_first.replace(range(reversed, 2, 4), "X");
    replace_first.replace(range(reversed, 2, 2), "1");
    replace_first.replace(range(reversed, 2, 2), "2");
    CK_CHECK(reversed.commit(std::move(replace_first)));
    CK_CHECK(reversed.text() == "ab12Xef");

    // Enough ties to leave the small-input path of any sorting algorithm.
    EditorDocument many{"<>"};
    auto transaction = many.transaction();
    std::string expected = "<";
    for (char letter = 'a'; letter <= 'z'; ++letter) {
        transaction.replace(range(many, 1, 1), std::string(1, letter));
        expected.push_back(letter);
    }
    expected.push_back('>');
    CK_CHECK(many.commit(std::move(transaction)));
    CK_CHECK(many.text() == expected);
}

CK_TEST(editor_document_keeps_a_leading_zero_width_no_break_space_in_edit_text) {
    // U+FEFF is a byte-order mark only at the start of loaded input; inside an edit it is text.
    EditorDocument document{"ab"};
    CK_CHECK(document.replace(range(document, 1, 1), "\xEF\xBB\xBFx"));
    CK_CHECK(document.text() == "a\xEF\xBB\xBFxb");
    CK_CHECK(!document.has_utf8_bom());
}

CK_TEST(editor_document_undo_and_redo_report_the_span_they_change_and_the_selection_to_restore) {
    EditorDocument document{"zero\none two three\nfour"};
    std::vector<ckv::widgets::DocumentChange> changes;
    const auto observer = document.subscribe([&changes](const auto& change) { changes.push_back(change); });
    auto transaction = document.transaction();
    transaction.replace(range(document, 9, 12), "2");
    transaction.replace(range(document, 13, 13), "+");
    transaction.set_selection_before(ckv::widgets::DocumentSelection{12, 9});
    CK_CHECK(document.commit(std::move(transaction)));
    CK_CHECK(document.text() == "zero\none 2 +three\nfour");

    // The undo reverts exactly the covering span the commit reported: [9, 12) of the new text
    // becomes the four original bytes "two ".
    const auto undone = document.undo();
    CK_CHECK(undone.has_value());
    CK_CHECK(document.text() == "zero\none two three\nfour");
    CK_CHECK(undone->replaced_begin_byte == 9U && undone->replaced_end_byte == 12U && undone->inserted_bytes == 4U);
    CK_CHECK(undone->first_affected_line == 1U && undone->last_affected_line == 1U);
    CK_CHECK(undone->selection == (ckv::widgets::DocumentSelection{12, 9}));
    CK_CHECK(changes.back() == *undone);

    const auto redone = document.redo();
    CK_CHECK(redone.has_value());
    CK_CHECK(document.text() == "zero\none 2 +three\nfour");
    CK_CHECK(redone->replaced_begin_byte == 9U && redone->replaced_end_byte == 13U && redone->inserted_bytes == 3U);
    CK_CHECK(redone->selection == (ckv::widgets::DocumentSelection{12, 12}));

    // Without a recorded selection an undo suggests a caret after the text it restored.
    CK_CHECK(document.replace(range(document, 0, 4), "ZERO!"));
    const auto plain = document.undo();
    CK_CHECK(plain && plain->selection == (ckv::widgets::DocumentSelection{4, 4}));
    CK_CHECK(document.set_text("x") == DocumentEditStatus::Ok);
    CK_CHECK(!document.undo().has_value());
    document.unsubscribe(observer);
}

CK_TEST(editor_document_refuses_a_transaction_whose_recorded_selection_is_not_in_its_text) {
    EditorDocument document{"a\xCC\x81" "bc"};
    auto transaction = document.transaction();
    transaction.replace(range(document, 3, 4), "B");
    transaction.set_selection_before(ckv::widgets::DocumentSelection{1, 3});
    CK_CHECK(document.commit(std::move(transaction)).status == DocumentEditStatus::InvalidRange);
    CK_CHECK(document.text() == "a\xCC\x81" "bc");
}

CK_TEST(editor_document_checks_every_edit_end_of_a_transaction_across_and_within_lines) {
    // Line 0 "a<acute>b", line 1 "cd", line 2 "e<acute>". Ends at a line's end, at the
    // next line's start and between clusters are accepted together.
    const std::string text = "a\xCC\x81" "b\ncd\ne\xCC\x81";
    EditorDocument document{text};
    const auto revision = document.revision();
    const auto at = [revision](std::size_t byte) { return DocumentPosition{revision, byte}; };
    auto accepted = document.transaction();
    accepted.replace(DocumentRange{at(0), at(3)}, "A");
    accepted.replace(DocumentRange{at(4), at(5)}, "");
    accepted.replace(DocumentRange{at(5), at(7)}, "CD");
    accepted.replace(DocumentRange{at(8), at(11)}, "E");
    CK_CHECK(document.commit(std::move(accepted)));
    CK_CHECK(document.text() == "AbCD\nE");  // [4, 5) was the first line break
    CK_CHECK(document.undo());

    // One end inside a cluster refuses the whole transaction, wherever it is queued: the
    // end at byte 10 falls between "e" and its combining acute on the last line.
    const auto current = document.revision();
    const auto now = [current](std::size_t byte) { return DocumentPosition{current, byte}; };
    auto refused = document.transaction();
    refused.replace(DocumentRange{now(8), now(10)}, "x");
    refused.replace(DocumentRange{now(0), now(3)}, "A");
    refused.replace(DocumentRange{now(5), now(6)}, "C");
    CK_CHECK(document.commit(std::move(refused)).status == DocumentEditStatus::InvalidRange);
    auto inside_first = document.transaction();
    inside_first.replace(DocumentRange{now(1), now(3)}, "x");
    inside_first.replace(DocumentRange{now(8), now(11)}, "E");
    CK_CHECK(document.commit(std::move(inside_first)).status == DocumentEditStatus::InvalidRange);
    CK_CHECK(document.text() == text);
}
