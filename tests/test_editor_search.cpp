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

CK_TEST(editor_search_never_begins_a_match_inside_a_cluster_a_prepended_mark_opens) {
    // U+0600 ARABIC NUMBER SIGN is Prepend (UAX #29 GB9b): it joins the "a"
    // after it, so byte 2 is inside a cluster and only the last "a" matches.
    EditorDocument document{"\xD8\x80" "a a"};
    const auto matches = EditorSearch::find_all(document, EditorSearchQuery{"a", true, false});
    CK_CHECK(matches.size() == 1U);
    CK_CHECK(matches.front().range.begin.byte == 4U);
    const auto whole = EditorSearch::find_all(document, EditorSearchQuery{"\xD8\x80" "a", true, false});
    CK_CHECK(whole.size() == 1U);
    CK_CHECK(whole.front().range.begin.byte == 0U);
}

CK_TEST(editor_search_resumes_after_each_match_so_matches_never_overlap) {
    EditorDocument document{"aaaaa\nabab"};
    const auto runs = EditorSearch::find_all(document, EditorSearchQuery{"aa", true, false});
    CK_CHECK(runs.size() == 2U);
    CK_CHECK(runs[0].range.begin.byte == 0U);
    CK_CHECK(runs[1].range.begin.byte == 2U);
    const auto pairs = EditorSearch::find_all(document, EditorSearchQuery{"ab", true, false});
    CK_CHECK(pairs.size() == 2U);
    CK_CHECK(pairs[0].range.begin.byte == 6U);
    CK_CHECK(pairs[1].range.begin.byte == 8U);
}
