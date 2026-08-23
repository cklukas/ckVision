// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"

#include "cvision/widgets/editor_search.hpp"

using ckv::widgets::EditorDocument;
using ckv::widgets::EditorSearch;
using ckv::widgets::EditorSearchQuery;

CK_TEST(editor_search_finds_only_grapheme_safe_literal_matches) {
    EditorDocument document{"one two one"};
    const auto matches = EditorSearch::find_all(document, EditorSearchQuery{"one", true, true});
    CK_CHECK(matches.size() == 2U);
    CK_CHECK(matches[0].range.begin.byte == 0U);
    CK_CHECK(matches[1].range.begin.byte == 8U);
}

CK_TEST(editor_search_replace_all_is_one_undoable_transaction) {
    EditorDocument document{"cat cat cat"};
    const auto before = document.revision();
    const auto result = EditorSearch::replace_all(document, EditorSearchQuery{"cat", true, true}, "dog");
    CK_CHECK(result);
    CK_CHECK(document.revision() == before + 1U);
    CK_CHECK(document.text() == "dog dog dog");
    CK_CHECK(document.undo());
    CK_CHECK(document.text() == "cat cat cat");
}

CK_TEST(editor_search_case_folding_and_whole_words_are_explicit_ascii_rules) {
    EditorDocument document{"One one one_two (ONE)"};
    const auto matches = EditorSearch::find_all(document, EditorSearchQuery{"one", false, true});
    CK_CHECK(matches.size() == 3U);
    CK_CHECK(matches[0].range.begin.byte == 0U);
    CK_CHECK(matches[1].range.begin.byte == 4U);
    CK_CHECK(matches[2].range.begin.byte == 17U);
}

CK_TEST(editor_search_never_returns_a_partial_grapheme_match) {
    EditorDocument document{"a\xCC\x81 a"};
    const auto matches = EditorSearch::find_all(document, EditorSearchQuery{"a", true, false});
    CK_CHECK(matches.size() == 1U);
    CK_CHECK(matches.front().range.begin.byte == 4U);
}
