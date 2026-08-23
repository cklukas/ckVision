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
